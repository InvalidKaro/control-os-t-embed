#include "Application.h"

#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "BoardPins.h"
#include "Config.h"
#include "Log.h"

namespace fw {

namespace {
constexpr const char* Tag = "APP";
}

bool Application::begin() {
    Serial.begin(config::SerialBaud);
    LOG_INFO(Tag, "%s %s", config::FirmwareName, config::FirmwareVersion);
    LOG_INFO(Tag, "Board: %s", config::BoardName);

    settings_.begin();
    hardware_.begin();

    display_.setTheme(settings_.data().theme);
    if (!display_.begin(settings_.data().displayBrightness)) {
        LOG_ERROR(Tag, "Display geometry does not match expected 320x170");
    }

    if (!input_.begin()) {
        LOG_ERROR(Tag, "Input initialization failed");
        return false;
    }

    LedEffect configuredEffect = static_cast<LedEffect>(settings_.data().ledEffect);
    if (configuredEffect >= LedEffect::Count) configuredEffect = LedEffect::Cyberpunk;
    settings_.data().ledEffect = static_cast<uint8_t>(configuredEffect);
    leds_.begin(settings_.data().ledBrightness, configuredEffect);
    leds_.trigger(LedOverlay::Success, 700);

    drawMainMenu();
    initialized_ = true;

    LOG_INFO(Tag, "Flash=%lu bytes PSRAM=%lu bytes heap=%lu bytes",
        static_cast<unsigned long>(ESP.getFlashChipSize()),
        static_cast<unsigned long>(ESP.getPsramSize()),
        static_cast<unsigned long>(ESP.getFreeHeap()));
    return true;
}

void Application::tick(uint32_t now) {
    if (!initialized_) return;

    input_.tick(now);
    InputEvent event{};
    while (input_.pop(event)) {
        if (inMainMenu_) {
            handleMainInput(event);
        } else {
            modules_.handleInput(event);
        }
    }

    if (!inMainMenu_) {
        modules_.tick(now);
        if (modules_.wantsExit()) {
            modules_.clearExitRequest();
            modules_.leave();
            inMainMenu_ = true;
            leds_.trigger(LedOverlay::Notification, 280);
            drawMainMenu();
        }
    }

    leds_.tick(now, input_.encoderState());
}

void Application::handleMainInput(const InputEvent& event) {
    if (event.type == InputEventType::Rotate) {
        const int count = static_cast<int>(MainItems.size());
        int next = static_cast<int>(mainSelection_) + event.delta;
        next %= count;
        if (next < 0) next += count;
        mainSelection_ = static_cast<std::size_t>(next);
        leds_.trigger(LedOverlay::Rotation, 220);
        drawMainMenu();
        return;
    }

    if (event.type == InputEventType::EncoderClick) {
        leds_.trigger(LedOverlay::Click, 180);
        enterSelected();
        return;
    }

    if (event.type == InputEventType::EncoderLongPress) {
        leds_.trigger(LedOverlay::Notification, 450);
        return;
    }
}

void Application::enterSelected() {
    if (mainSelection_ == 14U) {
        deepSleep();
        return;
    }

    constexpr ScreenId screens[] = {
        ScreenId::RadioAnalyzer,
        ScreenId::RadioReceive,
        ScreenId::RadioTransmit,
        ScreenId::IrMonitor,
        ScreenId::IrTransmit,
        ScreenId::NfcReader,
        ScreenId::WifiScanner,
        ScreenId::BleScanner,
        ScreenId::SdBrowser,
        ScreenId::NrfScanner,
        ScreenId::LedStudio,
        ScreenId::SystemInfo,
        ScreenId::Settings,
        ScreenId::About
    };

    constexpr std::size_t screenCount = sizeof(screens) / sizeof(screens[0]);
    if (mainSelection_ >= screenCount) return;
    inMainMenu_ = false;
    modules_.enter(screens[mainSelection_]);
}

void Application::drawMainMenu() {
    char status[24]{};
    const uint8_t battery = hardware_.batteryPercent();
    std::snprintf(status, sizeof(status), "%u%%", battery);
    display_.menu("CONTROL OS", MainItems.data(), MainItems.size(), mainSelection_, status);
}

void Application::deepSleep() {
    LOG_INFO(Tag, "Entering deep sleep; wake on BACK key GPIO%d", pins::BackKey);
    settings_.save();
    hardware_.stopActiveOperations();
    leds_.setEffect(LedEffect::Off);
    leds_.tick(millis() + config::LedFrameMs, input_.encoderState());
    display_.message("POWER", "Deep sleep", "Press BACK to wake", nullptr);
    display_.setBrightness(0);

    pinMode(pins::BackKey, INPUT_PULLUP);
    const gpio_num_t wakePin = static_cast<gpio_num_t>(pins::BackKey);
    rtc_gpio_pullup_en(wakePin);
    rtc_gpio_pulldown_dis(wakePin);
    digitalWrite(pins::PowerEnable, LOW);
    esp_sleep_enable_ext0_wakeup(wakePin, 0);
    esp_deep_sleep_start();
}

}  // namespace fw

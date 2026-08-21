#pragma once

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "Display.h"
#include "Input.h"
#include "LedEngine.h"
#include "Modules.h"
#include "Settings.h"

namespace fw {

class Application final {
public:
    bool begin();
    void tick(uint32_t now);

private:
    static constexpr std::array<const char*, 15> MainItems{
        "Sub-GHz Analyzer",
        "Sub-GHz Receive",
        "Sub-GHz Test TX",
        "IR Monitor",
        "IR Test TX",
        "NFC Reader",
        "Wi-Fi Scanner",
        "BLE Scanner",
        "SD Browser",
        "nRF24 Scanner",
        "LED Studio",
        "System Info",
        "Settings",
        "About",
        "Deep Sleep"
    };

    void handleMainInput(const InputEvent& event);
    void enterSelected();
    void drawMainMenu();
    void deepSleep();

    SettingsManager settings_{};
    DisplayManager display_{};
    InputManager input_{};
    LedManager leds_{};
    HardwareServices hardware_{};
    ModulesUi modules_{hardware_, display_, leds_, settings_};

    bool inMainMenu_ = true;
    std::size_t mainSelection_ = 0;
    bool initialized_ = false;
    uint32_t lastUiDrawAt_ = 0;
};

}  // namespace fw

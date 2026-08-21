#include "Display.h"

#include <algorithm>
#include <cstdio>

#include "BoardPins.h"
#include "Log.h"

namespace fw {

namespace {
constexpr const char* Tag = "DISPLAY";
constexpr uint8_t BacklightChannel = 7;
constexpr uint32_t BacklightFrequency = 12000;
constexpr uint8_t BacklightResolutionBits = 8;
}

uint16_t DisplayManager::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
}

const Theme& DisplayManager::themeFor(ThemeId id) {
    static const Theme dark{
        rgb565(7, 9, 13), rgb565(18, 22, 30), rgb565(240, 244, 248), rgb565(115, 125, 138),
        rgb565(0, 190, 255), rgb565(130, 90, 255), rgb565(40, 220, 120), rgb565(255, 190, 40), rgb565(255, 65, 85)
    };
    static const Theme oled{
        rgb565(0, 0, 0), rgb565(7, 7, 7), rgb565(255, 255, 255), rgb565(105, 105, 105),
        rgb565(0, 220, 255), rgb565(255, 255, 255), rgb565(0, 255, 130), rgb565(255, 190, 0), rgb565(255, 35, 50)
    };
    static const Theme cyberpunk{
        rgb565(7, 3, 18), rgb565(20, 10, 35), rgb565(245, 240, 255), rgb565(150, 130, 175),
        rgb565(0, 245, 255), rgb565(255, 25, 190), rgb565(70, 255, 145), rgb565(255, 225, 20), rgb565(255, 45, 90)
    };
    static const Theme matrix{
        rgb565(0, 4, 0), rgb565(2, 15, 4), rgb565(180, 255, 185), rgb565(65, 125, 70),
        rgb565(0, 255, 65), rgb565(35, 155, 60), rgb565(65, 255, 100), rgb565(210, 255, 35), rgb565(255, 60, 60)
    };
    static const Theme retro{
        rgb565(28, 20, 12), rgb565(45, 31, 18), rgb565(255, 224, 170), rgb565(170, 135, 90),
        rgb565(255, 145, 50), rgb565(255, 220, 75), rgb565(120, 220, 100), rgb565(255, 185, 60), rgb565(235, 65, 45)
    };
    static const Theme light{
        rgb565(238, 242, 246), rgb565(255, 255, 255), rgb565(25, 31, 40), rgb565(105, 115, 125),
        rgb565(0, 125, 220), rgb565(110, 75, 220), rgb565(20, 165, 80), rgb565(225, 145, 0), rgb565(215, 40, 55)
    };
    static const Theme vaporwave{
        rgb565(14, 5, 30), rgb565(35, 14, 55), rgb565(252, 230, 255), rgb565(170, 130, 185),
        rgb565(255, 65, 195), rgb565(40, 230, 255), rgb565(70, 255, 180), rgb565(255, 215, 80), rgb565(255, 65, 100)
    };

    switch (id) {
        case ThemeId::Dark: return dark;
        case ThemeId::Oled: return oled;
        case ThemeId::Cyberpunk: return cyberpunk;
        case ThemeId::Matrix: return matrix;
        case ThemeId::Retro: return retro;
        case ThemeId::Light: return light;
        case ThemeId::Vaporwave: return vaporwave;
        case ThemeId::Count: return cyberpunk;
    }
    return cyberpunk;
}

bool DisplayManager::begin(uint8_t brightness) {
    pinMode(pins::DisplayBacklight, OUTPUT);
    ledcSetup(BacklightChannel, BacklightFrequency, BacklightResolutionBits);
    ledcAttachPin(pins::DisplayBacklight, BacklightChannel);
    brightness_ = std::min<uint8_t>(brightness, 100U);
    ledcWrite(BacklightChannel, 0);

    tft_.init();
    tft_.setRotation(3);
    tft_.setSwapBytes(true);
    tft_.setTextWrap(false, false);
    initialized_ = true;
    clear();
    applyBacklight();

    LOG_INFO(Tag, "ST7789 ready: %dx%d rotation=3 SPI=40MHz", tft_.width(), tft_.height());
    return tft_.width() == pins::DisplayWidth && tft_.height() == pins::DisplayHeight;
}

void DisplayManager::setBrightness(uint8_t percent) {
    brightness_ = std::min<uint8_t>(percent, 100U);
    applyBacklight();
}

uint8_t DisplayManager::brightness() const {
    return brightness_;
}

void DisplayManager::setTheme(ThemeId theme) {
    if (theme == ThemeId::Count) theme = ThemeId::Cyberpunk;
    themeId_ = theme;
}

ThemeId DisplayManager::themeId() const {
    return themeId_;
}

const Theme& DisplayManager::theme() const {
    return themeFor(themeId_);
}

TFT_eSPI& DisplayManager::tft() {
    return tft_;
}

void DisplayManager::clear() {
    if (!initialized_) return;
    tft_.fillScreen(theme().background);
}

void DisplayManager::header(const char* title, const char* right) {
    const Theme& c = theme();
    tft_.fillRect(0, 0, pins::DisplayWidth, 25, c.surface);
    tft_.drawFastHLine(0, 24, pins::DisplayWidth, c.accent);
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(c.text, c.surface);
    tft_.drawString(title != nullptr ? title : "", 8, 12, 2);
    if (right != nullptr) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(c.muted, c.surface);
        tft_.drawString(right, pins::DisplayWidth - 7, 12, 1);
    }
}

void DisplayManager::footer(const char* left, const char* right) {
    const Theme& c = theme();
    constexpr int16_t y = pins::DisplayHeight - 23;
    tft_.fillRect(0, y, pins::DisplayWidth, 23, c.surface);
    tft_.drawFastHLine(0, y, pins::DisplayWidth, c.accent2);
    tft_.setTextColor(c.muted, c.surface);
    tft_.setTextDatum(ML_DATUM);
    tft_.drawString(left != nullptr ? left : "", 8, y + 12, 1);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(right != nullptr ? right : "", pins::DisplayWidth - 8, y + 12, 1);
}

void DisplayManager::menu(
    const char* title,
    const char* const* items,
    std::size_t count,
    std::size_t selected,
    const char* status
) {
    clear();
    header(title, status);
    footer("Back", "Press: open");

    if (items == nullptr || count == 0U) {
        message(title, "No entries", nullptr, nullptr);
        return;
    }

    selected %= count;
    constexpr int visibleRows = 5;
    std::size_t first = 0;
    if (selected >= 2U) first = selected - 2U;
    if (first + visibleRows > count) {
        first = count > visibleRows ? count - visibleRows : 0U;
    }

    const Theme& c = theme();
    for (int row = 0; row < visibleRows; ++row) {
        const std::size_t index = first + static_cast<std::size_t>(row);
        if (index >= count) break;
        const int16_t y = 31 + row * 23;
        const bool active = index == selected;
        if (active) {
            tft_.fillRoundRect(7, y - 2, pins::DisplayWidth - 14, 21, 4, c.accent);
            tft_.setTextColor(c.background, c.accent);
        } else {
            tft_.setTextColor(c.text, c.background);
        }
        tft_.setTextDatum(ML_DATUM);
        tft_.drawString(items[index], 14, y + 8, 2);
    }
}

void DisplayManager::message(const char* title, const char* line1, const char* line2, const char* line3) {
    clear();
    header(title);
    footer("Back", "");
    const Theme& c = theme();
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(c.text, c.background);
    if (line1 != nullptr) tft_.drawString(line1, pins::DisplayWidth / 2, 62, 2);
    tft_.setTextColor(c.muted, c.background);
    if (line2 != nullptr) tft_.drawString(line2, pins::DisplayWidth / 2, 88, 2);
    if (line3 != nullptr) tft_.drawString(line3, pins::DisplayWidth / 2, 112, 1);
}

void DisplayManager::keyValue(
    const char* title,
    const char* const* keys,
    const char* const* values,
    std::size_t count,
    const char* footerLeft,
    const char* footerRight
) {
    clear();
    header(title);
    footer(footerLeft, footerRight);
    const Theme& c = theme();
    const std::size_t visible = std::min<std::size_t>(count, 6U);
    for (std::size_t i = 0; i < visible; ++i) {
        const int16_t y = 33 + static_cast<int16_t>(i) * 18;
        tft_.setTextDatum(ML_DATUM);
        tft_.setTextColor(c.muted, c.background);
        tft_.drawString(keys[i], 9, y, 1);
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(c.text, c.background);
        tft_.drawString(values[i], pins::DisplayWidth - 9, y, 1);
    }
}

void DisplayManager::progress(const char* title, const char* label, float value, const char* footerLeft) {
    value = std::clamp(value, 0.0F, 1.0F);
    clear();
    header(title);
    footer(footerLeft, "");
    const Theme& c = theme();
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(c.text, c.background);
    tft_.drawString(label != nullptr ? label : "", pins::DisplayWidth / 2, 62, 2);
    constexpr int16_t barX = 26;
    constexpr int16_t barY = 86;
    constexpr int16_t barW = pins::DisplayWidth - 52;
    constexpr int16_t barH = 17;
    tft_.drawRoundRect(barX, barY, barW, barH, 5, c.muted);
    const int16_t fill = static_cast<int16_t>((barW - 4) * value);
    if (fill > 0) tft_.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 3, c.accent);
    char percent[12]{};
    std::snprintf(percent, sizeof(percent), "%u%%", static_cast<unsigned>(value * 100.0F));
    tft_.setTextColor(c.muted, c.background);
    tft_.drawString(percent, pins::DisplayWidth / 2, 118, 2);
}

void DisplayManager::applyBacklight() {
    if (!initialized_) return;
    const uint32_t duty = static_cast<uint32_t>(brightness_) * 255U / 100U;
    ledcWrite(BacklightChannel, duty);
}

}  // namespace fw

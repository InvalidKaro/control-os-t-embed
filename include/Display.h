#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "Settings.h"

namespace fw {

struct Theme {
    uint16_t background;
    uint16_t surface;
    uint16_t text;
    uint16_t muted;
    uint16_t accent;
    uint16_t accent2;
    uint16_t success;
    uint16_t warning;
    uint16_t error;
};

class DisplayManager final {
public:
    bool begin(uint8_t brightness);
    void setBrightness(uint8_t percent);
    uint8_t brightness() const;
    void setTheme(ThemeId theme);
    ThemeId themeId() const;
    const Theme& theme() const;
    TFT_eSPI& tft();

    void clear();
    void header(const char* title, const char* right = nullptr);
    void footer(const char* left, const char* right);
    void menu(const char* title, const char* const* items, std::size_t count, std::size_t selected, const char* status = nullptr);
    void message(const char* title, const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);
    void keyValue(const char* title, const char* const* keys, const char* const* values, std::size_t count, const char* footerLeft = "Back", const char* footerRight = "");
    void progress(const char* title, const char* label, float value, const char* footerLeft = "Back");

private:
    static const Theme& themeFor(ThemeId id);
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    void applyBacklight();

    TFT_eSPI tft_{};
    ThemeId themeId_ = ThemeId::Cyberpunk;
    uint8_t brightness_ = 72;
    bool initialized_ = false;
};

}  // namespace fw

#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <cstdint>

namespace fw {

enum class ThemeId : uint8_t {
    Dark = 0,
    Oled,
    Cyberpunk,
    Matrix,
    Retro,
    Light,
    Vaporwave,
    Count
};

struct SettingsData {
    uint8_t displayBrightness = 72;
    uint8_t ledBrightness = 96;
    uint8_t ledEffect = 5;
    ThemeId theme = ThemeId::Cyberpunk;
    float radioFrequencyMhz = 433.92F;
};

class SettingsManager final {
public:
    bool begin();
    void save();
    void resetDefaults();
    SettingsData& data();
    const SettingsData& data() const;

private:
    Preferences preferences_{};
    SettingsData data_{};
    bool opened_ = false;
};

}  // namespace fw

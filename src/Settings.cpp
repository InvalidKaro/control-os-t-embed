#include "Settings.h"

#include "Config.h"
#include "Log.h"

namespace fw {

namespace {
constexpr const char* Tag = "SETTINGS";
}

bool SettingsManager::begin() {
    opened_ = preferences_.begin("control-os", false);
    if (!opened_) {
        LOG_WARN(Tag, "Preferences unavailable; defaults active");
        resetDefaults();
        return false;
    }

    data_.displayBrightness = preferences_.getUChar("disp", config::DefaultBacklight);
    data_.ledBrightness = preferences_.getUChar("ledb", config::DefaultLedBrightness);
    data_.ledEffect = preferences_.getUChar("lede", 18);
    data_.theme = static_cast<ThemeId>(preferences_.getUChar("theme", static_cast<uint8_t>(ThemeId::Cyberpunk)));
    data_.radioFrequencyMhz = preferences_.getFloat("rfmhz", config::DefaultRadioFrequencyMhz);

    if (static_cast<uint8_t>(data_.theme) >= static_cast<uint8_t>(ThemeId::Count)) {
        data_.theme = ThemeId::Cyberpunk;
    }
    if (data_.displayBrightness > 100U) data_.displayBrightness = config::DefaultBacklight;

    LOG_INFO(Tag, "Loaded settings");
    return true;
}

void SettingsManager::save() {
    if (!opened_) {
        return;
    }

    preferences_.putUChar("disp", data_.displayBrightness);
    preferences_.putUChar("ledb", data_.ledBrightness);
    preferences_.putUChar("lede", data_.ledEffect);
    preferences_.putUChar("theme", static_cast<uint8_t>(data_.theme));
    preferences_.putFloat("rfmhz", data_.radioFrequencyMhz);
    LOG_INFO(Tag, "Saved settings");
}

void SettingsManager::resetDefaults() {
    data_.displayBrightness = config::DefaultBacklight;
    data_.ledBrightness = config::DefaultLedBrightness;
    data_.ledEffect = 18;
    data_.theme = ThemeId::Cyberpunk;
    data_.radioFrequencyMhz = config::DefaultRadioFrequencyMhz;
}

SettingsData& SettingsManager::data() {
    return data_;
}

const SettingsData& SettingsManager::data() const {
    return data_;
}

}  // namespace fw

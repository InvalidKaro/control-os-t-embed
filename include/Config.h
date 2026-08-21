#pragma once

#include <Arduino.h>

namespace fw::config {

inline constexpr char FirmwareName[] = "Control OS";
inline constexpr char FirmwareVersion[] = "0.9.0-rc2";
inline constexpr char BoardName[] = "LILYGO T-Embed CC1101 Plus";

inline constexpr uint32_t SerialBaud = 115200;
inline constexpr uint32_t UiFrameMs = 33;
inline constexpr uint32_t LedFrameMs = 8;
inline constexpr uint8_t DefaultBacklight = 72;
inline constexpr uint8_t DefaultLedBrightness = 96;

inline constexpr float DefaultRadioFrequencyMhz = 433.92F;
inline constexpr float DefaultRadioBitRateKbps = 4.8F;
inline constexpr float DefaultRadioDeviationKhz = 25.4F;
inline constexpr float DefaultRadioBandwidthKhz = 135.0F;
inline constexpr int8_t DefaultRadioPowerDbm = 10;
inline constexpr uint8_t DefaultRadioPreambleBits = 16;

inline constexpr uint32_t EncoderPollMs = 2;
inline constexpr uint32_t ButtonDebounceMs = 18;
inline constexpr uint32_t ButtonLongPressMs = 650;

}  // namespace fw::config

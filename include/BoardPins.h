#pragma once

#include <Arduino.h>

namespace fw::pins {

inline constexpr int8_t PowerEnable = 15;

inline constexpr int8_t SpiSck = 11;
inline constexpr int8_t SpiMosi = 9;
inline constexpr int8_t SpiMiso = 10;

inline constexpr int8_t DisplayCs = 41;
inline constexpr int8_t DisplayDc = 16;
inline constexpr int8_t DisplayBacklight = 21;
inline constexpr int8_t DisplayReset = -1;

inline constexpr int8_t EncoderA = 4;
inline constexpr int8_t EncoderB = 5;
inline constexpr int8_t EncoderKey = 0;
inline constexpr int8_t BackKey = 6;

inline constexpr int8_t LedData = 14;
inline constexpr uint8_t LedCount = 8;

inline constexpr int8_t I2cSda = 8;
inline constexpr int8_t I2cScl = 18;
inline constexpr uint8_t Pn532Address = 0x24;
inline constexpr uint8_t Bq27220Address = 0x55;
inline constexpr uint8_t Bq25896Address = 0x6B;
inline constexpr int8_t Pn532Reset = 45;
inline constexpr int8_t Pn532Irq = 17;

inline constexpr int8_t SdCs = 13;

inline constexpr int8_t Cc1101Cs = 12;
inline constexpr int8_t Cc1101Gdo0 = 3;
inline constexpr int8_t Cc1101Gdo2 = 38;
inline constexpr int8_t Cc1101Sw0 = 48;
inline constexpr int8_t Cc1101Sw1 = 47;

inline constexpr int8_t Nrf24Ce = 43;
inline constexpr int8_t Nrf24Cs = 44;

inline constexpr int8_t IrTx = 2;
inline constexpr int8_t IrRx = 1;

inline constexpr int8_t MicData = 42;
inline constexpr int8_t MicClock = 39;
inline constexpr int8_t SpeakerBclk = 46;
inline constexpr int8_t SpeakerLrclk = 40;
inline constexpr int8_t SpeakerData = 7;

inline constexpr uint16_t DisplayWidth = 320;
inline constexpr uint16_t DisplayHeight = 170;

}  // namespace fw::pins

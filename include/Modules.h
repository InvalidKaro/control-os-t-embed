#pragma once

#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <BLEDevice.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <RadioLib.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "BoardPins.h"
#include "Display.h"
#include "Input.h"
#include "LedEngine.h"
#include "Settings.h"

namespace fw {

enum class ScreenId : uint8_t {
    MainMenu = 0,
    RadioAnalyzer,
    RadioReceive,
    RadioTransmit,
    IrMonitor,
    IrTransmit,
    NfcReader,
    WifiScanner,
    BleScanner,
    SdBrowser,
    NrfScanner,
    LedStudio,
    SystemInfo,
    Settings,
    About
};

class HardwareServices final {
public:
    HardwareServices();

    bool begin();
    void stopActiveOperations();
    void deselectSpi();

    bool cc1101Ready() const;
    bool pn532Ready() const;
    bool sdReady() const;
    bool nrfReady() const;
    bool fuelGaugeReady() const;
    bool chargerReady() const;

    bool configureRadio(float frequencyMhz);
    bool startRadioReceive();
    bool pollRadioPacket(char* output, std::size_t outputSize, float& rssi, uint8_t& lqi);
    bool sendRadioTestPacket(const char* text);
    float radioRssi();
    int16_t radioState() const;

    bool pollIr(char* protocol, std::size_t protocolSize, uint64_t& value, uint16_t& bits);
    void resumeIr();
    void sendIrNec(uint32_t code);

    bool readNfcUid(char* output, std::size_t outputSize);

    void startWifiScan();
    bool wifiScanRunning() const;
    int wifiScanCount() const;
    void wifiScanEntry(int index, char* output, std::size_t outputSize) const;
    void clearWifiScan();

    void startBleScan(uint32_t seconds = 2);
    bool bleScanRunning() const;
    bool pollBleScanComplete();
    int bleScanCount();
    void bleScanEntry(int index, char* output, std::size_t outputSize);

    bool mountSd();
    int listSdRoot(std::array<std::array<char, 48>, 8>& entries);

    void scanNrf(std::array<uint8_t, 16>& bins);

    uint16_t batteryVoltageMv();
    uint8_t batteryPercent();
    bool charging();

private:
    static void IRAM_ATTR onRadioPacket();
    static void onBleScanComplete(BLEScanResults results);
    static volatile bool radioPacketFlag_;
    static volatile bool bleScanDone_;

    bool selectRfPath(float frequencyMhz);
    bool i2cPresent(uint8_t address);
    bool readWordLE(uint8_t address, uint8_t reg, uint16_t& value);
    uint8_t nrfReadRegister(uint8_t reg);
    void nrfWriteRegister(uint8_t reg, uint8_t value);
    bool initNrf24();

    SPIClass sharedSpi_;
    Module radioModule_;
    CC1101 cc1101_;
    Adafruit_PN532 pn532_;
    IRrecv irReceiver_;
    IRsend irSender_;
    decode_results irResults_{};

    bool cc1101Ready_ = false;
    bool pn532Ready_ = false;
    bool sdReady_ = false;
    bool nrfReady_ = false;
    bool fuelGaugeReady_ = false;
    bool chargerReady_ = false;
    bool radioReceiving_ = false;
    int16_t radioState_ = RADIOLIB_ERR_NONE;
    float currentFrequencyMhz_ = 433.92F;

    BLEScan* bleScan_ = nullptr;
    BLEScanResults bleResults_{};
    bool bleScanInProgress_ = false;
};

class ModulesUi final {
public:
    ModulesUi(HardwareServices& hw, DisplayManager& display, LedManager& leds, SettingsManager& settings);

    void enter(ScreenId screen);
    void leave();
    void tick(uint32_t now);
    void handleInput(const InputEvent& event);
    ScreenId screen() const;
    bool wantsExit() const;
    void clearExitRequest();

private:
    void drawRadioAnalyzer();
    void drawRadioReceive();
    void drawRadioTransmit();
    void drawIrMonitor();
    void drawIrTransmit();
    void drawNfcReader();
    void drawWifiScanner();
    void drawBleScanner();
    void drawSdBrowser();
    void drawNrfScanner();
    void drawLedStudio();
    void drawSystemInfo();
    void drawSettings();
    void drawAbout();
    void redraw();

    void rotateRadioFrequency(int direction);
    static float radioFrequencyForIndex(uint8_t index);
    static const char* radioLabelForIndex(uint8_t index);

    HardwareServices& hw_;
    DisplayManager& display_;
    LedManager& leds_;
    SettingsManager& settings_;

    ScreenId screen_ = ScreenId::MainMenu;
    bool exitRequested_ = false;
    bool dirty_ = true;
    uint32_t lastRefreshAt_ = 0;
    uint8_t radioFrequencyIndex_ = 1;
    uint8_t settingsRow_ = 0;
    uint8_t ledEffectIndex_ = static_cast<uint8_t>(LedEffect::Cyberpunk);

    char radioPacket_[80]{};
    float radioPacketRssi_ = 0.0F;
    uint8_t radioPacketLqi_ = 0;
    uint32_t radioRxCount_ = 0;

    char irProtocol_[24]{};
    uint64_t irValue_ = 0;
    uint16_t irBits_ = 0;
    bool irSeen_ = false;

    char nfcUid_[48]{};
    bool nfcSeen_ = false;

    std::array<std::array<char, 48>, 8> sdEntries_{};
    int sdEntryCount_ = 0;
    std::array<uint8_t, 16> nrfBins_{};
};

}  // namespace fw

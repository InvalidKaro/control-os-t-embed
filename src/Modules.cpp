#include "Modules.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "Config.h"
#include "Log.h"

namespace fw {

volatile bool HardwareServices::radioPacketFlag_ = false;
volatile bool HardwareServices::bleScanDone_ = false;

namespace {
constexpr const char* Tag = "HW";

void safeCopy(char* destination, std::size_t size, const char* source) {
    if (destination == nullptr || size == 0U) return;
    destination[0] = '\0';
    if (source == nullptr) return;
    std::strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

const char* yesNo(bool value) {
    return value ? "yes" : "no";
}
}

HardwareServices::HardwareServices()
    : sharedSpi_(HSPI),
      radioModule_(pins::Cc1101Cs, pins::Cc1101Gdo0, RADIOLIB_NC, pins::Cc1101Gdo2, sharedSpi_),
      cc1101_(&radioModule_),
      pn532_(pins::Pn532Irq, pins::Pn532Reset, &Wire),
      irReceiver_(pins::IrRx),
      irSender_(pins::IrTx) {
}

bool HardwareServices::begin() {
    pinMode(pins::PowerEnable, OUTPUT);
    digitalWrite(pins::PowerEnable, HIGH);

    pinMode(pins::DisplayCs, OUTPUT);
    pinMode(pins::SdCs, OUTPUT);
    pinMode(pins::Cc1101Cs, OUTPUT);
    pinMode(pins::Nrf24Cs, OUTPUT);
    pinMode(pins::Nrf24Ce, OUTPUT);
    pinMode(pins::Cc1101Sw0, OUTPUT);
    pinMode(pins::Cc1101Sw1, OUTPUT);
    deselectSpi();
    digitalWrite(pins::Nrf24Ce, LOW);
    digitalWrite(pins::Cc1101Sw0, LOW);
    digitalWrite(pins::Cc1101Sw1, LOW);

    sharedSpi_.begin(pins::SpiSck, pins::SpiMiso, pins::SpiMosi, -1);
    Wire.begin(pins::I2cSda, pins::I2cScl);
    Wire.setClock(400000);

    fuelGaugeReady_ = i2cPresent(pins::Bq27220Address);
    chargerReady_ = i2cPresent(pins::Bq25896Address);

    deselectSpi();
    pn532_.begin();
    const uint32_t pnVersion = pn532_.getFirmwareVersion();
    pn532Ready_ = pnVersion != 0U;
    if (pn532Ready_) {
        pn532_.SAMConfig();
    }

    irReceiver_.enableIRIn();
    irSender_.begin();

    nrfReady_ = initNrf24();

    sdReady_ = mountSd();
    cc1101Ready_ = configureRadio(config::DefaultRadioFrequencyMhz);

    LOG_INFO(Tag, "PN532=%s SD=%s CC1101=%s nRF24=%s BQ27220=%s BQ25896=%s",
        yesNo(pn532Ready_), yesNo(sdReady_), yesNo(cc1101Ready_), yesNo(nrfReady_), yesNo(fuelGaugeReady_), yesNo(chargerReady_));

    return true;
}

void HardwareServices::stopActiveOperations() {
    if (cc1101Ready_) {
        cc1101_.clearPacketReceivedAction();
        cc1101_.standby();
    }
    radioReceiving_ = false;
    radioPacketFlag_ = false;

    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);

    if (bleScan_ != nullptr && bleScanInProgress_) {
        bleScan_->stop();
    }
    bleScanInProgress_ = false;
    bleScanDone_ = false;
    bleResults_ = BLEScanResults{};
    if (BLEDevice::getInitialized()) {
        BLEDevice::deinit(false);
    }
    bleScan_ = nullptr;

    digitalWrite(pins::Nrf24Ce, LOW);
    deselectSpi();
}

void HardwareServices::deselectSpi() {
    digitalWrite(pins::DisplayCs, HIGH);
    digitalWrite(pins::SdCs, HIGH);
    digitalWrite(pins::Cc1101Cs, HIGH);
    digitalWrite(pins::Nrf24Cs, HIGH);
    digitalWrite(pins::Nrf24Ce, LOW);
}

bool HardwareServices::cc1101Ready() const { return cc1101Ready_; }
bool HardwareServices::pn532Ready() const { return pn532Ready_; }
bool HardwareServices::sdReady() const { return sdReady_; }
bool HardwareServices::nrfReady() const { return nrfReady_; }
bool HardwareServices::fuelGaugeReady() const { return fuelGaugeReady_; }
bool HardwareServices::chargerReady() const { return chargerReady_; }

bool HardwareServices::selectRfPath(float frequencyMhz) {
    if (frequencyMhz >= 280.0F && frequencyMhz <= 360.0F) {
        digitalWrite(pins::Cc1101Sw1, HIGH);
        digitalWrite(pins::Cc1101Sw0, LOW);
        return true;
    }
    if (frequencyMhz >= 380.0F && frequencyMhz <= 500.0F) {
        digitalWrite(pins::Cc1101Sw1, HIGH);
        digitalWrite(pins::Cc1101Sw0, HIGH);
        return true;
    }
    if (frequencyMhz >= 750.0F && frequencyMhz <= 960.0F) {
        digitalWrite(pins::Cc1101Sw1, LOW);
        digitalWrite(pins::Cc1101Sw0, HIGH);
        return true;
    }
    digitalWrite(pins::Cc1101Sw1, LOW);
    digitalWrite(pins::Cc1101Sw0, LOW);
    return false;
}

bool HardwareServices::configureRadio(float frequencyMhz) {
    if (cc1101Ready_) {
        cc1101_.clearPacketReceivedAction();
        cc1101_.standby();
    }
    deselectSpi();
    selectRfPath(frequencyMhz);
    currentFrequencyMhz_ = frequencyMhz;
    radioPacketFlag_ = false;
    radioReceiving_ = false;

    radioState_ = cc1101_.begin(
        frequencyMhz,
        config::DefaultRadioBitRateKbps,
        config::DefaultRadioDeviationKhz,
        config::DefaultRadioBandwidthKhz,
        config::DefaultRadioPowerDbm,
        config::DefaultRadioPreambleBits
    );

    cc1101Ready_ = radioState_ == RADIOLIB_ERR_NONE;
    if (cc1101Ready_) {
        cc1101_.setPacketReceivedAction(onRadioPacket);
    }
    return cc1101Ready_;
}

bool HardwareServices::startRadioReceive() {
    if (!cc1101Ready_) return false;
    deselectSpi();
    radioPacketFlag_ = false;
    radioState_ = cc1101_.startReceive();
    radioReceiving_ = radioState_ == RADIOLIB_ERR_NONE;
    return radioReceiving_;
}

bool HardwareServices::pollRadioPacket(char* output, std::size_t outputSize, float& rssi, uint8_t& lqi) {
    if (!radioReceiving_ || !radioPacketFlag_ || output == nullptr || outputSize < 2U) return false;
    radioPacketFlag_ = false;
    deselectSpi();

    const std::size_t availableLength = cc1101_.getPacketLength();
    if (availableLength == 0U) {
        cc1101_.startReceive();
        return false;
    }
    const std::size_t packetLength = std::min<std::size_t>(availableLength, outputSize - 1U);
    radioState_ = cc1101_.readData(reinterpret_cast<uint8_t*>(output), packetLength);
    output[packetLength] = '\0';
    rssi = cc1101_.getRSSI();
    lqi = cc1101_.getLQI();
    cc1101_.startReceive();

    return radioState_ == RADIOLIB_ERR_NONE;
}

bool HardwareServices::sendRadioTestPacket(const char* text) {
    if (!cc1101Ready_ || text == nullptr) return false;
    deselectSpi();
    radioReceiving_ = false;
    radioPacketFlag_ = false;
    cc1101_.standby();
    const int16_t txState = cc1101_.transmit(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
    const int16_t rxState = cc1101_.startReceive();
    radioReceiving_ = rxState == RADIOLIB_ERR_NONE;
    radioState_ = txState != RADIOLIB_ERR_NONE ? txState : rxState;
    return txState == RADIOLIB_ERR_NONE && radioReceiving_;
}

float HardwareServices::radioRssi() {
    if (!cc1101Ready_) return -127.0F;
    deselectSpi();
    return cc1101_.getRSSI();
}

int16_t HardwareServices::radioState() const { return radioState_; }

void IRAM_ATTR HardwareServices::onRadioPacket() {
    radioPacketFlag_ = true;
}

bool HardwareServices::pollIr(char* protocol, std::size_t protocolSize, uint64_t& value, uint16_t& bits) {
    if (!irReceiver_.decode(&irResults_)) return false;
    const String type = typeToString(irResults_.decode_type);
    safeCopy(protocol, protocolSize, type.c_str());
    value = irResults_.value;
    bits = irResults_.bits;
    return true;
}

void HardwareServices::resumeIr() {
    irReceiver_.resume();
}

void HardwareServices::sendIrNec(uint32_t code) {
    irReceiver_.disableIRIn();
    irSender_.sendNEC(code, 32);
    irReceiver_.enableIRIn();
}

bool HardwareServices::readNfcUid(char* output, std::size_t outputSize) {
    if (!pn532Ready_ || output == nullptr || outputSize == 0U) return false;
    uint8_t uid[7]{};
    uint8_t length = 0;
    const bool found = pn532_.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &length, 75);
    if (!found) return false;

    std::size_t used = 0;
    for (uint8_t i = 0; i < length && used + 4U < outputSize; ++i) {
        const int written = std::snprintf(output + used, outputSize - used, i == 0U ? "%02X" : ":%02X", uid[i]);
        if (written <= 0) break;
        used += static_cast<std::size_t>(written);
    }
    return true;
}

void HardwareServices::startWifiScan() {
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true, false, 250U);
}

bool HardwareServices::wifiScanRunning() const {
    return WiFi.scanComplete() == WIFI_SCAN_RUNNING;
}

int HardwareServices::wifiScanCount() const {
    const int result = WiFi.scanComplete();
    return result >= 0 ? result : 0;
}

void HardwareServices::wifiScanEntry(int index, char* output, std::size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    const int count = wifiScanCount();
    if (index < 0 || index >= count) {
        safeCopy(output, outputSize, "-");
        return;
    }
    const String ssid = WiFi.SSID(index);
    std::snprintf(output, outputSize, "%s  %ld dBm  ch%d", ssid.c_str(), static_cast<long>(WiFi.RSSI(index)), WiFi.channel(index));
}

void HardwareServices::clearWifiScan() {
    WiFi.scanDelete();
}

void HardwareServices::onBleScanComplete(BLEScanResults results) {
    static_cast<void>(results);
    bleScanDone_ = true;
}

void HardwareServices::startBleScan(uint32_t seconds) {
    if (bleScan_ == nullptr) {
        BLEDevice::init("");
        bleScan_ = BLEDevice::getScan();
        if (bleScan_ == nullptr) {
            bleScanInProgress_ = false;
            return;
        }
        bleScan_->setActiveScan(false);
        bleScan_->setInterval(100);
        bleScan_->setWindow(80);
    }
    if (bleScanInProgress_) {
        bleScan_->stop();
        bleScanInProgress_ = false;
    }
    bleScan_->clearResults();
    bleResults_ = BLEScanResults{};
    bleScanDone_ = false;
    bleScanInProgress_ = bleScan_->start(seconds, onBleScanComplete, false);
}

bool HardwareServices::bleScanRunning() const {
    return bleScanInProgress_;
}

bool HardwareServices::pollBleScanComplete() {
    if (!bleScanInProgress_ || !bleScanDone_ || bleScan_ == nullptr) return false;
    bleResults_ = bleScan_->getResults();
    bleScanDone_ = false;
    bleScanInProgress_ = false;
    return true;
}

int HardwareServices::bleScanCount() {
    return bleResults_.getCount();
}

void HardwareServices::bleScanEntry(int index, char* output, std::size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    if (index < 0 || index >= bleResults_.getCount()) {
        safeCopy(output, outputSize, "-");
        return;
    }
    const BLEAdvertisedDevice device = bleResults_.getDevice(index);
    const std::string name = device.haveName() ? device.getName() : std::string("<unnamed>");
    std::snprintf(output, outputSize, "%s  %d dBm", name.c_str(), device.getRSSI());
}

bool HardwareServices::mountSd() {
    deselectSpi();
    sdReady_ = SD.begin(pins::SdCs, sharedSpi_, 8000000U);
    return sdReady_;
}

int HardwareServices::listSdRoot(std::array<std::array<char, 48>, 8>& entries) {
    for (auto& entry : entries) entry.fill('\0');
    if (!sdReady_ && !mountSd()) return 0;
    deselectSpi();
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return 0;

    int count = 0;
    while (count < static_cast<int>(entries.size())) {
        File file = root.openNextFile();
        if (!file) break;
        std::snprintf(entries[static_cast<std::size_t>(count)].data(), entries[static_cast<std::size_t>(count)].size(), "%s%s", file.name(), file.isDirectory() ? "/" : "");
        ++count;
        file.close();
    }
    root.close();
    return count;
}

uint8_t HardwareServices::nrfReadRegister(uint8_t reg) {
    deselectSpi();
    sharedSpi_.beginTransaction(SPISettings(8000000U, MSBFIRST, SPI_MODE0));
    digitalWrite(pins::Nrf24Cs, LOW);
    sharedSpi_.transfer(reg & 0x1FU);
    const uint8_t value = sharedSpi_.transfer(0xFFU);
    digitalWrite(pins::Nrf24Cs, HIGH);
    sharedSpi_.endTransaction();
    return value;
}

void HardwareServices::nrfWriteRegister(uint8_t reg, uint8_t value) {
    deselectSpi();
    sharedSpi_.beginTransaction(SPISettings(8000000U, MSBFIRST, SPI_MODE0));
    digitalWrite(pins::Nrf24Cs, LOW);
    sharedSpi_.transfer(static_cast<uint8_t>(0x20U | (reg & 0x1FU)));
    sharedSpi_.transfer(value);
    digitalWrite(pins::Nrf24Cs, HIGH);
    sharedSpi_.endTransaction();
}

bool HardwareServices::initNrf24() {
    digitalWrite(pins::Nrf24Ce, LOW);
    nrfWriteRegister(0x05U, 42U);
    const uint8_t verify = nrfReadRegister(0x05U);
    if (verify != 42U) {
        return false;
    }
    // CONFIG: PWR_UP + PRIM_RX. Receiver only; no TX mode is used by this firmware.
    nrfWriteRegister(0x00U, 0x03U);
    // RF_SETUP: 1 Mbps, minimum practical receive configuration.
    nrfWriteRegister(0x06U, 0x06U);
    delayMicroseconds(1600U);
    return true;
}

void HardwareServices::scanNrf(std::array<uint8_t, 16>& bins) {
    bins.fill(0);
    if (!nrfReady_) return;

    // RPD is a receive-only energy indication above roughly -64 dBm.
    for (uint8_t channel = 0; channel < 126U; ++channel) {
        nrfWriteRegister(0x05U, channel);
        digitalWrite(pins::Nrf24Ce, HIGH);
        delayMicroseconds(180U);
        const bool hit = (nrfReadRegister(0x09U) & 0x01U) != 0U;
        digitalWrite(pins::Nrf24Ce, LOW);
        if (hit) {
            const std::size_t bin = std::min<std::size_t>(bins.size() - 1U, static_cast<std::size_t>(channel) * bins.size() / 126U);
            if (bins[bin] < 255U) ++bins[bin];
        }
    }
}

bool HardwareServices::i2cPresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool HardwareServices::readWordLE(uint8_t address, uint8_t reg, uint16_t& value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(address), 2) != 2) return false;
    const uint8_t low = Wire.read();
    const uint8_t high = Wire.read();
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8U));
    return true;
}

uint16_t HardwareServices::batteryVoltageMv() {
    uint16_t value = 0;
    if (!fuelGaugeReady_ || !readWordLE(pins::Bq27220Address, 0x08, value)) return 0;
    return value;
}

uint8_t HardwareServices::batteryPercent() {
    uint16_t value = 0;
    if (!fuelGaugeReady_ || !readWordLE(pins::Bq27220Address, 0x2C, value)) return 0;
    return static_cast<uint8_t>(std::min<uint16_t>(100U, value));
}

bool HardwareServices::charging() {
    if (!chargerReady_) return false;
    Wire.beginTransmission(pins::Bq25896Address);
    Wire.write(0x0B);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(pins::Bq25896Address), 1) != 1) return false;
    const uint8_t status = Wire.read();
    const uint8_t chargeState = static_cast<uint8_t>((status >> 3U) & 0x03U);
    return chargeState == 1U || chargeState == 2U;
}

ModulesUi::ModulesUi(HardwareServices& hw, DisplayManager& display, LedManager& leds, SettingsManager& settings)
    : hw_(hw), display_(display), leds_(leds), settings_(settings) {
    float bestDistance = 10000.0F;
    for (uint8_t index = 0; index < 4U; ++index) {
        const float candidateDistance = std::fabs(settings_.data().radioFrequencyMhz - radioFrequencyForIndex(index));
        if (candidateDistance < bestDistance) {
            bestDistance = candidateDistance;
            radioFrequencyIndex_ = index;
        }
    }
}

void ModulesUi::enter(ScreenId screen) {
    screen_ = screen;
    exitRequested_ = false;
    dirty_ = true;
    lastRefreshAt_ = 0;

    switch (screen_) {
        case ScreenId::RadioAnalyzer:
        case ScreenId::RadioReceive:
        case ScreenId::RadioTransmit:
            hw_.configureRadio(radioFrequencyForIndex(radioFrequencyIndex_));
            hw_.startRadioReceive();
            break;
        case ScreenId::NfcReader:
            nfcSeen_ = hw_.readNfcUid(nfcUid_, sizeof(nfcUid_));
            break;
        case ScreenId::WifiScanner:
            hw_.startWifiScan();
            break;
        case ScreenId::BleScanner:
            hw_.startBleScan(2);
            break;
        case ScreenId::SdBrowser:
            sdEntryCount_ = hw_.listSdRoot(sdEntries_);
            break;
        case ScreenId::NrfScanner:
            hw_.scanNrf(nrfBins_);
            break;
        case ScreenId::LedStudio:
            ledEffectIndex_ = static_cast<uint8_t>(leds_.effect());
            break;
        default:
            break;
    }
    redraw();
}

void ModulesUi::leave() {
    hw_.stopActiveOperations();
    settings_.save();
}

void ModulesUi::tick(uint32_t now) {
    switch (screen_) {
        case ScreenId::RadioAnalyzer:
            if (now - lastRefreshAt_ >= 170U) {
                lastRefreshAt_ = now;
                dirty_ = true;
            }
            break;
        case ScreenId::RadioReceive: {
            float rssi = 0.0F;
            uint8_t lqi = 0;
            if (hw_.pollRadioPacket(radioPacket_, sizeof(radioPacket_), rssi, lqi)) {
                radioPacketRssi_ = rssi;
                radioPacketLqi_ = lqi;
                ++radioRxCount_;
                leds_.trigger(LedOverlay::RadioRx, 320);
                dirty_ = true;
            }
            if (now - lastRefreshAt_ >= 250U) {
                lastRefreshAt_ = now;
                dirty_ = true;
            }
            break;
        }
        case ScreenId::IrMonitor: {
            char protocol[24]{};
            uint64_t value = 0;
            uint16_t bits = 0;
            if (hw_.pollIr(protocol, sizeof(protocol), value, bits)) {
                safeCopy(irProtocol_, sizeof(irProtocol_), protocol);
                irValue_ = value;
                irBits_ = bits;
                irSeen_ = true;
                hw_.resumeIr();
                leds_.trigger(LedOverlay::Notification, 260);
                dirty_ = true;
            }
            break;
        }
        case ScreenId::WifiScanner:
            if (!hw_.wifiScanRunning() && now - lastRefreshAt_ >= 300U) {
                lastRefreshAt_ = now;
                dirty_ = true;
            }
            break;
        case ScreenId::BleScanner:
            if (hw_.pollBleScanComplete()) {
                dirty_ = true;
            }
            break;
        default:
            break;
    }

    if (dirty_) redraw();
}

void ModulesUi::handleInput(const InputEvent& event) {
    if (event.type == InputEventType::BackClick) {
        exitRequested_ = true;
        return;
    }

    if (event.type == InputEventType::Rotate) {
        const int direction = event.delta >= 0 ? 1 : -1;
        if (screen_ == ScreenId::RadioAnalyzer || screen_ == ScreenId::RadioReceive || screen_ == ScreenId::RadioTransmit) {
            rotateRadioFrequency(direction);
        } else if (screen_ == ScreenId::LedStudio) {
            leds_.nextEffect(direction);
            ledEffectIndex_ = static_cast<uint8_t>(leds_.effect());
            settings_.data().ledEffect = ledEffectIndex_;
            leds_.trigger(LedOverlay::Rotation, 220);
            dirty_ = true;
        } else if (screen_ == ScreenId::Settings) {
            int next = static_cast<int>(settingsRow_) + direction;
            next %= 4;
            if (next < 0) next += 4;
            settingsRow_ = static_cast<uint8_t>(next);
            dirty_ = true;
        }
        return;
    }

    if (event.type == InputEventType::EncoderClick) {
        switch (screen_) {
            case ScreenId::RadioTransmit:
                leds_.trigger(LedOverlay::Warning, 220);
                dirty_ = true;
                break;
            case ScreenId::IrTransmit:
                hw_.sendIrNec(0x00FF00FFU);
                leds_.trigger(LedOverlay::Success, 350);
                dirty_ = true;
                break;
            case ScreenId::NfcReader:
                nfcSeen_ = hw_.readNfcUid(nfcUid_, sizeof(nfcUid_));
                leds_.trigger(nfcSeen_ ? LedOverlay::Success : LedOverlay::Warning, 320);
                dirty_ = true;
                break;
            case ScreenId::WifiScanner:
                hw_.startWifiScan();
                dirty_ = true;
                break;
            case ScreenId::BleScanner:
                hw_.startBleScan(2);
                dirty_ = true;
                break;
            case ScreenId::SdBrowser:
                sdEntryCount_ = hw_.listSdRoot(sdEntries_);
                dirty_ = true;
                break;
            case ScreenId::NrfScanner:
                hw_.scanNrf(nrfBins_);
                dirty_ = true;
                break;
            case ScreenId::LedStudio:
                leds_.trigger(LedOverlay::Click, 190);
                settings_.save();
                dirty_ = true;
                break;
            case ScreenId::Settings: {
                SettingsData& s = settings_.data();
                if (settingsRow_ == 0U) {
                    s.displayBrightness = static_cast<uint8_t>((s.displayBrightness + 10U) % 110U);
                    if (s.displayBrightness == 0U) s.displayBrightness = 10U;
                    display_.setBrightness(s.displayBrightness);
                } else if (settingsRow_ == 1U) {
                    const uint16_t next = static_cast<uint16_t>(s.ledBrightness) + 24U;
                    s.ledBrightness = static_cast<uint8_t>(next > 255U ? 32U : next);
                    leds_.setBrightness(s.ledBrightness);
                } else if (settingsRow_ == 2U) {
                    uint8_t theme = static_cast<uint8_t>(s.theme) + 1U;
                    theme %= static_cast<uint8_t>(ThemeId::Count);
                    s.theme = static_cast<ThemeId>(theme);
                    display_.setTheme(s.theme);
                } else {
                    settings_.save();
                    leds_.trigger(LedOverlay::Success, 350);
                }
                dirty_ = true;
                break;
            }
            default:
                break;
        }
    }

    if (event.type == InputEventType::EncoderLongPress) {
        if (screen_ == ScreenId::RadioTransmit) {
            char payload[48]{};
            std::snprintf(payload, sizeof(payload), "CONTROL-OS TEST %.2f", radioFrequencyForIndex(radioFrequencyIndex_));
            const bool ok = hw_.sendRadioTestPacket(payload);
            leds_.trigger(ok ? LedOverlay::RadioTx : LedOverlay::Error, 420);
            dirty_ = true;
        } else if (screen_ == ScreenId::Settings) {
            settings_.resetDefaults();
            display_.setBrightness(settings_.data().displayBrightness);
            display_.setTheme(settings_.data().theme);
            leds_.setBrightness(settings_.data().ledBrightness);
            leds_.setEffect(static_cast<LedEffect>(settings_.data().ledEffect));
            settings_.save();
            leds_.trigger(LedOverlay::Warning, 500);
            dirty_ = true;
        }
    }
}

ScreenId ModulesUi::screen() const { return screen_; }
bool ModulesUi::wantsExit() const { return exitRequested_; }
void ModulesUi::clearExitRequest() { exitRequested_ = false; }

void ModulesUi::rotateRadioFrequency(int direction) {
    int next = static_cast<int>(radioFrequencyIndex_) + direction;
    next %= 4;
    if (next < 0) next += 4;
    radioFrequencyIndex_ = static_cast<uint8_t>(next);
    settings_.data().radioFrequencyMhz = radioFrequencyForIndex(radioFrequencyIndex_);
    hw_.configureRadio(settings_.data().radioFrequencyMhz);
    hw_.startRadioReceive();
    leds_.trigger(LedOverlay::Rotation, 220);
    dirty_ = true;
}

float ModulesUi::radioFrequencyForIndex(uint8_t index) {
    constexpr float values[] = {315.0F, 433.92F, 868.0F, 915.0F};
    return values[index % 4U];
}

const char* ModulesUi::radioLabelForIndex(uint8_t index) {
    constexpr const char* labels[] = {"315.000 MHz", "433.920 MHz", "868.000 MHz", "915.000 MHz"};
    return labels[index % 4U];
}

void ModulesUi::redraw() {
    dirty_ = false;
    switch (screen_) {
        case ScreenId::RadioAnalyzer: drawRadioAnalyzer(); break;
        case ScreenId::RadioReceive: drawRadioReceive(); break;
        case ScreenId::RadioTransmit: drawRadioTransmit(); break;
        case ScreenId::IrMonitor: drawIrMonitor(); break;
        case ScreenId::IrTransmit: drawIrTransmit(); break;
        case ScreenId::NfcReader: drawNfcReader(); break;
        case ScreenId::WifiScanner: drawWifiScanner(); break;
        case ScreenId::BleScanner: drawBleScanner(); break;
        case ScreenId::SdBrowser: drawSdBrowser(); break;
        case ScreenId::NrfScanner: drawNrfScanner(); break;
        case ScreenId::LedStudio: drawLedStudio(); break;
        case ScreenId::SystemInfo: drawSystemInfo(); break;
        case ScreenId::Settings: drawSettings(); break;
        case ScreenId::About: drawAbout(); break;
        case ScreenId::MainMenu: break;
    }
}

void ModulesUi::drawRadioAnalyzer() {
    char rssiText[24]{};
    char stateText[24]{};
    char batteryText[24]{};
    const float rssi = hw_.cc1101Ready() ? hw_.radioRssi() : -127.0F;
    std::snprintf(rssiText, sizeof(rssiText), "%.1f dBm", static_cast<double>(rssi));
    std::snprintf(stateText, sizeof(stateText), "%s (%d)", hw_.cc1101Ready() ? "ready" : "offline", hw_.radioState());
    std::snprintf(batteryText, sizeof(batteryText), "%u%% / %umV", hw_.batteryPercent(), hw_.batteryVoltageMv());
    const char* keys[] = {"Frequency", "RSSI", "CC1101", "Battery", "RF path"};
    const char* vals[] = {radioLabelForIndex(radioFrequencyIndex_), rssiText, stateText, batteryText, radioLabelForIndex(radioFrequencyIndex_)};
    display_.keyValue("SUB-GHZ ANALYZER", keys, vals, 5, "Back", "Rotate: band");
}

void ModulesUi::drawRadioReceive() {
    char countText[16]{};
    char rssiText[20]{};
    char lqiText[16]{};
    std::snprintf(countText, sizeof(countText), "%lu", static_cast<unsigned long>(radioRxCount_));
    std::snprintf(rssiText, sizeof(rssiText), "%.1f dBm", static_cast<double>(radioPacketRssi_));
    std::snprintf(lqiText, sizeof(lqiText), "%u", radioPacketLqi_);
    const char* keys[] = {"Frequency", "Packets", "RSSI", "LQI", "Last data"};
    const char* vals[] = {radioLabelForIndex(radioFrequencyIndex_), countText, rssiText, lqiText, radioPacket_[0] != '\0' ? radioPacket_ : "waiting..."};
    display_.keyValue("SUB-GHZ RX", keys, vals, 5, "Back", "Rotate: band");
}

void ModulesUi::drawRadioTransmit() {
    const char* keys[] = {"Frequency", "Mode", "Payload", "Power"};
    const char* vals[] = {radioLabelForIndex(radioFrequencyIndex_), "manual test packet", "CONTROL-OS TEST", "check local rules"};
    display_.keyValue("SUB-GHZ TEST TX", keys, vals, 4, "Back", "Hold: transmit");
}

void ModulesUi::drawIrMonitor() {
    char valueText[28]{};
    char bitsText[12]{};
    if (irSeen_) {
        std::snprintf(valueText, sizeof(valueText), "0x%08lX", static_cast<unsigned long>(irValue_ & 0xFFFFFFFFULL));
        std::snprintf(bitsText, sizeof(bitsText), "%u", irBits_);
    } else {
        safeCopy(valueText, sizeof(valueText), "waiting...");
        safeCopy(bitsText, sizeof(bitsText), "-");
    }
    const char* keys[] = {"Protocol", "Bits", "Value"};
    const char* vals[] = {irSeen_ ? irProtocol_ : "-", bitsText, valueText};
    display_.keyValue("IR MONITOR", keys, vals, 3, "Back", "Aim remote at RX");
}

void ModulesUi::drawIrTransmit() {
    const char* keys[] = {"Protocol", "Bits", "Code", "Safety"};
    const char* vals[] = {"NEC", "32", "0x00FF00FF", "manual press only"};
    display_.keyValue("IR TEST TX", keys, vals, 4, "Back", "Press: send");
}

void ModulesUi::drawNfcReader() {
    const char* keys[] = {"PN532", "Type", "UID"};
    const char* vals[] = {hw_.pn532Ready() ? "ready" : "offline", "ISO14443A", nfcSeen_ ? nfcUid_ : "no tag"};
    display_.keyValue("NFC READER", keys, vals, 3, "Back", "Press: rescan");
}

void ModulesUi::drawWifiScanner() {
    display_.clear();
    display_.header("WI-FI SCANNER", hw_.wifiScanRunning() ? "scanning..." : "passive scan");
    display_.footer("Back", "Press: rescan");
    TFT_eSPI& tft = display_.tft();
    const Theme& c = display_.theme();
    tft.setTextDatum(ML_DATUM);
    const int count = hw_.wifiScanCount();
    for (int i = 0; i < std::min(count, 6); ++i) {
        char line[64]{};
        hw_.wifiScanEntry(i, line, sizeof(line));
        tft.setTextColor(i == 0 ? c.accent : c.text, c.background);
        tft.drawString(line, 8, 35 + i * 18, 1);
    }
    if (count == 0 && !hw_.wifiScanRunning()) {
        tft.setTextColor(c.muted, c.background);
        tft.drawString("No networks found", 8, 45, 2);
    }
}

void ModulesUi::drawBleScanner() {
    display_.clear();
    display_.header("BLE SCANNER", hw_.bleScanRunning() ? "scanning..." : "passive");
    display_.footer("Back", "Press: rescan");
    TFT_eSPI& tft = display_.tft();
    const Theme& c = display_.theme();
    tft.setTextDatum(ML_DATUM);
    const int count = hw_.bleScanCount();
    for (int i = 0; i < std::min(count, 6); ++i) {
        char line[64]{};
        hw_.bleScanEntry(i, line, sizeof(line));
        tft.setTextColor(i == 0 ? c.accent : c.text, c.background);
        tft.drawString(line, 8, 35 + i * 18, 1);
    }
    if (count == 0) {
        tft.setTextColor(c.muted, c.background);
        tft.drawString("No BLE advertisers found", 8, 45, 2);
    }
}

void ModulesUi::drawSdBrowser() {
    display_.clear();
    display_.header("SD BROWSER", hw_.sdReady() ? "mounted" : "offline");
    display_.footer("Back", "Press: refresh");
    TFT_eSPI& tft = display_.tft();
    const Theme& c = display_.theme();
    tft.setTextDatum(ML_DATUM);
    for (int i = 0; i < sdEntryCount_; ++i) {
        tft.setTextColor(i == 0 ? c.accent : c.text, c.background);
        tft.drawString(sdEntries_[static_cast<std::size_t>(i)].data(), 8, 35 + i * 15, 1);
    }
    if (sdEntryCount_ == 0) {
        tft.setTextColor(c.muted, c.background);
        tft.drawString(hw_.sdReady() ? "Root is empty" : "SD card not mounted", 8, 45, 2);
    }
}

void ModulesUi::drawNrfScanner() {
    display_.clear();
    display_.header("nRF24 SCANNER", hw_.nrfReady() ? "2.4 GHz" : "offline");
    display_.footer("Back", "Press: rescan");
    TFT_eSPI& tft = display_.tft();
    const Theme& c = display_.theme();
    constexpr int x0 = 10;
    constexpr int baseY = 135;
    constexpr int width = 18;
    constexpr int gap = 1;
    for (std::size_t i = 0; i < nrfBins_.size(); ++i) {
        const int height = std::min<int>(90, static_cast<int>(nrfBins_[i]) * 10);
        tft.fillRect(x0 + static_cast<int>(i) * (width + gap), baseY - height, width, height, height > 30 ? c.accent : c.accent2);
    }
    tft.setTextColor(c.muted, c.background);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("2400", 8, 40, 1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("2525 MHz", pins::DisplayWidth - 8, 40, 1);
}

void ModulesUi::drawLedStudio() {
    char indexText[18]{};
    std::snprintf(indexText, sizeof(indexText), "%u / %u", static_cast<unsigned>(static_cast<uint8_t>(leds_.effect()) + 1U), static_cast<unsigned>(static_cast<uint8_t>(LedEffect::Count)));
    const char* keys[] = {"Effect", "Index", "Brightness", "Reactive overlay"};
    char brightnessText[16]{};
    std::snprintf(brightnessText, sizeof(brightnessText), "%u / 255", leds_.brightness());
    const char* vals[] = {LedManager::effectName(leds_.effect()), indexText, brightnessText, "encoder + status"};
    display_.keyValue("LED STUDIO", keys, vals, 4, "Back", "Rotate: preview");
}

void ModulesUi::drawSystemInfo() {
    char heapText[20]{};
    char psramText[20]{};
    char flashText[20]{};
    char batteryText[24]{};
    char hardwareText[48]{};
    std::snprintf(heapText, sizeof(heapText), "%lu KB", static_cast<unsigned long>(ESP.getFreeHeap() / 1024U));
    std::snprintf(psramText, sizeof(psramText), "%lu / %lu KB", static_cast<unsigned long>(ESP.getFreePsram() / 1024U), static_cast<unsigned long>(ESP.getPsramSize() / 1024U));
    std::snprintf(flashText, sizeof(flashText), "%lu MB", static_cast<unsigned long>(ESP.getFlashChipSize() / (1024U * 1024U)));
    std::snprintf(batteryText, sizeof(batteryText), "%u%% %umV%s", hw_.batteryPercent(), hw_.batteryVoltageMv(), hw_.charging() ? " charging" : "");
    std::snprintf(hardwareText, sizeof(hardwareText), "RF:%c NFC:%c SD:%c NRF:%c", hw_.cc1101Ready() ? 'Y' : 'N', hw_.pn532Ready() ? 'Y' : 'N', hw_.sdReady() ? 'Y' : 'N', hw_.nrfReady() ? 'Y' : 'N');
    const char* keys[] = {"Free heap", "PSRAM", "Flash", "Battery", "Peripherals", "CPU"};
    const char* vals[] = {heapText, psramText, flashText, batteryText, hardwareText, "ESP32-S3 240MHz"};
    display_.keyValue("SYSTEM INFO", keys, vals, 6, "Back", "");
}

void ModulesUi::drawSettings() {
    display_.clear();
    display_.header("SETTINGS", "NVS");
    display_.footer("Back", "Press: change");
    TFT_eSPI& tft = display_.tft();
    const Theme& c = display_.theme();
    const SettingsData& s = settings_.data();
    char displayBrightness[18]{};
    char ledBrightness[18]{};
    char themeName[18]{};
    std::snprintf(displayBrightness, sizeof(displayBrightness), "%u%%", s.displayBrightness);
    std::snprintf(ledBrightness, sizeof(ledBrightness), "%u", s.ledBrightness);
    constexpr const char* themeNames[] = {"Dark", "OLED", "Cyberpunk", "Matrix", "Retro", "Light", "Vaporwave"};
    safeCopy(themeName, sizeof(themeName), themeNames[static_cast<uint8_t>(s.theme) % 7U]);
    const char* labels[] = {"Display brightness", "LED brightness", "Theme", "Save settings"};
    const char* values[] = {displayBrightness, ledBrightness, themeName, "write NVS"};
    for (uint8_t i = 0; i < 4U; ++i) {
        const int16_t y = 37 + i * 25;
        if (i == settingsRow_) {
            tft.fillRoundRect(7, y - 5, pins::DisplayWidth - 14, 22, 4, c.accent);
            tft.setTextColor(c.background, c.accent);
        } else {
            tft.setTextColor(c.text, c.background);
        }
        tft.setTextDatum(ML_DATUM);
        tft.drawString(labels[i], 12, y + 5, 1);
        tft.setTextDatum(MR_DATUM);
        tft.drawString(values[i], pins::DisplayWidth - 12, y + 5, 1);
    }
}

void ModulesUi::drawAbout() {
    const char* keys[] = {"Firmware", "Version", "Board", "Bruce", "Policy", "License"};
    const char* vals[] = {config::FirmwareName, config::FirmwareVersion, "T-Embed CC1101 Plus", "architecture/features inspired", "no jammer/deauth/theft", "AGPL-3.0-or-later"};
    display_.keyValue("ABOUT", keys, vals, 6, "Back", "");
}

}  // namespace fw

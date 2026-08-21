#include "LedEngine.h"

#include <algorithm>
#include <cmath>

#include "Config.h"
#include "Log.h"

namespace fw {

namespace {
constexpr const char* Tag = "LED";
constexpr float Pi = 3.14159265358979323846F;
constexpr float Tau = Pi * 2.0F;

uint8_t q8(float value) {
    if (value <= 0.0F) return 0;
    if (value >= 255.0F) return 255;
    return static_cast<uint8_t>(value);
}

CRGB mix(const CRGB& a, const CRGB& b, uint8_t amount) {
    return blend(a, b, amount);
}

CRGB hsv(uint8_t h, uint8_t s = 255, uint8_t v = 255) {
    CRGB out;
    hsv2rgb_rainbow(CHSV(h, s, v), out);
    return out;
}
}

bool LedManager::begin(uint8_t brightness, LedEffect effect) {
    brightness_ = brightness;
    effect_ = effect < LedEffect::Count ? effect : LedEffect::Cyberpunk;
    FastLED.addLeds<WS2812B, pins::LedData, GRB>(leds_.data(), static_cast<int>(pins::LedCount));
    FastLED.setBrightness(brightness_);
    FastLED.clear(true);
    LOG_INFO(Tag, "WS2812 ring ready: %u LEDs, effect=%s", static_cast<unsigned>(pins::LedCount), effectName(effect_));
    return true;
}

void LedManager::tick(uint32_t now, const EncoderState& encoder) {
    if (now - lastFrameAt_ < config::LedFrameMs) return;
    lastFrameAt_ = now;

    Frame frame{};
    clear(frame);
    renderBase(now, encoder, frame);
    renderOverlay(now, encoder, frame);
    leds_ = frame;
    FastLED.show();
}

void LedManager::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    FastLED.setBrightness(brightness_);
}

uint8_t LedManager::brightness() const {
    return brightness_;
}

void LedManager::setEffect(LedEffect effect) {
    if (effect >= LedEffect::Count) effect = LedEffect::Cyberpunk;
    effect_ = effect;
}

LedEffect LedManager::effect() const {
    return effect_;
}

void LedManager::nextEffect(int direction) {
    int value = static_cast<int>(effect_) + (direction >= 0 ? 1 : -1);
    const int count = static_cast<int>(LedEffect::Count);
    value %= count;
    if (value < 0) value += count;
    effect_ = static_cast<LedEffect>(value);
}

void LedManager::trigger(LedOverlay overlay, uint32_t durationMs) {
    overlay_ = overlay;
    overlayStartedAt_ = millis();
    overlayUntil_ = overlayStartedAt_ + durationMs;
}

const char* LedManager::effectName(LedEffect effect) {
    switch (effect) {
        case LedEffect::Off: return "Off";
        case LedEffect::Solid: return "Solid";
        case LedEffect::Breathing: return "Breathing";
        case LedEffect::SlowPulse: return "Slow Pulse";
        case LedEffect::Rainbow: return "Rainbow";
        case LedEffect::RainbowCycle: return "Rainbow Cycle";
        case LedEffect::GradientRotate: return "Gradient Rotate";
        case LedEffect::Aurora: return "Aurora";
        case LedEffect::Comet: return "Comet";
        case LedEffect::DualComet: return "Dual Comet";
        case LedEffect::Scanner: return "Scanner";
        case LedEffect::LarsonScanner: return "Larson Scanner";
        case LedEffect::Orbit: return "Orbit";
        case LedEffect::Bounce: return "Bounce";
        case LedEffect::Fire: return "Fire";
        case LedEffect::Ice: return "Ice";
        case LedEffect::Ocean: return "Ocean";
        case LedEffect::Matrix: return "Matrix";
        case LedEffect::Cyberpunk: return "Cyberpunk";
        case LedEffect::Vaporwave: return "Vaporwave";
        case LedEffect::Sparkle: return "Sparkle";
        case LedEffect::Twinkle: return "Twinkle";
        case LedEffect::Reactor: return "Reactor";
        case LedEffect::Portal: return "Portal";
        case LedEffect::Glitch: return "Glitch";
        case LedEffect::Count: break;
    }
    return "Unknown";
}

void LedManager::renderBase(uint32_t now, const EncoderState& encoder, Frame& frame) {
    const float t = static_cast<float>(now) / 1000.0F;

    switch (effect_) {
        case LedEffect::Off:
            return;

        case LedEffect::Solid:
            frame.fill(CRGB(0, 150, 220));
            return;

        case LedEffect::Breathing: {
            const uint8_t value = q8((0.12F + 0.88F * (std::sin(t * Pi) * 0.5F + 0.5F)) * 255.0F);
            frame.fill(scaled(CRGB(0, 210, 255), value));
            return;
        }

        case LedEffect::SlowPulse: {
            const float wave = std::sin(t * 1.1F) * 0.5F + 0.5F;
            const uint8_t value = q8((0.05F + wave * wave * wave * 0.95F) * 255.0F);
            frame.fill(scaled(CRGB(120, 45, 255), value));
            return;
        }

        case LedEffect::Rainbow: {
            const uint8_t hue = static_cast<uint8_t>(std::fmod(t * 28.0F, 255.0F));
            frame.fill(hsv(hue));
            return;
        }

        case LedEffect::RainbowCycle: {
            const uint8_t offset = static_cast<uint8_t>(std::fmod(t * 38.0F, 255.0F));
            for (std::size_t i = 0; i < frame.size(); ++i) {
                frame[i] = hsv(static_cast<uint8_t>(offset + i * 255U / frame.size()));
            }
            return;
        }

        case LedEffect::GradientRotate: {
            const float phase = t * 1.5F;
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float wave = std::sin(phase + static_cast<float>(i) * Tau / frame.size()) * 0.5F + 0.5F;
                frame[i] = mix(CRGB(0, 220, 255), CRGB(255, 20, 190), q8(wave * 255.0F));
            }
            return;
        }

        case LedEffect::Aurora: {
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float x = static_cast<float>(i);
                const float a = std::sin(x * 1.15F + t * 1.3F);
                const float b = std::sin(x * 0.43F - t * 0.85F);
                const float wave = (a + b + 2.0F) / 4.0F;
                frame[i] = scaled(mix(CRGB(0, 255, 110), CRGB(75, 35, 255), q8(wave * 255.0F)), q8((0.2F + wave * 0.8F) * 255.0F));
            }
            return;
        }

        case LedEffect::Comet: {
            const float head = t * 2.8F;
            for (uint8_t i = 0; i < 6U; ++i) {
                add(frame, head - static_cast<float>(i) * 0.72F, CRGB(0, 235, 255), q8(std::pow(0.56F, i) * 255.0F));
            }
            return;
        }

        case LedEffect::DualComet: {
            const float head = t * 2.5F;
            for (uint8_t i = 0; i < 5U; ++i) {
                const uint8_t fade = q8(std::pow(0.55F, i) * 255.0F);
                add(frame, head - i * 0.7F, CRGB(0, 230, 255), fade);
                add(frame, head + 4.0F - i * 0.7F, CRGB(255, 20, 180), fade);
            }
            return;
        }

        case LedEffect::Scanner:
        case LedEffect::LarsonScanner: {
            float phase = std::fmod(t * 1.25F, 2.0F);
            if (phase > 1.0F) phase = 2.0F - phase;
            const float head = phase * static_cast<float>(frame.size() - 1U);
            add(frame, head, CRGB(255, 25, 45), 255);
            if (effect_ == LedEffect::LarsonScanner) {
                for (std::size_t i = 0; i < frame.size(); ++i) {
                    const float d = std::fabs(static_cast<float>(i) - head);
                    blendAdd(frame[i], CRGB(255, 0, 0), q8(std::exp(-d * 1.4F) * 130.0F));
                }
            }
            return;
        }

        case LedEffect::Orbit:
            add(frame, t * 2.2F, CRGB(0, 235, 255), 255);
            add(frame, -t * 1.65F + 4.0F, CRGB(255, 30, 190), 210);
            return;

        case LedEffect::Bounce: {
            float phase = std::fmod(t * 1.5F, 2.0F);
            if (phase > 1.0F) phase = 2.0F - phase;
            const float head = phase * static_cast<float>(frame.size() - 1U);
            add(frame, head, CRGB(80, 160, 255), 255);
            add(frame, head - 0.8F, CRGB(40, 60, 180), 90);
            return;
        }

        case LedEffect::Fire: {
            const uint32_t bucket = now / 60U;
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const uint8_t n = static_cast<uint8_t>(hash32(bucket * 41U + i * 733U) >> 24U);
                const uint8_t red = static_cast<uint8_t>(145U + n % 111U);
                const uint8_t green = static_cast<uint8_t>(20U + (static_cast<uint16_t>(red) * (n / 2U)) / 255U);
                frame[i] = CRGB(red, green, n / 22U);
            }
            return;
        }

        case LedEffect::Ice: {
            const uint32_t bucket = now / 95U;
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float wave = std::sin(t * 1.7F + static_cast<float>(i) * 0.8F) * 0.5F + 0.5F;
                frame[i] = mix(CRGB(0, 45, 135), CRGB(185, 255, 255), q8(wave * 255.0F));
                if ((hash32(bucket + i * 67U) >> 24U) > 242U) frame[i] = CRGB::White;
            }
            return;
        }

        case LedEffect::Ocean: {
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float x = static_cast<float>(i);
                const float wave = (std::sin(x * 1.1F - t * 1.5F) + std::sin(x * 0.4F + t * 0.7F) + 2.0F) / 4.0F;
                frame[i] = mix(CRGB(0, 8, 70), CRGB(0, 210, 255), q8(wave * 255.0F));
            }
            return;
        }

        case LedEffect::Matrix: {
            const float head = wrap(t * 1.9F);
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float d = distance(static_cast<float>(i), head);
                const float value = std::exp(-d * 1.35F);
                frame[i] = CRGB(0, q8(value * 255.0F), q8(value * 28.0F));
            }
            return;
        }

        case LedEffect::Cyberpunk: {
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float pulse = 0.3F + 0.7F * (std::sin(t * 2.4F + static_cast<float>(i) * 0.85F) * 0.5F + 0.5F);
                const bool alternate = ((i + static_cast<std::size_t>(t * 2.0F)) & 1U) == 0U;
                frame[i] = scaled(alternate ? CRGB(0, 240, 255) : CRGB(255, 0, 180), q8(pulse * 255.0F));
            }
            return;
        }

        case LedEffect::Vaporwave: {
            const float head = wrap(t * 1.25F);
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const uint8_t amount = static_cast<uint8_t>(i * 255U / (frame.size() - 1U));
                frame[i] = mix(CRGB(255, 55, 205), CRGB(35, 225, 255), amount);
                const float d = distance(static_cast<float>(i), head);
                blendAdd(frame[i], CRGB::White, q8(std::exp(-d * 2.0F) * 100.0F));
            }
            return;
        }

        case LedEffect::Sparkle: {
            frame.fill(scaled(CRGB(25, 40, 80), 35));
            const std::size_t pixel = static_cast<std::size_t>((hash32(now / 55U) >> 24U) % frame.size());
            frame[pixel] = CRGB::White;
            return;
        }

        case LedEffect::Twinkle: {
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const float phase = static_cast<float>(hash32(i * 997U) & 0xFFFFU) / 65535.0F * Tau;
                const float wave = std::pow(std::sin(t * (0.8F + 0.2F * i) + phase) * 0.5F + 0.5F, 4.0F);
                frame[i] = scaled(CRGB(170, 210, 255), q8(wave * 255.0F));
            }
            return;
        }

        case LedEffect::Reactor: {
            const float pulse = std::pow(std::sin(t * 4.0F) * 0.5F + 0.5F, 2.0F);
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const bool core = (i & 1U) == 0U;
                frame[i] = scaled(core ? CRGB(0, 245, 255) : CRGB(130, 20, 255), q8((core ? 0.35F + pulse * 0.65F : 0.2F + (1.0F - pulse) * 0.6F) * 255.0F));
            }
            return;
        }

        case LedEffect::Portal: {
            const float a = t * 3.0F;
            const float b = -t * 2.3F + 4.0F;
            for (uint8_t i = 0; i < 4U; ++i) {
                const uint8_t fade = static_cast<uint8_t>(255U - i * 52U);
                add(frame, a - i * 0.75F, CRGB(0, 240, 255), fade);
                add(frame, b + i * 0.75F, CRGB(255, 20, 190), fade);
            }
            return;
        }

        case LedEffect::Glitch: {
            const uint32_t bucket = now / 50U;
            const int shift = static_cast<int>((hash32(bucket) >> 24U) % frame.size());
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const uint8_t state = static_cast<uint8_t>(hash32(bucket * 37U + i * 71U) >> 24U);
                if (state > 120U) {
                    frame[(i + static_cast<std::size_t>(shift)) % frame.size()] = (state & 1U) ? CRGB(0, 235, 255) : CRGB(255, 0, 175);
                }
            }
            return;
        }

        case LedEffect::Count:
            return;
    }

    if (encoder.pressed) {
        frame.fill(CRGB(30, 30, 30));
    }
}

void LedManager::renderOverlay(uint32_t now, const EncoderState& encoder, Frame& frame) {
    if (overlay_ == LedOverlay::None) return;
    if (static_cast<int32_t>(now - overlayUntil_) >= 0) {
        overlay_ = LedOverlay::None;
        return;
    }

    const float age = static_cast<float>(now - overlayStartedAt_) / 1000.0F;
    const float fade = std::clamp(1.0F - static_cast<float>(now - overlayStartedAt_) / static_cast<float>(std::max<uint32_t>(1U, overlayUntil_ - overlayStartedAt_)), 0.0F, 1.0F);

    switch (overlay_) {
        case LedOverlay::Rotation: {
            const float pos = static_cast<float>(encoder.position);
            const int direction = encoder.direction == 0 ? 1 : encoder.direction;
            const float velocity = std::min(1.0F, std::fabs(encoder.velocity) / 100.0F);
            for (uint8_t i = 0; i < 5U; ++i) {
                add(frame, pos - direction * static_cast<float>(i) * 0.72F, CRGB(255, 255, 255), q8(std::pow(0.58F, i) * (150.0F + velocity * 105.0F) * fade));
            }
            return;
        }
        case LedOverlay::Click:
            for (auto& p : frame) blendAdd(p, CRGB::White, q8(std::exp(-age * 15.0F) * 255.0F));
            return;
        case LedOverlay::Success:
            for (auto& p : frame) blendAdd(p, CRGB(30, 255, 90), q8(fade * 210.0F));
            return;
        case LedOverlay::Warning:
            for (std::size_t i = 0; i < frame.size(); ++i) if (((i + static_cast<std::size_t>(now / 90U)) & 1U) == 0U) blendAdd(frame[i], CRGB(255, 165, 0), q8(fade * 230.0F));
            return;
        case LedOverlay::Error:
            for (auto& p : frame) blendAdd(p, CRGB(255, 15, 25), q8(fade * 255.0F));
            return;
        case LedOverlay::RadioRx:
            add(frame, -age * 9.0F, CRGB(0, 180, 255), q8(fade * 255.0F));
            add(frame, -age * 9.0F + 4.0F, CRGB(0, 80, 255), q8(fade * 180.0F));
            return;
        case LedOverlay::RadioTx:
            add(frame, age * 9.0F, CRGB(255, 65, 10), q8(fade * 255.0F));
            add(frame, age * 9.0F + 4.0F, CRGB(255, 180, 0), q8(fade * 180.0F));
            return;
        case LedOverlay::Notification:
            add(frame, age * 6.0F, CRGB(255, 255, 255), 255);
            return;
        case LedOverlay::None:
            return;
    }
}

void LedManager::clear(Frame& frame) {
    frame.fill(CRGB::Black);
}

void LedManager::add(Frame& frame, float position, const CRGB& color, uint8_t amount) {
    position = wrap(position);
    const float floorValue = std::floor(position);
    const std::size_t lower = static_cast<std::size_t>(floorValue) % frame.size();
    const std::size_t upper = (lower + 1U) % frame.size();
    const float fraction = position - floorValue;
    blendAdd(frame[lower], color, q8((1.0F - fraction) * amount));
    blendAdd(frame[upper], color, q8(fraction * amount));
}

void LedManager::blendAdd(CRGB& destination, const CRGB& source, uint8_t amount) {
    const CRGB value = scaled(source, amount);
    destination.r = static_cast<uint8_t>(std::min<uint16_t>(255U, static_cast<uint16_t>(destination.r) + value.r));
    destination.g = static_cast<uint8_t>(std::min<uint16_t>(255U, static_cast<uint16_t>(destination.g) + value.g));
    destination.b = static_cast<uint8_t>(std::min<uint16_t>(255U, static_cast<uint16_t>(destination.b) + value.b));
}

CRGB LedManager::scaled(CRGB color, uint8_t amount) {
    color.nscale8_video(amount);
    return color;
}

float LedManager::wrap(float position) {
    const float count = static_cast<float>(pins::LedCount);
    position = std::fmod(position, count);
    if (position < 0.0F) position += count;
    return position;
}

float LedManager::distance(float a, float b) {
    const float count = static_cast<float>(pins::LedCount);
    const float d = std::fabs(wrap(a) - wrap(b));
    return std::min(d, count - d);
}

uint32_t LedManager::hash32(uint32_t value) {
    value += 0x9E3779B9U;
    value = (value ^ (value >> 16U)) * 0x85EBCA6BU;
    value = (value ^ (value >> 13U)) * 0xC2B2AE35U;
    return value ^ (value >> 16U);
}

}  // namespace fw

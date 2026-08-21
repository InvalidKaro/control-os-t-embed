#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include <array>
#include <cstdint>

#include "BoardPins.h"
#include "Input.h"

namespace fw {

enum class LedEffect : uint8_t {
    Off = 0,
    Solid,
    Breathing,
    SlowPulse,
    Rainbow,
    RainbowCycle,
    GradientRotate,
    Aurora,
    Comet,
    DualComet,
    Scanner,
    LarsonScanner,
    Orbit,
    Bounce,
    Fire,
    Ice,
    Ocean,
    Matrix,
    Cyberpunk,
    Vaporwave,
    Sparkle,
    Twinkle,
    Reactor,
    Portal,
    Glitch,
    Count
};

enum class LedOverlay : uint8_t {
    None = 0,
    Rotation,
    Click,
    Success,
    Warning,
    Error,
    RadioRx,
    RadioTx,
    Notification
};

class LedManager final {
public:
    bool begin(uint8_t brightness, LedEffect effect);
    void tick(uint32_t now, const EncoderState& encoder);
    void setBrightness(uint8_t brightness);
    uint8_t brightness() const;
    void setEffect(LedEffect effect);
    LedEffect effect() const;
    void nextEffect(int direction);
    void trigger(LedOverlay overlay, uint32_t durationMs = 260);
    static const char* effectName(LedEffect effect);

private:
    using Frame = std::array<CRGB, pins::LedCount>;

    void renderBase(uint32_t now, const EncoderState& encoder, Frame& frame);
    void renderOverlay(uint32_t now, const EncoderState& encoder, Frame& frame);
    static void clear(Frame& frame);
    static void add(Frame& frame, float position, const CRGB& color, uint8_t amount = 255);
    static void blendAdd(CRGB& destination, const CRGB& source, uint8_t amount = 255);
    static CRGB scaled(CRGB color, uint8_t amount);
    static float wrap(float position);
    static float distance(float a, float b);
    static uint32_t hash32(uint32_t value);

    Frame leds_{};
    LedEffect effect_ = LedEffect::Cyberpunk;
    LedOverlay overlay_ = LedOverlay::None;
    uint8_t brightness_ = 96;
    uint32_t overlayStartedAt_ = 0;
    uint32_t overlayUntil_ = 0;
    uint32_t lastFrameAt_ = 0;
};

}  // namespace fw

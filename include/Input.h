#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace fw {

enum class InputEventType : uint8_t {
    None = 0,
    Rotate,
    EncoderPress,
    EncoderRelease,
    EncoderClick,
    EncoderLongPress,
    BackPress,
    BackRelease,
    BackClick
};

struct InputEvent {
    InputEventType type = InputEventType::None;
    int32_t delta = 0;
    uint32_t atMs = 0;
};

struct EncoderState {
    int32_t position = 0;
    int8_t direction = 0;
    float velocity = 0.0F;
    float acceleration = 0.0F;
    bool pressed = false;
};

class InputManager final {
public:
    InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    bool begin();
    void tick(uint32_t now);
    bool pop(InputEvent& event);
    const EncoderState& encoderState() const;

private:
    struct Button {
        int8_t pin = -1;
        bool raw = false;
        bool stable = false;
        bool longSent = false;
        uint32_t changedAt = 0;
        uint32_t pressedAt = 0;
    };

    static void encoderTaskEntry(void* context);
    void encoderTask();
    void sampleEncoder();
    void updateEncoderMotion(uint32_t now);
    void updateButton(Button& button, uint32_t now, bool encoderButton);
    bool push(const InputEvent& event);

    static constexpr std::size_t QueueCapacity = 24;

    volatile int32_t rawQuarterSteps_ = 0;
    volatile int32_t rawPosition_ = 0;
    uint8_t previousAb_ = 0;
    portMUX_TYPE encoderMux_ = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t encoderTaskHandle_ = nullptr;

    int32_t consumedPosition_ = 0;
    uint32_t lastMovementUs_ = 0;
    uint32_t lastDecayMs_ = 0;
    float previousVelocity_ = 0.0F;
    EncoderState encoderState_{};

    Button encoderButton_{};
    Button backButton_{};

    std::array<InputEvent, QueueCapacity> queue_{};
    std::size_t queueHead_ = 0;
    std::size_t queueTail_ = 0;
    std::size_t queueCount_ = 0;
};

}  // namespace fw

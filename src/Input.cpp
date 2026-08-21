#include "Input.h"

#include <cmath>

#include "BoardPins.h"
#include "Config.h"
#include "Log.h"

namespace fw {

namespace {
constexpr const char* Tag = "INPUT";

constexpr int8_t TransitionTable[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

bool pressed(int8_t pin) {
    return digitalRead(pin) == LOW;
}

int32_t floorDiv(int32_t value, int32_t divisor) {
    int32_t quotient = value / divisor;
    const int32_t remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}
}

bool InputManager::begin() {
    pinMode(pins::EncoderA, INPUT_PULLUP);
    pinMode(pins::EncoderB, INPUT_PULLUP);
    pinMode(pins::EncoderKey, INPUT_PULLUP);
    pinMode(pins::BackKey, INPUT_PULLUP);

    previousAb_ = static_cast<uint8_t>(
        (digitalRead(pins::EncoderA) ? 1U : 0U) |
        (digitalRead(pins::EncoderB) ? 2U : 0U)
    );

    encoderButton_.pin = pins::EncoderKey;
    encoderButton_.raw = pressed(encoderButton_.pin);
    encoderButton_.stable = encoderButton_.raw;
    encoderButton_.changedAt = millis();

    backButton_.pin = pins::BackKey;
    backButton_.raw = pressed(backButton_.pin);
    backButton_.stable = backButton_.raw;
    backButton_.changedAt = millis();

    lastMovementUs_ = micros();
    lastDecayMs_ = millis();

    const BaseType_t created = xTaskCreatePinnedToCore(
        &InputManager::encoderTaskEntry,
        "encoder-poll",
        2048,
        this,
        3,
        &encoderTaskHandle_,
        1
    );

    if (created != pdPASS) {
        LOG_ERROR(Tag, "Failed to create encoder poll task");
        return false;
    }

    LOG_INFO(Tag, "Encoder poll task running at %lu ms", static_cast<unsigned long>(config::EncoderPollMs));
    return true;
}

void InputManager::tick(uint32_t now) {
    updateEncoderMotion(now);
    updateButton(encoderButton_, now, true);
    updateButton(backButton_, now, false);
}

bool InputManager::pop(InputEvent& event) {
    if (queueCount_ == 0U) {
        return false;
    }

    event = queue_[queueTail_];
    queueTail_ = (queueTail_ + 1U) % QueueCapacity;
    --queueCount_;
    return true;
}

const EncoderState& InputManager::encoderState() const {
    return encoderState_;
}

void InputManager::encoderTaskEntry(void* context) {
    auto* self = static_cast<InputManager*>(context);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }
    self->encoderTask();
}

void InputManager::encoderTask() {
    TickType_t lastWake = xTaskGetTickCount();
    TickType_t interval = pdMS_TO_TICKS(config::EncoderPollMs);
    if (interval == 0) {
        interval = 1;
    }

    while (true) {
        sampleEncoder();
        vTaskDelayUntil(&lastWake, interval);
    }
}

void InputManager::sampleEncoder() {
    const uint8_t current = static_cast<uint8_t>(
        (digitalRead(pins::EncoderA) ? 1U : 0U) |
        (digitalRead(pins::EncoderB) ? 2U : 0U)
    );

    if (current == previousAb_) {
        return;
    }

    const uint8_t lookup = static_cast<uint8_t>((previousAb_ << 2U) | current);
    previousAb_ = current;
    const int8_t delta = TransitionTable[lookup];
    if (delta == 0) {
        return;
    }

    portENTER_CRITICAL(&encoderMux_);
    rawQuarterSteps_ += delta;
    rawPosition_ = floorDiv(rawQuarterSteps_, 2);
    portEXIT_CRITICAL(&encoderMux_);
}

void InputManager::updateEncoderMotion(uint32_t now) {
    int32_t position = 0;
    portENTER_CRITICAL(&encoderMux_);
    position = rawPosition_;
    portEXIT_CRITICAL(&encoderMux_);

    const int32_t delta = position - consumedPosition_;
    if (delta == 0) {
        if (now - lastDecayMs_ >= 12U) {
            lastDecayMs_ = now;
            encoderState_.velocity *= 0.84F;
            encoderState_.acceleration *= 0.78F;
            if (std::fabs(encoderState_.velocity) < 0.05F) {
                encoderState_.velocity = 0.0F;
                encoderState_.direction = 0;
            }
            if (std::fabs(encoderState_.acceleration) < 0.1F) {
                encoderState_.acceleration = 0.0F;
            }
            previousVelocity_ = encoderState_.velocity;
        }
        return;
    }

    consumedPosition_ = position;
    const uint32_t currentUs = micros();
    uint32_t elapsedUs = currentUs - lastMovementUs_;
    lastMovementUs_ = currentUs;
    if (elapsedUs < 1000U) {
        elapsedUs = 1000U;
    }

    const float seconds = static_cast<float>(elapsedUs) / 1000000.0F;
    const float rawVelocity = static_cast<float>(delta) / seconds;
    const float oldVelocity = encoderState_.velocity;
    encoderState_.velocity = oldVelocity * 0.62F + rawVelocity * 0.38F;
    const float rawAcceleration = (encoderState_.velocity - previousVelocity_) / seconds;
    encoderState_.acceleration = encoderState_.acceleration * 0.68F + rawAcceleration * 0.32F;
    previousVelocity_ = encoderState_.velocity;
    encoderState_.position += delta;
    encoderState_.direction = delta > 0 ? 1 : -1;

    push(InputEvent{InputEventType::Rotate, delta, now});
}

void InputManager::updateButton(Button& button, uint32_t now, bool encoderButton) {
    const bool physical = pressed(button.pin);
    if (physical != button.raw) {
        button.raw = physical;
        button.changedAt = now;
    }

    if (button.raw != button.stable && now - button.changedAt >= config::ButtonDebounceMs) {
        button.stable = button.raw;
        if (button.stable) {
            button.pressedAt = now;
            button.longSent = false;
            if (encoderButton) {
                encoderState_.pressed = true;
            }
            push(InputEvent{
                encoderButton ? InputEventType::EncoderPress : InputEventType::BackPress,
                0,
                now
            });
        } else {
            if (encoderButton) {
                encoderState_.pressed = false;
            }
            push(InputEvent{
                encoderButton ? InputEventType::EncoderRelease : InputEventType::BackRelease,
                0,
                now
            });
            if (!button.longSent) {
                push(InputEvent{
                    encoderButton ? InputEventType::EncoderClick : InputEventType::BackClick,
                    0,
                    now
                });
            }
            button.longSent = false;
        }
    }

    if (
        encoderButton &&
        button.stable &&
        !button.longSent &&
        now - button.pressedAt >= config::ButtonLongPressMs
    ) {
        button.longSent = true;
        push(InputEvent{InputEventType::EncoderLongPress, 0, now});
    }
}

bool InputManager::push(const InputEvent& event) {
    if (queueCount_ >= QueueCapacity) {
        queueTail_ = (queueTail_ + 1U) % QueueCapacity;
        --queueCount_;
        LOG_WARN(Tag, "Input queue overflow; oldest event dropped");
    }

    queue_[queueHead_] = event;
    queueHead_ = (queueHead_ + 1U) % QueueCapacity;
    ++queueCount_;
    return true;
}

}  // namespace fw

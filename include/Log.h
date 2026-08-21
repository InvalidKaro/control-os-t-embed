#pragma once

#include <Arduino.h>

#define LOG_DEBUG(TAG, FMT, ...) Serial.printf("[%10lu] [D] [%s] " FMT "\n", static_cast<unsigned long>(millis()), TAG, ##__VA_ARGS__)
#define LOG_INFO(TAG, FMT, ...)  Serial.printf("[%10lu] [I] [%s] " FMT "\n", static_cast<unsigned long>(millis()), TAG, ##__VA_ARGS__)
#define LOG_WARN(TAG, FMT, ...)  Serial.printf("[%10lu] [W] [%s] " FMT "\n", static_cast<unsigned long>(millis()), TAG, ##__VA_ARGS__)
#define LOG_ERROR(TAG, FMT, ...) Serial.printf("[%10lu] [E] [%s] " FMT "\n", static_cast<unsigned long>(millis()), TAG, ##__VA_ARGS__)

#include <Arduino.h>
#include "Application.h"

namespace {
fw::Application app;
}

void setup() {
    app.begin();
}

void loop() {
    app.tick(millis());
    yield();
}

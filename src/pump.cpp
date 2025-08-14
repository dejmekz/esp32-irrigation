#include "pump.h"
#include "config.h"
#include "Arduino.h"

void pump_init() {
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, HIGH); // Vypne čerpadlo (HIGH znamená vypnuto, LOW znamená zapnuto)
}

void pump_state(bool state) {
    if (state) {
        digitalWrite(PUMP_PIN, LOW); // Zapne čerpadlo
    } else {
        digitalWrite(PUMP_PIN, HIGH); // Vypne čerpadlo
    }
}

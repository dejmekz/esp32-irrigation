#include "pump.h"
#include "config.h"
#include "Arduino.h"

unsigned long last_pump_time_on = 0;

void pump_init() {
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, HIGH); // Vypne čerpadlo (HIGH znamená vypnuto, LOW znamená zapnuto)
}

void pump_on(bool state) {
    if (state) {
        last_pump_time_on = millis();
        digitalWrite(PUMP_PIN, LOW); // Zapne čerpadlo
    } else {
        digitalWrite(PUMP_PIN, HIGH); // Vypne čerpadlo
    }
}

bool pump_is_ready()
{
    // Čerpadlo je připraveno, pokud bylo zapnuto alespoň před PUMP_MIN_RUN_TIME minutami
    if (millis() - last_pump_time_on >= PUMP_MIN_RUN_TIME * 60 * 1000) {
        return true;
    }
    return false;
}
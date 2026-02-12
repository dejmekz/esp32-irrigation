#ifndef PUMP_H
#define PUMP_H

const int PUMP_MIN_RUN_TIME = 5; // minutes

void pump_init();
void pump_on(bool state);
bool pump_is_ready();

#endif
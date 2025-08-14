#ifndef VALVES_H
#define VALVES_H

#include <PCF8574.h>
#include "config.h"

extern PCF8574 pfc_box12; // Ventil krabice 1,2
extern PCF8574 pfc_box34; // Ventil krabice 3,4

void valves_init();
void valves_write(uint16_t value);
void valves_off();

#endif
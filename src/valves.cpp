#include "valves.h"
#include <esp32-hal-log.h>

PCF8574 pfc_box12(PCF_38_ADDRESS); // Ventil krabice 1,2
PCF8574 pfc_box34(PCF_3C_ADDRESS); // Ventil krabice 3,4

void valves_init()
{
    if (!pfc_box12.begin(255))
    {
        log_e("pfc_box12 initialization failed!");
    }

    if (!pfc_box34.begin(255))
    {
        log_e("pfc_box34 initialization failed!");
    }
}


void valves_write(uint16_t value)
{
    uint8_t box12 = static_cast<uint8_t>(value & 0x003F);
    uint8_t box34 = static_cast<uint8_t>((value & 0xFC0) >> 6);

    uint8_t negBox12 = ~box12;
    uint8_t negBox34 = ~box34;

    pfc_box12.write8(negBox12);
    pfc_box34.write8(negBox34);


    log_d("Box 1,2: %u | %u | Box 3,4: %u | %u", box12, negBox12, box34, negBox34);
}

void valves_off()
{
    pfc_box12.write8(0xFF); // Vypne všechny ventily v krabici 1,2
    pfc_box34.write8(0xFF); // Vypne všechny ventily v krabici 3,4
}
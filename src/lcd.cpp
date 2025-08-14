
#include <lcd.h>
#include "utils.h"
#include "config.h"

LCDi2c lcd(0x27, Wire);

void lcd_init() {
    lcd.begin(LCD_ROWS, LCD_COLUMNS);
    lcd.cls();
}

void lcd_print_date_time(int row, int column, DateTime time)
{
    lcd.locate(row, column);

    char buffer[20];
    snprintf(buffer, 20, "%02d.%02d.%04d %02d:%02d:%02d", time.day(), time.month(), time.year(), time.hour(), time.minute(), time.second());

    lcd.print(buffer);
}

void lcd_print_temp(int row, int column, float value)
{
    lcd.locate(row, column);

    char buffer[10];
    dtostrf(value, 5, 2, buffer);

    lcd.printf("Teplota: %s C", buffer);
}


void lcd_print_task(int row, int column, Task *task, uint16_t remains)
{
    lcd.clr(1);
    lcd.clr(2);

    lcd.locate(row, column);
    if (task == nullptr || task->type == TaskType::None)
    {
        lcd.print("Ukol:?? - neznamy");
        return;
    }

    if (task->type == TaskType::Scheduler)
    {
        lcd.printf("Ukol:%02d @ %02d:%02d", task->id, task->hour, task->minute);
    }
    else
    {
        lcd.printf("Ukol:%02d - %02d [%02d]", task->id, remains, task->minute);
        lcd.locate(row + 1, column);
        
        if (task->type == TaskType::Pump)
        {   
            lcd.printf("Cerpadlo: %s", task->pumpOn ? "ZAP" : "VYP");
        }

        if (task->type == TaskType::Valve) 
        {
            String valveStr = toValveString(task->valves);
            lcd.print(valveStr);
        }        
    }
}
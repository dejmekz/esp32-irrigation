
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


void lcd_print_task(TaskManager& taskManager)
{
    lcd.clr(1);
    lcd.clr(2);

    lcd.locate(1, 1);
    lcd.printf(taskManager.statusMessage());

    if (taskManager.isRunning() && taskManager.isPumpOn()) { 
        lcd.locate(2, 1);

        valve_setting_t* tsk = taskManager.actualValveSetting();
        if (tsk == nullptr) {
            lcd.print("No active task");
            return;
        } else {
           lcd.print(toValveString(tsk->valves));
        }
   }
}
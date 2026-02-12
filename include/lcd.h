#ifndef LCD_H
#define LCD_H

#include <LCDi2c.h>
#include <RTClib.h>
#include "TaskManager.h"

extern LCDi2c lcd;

void lcd_init();
void lcd_print_date_time(int row, int column, DateTime time);
void lcd_print_temp(int row, int column, float value);
void lcd_print_task(TaskManager& taskManager);


#endif
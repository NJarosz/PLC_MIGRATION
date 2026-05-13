#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t col, uint8_t row);
void LCD_Print(const char *str);
void LCD_PrintChar(char c);

// State display helpers — call at each state transition
void LCD_ShowIdle(void);
void LCD_ShowArmed(const char *operator_name, uint16_t count, uint16_t goal);
void LCD_ShowRunning(const char *seq_name, uint16_t count, uint16_t goal);
void LCD_ShowGoalReached(uint16_t count, uint16_t goal);
void LCD_ShowCountResetConfirm(uint16_t count);  // "press ACK again to confirm"
void LCD_ShowFault(void);

#endif

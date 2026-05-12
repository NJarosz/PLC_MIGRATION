#include "lcd.h"
#include "i2c.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

// PCF8574 bit mapping (standard wiring on most backpack modules)
// P7=D7 P6=D6 P5=D5 P4=D4 P3=BL P2=EN P1=RW P0=RS
#define LCD_BL   0x08
#define LCD_EN   0x04
#define LCD_RS   0x01

// HD44780 commands
#define CMD_CLEAR        0x01
#define CMD_HOME         0x02
#define CMD_ENTRY        0x06   // left-to-right, no shift
#define CMD_DISPLAY_ON   0x0C   // display on, cursor off, blink off
#define CMD_FUNCTION_SET 0x28   // 4-bit, 2-line, 5x8

#define DDRAM_ROW1  0x00
#define DDRAM_ROW2  0x40

static uint8_t lcd_addr = 0x27;  // updated by scan in LCD_Init

// ── Low-level I2C / PCF8574 ───────────────────────────────────────────────

static void pcf_write(uint8_t data) {
    HAL_I2C_Master_Transmit(&hi2c1, lcd_addr << 1, &data, 1, 10);
}

static void pulse_enable(uint8_t data) {
    pcf_write(data | LCD_EN);
    HAL_Delay(1);
    pcf_write(data & ~LCD_EN);
    HAL_Delay(1);
}

// Send one nibble (high 4 bits of data are the nibble, low bits are control)
static void write_nibble(uint8_t nibble, uint8_t flags) {
    pulse_enable((nibble & 0xF0) | flags | LCD_BL);
}

// Send a full byte as two nibbles
static void lcd_send(uint8_t byte, uint8_t flags) {
    write_nibble(byte,        flags);   // high nibble
    write_nibble(byte << 4,   flags);   // low nibble
}

static void lcd_cmd(uint8_t cmd)      { lcd_send(cmd,  0x00); }
static void lcd_data(uint8_t ch)      { lcd_send(ch,   LCD_RS); }

// ── Address scan ─────────────────────────────────────────────────────────

static uint8_t scan_for_lcd(void) {
    // PCF8574 modules are almost always at 0x27 or 0x3F
    const uint8_t candidates[] = {0x27, 0x3F};
    for (uint8_t i = 0; i < sizeof(candidates); i++) {
        uint8_t dummy = LCD_BL;
        if (HAL_I2C_Master_Transmit(&hi2c1, candidates[i] << 1,
                                    &dummy, 1, 10) == HAL_OK) {
            printf("[LCD] found at 0x%02X\r\n", candidates[i]);
            return candidates[i];
        }
    }
    printf("[LCD] no device found at 0x27 or 0x3F — check wiring\r\n");
    return 0x27;  // fall back so remaining init doesn't hard-fault
}

// ── Public API ────────────────────────────────────────────────────────────

void LCD_Init(void) {
    HAL_Delay(50);  // wait for LCD power-on

    lcd_addr = scan_for_lcd();

    // HD44780 software reset sequence (4-bit mode initialisation)
    write_nibble(0x30, 0x00); HAL_Delay(5);
    write_nibble(0x30, 0x00); HAL_Delay(1);
    write_nibble(0x30, 0x00); HAL_Delay(1);
    write_nibble(0x20, 0x00); HAL_Delay(1);  // switch to 4-bit

    lcd_cmd(CMD_FUNCTION_SET);
    lcd_cmd(CMD_DISPLAY_ON);
    lcd_cmd(CMD_CLEAR);  HAL_Delay(2);
    lcd_cmd(CMD_ENTRY);

    printf("[LCD] init OK\r\n");
}

void LCD_Clear(void) {
    lcd_cmd(CMD_CLEAR);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t col, uint8_t row) {
    uint8_t offsets[] = {DDRAM_ROW1, DDRAM_ROW2};
    lcd_cmd(0x80 | (col + offsets[row & 1]));
}

void LCD_PrintChar(char c) {
    lcd_data((uint8_t)c);
}

void LCD_Print(const char *str) {
    while (*str) lcd_data((uint8_t)*str++);
}

// ── State display helpers ─────────────────────────────────────────────────

static void lcd_print_row(uint8_t row, const char *fmt, ...) {
    char buf[17];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // Pad with spaces to fill the row so previous content is overwritten
    int len = strlen(buf);
    while (len < 16) buf[len++] = ' ';
    buf[16] = '\0';
    LCD_SetCursor(0, row);
    LCD_Print(buf);
}

void LCD_ShowIdle(void) {
    lcd_print_row(0, "  PLC System    ");
    lcd_print_row(1, "  Scan Badge... ");
}

void LCD_ShowArmed(const char *operator_name, uint16_t count, uint16_t goal) {
    if (operator_name && operator_name[0] != '\0') {
        lcd_print_row(0, "Op: %.12s", operator_name);
    } else {
        lcd_print_row(0, "Bypass Mode");
    }
    if (goal > 0) {
        lcd_print_row(1, "Cnt:%u/%u", count, goal);
    } else {
        lcd_print_row(1, "Press RUN btn");
    }
}

void LCD_ShowRunning(const char *seq_name, uint16_t count, uint16_t goal) {
    if (goal > 0) {
        lcd_print_row(0, "Running %u/%u", count, goal);
    } else {
        lcd_print_row(0, "Running...");
    }
    lcd_print_row(1, "%.16s", seq_name ? seq_name : "");
}

void LCD_ShowGoalReached(uint16_t count, uint16_t goal) {
    lcd_print_row(0, "GOAL REACHED!");
    lcd_print_row(1, "Cnt:%u/%u OK", count, goal);
}

void LCD_ShowFault(void) {
    lcd_print_row(0, "!!! FAULT !!!   ");
    lcd_print_row(1, "Reset to clear  ");
}

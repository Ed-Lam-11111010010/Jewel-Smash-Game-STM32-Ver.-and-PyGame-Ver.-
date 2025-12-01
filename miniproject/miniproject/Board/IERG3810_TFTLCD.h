#ifndef __IERG3810_TFTLCD_H
#define __IERG3810_TFTLCD_H
#include "stm32f10x.h"

#define LCD_LIGHT_ON  (GPIOB->BSRR = 1<<0)
#define LCD_LIGHT_OFF (GPIOB->BRR = 1<<0)

// Color definitions
#define c_black     0x0000
#define c_white     0xFFFF
#define c_RED       0xF800
#define c_GREEN     0x07E0
#define c_BLUE      0x001F
#define c_YELLOW    0xFFE0

// LCD structure
typedef struct {
    u16 LCD_REG;
    u16 LCD_RAM;
} LCD_TypeDef;

#define LCD_BASE    ((u32)(0x6C000000 | 0x000007FE))
#define LCD         ((LCD_TypeDef *) LCD_BASE)

// Function declarations
void lcd_init(void);
void lcd_wr_reg(u16 regval);
void lcd_wr_data(u16 data);
void lcd_drawDot(u16 x, u16 y, u16 color);
void lcd_fillRectangle(u16 color, u16 start_x, u16 length_x, u16 start_y, u16 length_y);
void lcd_showChar(u16 x, u16 y, u8 ascii, u16 color, u16 bgcolor);
void lcd_backlight_init(void);
void lcd_sevenSegment(u16 color, u16 start_x, u16 start_y, u8 digit);
void lcd_showChar(u16 x, u16 y, u8 ascii, u16 color, u16 bgcolor);
void lcd_showString(u16 x, u16 y, const char* s, u16 color, u16 bgcolor);
void lcd_showChinChar(u16 x, u16 y, u8 index, u16 color, u16 bgcolor);
void lcd_showChinString(u16 x, u16 y, int index,char* s, u16 color, u16 bgcolor);
void lcd_showChinScaled(u16 x, u16 y, u8 index, u16 color, u16 bgcolor);

#endif

#include "stm32f10x.h"
#include "IERG3810_CLOCK.h"
#include "IERG3810_TFTLCD.h"
#include "FONT.H"
#include "CFONT.H"

#define LCD_LIGHT_ON  (GPIOB->BSRR = 1<<0)
#define LCD_LIGHT_OFF (GPIOB->BRR = 1<<0)

	
void lcd_9341_setParameter(void) {
    lcd_wr_reg(0x01); // Software reset
    lcd_wr_reg(0x11); // Exit sleep
    lcd_wr_reg(0x3A); // Pixel format
    lcd_wr_data(0x55); // 16-bit color
    lcd_wr_reg(0x29); // Display on
    lcd_wr_reg(0x36); // Memory access control
    lcd_wr_data(0xC8); // Display direction
}

void lcd_7789_setParameter(void) {
    lcd_wr_reg(0x01); // Software reset
    lcd_wr_reg(0x11); // Exit sleep
    lcd_wr_reg(0x3A); // Pixel format
    lcd_wr_data(0x05);
    lcd_wr_reg(0x29); // Display on
    lcd_wr_reg(0x36); // Memory access control
    lcd_wr_data(0x80); // Display direction
}



void lcd_backlight_init(void) {
	    // Enable GPIOB clock (should already be enabled in lcd_init, but included for completeness)
    RCC->APB2ENR |= 1<<3;   // Enable PORTB clock
    
    // Configure PB0 as push-pull output (50MHz) for backlight control
    GPIOB->CRL &= 0xFFFFFFF0;  // Clear bits 0-3
    GPIOB->CRL |= 0x00000003;  // Output mode, 50MHz, push-pull
    

}


void lcd_init(void) //set FSMC
{
    RCC->AHBENR |= 1<<8;    //FSMC
    RCC->APB2ENR |= 1<<3;   //PORTB
    RCC->APB2ENR |= 1<<5;   //PORTD  
    RCC->APB2ENR |= 1<<6;   //PORTE
    RCC->APB2ENR |= 1<<8;   //PORTG

	GPIOD->CRL &= 0XFFFFFFF0; //PB0
    GPIOD->CRL |= 0X00000003;
	
    //PORTD
    GPIOD->CRH &= 0X00FFF000;
    GPIOD->CRH |= 0XBB000BBB;
    GPIOD->CRL &= 0XFF00FF00;
    GPIOD->CRL |= 0X00BB00BB;
    
    //PORTE
    GPIOE->CRH &= 0X00000000;
    GPIOE->CRH |= 0XBBBBBBBB;
    GPIOE->CRL &= 0X0FFFFFFF;
    GPIOE->CRL |= 0XB0000000;
    
    //PORTG12
    GPIOG->CRH &= 0XFFF0FFFF;
    GPIOG->CRH |= 0X000B0000;
    GPIOG->CRL &= 0XFFFFFFF0; //PG0 -> RS
    GPIOG->CRL |= 0X0000000B;
    
    //LCD uses FSMC Bank 4 memory bank
		//Use Mode A
    FSMC_Bank1->BTCR[6] = 0x00000000; // Reset BCR4
    FSMC_Bank1->BTCR[7] = 0x00000000; // Reset BTR4
    FSMC_Bank1E->BWTR[6] = 0x00000000; // Reset BWTR4
    
    FSMC_Bank1->BTCR[6] |= 1<<12; // WREN
    FSMC_Bank1->BTCR[6] |= 1<<14; // EXTMOD
    FSMC_Bank1->BTCR[6] |= 1<<4;  // MWID
    
    FSMC_Bank1->BTCR[7] |= 0<<28; // ACCMOD
    FSMC_Bank1->BTCR[7] |= 1<<0;  // ADDSET
    FSMC_Bank1->BTCR[7] |= 0xF<<8; // DATAST
    
    FSMC_Bank1E->BWTR[6] |= 0<<28; // ACCMOD
    FSMC_Bank1E->BWTR[6] |= 0<<0;  // ADDSET  
    FSMC_Bank1E->BWTR[6] |= 3<<8;  // DATAST
    
    FSMC_Bank1->BTCR[6] |= 1<<0; // MBKEN
    
    // LCD initialization
    
    // Use appropriate initialization based on your LCD model
    lcd_9341_setParameter(); //lcd_7789_setParameter()
		
		lcd_backlight_init();
		LCD_LIGHT_ON;
		
}

void lcd_wr_reg(u16 regval) 
{
    LCD->LCD_REG = regval;
}

void lcd_wr_data(u16 data) 
{
    LCD->LCD_RAM = data;
}

void lcd_drawDot(u16 x, u16 y, u16 color) {
    lcd_wr_reg(0x2A); // set x position
    lcd_wr_data(x>>8);
    lcd_wr_data(x & 0xFF);
    lcd_wr_data(0x01);
    lcd_wr_data(0x3F);
    
    lcd_wr_reg(0x2B); // set y position
    lcd_wr_data(y>>8);
    lcd_wr_data(y & 0xFF);
    lcd_wr_data(0x01);
    lcd_wr_data(0xDF);
    
    lcd_wr_reg(0x2C); // write color
    lcd_wr_data(color);
}

void lcd_fillRectangle(u16 color, u16 start_x, u16 length_x, u16 start_y, u16 length_y) {
    u32 index = 0;
    
    // Set column address
    lcd_wr_reg(0x2A);
    lcd_wr_data(start_x >> 8);
    lcd_wr_data(start_x & 0xFF);
    lcd_wr_data((start_x + length_x - 1) >> 8);
    lcd_wr_data((start_x + length_x - 1) & 0xFF);
    
    // Set page address  
    lcd_wr_reg(0x2B);
    lcd_wr_data(start_y >> 8);
    lcd_wr_data(start_y & 0xFF);
    lcd_wr_data((start_y + length_y - 1) >> 8);
    lcd_wr_data((start_y + length_y - 1) & 0xFF);
    
    // Write to memory
    lcd_wr_reg(0x2C);

    
    // Fill the rectangle
    for (index = 0; index < length_x * length_y; index++) {
        lcd_wr_data(color);
    }
}

void lcd_sevenSegment(u16 color, u16 start_x, u16 start_y, u8 digit) {
    // Segment patterns for digits 0-9
    u8 segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    u8 pattern = segments[digit];
    
    // Segment coordinates (adjust based on your dimensions)
    // Segment a (top)
    if (pattern & 0x01) lcd_fillRectangle(color, start_x+10, 60, start_y+140, 10);
    // Segment b (top-right)
    if (pattern & 0x02) lcd_fillRectangle(color, start_x+70, 10, start_y+75, 60);
    // Segment c (bottom-right)
    if (pattern & 0x04) lcd_fillRectangle(color, start_x+70, 10, start_y+10, 60);
    // Segment d (bottom)
    if (pattern & 0x08) lcd_fillRectangle(color, start_x+10, 60, start_y, 10);
    // Segment e (bottom-left)
    if (pattern & 0x10) lcd_fillRectangle(color, start_x, 10, start_y+10, 60);
    // Segment f (top-left)
    if (pattern & 0x20) lcd_fillRectangle(color, start_x, 10, start_y+75, 60);
    // Segment g (middle)
    if (pattern & 0x40) lcd_fillRectangle(color, start_x+10, 60, start_y+70, 10);
}

void lcd_showChar(u16 x, u16 y, u8 ascii, u16 color, u16 bgcolor) {
	u8 i, b, temp1, temp2;
	u16 tempX, tempY;
	if	(ascii<32 || ascii >127) return;
	ascii -= 32;
	tempX = x;
	
	for (i = 0; i < 16; i = i + 2) {
        temp1 = asc2_1608[ascii][i];      // Upper byte of column
        temp2 = asc2_1608[ascii][i + 1];  // Lower byte of column
        tempY = y;
        
        // Process each bit in the bytes
        for (b = 0; b < 8; b++) {
            // Draw upper byte pixel
            if (temp1%2 == 1) 
                lcd_drawDot(tempX, tempY + 8, color);
            
            // Draw lower byte pixel  
            if (temp2%2 == 1)
                lcd_drawDot(tempX, tempY, color);
            
            temp1 = temp1 >> 1;
            temp2 = temp2 >> 1;
            tempY++;
        }
        tempX++;
    }
}

void lcd_showString(u16 x, u16 y, const char* s, u16 color, u16 bgcolor) {
    while (*s) {//while string exist
		char ch = *s++;//read current character then pos +1
		if ((u8)ch < 32 || (u8)ch > 127) ch = ' ';//restrict the passed char to be matching the char in font.h
		lcd_showChar(x, y, (u8)ch, color, bgcolor);//print char
		x += 8; // advance by character width of 8
	}
}

void lcd_showChinChar(u16 x, u16 y, u8 index, u16 color, u16 bgcolor) {
    u8 i, j, b, temp;
    u16 tempX, tempY;
    
    // Check index bounds
    if (index >= sizeof(chi_1616) / 32) return;
    
    tempX = x;
    
    // Process each column (16 pixels wide, 16 pixels high)
    for (i = 0; i < 16; i++) {
        tempY = y;
        
        // Process each row (2 bytes per row)
        for (j = 0; j < 2; j++) {
            temp = chi_1616[index][i * 2 + j];
            
            // Process each bit in the byte
            for (b = 0; b < 8; b++) {
                if (temp & 0x80)  // MSB first
                    lcd_drawDot(tempX + j * 8 + b, tempY, color);
                else if (bgcolor != color)
                    lcd_drawDot(tempX + j * 8 + b, tempY, bgcolor);
                
                temp = temp << 1;
            }
            tempY++;
        }
    }
}

void lcd_showChinScaled(u16 x, u16 y, u8 index, u16 color, u16 bgcolor) {
    int scale=2;//for scaling if want to show larger character
		int col, bit1, bit2, dx1, dy1, dx2, dy2;
	
    if (index >= (sizeof(chi_1616)/sizeof(chi_1616[0]))) return;

    for (col = 0; col < 16; col++) {//16 columns for chin char
        u8 upper = chi_1616[index][col*scale + 0];
        u8 lower = chi_1616[index][col*scale + 1];

        // Upper half rows y+8..y+15
        for (bit1 = 0; bit1 < 8; bit1++) {
            u8 on = (upper >> bit1) & 1;//extract the bit
						//Base x,y position
            u16 px = x + col*scale;
            u16 py = y + (8 + bit1)*scale;
            for (dx1 = 0; dx1 < scale; dx1++) {
                for (dy1 = 0; dy1 < scale; dy1++) {
                    if (on) lcd_drawDot(px + dx1, py + dy1, color);//only draw when the bit ==1
                }
            }
        }

        // Lower half rows y..y+7
        for (bit2 = 0; bit2 < 8; bit2++) {
            u8 on = (lower >> bit2) & 1;
            u16 px = x + col*scale;
            u16 py = y + bit2*scale;
            for (dx2 = 0; dx2 < scale; dx2++) {
                for (dy2 = 0; dy2 < scale; dy2++) {
                    if (on) lcd_drawDot(px + dx2, py + dy2, color);
                }
            }
        }
    }
}


// Function to show Chinese string
void lcd_showChinString(u16 x, u16 y, int index,char* s, u16 color, u16 bgcolor) {
	int i=0;
	for( i=0; i<3; i++) {//chinese characters
		lcd_showChinScaled(x, y, index, color, bgcolor);//print char
		x += 16*2; // advance by character width of 8
		index++;
	}
	while(*s){//sid
		char ch=*s++;
		lcd_showChar(x, y, ch, color, bgcolor);
		x += 8;
	}
}



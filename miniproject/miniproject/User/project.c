#include "stm32f10x.h"
#include "stdlib.h"
#include "stdio.h"
#include "IERG3810_LED.h"
#include "IERG3810_KEY.h"
#include "IERG3810_Buzzer.h"
#include "IERG3810_Clock.h"
#include "IERG3810_TFTLCD.h"
#include "IERG3810_USART.h"

// ==========================================
// COLOR DEFINITIONS
// ==========================================
#define BLACK     0x0000
#define WHITE     0xFFFF
#define BLUE      0x001F
#define GREEN     0x944a
#define RED       0xF800
#define YELLOW    0xFFE0
#define ORANGE    0xFD20
#define CYAN      0x07FF
#define PINK      0xF81F
#define MAGENTA   0xF81F
#define GREY      0x528A 
#define DARK_GREY 0x2124
#define LIGHT_GREY 0xA534 
#define GOLD      0xFEA0

// New Visual Colors
#define GRID_BG_COLOR 0x10A2  // Very Dark Blue/Grey for grid background
#define CELL_BORDER   0x4208  // Color of the line between cells

// ==========================================
// GAME SETTINGS & LAYOUT
// ==========================================
#define GRID_SIZE 9         
#define TILE_SIZE 20

// SCREEN SETTINGS (240x320)
#define FRAME_COLOR         GREY
#define SCREEN_BG_COLOR     BLACK

// Active Screen Area
#define SCREEN_MIN_X    10
#define SCREEN_MAX_X    230
#define SCREEN_MIN_Y    10
#define SCREEN_MAX_Y    310

// COORDINATE SYSTEM: Y=300 is TOP, Y=0 is BOTTOM
#define MARGIN_X        30   

// UI Layout
#define UI_BAR_Y        260  
#define UI_BAR_HEIGHT   25   

// Grid Placement (Y=50 Bottom)
#define GRID_BASE_Y     50   

#define TOTAL_GAME_TIME 180  


// ==========================================
// GLOBAL VARIABLES
// ==========================================
typedef enum {
    STATE_MENU,
    STATE_INSTRUCTIONS,
    STATE_GAME,
    STATE_GAMEOVER
} GameState;

GameState current_state = STATE_MENU;

// Inputs
volatile u32 ps2count = 0;
volatile u32 ps2key = 0;
int key_debounce = 0;

// Game Logic
int score = 0;
int cursor_x = 4;           
int cursor_y = 4;           
int is_selected = 0;        
int game_timer_seconds = TOTAL_GAME_TIME;
volatile int systick_counter = 0; 
// Random Seed Counter
volatile int seed_counter = 0;

// Tile colors
const u16 GEM_COLORS[] = {RED, GREEN, BLUE, YELLOW, ORANGE, MAGENTA};
#define NUM_COLORS 6

// Tile types
#define NORMAL_TILE 0
#define HORIZONTAL_CLEARER 1
#define VERTICAL_CLEARER 2
#define BOMB 3

int grid[GRID_SIZE][GRID_SIZE][2];

// PS2 Codes
#define PS2_NUM2    0x72  
#define PS2_NUM4    0x6B    
#define PS2_NUM5    0x73  
#define PS2_NUM6    0x74  
#define PS2_NUM8    0x75  
#define PS2_NUM_MINUS 0x4A // PS/2 code for '-'. Update if your hardware is different.

// Buzzer
#define BUZZER_ON  (GPIOB->BSRR = 1 << 8)
#define BUZZER_OFF (GPIOB->BRR = 1 << 8)

// ==========================================
// HARDWARE INIT & INTERRUPTS
// ==========================================
void Delay(u32 count) {
    u32 i;
    for (i = 0; i < count; i++);
}

void IERG3810_NVIC_SetPriorityGroup(u8 prigroup) {
    u32 temp, temp1;
    temp1 = prigroup & 0x00000007;
    temp1 <<= 8;
    temp = SCB->AIRCR;
    temp &= 0x0000F8FF;
    temp |= 0x05FA0000;
    temp |= temp1;
    SCB->AIRCR = temp;
}

void IERG3810_PS2key_ExtiInit(void) {
    RCC->APB2ENR |= 1 << 4;
    GPIOC->CRH &= 0xFFFF0FFF;
    GPIOC->CRH |= 0x00008000;
    GPIOC->ODR |= 1 << 11;
    GPIOC->CRH &= 0xFFFFF0FF;
    GPIOC->CRH |= 0x00000800;
    RCC->APB2ENR |= 0x01;
    AFIO->EXTICR[2] &= 0xFFFF0FFF;
    AFIO->EXTICR[2] |= 0x00002000;
    EXTI->IMR |= 1 << 11;
    EXTI->FTSR |= 1 << 11; 
    NVIC->IP[40] = 0x65;
    NVIC->ISER[1] |= (1 << 8);
}

void EXTI15_10_IRQHandler(void) {
    static u32 shift_reg = 0;
    if (EXTI->PR & (1 << 11)) {
        u32 data_bit = (GPIOC->IDR & (1 << 10)) ? 1 : 0;
        if (ps2count == 0) {
            if (data_bit == 0) {
                ps2count = 1;
                shift_reg = 0;
            }
        } else if (ps2count >= 1 && ps2count <= 8) {
            shift_reg |= (data_bit << (ps2count - 1));
            ps2count++;
        } else if (ps2count == 9) {
            ps2count++;
        } else if (ps2count == 10) {
            if (data_bit == 1) {
                ps2key = shift_reg;
            }
            ps2count = 0;
        }
        EXTI->PR = 1 << 11; 
    }
}

void IERG3810_SYSTICK_Init10ms(void) {
    SysTick->CTRL = 0;
    SysTick->LOAD = 90000;
    SysTick->CTRL |= (1 << 16 | 0x03);
}

void SysTick_Handler(void) {
    if (key_debounce > 0) key_debounce--;
    
    // Countdown logic
    if (current_state == STATE_GAME) {
        systick_counter++;
        if (systick_counter >= 100) { // 1 second
            systick_counter = 0;
            if (game_timer_seconds > 0) {
                game_timer_seconds--;
            } else {
                current_state = STATE_GAMEOVER;
            }
        }
    }
}

// ==========================================
// GRAPHICS & RENDERER
// ==========================================

void draw_frame(void) {
    // 1. Draw the Bezel
    lcd_fillRectangle(FRAME_COLOR, 0, 240, 0, 320);
    // 2. Draw the Active Screen
    lcd_fillRectangle(SCREEN_BG_COLOR, SCREEN_MIN_X, SCREEN_MAX_X - SCREEN_MIN_X, SCREEN_MIN_Y, SCREEN_MAX_Y - SCREEN_MIN_Y);
    // 3. Optional Text
    lcd_showString(80, 290, "IERG3810", WHITE, FRAME_COLOR); 
}

// *** VISUAL UPDATE: Draw Tile Background + Jewel ***
void draw_jewel_tile(int x_pos, int y_pos, int color_index, int type) {
    // 1. Draw Cell Background (The Frame)
    lcd_fillRectangle(GRID_BG_COLOR, x_pos, TILE_SIZE, y_pos, TILE_SIZE);
    
    // 2. Draw Cell Border
    lcd_fillRectangle(CELL_BORDER, x_pos, TILE_SIZE, y_pos, 1); // Bottom
    lcd_fillRectangle(CELL_BORDER, x_pos, TILE_SIZE, y_pos + TILE_SIZE - 1, 1); // Top
    lcd_fillRectangle(CELL_BORDER, x_pos, 1, y_pos, TILE_SIZE); // Left
    lcd_fillRectangle(CELL_BORDER, x_pos + TILE_SIZE - 1, 1, y_pos, TILE_SIZE); // Right
    
    // If empty, just return (we drew the empty cell bg)
    if (color_index == -1) return;

    // Center coordinates for shapes
    int cx = x_pos + 10;
    int cy = y_pos + 10;
    u16 color = GEM_COLORS[color_index];
    int i, width, start_x;
    
    // 3. Draw Geometric Shape (Slightly smaller to fit in frame)
    switch(color_index) {
        case 0: // RED -> SQUARE
            lcd_fillRectangle(color, x_pos + 5, 10, y_pos + 5, 10);
            break;
            
        case 1: // GREEN -> CIRCLE
            lcd_fillRectangle(color, x_pos + 7, 6, y_pos + 4, 1);
            lcd_fillRectangle(color, x_pos + 5, 10, y_pos + 5, 1);
            lcd_fillRectangle(color, x_pos + 4, 12, y_pos + 6, 8); 
            lcd_fillRectangle(color, x_pos + 5, 10, y_pos + 14, 1);
            lcd_fillRectangle(color, x_pos + 7, 6, y_pos + 15, 1);
            break;
            
        case 2: // BLUE -> DIAMOND
            for (i = 0; i < 6; i++) {
                width = 2 * i + 1; 
                start_x = cx - i;
                lcd_fillRectangle(color, start_x, width, cy + (5 - i), 1);
                lcd_fillRectangle(color, start_x, width, cy - (5 - i), 1);
            }
            lcd_fillRectangle(color, x_pos + 4, 13, cy, 1);
            break;
            
        case 3: // YELLOW -> PENTAGON
             // Top Triangle
            for (i = 0; i < 5; i++) {
                width = 2 * i + 2; 
                start_x = cx - (width / 2);
                lcd_fillRectangle(color, start_x, width, cy - 5 + i, 1);
            }
            // Base
            for (i = 0; i < 6; i++) {
                width = 10 - i; 
                start_x = cx - (width / 2);
                lcd_fillRectangle(color, start_x, width, cy + i, 1);
            }
            break;
            
        case 4: // ORANGE -> TRIANGLE (Down)
            for (i = 0; i < 11; i++) {
                width = 11 - i; 
                lcd_fillRectangle(color, cx - (width/2), width, y_pos + 5 + i, 1);
            }
            break;
            
        case 5: // MAGENTA -> PLUS
            lcd_fillRectangle(color, x_pos + 8, 4, y_pos + 3, 14); // Vertical
            lcd_fillRectangle(color, x_pos + 3, 14, y_pos + 8, 4); // Horizontal
            break;
    }
    
    // 4. Special Markers
    if (type == HORIZONTAL_CLEARER) {
        lcd_fillRectangle(WHITE, x_pos + 3, 14, cy - 1, 2);
    } else if (type == VERTICAL_CLEARER) {
        lcd_fillRectangle(WHITE, cx - 1, 2, y_pos + 3, 14);
    } else if (type == BOMB) {
        lcd_fillRectangle(WHITE, cx - 3, 6, cy - 3, 6);
        lcd_drawDot(cx, cy, RED);
    }
}

// *** OPTIMIZATION: Helper to draw a single tile at grid coords ***
void draw_single_tile(int x, int y) {
    int tile_x = MARGIN_X + x * TILE_SIZE;
    int tile_y = GRID_BASE_Y + (y * TILE_SIZE);
    
    int color_idx = grid[y][x][1];
    int type = grid[y][x][0];
    
    // Draw the tile content (Background + Jewel)
    draw_jewel_tile(tile_x, tile_y, color_idx, type);
    
    // Check if Cursor is here -> Draw Selection Border over it
    if (x == cursor_x && y == cursor_y) {
        u16 border_color = is_selected ? RED : WHITE;
        // Draw 2px thick border
        for (int i = 0; i < 2; i++) {
            lcd_fillRectangle(border_color, tile_x + i, TILE_SIZE - 2*i, tile_y + i, 1);
            lcd_fillRectangle(border_color, tile_x + i, TILE_SIZE - 2*i, tile_y + TILE_SIZE - 1 - i, 1);
            lcd_fillRectangle(border_color, tile_x + i, 1, tile_y + i, TILE_SIZE - 2*i);
            lcd_fillRectangle(border_color, tile_x + TILE_SIZE - 1 - i, 1, tile_y + i, TILE_SIZE - 2*i);
        }
    }
}

void init_grid_no_matches(void) {
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            grid[y][x][0] = NORMAL_TILE;
            grid[y][x][1] = rand() % NUM_COLORS;
        }
    }
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE - 2; x++) {
            if (grid[y][x][1] == grid[y][x+1][1] && grid[y][x][1] == grid[y][x+2][1]) {
                grid[y][x+1][1] = (grid[y][x+1][1] + 1) % NUM_COLORS;
            }
        }
    }
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE - 2; y++) {
            if (grid[y][x][1] == grid[y+1][x][1] && grid[y][x][1] == grid[y+2][x][1]) {
                grid[y+1][x][1] = (grid[y+1][x][1] + 1) % NUM_COLORS;
            }
        }
    }
    score = 0;
    cursor_x = GRID_SIZE / 2; 
    cursor_y = GRID_SIZE / 2; 
    is_selected = 0;
    game_timer_seconds = TOTAL_GAME_TIME;
}

void draw_ui_bar(void) {
    lcd_fillRectangle(LIGHT_GREY, SCREEN_MIN_X, SCREEN_MAX_X - SCREEN_MIN_X, UI_BAR_Y, UI_BAR_HEIGHT);
    char str[25];
    sprintf(str, "PTS: %d", score);
    lcd_showString(SCREEN_MIN_X + 5, UI_BAR_Y + 5, str, BLACK, LIGHT_GREY);
    int min = game_timer_seconds / 60;
    int sec = game_timer_seconds % 60;
    sprintf(str, "TIME %02d:%02d", min, sec);
    lcd_showString(SCREEN_MIN_X + 110, UI_BAR_Y + 5, str, RED, LIGHT_GREY);
}

// Full Grid Redraw (Used for Gravity/Matches/Start)
void draw_grid_stable(void) {
    // Clear the Grid Background Area
    lcd_fillRectangle(GRID_BG_COLOR, MARGIN_X - 5, (GRID_SIZE * TILE_SIZE) + 10, GRID_BASE_Y - 5, (GRID_SIZE * TILE_SIZE) + 10);
    
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            draw_single_tile(x, y);
        }
    }
    draw_ui_bar();
}

void draw_start_screen(void) {
    draw_frame();
    lcd_showString(SCREEN_MIN_X + 60, 260, "JEWEL LEGEND", CYAN, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 60, 200, "Designed by", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 35, 180, "1155184266 Lam Chi", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 100, 160, "&", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 20, 140, "1155214311 Yu Ho Ming", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 65, 60, "PRESS KEY 1", YELLOW, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 70, 40, "TO START", YELLOW, SCREEN_BG_COLOR);
}

void draw_instructions_screen(void) {
    draw_frame();
    lcd_showString(SCREEN_MIN_X + 40, 270,  "-- HOW TO PLAY --", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 30, 230,  "Key 8: UP", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 30, 210, "Key 2: DOWN", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 30, 190, "Key 4: LEFT", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 30, 170, "Key 6: RIGHT", WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 30, 130, "Key 5: SELECT/SWAP", RED, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 60, 60, "PRESS KEY 1", YELLOW, SCREEN_BG_COLOR);
}

void draw_gameover_screen(void) {
    draw_frame();
    lcd_showString(SCREEN_MIN_X + 70, 230, "GAME OVER", RED, SCREEN_BG_COLOR);
    char score_str[20];
    sprintf(score_str, "FINAL: %d", score);
    lcd_showString(SCREEN_MIN_X + 75, 200, score_str, WHITE, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 60, 60, "PRESS KEY UP", YELLOW, SCREEN_BG_COLOR);
    lcd_showString(SCREEN_MIN_X + 70, 40, "TO RESET", YELLOW, SCREEN_BG_COLOR);
}

// ==========================================
// LOGIC FUNCTIONS
// ==========================================

void swap_tiles(int x1, int y1, int x2, int y2) {
    int temp_type = grid[y1][x1][0];
    int temp_color = grid[y1][x1][1];
    grid[y1][x1][0] = grid[y2][x2][0];
    grid[y1][x1][1] = grid[y2][x2][1];
    grid[y2][x2][0] = temp_type;
    grid[y2][x2][1] = temp_color;
}

int find_and_clear_matches(void) {
    int matches_found = 0;
    int to_clear[GRID_SIZE][GRID_SIZE] = {0};
    // --- Extended for special markers ---
    // Horizontal
    for (int y = 0; y < GRID_SIZE; y++) {
        int run_len = 1;
        for (int x = 1; x < GRID_SIZE; x++) {
            if (grid[y][x][1] == grid[y][x-1][1] && grid[y][x][1] != -1 && 
                    grid[y][x][0] == NORMAL_TILE && grid[y][x-1][0] == NORMAL_TILE) {
                run_len++;
            } else {
                if (run_len == 4) {
                    grid[y][x-1][0] = HORIZONTAL_CLEARER;
                }
                if (run_len == 5) {
                    grid[y][x-3][0] = BOMB;
                }
                run_len = 1;
            }
        }
        // Handle runs ending at edge
        if (run_len == 4) grid[y][GRID_SIZE-1][0] = HORIZONTAL_CLEARER;
        if (run_len == 5) grid[y][GRID_SIZE-3][0] = BOMB;
        // Proceed with normal matching (matches of 3+)
        for (int x = 0; x < GRID_SIZE - 2; x++) {
            if (grid[y][x][0] == NORMAL_TILE && grid[y][x+1][0] == NORMAL_TILE && grid[y][x+2][0] == NORMAL_TILE &&
                    grid[y][x][1] != -1 &&
                    grid[y][x][1] == grid[y][x+1][1] &&
                    grid[y][x][1] == grid[y][x+2][1]) {
                to_clear[y][x] = 1; to_clear[y][x+1] = 1; to_clear[y][x+2] = 1; matches_found = 1;
            }
        }
    }
    // Vertical
    for (int x = 0; x < GRID_SIZE; x++) {
        int run_len = 1;
        for (int y = 1; y < GRID_SIZE; y++) {
            if (grid[y][x][1] == grid[y-1][x][1] && grid[y][x][1] != -1 &&
                    grid[y][x][0] == NORMAL_TILE && grid[y-1][x][0] == NORMAL_TILE) {
                run_len++;
            } else {
                if (run_len == 4) {
                    grid[y-1][x][0] = VERTICAL_CLEARER;
                }
                if (run_len == 5) {
                    grid[y-3][x][0] = BOMB;
                }
                run_len = 1;
            }
        }
        if (run_len == 4) grid[GRID_SIZE-1][x][0] = VERTICAL_CLEARER;
        if (run_len == 5) grid[GRID_SIZE-3][x][0] = BOMB;
        for (int y = 0; y < GRID_SIZE - 2; y++) {
            if (grid[y][x][0] == NORMAL_TILE && grid[y+1][x][0] == NORMAL_TILE && grid[y+2][x][0] == NORMAL_TILE &&
                    grid[y][x][1] != -1 &&
                    grid[y][x][1] == grid[y+1][x][1] &&
                    grid[y][x][1] == grid[y+2][x][1]) {
                to_clear[y][x] = 1; to_clear[y+1][x] = 1; to_clear[y+2][x] = 1; matches_found = 1;
            }
        }
    }
    int tiles_cleared = 0;
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (to_clear[y][x]) {
                grid[y][x][1] = -1;
                grid[y][x][0] = NORMAL_TILE;
                tiles_cleared++;
            }
        }
    }
    if (tiles_cleared > 0) score += tiles_cleared * 10;
    return matches_found;
}

void apply_gravity(void) {
    int moved;
    do {
        moved = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE - 1; y++) {
                if (grid[y][x][1] == -1 && grid[y+1][x][1] != -1) {
                    grid[y][x][0] = grid[y+1][x][0];
                    grid[y][x][1] = grid[y+1][x][1];
                    grid[y+1][x][0] = NORMAL_TILE;
                    grid[y+1][x][1] = -1;
                    moved = 1;
                }
            }
            if (grid[GRID_SIZE - 1][x][1] == -1) {
                grid[GRID_SIZE - 1][x][0] = NORMAL_TILE;
                grid[GRID_SIZE - 1][x][1] = rand() % NUM_COLORS;
                moved = 1;
            }
        }
        if (moved) {
            draw_grid_stable();
            Delay(60000); 
        }
    } while (moved);
}

// Clear row/column/3x3 area - helpers for clearers and bomb
void clear_row(int row) {
    for (int x = 0; x < GRID_SIZE; x++) {
        grid[row][x][1] = -1; // clear each cell
        grid[row][x][0] = NORMAL_TILE;
    }
    draw_grid_stable();
}

void clear_column(int col) {
    for (int y = 0; y < GRID_SIZE; y++) {
        grid[y][col][1] = -1;
        grid[y][col][0] = NORMAL_TILE;
    }
    draw_grid_stable();
}

void clear_3x3_area(int cx, int cy) {
    for (int y = cy - 1; y <= cy + 1; y++) {
        for (int x = cx - 1; x <= cx + 1; x++) {
            if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                grid[y][x][1] = -1;
                grid[y][x][0] = NORMAL_TILE;
            }
        }
    }
    draw_grid_stable();
}

void handle_keyboard_input(void) {
    static u32 last_key = 0;
    static u32 key_repeat = 0;
    
    if (ps2key != 0 && key_debounce == 0) {
        if (ps2key != last_key || key_repeat == 0) {
            int key_handled = 0;
            int dx = 0, dy = 0;
            int action_taken = 0; 
            
            switch (ps2key) {
                case PS2_NUM8: dy = 1;  key_handled = 1; break; 
                case PS2_NUM2: dy = -1; key_handled = 1; break; 
                case PS2_NUM4: dx = -1; key_handled = 1; break;
                case PS2_NUM6: dx = 1;  key_handled = 1; break;
                case PS2_NUM5: {
									int tile_type = grid[cursor_y][cursor_x][0];
									// If special tile, activate immediately, then refill and clear combo chains
									if (tile_type == HORIZONTAL_CLEARER) {
											BUZZER_ON; Delay(10000); BUZZER_OFF;
											clear_row(cursor_y);
											apply_gravity();
											while (find_and_clear_matches()) apply_gravity();
											draw_single_tile(cursor_x, cursor_y);
											is_selected = 0;
											key_handled = 1;
											break;
									}
									if (tile_type == VERTICAL_CLEARER) {
											BUZZER_ON; Delay(10000); BUZZER_OFF;
											clear_column(cursor_x);
											apply_gravity();
											while (find_and_clear_matches()) apply_gravity();
											draw_single_tile(cursor_x, cursor_y);
											is_selected = 0;
											key_handled = 1;
											break;
									}
									if (tile_type == BOMB) {
											BUZZER_ON; Delay(10000); BUZZER_OFF;
											clear_3x3_area(cursor_x, cursor_y);
											apply_gravity();
											while (find_and_clear_matches()) apply_gravity();
											draw_single_tile(cursor_x, cursor_y);
											is_selected = 0;
											key_handled = 1;
											break;
									}
									// Normal selection logic for regular gems
									is_selected = !is_selected;
									key_handled = 1;
									draw_single_tile(cursor_x, cursor_y);
									if (is_selected) { BUZZER_ON; Delay(10000); BUZZER_OFF; }
									break;
							}

            }
            
            if (key_handled) {
                if (dx != 0 || dy != 0) {
                    if (!is_selected) {
                        // *** OPTIMIZATION: Partial Redraw for Cursor Movement ***
                        int old_x = cursor_x;
                        int old_y = cursor_y;
                        int new_x = cursor_x + dx;
                        int new_y = cursor_y + dy;
                        
                        if (new_x >= 0 && new_x < GRID_SIZE && new_y >= 0 && new_y < GRID_SIZE) {
                            cursor_x = new_x; 
                            cursor_y = new_y; 
                            
                            // Only redraw the two affected tiles
                            draw_single_tile(old_x, old_y); // Remove cursor from old
                            draw_single_tile(cursor_x, cursor_y); // Add cursor to new
                        }
                    } else {
                        // Swapping requires updating the logic and drawing
                        int target_x = cursor_x + dx;
                        int target_y = cursor_y + dy;
                        if (target_x >= 0 && target_x < GRID_SIZE && target_y >= 0 && target_y < GRID_SIZE) {
                            swap_tiles(cursor_x, cursor_y, target_x, target_y);
                            // Draw swap immediately (partial redraw of 2 tiles)
                            draw_single_tile(cursor_x, cursor_y);
                            draw_single_tile(target_x, target_y);
                            Delay(50000);
                            
                            if (find_and_clear_matches()) {
                                BUZZER_ON; Delay(30000); BUZZER_OFF;
                                apply_gravity(); // Full redraws happen here
                                while (find_and_clear_matches()) apply_gravity();
                            } else {
                                // Swap back
                                swap_tiles(cursor_x, cursor_y, target_x, target_y);
                                draw_single_tile(cursor_x, cursor_y);
                                draw_single_tile(target_x, target_y);
                            }
                            is_selected = 0; 
                            draw_single_tile(cursor_x, cursor_y); // Update cursor color back to white
                            action_taken = 1;
                        }
                    }
                }
                
                // If a full action (like swap) happened, UI updates inside apply_gravity
                // But if purely cursor move, we don't need to redraw everything.
                if (action_taken) {
                    draw_ui_bar(); // Just update score/time
                }
                
                key_debounce = 10; key_repeat = 5;
            }
            last_key = ps2key;
        } else {
            key_repeat--;
        }
        ps2key = 0;
    }
}

// Shuffle function - add after other helper functions
void shuffle_game_grid() {
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int tx = rand() % GRID_SIZE;
            int ty = rand() % GRID_SIZE;
            // Swap type and color
            int tmp_type = grid[y][x][0];
            int tmp_color = grid[y][x][1];
            grid[y][x][0] = grid[ty][tx][0];
            grid[y][x][1] = grid[ty][tx][1];
            grid[ty][tx][0] = tmp_type;
            grid[ty][tx][1] = tmp_color;
        }
    }
    draw_grid_stable();
}




// ==========================================
// MAIN LOOP
// ==========================================
int main(void) {
    IERG3810_clocktree_init();
    IERG3810_KEY_Init();    
    IERG3810_LED_Init();
    IERG3810_Buzzer_Init();
    IERG3810_NVIC_SetPriorityGroup(5);
    IERG3810_PS2key_ExtiInit();
    lcd_init();
    IERG3810_SYSTICK_Init10ms();
    IERG3810_usart2_init(36, 9600);
    
    DS0_off; DS1_off; BUZZER_OFF;
    
    int btn1_prev_state = 1; 
    int btnUp_prev_state = 1;

    current_state = STATE_MENU;
    draw_start_screen();
    
    while(1) {
				// *** RANDOM SEEDING LOGIC ***
        seed_counter++; // Always increments waiting for user
			
        int btn1_curr = (GPIOE->IDR & GPIO_Pin_3) ? 1 : 0; 
        int btnUp_curr = (GPIOA->IDR & GPIO_Pin_0) ? 1 : 0;
        
        // Key 1 Logic
        if (btn1_curr == 0 && btn1_prev_state == 1) { 
            if (current_state == STATE_MENU) {
                current_state = STATE_INSTRUCTIONS;
                draw_instructions_screen();
            } else if (current_state == STATE_INSTRUCTIONS) {
								// Seed the RNG here!
                srand(seed_counter);
							
                current_state = STATE_GAME;
                init_grid_no_matches();
                draw_frame(); 
                draw_grid_stable();
            }
            Delay(50000);
        }
        btn1_prev_state = btn1_curr;
        
        // Key UP Logic
        if (btnUp_curr == 0 && btnUp_prev_state == 1) {
            if (current_state == STATE_GAMEOVER || current_state == STATE_GAME) {
                current_state = STATE_MENU;
                draw_start_screen();
            }
             Delay(50000);
        }
        btnUp_prev_state = btnUp_curr;

        if (current_state == STATE_GAME) {
            if (game_timer_seconds <= 0) {
                current_state = STATE_GAMEOVER;
                draw_gameover_screen();
            } else {
                handle_keyboard_input();
                static int last_timer = 0;
                if (game_timer_seconds != last_timer) {
                    draw_ui_bar();
                    last_timer = game_timer_seconds;
                }
            }
        }
        Delay(10000);
    }
}

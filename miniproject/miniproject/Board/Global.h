#ifndef __GLOBAL_H
#define __GLOBAL_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// PS2 keyboard scan codes for number keys
#define PS2_KEY_2 0x72
#define PS2_KEY_4 0x6B
#define PS2_KEY_5 0x73
#define PS2_KEY_6 0x74
#define PS2_KEY_8 0x75

extern u32 ps2count;
extern u32 ps2key;
extern u32 sheep;
extern u32 timeout;
extern u8 task1HeartBeat;
extern u8 task2HeartBeat;
extern u8 task3HeartBeat;
extern u8 task4HeartBeat;

#endif
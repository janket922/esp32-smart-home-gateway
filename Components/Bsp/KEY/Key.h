#ifndef __KEY_H
#define __KEY_H

#include "XL9555.h"

typedef enum
{
	KEY_1 = 0,
	KEY_2,
} key_num;

typedef enum
{
	KEY_RELEASED = 0,
	KEY1_PRESSED,
	KEY2_PRESSED,
} key_state;
void Key_Init(void);
key_state Key_Get_State(key_num key);
#endif
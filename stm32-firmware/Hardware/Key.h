#ifndef __KEY_H
#define __KEY_H
#include "Delay.h"

void Key_Init(void);
uint8_t Key_GetNum(void);
uint8_t Read_Keys_Toggle(void);
uint8_t Key_ReadBits(void);
uint8_t read_key(void);

#endif

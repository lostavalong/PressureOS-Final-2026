#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"
#include <stdio.h>

void Serial_Init(void);
void Serial_SendByte(uint8_t byte);
void Serial_SendArray(const uint8_t *array, uint16_t length);
void Serial_SendString(const char *string);
void Serial_Printf(const char *format, ...);
uint8_t Serial_ReceiveByte_NonBlocking(uint8_t *data);
uint8_t Serial_TakeRxOverflow(void);
uint8_t Serial_TakeRxError(void);

#endif

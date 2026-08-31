#ifndef __AD7124_H
#define __AD7124_H

#include "stm32f10x_conf.h"

#define AD7124_CHANNEL_PRESSURE      0u
#define AD7124_CHANNEL_TEMPERATURE   1u

#define AD7124_STATUS_RDY            0x80u
#define AD7124_STATUS_ERROR_FLAG     0x40u
#define AD7124_STATUS_POR_FLAG       0x10u
#define AD7124_STATUS_CHANNEL_MASK   0x0Fu

typedef struct
{
    uint32_t raw;
    uint8_t status;
    uint8_t channel;
} AD7124_Sample;

typedef enum
{
    AD7124_RESULT_OK = 0,
    AD7124_RESULT_DRDY_TIMEOUT,
    AD7124_RESULT_SPI_ERROR,
    AD7124_RESULT_ADC_ERROR
} AD7124_Result;

void AD7124_SPI_Config(void);
uint8_t AD7124_8_SoftReset(void);
uint8_t AD7124_8_Init(void);
uint8_t AD7124_8_ReadId(void);
uint8_t AD7124_8_ConfigValid(void);
uint32_t AD7124_8_ReadErrorRegister(void);
AD7124_Result AD7124_8_ReadSample(AD7124_Sample *sample, uint16_t timeout_ms);

#endif

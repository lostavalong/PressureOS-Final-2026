#include "AD7124.h"
#include "Delay.h"

#define AD7124_REG_STATUS       0x00u
#define AD7124_REG_ADC_CONTROL  0x01u
#define AD7124_REG_DATA         0x02u
#define AD7124_REG_IO_CONTROL1  0x03u
#define AD7124_REG_ID           0x05u
#define AD7124_REG_ERROR        0x06u
#define AD7124_REG_CHANNEL0     0x09u
#define AD7124_REG_CHANNEL1     0x0Au
#define AD7124_REG_CONFIG0      0x19u
#define AD7124_REG_CONFIG1      0x1Au
#define AD7124_REG_FILTER0      0x21u
#define AD7124_REG_FILTER1      0x22u

#define AD7124_ADC_CONTROL_VALUE  0x0480u
#define AD7124_IO_CONTROL1_VALUE  0x001276u
#define AD7124_CHANNEL0_VALUE     0x81ACu
#define AD7124_CHANNEL1_VALUE     0x90A4u
#define AD7124_CONFIG0_VALUE      0x0184u
#define AD7124_CONFIG1_VALUE      0x0982u
#define AD7124_FILTER0_VALUE      0x0600A0u
#define AD7124_FILTER1_VALUE      0x060080u

#define AD7124_CS_LOW()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define AD7124_CS_HIGH()  GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define SPI_WAIT_LIMIT    100000u

static uint8_t s_config_valid = 0u;

static uint8_t SPI1_TransferByte(uint8_t tx, uint8_t *rx)
{
    uint32_t timeout = SPI_WAIT_LIMIT;

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET)
    {
        if (timeout == 0u)
            return 0u;
        --timeout;
    }

    SPI_I2S_SendData(SPI1, tx);
    timeout = SPI_WAIT_LIMIT;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET)
    {
        if (timeout == 0u)
            return 0u;
        --timeout;
    }

    *rx = (uint8_t)SPI_I2S_ReceiveData(SPI1);
    return 1u;
}

static uint8_t SPI1_WaitNotBusy(void)
{
    uint32_t timeout = SPI_WAIT_LIMIT;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET)
    {
        if (timeout == 0u)
            return 0u;
        --timeout;
    }
    return 1u;
}

static uint8_t AD7124_WriteRegister(uint8_t address, const uint8_t *data, uint8_t length)
{
    uint8_t ignored;
    uint8_t index;

    AD7124_CS_LOW();
    Delay_us(2);
    if (!SPI1_TransferByte((uint8_t)(address & 0x3Fu), &ignored))
        goto fail;

    for (index = 0u; index < length; ++index)
    {
        if (!SPI1_TransferByte(data[index], &ignored))
            goto fail;
    }
    if (!SPI1_WaitNotBusy())
        goto fail;

    Delay_us(2);
    AD7124_CS_HIGH();
    return 1u;

fail:
    AD7124_CS_HIGH();
    return 0u;
}

static uint8_t AD7124_ReadRegister(uint8_t address, uint8_t *data, uint8_t length)
{
    uint8_t ignored;
    uint8_t index;

    AD7124_CS_LOW();
    Delay_us(2);
    if (!SPI1_TransferByte((uint8_t)(0x40u | (address & 0x3Fu)), &ignored))
        goto fail;

    for (index = 0u; index < length; ++index)
    {
        if (!SPI1_TransferByte(0xFFu, &data[index]))
            goto fail;
    }
    if (!SPI1_WaitNotBusy())
        goto fail;

    Delay_us(2);
    AD7124_CS_HIGH();
    return 1u;

fail:
    AD7124_CS_HIGH();
    return 0u;
}

static uint8_t AD7124_Write16(uint8_t address, uint16_t value)
{
    uint8_t data[2];
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
    return AD7124_WriteRegister(address, data, 2u);
}

static uint8_t AD7124_Write24(uint8_t address, uint32_t value)
{
    uint8_t data[3];
    data[0] = (uint8_t)(value >> 16);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)value;
    return AD7124_WriteRegister(address, data, 3u);
}

static uint8_t AD7124_Read16(uint8_t address, uint16_t *value)
{
    uint8_t data[2];
    if (!AD7124_ReadRegister(address, data, 2u))
        return 0u;
    *value = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    return 1u;
}

static uint8_t AD7124_Read24(uint8_t address, uint32_t *value)
{
    uint8_t data[3];
    if (!AD7124_ReadRegister(address, data, 3u))
        return 0u;
    *value = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    return 1u;
}

void AD7124_SPI_Config(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* MISO is an input. AF push-pull on PA6 can cause bus contention. */
    gpio.GPIO_Pin = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    AD7124_CS_HIGH();

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_High;
    spi.SPI_CPHA = SPI_CPHA_2Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7u;
    SPI_Init(SPI1, &spi);
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI1, ENABLE);
}

uint8_t AD7124_8_SoftReset(void)
{
    uint8_t ignored;
    uint8_t index;

    s_config_valid = 0u;
    AD7124_CS_LOW();
    for (index = 0u; index < 8u; ++index)
    {
        if (!SPI1_TransferByte(0xFFu, &ignored))
        {
            AD7124_CS_HIGH();
            return 0u;
        }
    }
    if (!SPI1_WaitNotBusy())
    {
        AD7124_CS_HIGH();
        return 0u;
    }
    AD7124_CS_HIGH();
    Delay_ms(2);
    return 1u;
}

uint8_t AD7124_8_ReadId(void)
{
    uint8_t id = 0xFFu;
    if (!AD7124_ReadRegister(AD7124_REG_ID, &id, 1u))
        return 0xFFu;
    return id;
}

uint8_t AD7124_8_Init(void)
{
    uint16_t value16;
    uint32_t value24;
    uint8_t id;

    s_config_valid = 0u;
    if (!AD7124_Write16(AD7124_REG_ADC_CONTROL, AD7124_ADC_CONTROL_VALUE)
        || !AD7124_Write24(AD7124_REG_IO_CONTROL1, AD7124_IO_CONTROL1_VALUE)
        || !AD7124_Write16(AD7124_REG_CHANNEL0, AD7124_CHANNEL0_VALUE)
        || !AD7124_Write16(AD7124_REG_CONFIG0, AD7124_CONFIG0_VALUE)
        || !AD7124_Write24(AD7124_REG_FILTER0, AD7124_FILTER0_VALUE)
        || !AD7124_Write16(AD7124_REG_CHANNEL1, AD7124_CHANNEL1_VALUE)
        || !AD7124_Write16(AD7124_REG_CONFIG1, AD7124_CONFIG1_VALUE)
        || !AD7124_Write24(AD7124_REG_FILTER1, AD7124_FILTER1_VALUE))
        return 0u;

    Delay_ms(20);
    id = AD7124_8_ReadId();
    if ((id == 0x00u) || (id == 0xFFu))
        return 0u;

    if (!AD7124_Read16(AD7124_REG_ADC_CONTROL, &value16)
        || (value16 != AD7124_ADC_CONTROL_VALUE)
        || !AD7124_Read24(AD7124_REG_IO_CONTROL1, &value24)
        || (value24 != AD7124_IO_CONTROL1_VALUE)
        || !AD7124_Read16(AD7124_REG_CHANNEL0, &value16)
        || (value16 != AD7124_CHANNEL0_VALUE)
        || !AD7124_Read16(AD7124_REG_CHANNEL1, &value16)
        || (value16 != AD7124_CHANNEL1_VALUE)
        || !AD7124_Read16(AD7124_REG_CONFIG0, &value16)
        || (value16 != AD7124_CONFIG0_VALUE)
        || !AD7124_Read16(AD7124_REG_CONFIG1, &value16)
        || (value16 != AD7124_CONFIG1_VALUE))
        return 0u;

    s_config_valid = 1u;
    return 1u;
}

uint8_t AD7124_8_ConfigValid(void)
{
    return s_config_valid;
}

uint32_t AD7124_8_ReadErrorRegister(void)
{
    uint32_t error = 0xFFFFFFFFu;
    if (!AD7124_Read24(AD7124_REG_ERROR, &error))
        return 0xFFFFFFFFu;
    return error;
}

AD7124_Result AD7124_8_ReadSample(AD7124_Sample *sample, uint16_t timeout_ms)
{
    uint8_t status = AD7124_STATUS_RDY;
    uint8_t data[4];
    uint16_t elapsed = 0u;

    if (sample == 0)
        return AD7124_RESULT_SPI_ERROR;

    while (elapsed < timeout_ms)
    {
        if (!AD7124_ReadRegister(AD7124_REG_STATUS, &status, 1u))
            return AD7124_RESULT_SPI_ERROR;
        if ((status & AD7124_STATUS_RDY) == 0u)
            break;
        Delay_ms(1);
        ++elapsed;
    }
    if ((status & AD7124_STATUS_RDY) != 0u)
        return AD7124_RESULT_DRDY_TIMEOUT;

    /*
     * ADC_CONTROL.DATA_STATUS is enabled (0x0480), so one DATA read returns
     * 24-bit conversion data followed by the STATUS byte belonging to that
     * exact conversion.  A separate STATUS read reports CH_ACTIVE (the
     * channel currently being converted), which can already be the next
     * channel and must not be used to label the data in multi-channel mode.
     */
    if (!AD7124_ReadRegister(AD7124_REG_DATA, data, 4u))
        return AD7124_RESULT_SPI_ERROR;

    sample->raw = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    sample->status = data[3];
    sample->channel = (uint8_t)(data[3] & AD7124_STATUS_CHANNEL_MASK);

    if ((data[3] & (AD7124_STATUS_ERROR_FLAG | AD7124_STATUS_POR_FLAG)) != 0u)
        return AD7124_RESULT_ADC_ERROR;
    return AD7124_RESULT_OK;
}

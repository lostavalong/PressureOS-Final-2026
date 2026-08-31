#include "Serial.h"

#include <stdarg.h>

#define SERIAL_RX_BUFFER_SIZE 256u
#define SERIAL_PRINTF_BUFFER  160u
#define SERIAL_TX_WAIT_LIMIT   100000u

static volatile uint8_t s_rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0u;
static volatile uint16_t s_rx_tail = 0u;
static volatile uint8_t s_rx_overflow = 0u;
static volatile uint8_t s_rx_error = 0u;

void Serial_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate = 115200u;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &usart);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

void Serial_SendByte(uint8_t byte)
{
    uint32_t timeout = SERIAL_TX_WAIT_LIMIT;

    /*
     * Keep the proven board behaviour used by the original firmware:
     * write the first byte immediately, then wait until TDR is available
     * for the next byte.  A bounded wait prevents a UART fault from
     * freezing ADC acquisition and the local display.
     */
    USART_SendData(USART1, byte);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
        if (timeout == 0u)
            return;
        --timeout;
    }
}

void Serial_SendArray(const uint8_t *array, uint16_t length)
{
    uint16_t index;
    for (index = 0u; index < length; ++index)
        Serial_SendByte(array[index]);
}

void Serial_SendString(const char *string)
{
    while (*string != '\0')
        Serial_SendByte((uint8_t)*string++);
}

int fputc(int ch, FILE *stream)
{
    (void)stream;
    Serial_SendByte((uint8_t)ch);
    return ch;
}

void Serial_Printf(const char *format, ...)
{
    char buffer[SERIAL_PRINTF_BUFFER];
    int length;
    va_list arguments;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length <= 0)
        return;
    if (length >= (int)sizeof(buffer))
        length = (int)sizeof(buffer) - 1;
    Serial_SendArray((const uint8_t *)buffer, (uint16_t)length);
}

uint8_t Serial_ReceiveByte_NonBlocking(uint8_t *data)
{
    if ((data == 0) || (s_rx_head == s_rx_tail))
        return 0u;

    *data = s_rx_buffer[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) & (SERIAL_RX_BUFFER_SIZE - 1u));
    return 1u;
}

uint8_t Serial_TakeRxOverflow(void)
{
    uint8_t value = s_rx_overflow;
    s_rx_overflow = 0u;
    return value;
}

uint8_t Serial_TakeRxError(void)
{
    uint8_t value = s_rx_error;
    s_rx_error = 0u;
    return value;
}

void USART1_IRQHandler(void)
{
    uint32_t status = USART1->SR;
    uint32_t error_mask = USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE;

    if ((status & error_mask) != 0u)
        s_rx_error = 1u;

    if ((status & (USART_SR_RXNE | error_mask)) != 0u)
    {
        uint8_t value = (uint8_t)USART1->DR;
        if ((status & USART_SR_RXNE) != 0u)
        {
            uint16_t next = (uint16_t)((s_rx_head + 1u) & (SERIAL_RX_BUFFER_SIZE - 1u));
            if (next == s_rx_tail)
                s_rx_overflow = 1u;
            else
            {
                s_rx_buffer[s_rx_head] = value;
                s_rx_head = next;
            }
        }
    }
}

#include "stm32f10x.h"

/*
 * SysTick delays derived from the clock that is actually running.
 * SystemCoreClockUpdate() must be called once near the start of main().
 * The old implementation assumed 72 MHz, so a board falling back to the
 * 8 MHz HSI stretched every delay by roughly nine times.
 */
void Delay_us(uint32_t xus)
{
    uint32_t ticks_per_us;
    uint32_t max_chunk_us;
    uint32_t chunk_us;

    if (xus == 0u)
        return;

    ticks_per_us = SystemCoreClock / 1000000u;
    if (ticks_per_us == 0u)
        ticks_per_us = 1u;

    max_chunk_us = 0x00FFFFFFu / ticks_per_us;
    if (max_chunk_us == 0u)
        max_chunk_us = 1u;

    while (xus != 0u)
    {
        chunk_us = (xus > max_chunk_us) ? max_chunk_us : xus;
        SysTick->LOAD = ticks_per_us * chunk_us - 1u;
        SysTick->VAL = 0u;
        SysTick->CTRL = 0x00000005u; /* HCLK source, counter enabled. */
        while ((SysTick->CTRL & 0x00010000u) == 0u)
        {
        }
        SysTick->CTRL = 0x00000004u;
        xus -= chunk_us;
    }
}

void Delay_ms(uint32_t xms)
{
    while (xms-- != 0u)
        Delay_us(1000u);
}

void Delay_s(uint32_t xs)
{
    while (xs-- != 0u)
        Delay_ms(1000u);
}

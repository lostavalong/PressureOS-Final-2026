#include "stm32f10x.h"                  // Device header
#include "Delay.h"

uint8_t trg,cont;
void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

//uint8_t Key_GetNum(void)
//{
//	uint8_t KeyNum = 0;
//	if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == 0)
//	{
//		Delay_ms(20);
//		while(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == 0);
//		Delay_ms(20);
//		KeyNum = 1;
//	}
//	if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == 0)
//	{
//		Delay_ms(20);
//		while(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == 0);
//		Delay_ms(20);
//		KeyNum = 2;
//	}
//	if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == 0)
//	{
//		Delay_ms(20);
//		while(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == 0);
//		Delay_ms(20);
//		KeyNum = 3;
//	}
//	
//	return KeyNum;
//}

//uint8_t Read_Keys_Toggle(void)
//{
//    static uint8_t last_value = 0;   // 上一次返回值
//    static uint8_t last_key = 0;     // 上一次按下的按键
//    static uint8_t debounce_key = 0; // 当前检测到的有效按键（防抖）
//    uint8_t this_key = 0;

//    // 读取当前按下的键（低电平有效）
//    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == 0)
//        this_key = 1;
//    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == 0)
//        this_key = 2;
//    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == 0)
//        this_key = 3;
//    else
//        this_key = 0;

//    // 只在按键从未按下 → 按下时响应（防抖）
//    if (this_key != 0 && debounce_key == 0)
//    {
//        debounce_key = this_key; // 标记为已按下，等待松开

//        if (last_value == 0)
//        {
//            last_value = this_key;
//            last_key = this_key;
//        }
//        else
//        {
//            if (this_key == last_key)
//            {
//                last_value = 0;  // 同一个键：清零
//            }
//            else
//            {
//                last_value = this_key;  // 不同键：更新值
//                last_key = this_key;
//            }
//        }
//    }
//    else if (this_key == 0)
//    {
//        debounce_key = 0; // 按键松开后清除防抖锁
//    }

//    return last_value;
//}

uint8_t Key_ReadBits(void)
{
    uint8_t value = 0;
    // 读取PC0, PC1, PC2
    value |= (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == 0 ? 1 : 0) << 0; // GC0 -> bit0
    value |= (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == 0 ? 1 : 0) << 1; // GC1 -> bit1
    value |= (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == 0 ? 1 : 0) << 2; // GC2 -> bit2
    return value; // 低三位为你的键值
}

uint8_t read_key(void){
	uint8_t key_data = (Key_ReadBits() ^ 0xff) & 0xff;
	trg = key_data&(key_data^cont);
	cont = key_data;
	
	return trg;
}

#include "stm32f10x.h"                  // Device header
#include "LCD.h"
#include <string.h>
#include "delay.h"

// 像素绘制模式
// 由于大多数绘制操作都是设置像素，因此使用全局变量来选择绘制模式，
// 而不是在每次调用绘图函数时都传递设置/清除/反转模式参数 
uint8_t LCD_PixelMode;


// Screen dimensions
uint16_t scr_width = 400;
uint16_t scr_height = 240;

// 视频RAM缓冲区
static uint8_t vRAM[(SCR_W * SCR_H) >> 3];

// 单个像素设置/清除的查找表
static const uint8_t LUT_PSET[8] = { 0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE };
static const uint8_t LUT_PRST[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

// 交换变量的值（异或算法）
// 说明：
//   变量必须是相同的数据类型
//   当 A 和 B 是同一个对象时，该方法无效——此时结果会变为 0
#define SWAP_VARS(A, B) do { (A) ^= (B); (B) ^= (A); (A) ^= (B); } while (0)

// === GPIO 初始化（SCS, EXTCOMIN, DISP）===
void LCD_GPIO_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    // SCS = PC7
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    LCD_SCS_LOW();  // 初始拉高

    // EXTCOMIN = PC9（TIM3_CH4）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// === SPI2 初始化（PB13=SCK, PB15=MOSI）===
void LCD_SPI2_Init(void)
{
    RCC_ClocksTypeDef clocks;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    // PB13 - SCK, PB15 - MOSI
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SPI_I2S_DeInit(SPI2);
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    /* Keep LCD SCLK near the proven 1.125 MHz value at either 8 or 72 MHz. */
    RCC_GetClocksFreq(&clocks);
    if ((clocks.PCLK1_Frequency / 2u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    else if ((clocks.PCLK1_Frequency / 4u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    else if ((clocks.PCLK1_Frequency / 8u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    else if ((clocks.PCLK1_Frequency / 16u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    else if ((clocks.PCLK1_Frequency / 32u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;
    else if ((clocks.PCLK1_Frequency / 64u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    else if ((clocks.PCLK1_Frequency / 128u) <= 1200000u)
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    else
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
}

// === SPI 发送一个字节 ===
void LCD_Write_Byte(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, data);
}

// === 输出 EXTCOMIN 方波，TIM3_CH4@PC9 输出 1Hz ===
void LCD_EXTCOMIN_PWM_Init(void)
{
    RCC_ClocksTypeDef clocks;
    uint32_t timer_clock_hz;
    uint32_t prescaler_divisor;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_GetClocksFreq(&clocks);
    timer_clock_hz = clocks.PCLK1_Frequency;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        timer_clock_hz *= 2u;
    prescaler_divisor = timer_clock_hz / 10000u;
    if (prescaler_divisor == 0u)
        prescaler_divisor = 1u;

    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(prescaler_divisor - 1u);
    TIM_TimeBaseStructure.TIM_Period = 9999;           // 10kHz / 10000 = 1Hz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 5000; // 占空比 50%
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM3, &TIM_OCInitStructure);

    TIM_Cmd(TIM3, ENABLE);
}

// 清除显示内存（清屏）
void LCD_Clear(void) {
	// Send "Clear Screen" command
	LCD_SCS_HIGH();
	Delay_us(50);
	LCD_Write_Byte(0x20);
	LCD_Write_Byte(0x00);
	Delay_us(50);
	LCD_SCS_LOW();
}

// Clear vRAM without assuming that the linker placed it on a word boundary.
void LCD_vRAM_Clear(void) {
	register uint8_t *ptr = vRAM;
	register uint32_t i = sizeof(vRAM);

	while (i--) {
		*ptr++ = 0x00;
	}
}

void SPI_SendBuf(uint8_t *pBuf, uint32_t length)
{
    while (length--)
    {
        while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
			SPI_I2S_SendData(SPI2, *pBuf++);
        // 如果需要避免 RX 溢出，也可以加上接收清空：
//        while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
//			(void)SPI_I2S_ReceiveData(SPI2);
    }
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET); // 等待发送完成
}

// Send vRAM buffer into display
void SMLCD_Flush(void) {
	register uint8_t *ptr = vRAM;

	LCD_SCS_HIGH();
	// Send "Write Line" command
	LCD_Write_Byte(SMLCD_CMD_WRITE);

	// Use look-up table for line number
	// Look-up table for line numbers with reversed bit order
	static const uint8_t LUT_LINE[240] = {
			0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08,
			0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04,
			0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 0x0C,
			0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 0x02,
			0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A,
			0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, 0x06,
			0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 0x0E,
			0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01,
			0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1, 0x09,
			0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 0x05,
			0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D,
			0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD, 0x03,
			0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 0x0B,
			0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07,
			0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 0x0F
	};
	
	// This variable declared as 32-bit to improve performance on on 32-bit MCU
	// it can be changed to 8 or 16 bit
	register uint32_t line;
	
	// Normal lines order
	line = 0;
	do {
		// Send: line number -> line data -> trailer byte
		LCD_Write_Byte(LUT_LINE[line]);
		SPI_SendBuf(ptr, SCR_W >> 3);
		LCD_Write_Byte(SMLCD_CMD_NOP);
		ptr += SCR_W >> 3;
	} while (++line < SCR_H);

	// One more trailer after last string has been transmitted
	LCD_Write_Byte(SMLCD_CMD_NOP);
	LCD_SCS_LOW();
}

// 绘制单个像素
// 输入：
//   X, Y - 像素的坐标
// 注意：像素的绘制模式将由 LCD_PixelMode 的值决定
// 注意：为提升性能，X 和 Y 坐标被声明为 "register uint32_t"
//       对于其他编译器/CPU，可以或应当改用其他类型（如 16 位）
void LCD_Pixel(register uint32_t X, register uint32_t Y) {
	register uint32_t offset;
	register uint8_t bpos;

//	if (lcd_orientation & (LCD_ORIENT_CCW | LCD_ORIENT_CW)) {
//		SWAP_VARS(X, Y);
//	}
//	if (lcd_orientation & (LCD_ORIENT_180 | LCD_ORIENT_CCW)) {
//		X = SCR_W - 1 - X;
//	}

	// 视频缓冲区中的偏移量
	offset = ((Y * SCR_W) + X) >> 3;

	// 确保偏移量在视频缓冲区范围内
	if (offset > ((SCR_W * SCR_H) >> 3) - 1) {
		return;
	}

	// 字节中的位位置
	bpos = X & 0x07;

	// 在 vRAM 中更新像素
	// 使用 bit-banding

	// 用于计算 bit-banding 地址的查找表
	// 注意：是的，看起来挺吓人，但它是静态的
	static const uint32_t LUT_BB[8] = {
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x1C,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x18,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x14,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x10,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x0C,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x08,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x04,
			SRAM_BB_BASE + (SRAM_BASE << 5) + 0x00
	};

	// 指向与给定像素坐标对应的 bit-banding 地址的指针
	register uint32_t *BB = (uint32_t *)(LUT_BB[bpos] + ((uint32_t)((void *)(&vRAM[offset])) << 5));

	// 更新像素
	switch (LCD_PixelMode) {
		case LCD_PRES:
			*BB  = 1;
			break;
		case LCD_PINV:
			*BB ^= 1;
			break;
		case LCD_PSET:
		default:
			*BB  = 0;
			break;
	}
	
}

// 优化的垂直线绘制（不考虑屏幕旋转）
// 输入：
//   X - 水平坐标
//   Y - 垂直坐标
//   H - 线段高度
void LCD_VLineInt(uint16_t X, uint16_t Y, uint16_t H) {
	register uint8_t *ptr = &vRAM[((Y * SCR_W) + X) >> 3];
	register uint8_t mask;

	// Draw line
	X &= 0x07;
	switch (LCD_PixelMode) {
		case LCD_PRES:
			mask = LUT_PRST[X];
			while (H--) {
				*ptr |= mask;
				ptr += SCR_W >> 3;
			}
			break;
//		case LCD_PINV:
//			mask = LUT_PRST[X];
//			while (H--) {
//				*ptr ^= mask;
//				ptr += SCR_W >> 3;
//			}
//			break;
		case LCD_PSET:
		default:
			mask = LUT_PSET[X];
			while (H--) {
				*ptr &= mask;
				ptr += SCR_W >> 3;
			}
			break;
	}

}

void LCD_HLineInt(uint16_t X, uint16_t Y, uint16_t W) {
	register uint8_t *ptr = &vRAM[((Y * SCR_W) + X) >> 3];
	register uint8_t modulo = X & 0x07;
	register uint8_t mask;

	// Look-up tables
	static const uint8_t LUT_B1[] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80 };
	static const uint8_t LUT_B2[] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

	// 第一个部分字节
	if (modulo) {
		// 获取第一个部分字节的位掩码
		modulo = 8 - modulo;
		mask = LUT_B1[modulo];

		if (modulo > W) {
			// 如果线段不会超出当前字节，则修剪位掩码
			mask |= LUT_B2[modulo - W];
		}

		// 更新第一个部分字节
		switch (LCD_PixelMode) {
			case LCD_PRES:
				*ptr |= ~mask;
				break;
//			case LCD_PINV:
//				*ptr ^= ~mask;
//				break;
			case LCD_PSET:
			default:
				*ptr &=  mask;
				break;
		}

		// 线段结束了吗？
		if (modulo > W) {
			return;
		}

		// 将指针移动到该行的下一个字节，并减少线段高度计数器
		ptr++;
		W -= modulo;
	}

	/*
	 * Do not cast ptr to uint32_t*.  A 400-pixel row occupies 50 bytes,
	 * therefore many rows naturally begin at non-word-aligned addresses.
	 */
	if (W > 7) {
		// Modify full bytes (8 pixels at once)
		switch (LCD_PixelMode) {
			case LCD_PRES:
				do {
					*ptr++ = 0xFF;
					W -= 8;
				} while (W > 7);
				break;
//			case LCD_PINV:
//				do {
//					*ptr++ ^= 0xFF;
//					W -= 8;
//				} while (W > 7);
//				break;
			case LCD_PSET:
			default:
				do {
					*ptr++ = 0x00;
					W -= 8;
				} while (W > 7);
				break;
		}

	}

	// Last partial byte?
	if (W) {
		mask = LUT_B2[8 - W];
		switch (LCD_PixelMode) {
			case LCD_PRES:
				*ptr |= ~mask;
				break;
//			case LCD_PINV:
//				*ptr ^= ~mask;
//				break;
			case LCD_PSET:
			default:
				*ptr &= mask;
				break;
		}
	}
}

// Draw horizontal line
// input:
//   X1, X2 - left and right horizontal coordinates
//   Y - vertical coordinate
void LCD_HLine(uint16_t X1, uint16_t X2, uint16_t Y) {
	register uint16_t X;
	register uint16_t L;

	if (X1 > X2) {
		X = X2; L = X1 - X2;
	} else {
		X = X1; L = X2 - X1;
	}
	L++;

	LCD_HLineInt(X, Y, L);
}

// Draw vertical line
// input:
//   X - horizontal coordinate
//   Y1,Y2 - top and bottom vertical coordinates
void LCD_VLine(uint16_t X, uint16_t Y1, uint16_t Y2) {
	register uint16_t Y;
	register uint16_t L;

	if (Y1 > Y2) {
		Y = Y2; L = Y1 - Y2;
	} else {
		Y = Y1; L = Y2 - Y1;
	}
	L++;

	LCD_VLineInt(X, Y, L);
}

// 绘制矩形
// 输入：
//   X1,Y1 - 左上角坐标
//   X2,Y2 - 右下角坐标
void LCD_Rect(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2) {
	LCD_HLine(X1, X2, Y1);
	LCD_HLine(X1, X2, Y2);
	if (Y1 > Y2) {
		SWAP_VARS(Y1, Y2);
	}
	Y1++;
	Y2--;
	LCD_VLine(X1, Y1, Y2);
	LCD_VLine(X2, Y1, Y2);
}

// 绘制实心矩形
// 输入：
//   X1, Y1 - 左上角坐标
//   X2, Y2 - 右下角坐标
// 注意：不会检查 vRAM 边界，因此调用者在指定 X 和 Y 坐标时
//       必须确保在屏幕宽度和高度范围内
void LCD_FillRect(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2) {
	static const uint8_t LUT_B1[] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };
	static const uint8_t LUT_B2[] = { 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF };

	if (X1 > X2) {
		SWAP_VARS(X1, X2);
	}

	if (Y1 > Y2) {
		SWAP_VARS(Y1, Y2);
	}

	uint16_t dW;

	// Mask for first and last byte
	register uint8_t mask_fb = LUT_B1[X1 & 0x07];
	register uint8_t mask_lb = LUT_B2[X2 & 0x07];

	// Offset in vRAM
	uint8_t *ptr_base = &vRAM[(((Y1 * SCR_W) + X1) >> 3)];

	// Line width in bytes
	dW = (X2 >> 3) - (X1 >> 3);

	if (dW) {
		// Multiple bytes
		register uint16_t cntr;
		register uint8_t *ptr;

		switch (LCD_PixelMode) {
			case LCD_PRES:
				do {
					cntr = dW;
					ptr = ptr_base;
					*ptr++ |= mask_fb;
					while (--cntr) {
						*ptr++ = 0xFF;
					};
					*ptr |= mask_lb;
					ptr_base += SCR_W >> 3;
				} while (Y1++ < Y2);
				break;
			case LCD_PSET:
			default:
				mask_fb = ~mask_fb;
				mask_lb = ~mask_lb;
				do {
					cntr = dW;
					ptr = ptr_base;
					*ptr++ &= mask_fb;
					while (--cntr) {
						*ptr++ = 0x00;
					};
					*ptr &= mask_lb;
					ptr_base += SCR_W >> 3;
				} while (Y1++ < Y2);
				break;
		}
	} else {
		// Single byte
		mask_fb &= mask_lb;

		switch (LCD_PixelMode) {
			case LCD_PRES:
				do {
					*ptr_base |= mask_fb;
					ptr_base += SCR_W >> 3;
				} while (Y1++ < Y2);
				break;
			
			case LCD_PSET:
			default:
				mask_fb = ~mask_fb;
				do {
					*ptr_base &= mask_fb;
					ptr_base += SCR_W >> 3;
				} while (Y1++ < Y2);
				break;
		}
	}
}

// 绘制线条
// 输入：
//   X1, Y1 - 起点坐标（左上）
//   X2, Y2 - 终点坐标（右下）
void LCD_Line(int16_t X1, int16_t Y1, int16_t X2, int16_t Y2) {
	int16_t dX = X2 - X1;
	int16_t dY = Y2 - Y1;
	int16_t dXsym = (dX > 0) ? 1 : -1;
	int16_t dYsym = (dY > 0) ? 1 : -1;

	if (dX == 0) {
		LCD_VLineInt(X1, Y1, Y2);
		return;
	}
	if (dY == 0) {
		LCD_HLineInt(X1, X2, Y1);
		return;
	}

	dX *= dXsym;
	dY *= dYsym;
	int16_t dX2 = dX << 1;
	int16_t dY2 = dY << 1;
	int16_t di;

	if (dX >= dY) {
		di = dY2 - dX;
		while (X1 != X2) {
			LCD_Pixel(X1, Y1);
			X1 += dXsym;
			if (di < 0) {
				di += dY2;
			} else {
				di += dY2 - dX2;
				Y1 += dYsym;
			}
		}
	} else {
		di = dX2 - dY;
		while (Y1 != Y2) {
			LCD_Pixel(X1, Y1);
			Y1 += dYsym;
			if (di < 0) {
				di += dX2;
			} else {
				di += dX2 - dY2;
				X1 += dXsym;
			}
		}
	}
	LCD_Pixel(X1, Y1);
}

// 绘制圆
// 输入：
//   Xc, Yc - 圆心坐标
//   R      - 圆的半径
void LCD_Circle(int16_t Xc, int16_t Yc, uint16_t R) {
	int16_t err = 1 - R;
	int16_t dx  = 1;
	int16_t dy  = -2 * R;
	int16_t x   = 0;
	int16_t y   = R;

	register int16_t sh = scr_height;
	register int16_t sw = scr_width;
	register int16_t tt;

	// Vertical and horizontal points
	if (Xc + R < sw) LCD_Pixel(Xc + R, Yc);
	if (Xc - R > -1) LCD_Pixel(Xc - R, Yc);
	if (Yc + R < sh) LCD_Pixel(Xc, Yc + R);
	if (Yc - R > -1) LCD_Pixel(Xc, Yc - R);

	while (x < y) {
		if (err >= 0) {
			dy  += 2;
			err += dy;
			y--;
		}
		dx  += 2;
		err += dx + 1;
		x++;

		// Draw pixels of eight octants
		tt = Xc + x;
		if (tt < sw) {
			if (Yc + y < sh) LCD_Pixel(tt, Yc + y);
			if (Yc - y > -1) LCD_Pixel(tt, Yc - y);
		}
		tt = Xc - x;
		if (tt > -1) {
			if (Yc + y < sh) LCD_Pixel(tt, Yc + y);
			if (Yc - y > -1) LCD_Pixel(tt, Yc - y);
		}
		tt = Xc + y;
		if (tt < sw) {
			if (Yc + x < sh) LCD_Pixel(tt, Yc + x);
			if (Yc - x > -1) LCD_Pixel(tt, Yc - x);
		}
		tt = Xc - y;
		if (tt > -1) {
			if (Yc + x < sh) LCD_Pixel(tt, Yc + x);
			if (Yc - x > -1) LCD_Pixel(tt, Yc - x);
		}
	}
}

// 绘制椭圆
// 输入：
//   Xc, Yc - 椭圆中心的坐标
//   Ra, Rb - 水平半径和垂直半径
void LCD_Ellipse(int16_t Xc, int16_t Yc, uint16_t Ra, uint16_t Rb) {
	int16_t x  = 0;
	int16_t y  = Rb;
	int32_t A2 = Ra * Ra;
	int32_t B2 = Rb * Rb;
	int32_t C1 = -((A2 >> 2) + (Ra & 0x01) + B2);
	int32_t C2 = -((B2 >> 2) + (Rb & 0x01) + A2);
	int32_t C3 = -((B2 >> 2) + (Rb & 0x01));
	int32_t t  = -A2 * y;
	int32_t dX = B2 * x * 2;
	int32_t dY = -A2 * y * 2;
	int32_t dXt2 = B2 * 2;
	int32_t dYt2 = A2 * 2;

	register int16_t sh = scr_height;
	register int16_t sw = scr_width;

	while ((y >= 0) && (x <= Ra)) {
		if ((Xc + x < sw) && (Yc + y < sh)) {
			LCD_Pixel(Xc + x, Yc + y);
		}
		if (x || y) {
			if ((Xc - x > -1) && (Yc - y > -1)) {
				LCD_Pixel(Xc - x, Yc - y);
			}
		}
		if (x && y) {
			if ((Xc + x < sw) && (Yc - y > - 1)) {
				LCD_Pixel(Xc + x, Yc - y);
			}
			if ((Xc - x > -1) && (Yc + y < sh)) {
				LCD_Pixel(Xc - x, Yc + y);
			}
		}

		if ((t + (x * B2) <= C1) || (t + (y * A2) <= C3)) {
			dX += dXt2;
			t  += dX;
			x++;
		} else if (t - (y * A2) > C2) {
			dY += dYt2;
			t  += dY;
			y--;
		} else {
			dX += dXt2;
			dY += dYt2;
			t  += dX;
			t  += dY;
			x++;
			y--;
		}
	}
}

// Text functions

// Draw a single character
// input:
//   X,Y - character top left corner coordinates
//   chr - character to be drawn
//   font - pointer to font
// return: character width in pixels
uint8_t LCD_PutChar(uint16_t X, uint16_t Y, uint8_t chr, const Font_TypeDef *font) {
	uint16_t pX;
	uint16_t pY;
	uint8_t tmpCh;
	uint8_t bL;
	const uint8_t *pCh;

	// If the specified character code is out of bounds should substitute the code of the "unknown" character
	if ((chr < font->font_MinChar) || (chr > font->font_MaxChar)) {
		chr = font->font_UnknownChar;
	}

	// Pointer to the first byte of character in font data array
	pCh = &font->font_Data[(chr - font->font_MinChar) * font->font_BPC];

	// Draw character
	if (font->font_Scan == FONT_V) {
		// Vertical pixels order
		if (font->font_Height < 9) {
			// Height is 8 pixels or less (one byte per column)
			pX = X;
			while (pX < X + font->font_Width) {
				pY = Y;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						LCD_Pixel(pX, pY);
					}
					tmpCh >>= 1;
					pY++;
				}
				pX++;
			}
		} else {
			// Height is more than 8 pixels (several bytes per column)
			pX = X;
			while (pX < X + font->font_Width) {
				pY = Y;
				while (pY < Y + font->font_Height) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								LCD_Pixel(pX, pY);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pY++;
								bL--;
							} else {
								pY += bL;
								break;
							}
						}
					} else {
						pY += bL;
					}
				}
				pX++;
			}
		}
	} else {
		// Horizontal pixels order
		if (font->font_Width < 9) {
			// Width is 8 pixels or less (one byte per row)
			pY = Y;
			while (pY < Y + font->font_Height) {
				pX = X;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) {
						LCD_Pixel(pX, pY);
					}
					tmpCh >>= 1;
					pX++;
				}
				pY++;
			}
		} else {
			// Width is more than 8 pixels (several bytes per row)
			pY = Y;
			while (pY < Y + font->font_Height) {
				pX = X;
				while (pX < X + font->font_Width) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) {
								LCD_Pixel(pX, pY);
							}
							tmpCh >>= 1;
							if (tmpCh) {
								pX++;
								bL--;
							} else {
								pX += bL;
								break;
							}
						}
					} else {
						pX += bL;
					}
				}
				pY++;
			}
		}
	}
	
	return font->font_Width + 1;
	//return font->font_Width + 1;
}

// 绘制字符串
// 输入：
//   X, Y   - 第一个字符的左上角坐标
//   str    - 指向以零结尾的字符串的指针
//   font   - 字体指针
// 返回值：
//   字符串的总宽度（单位：像素）
uint16_t LCD_PutStr(uint16_t X, uint16_t Y, const char *str, const Font_TypeDef *font) {
	uint16_t pX = X;
	uint16_t eX = scr_width - font->font_Width - 1;

	while (*str) {
		pX += LCD_PutChar(pX, Y, *str++, font);
		if (pX > eX) break;
	}

	return (pX - X);
}

uint16_t LCD_PutStrLF(uint16_t X, uint16_t Y, const char *str, const Font_TypeDef *font) {
	uint32_t strLen = 0;

	while (*str) {
		LCD_PutChar(X, Y, *str++, font);
		if (X < scr_width - font->font_Width - 1) {
			X += font->font_Width + 1;
		} else if (Y < scr_height - font->font_Height - 1) {
			X = 0;
			Y += font->font_Height + 2;
		} else {
			X = 0;
			Y = 0;
		}
		strLen++;
	};

	return strLen * (font->font_Width + 1);
}

// 绘制有符号整数值
// 输入：
//   X, Y   - 第一个数字的左上角坐标
//   num    - 有符号整数值
//   font   - 字体指针
// 返回值：
//   数字所占宽度（单位：像素）
uint8_t LCD_PutInt(uint16_t X, uint16_t Y, int32_t num, const Font_TypeDef *font) {
	uint8_t str[11]; // 10 chars max for INT32_MIN..INT32_MAX (without sign)
	uint8_t *pStr = str;
	uint8_t neg = 0;
	uint16_t pX = X;

	// String termination character
	*pStr++ = '\0';

	// Convert number to characters
	if (num < 0) {
		neg = 1;
		num *= -1;
	}
	do { *pStr++ = (num % 10) + '0'; } while (num /= 10);
	if (neg) {
		*pStr++ = '-';
	}

	// Draw a number
	while (*--pStr) {
		pX += LCD_PutChar(pX, Y, *pStr, font);
	}

	return (pX - X);
}

// 绘制无符号整数值
// 输入：
//   X, Y   - 第一个数字的左上角坐标
//   num    - 无符号整数值
//   font   - 字体指针
// 返回值：
//   数字所占宽度（单位：像素）
uint8_t LCD_PutIntU(uint16_t X, uint16_t Y, uint32_t num, const Font_TypeDef *font) {
	uint8_t str[11]; // 10 chars max for UINT32_MAX
	uint8_t *pStr = str;
	uint16_t pX = X;

	// Convert number to characters
	*pStr++ = 0; // String termination character
	do { *pStr++ = (num % 10) + '0'; } while (num /= 10);

	// Draw a number
	while (*--pStr) {
		pX += LCD_PutChar(pX, Y, *pStr, font);
	}

	return (pX - X);
}

// 绘制带小数点的有符号整数值
// 输入：
//   X, Y       - 第一个字符的左上角坐标  
//   num        - 无符号整数值（将被格式化为带小数点的数）  
//   decimals   - 小数点后的位数  
//   font       - 字体指针  
// 返回值：
//   数字所占的总宽度（单位：像素）
uint8_t LCD_PutIntF(uint16_t X, uint16_t Y, int32_t num, uint8_t decimals, const Font_TypeDef *font) {
	uint8_t str[11]; // 10 chars max for INT32_MIN..INT32_MAX (without sign)
	uint8_t *pStr = str;
	uint8_t neg = 0;
	uint8_t strLen = 0;
	uint16_t pX = X;

	// Convert number to characters
	*pStr++ = '\0'; // String termination character
	if (num < 0) {
		neg = 1;
		num *= -1;
	}
	do {
		*pStr++ = (num % 10) + '0';
		strLen++;
	} while (num /= 10);

	// Add leading zeroes
	if (strLen <= decimals) {
		while (strLen <= decimals) {
			*pStr++ = '0';
			strLen++;
		}
	}

	// Minus sign?
	if (neg) {
		*pStr++ = '-';
		strLen++;
	}

	// Draw a number
	while (*--pStr) {
		pX += LCD_PutChar(pX, Y, *pStr, font);
		if (decimals && (--strLen == decimals)) {
			// Draw decimal point
			LCD_FillRect(pX, Y + font->font_Height - 5, pX + 3, Y + font->font_Height - 7);
			pX += 3;
		}
	}

	return (pX - X);
}

// 绘制带前导零的有符号整数值
// 输入：
//   X, Y     - 第一个字符的左上角坐标  
//   num      - 无符号整数值  
//   digits   - 最小总位数（例如：num=35，digits=5 → 显示为 00035）  
//   font     - 字体指针  
// 返回值：
//   数字所占的总宽度（单位：像素）
uint8_t LCD_PutIntLZ(uint16_t X, uint16_t Y, int32_t num, uint8_t digits, const Font_TypeDef *font) {
	uint8_t str[11]; // 10 chars max for INT32_MIN..INT32_MAX (without sign)
	uint8_t *pStr = str;
	uint8_t neg = 0;
	uint8_t strLen = 0;
	uint16_t pX = X;

	// Convert number to characters
	*pStr++ = 0; // String termination character
	if (num < 0) {
		neg = 1;
		num *= -1;
	}
	do {
		*pStr++ = (num % 10) + '0';
		strLen++;
	} while (num /= 10);

	// Add leading zeroes
	if (strLen < digits) {
		while (strLen++ < digits) {
			*pStr++ = '0';
		}
	}

	// Minus sign?
	if (neg) *pStr++ = '-';

	// Draw a number
	while (*--pStr) {
		pX += LCD_PutChar(pX, Y, *pStr, font);
	}

	return (pX - X);
}

// 以十六进制形式绘制整数
// 输入：
//   X, Y   - 第一个字符的左上角坐标  
//   num    - 无符号整数值  
//   font   - 字体指针  
// 返回值：
//   数值所占的总宽度（单位：像素）
uint8_t LCD_PutHex(uint16_t X, uint16_t Y, uint32_t num, const Font_TypeDef *font) {
	uint8_t str[11]; // 10 chars max for UINT32_MAX
	uint8_t *pStr = str;
	uint16_t pX = X;

	// Convert number to characters
	*pStr++ = 0; // String termination character
	do {
		*pStr = (num % 0x10) + '0';
		if (*pStr > '9') {
			*pStr += 7;
		}
		pStr++;
	} while (num /= 0x10);

	// Draw a number
	while (*--pStr) {
		pX += LCD_PutChar(pX, Y, *pStr, font);
	}

	return (pX - X);
}

// 绘制单色位图
// 输入参数：
//   X, Y - 位图左上角的坐标
//   W, H - 位图的宽度和高度（单位：像素）
//   pBMP - 指向位图数据数组的指针
// 说明：位图中每个 '1' 位将绘制为一个像素，
//       每个 '0' 位则不绘制（透明位图）
// 位图格式：每个字节对应垂直方向上的8个像素，最低位在上，多余的低位舍弃
void LCD_DrawBitmap(uint16_t X, uint16_t Y, uint16_t W, uint16_t H, const uint8_t* pBMP) {
	uint16_t pX;
	uint16_t pY;
	uint8_t tmpCh;
	uint8_t bL;

	pY = Y;
	while (pY < Y + H) {
		pX = X;
		while (pX < X + W) {
			bL = 0;
			tmpCh = *pBMP++;
			if (tmpCh) {
				while (bL < 8) {
					if (tmpCh & 0x01) {
						LCD_Pixel(pX, pY + bL);
					}
					tmpCh >>= 1;
					if (tmpCh) {
						bL++;
					} else {
						pX++;
						break;
					}
				}
			} else {
				pX++;
			}
		}
		pY += 8;
	}
}

// 反转视频缓冲区中的图像区域
// 输入：
//   X, Y - 区域左上角的坐标  
//   W    - 区域的宽度（像素）  
//   H    - 区域的高度（像素）
void LCD_Invert(uint16_t X, uint16_t Y, uint16_t W, uint16_t H) {
	uint8_t tmp;

	// Inverting part of image with support of screen rotation functionality
	// is a non-trivial task (look at LCD_FillRect...)
	// Therefore do that in the most simple and blunt way...

	// Save value of PixelMode variable
	tmp = LCD_PixelMode;
	// Change drawing mode to INVERT
	LCD_PixelMode = LCD_PINV;
	// Draw a filled rectangle
	LCD_FillRect(X, Y, X + W - 1, Y + H - 1);
	// Restore previous value of PixelMode
	LCD_PixelMode = tmp;
}

// Invert the full buffer without relying on word alignment.
void LCD_InvertFull(void) {
	register uint8_t *ptr = vRAM;
	register uint32_t i = sizeof(vRAM);

	while (i--) {
		*ptr++ ^= 0xFF;
	}
}

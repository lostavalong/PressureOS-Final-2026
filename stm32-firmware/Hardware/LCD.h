#ifndef __LCD_H
#define __LCD_H

#include "stm32f10x.h"

// Public variables
extern uint16_t scr_width;
extern uint16_t scr_height;
extern uint8_t LCD_PixelMode;

// Pixel draw mode
enum {
	LCD_PSET = 0, // Set pixel
	LCD_PRES = 1, // Reset pixel
	LCD_PINV = 2  // Invert pixel
};

// Screen resolution (in pixels)
#define SCR_W                      400 // width
#define SCR_H                      240 // height

// Display commands
#define SMLCD_CMD_WRITE            (0x80) // Write line
#define SMLCD_CMD_VCOM             (0x40) // VCOM bit (not a command in fact)
#define SMLCD_CMD_CLS              (0x20) // Clear the screen to all white
#define SMLCD_CMD_NOP              (0x00) // No command

// Font scan lines enumeration
enum {
	FONT_V = (uint8_t)0,        // Vertical font scan lines
	FONT_H = (uint8_t)(!FONT_V) // Horizontal font scan lines
};


// Structure describing a font
typedef struct {
	uint8_t font_Width;       // Width of character
	uint8_t font_Height;      // Height of character
	uint8_t font_BPC;         // Bytes for one character
	uint8_t font_Scan;        // Font scan lines behavior
	uint8_t font_MinChar;     // Code of the first known symbol
	uint8_t font_MaxChar;     // Code of the last known symbol
	uint8_t font_UnknownChar; // Code of the unknown symbol
	const uint8_t *font_Data;      // Font data
} Font_TypeDef;

// SCS (片选)
#define LCD_SCS_PORT GPIOC
#define LCD_SCS_PIN  GPIO_Pin_7

#define LCD_SCS_LOW()   GPIO_ResetBits(LCD_SCS_PORT, LCD_SCS_PIN)
#define LCD_SCS_HIGH()  GPIO_SetBits(LCD_SCS_PORT, LCD_SCS_PIN)

// 初始化函数
void LCD_GPIO_Init(void);
void LCD_SPI2_Init(void);
void LCD_EXTCOMIN_PWM_Init(void);
void LCD_Clear(void);
void LCD_vRAM_Clear(void);
void LCD_Write_Byte(uint8_t data);

void SPI_SendBuf(uint8_t *pBuf, uint32_t length);
void SMLCD_Flush(void);
void LCD_Pixel(register uint32_t X, register uint32_t Y);
void LCD_VLineInt(uint16_t X, uint16_t Y, uint16_t H);
void LCD_HLineInt(uint16_t X, uint16_t Y, uint16_t W);
void LCD_HLine(uint16_t X1, uint16_t X2, uint16_t Y);
void LCD_VLine(uint16_t X, uint16_t Y1, uint16_t Y2);
void LCD_Rect(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2);
void LCD_FillRect(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2);
void LCD_Line(int16_t X1, int16_t Y1, int16_t X2, int16_t Y2);
void LCD_Circle(int16_t Xc, int16_t Yc, uint16_t R);
void LCD_Ellipse(int16_t Xc, int16_t Yc, uint16_t Ra, uint16_t Rb);

uint8_t LCD_PutChar(uint16_t X, uint16_t Y, uint8_t chr, const Font_TypeDef *font);
uint16_t LCD_PutStr(uint16_t X, uint16_t Y, const char *str, const Font_TypeDef *font);
uint16_t LCD_PutStrLF(uint16_t X, uint16_t Y, const char *str, const Font_TypeDef *font);
uint8_t LCD_PutInt(uint16_t X, uint16_t Y, int32_t num, const Font_TypeDef *font);
uint8_t LCD_PutIntU(uint16_t X, uint16_t Y, uint32_t num, const Font_TypeDef *font);
uint8_t LCD_PutIntF(uint16_t X, uint16_t Y, int32_t num, uint8_t decimals, const Font_TypeDef *font);
uint8_t LCD_PutIntLZ(uint16_t X, uint16_t Y, int32_t num, uint8_t digits, const Font_TypeDef *font);
uint8_t LCD_PutHex(uint16_t X, uint16_t Y, uint32_t num, const Font_TypeDef *font);
void LCD_DrawBitmap(uint16_t X, uint16_t Y, uint16_t W, uint16_t H, const uint8_t* pBMP);
void LCD_Invert(uint16_t X, uint16_t Y, uint16_t W, uint16_t H);
void LCD_InvertFull(void);

#endif

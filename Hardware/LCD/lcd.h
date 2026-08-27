#ifndef LCD_H
#define LCD_H

#include "main.h"
#include <stdlib.h>
#include <math.h>
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;

/* 字号枚举 */
typedef enum {
    FONT_1206 = 12,
    FONT_1608 = 16,
    FONT_2412 = 24,
    FONT_3216 = 32
} FontSize;

/* 硬件引脚 */
#define LCD_CS_PIN         GPIO_PIN_4
#define LCD_CS_PORT        GPIOA
#define LCD_DC_PIN         GPIO_PIN_0
#define LCD_DC_PORT        GPIOB
#define LCD_RST_PIN        GPIO_PIN_1
#define LCD_RST_PORT       GPIOB

/* 引脚控制宏 */
#define LCD_CS(n)    HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  n ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_DC(n)    HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  n ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_RST(n)   HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, n ? GPIO_PIN_SET : GPIO_PIN_RESET)

/* LCD 参数 */
#define LCD_WIDTH       128
#define LCD_HEIGHT      160
#define LCD_CMD         0
#define LCD_DATA        1

/* 颜色定义（RGB565） */
#define WHITE           0xFFFF
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define GRAY            0x8430
#define STATUS_BG       0x1082

/* 底层通信 */
void LCD_Write_Byte(uint8_t data, uint8_t cmd);
void LCD_Set_Window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/* 初始化与清屏 */
void LCD_Init(void);
void LCD_Clear(uint16_t color);

/* 基础图形 */
void LCD_Draw_Point(uint16_t x, uint16_t y, uint16_t color);
void LCD_Draw_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_Draw_HLine(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color);
void LCD_Draw_VLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_Draw_RoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color);
void LCD_Draw_RoundRect_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color);

/* 批量填充（核心优化接口） */
void LCD_Fill_Rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_Fill_Colors(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* 字库显示 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t chr, FontSize size, uint16_t color);
void LCD_ShowString(uint16_t x, uint16_t y, uint8_t *str, FontSize size, uint16_t color);
void LCD_ShowChineseChar(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint16_t color);
void LCD_ShowChineseString(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint16_t color);

#endif

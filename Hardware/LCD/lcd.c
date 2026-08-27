#include "lcd.h"
#include "delay.h"
#include "lcdfont.h"

/* SPI 批量发送缓冲：128 字节 = 64 像素，平衡栈占用与传输速度 */
#define LCD_BATCH_SIZE  128

/* ==================== 底层 SPI 通信 ==================== */

/**
  * @brief  通过 SPI1 发送单字节（内联优化，5ms 超时）
  */
static inline void SPI_Send_Byte(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi1, &byte, 1, 5);
}

/**
  * @brief  向 LCD 写入命令或数据（内联优化）
  * @param  dat  待发送字节
  * @param  cmd  0=命令，1=数据
  */
static inline void LCD_Write_Byte(uint8_t dat, uint8_t cmd)
{
    LCD_CS(0);
    LCD_DC(cmd);
    SPI_Send_Byte(dat);
    LCD_CS(1);
}

/**
  * @brief  设置 LCD 显示窗口（内联优化）
  * @note   设置行列地址后自动进入数据写入模式（0x2C）
  */
static inline void LCD_Set_Window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_Write_Byte(0x2A, LCD_CMD);
    LCD_Write_Byte(x1>>8, LCD_DATA);
    LCD_Write_Byte(x1&0xFF, LCD_DATA);
    LCD_Write_Byte(x2>>8, LCD_DATA);
    LCD_Write_Byte(x2&0xFF, LCD_DATA);

    LCD_Write_Byte(0x2B, LCD_CMD);
    LCD_Write_Byte(y1>>8, LCD_DATA);
    LCD_Write_Byte(y1&0xFF, LCD_DATA);
    LCD_Write_Byte(y2>>8, LCD_DATA);
    LCD_Write_Byte(y2&0xFF, LCD_DATA);

    LCD_Write_Byte(0x2C, LCD_CMD);
}

/* ==================== 批量颜色填充（核心优化） ==================== */

/**
  * @brief  批量填充指定区域颜色（SPI 批量传输优化）
  * @param  x1, y1  左上角坐标
  * @param  x2, y2  右下角坐标
  * @param  color   RGB565 颜色值
  * @note   使用 128 字节缓冲区批量发送，相比逐像素点绘制：
  *         - 减少 SPI 片选切换开销
  *         - 减少函数调用次数
  *         - 128×160 全屏填充约 200ms（逐点约 800ms）
  */
void LCD_Fill_Colors(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint8_t buf[LCD_BATCH_SIZE];
    uint16_t chunk;
    uint32_t total_pixels = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    /* 预填缓冲：每个像素 2 字节（RGB565） */
    for(uint16_t i = 0; i < LCD_BATCH_SIZE; i += 2)
    {
        buf[i]   = color >> 8;
        buf[i+1] = color & 0xFF;
    }

    LCD_Set_Window(x1, y1, x2, y2);
    LCD_CS(0);
    LCD_DC(LCD_DATA);

    while(total_pixels)
    {
        chunk = (total_pixels > (LCD_BATCH_SIZE/2)) ? (LCD_BATCH_SIZE/2) : (uint16_t)total_pixels;
        HAL_SPI_Transmit(&hspi1, buf, chunk * 2, 100);
        total_pixels -= chunk;
    }

    LCD_CS(1);
}

/**
  * @brief  全屏清屏
  */
void LCD_Clear(uint16_t color)
{
    LCD_Fill_Colors(0, 0, LCD_WIDTH-1, LCD_HEIGHT-1, color);
}

/**
  * @brief  矩形区域填充
  */
void LCD_Fill_Rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_Fill_Colors(x1, y1, x2, y2, color);
}

/* ==================== 基础图形绘制 ==================== */

/**
  * @brief  画点（带边界检查）
  */
static inline void LCD_Draw_Point(uint16_t x, uint16_t y, uint16_t color)
{
    if(x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    LCD_Set_Window(x, y, x, y);
    LCD_CS(0);
    LCD_DC(LCD_DATA);
    SPI_Send_Byte(color >> 8);
    SPI_Send_Byte(color & 0xFF);
    LCD_CS(1);
}

/**
  * @brief  水平直线（批量填充优化）
  */
void LCD_Draw_HLine(uint16_t x1, uint16_t y, uint16_t x2, uint16_t color)
{
    if(x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if(y >= LCD_HEIGHT) return;
    if(x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    LCD_Fill_Colors(x1, y, x2, y, color);
}

/**
  * @brief  垂直直线（批量填充优化）
  */
void LCD_Draw_VLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color)
{
    if(y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    if(x >= LCD_WIDTH) return;
    if(y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
    LCD_Fill_Colors(x, y1, x, y2, color);
}

/**
  * @brief  通用直线绘制（Bresenham 算法）
  * @note   水平/垂直线自动转批量填充，斜线使用逐点绘制
  */
void LCD_Draw_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx, dy, step;
    int16_t x, y;
    
    if(x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
    if(x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if(y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    if(y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
    
    if(y1 == y2) { LCD_Draw_HLine(x1, y1, x2, color); return; }
    if(x1 == x2) { LCD_Draw_VLine(x1, y1, y2, color); return; }
    
    dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    
    if(dx >= dy) {
        step = (x2 > x1) ? 1 : -1;
        dy = (y2 > y1) ? 1 : -1;
        x = x1; y = y1;
        int16_t err = dx / 2;
        while(x != x2) {
            LCD_Draw_Point(x, y, color);
            err -= dy;
            if(err < 0) { y += dy; err += dx; }
            x += step;
        }
    } else {
        step = (y2 > y1) ? 1 : -1;
        dx = (x2 > x1) ? 1 : -1;
        x = x1; y = y1;
        int16_t err = dy / 2;
        while(y != y2) {
            LCD_Draw_Point(x, y, color);
            err -= dx;
            if(err < 0) { x += dx; err += dy; }
            y += step;
        }
    }
    LCD_Draw_Point(x2, y2, color);
}

/* ==================== 圆角矩形绘制 ==================== */

/**
  * @brief  圆角矩形填充
  */
void LCD_Draw_RoundRect_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                                 uint16_t radius, uint16_t color)
{
    LCD_Fill_Rect(x+radius, y, x+w-radius-1, y+h-1, color);
    LCD_Fill_Rect(x, y+radius, x+w-1, y+h-radius-1, color);
    
    for(int r=0; r<<radius; r++) {
        int d = radius - r;
        if(y+r < y+h && x+d < x+w) 
            LCD_Draw_HLine(x+d, y+r, x+w-d-1, color);
        if(y+h-1-r > y && x+d < x+w)
            LCD_Draw_HLine(x+d, y+h-1-r, x+w-d-1, color);
    }
}

/**
  * @brief  圆角矩形描边
  */
void LCD_Draw_RoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                        uint16_t radius, uint16_t color)
{
    LCD_Draw_HLine(x+radius, y, x+w-radius-1, color);
    LCD_Draw_HLine(x+radius, y+h-1, x+w-radius-1, color);
    LCD_Draw_VLine(x, y+radius, y+h-radius-1, color);
    LCD_Draw_VLine(x+w-1, y+radius, y+h-radius-1, color);

    for(int r=0; r<<radius; r++){
        LCD_Draw_Point(x+radius-r, y+r, color);
        LCD_Draw_Point(x+w-radius+r-1, y+r, color);
        LCD_Draw_Point(x+radius-r, y+h-r-1, color);
        LCD_Draw_Point(x+w-radius+r-1, y+h-r-1, color);
    }
}

/**
  * @brief  矩形边框
  */
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t i;
    for(i = x1; i <= x2; i++) LCD_Draw_Point(i, y1, color);
    for(i = x1; i <= x2; i++) LCD_Draw_Point(i, y2, color);
    for(i = y1; i <= y2; i++) LCD_Draw_Point(x1, i, color);
    for(i = y1; i <= y2; i++) LCD_Draw_Point(x2, i, color);
}

/* ==================== LCD 初始化 ==================== */

/**
  * @brief  ST7735 初始化序列
  * @note   使用 osDelay 替代 HAL_Delay，兼容 FreeRTOS 调度
  */
void LCD_Init(void)
{
    LCD_RST(0);
    osDelay(100);
    LCD_RST(1);
    osDelay(100);

    LCD_Write_Byte(0x11, LCD_CMD); osDelay(120);
    LCD_Write_Byte(0x36, LCD_CMD); LCD_Write_Byte(0xC0, LCD_DATA);
    LCD_Write_Byte(0x3A, LCD_CMD); LCD_Write_Byte(0x05, LCD_DATA);
    LCD_Write_Byte(0xB2, LCD_CMD);
    LCD_Write_Byte(0x0C, LCD_DATA); LCD_Write_Byte(0x0C, LCD_DATA);
    LCD_Write_Byte(0x00, LCD_DATA); LCD_Write_Byte(0x33, LCD_DATA); LCD_Write_Byte(0x33, LCD_DATA);
    LCD_Write_Byte(0xB7, LCD_CMD); LCD_Write_Byte(0x35, LCD_DATA);
    LCD_Write_Byte(0xBB, LCD_CMD); LCD_Write_Byte(0x19, LCD_DATA);
    LCD_Write_Byte(0xC0, LCD_CMD); LCD_Write_Byte(0x2C, LCD_DATA);
    LCD_Write_Byte(0xC2, LCD_CMD); LCD_Write_Byte(0x01, LCD_DATA);
    LCD_Write_Byte(0xC3, LCD_CMD); LCD_Write_Byte(0x12, LCD_DATA);
    LCD_Write_Byte(0xC4, LCD_CMD); LCD_Write_Byte(0x20, LCD_DATA);
    LCD_Write_Byte(0xC6, LCD_CMD); LCD_Write_Byte(0x0F, LCD_DATA);
    LCD_Write_Byte(0xD0, LCD_CMD); LCD_Write_Byte(0xA4, LCD_DATA); LCD_Write_Byte(0xA1, LCD_DATA);

    uint8_t gamma_pos[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
    for(uint8_t i=0;i<14;i++) LCD_Write_Byte(gamma_pos[i], LCD_DATA);
    uint8_t gamma_neg[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
    for(uint8_t i=0;i<14;i++) LCD_Write_Byte(gamma_neg[i], LCD_DATA);

    LCD_Write_Byte(0x29, LCD_CMD); osDelay(100);
    LCD_Clear(WHITE);
}

/* ==================== 字库显示函数 ==================== */

/**
  * @brief  在字库中查找汉字索引
  * @param  str  GB2312 编码的汉字字符串（2 字节）
  * @param  lib  字库字符串（按顺序排列的汉字）
  * @retval int16_t  索引值，-1=未找到
  */
static int16_t FindChineseIndex(const char *str, const char *lib)
{
    uint8_t gb1 = str[0];
    uint8_t gb2 = str[1];
    uint16_t i = 0;
    
    while(lib[i] != '\0' && lib[i+1] != '\0')
    {
        if((uint8_t)lib[i] == gb1 && (uint8_t)lib[i+1] == gb2)
        {
            return i/2;
        }
        i += 2;
    }
    return -1;
}

/**
  * @brief  按索引显示汉字
  * @param  index  汉字在字库中的索引
  * @param  size   字号：12/16/24
  * @param  color  RGB565 颜色
  */
void LCD_ShowChineseByIndex(uint16_t x, uint16_t y, uint8_t index, uint8_t size, uint16_t color)
{
    uint8_t i, j, k, byte;
    uint16_t byte_cnt = 0;
    const unsigned char *pFont = NULL;
    uint8_t col_bytes;
    
    if(size == 12)
    {
        if(index >= sizeof(chinese_font_12x12)/24) return;
        pFont = chinese_font_12x12[index];
        col_bytes = 2;
    }
    else if(size == 16)
    {
        if(index >= sizeof(chinese_font_16x16)/32) return;
        pFont = chinese_font_16x16[index];
        col_bytes = 2;
    }
    else if(size == 24)
    {
        if(index >= sizeof(chinese_font_24x24)/72) return;
        pFont = chinese_font_24x24[index];
        col_bytes = 3;
    }
    else return;
    
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < col_bytes; j++)
        {
            byte = pFont[byte_cnt++];
            for(k = 0; k < 8; k++)
            {
                if(byte & (0x80 >> k))
                {
                    LCD_Draw_Point(x + i, y + j*8 + k, color);
                }
            }
        }
    }
}

/**
  * @brief  显示 ASCII 字符
  * @param  chr   字符 ASCII 码（' ' 对应 32，已偏移）
  * @param  size  字号枚举
  */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t chr, FontSize size, uint16_t color)
{
    uint8_t i, j, k, byte;
    const uint8_t *pFont = NULL;
    uint8_t w = 0, h = 0;

    chr -= 32;
    if(chr >= 95) return;

    switch(size)
    {
        case FONT_1206: w=6; h=12; pFont = asc2_1206[chr]; break;
        case FONT_1608: w=8; h=16; pFont = asc2_1608[chr]; break;
        case FONT_2412: w=12;h=24; pFont = asc2_2412[chr]; break;
        case FONT_3216: w=16;h=32; pFont = asc2_3216[chr]; break;
        default: return;
    }

    for(i = 0; i < w; i++)
    {
        for(j = 0; j < h/8 + (h%8?1:0); j++)
        {
            byte = *pFont++;
            for(k = 0; k < 8; k++)
            {
                if(byte & (0x80 >> k))
                {
                    if(x+i >= LCD_WIDTH || y+j*8+k >= LCD_HEIGHT) continue;
                    LCD_Draw_Point(x+i, y+j*8+k, color);
                }
            }
        }
    }
}

/**
  * @brief  显示 ASCII 字符串
  */
void LCD_ShowString(uint16_t x, uint16_t y, uint8_t *str, FontSize size, uint16_t color)
{
    uint16_t cur_x = x;
    uint8_t w = 0;

    switch(size)
    {
        case FONT_1206: w=6;  break;
        case FONT_1608: w=8;  break;
        case FONT_2412: w=12; break;
        case FONT_3216: w=16; break;
        default: return;
    }

    while(*str)
    {
        if(*str == '\n') { cur_x = x; y += 16; str++; continue; }
        if(*str == '\r') { str++; continue; }

        LCD_ShowChar(cur_x, y, *str, size, color);
        cur_x += w;
        if(cur_x >= LCD_WIDTH) break;
        str++;
    }
}

/**
  * @brief  显示单个汉字
  * @param  str  GB2312 编码的汉字（2 字节）
  */
void LCD_ShowChineseChar(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint16_t color)
{
    uint8_t i, j, k, byte;
    int16_t index = -1;
    const uint8_t *pFont = NULL;
    uint8_t col_bytes = size / 8;

    if(size == 12)
    {
        index = FindChineseIndex((char*)str, chinese_lib_12x12);
        pFont = chinese_font_12x12[index];
    }
    else if(size == 16)
    {
        index = FindChineseIndex((char*)str, chinese_lib_16x16);
        pFont = chinese_font_16x16[index];
    }
    else if(size == 24)
    {
        index = FindChineseIndex((char*)str, chinese_lib_24x24);
        pFont = chinese_font_24x24[index];
    }
    if(index < 0) return;

    for(i = 0; i < size; i++)
    {
        for(j = 0; j < col_bytes; j++)
        {
            byte = *pFont++;
            for(k = 0; k < 8; k++)
            {
                if(byte & (0x80 >> k))
                {
                    if(x+i >= LCD_WIDTH || y+j*8+k >= LCD_HEIGHT) continue;
                    LCD_Draw_Point(x+i, y+j*8+k, color);
                }
            }
        }
    }
}

/**
  * @brief  显示汉字字符串（自动混合 ASCII）
  */
void LCD_ShowChineseString(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint16_t color)
{
    uint16_t cur_x = x;

    while(*str)
    {
        if(*str >= 0xA1 && *(str+1) >= 0xA1)
        {
            LCD_ShowChineseChar(cur_x, y, str, size, color);
            cur_x += size;
            str += 2;
        }
        else if(*str < 0x80)
        {
            LCD_ShowChar(cur_x, y, *str, (FontSize)size, color);
            cur_x += size/2;
            str++;
        }
        else str++;

        if(cur_x >= LCD_WIDTH) { cur_x = x; y += size; }
        if(y + size > LCD_HEIGHT) break;
    }
}

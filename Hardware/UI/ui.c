#include "ui.h"
#include <string.h>
#include "cmsis_os.h"

// ==================== 配色 ====================
#define UI_BG            0x2D7F
#define UI_CARD          0xFFFF
#define UI_BORDER        0x05FF
#define UI_TEXT_MAIN     0x0000
#define UI_TEXT_SUB      0x5CAF
#define UI_GREEN         0x07E0
#define UI_YELLOW        0xFFE0
#define UI_RED           0xF800

// ==================== 布局 ====================
#define CARD_RADIUS      4

#define TIME_X       6
#define TIME_Y       4
#define TIME_W       116
#define TIME_H       20

#define ENV1_X       6
#define ENV1_Y       30
#define ENV1_W       56
#define ENV1_H       28

#define ENV2_X       66
#define ENV2_Y       30
#define ENV2_W       56
#define ENV2_H       28

#define GAS1_X       6
#define GAS1_Y       62
#define GAS1_W       56
#define GAS1_H       28

#define GAS2_X       66
#define GAS2_Y       62
#define GAS2_W       56
#define GAS2_H       28

#define STATUS_X     6
#define STATUS_Y     94
#define STATUS_W     116
#define STATUS_H     28

#define WIFI_X       6
#define WIFI_Y       126
#define WIFI_W       116
#define WIFI_H       28

// ==================== 内部工具 ====================
static void UI_DrawCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    LCD_Draw_RoundRect_Fill(x, y, w, h, CARD_RADIUS, UI_CARD);
    LCD_Draw_RoundRect(x, y, w, h, CARD_RADIUS, UI_BORDER);
}

static uint16_t UI_GetColor(float val, float warn, float danger)
{
    if(val >= danger) return UI_RED;
    if(val >= warn)   return UI_YELLOW;
    return UI_GREEN;
}

// 轻量级无符号整数转字符串（最大5位）
static char* uitoa(uint16_t val, char* buf)
{
    char *p = buf + 5;
    *p = '\0';
    do {
        *(--p) = '0' + (val % 10);
        val /= 10;
    } while (val);
    return p;
}

// 手动格式化时间 HH:MM:SS
static void format_time(char* buf, const DS1302_TimeTypeDef* time)
{
    buf[0] = '0' + (time->hour / 10);
    buf[1] = '0' + (time->hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (time->minute / 10);
    buf[4] = '0' + (time->minute % 10);
    buf[5] = ':';
    buf[6] = '0' + (time->second / 10);
    buf[7] = '0' + (time->second % 10);
    buf[8] = '\0';
}

// 浮点数取整转字符串（不显示小数，节省栈）
static void format_float_int(char* buf, float val)
{
    int16_t int_part = (int16_t)val;
    char *p = uitoa(int_part, buf);
    // 将结果移到缓冲区开头
    if(p != buf) {
        while(*p) *buf++ = *p++;
        *buf = '\0';
    }
}

// ==================== UI 初始化 ====================
void UI_Init(void)
{
    LCD_Clear(UI_BG);

    UI_DrawCard(TIME_X, TIME_Y, TIME_W, TIME_H);
    LCD_ShowString(10, 8, (uint8_t*)"TIME", FONT_1206, UI_TEXT_SUB);

    UI_DrawCard(ENV1_X, ENV1_Y, ENV1_W, ENV1_H);
    LCD_ShowString(10, 34, (uint8_t*)"CO2", FONT_1206, UI_TEXT_SUB);
    UI_DrawCard(ENV2_X, ENV2_Y, ENV2_W, ENV2_H);
    LCD_ShowString(70, 34, (uint8_t*)"TVOC", FONT_1206, UI_TEXT_SUB);

    UI_DrawCard(GAS1_X, GAS1_Y, GAS1_W, GAS1_H);
    LCD_ShowString(10, 66, (uint8_t*)"TEMP", FONT_1206, UI_TEXT_SUB);
    UI_DrawCard(GAS2_X, GAS2_Y, GAS2_W, GAS2_H);
    LCD_ShowString(70, 66, (uint8_t*)"HUMI", FONT_1206, UI_TEXT_SUB);

    UI_DrawCard(STATUS_X, STATUS_Y, STATUS_W, STATUS_H);
    LCD_ShowString(10, 98, (uint8_t*)"PM2.5  H2S  NH3", FONT_1206, UI_TEXT_SUB);

    UI_DrawCard(WIFI_X, WIFI_Y, WIFI_W, WIFI_H);
    LCD_ShowString(10, 130, (uint8_t*)"SYSTEM STATUS", FONT_1206, UI_TEXT_SUB);
}

// ==================== 全局刷新 ====================
void UI_UpdateAll(float temp, float humi,
                  uint16_t eco2, uint16_t tvoc,
                  float pm25,
                  MQ_Data_t *mq,
                  DS1302_TimeTypeDef *time)
{
    char buf[12];  // 足够容纳整数+单位
    uint16_t color_co2, color_pm25, color_h2s, color_nh3;

    // 1. 时间
    format_time(buf, time);
    LCD_Fill_Rect(50, 6, 120, 22, UI_CARD);
    LCD_ShowString(50, 8, (uint8_t*)buf, FONT_1206, UI_TEXT_MAIN);

    // 2. CO2
    color_co2 = UI_GetColor(eco2, 800, 1200);
    char *p = uitoa(eco2, buf);
    LCD_Fill_Rect(10, 42, 56, 54, UI_CARD);
    LCD_ShowString(10, 44, (uint8_t*)p, FONT_1608, color_co2);

    // 3. TVOC
    p = uitoa(tvoc, buf);
    LCD_Fill_Rect(70, 42, 122, 54, UI_CARD);
    LCD_ShowString(70, 44, (uint8_t*)p, FONT_1608, UI_GREEN);

    // 4. 温度
    format_float_int(buf, temp);
    strcat(buf, "C");
    LCD_Fill_Rect(10, 74, 56, 86, UI_CARD);
    LCD_ShowString(10, 76, (uint8_t*)buf, FONT_1608, UI_TEXT_MAIN);

    // 5. 湿度
    format_float_int(buf, humi);
    strcat(buf, "%");
    LCD_Fill_Rect(70, 74, 122, 86, UI_CARD);
    LCD_ShowString(70, 76, (uint8_t*)buf, FONT_1608, UI_TEXT_MAIN);

    // 6. PM2.5 / H2S / NH3
    color_pm25 = UI_GetColor(pm25, 75, 150);
    color_h2s  = UI_GetColor(mq->h2s_ppm, 5, 10);
    color_nh3  = UI_GetColor(mq->nh3_ppm, 10, 20);

    format_float_int(buf, pm25);
    LCD_Fill_Rect(10, 106, 30, 118, UI_CARD);
    LCD_ShowString(10, 108, (uint8_t*)buf, FONT_1206, color_pm25);

    format_float_int(buf, mq->h2s_ppm);
    LCD_Fill_Rect(40, 106, 60, 118, UI_CARD);
    LCD_ShowString(40, 108, (uint8_t*)buf, FONT_1206, color_h2s);

    format_float_int(buf, mq->nh3_ppm);
    LCD_Fill_Rect(70, 106, 116, 118, UI_CARD);
    LCD_ShowString(70, 108, (uint8_t*)buf, FONT_1206, color_nh3);

    // 7. 系统状态
    LCD_Fill_Rect(70, 130, 122, 150, UI_CARD);
    if(color_pm25 == UI_RED || color_h2s == UI_RED || color_nh3 == UI_RED)
        LCD_ShowString(70, 132, (uint8_t*)"ALARM", FONT_1608, UI_RED);
    else
        LCD_ShowString(70, 132, (uint8_t*)"NORMAL", FONT_1608, UI_GREEN);
}

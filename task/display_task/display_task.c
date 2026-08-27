#include "display_task.h"
#include "system_data.h"
#include "lcd.h"         // ST7735 1.8寸 TFT LCD 驱动（SPI 接口）
#include "ds1302.h"      // RTC 时间读取（用于显示）
#include "stdio.h"
#include "mq_senser.h"
#include <string.h>

/* ==================== 全局变量定义 ==================== */
/* 当前显示页面索引，由 system_task.c 中的按键处理修改 */
volatile uint8_t g_current_page = 0;

/* 设置页编辑状态：0=浏览模式，1~6=编辑第几项阈值 */
volatile uint8_t g_setting_idx = 0;

/* 报警阈值默认值，首次上电或 DS1302 RAM 损坏时使用 */
AlarmThreshold_t g_alarm_thresh = {
    .pm25_warn = 75.0f,   .pm25_danger = 150.0f,
    .h2s_warn  = 5.0f,    .h2s_danger  = 10.0f,
    .nh3_warn  = 10.0f,   .nh3_danger  = 20.0f
};

/* ==================== 配色方案（16位 RGB565） ==================== */
#define CLR_BG          0x18C3   /* 深蓝灰，主背景色 */
#define CLR_TOPBAR      0x04FF   /* 亮蓝，顶部标题栏 */
#define CLR_CARD        0xFFFF   /* 纯白，卡片背景 */
#define CLR_CARD_BORDER 0xC618   /* 浅灰，卡片边框 */
#define CLR_TEXT_MAIN   0x0000   /* 黑色，主文字 */
#define CLR_TEXT_SUB    0x5CAF   /* 灰蓝，次要文字 */
#define CLR_GREEN       0x07E0   /* 绿色，正常状态 */
#define CLR_YELLOW      0xFFE0   /* 黄色，警告状态 */
#define CLR_RED         0xF800   /* 红色，危险状态 */
#define CLR_TEXT_G      0x8410   /* 深灰，备用文字色 */

/* WiFi 连接状态，用于系统状态页显示 */
extern volatile uint8_t wifi_ok;

/* ==================== 工具函数 ==================== */

/**
  * @brief  无符号整数转字符串（从 buf[0] 开始写入，避免首地址偏移）
  * @param  val  待转换的 16 位无符号整数
  * @param  buf  输出缓冲区，至少 6 字节
  * @retval char*  返回 buf 首地址，方便链式调用
  */
static char* uitoa(uint16_t val, char* buf)
{
    uint16_t v = val;
    uint8_t digits = 0;
    do { digits++; v /= 10; } while(v);
    
    buf[digits] = '\0';
    while(digits--) {
        buf[digits] = '0' + (val % 10);
        val /= 10;
    }
    return buf;
}

/**
  * @brief  浮点数格式化：保留 1 位小数，范围 0~999.9
  * @param  buf  输出缓冲区，至少 8 字节
  * @param  val  待格式化的浮点数
  */
static void fmt_float_1dec(char* buf, float val)
{
    int16_t intp = (int16_t)val;
    uint16_t frac = (uint16_t)((val - intp) * 10.0f);
    if(frac > 9) frac = 9;
    if(intp < 0) intp = 0;
    char *p = buf;
    if(intp >= 100) { *p++ = '0' + intp/100; intp %= 100; }
    *p++ = '0' + intp/10;
    *p++ = '0' + intp%10;
    *p++ = '.'; *p++ = '0' + frac; *p = '\0';
}

/**
  * @brief  根据数值与阈值返回状态颜色
  * @param  val    当前数值
  * @param  warn   警告阈值
  * @param  danger 危险阈值
  * @retval uint16_t  RGB565 颜色码
  */
static uint16_t UI_GetColor(float val, float warn, float danger)
{
    if(val >= danger) return CLR_RED;
    if(val >= warn)   return CLR_YELLOW;
    return CLR_GREEN;
}

/**
  * @brief  绘制圆角矩形卡片（填充 + 边框）
  * @param  x, y, w, h  位置和尺寸
  */
static void UI_DrawCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    LCD_Draw_RoundRect_Fill(x, y, w, h, 3, CLR_CARD);
    LCD_Draw_RoundRect(x, y, w, h, 3, CLR_CARD_BORDER);
}

/* ==================== 顶部栏绘制 ==================== */
static void draw_topbar(const char* title, const DS1302_TimeTypeDef* time, uint8_t page)
{
    char buf[12];
    LCD_Fill_Rect(0, 0, LCD_WIDTH-1, 15, CLR_TOPBAR);
    LCD_ShowString(4, 2, (uint8_t*)title, FONT_1206, CLR_CARD);
    
    /* 时分显示（HH:MM） */
    buf[0] = '0' + (time->hour / 10); buf[1] = '0' + (time->hour % 10);
    buf[2] = ':'; buf[3] = '0' + (time->minute / 10); buf[4] = '0' + (time->minute % 10);
    buf[5] = '\0';
    LCD_ShowString(70, 2, (uint8_t*)buf, FONT_1206, CLR_CARD);
    
    /* 页面指示点（4 页对应 4 个点） */
    for(uint8_t i = 0; i < 4; i++) {
        uint16_t c = (i == page) ? 0xFFFF : 0x4208;
        LCD_Fill_Rect(108 + i*5, 5, 111 + i*5, 8, c);
    }
}

/* ==================== Page 0: 主监控页 ==================== */
static void draw_page_main(float temp, float humi, uint16_t eco2, uint16_t tvoc,
                           float pm25, DS1302_TimeTypeDef* time)
{
    char buf[16];
    /* 根据阈值计算各参数状态颜色 */
    uint16_t c_temp  = UI_GetColor(temp, 35.0f, 45.0f);
    uint16_t c_humi  = UI_GetColor(humi, 80.0f, 95.0f);
    uint16_t c_co2   = UI_GetColor((float)eco2, 800.0f, 1200.0f);
    uint16_t c_tvoc  = UI_GetColor((float)tvoc, 200.0f, 500.0f);
    uint16_t c_pm25  = UI_GetColor(pm25, g_alarm_thresh.pm25_warn, g_alarm_thresh.pm25_danger);
    
    /* TEMP 卡片（左上） */
    UI_DrawCard(4, 18, 58, 48);
    LCD_ShowString(8, 22, (uint8_t*)"TEMP", FONT_1206, CLR_TEXT_SUB);
    fmt_float_1dec(buf, temp);
    LCD_ShowString(8, 36, (uint8_t*)buf, FONT_2412, c_temp);
    LCD_ShowString(8, 60, (uint8_t*)"C", FONT_1206, CLR_TEXT_SUB);
    
    /* HUMI 卡片（右上） */
    UI_DrawCard(66, 18, 58, 48);
    LCD_ShowString(70, 22, (uint8_t*)"HUMI", FONT_1206, CLR_TEXT_SUB);
    fmt_float_1dec(buf, humi);
    LCD_ShowString(70, 36, (uint8_t*)buf, FONT_2412, c_humi);
    LCD_ShowString(70, 60, (uint8_t*)"%", FONT_1206, CLR_TEXT_SUB);
    
    /* CO2 卡片（左中） */
    UI_DrawCard(4, 70, 58, 34);
    LCD_ShowString(8, 74, (uint8_t*)"CO2", FONT_1206, CLR_TEXT_SUB);
    uitoa(eco2, buf);
    LCD_ShowString(8, 86, (uint8_t*)buf, FONT_1608, c_co2);
    LCD_ShowString(8, 102, (uint8_t*)"ppm", FONT_1206, CLR_TEXT_SUB);
    
    /* TVOC 卡片（右中） */
    UI_DrawCard(66, 70, 58, 34);
    LCD_ShowString(70, 74, (uint8_t*)"TVOC", FONT_1206, CLR_TEXT_SUB);
    uitoa(tvoc, buf);
    LCD_ShowString(70, 86, (uint8_t*)buf, FONT_1608, c_tvoc);
    LCD_ShowString(70, 102, (uint8_t*)"ppb", FONT_1206, CLR_TEXT_SUB);
    
    /* PM2.5 卡片（左下） */
    UI_DrawCard(4, 108, 58, 34);
    LCD_ShowString(8, 112, (uint8_t*)"PM2.5", FONT_1206, CLR_TEXT_SUB);
    fmt_float_1dec(buf, pm25);
    LCD_ShowString(8, 124, (uint8_t*)buf, FONT_1608, c_pm25);
    LCD_ShowString(8, 140, (uint8_t*)"ug", FONT_1206, CLR_TEXT_SUB);
    
    /* DATE 卡片（右下） */
    UI_DrawCard(66, 108, 58, 34);
    LCD_ShowString(70, 112, (uint8_t*)"DATE", FONT_1206, CLR_TEXT_SUB);
    buf[0] = '0' + (time->month / 10); buf[1] = '0' + (time->month % 10);
    buf[2] = '-'; buf[3] = '0' + (time->date / 10); buf[4] = '0' + (time->date % 10);
    buf[5] = '\0';
    LCD_ShowString(70, 124, (uint8_t*)buf, FONT_1608, CLR_TEXT_MAIN);
    LCD_ShowString(70, 140, (uint8_t*)"2026", FONT_1206, CLR_TEXT_SUB);
    
    /* 底部提示栏 */
    LCD_Fill_Rect(0, 146, LCD_WIDTH-1, LCD_HEIGHT-1, CLR_BG);
    LCD_ShowString(4, 148, (uint8_t*)"KEY1:Switch KEY3:Long=Set", FONT_1206, CLR_TEXT_SUB);
}

/* ==================== Page 1: 气体详情页 ==================== */
static void draw_page_gas(uint16_t eco2, uint16_t tvoc, float pm25,
                          MQ_Data_t* mq, DS1302_TimeTypeDef* time)
{
    char buf[16];
    uint16_t c_h2s = UI_GetColor(mq->h2s_ppm, g_alarm_thresh.h2s_warn, g_alarm_thresh.h2s_danger);
    uint16_t c_nh3 = UI_GetColor(mq->nh3_ppm, g_alarm_thresh.nh3_warn, g_alarm_thresh.nh3_danger);
    uint16_t c_co2 = UI_GetColor((float)eco2, 800.0f, 1200.0f);
    uint16_t c_tvoc= UI_GetColor((float)tvoc, 200.0f, 500.0f);
    uint16_t c_pm  = UI_GetColor(pm25, g_alarm_thresh.pm25_warn, g_alarm_thresh.pm25_danger);
    
    /* H2S 大卡片 */
    UI_DrawCard(4, 18, 120, 38);
    LCD_ShowString(8, 22, (uint8_t*)"H2S", FONT_1608, CLR_TEXT_MAIN);
    fmt_float_1dec(buf, mq->h2s_ppm);
    LCD_ShowString(8, 40, (uint8_t*)buf, FONT_2412, c_h2s);
    LCD_ShowString(90, 44, (uint8_t*)"ppm", FONT_1206, CLR_TEXT_SUB);
    
    /* NH3 大卡片 */
    UI_DrawCard(4, 60, 120, 38);
    LCD_ShowString(8, 64, (uint8_t*)"NH3", FONT_1608, CLR_TEXT_MAIN);
    fmt_float_1dec(buf, mq->nh3_ppm);
    LCD_ShowString(8, 82, (uint8_t*)buf, FONT_2412, c_nh3);
    LCD_ShowString(90, 86, (uint8_t*)"ppm", FONT_1206, CLR_TEXT_SUB);
    
    /* CO2 / TVOC 小卡片 */
    UI_DrawCard(4, 102, 58, 34);
    LCD_ShowString(8, 106, (uint8_t*)"CO2", FONT_1206, CLR_TEXT_SUB);
    uitoa(eco2, buf);
    LCD_ShowString(8, 118, (uint8_t*)buf, FONT_1608, c_co2);
    
    UI_DrawCard(66, 102, 58, 34);
    LCD_ShowString(70, 106, (uint8_t*)"TVOC", FONT_1206, CLR_TEXT_SUB);
    uitoa(tvoc, buf);
    LCD_ShowString(70, 118, (uint8_t*)buf, FONT_1608, c_tvoc);
    
    /* PM2.5 底部状态条 */
    LCD_Fill_Rect(0, 140, LCD_WIDTH-1, LCD_HEIGHT-1, c_pm);
    LCD_ShowString(4, 144, (uint8_t*)"PM2.5:", FONT_1206, (c_pm==CLR_RED)?CLR_CARD:CLR_TEXT_MAIN);
    fmt_float_1dec(buf, pm25);
    LCD_ShowString(50, 144, (uint8_t*)buf, FONT_1608, (c_pm==CLR_RED)?CLR_CARD:CLR_TEXT_MAIN);
    LCD_ShowString(90, 144, (uint8_t*)"ug/m3", FONT_1206, (c_pm==CLR_RED)?CLR_CARD:CLR_TEXT_SUB);
}

/* ==================== Page 2: 系统状态页 ==================== */
static void draw_page_system(DS1302_TimeTypeDef* time)
{
    uint32_t tick = xTaskGetTickCount() / 1000;
    uint32_t sec = tick % 60;
    uint32_t min = (tick / 60) % 60;
    uint32_t hour = tick / 3600;
    char buf[16];
    
    /* 系统状态项：名称 + 状态 + 颜色 */
	struct { const char* name; const char* status; uint16_t color; } items[] = {
    {"WiFi",  wifi_ok ? "ONLINE " : "OFFLINE", wifi_ok ? CLR_GREEN : CLR_RED},
    {"MQTT",  wifi_ok ? "CONN   " : "DISCONN", wifi_ok ? CLR_GREEN : CLR_RED},
    {"SHT30", "OK     ", CLR_GREEN},   /* 温湿度 */
    {"SGP30", "OK     ", CLR_GREEN},   /* eCO2/TVOC */
    {"MQ",    "OK     ", CLR_GREEN},   /* MQ-136 + MQ-137 气体传感器 */
    {"PM2.5", "OK     ", CLR_GREEN},   /* 粉尘 */
};

    
    for(uint8_t i = 0; i < 6; i++) {
        uint16_t y = 18 + i * 18;
        UI_DrawCard(4, y, 120, 14);
        LCD_Fill_Rect(5, y+1, 123, y+12, CLR_CARD);
        
        LCD_ShowString(8, y+2, (uint8_t*)items[i].name, FONT_1206, CLR_TEXT_MAIN);
        LCD_Fill_Rect(90, y+2, 122, y+12, items[i].color);
        LCD_ShowString(94, y+3, (uint8_t*)items[i].status, FONT_1206, 
                       (items[i].color==CLR_RED)?CLR_CARD:CLR_TEXT_MAIN);
    }
    
    /* 系统运行时间 */
    LCD_Fill_Rect(4, 130, LCD_WIDTH-5, 146, CLR_BG);
    LCD_ShowString(8, 132, (uint8_t*)"UPTIME:", FONT_1206, CLR_TEXT_SUB);
    buf[0] = '0' + (hour / 10); buf[1] = '0' + (hour % 10); buf[2] = ':';
    buf[3] = '0' + (min / 10); buf[4] = '0' + (min % 10); buf[5] = ':';
    buf[6] = '0' + (sec / 10); buf[7] = '0' + (sec % 10); buf[8] = '\0';
    LCD_ShowString(70, 132, (uint8_t*)buf, FONT_1608, CLR_TEXT_MAIN);
    
    /* 底部提示 */
    LCD_Fill_Rect(0, 150, LCD_WIDTH-1, LCD_HEIGHT-1, CLR_BG);
    LCD_ShowString(4, 152, (uint8_t*)"KEY3 Long: Settings", FONT_1206, CLR_TEXT_SUB);
}

/* ==================== Page 3: 阈值设置页 ==================== */
static void draw_page_settings(void)
{
    const char* names[] = {"PM25.W", "PM25.D", "H2S.W ", "H2S.D ", "NH3.W ", "NH3.D "};
    float* vals[] = {
        &g_alarm_thresh.pm25_warn, &g_alarm_thresh.pm25_danger,
        &g_alarm_thresh.h2s_warn,  &g_alarm_thresh.h2s_danger,
        &g_alarm_thresh.nh3_warn,  &g_alarm_thresh.nh3_danger
    };
    char buf[16];
    
    for(uint8_t i = 0; i < 6; i++) {
        uint16_t y = 18 + i * 20;
        /* 当前编辑项高亮显示 */
        uint16_t bg = (g_setting_idx == i+1) ? 0x4208 : CLR_CARD;
        
        LCD_Fill_Rect(4, y, LCD_WIDTH-5, y+16, bg);
        LCD_ShowString(8, y+2, (uint8_t*)names[i], FONT_1206, CLR_TEXT_MAIN);
        
        /* PM2.5 用整数显示，气体用 1 位小数 */
        if(i == 0 || i == 1) {
            uitoa((uint16_t)*vals[i], buf);
        } else {
            fmt_float_1dec(buf, *vals[i]);
        }
        LCD_ShowString(70, y+2, (uint8_t*)buf, FONT_1608, 
                       (g_setting_idx == i+1) ? CLR_RED : CLR_TEXT_MAIN);
    }
    
    /* 底部操作提示 */
    LCD_Fill_Rect(0, 140, LCD_WIDTH-1, LCD_HEIGHT-1, CLR_BG);
    if(g_setting_idx == 0) {
        LCD_ShowString(4, 144, (uint8_t*)"KEY3:Enter Edit", FONT_1206, CLR_TEXT_SUB);
    } else {
        LCD_ShowString(4, 144, (uint8_t*)"1:+ 2:- 3:Next 4:Save", FONT_1206, CLR_TEXT_SUB);
    }
}

/**
  * @brief  从 DS1302 RAM 恢复报警阈值
  * @param  None
  * @retval None
  * @note   读取 DS1302 内置 RAM，校验通过后反序列化阈值
  *         校验失败或数据异常时保持默认阈值不变
  */
static void Threshold_LoadFromRAM(void)
{
    uint8_t buf[DS1302_RAM_SIZE];
    uint8_t checksum = 0;
    float *fptr;
    uint8_t i;
    
    DS1302_ReadRAM(buf);
    
    /* 计算校验和（前 30 字节异或） */
    for(i = 0; i < 30; i++) checksum ^= buf[i];
    
    /* 校验和匹配且数据非空（0xFF 为默认空值）时恢复 */
    if(checksum == buf[30] && buf[0] != 0xFF)
    {
        /* 反序列化 6 个 float（24 字节） */
        fptr = (float*)&buf[0];
        g_alarm_thresh.pm25_warn  = fptr[0];
        g_alarm_thresh.pm25_danger= fptr[1];
        g_alarm_thresh.h2s_warn   = fptr[2];
        g_alarm_thresh.h2s_danger = fptr[3];
        g_alarm_thresh.nh3_warn   = fptr[4];
        g_alarm_thresh.nh3_danger = fptr[5];
        
        printf("[THRESH] Loaded from DS1302 RAM\r\n");
    }
    else
    {
        printf("[THRESH] RAM empty/corrupt, use default\r\n");
    }
}

/* ==================== FreeRTOS 任务入口 ==================== */
void StartLCD_Task(void const * argument)
{
    SensorData_t data;
    MQ_Data_t mq;
    DS1302_TimeTypeDef time;
    uint8_t last_page = 0xFF;   /* 0xFF 确保首次进入必定清屏 */

    LCD_Init();
    LCD_Clear(CLR_BG);
	
    /* 从 DS1302 RAM 恢复阈值（只执行一次，掉电保存） */
    Threshold_LoadFromRAM();

    for(;;)
    {
        if(SystemData_Read(&data))
        {
            /* 从全局数据结构解包到本地显示变量 */
            mq.h2s_ppm = data.h2s_ppm;
            mq.nh3_ppm = data.nh3_ppm;
            time.year   = data.year + 2000;
            time.month  = data.mon;
            time.date   = data.day;
            time.hour   = data.hour;
            time.minute = data.min;
            time.second = data.sec;

            /* 获取 LCD 总线访问权（超时 100ms，避免阻塞传感器任务） */
            if(LCD_Lock(100))
            {
                /* 页面切换时全屏刷新，避免残影 */
                if(g_current_page != last_page) {
                    LCD_Clear(CLR_BG);
                    last_page = g_current_page;
                }
                
                /* 绘制顶部公共栏：标题 + 时间 + 页面指示点 */
                const char* titles[] = {"MAIN", "GAS", "SYS", "SET"};
                draw_topbar(titles[g_current_page], &time, g_current_page);
                
                /* 根据当前页面分发绘制函数 */
                switch(g_current_page)
                {
                    case PAGE_MAIN:
                        draw_page_main(data.temp, data.humi, data.eco2, data.tvoc,
                                       data.pm25, &time);
                        break;
                    case PAGE_GAS:
                        draw_page_gas(data.eco2, data.tvoc, data.pm25, &mq, &time);
                        break;
                    case PAGE_SYSTEM:
                        draw_page_system(&time);
                        break;
                    case PAGE_SETTINGS:
                        draw_page_settings();
                        break;
                    default:
                        g_current_page = 0;
                        break;
                }
                LCD_Unlock();
            }
        }
        /* 300ms 刷新周期，兼顾显示流畅度与 CPU 占用 */
        osDelay(300);
    }
}

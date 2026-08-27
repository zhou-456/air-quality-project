#include "system_task.h"
#include "system_data.h"
#include "alarm.h"       // 蜂鸣器与 LED 报警驱动
#include "key.h"         // 矩阵/独立按键扫描驱动
#include "led.h"         // LED 指示灯驱动
#include "display_task.h"// 显示页面状态（g_current_page, g_setting_idx）
#include "ds1302.h"      // DS1302 RAM 读写（阈值掉电保存）
#include <stdio.h>

/* MQTT 连接状态标志（预留扩展：联网联动报警，当前未使用） */
extern volatile uint8_t wifi_ok;

/**
  * @brief  系统控制任务：按键处理 + 阈值编辑 + 报警判断
  * @param  argument  FreeRTOS 任务参数（未使用）
  * @retval None
  * @note   任务职责：
  *         1. 按键扫描与页面切换（KEY1：翻页，KEY3 长按：进入设置）
  *         2. 报警阈值编辑模式（KEY1/KEY2：±1，KEY3：下一项，KEY4：保存）
  *         3. 实时报警判断（500ms 周期，动态阈值比较）
  *         
  *         阈值存储方案：利用 DS1302 内置 31 字节 RAM
  *         - 掉电后由备用电池供电，数据不丢失
  *         - 6 个 float（24 字节）+ 校验和（1 字节），共 25 字节
  */
void StartSystem_Task(void const * argument)
{
    Alarm_Init();
    SensorData_t data;
    uint32_t last_alarm = 0;

    for(;;)
    {
        uint8_t key = KEY_Scan();

        if(key != KEY_NONE)
        {
            printf("KEY:%d\r\n", key);

            /* ===== 设置页编辑模式：阈值参数调整 ===== */
            if(g_current_page == PAGE_SETTINGS && g_setting_idx != 0)
            {
                /* 阈值参数指针数组，与 g_setting_idx 1~6 对应 */
                float* vals[] = {
                    &g_alarm_thresh.pm25_warn,  /* idx=0: PM2.5 警告阈值 */
                    &g_alarm_thresh.pm25_danger,/* idx=1: PM2.5 危险阈值 */
                    &g_alarm_thresh.h2s_warn,   /* idx=2: H?S 警告阈值 */
                    &g_alarm_thresh.h2s_danger, /* idx=3: H?S 危险阈值 */
                    &g_alarm_thresh.nh3_warn,   /* idx=4: NH? 警告阈值 */
                    &g_alarm_thresh.nh3_danger  /* idx=5: NH? 危险阈值 */
                };
                uint8_t idx = g_setting_idx - 1;

                switch(key)
                {
                    case KEY1:   /* 数值 +1 */
                        *vals[idx] += 1.0f;
                        if(*vals[idx] > 999) *vals[idx] = 999;
                        break;
                        
                    case KEY2:   /* 数值 -1 */
                        *vals[idx] -= 1.0f;
                        if(*vals[idx] < 0) *vals[idx] = 0;
                        break;
                        
                    case KEY3:   /* 切换到下一项阈值 */
                        g_setting_idx++;
                        if(g_setting_idx > 6) g_setting_idx = 1;
                        break;
                        
                    case KEY4:   /* 保存阈值并退出编辑模式 */
                    {
                        /* 将 6 个 float 阈值序列化到缓冲区 */
                        uint8_t buf[DS1302_RAM_SIZE] = {0};
                        uint8_t i;
                        uint8_t checksum = 0;
                        float *fptr = (float*)&buf[0];
                        
                        fptr[0] = g_alarm_thresh.pm25_warn;
                        fptr[1] = g_alarm_thresh.pm25_danger;
                        fptr[2] = g_alarm_thresh.h2s_warn;
                        fptr[3] = g_alarm_thresh.h2s_danger;
                        fptr[4] = g_alarm_thresh.nh3_warn;
                        fptr[5] = g_alarm_thresh.nh3_danger;
                        
                        /* 计算 XOR 校验和，防止 RAM 数据损坏导致阈值异常 */
                        for(i = 0; i < 30; i++) checksum ^= buf[i];
                        buf[30] = checksum;
                        
                        /* 写入 DS1302 备用电池供电的 RAM，实现掉电保存 */
                        DS1302_WriteRAM(buf);
                        printf("[THRESH] Saved to DS1302 RAM\r\n");
                        
                        g_setting_idx = 0;  /* 退出编辑模式 */
                        break;
                    }
                    
                    default:
                        break;
                }
                /* 编辑模式下屏蔽其他按键功能，防止误操作 */
            }
            /* ===== 正常模式：页面切换与功能按键 ===== */
            else
            {
                switch(key)
                {
                    case KEY1:   /* 显示页面循环切换 */
                        g_current_page++;
                        if(g_current_page >= PAGE_MAX) g_current_page = 0;
                        printf("Page:%d\r\n", g_current_page);
                        break;
                        
                    case KEY2:   /* LED2 状态翻转（测试/指示用） */
                        HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
                        break;
                        
                    case KEY3:   /* 蜂鸣器短鸣测试 */
                        Beep_On();
                        osDelay(200);
                        Beep_Off();
                        break;
                        
                    case KEY4:   /* 预留功能键 */
                        break;
                        
                    default:
                        break;
                }
            }

            /* ===== KEY3 长按：从任意页面进入阈值设置 ===== */
            if(key == KEY3_LONG)
            {
                g_current_page = PAGE_SETTINGS;
                g_setting_idx = 1;  /* 默认选中第一项阈值 */
                printf("Enter settings\r\n");
            }
        }

        /* ===== 周期性报警判断（500ms）===== */
        if(osKernelSysTick() - last_alarm >= 500)
        {
            last_alarm = osKernelSysTick();
            
            if(SystemData_Read(&data))
            {
                /* 危险阈值比较：任一参数超标即触发声光报警 */
                if(data.pm25 > g_alarm_thresh.pm25_danger || 
                   data.h2s_ppm > g_alarm_thresh.h2s_danger || 
                   data.nh3_ppm > g_alarm_thresh.nh3_danger)
                {
                    Beep_On();    /* 蜂鸣器鸣响 */
                    Alarm_On();   /* 报警 LED 闪烁 */
                }
                else
                {
                    Beep_Off();
                    Alarm_Off();
                }
            }
        }

        /* 20ms 轮询周期，兼顾按键响应速度与 CPU 占用 */
        osDelay(20);
    }
}

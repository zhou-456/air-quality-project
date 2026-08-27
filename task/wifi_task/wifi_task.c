#include "wifi_task.h"
#include "system_data.h"
#include "esp8266.h"     // ESP8266 AT 指令驱动（MQTT 协议封装）
#include "cmsis_os.h"

/* WiFi 连接状态标志，0=未连接，1=已连接（MQTT 层面） */
extern volatile uint8_t wifi_ok;

/**
  * @brief  WiFi 通信任务：ESP8266 联网 + MQTT 数据上报
  * @param  argument  FreeRTOS 任务参数（未使用）
  * @retval None
  * @note   任务职责：
  *         1. ESP8266 模块初始化（AT 指令握手、STA 模式配置）
  *         2. MQTT 服务器连接（OneNET / 阿里云等 IoT 平台）
  *         3. 周期性传感器数据上报（10 秒间隔）
  *         
  *         状态机设计：
  *         ├─ wifi_connected=0 → 初始化模块，指数退避重试
  *         ├─ wifi_ok=0      → 建立 MQTT 连接
  *         └─ wifi_ok=1      → 定时上报数据
  *         
  *         串口保护：所有 AT 指令交互通过 esp8266MutexHandle 互斥锁串行化
  */
void StartWiFi_Task(void const * argument)
{
    /* 等待系统稳定，避免与其他任务竞争初始化资源 */
    osDelay(3000);

    uint8_t wifi_connected = 0;   /* ESP8266 模块初始化状态 */
    uint32_t last_report = 0;      /* 上次数据上报时间戳 */
    uint32_t retry_delay = 1000;   /* 重试间隔，指数退避 */
    SensorData_t data;

    for(;;)
    {
        /* ===== 阶段 1：ESP8266 模块初始化 ===== */
        if(!wifi_connected)
        {
            if(ESP8266_Init() == 0)
            {
                /* 初始化成功，重置退避间隔 */
                wifi_connected = 1;
                retry_delay = 1000;
            }
            else
            {
                /* 初始化失败，指数退避重试（1s → 2s → 4s ... 上限 30s） */
                osDelay(retry_delay);
                retry_delay <<= 1;
                if(retry_delay > 30000) retry_delay = 30000;
                continue;
            }
        }

        /* ===== 阶段 2：MQTT 服务器连接 ===== */
        if(wifi_ok == 0)
        {
            /* 获取串口访问权，超时 10 秒（MQTT 握手可能耗时较长） */
            if(ESP_Lock(10000))
            {
                uint8_t ret = ESP8266_MQTT_Connect();
                ESP_Unlock();
                
                if(ret == 0)
                {
                    /* MQTT 连接成功，标记状态并记录时间戳 */
                    wifi_ok = 1;
                    last_report = osKernelSysTick();
                }
                else
                {
                    /* 连接失败，3 秒后重试 */
                    osDelay(3000);
                    continue;
                }
            }
        }

        /* ===== 阶段 3：周期性数据上报（10 秒间隔）===== */
        if(osKernelSysTick() - last_report >= 10000)
        {
            last_report = osKernelSysTick();
            
            /* 读取最新传感器数据，获取串口发送权 */
            if(SystemData_Read(&data) && ESP_Lock(5000))
            {
                /* 打包 7 个参数通过 MQTT 发布到云端 */
                ESP8266_MQTT_Publish(data.temp, data.humi, data.eco2, data.tvoc,
                                     data.pm25, data.h2s_ppm, data.nh3_ppm);
                ESP_Unlock();
            }
        }

        /* 100ms 轮询，平衡响应速度与 CPU 占用 */
        osDelay(100);
    }
}

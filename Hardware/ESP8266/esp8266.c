#include "esp8266.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "cmsis_os.h"

/* 静态大缓冲区，避免任务栈溢出 */
static char esp_cmd_buf[512];   /* AT 指令组装缓冲区 */
static char esp_json_buf[384];  /* MQTT JSON 报文组装缓冲区 */

/* 串口接收环形缓冲区（中断写入，任务读取） */
unsigned char esp8266_buf[ESP8266_BUF_SIZE];
volatile uint32_t esp8266_cnt = 0;      /* 当前写入位置 */
volatile uint32_t esp8266_cntPre = 0;   /* 上次读取位置，用于判断接收完成 */

/* USART2 句柄（main.c 中初始化） */
extern UART_HandleTypeDef huart2;

/* MQTT 连接状态机 */
volatile MQTT_State_t mqtt_state = MQTT_DISCONNECTED;

/**
  * @brief  清空串口接收缓冲区
  * @note   进入临界区保护，防止与串口中断竞态
  */
void ESP8266_Clear(void)
{
    taskENTER_CRITICAL();
    memset(esp8266_buf, 0, sizeof(esp8266_buf));
    esp8266_cnt = 0;
    esp8266_cntPre = 0;
    taskEXIT_CRITICAL();
}

/**
  * @brief  等待串口数据接收完成
  * @retval REV_OK=接收完成，REV_WAIT=正在接收
  * @note   通过比较两次读取的计数器判断数据流是否停止
  */
_Bool ESP8266_WaitRecive(void)
{
    taskENTER_CRITICAL();
    if(esp8266_cnt == 0)
    {
        taskEXIT_CRITICAL();
        return REV_WAIT;
    }
    if(esp8266_cnt == esp8266_cntPre)
    {
        esp8266_cnt = 0;
        taskEXIT_CRITICAL();
        return REV_OK;
    }
    esp8266_cntPre = esp8266_cnt;
    taskEXIT_CRITICAL();
    return REV_WAIT;
}

/**
  * @brief  等待指定响应字符串
  * @param  str      目标响应字符串
  * @param  timeout_ms  超时时间
  * @retval 1=成功，0=超时或收到 ERROR/FAIL
  * @note   超时不清除缓冲区，允许调用者检查残留数据
  */
_Bool ESP8266_WaitForStr(char *str, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while((HAL_GetTick() - start) < timeout_ms)
    {
        if(ESP8266_WaitRecive() == REV_OK)
        {
            if(strstr((char*)esp8266_buf, str) != NULL)
            {
                ESP8266_Clear();
                return 1;
            }
            if(strstr((char*)esp8266_buf, "ERROR") != NULL || 
               strstr((char*)esp8266_buf, "FAIL") != NULL)
            {
                printf("[ESP] Detected ERROR/FAIL in response\r\n");
                ESP8266_Clear();
                return 0;
            }
        }
        osDelay(5);
    }
    return 0;
}

/**
  * @brief  发送 AT 指令并等待响应
  * @param  cmd      AT 指令字符串
  * @param  resp     期望响应字符串
  * @param  timeout_ms  超时时间
  * @retval 1=成功，0=失败
  */
_Bool ESP8266_SendCmdWait(char *cmd, char *resp, uint32_t timeout_ms)
{
    ESP8266_Clear();
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 2000);
    return ESP8266_WaitForStr(resp, timeout_ms);
}

/**
  * @brief  发送 AT 指令并等待 OK（默认 5 秒超时）
  */
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
    return ESP8266_SendCmdWait(cmd, res, 5000);
}

/**
  * @brief  软件复位 ESP8266 模块
  */
void ESP8266_Reset(void)
{
    printf("[ESP] Resetting module...\r\n");
    ESP8266_SendCmdWait("AT+RST\r\n", "ready", 8000);
    osDelay(2000);
    ESP8266_Clear();
}

/**
  * @brief  检查 WiFi 连接状态（获取 STA IP）
  * @retval 1=已连接（获得有效 IP），0=未连接
  */
_Bool ESP8266_CheckWiFiStatus(void)
{
    uint32_t start = HAL_GetTick();
    ESP8266_Clear();
    HAL_UART_Transmit(&huart2, (uint8_t*)"AT+CIPSTA?\r\n", 12, 1000);

    while((HAL_GetTick() - start) < 3000)
    {
        if(ESP8266_WaitRecive() == REV_OK)
        {
            if(strstr((char*)esp8266_buf, "OK") != NULL)
            {
                if(strstr((char*)esp8266_buf, "ip") != NULL && 
                   strstr((char*)esp8266_buf, "0.0.0.0") == NULL)
                {
                    ESP8266_Clear();
                    return 1;
                }
                ESP8266_Clear();
                return 0;
            }
            if(strstr((char*)esp8266_buf, "ERROR") != NULL)
            {
                ESP8266_Clear();
                return 0;
            }
        }
        osDelay(5);
    }
    ESP8266_Clear();
    return 0;
}

/**
  * @brief  ESP8266 初始化：AT 握手 + STA 模式 + 连接 WiFi
  * @retval 0=成功，1=失败
  * @note   流程：AT 测试 → 设置 STA 模式 → 启用 DHCP → 连接 AP
  *         失败时自动复位重试
  */
uint8_t ESP8266_Init(void)
{
    osDelay(1200);
    ESP8266_Clear();

    /* AT 握手测试 */
    if(!ESP8266_SendCmdWait("AT\r\n", "OK", 3000))
    {
        printf("[ESP] No response, try reset...\r\n");
        ESP8266_Reset();
        if(!ESP8266_SendCmdWait("AT\r\n", "OK", 3000))
            return 1;
    }

    /* 设置为 STA 模式 */
    if(!ESP8266_SendCmdWait("AT+CWMODE=1\r\n", "OK", 3000))
    {
        printf("[ESP] Set CWMODE failed\r\n");
        return 1;
    }

    /* 启用 STA 接口 DHCP */
    if(!ESP8266_SendCmdWait("AT+CWDHCP=1,1\r\n", "OK", 3000))
    {
        printf("[ESP] Set DHCP failed\r\n");
        return 1;
    }

    /* 断开已有连接，避免缓存干扰 */
    ESP8266_SendCmdWait("AT+CWQAP\r\n", "OK", 2000);
    osDelay(500);

    /* 连接 WiFi */
    printf("[ESP] Connecting to AP: %s\r\n", ESP8266_WIFI_NAME);
    snprintf(esp_cmd_buf, sizeof(esp_cmd_buf),
             "AT+CWJAP=\"%s\",\"%s\"\r\n", ESP8266_WIFI_NAME, ESP8266_WIFI_PWD);

    if(ESP8266_SendCmdWait(esp_cmd_buf, "GOT IP", 20000))
    {
        printf("[WiFi] GOT IP Success\r\n");
        return 0;
    }
    else
    {
        /* 检查残留缓冲区是否包含成功响应 */
        if(strstr((char*)esp8266_buf, "WIFI GOT IP") != NULL)
        {
            printf("[WiFi] GOT IP (from buffer)\r\n");
            ESP8266_Clear();
            return 0;
        }
        printf("[WiFi] Connect AP Failed! Resp: %s\r\n", esp8266_buf);
        ESP8266_Clear();
        return 1;
    }
}

/**
  * @brief  建立 MQTT 连接（OneNET 平台）
  * @retval 0=成功，1=失败
  * @note   流程：清理旧连接 → 配置用户凭证 → 建立连接 → 订阅属性下发主题
  */
_Bool ESP8266_MQTT_Connect(void)
{
    /* 清理可能存在的旧 MQTT 连接 */
    ESP8266_SendCmdWait("AT+MQTTCLEAN=0\r\n", "OK", 2000);
    osDelay(1000);

    /* 配置 MQTT 用户凭证（设备名、产品 ID、Token） */
    snprintf(esp_cmd_buf, sizeof(esp_cmd_buf),
        "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
        ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_TOKEN);

    if(!ESP8266_SendCmdWait(esp_cmd_buf, "OK", MQTT_USERCFG_TIMEOUT))
    {
        printf("MQTT USERCFG ERROR!!\r\n");
        return 1;
    }
    printf("MQTT USERCFG OK\r\n");
    osDelay(5000);

    /* 连接 MQTT 服务器 */
    snprintf(esp_cmd_buf, sizeof(esp_cmd_buf),
             "AT+MQTTCONN=0,\"%s\",%s,1\r\n", MQTT_SERVER, MQTT_PORT);

    if(!ESP8266_SendCmdWait(esp_cmd_buf, "+MQTTCONNECTED", MQTT_CONN_TIMEOUT))
    {
        printf("MQTT CONN ERROR!!\r\n");
        return 1;
    }
    printf("MQTT CONNECTED\r\n");
    mqtt_state = MQTT_CONNECTED;

    /* 订阅云端属性下发主题（接收远程控制指令） */
    osDelay(500);
    snprintf(esp_cmd_buf, sizeof(esp_cmd_buf),
             "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n",
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    ESP8266_SendCmdWait(esp_cmd_buf, "OK", MQTT_SUB_TIMEOUT);

    return 0;
}

/**
  * @brief  发布传感器数据到 MQTT 主题
  * @param  temp   温度（℃）
  * @param  humi   湿度（%RH）
  * @param  co2    等效 CO?（ppm）
  * @param  tvoc   TVOC（ppb）
  * @param  pm25   PM2.5（μg/m3）
  * @param  h2s    硫化氢（ppm）
  * @param  nh3    氨气（ppm）
  * @retval 0=成功，1=失败
  * @note   使用 AT+MQTTPUBRAW 指令，支持重试 3 次
  */
_Bool ESP8266_MQTT_Publish(float temp, float humi, uint16_t co2, uint16_t tvoc,
                           float pm25, float h2s, float nh3)
{
    int len, retry;

    /* 组装 OneNET 物模型 JSON 报文 */
    len = snprintf(esp_json_buf, sizeof(esp_json_buf),
        "{\"id\":\"%lu\",\"params\":{"
        "\"CurrentTemperature\":{\"value\":%.1f},"
        "\"humidity\":{\"value\":%.1f},"
        "\"CO2Value\":{\"value\":%.1f},"
        "\"tvoc\":{\"value\":%d},"
        "\"PM2D5\":{\"value\":%.1f},"
        "\"h2s\":{\"value\":%.1f},"
        "\"nh3\":{\"value\":%.1f}}}",
        HAL_GetTick(), temp, humi, (float)co2, tvoc, pm25, h2s, nh3);

    if(len >= sizeof(esp_json_buf) || len < 0)
    {
        printf("[MQTT] JSON too long!\r\n");
        return 1;
    }

    printf("[MQTT] JSON len=%d: %s\r\n", len, esp_json_buf);

    /* 重试机制：最多 3 次 */
    for(retry = 0; retry < MQTT_PUB_MAX_RETRY; retry++)
    {
        /* 请求发送权限（等待模块返回 ">" 提示符） */
        snprintf(esp_cmd_buf, sizeof(esp_cmd_buf),
                 "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/post\",%d,0,0\r\n",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME, len);

        if(!ESP8266_SendCmdWait(esp_cmd_buf, ">", MQTT_PUB_WAIT_TIMEOUT))
        {
            printf("[MQTT] Wait for > timeout (retry %d)\r\n", retry+1);
            if(retry < MQTT_PUB_MAX_RETRY - 1) osDelay(1000);
            continue;
        }

        /* 发送 JSON 数据 */
        ESP8266_Clear();
        HAL_UART_Transmit(&huart2, (uint8_t*)esp_json_buf, len, 5000);

        /* 等待发布确认 */
        if(ESP8266_WaitForStr("+MQTTPUB:OK", MQTT_PUB_RESP_TIMEOUT))
        {
            printf("[MQTT] Publish OK\r\n");
            return 0;
        }
        else
        {
            printf("[MQTT] Publish no ACK (retry %d)\r\n", retry+1);
            if(strstr((char*)esp8266_buf, "+MQTTDISCONNECTED") != NULL)
            {
                printf("[MQTT] Detected disconnect!\r\n");
                mqtt_state = MQTT_DISCONNECTED;
                return 1;
            }
        }

        if(retry < MQTT_PUB_MAX_RETRY - 1) osDelay(1000);
    }

    printf("[MQTT] Publish failed after %d retries\r\n", MQTT_PUB_MAX_RETRY);
    mqtt_state = MQTT_DISCONNECTED;
    return 1;
}

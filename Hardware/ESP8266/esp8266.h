#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"
#include "cmsis_os.h"

/* ======================== 用户配置区 ======================== */
#define ESP8266_WIFI_NAME      "hihi"
#define ESP8266_WIFI_PWD       "123456789"

#define ONENET_PRODUCT_ID      "UA0Ko6vlSY"
#define ONENET_DEVICE_NAME     "environmental"

#define MQTT_SERVER            "mqtts.heclouds.com"
#define MQTT_PORT              "1883"

#define ONENET_TOKEN           "version=2018-10-31&res=products%2FUA0Ko6vlSY%2Fdevices%2Fenvironmental&et=1807601183&method=md5&sign=FLxdPYgvw9AwJJCxPD%2BxZw%3D%3D"

/* MQTT 超时配置 */
#define MQTT_USERCFG_TIMEOUT    8000
#define MQTT_CONN_TIMEOUT       15000
#define MQTT_SUB_TIMEOUT        5000
#define MQTT_PUB_WAIT_TIMEOUT   8000
#define MQTT_PUB_RESP_TIMEOUT   10000
#define MQTT_PUB_MAX_RETRY      3

#define WIFI_CHECK_INTERVAL     60000
#define ESP8266_BUF_SIZE        1024
/* ============================================================ */

#define REV_OK      0
#define REV_WAIT    1

typedef enum {
    MQTT_DISCONNECTED = 0,
    MQTT_CONNECTED
} MQTT_State_t;

/* 串口接收计数器（中断写入，任务读取，volatile 防止编译器优化） */
extern volatile uint32_t esp8266_cnt;
extern volatile uint32_t esp8266_cntPre;
extern volatile MQTT_State_t mqtt_state;

void ESP8266_Clear(void);
_Bool ESP8266_WaitRecive(void);
_Bool ESP8266_WaitForStr(char *str, uint32_t timeout_ms);
_Bool ESP8266_SendCmdWait(char *cmd, char *resp, uint32_t timeout_ms);
_Bool ESP8266_SendCmd(char *cmd, char *res);

uint8_t ESP8266_Init(void);
void ESP8266_Reset(void);
_Bool ESP8266_CheckWiFiStatus(void);
_Bool ESP8266_MQTT_Connect(void);
_Bool ESP8266_MQTT_Publish(float temp, float humi, uint16_t co2, uint16_t tvoc,
                           float pm25, float h2s, float nh3);

#endif

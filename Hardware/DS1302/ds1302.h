#ifndef __DS1302_H
#define __DS1302_H

#include "main.h"

/* 引脚定义 */
#define DS1302_RST_PIN        DS1302_RST_Pin
#define DS1302_RST_PORT       DS1302_RST_GPIO_Port
#define DS1302_CLK_PIN        DS1302_CE_Pin
#define DS1302_CLK_PORT       DS1302_CE_GPIO_Port
#define DS1302_DAT_PIN        DS1302_DAT_Pin
#define DS1302_DAT_PORT       DS1302_DAT_GPIO_Port

#define DS1302_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()

/* 寄存器地址 */
#define DS1302_SEC       0x80
#define DS1302_MIN       0x82
#define DS1302_HOUR      0x84
#define DS1302_DATE      0x86
#define DS1302_MONTH     0x88
#define DS1302_DAY       0x8A
#define DS1302_YEAR      0x8C
#define DS1302_CONTROL   0x8E
#define DS1302_CLKBURST  0xBE

/* RAM 突发模式命令 */
#define DS1302_RAM_BURST_WRITE  0xFE
#define DS1302_RAM_BURST_READ   0xFF
#define DS1302_RAM_SIZE         31

/**
  * @brief  DS1302 时间结构体
  */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t week;
} DS1302_TimeTypeDef;

/* 初始化 & 单字节读写 */
void DS1302_Init(void);
void DS1302_WriteByte(uint8_t addr, uint8_t data);
uint8_t DS1302_ReadByte(uint8_t addr);

/* 突发模式（高效，推荐） */
void DS1302_ReadTimeBurst(DS1302_TimeTypeDef *time);
void DS1302_WriteTimeBurst(DS1302_TimeTypeDef *time);

/* 兼容接口（内部已优化为突发模式） */
void DS1302_WriteTime(DS1302_TimeTypeDef *time);
void DS1302_ReadTime(DS1302_TimeTypeDef *time);
void DS1302_SetTime(uint8_t year, uint8_t month, uint8_t date,
                    uint8_t hour, uint8_t min, uint8_t sec, uint8_t week);

/* RAM 操作（掉电保存） */
void DS1302_ReadRAM(uint8_t *buf);
void DS1302_WriteRAM(uint8_t *buf);

#endif

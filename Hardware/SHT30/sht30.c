#include "sht30.h"
#include "delay.h"
#include <stdio.h>
#include "cmsis_os.h"

/* 预计算转换常量，避免每次重复浮点除法 */
#define SHT30_SCALE_TEMP  (175.0f / 65535.0f)
#define SHT30_SCALE_RH    (100.0f / 65535.0f)
#define SHT30_OFFSET_TEMP (-45.0f)

/* CRC8 查表法（多项式 0x31，初始值 0xFF） */
static const uint8_t crc8_table[256] = {
    0x00,0x31,0x62,0x53,0xC4,0xF5,0xA6,0x97,0xB9,0x88,0xDB,0xEA,0x7D,0x4C,0x1F,0x2E,
    0x43,0x72,0x21,0x10,0x87,0xB6,0xE5,0xD4,0xFA,0xCB,0x98,0xA9,0x3E,0x0F,0x5C,0x6D,
    0x86,0xB7,0xE4,0xD5,0x42,0x73,0x20,0x11,0x3F,0x0E,0x5D,0x6C,0xFB,0xCA,0x99,0xA8,
    0xC5,0xF4,0xA7,0x96,0x01,0x30,0x63,0x52,0x7C,0x4D,0x1E,0x2F,0xB8,0x89,0xDA,0xEB,
    0x3D,0x0C,0x5F,0x6E,0xF9,0xC8,0x9B,0xAA,0x84,0xB5,0xE6,0xD7,0x40,0x71,0x22,0x13,
    0x7E,0x4F,0x1C,0x2D,0xBA,0x8B,0xD8,0xE9,0xC7,0xF6,0xA5,0x94,0x03,0x32,0x61,0x50,
    0xBB,0x8A,0xD9,0xE8,0x5F,0x6E,0x1D,0x2C,0x02,0x33,0x60,0x51,0xC6,0xF7,0xA4,0x95,
    0xF8,0xC9,0x9A,0xAB,0x3C,0x0D,0x5E,0x6F,0x41,0x70,0x23,0x12,0x85,0xB4,0xE7,0xD6,
    0x7A,0x4B,0x18,0x29,0xBE,0x8F,0xDC,0xED,0xC3,0xF2,0xA1,0x90,0x07,0x36,0x65,0x54,
    0x39,0x08,0x5B,0x6A,0xFD,0xCC,0x9F,0xAE,0x80,0xB1,0xE2,0xD3,0x44,0x75,0x26,0x17,
    0xFC,0xCD,0x9E,0xAF,0x38,0x09,0x5A,0x6B,0x45,0x74,0x27,0x16,0x81,0xB0,0xE3,0xD2,
    0xBF,0x8E,0xDD,0xEC,0x7B,0x4A,0x19,0x28,0x06,0x37,0x64,0x55,0xC2,0xF3,0xA0,0x91,
    0x47,0x76,0x25,0x14,0x83,0xB2,0xE1,0xD0,0xFE,0xCF,0x9C,0xAD,0x3A,0x0B,0x58,0x69,
    0x04,0x35,0x66,0x57,0xC0,0xF1,0xA2,0x93,0xBD,0x8C,0xDF,0xEE,0x79,0x48,0x1B,0x2A,
    0xC1,0xF0,0xA3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1A,0x2B,0xBC,0x8D,0xDE,0xEF,
    0x82,0xB3,0xE0,0xD1,0x46,0x77,0x24,0x15,0x3B,0x0A,0x59,0x68,0xFF,0xCE,0x9D,0xAC
};

/**
  * @brief  CRC8 校验（查表法）
  * @param  data      待校验数据指针
  * @param  len       数据长度
  * @param  checksum  接收到的 CRC 值
  * @retval 0=校验通过，1=失败
  */
static inline uint8_t SHT3x_CheckCrc(uint8_t *data, uint8_t len, uint8_t checksum)
{
    uint8_t crc = 0xFF;
    while(len--)
    {
        crc = crc8_table[crc ^ *data++];
    }
    return (crc == checksum) ? 0 : 1;
}

/* ==================== GPIO 模拟 I2C 底层 ==================== */

/**
  * @brief  I2C 引脚初始化（开漏输出 + 上拉）
  */
static inline void IIC_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = SHT30_SCL_PIN | SHT30_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SHT30_GPIO_PORT, &GPIO_InitStruct);

    SHT30_SCL_HIGH();
    SHT30_SDA_HIGH();
}

/**
  * @brief  I2C 起始信号
  */
static inline void IIC_Start(void)
{
    SHT30_SDA_HIGH();
    SHT30_SCL_HIGH();
    delay_us(2);
    SHT30_SDA_LOW();
    delay_us(2);
    SHT30_SCL_LOW();
}

/**
  * @brief  I2C 停止信号
  */
static inline void IIC_Stop(void)
{
    SHT30_SCL_LOW();
    SHT30_SDA_LOW();
    delay_us(2);
    SHT30_SCL_HIGH();
    delay_us(2);
    SHT30_SDA_HIGH();
}

/**
  * @brief  I2C 等待从机应答
  * @retval 0=ACK，1=NACK（超时 200us）
  */
static inline uint8_t IIC_Wait_Ack(void)
{
    uint8_t waitTime = 0;
    SHT30_SDA_HIGH();
    delay_us(1);
    SHT30_SCL_HIGH();
    delay_us(1);
    while(SHT30_SDA_READ())
    {
        if(++waitTime > 200)
        {
            IIC_Stop();
            return 1;
        }
    }
    SHT30_SCL_LOW();
    return 0;
}

/**
  * @brief  I2C 发送 ACK
  */
static inline void IIC_Ack(void)
{
    SHT30_SCL_LOW();
    SHT30_SDA_LOW();
    delay_us(1);
    SHT30_SCL_HIGH();
    delay_us(1);
    SHT30_SCL_LOW();
}

/**
  * @brief  I2C 发送 NACK
  */
static inline void IIC_NAck(void)
{
    SHT30_SCL_LOW();
    SHT30_SDA_HIGH();
    delay_us(1);
    SHT30_SCL_HIGH();
    delay_us(1);
    SHT30_SCL_LOW();
}

/**
  * @brief  I2C 发送单字节（MSB 先发）
  */
static inline void IIC_Send_Byte(uint8_t txd)
{
    for(uint8_t t = 0; t < 8; t++)
    {
        if(txd & 0x80) SHT30_SDA_HIGH();
        else           SHT30_SDA_LOW();
        txd <<= 1;
        delay_us(1);
        SHT30_SCL_HIGH();
        delay_us(1);
        SHT30_SCL_LOW();
    }
}

/**
  * @brief  I2C 读取单字节
  * @param  ack  1=发送 ACK 继续读取，0=发送 NACK 结束
  * @retval uint8_t  读取的字节
  */
static inline uint8_t IIC_Read_Byte(uint8_t ack)
{
    uint8_t receive = 0;
    SHT30_SDA_HIGH();
    for(uint8_t i = 0; i < 8; i++)
    {
        SHT30_SCL_LOW();
        delay_us(1);
        SHT30_SCL_HIGH();
        receive <<= 1;
        if(SHT30_SDA_READ()) receive |= 1;
        delay_us(1);
    }
    if(ack) IIC_Ack();
    else    IIC_NAck();
    return receive;
}

/* ==================== 温度/湿度计算 ==================== */

/**
  * @brief  计算温度值（摄氏度）
  * @param  rawValue  16 位原始数据（低 2 位为状态位，清零）
  * @retval float  温度值（-45 ~ 130 ℃）
  * @note   公式：T = -45 + 175 × raw / 65535
  */
static inline float SHT3x_CalcTemperatureC(uint16_t rawValue)
{
    rawValue &= ~0x0003;
    return (rawValue * SHT30_SCALE_TEMP) + SHT30_OFFSET_TEMP;
}

/**
  * @brief  计算相对湿度
  * @param  rawValue  16 位原始数据
  * @retval float  湿度值（0 ~ 100 %RH）
  * @note   公式：RH = 100 × raw / 65535
  */
static inline float SHT3x_CalcRH(uint16_t rawValue)
{
    rawValue &= ~0x0003;
    return (rawValue * SHT30_SCALE_RH);
}

/* ==================== 对外接口 ==================== */

/**
  * @brief  SHT30 初始化
  * @note   发送周期性测量命令（0x2236），启动自动采集
  */
void SHT30_Init(void)
{
    IIC_GPIO_Init();
    IIC_Start();
    IIC_Send_Byte(SHT30_ADDR_WRITE);
    IIC_Wait_Ack();
    IIC_Send_Byte(0x22);
    IIC_Wait_Ack();
    IIC_Send_Byte(0x36);
    IIC_Wait_Ack();
    IIC_Stop();

    osDelay(200);
}

/**
  * @brief  读取 SHT30 温湿度（带 CRC 校验 + 自动重试）
  * @param  temp  输出温度指针（℃）
  * @param  humi  输出湿度指针（%RH）
  * @retval 0=成功，1=失败（两次 CRC 校验均失败）
  * @note   最多重试 2 次，CRC 失败时自动重新读取
  */
uint8_t SHT30_ReadDataEx(float *temp, float *humi)
{
    uint8_t buf[6];
    uint16_t raw;
    uint8_t retry = 2;

    while(retry--)
    {
        /* 发送读取命令 */
        IIC_Start();
        IIC_Send_Byte(SHT30_ADDR_WRITE);
        if(IIC_Wait_Ack()) { IIC_Stop(); continue; }
        IIC_Send_Byte(0xE0);
        if(IIC_Wait_Ack()) { IIC_Stop(); continue; }
        IIC_Send_Byte(0x00);
        if(IIC_Wait_Ack()) { IIC_Stop(); continue; }

        /* 切换为读模式，接收 6 字节（温度 + CRC + 湿度 + CRC） */
        IIC_Start();
        IIC_Send_Byte(SHT30_ADDR_READ);
        if(IIC_Wait_Ack()) { IIC_Stop(); continue; }

        for(uint8_t i = 0; i < 6; i++)
        {
            buf[i] = IIC_Read_Byte(i < 5 ? 1 : 0);
        }
        IIC_Stop();

        /* 温度 CRC 校验 */
        if(!SHT3x_CheckCrc(&buf[0], 2, buf[2]))
        {
            raw = ((uint16_t)buf[0] << 8) | buf[1];
            *temp = SHT3x_CalcTemperatureC(raw);

            /* 湿度 CRC 校验 */
            if(!SHT3x_CheckCrc(&buf[3], 2, buf[5]))
            {
                raw = ((uint16_t)buf[3] << 8) | buf[4];
                *humi = SHT3x_CalcRH(raw);
                return 0;
            }
        }

        /* CRC 失败，等待 5ms 后重试 */
        osDelay(5);
    }

    *temp = *humi = 0.0f;
    return 1;
}

/**
  * @brief  兼容旧接口（内部调用 SHT30_ReadDataEx）
  */
void SHT30_ReadData(float *temp, float *humi)
{
    SHT30_ReadDataEx(temp, humi);
}

#include "sgp30.h"
#include "delay.h"
#include "cmsis_os.h"

/* 引脚操作宏 */
#define SCL_HIGH()  HAL_GPIO_WritePin(SGP30_SCL_PORT, SGP30_SCL_PIN, GPIO_PIN_SET)
#define SCL_LOW()   HAL_GPIO_WritePin(SGP30_SCL_PORT, SGP30_SCL_PIN, GPIO_PIN_RESET)
#define SDA_HIGH()  HAL_GPIO_WritePin(SGP30_SDA_PORT, SGP30_SDA_PIN, GPIO_PIN_SET)
#define SDA_LOW()   HAL_GPIO_WritePin(SGP30_SDA_PORT, SGP30_SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ()  HAL_GPIO_ReadPin(SGP30_SDA_PORT, SGP30_SDA_PIN)

/**
  * @brief  SGP30 初始化
  * @retval 0=成功，1=失败
  * @note   使用 GPIO 模拟 I2C，SDA 全程开漏输出免方向切换：
  *         - 发送时：写 0 拉低，写 1 释放（外部上拉拉高）
  *         - 接收时：写 1 释放总线后读取
  */
uint8_t SGP30_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* SCL：开漏输出 + 上拉 */
    gpio_init.Pin = SGP30_SCL_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SGP30_SCL_PORT, &gpio_init);

    /* SDA：开漏输出（全程不变，读时写 1 释放总线） */
    gpio_init.Pin = SGP30_SDA_PIN;
    HAL_GPIO_Init(SGP30_SDA_PORT, &gpio_init);

    SCL_HIGH();
    SDA_HIGH();

    osDelay(100);

    /* 发送初始化空气质量测量命令 */
    if(sgp30_write_command(SGP30_CMD_INIT_AIR_QUALITY))
        return 1;

    osDelay(50);
    return 0;
}

/* ==================== GPIO 模拟 I2C 底层 ==================== */

/**
  * @brief  I2C 起始信号
  */
static inline void sgp30_i2c_start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    delay_us(2);
    SDA_LOW();
    delay_us(2);
    SCL_LOW();
}

/**
  * @brief  I2C 停止信号
  */
static inline void sgp30_i2c_stop(void)
{
    SDA_LOW();
    SCL_HIGH();
    delay_us(2);
    SDA_HIGH();
    delay_us(2);
}

/**
  * @brief  I2C 发送单字节（MSB 先发）
  */
static inline void sgp30_i2c_send_byte(uint8_t byte)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        if(byte & 0x80) SDA_HIGH();
        else            SDA_LOW();
        byte <<= 1;
        delay_us(1);
        SCL_HIGH();
        delay_us(1);
        SCL_LOW();
    }
}

/**
  * @brief  I2C 读取单字节
  * @param  ack  1=发送 ACK 继续读取，0=发送 NACK 结束读取
  * @retval uint8_t  读取的字节
  */
static inline uint8_t sgp30_i2c_read_byte(uint8_t ack)
{
    uint8_t byte = 0;

    /* 开漏模式下，写 1 释放总线，然后读取 */
    SDA_HIGH();

    for(uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;
        SCL_HIGH();
        delay_us(1);
        if(SDA_READ()) byte |= 0x01;
        SCL_LOW();
        delay_us(1);
    }

    /* 发送 ACK/NACK */
    if(ack) SDA_LOW();
    else    SDA_HIGH();

    delay_us(1);
    SCL_HIGH();
    delay_us(1);
    SCL_LOW();
    SDA_HIGH();     /* 释放总线 */

    return byte;
}

/**
  * @brief  I2C 等待从机应答
  * @retval 0=ACK，1=NACK
  */
static inline uint8_t sgp30_i2c_wait_ack(void)
{
    uint8_t ack;
    SDA_HIGH();     /* 释放总线 */
    delay_us(1);
    SCL_HIGH();
    delay_us(1);
    ack = SDA_READ();
    SCL_LOW();
    delay_us(1);
    return ack;
}

/**
  * @brief  向 SGP30 发送 16 位命令
  * @param  cmd  SGP30 命令字
  * @retval 0=成功，1=失败（无应答）
  */
uint8_t sgp30_write_command(uint16_t cmd)
{
    sgp30_i2c_start();
    sgp30_i2c_send_byte(0xB0);          /* 写地址：0x58 << 1 | 0 */
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_send_byte((cmd >> 8) & 0xFF);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_send_byte(cmd & 0xFF);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_stop();
    return 0;
}

/* ==================== 对外接口 ==================== */

/**
  * @brief  读取 SGP30 空气质量数据（eCO2 + TVOC）
  * @param  eco2   输出等效 CO2 浓度指针（ppm）
  * @param  tvoc   输出 TVOC 浓度指针（ppb）
  * @retval 0=成功，1=失败或数据异常
  * @note   测量后必须等待 ≥12ms 再读取
  *         返回数据包含 CRC 校验位，此处简化为范围校验
  */
uint8_t SGP30_ReadAirQuality(uint16_t *eco2, uint16_t *tvoc)
{
    uint8_t data[6];

    if(sgp30_write_command(SGP30_CMD_MEASURE_AIR_QUALITY))
        return 1;

    osDelay(12);    /* SGP30 测量必须 ≥12ms */

    sgp30_i2c_start();
    sgp30_i2c_send_byte(0xB1);          /* 读地址：0x58 << 1 | 1 */
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    for(uint8_t i = 0; i < 6; i++)
    {
        data[i] = sgp30_i2c_read_byte(i < 5 ? 1 : 0);
    }
    sgp30_i2c_stop();

    *eco2 = ((uint16_t)data[0] << 8) | data[1];
    *tvoc = ((uint16_t)data[3] << 8) | data[4];

    /* 范围校验（SGP30 有效范围：eCO2 400~60000 ppm，TVOC 0~60000 ppb） */
    if(*eco2 < 400 || *eco2 > 60000 || *tvoc > 60000)
    {
        *eco2 = 0;
        *tvoc = 0;
        return 1;   /* 数据异常 */
    }

    return 0;
}

/**
  * @brief  读取 SGP30 序列号（调试用途）
  * @param  serial_id  输出序列号指针
  * @retval 0=成功，1=失败
  */
uint8_t SGP30_GetSerialID(uint32_t *serial_id)
{
    uint8_t data[9];

    sgp30_i2c_start();
    sgp30_i2c_send_byte(0xB0);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_send_byte(0x36);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_send_byte(0x82);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    sgp30_i2c_stop();
    osDelay(20);

    sgp30_i2c_start();
    sgp30_i2c_send_byte(0xB1);
    if(sgp30_i2c_wait_ack()) { sgp30_i2c_stop(); return 1; }

    for(uint8_t i = 0; i < 9; i++)
    {
        data[i] = sgp30_i2c_read_byte(i < 8 ? 1 : 0);
    }
    sgp30_i2c_stop();

    *serial_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                 ((uint32_t)data[3] << 8)  | data[4];
    return 0;
}

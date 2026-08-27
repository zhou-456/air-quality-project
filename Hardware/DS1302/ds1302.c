#include "ds1302.h"
#include "delay.h"

/* 引脚操作宏 */
#define DS1302_RST(n)   HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, n ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define DS1302_CLK(n)   HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, n ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define DS1302_DAT(n)   HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, n ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define DS1302_DAT_READ HAL_GPIO_ReadPin(DS1302_DAT_PORT, DS1302_DAT_PIN)

/**
  * @brief  配置 DAT 引脚方向
  * @param  mode  0=推挽输出，1=上拉输入
  * @retval None
  * @note   DS1302 为单总线半双工通信，发送时输出，接收时输入
  */
static void DS1302_DAT_Config(uint8_t mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DS1302_DAT_PIN;
    
    if(mode == 0) {
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    } else {
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    }
    HAL_GPIO_Init(DS1302_DAT_PORT, &GPIO_InitStruct);
}

/**
  * @brief  DS1302 初始化
  * @param  None
  * @retval None
  * @note   1. 配置 RST/CLK 为推挽输出，DAT 初始化为输出
  *         2. 关闭写保护
  *         3. 检查时钟运行位，若停止则启动
  */
void DS1302_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    DS1302_GPIO_CLK_ENABLE();
    
    /* RST 和 CLK 推挽输出 */
    GPIO_InitStruct.Pin = DS1302_RST_PIN | DS1302_CLK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS1302_RST_PORT, &GPIO_InitStruct);
    
    /* DAT 初始化为推挽输出 */
    DS1302_DAT_Config(0);
    
    DS1302_RST(0);
    DS1302_CLK(0);
    DS1302_DAT(0);
    
    delay_ms(2);
    
    /* 关闭写保护 */
    DS1302_WriteByte(DS1302_CONTROL, 0x00);
    delay_us(10);
    
    /* 确保时钟运行（清除 CH 位） */
    uint8_t sec = DS1302_ReadByte(DS1302_SEC);
    if(sec & 0x80) {
        DS1302_WriteByte(DS1302_SEC, sec & 0x7F);
    }
}

/* ==================== 底层单字节读写 ==================== */

/**
  * @brief  向 DS1302 写入单字节（LSB 先发）
  * @param  data  待发送的 8 位数据
  */
static void DS1302_Write(uint8_t data)
{
    DS1302_DAT_Config(0);   /* 推挽输出 */
    
    for(uint8_t i = 0; i < 8; i++) {
        DS1302_CLK(0);
        delay_us(2);
        
        DS1302_DAT(data & 0x01);
        data >>= 1;
        
        delay_us(2);
        DS1302_CLK(1);
        delay_us(4);
    }
}

/**
  * @brief  从 DS1302 读取单字节（LSB 先收）
  * @retval uint8_t  读取的 8 位数据
  */
static uint8_t DS1302_Read(void)
{
    uint8_t data = 0;
    uint8_t bit;
    
    DS1302_DAT_Config(1);   /* 上拉输入 */
    delay_us(2);
    
    for(uint8_t i = 0; i < 8; i++) {
        DS1302_CLK(0);
        delay_us(4);
        
        bit = DS1302_DAT_READ;
        data >>= 1;
        if(bit) data |= 0x80;
        
        DS1302_CLK(1);
        delay_us(2);
    }
    
    return data;
}

/**
  * @brief  向指定寄存器地址写入单字节
  * @param  addr  寄存器地址
  * @param  data  待写入数据
  */
void DS1302_WriteByte(uint8_t addr, uint8_t data)
{
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(addr);
    DS1302_Write(data);
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
}

/**
  * @brief  从指定寄存器地址读取单字节
  * @param  addr  寄存器地址
  * @retval uint8_t  读取数据
  */
uint8_t DS1302_ReadByte(uint8_t addr)
{
    uint8_t data;
    
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(addr | 0x01);
    
    data = DS1302_Read();
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
    
    DS1302_DAT_Config(0);   /* 读完后切回输出 */
    
    return data;
}

/* ==================== BCD 转换 ==================== */

static uint8_t DEC2BCD(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

static uint8_t BCD2DEC(uint8_t bcd)
{
    uint8_t high = (bcd >> 4) & 0x0F;
    uint8_t low = bcd & 0x0F;
    if(high > 9 || low > 9) return 0;
    return high * 10 + low;
}

/* ==================== 突发模式（高效读写） ==================== */

/**
  * @brief  突发模式读取时间（8 字节一次性读取）
  * @param  time  指向时间结构体的指针
  * @note   相比单字节读取，减少 7 次通信开销，效率提升约 8 倍
  */
void DS1302_ReadTimeBurst(DS1302_TimeTypeDef *time)
{
    uint8_t buf[8];
    
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(DS1302_CLKBURST | 0x01);   /* 读突发命令 0xBF */
    
    for(uint8_t i = 0; i < 8; i++) {
        buf[i] = DS1302_Read();
    }
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
    
    /* 解析 BCD 时间数据 */
    time->second = BCD2DEC(buf[0] & 0x7F);
    time->minute = BCD2DEC(buf[1]);
    time->hour   = BCD2DEC(buf[2] & 0x3F);
    time->date   = BCD2DEC(buf[3]);
    time->month  = BCD2DEC(buf[4]);
    time->week   = BCD2DEC(buf[5]);
    time->year   = BCD2DEC(buf[6]);
    
    if(time->year < 50) time->year += 2000;
    else                time->year += 1900;
}

/**
  * @brief  突发模式写入时间（8 字节一次性写入）
  * @param  time  指向时间结构体的指针
  */
void DS1302_WriteTimeBurst(DS1302_TimeTypeDef *time)
{
    uint8_t buf[8];
    
    buf[0] = DEC2BCD(time->second) & 0x7F;
    buf[1] = DEC2BCD(time->minute);
    buf[2] = DEC2BCD(time->hour) & 0x3F;
    buf[3] = DEC2BCD(time->date);
    buf[4] = DEC2BCD(time->month);
    buf[5] = DEC2BCD(time->week);
    buf[6] = DEC2BCD(time->year % 100);
    buf[7] = 0x80;  /* 写保护开启 */
    
    DS1302_WriteByte(DS1302_CONTROL, 0x00);   /* 先关写保护 */
    delay_us(10);
    
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(DS1302_CLKBURST);   /* 写突发命令 0xBE */
    
    for(uint8_t i = 0; i < 8; i++) {
        DS1302_Write(buf[i]);
    }
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
}

/* ==================== 兼容接口（内部使用突发模式） ==================== */

void DS1302_ReadTime(DS1302_TimeTypeDef *time)
{
    DS1302_ReadTimeBurst(time);
}

void DS1302_WriteTime(DS1302_TimeTypeDef *time)
{
    DS1302_WriteTimeBurst(time);
}

/**
  * @brief  快速设置时间（调试/首次烧录用）
  * @param  year  年份后两位（如 26 表示 2026）
  * @param  month  月 1-12
  * @param  date   日 1-31
  * @param  hour   时 0-23
  * @param  min    分 0-59
  * @param  sec    秒 0-59
  * @param  week   星期 1-7
  */
void DS1302_SetTime(uint8_t year, uint8_t month, uint8_t date, 
                    uint8_t hour, uint8_t min, uint8_t sec, uint8_t week)
{
    DS1302_TimeTypeDef time = {
        .year = year, .month = month, .date = date,
        .hour = hour, .minute = min, .second = sec, .week = week
    };
    DS1302_WriteTimeBurst(&time);
}

/* ==================== RAM 突发读写（31 字节掉电保存） ==================== */

/**
  * @brief  突发模式读取 RAM（31 字节）
  * @param  buf  接收缓冲区，至少 31 字节
  */
void DS1302_ReadRAM(uint8_t *buf)
{
    uint8_t i;
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(DS1302_RAM_BURST_READ);
    for(i = 0; i < DS1302_RAM_SIZE; i++)
    {
        buf[i] = DS1302_Read();
    }
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
}

/**
  * @brief  突发模式写入 RAM（31 字节）
  * @param  buf  待写入数据，31 字节
  */
void DS1302_WriteRAM(uint8_t *buf)
{
    uint8_t i;
    DS1302_RST(0);
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(1);
    delay_us(2);
    
    DS1302_Write(DS1302_RAM_BURST_WRITE);
    for(i = 0; i < DS1302_RAM_SIZE; i++)
    {
        DS1302_Write(buf[i]);
    }
    
    DS1302_CLK(0);
    delay_us(2);
    DS1302_RST(0);
    delay_us(2);
}

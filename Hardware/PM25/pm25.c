#include "pm25.h"
#include <stdio.h>
#include "cmsis_os.h"
#include "adc.h"

/* ADC2 句柄（main.c 中初始化） */
extern ADC_HandleTypeDef hadc2;

/**
  * @brief  PM2.5 传感器 LED 驱动开启
  * @note   GP2Y1010AU0F 需要 LED 脉冲驱动，低电平点亮
  */
static inline void PM25_LED_On(void)
{
    HAL_GPIO_WritePin(PM25_LED_PORT, PM25_LED_PIN, GPIO_PIN_SET);
}

/**
  * @brief  PM2.5 传感器 LED 驱动关闭
  */
static inline void PM25_LED_Off(void)
{
    HAL_GPIO_WritePin(PM25_LED_PORT, PM25_LED_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  DWT 周期计数器实现微秒级延时
  * @param  us  延时微秒数
  * @note   基于 Cortex-M3 DWT 模块，72MHz 时 1us = 72 个时钟周期
  *         无需定时器，不占用中断资源
  */
static inline void delay_us_dwt(uint32_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < ticks);
}

/**
  * @brief  ADC2 单次采样
  * @retval uint16_t  12 位 ADC 采样值
  * @note   启动 → 轮询等待 → 读取 → 停止，单次模式不复用通道配置
  */
static inline uint16_t PM25_ADC_Read(void)
{
    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 5);
    uint16_t val = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);
    return val;
}

/**
  * @brief  PM2.5 单次完整采样（LED 脉冲时序 + ADC）
  * @retval uint16_t  ADC 采样值
  * @note   GP2Y1010AU0F 官方时序：
  *         LED On → 延时 280us → ADC 采样 → 延时 40us → LED Off
  */
static uint16_t PM25_SampleOnce(void)
{
    uint16_t val;

    PM25_LED_On();
    delay_us_dwt(280);      /* GP2Y 官方要求：280us 稳定后采样 */
    val = PM25_ADC_Read();
    delay_us_dwt(40);       /* 保持 40us 后关闭 LED */
    PM25_LED_Off();

    return val;
}

/**
  * @brief  PM2.5 传感器初始化
  * @param  None
  * @retval None
  * @note   1. 配置 LED 驱动 GPIO
  *         2. 启用 DWT 周期计数器（Cortex-M3 内置）
  *         3. 配置 ADC2 通道 6（采样时间 1.5 周期，GP2Y 输出变化慢）
  */
void PM25_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gp = {0};
    gp.Pin = PM25_LED_PIN;
    gp.Mode = GPIO_MODE_OUTPUT_PP;
    gp.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PM25_LED_PORT, &gp);
    PM25_LED_Off();

    /* 启用 DWT 周期计数器（Cortex-M3 内置，无需额外硬件） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 配置 ADC2 通道 6（只配置一次，后续直接启动采样） */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;   /* GP2Y 输出变化慢，短采样即可 */
    HAL_ADC_ConfigChannel(&hadc2, &sConfig);
}

/**
  * @brief  获取 PM2.5 浓度（中值滤波，抗脉冲干扰）
  * @param  pm25_ug_m3  输出浓度指针（ug/m3）
  * @retval None
  * @note   1. 采集 5 次，间隔 10ms（模块要求最小间隔）
  *         2. 中值滤波取中间值，消除偶然脉冲干扰
  *         3. 电压转浓度：0.3f 为经验系数，需根据具体型号校准
  */
void PM25_GetData(float *pm25_ug_m3)
{
    if(pm25_ug_m3 == NULL) return;

    uint16_t samples[5];
    uint8_t i, j;

    /* 采集 5 次，间隔 10ms */
    for(i = 0; i < 5; i++)
    {
        samples[i] = PM25_SampleOnce();
        osDelay(10);
    }

    /* 中值滤波：冒泡排序取中间值 */
    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4 - i; j++)
        {
            if(samples[j] > samples[j+1])
            {
                uint16_t tmp = samples[j];
                samples[j] = samples[j+1];
                samples[j+1] = tmp;
            }
        }
    }
    uint16_t raw = samples[2];   /* 中值 */

    /* 转电压（3.3V 量程，12bit） */
    float voltage = (raw * 3300.0f) / 4095.0f;

    /* 电压转浓度（经验公式，需根据 GP2Y 型号校准） */
    if(voltage < 80.0f)
        *pm25_ug_m3 = 0.0f;
    else
        *pm25_ug_m3 = (voltage - 80.0f) * 0.3f;
}

/**
  * @brief  获取原始采样值（含 LED 脉冲时序，调试接口）
  * @retval uint16_t  单次完整采样后的 ADC 值
  */
uint16_t PM25_GetRaw(void)
{
    return PM25_SampleOnce();
}


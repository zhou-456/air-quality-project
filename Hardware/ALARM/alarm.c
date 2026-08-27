#include "alarm.h"
#include "gpio.h"  // 依赖GPIO驱动，必须包含

/**
 * @brief  报警模块初始化：报警灯默认熄灭，蜂鸣器默认关闭
 * @retval 无
 */
void Alarm_Init(void)
{
    // 蜂鸣器默认关闭（低电平，不响）
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
    // 报警灯默认熄灭（高电平，不亮）
    HAL_GPIO_WritePin(ALARMLED_GPIO_Port, ALARMLED_Pin, GPIO_PIN_RESET);
}

/* ------------------------------ 报警灯驱动 ------------------------------ */
/**
 * @brief  报警灯点亮（低电平）
 * @retval 无
 */
void Alarm_On(void)
{
    HAL_GPIO_WritePin(ALARMLED_GPIO_Port, ALARMLED_Pin, GPIO_PIN_SET);
}

/**
 * @brief  报警灯熄灭（高电平）
 * @retval 无
 */
void Alarm_Off(void)
{
    HAL_GPIO_WritePin(ALARMLED_GPIO_Port, ALARMLED_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  报警灯电平翻转（亮<->灭）
 * @retval 无
 */
void Alarm_Toggle(void)
{
    HAL_GPIO_TogglePin(ALARMLED_GPIO_Port, ALARMLED_Pin);
}

/* ------------------------------ 蜂鸣器驱动 ------------------------------ */
/**
 * @brief  蜂鸣器开启（高电平，响）
 * @retval 无
 */
void Beep_On(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
}

/**
 * @brief  蜂鸣器关闭（低电平，不响）
 * @retval 无
 */
void Beep_Off(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  蜂鸣器电平翻转（响<->不响）
 * @retval 无
 */
void Beep_Toggle(void)
{
    HAL_GPIO_TogglePin(BEEP_GPIO_Port, BEEP_Pin);
}

/* ------------------------------ 组合报警驱动（灯+蜂鸣器） ------------------------------ */
/**
 * @brief  报警灯+蜂鸣器同时开启
 * @retval 无
 */
void Alarm_Beep_On(void)
{
    Alarm_On();  // 灯亮
    Beep_On();   // 蜂鸣器响
}

/**
 * @brief  报警灯+蜂鸣器同时关闭
 * @retval 无
 */
void Alarm_Beep_Off(void)
{
    Alarm_Off(); // 灯灭
    Beep_Off();  // 蜂鸣器停
}

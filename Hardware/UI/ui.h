#ifndef __UI_H
#define __UI_H

#include "lcd.h"
#include "ds1302.h"
#include "mq_senser.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_Init(void);
void UI_UpdateAll(float temp, float humi,
                  uint16_t eco2, uint16_t tvoc,
                  float pm25,
                  MQ_Data_t *mq,
                  DS1302_TimeTypeDef *time);

#ifdef __cplusplus
}
#endif

#endif

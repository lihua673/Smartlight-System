#ifndef __LED_PWM_H
#define __LED_PWM_H

#include "main.h"

// 初始化 PWM（在 main 里调用一次）
void LED_PWM_Init(void);

// 设置亮度：0 ~ 1000
// 0 = 灭
// 1000 = 最亮
void LED_SetBrightness(uint16_t duty);

#endif

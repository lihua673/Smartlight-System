#include "led_pwm.h"

extern TIM_HandleTypeDef htim3;  // 引用CubeMX生成的定时器句柄

// PWM初始化（只启动，不用重复配置）
void LED_PWM_Init(void)
{
    // 启动 TIM3_CH2 → PA7 PWM
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

// 规范设置亮度函数
void LED_SetBrightness(uint16_t duty)
{
    // 限幅保护（防止越界）
    if(duty > 999)
        duty = 999;

    // 设置占空比
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, duty);
}

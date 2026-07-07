#ifndef __LIGHT_CONTROLLER_H
#define __LIGHT_CONTROLLER_H

#include "sensors.h"
#include <stdint.h>

/* 根据传感器数据计算目标PWM占空比(0~999) */
uint16_t LightCtrl_Update(SensorData_t *sensor);

#endif

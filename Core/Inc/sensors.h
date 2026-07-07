#ifndef __SENSORS_H
#define __SENSORS_H

#include "app_state.h"
#include <stdint.h>

/* 传感器统一数据结构 */
typedef struct {
    float    lux;             // 环境光照(lux)
    uint8_t  human_present;   // 雷达检测到人体
    uint8_t  face_detected;   // K210人脸识别
    uint16_t radar_distance;  // 雷达测距(cm)
} SensorData_t;

/* 采集所有传感器数据（内置200ms节流） */
void Sensors_ReadAll(SensorData_t *out);

#endif

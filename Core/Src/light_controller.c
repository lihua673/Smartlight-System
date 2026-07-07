#include "light_controller.h"

/* 光照分档亮度表: <80lux=70%, <120lux=50%, <170lux=30%, >=170lux=0% */
uint16_t LightCtrl_Update(SensorData_t *sensor)
{
    if (sensor == NULL) return 0;

    /* 基础亮度（根据光照） */
    uint16_t base_duty = 0;
    if (sensor->lux < 80)       base_duty = 999 * 70 / 100;
    else if (sensor->lux < 120) base_duty = 999 * 50 / 100;
    else if (sensor->lux < 170) base_duty = 999 * 30 / 100;
    else                        base_duty = 0;

    /* 有人 + 有人脸：额外增加30% */
    uint16_t extra_duty = 0;
    if (sensor->human_present && sensor->face_detected)
    {
        extra_duty = 999 * 30 / 100;
    }

    uint16_t target = base_duty + extra_duty;
    if (target > 999) target = 999;

    return target;
}

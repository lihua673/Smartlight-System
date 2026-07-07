#include "sensors.h"
#include "BH1750.h"

/* 采集所有传感器数据（200ms节流） */
void Sensors_ReadAll(SensorData_t *out)
{
    if (out == NULL) return;

    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick >= 200)
    {
        last_tick = HAL_GetTick();
        g_app.lux = bh_data_read();
    }

    out->lux            = g_app.lux;
    out->human_present  = (g_app.radar.human_state != HUMAN_NONE) ? 1 : 0;
    out->face_detected  = g_app.face_detected;
    out->radar_distance = g_app.radar.distance;
}

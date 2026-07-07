#include "cloud_comm.h"
#include "esp8266.h"
#include "app_state.h"

/* 初始化WiFi和MQTT连接 */
void Cloud_Init(void)
{
    ESP8266_Init();
    if (ESP8266_Init_Success)
    {
        MQTT_Init();
        g_app.mqtt_ready = 1;
    }
}

/* 上传亮度（20s节流） */
void Cloud_Upload(uint8_t percent)
{
    if (!g_app.wifi_ready || !g_app.mqtt_ready) return;

    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick >= 20000)
    {
        last_tick = HAL_GetTick();
        MQTT_Publish_Data(percent);
    }
}

/* 连接状态 */
uint8_t Cloud_IsConnected(void)
{
    return g_app.wifi_ready && g_app.mqtt_ready;
}

/* 断连自动重连（30s间隔） */
void Cloud_TryReconnect(void)
{
    if (Cloud_IsConnected()) return;

    static uint32_t last_try = 0;
    if (HAL_GetTick() - last_try >= 30000)
    {
        last_try = HAL_GetTick();
        ESP8266_Init();
        if (ESP8266_Init_Success)
        {
            MQTT_Init();
            g_app.mqtt_ready = 1;
        }
    }
}

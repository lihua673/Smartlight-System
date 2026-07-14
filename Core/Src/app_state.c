#include "app_state.h"

/* 全局应用状态
 * 注意：app_state.h中的 #define last_publish_tick 宏会干扰
 * 结构体初始化器中的 .last_publish_tick 字段名，需先 undef */
#undef last_publish_tick

AppState_t g_app = {
    .lux = 0,
    .face_detected = 0,
    .face_count = 0,
    .led_duty = 0,
    .manual_mode = 0,
    .manual_brightness = 0,
    .wifi_ready = 0,
    .mqtt_ready = 0,
    .last_publish_tick = 0,
    .esp_ip = {0}
};

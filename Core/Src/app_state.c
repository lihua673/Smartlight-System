#include "app_state.h"

/* 全局应用状态 */
AppState_t g_app = {
    .lux = 0,
    .face_detected = 0,
    .led_duty = 0,
    .wifi_ready = 0,
    .mqtt_ready = 0,
    .last_publish_tick = 0
};

#ifndef __APP_STATE_H
#define __APP_STATE_H

#include "ld2402_uart.h"
#include <stdint.h>

/* 全局应用状态结构体 */
typedef struct {
    /* 传感器数据 */
    float    lux;              // BH1750光照值(lux)
    uint8_t  face_detected;    // K210人脸识别标志
    uint16_t led_duty;         // 当前实际PWM占空比(0~999)

    /* 雷达状态 */
    LD2402_Data_t radar;

    /* 通信状态 */
    uint8_t wifi_ready;        // ESP8266初始化成功
    uint8_t mqtt_ready;        // MQTT连接成功

    /* UART接收缓冲 */
    struct {
        char     buf[1024];
        uint16_t len;
        uint8_t  complete;
    } uart1_rx, uart2_rx;

    /* 时间基准 */
    uint32_t last_publish_tick;  // 上次MQTT上传时间
} AppState_t;

/* 全局状态实例 */
extern AppState_t g_app;

/* 便捷别名（兼容旧代码） */
#define g_lux               g_app.lux
#define g_face_detected     g_app.face_detected
#define g_led_duty          g_app.led_duty
#define ESP8266_Init_Success g_app.wifi_ready
#define ld24_data           g_app.radar
#define last_publish_tick   g_app.last_publish_tick

#define USART1_RX_BUF       g_app.uart1_rx.buf
#define USART1_RX_LEN       g_app.uart1_rx.len
#define USART1_RX_FINISH    g_app.uart1_rx.complete
#define USART2_RX_BUF       g_app.uart2_rx.buf
#define USART2_RX_LEN       g_app.uart2_rx.len
#define USART2_RX_FINISH    g_app.uart2_rx.complete

#endif

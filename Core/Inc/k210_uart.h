#ifndef K210_UART_H
#define K210_UART_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* K210人脸检测结果 */
typedef struct {
    uint8_t  face_count;      // 检测到的人脸数量(0~10)
    uint8_t  detected;        // 0=无人脸, 1=检测到人脸(兼容旧代码)
    uint16_t x;               // 最大人脸框左上角X坐标
    uint16_t y;               // 最大人脸框左上角Y坐标
    uint16_t w;               // 最大人脸框宽度
    uint16_t h;               // 最大人脸框高度
    uint32_t last_tick;       // 最后一次收到有效数据的时间戳
} K210_FaceData_t;

/* 协议扩展: 最多支持的人脸数量 */
#define K210_MAX_FACES        10

/* K210协议常量 */
#define K210_FRAME_START    0x24
#define K210_FRAME_END      0x23
#define K210_SEPARATOR      0x2C    // 逗号','
#define K210_CLASS_NUM      0x05
#define K210_CLASS_GROUP    0xBB
#define K210_FACE_TIMEOUT_MS  1500  // 超时判定无人脸

/* 接收缓冲区大小 */
#define K210_RX_BUF_SIZE    256   /* 10脸×16+2=162字节, 256留足余量 */

extern K210_FaceData_t g_k210_face;
extern uint8_t k210_rx_buf[K210_RX_BUF_SIZE];
extern volatile uint8_t k210_rx_idx;

void K210_UART_Init(void);
void K210_UART_RxProcess(void);
void K210_ParseFrame(void);

#endif

#ifndef __CLOUD_COMM_H
#define __CLOUD_COMM_H

#include <stdint.h>

/* 初始化云端连接（WiFi + MQTT） */
void Cloud_Init(void);

/* 上传亮度数据到云端（内置20s节流） */
void Cloud_Upload(uint8_t percent);

/* 云端连接状态: 1=已连接, 0=未连接 */
uint8_t Cloud_IsConnected(void);

/* 尝试重连（由主循环调用，内置30s间隔限制） */
void Cloud_TryReconnect(void);

#endif

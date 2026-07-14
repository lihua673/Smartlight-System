#ifndef __WEB_SERVER_H
#define __WEB_SERVER_H
#include <stdint.h>

/* 启动TCP Server (WiFi连接后调用) */
void WebServer_Init(void);

/* 主循环中调用：检测+处理HTTP请求 */
void WebServer_Poll(void);

/* 获取ESP8266 IP地址字符串 */
const char* WebServer_GetIP(void);

#endif

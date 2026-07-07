#ifndef __ESP8266_H__
#define __ESP8266_H__
#include "main.h"

// 对外暴露串口接收缓存，供it.c、main.c、其他文件访问
extern uint8_t usart2_rx_buf[];
extern uint16_t usart2_rx_len;
extern UART_HandleTypeDef huart2;
extern uint8_t ESP8266_Init_Success;

void ESP8266_Init(void);
void MQTT_Init(void);
void MQTT_Publish_Data(int light_state);
int8_t MQTT_Get_Data(char *name);
uint8_t ESP_WaitResp(char *wait_str, uint16_t timeout_ms);
void ESP8266_PollReceive(void);

#endif

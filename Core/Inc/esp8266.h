#ifndef __ESP8266_H__
#define __ESP8266_H__
#include "main.h"

extern UART_HandleTypeDef huart2;

void ESP8266_Init(void);
uint8_t MQTT_Init(void);   /* 返回1=成功, 0=失败 */
void MQTT_Publish_Data(int light_state);
int8_t MQTT_Get_Data(char *name);
uint8_t ESP_WaitResp(char *wait_str, uint16_t timeout_ms);
void ESP8266_PollReceive(void);

#endif

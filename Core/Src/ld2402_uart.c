#include "app_state.h"
#include "ld2402_uart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "led_pwm.h"
#define RX_TIMEOUT_MS  180   // 超过180ms无新字节视为帧结束

extern UART_HandleTypeDef huart3;

// 接收缓冲区
uint8_t rx_buf[64];
uint8_t rx_idx;
volatile uint32_t rx_last_time;   // 最后一次收到字节的时间戳

void LD2402_UART_Init(void)
{
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_idx = 0;
		rx_last_time = HAL_GetTick();
    ld24_data.human_state = HUMAN_NONE;
    ld24_data.distance = 0;
    // 启动单次字节中断接收
    HAL_UART_Receive_IT(&huart3, rx_buf, 1);
}

// 串口接收中断回调
void LD2402_UART_RxProcess(void)
{
	rx_last_time = HAL_GetTick();       // 更新最后接收时间
	rx_idx++;                           // 先递增，表示已成功存入一个字节
	if(rx_idx >= sizeof(rx_buf))        // 防溢出（保留最后一个位置给'\0'）
	{
			rx_idx = 0;                     // 环形覆盖或清空（这里采用清空从头开始）
			memset(rx_buf, 0, sizeof(rx_buf));
	}
	// 启动下一次接收，使用当前索引位置
	HAL_UART_Receive_IT(&huart3, &rx_buf[rx_idx], 1);

}

void LD2402_ParseData(void)
{
    if(rx_idx == 0) return;
    
    uint32_t now = HAL_GetTick();
    if((now - rx_last_time) < RX_TIMEOUT_MS)
    {
        return;   // 还在接收中，不处理
    }
    
    // 补字符串结束符
    rx_buf[rx_idx] = '\0';
    
    // 识别报文（只检测关键字，不关心长度）
    if(strstr((char*)rx_buf, "distance") != NULL)
    {
        ld24_data.human_state = HUMAN_MOVE;
        // 可以进一步提取距离数值，例如：
        char *p = strstr((char*)rx_buf, "distance: ");
        if(p) {
            ld24_data.distance = atoi(p + 10); // 跳过"distance: "
        }
    }
    else if(strstr((char*)rx_buf, "OFF") != NULL)
    {
        ld24_data.human_state = HUMAN_NONE;
        ld24_data.distance = 0;
    }
    
    // 解析完清空缓存
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_idx = 0;
}

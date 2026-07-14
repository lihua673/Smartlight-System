#include "cloud_comm.h"
#include "esp8266.h"
#include "app_state.h"
#include <string.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* 初始化WiFi → WebServer TCP → MQTT */
void Cloud_Init(void)
{
    /* 步骤1: WiFi连接 */
    ESP8266_Init();
    if (!ESP8266_Init_Success) return;

    /* ── 步骤2: 启动Web控制面板TCP Server (端口80) ──
     * 必须在MQTT之前启动，避免socket冲突
     */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, 128);
    HAL_UART_AbortReceive_IT(&huart2);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);
    {
        const char *cmd = "AT+CIPSERVER=1,80\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    }
    if (ESP_WaitResp("OK", 3000))
    {
        HAL_UART_Transmit(&huart1,
            (uint8_t *)"\r\n=== WebServer :80 OK ===\r\n",
            strlen("\r\n=== WebServer :80 OK ===\r\n"),
            HAL_MAX_DELAY);
    }
    else
    {
        /* CIPSERVER失败不代表系统不可用，MQTT仍可正常工作 */
        HAL_UART_Transmit(&huart1,
            (uint8_t *)"WebServer:80 FAIL (MQTT only mode)\r\n",
            strlen("WebServer:80 FAIL (MQTT only mode)\r\n"),
            HAL_MAX_DELAY);
    }
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, 256);
    HAL_UART_AbortReceive_IT(&huart2);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);

    /* 步骤3: MQTT连接 */
    if (MQTT_Init())
        g_app.mqtt_ready = 1;
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

        /* 重新走完整初始化流程 */
        ESP8266_Init();
        if (ESP8266_Init_Success)
        {
            /* 重连时也需要重启CIPSERVER */
            USART2_RX_LEN = 0;
            memset(USART2_RX_BUF, 0, 128);
            HAL_UART_AbortReceive_IT(&huart2);
            HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);
            {
                const char *cmd = "AT+CIPSERVER=1,80\r\n";
                HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
            }
            ESP_WaitResp("OK", 2000);
            USART2_RX_LEN = 0;
            memset(USART2_RX_BUF, 0, 256);
            HAL_UART_AbortReceive_IT(&huart2);
            HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);

            if (MQTT_Init())
                g_app.mqtt_ready = 1;
        }
    }
}

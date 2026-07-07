#include "stm32f4xx_it.h"
#include <stdio.h>
#include "esp8266.h"
#include <string.h>
#include "OLED.h"
#include "led_pwm.h"
#include "wifi_config.h"
#include "app_state.h"
#include "delay.h"



extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// 安全的串口字符串发送函数（使用strlen替代不安全的sizeof宏）
static inline void UART_SendStr(UART_HandleTypeDef *huart, const char *str) {
    HAL_UART_Transmit(huart, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

/* ESP8266初始化 */
void ESP8266_Init()
{
    HAL_Delay(500);

    /* 1: AT测试 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart2, "AT\r\n");

    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "AT测试正常\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "AT测试失败\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    /* 2: 模块复位 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart2, "AT+RST\r\n");
    UART_SendStr(&huart1, "模块复位\r\n");
    OLED_Clear();
    OLED_ShowString(1, 1, "AT+RST");
    HAL_Delay(1000);

    /* 3: 开启STA模式 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart2, "AT+CWMODE=1\r\n");
    if (ESP_WaitResp("OK", 3000))
    {
        UART_SendStr(&huart1, "开启STA模式成功\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "CWMODE OK");
    }
    else
    {
        UART_SendStr(&huart1, "开启STA模式失败\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    /* 4: 连接网络 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    {
        char cmd_buf[128];
        snprintf(cmd_buf, sizeof(cmd_buf),
            "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
        HAL_UART_Transmit(&huart2, (uint8_t*)cmd_buf, strlen(cmd_buf), HAL_MAX_DELAY);
    }

    uint8_t conn_flag = 0;
    uint8_t ip_flag = 0;
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < 10000)  // 10秒超时
    {
        ESP8266_PollReceive();
        if (strstr(USART2_RX_BUF, "WIFI CONNECTED") != NULL) conn_flag = 1;
        if (strstr(USART2_RX_BUF, "WIFI GOT IP") != NULL) ip_flag = 1;
        if (conn_flag && ip_flag) break;
    }
    UART_SendStr(&huart1, "连接网络\r\n");
    OLED_Clear();
    OLED_ShowString(1, 1, "AT+CWJAP");
    if (conn_flag && ip_flag)
    {
        UART_SendStr(&huart1, "网络连接成功\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "网络连接失败\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    ESP8266_Init_Success = 1;
    HAL_Delay(2000);
}

/* MQTT初始化函数 */
void MQTT_Init()
{
    /* 1: 设置用户属性 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart1, "设置用户属性 \r\n");
    Delay_us(200);
    UART_SendStr(&huart2,
        "AT+MQTTUSERCFG=0,1,\"mytest\",\"ZIJxyYhSR6\",\"version=2018-10-31&res=products%2FZIJxyYhSR6%2Fdevices%2Fmytest&et=1909135998&method=md5&sign=v4Y%2B%2BoPTIzzRC%2BH8U4g%2Bnw%3D%3D\",0,0,\"\"\r\n");

    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "设置用户属性成功 \r\n");
        Delay_us(200);
        OLED_Clear();
        OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "设置用户属性失败 \r\n");
        Delay_us(200);
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    /* 2: 连接OneNET服务器 */
    HAL_Delay(3000);
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart1, "连接OneNET服务器 \r\n");
    Delay_us(200);
    UART_SendStr(&huart2, "AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n");

    if (ESP_WaitResp("OK", 8000))
    {
        UART_SendStr(&huart1, "OneNET服务器连接成功 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "OneNET服务器连接失败 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    /* 3: 订阅主题 */
    HAL_Delay(1000);
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart1, "订阅'设备属性上报响应'主题 \r\n");
    UART_SendStr(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/post/reply\",0\r\n");
    Delay_us(200);

    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "订阅成功 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "订阅失败 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

    HAL_Delay(1000);
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SendStr(&huart1, "订阅'设备属性设置请求'主题 \r\n");
    UART_SendStr(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/set\",0\r\n");

    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "订阅成功 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "AT+MQTTSUB:set");
    }
    else
    {
        UART_SendStr(&huart1, "订阅失败 \r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "AT+MQTTSUB:ERROR");
        HAL_Delay(500);
        return;
    }
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
}

void MQTT_Publish_Data(int light_state)
{
    char data[128];
    snprintf(data, sizeof(data),
        "{\"id\":\"123456\",\"params\":{\"light_state\":{\"value\":%d}}}", light_state);
    uint16_t data_len = strlen(data);
    char at_cmd[128];
    snprintf(at_cmd, sizeof(at_cmd),
        "AT+MQTTPUBRAW=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/post\",%d,0,0\r\n", data_len);

    UART_SendStr(&huart1, "准备向云平台发送数据 \r\n");

    HAL_UART_Transmit(&huart2, (uint8_t *)at_cmd, strlen(at_cmd), HAL_MAX_DELAY);
    Delay_us(200);
    if (ESP_WaitResp(">", 6000))
    {
        UART_SendStr(&huart1, "准备发送数据成功 \r\n");
        HAL_Delay(5);
        Delay_us(200);
    }
    else
    {
        UART_SendStr(&huart1, "准备发送数据失败 \r\n");
        HAL_Delay(500);
        return;
    }

    // 清空缓冲区，保证数据干净
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));

    UART_SendStr(&huart1, "正在发送数据 \r\n");
    Delay_us(200);
    HAL_UART_Transmit(&huart2, (uint8_t *)data, data_len, HAL_MAX_DELAY);

    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "数据上传成功 \r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)data, data_len, HAL_MAX_DELAY);
    }
    else
    {
        UART_SendStr(&huart1, "数据上传失败 \r\n");
        HAL_Delay(500);
        return;
    }
}

/**
 * @brief 从 USART2 缓冲区中提取指定字段的数值
 * @param name 要匹配的字段名，如 "light_state"
 * @return 提取到的数值，如果没有找到则返回 -1
 */
int8_t MQTT_Get_Data(char *name)
{
    char *p_name = strstr(USART2_RX_BUF, name);
    if (p_name == NULL)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
        return -1;
    }
    char *p_value = strstr(p_name, "\"value\":");
    if (p_value == NULL)
    {
        return -1;
    }

    // 数据完整，打印并解析
    UART_SendStr(&huart1, "成功接收到数据: \r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
    Delay_us(20);

    p_value += 8;  // 跳过 "value":
    while (*p_value == ' ' || *p_value == '\t')
    {
        p_value++;
    }
    uint8_t result = 0;
    while (*p_value >= '0' && *p_value <= '9')
    {
        result = result * 10 + (*p_value - '0');
        p_value++;
    }
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    return result;
}

/* 轮询接收函数：从USART2硬件寄存器读取数据到缓冲区 */
void ESP8266_PollReceive(void)
{
    uint8_t temp;

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
    {
        temp = (uint8_t)(huart2.Instance->DR & 0xFF);

        if (USART2_RX_LEN < (sizeof(USART2_RX_BUF) - 1))
        {
            USART2_RX_BUF[USART2_RX_LEN++] = temp;
            USART2_RX_BUF[USART2_RX_LEN] = '\0';
        }
    }
}

/* 轮询检测函数：等待指定字符串出现或超时 */
uint8_t ESP_WaitResp(char *wait_str, uint16_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        ESP8266_PollReceive();

        if (strstr(USART2_RX_BUF, wait_str) != NULL)
        {
            return 1;
        }
    }
    // 超时打印原始缓冲区，方便调试
    char debug[128];
    snprintf(debug, sizeof(debug), "Wait %s timeout, buf:%s...", wait_str, USART2_RX_BUF);
    HAL_UART_Transmit(&huart1, (uint8_t*)debug, strlen(debug), HAL_MAX_DELAY);
    return 0;
}

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

/* 安全的串口字符串发送函数 */
static inline void UART_SendStr(UART_HandleTypeDef *huart, const char *str) {
    HAL_UART_Transmit(huart, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

/* 清UART2缓冲并重新校准IT接收指针（先中止再重开，避免BUSY） */
static inline void UART2_ClrBuf(void) {
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, 256);
    HAL_UART_AbortReceive_IT(&huart2);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);
}

/* ═══════════════════════════════════════════════════════════════
 *  UART2 中断接收回调
 *  每收到1字节自动存入USART2_RX_BUF，无需主循环轮询
 *  （与LD2402/K210相同的IT接收模式）
 * ═══════════════════════════════════════════════════════════════ */
void ESP8266_UART_RxProcess(void)
{
    if (USART2_RX_LEN < 1023)
    {
        USART2_RX_LEN++;
        USART2_RX_BUF[USART2_RX_LEN] = '\0';
    }
    /* 重新启动IT接收，下一字节存入下一位置 */
    HAL_UART_Receive_IT(&huart2,
        (uint8_t *)&USART2_RX_BUF[USART2_RX_LEN], 1);
}

/* ESP8266初始化 (WiFi连接 + 多连接模式)
 * 流程: 硬件复位 → 启动IT接收 → AT测试 → STA模式 → WiFi连接
 *       → 提取IP → CIPMUX=1 */
void ESP8266_Init()
{
    /* 硬件复位 */
    HAL_GPIO_WritePin(ESP_CH_PD_GPIO_Port, ESP_CH_PD_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ESP_RST_GPIO_Port, ESP_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(ESP_CH_PD_GPIO_Port, ESP_CH_PD_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(ESP_RST_GPIO_Port, ESP_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);

    /* ── 启动UART2中断接收 ──
     * 此后所有ESP8266发来的字节都由ISR自动捕获到USART2_RX_BUF
     * ESP_WaitResp只需监控缓冲，不再手动读DR */
    HAL_UART_Receive_IT(&huart2,
        (uint8_t *)&USART2_RX_BUF[0], 1);

    /* 1: AT测试 */
    UART2_ClrBuf();
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
    UART2_ClrBuf();
    UART_SendStr(&huart2, "AT+RST\r\n");
    UART_SendStr(&huart1, "模块复位\r\n");
    OLED_Clear();
    OLED_ShowString(1, 1, "AT+RST");
    HAL_Delay(1000);
    /* RST后重新启动IT接收（ESP8266重启期间UART可能收到乱码，清掉） */
    UART2_ClrBuf();

    /* 3: STA模式 */
    UART2_ClrBuf();
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

    /* 4: 连接WiFi */
    UART2_ClrBuf();
    {
        char cmd_buf[128];
        snprintf(cmd_buf, sizeof(cmd_buf),
            "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
        HAL_UART_Transmit(&huart2, (uint8_t*)cmd_buf, strlen(cmd_buf), HAL_MAX_DELAY);
    }
    {
        uint8_t conn_flag = 0, ip_flag = 0;
        uint32_t start_tick = HAL_GetTick();
        while ((HAL_GetTick() - start_tick) < 10000)
        {
            if (strstr(USART2_RX_BUF, "WIFI CONNECTED")) conn_flag = 1;
            if (strstr(USART2_RX_BUF, "WIFI GOT IP"))    ip_flag = 1;
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
    }

    /* ── 4.5: WiFi连接成功后立即提取IP（此时缓冲干净，无CIPSERVER/MQTT干扰）── */
    {
        UART2_ClrBuf();
        UART_SendStr(&huart2, "AT+CIFSR\r\n");
        if (ESP_WaitResp("OK", 3000))
        {
            char *ip = strstr(USART2_RX_BUF, "STAIP,\"");
            if (ip)
            {
                ip += 7;
                uint8_t i = 0;
                while (*ip && *ip != '"' && i < 15)
                    g_app.esp_ip[i++] = *ip++;
                g_app.esp_ip[i] = '\0';
            }
        }
        /* 打印IP到串口，方便用户查看 */
        {
            char msg[80];
            snprintf(msg, sizeof(msg),
                "\r\n========================================\r\n"
                "  ESP8266 IP: %s\r\n"
                "  手机连同一WiFi, 浏览器打开 http://%s\r\n"
                "========================================\r\n",
                g_app.esp_ip[0] ? g_app.esp_ip : "获取失败",
                g_app.esp_ip[0] ? g_app.esp_ip : "???");
            HAL_UART_Transmit(&huart1, (uint8_t *)msg,
                strlen(msg), HAL_MAX_DELAY);
        }
        UART2_ClrBuf();
    }

    /* 5: 开启多连接模式 (CIPSERVER + MQTT 共用) */
    UART2_ClrBuf();
    UART_SendStr(&huart2, "AT+CIPMUX=1\r\n");
    if (ESP_WaitResp("OK", 3000))
        UART_SendStr(&huart1, "CIPMUX=1 OK\r\n");
    UART2_ClrBuf();

    ESP8266_Init_Success = 1;
    HAL_Delay(2000);
}

/* MQTT初始化 */
uint8_t MQTT_Init()
{
    /* 1: 用户属性 */
    UART2_ClrBuf();
    UART_SendStr(&huart1, "设置用户属性 \r\n");
    Delay_us(200);
    UART_SendStr(&huart2,
        "AT+MQTTUSERCFG=0,1,\"mytest\",\"ZIJxyYhSR6\",\"version=2018-10-31&res=products%2FZIJxyYhSR6%2Fdevices%2Fmytest&et=1909135998&method=md5&sign=pF1rIkPvt01A6fEi6bzJMA%3D%3D\",0,0,\"\"\r\n");
    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "设置用户属性成功 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "设置用户属性失败 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500); return 0;
    }

    /* 2: 连接OneNET */
    HAL_Delay(3000);
    UART2_ClrBuf();
    UART_SendStr(&huart1, "连接OneNET服务器 \r\n");
    Delay_us(200);
    UART_SendStr(&huart2, "AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n");
    if (ESP_WaitResp("OK", 8000))
    {
        UART_SendStr(&huart1, "OneNET服务器连接成功 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "OneNET服务器连接失败 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500); return 0;
    }

    /* 3: 订阅主题 */
    HAL_Delay(1000);
    UART2_ClrBuf();
    UART_SendStr(&huart1, "订阅'设备属性上报响应'主题 \r\n");
    UART_SendStr(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/post/reply\",0\r\n");
    Delay_us(200);
    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "订阅成功 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "OK");
    }
    else
    {
        UART_SendStr(&huart1, "订阅失败 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500); return 0;
    }

    HAL_Delay(1000);
    UART2_ClrBuf();
    UART_SendStr(&huart1, "订阅'设备属性设置请求'主题 \r\n");
    UART_SendStr(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/set\",0\r\n");
    if (ESP_WaitResp("OK", 6000))
    {
        UART_SendStr(&huart1, "订阅成功 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "AT+MQTTSUB:set");
    }
    else
    {
        UART_SendStr(&huart1, "订阅失败 \r\n");
        OLED_Clear(); OLED_ShowString(1, 1, "AT+MQTTSUB:ERROR");
        HAL_Delay(500); return 0;
    }

    UART2_ClrBuf();
    return 1;
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
        HAL_Delay(5); Delay_us(200);
    }
    else
    {
        UART_SendStr(&huart1, "准备发送数据失败 \r\n");
        HAL_Delay(500); return;
    }

    UART2_ClrBuf();
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
    }
}

int8_t MQTT_Get_Data(char *name)
{
    char *p_name = strstr(USART2_RX_BUF, name);
    if (!p_name)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
        return -1;
    }
    char *p_value = strstr(p_name, "\"value\":");
    if (!p_value) return -1;

    UART_SendStr(&huart1, "成功接收到数据: \r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
    Delay_us(20);

    p_value += 8;
    while (*p_value == ' ' || *p_value == '\t') p_value++;
    uint8_t result = 0;
    while (*p_value >= '0' && *p_value <= '9')
    {
        result = result * 10 + (*p_value - '0');
        p_value++;
    }
    UART2_ClrBuf();
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 *  ESP_WaitResp: 等待ESP8266响应
 *  策略：临时关闭IT → 轮询读DR → 成功后重开IT
 *  这样AT命令期间不丢字节，正常运行期间IT捕获+IPD
 * ═══════════════════════════════════════════════════════════════ */
uint8_t ESP_WaitResp(char *wait_str, uint16_t timeout_ms)
{
    /* 临时关闭IT接收，切换到轮询模式 */
    HAL_UART_AbortReceive_IT(&huart2);

    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        /* 手动读DR（不依赖ISR），逐字节存入缓冲 */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
        {
            uint8_t ch = (uint8_t)(huart2.Instance->DR & 0xFF);
            if (USART2_RX_LEN < 1023)
            {
                USART2_RX_BUF[USART2_RX_LEN++] = ch;
                USART2_RX_BUF[USART2_RX_LEN] = '\0';
            }
        }
        /* 清除可能的UART错误标志(ORE/FE)，防止锁死 */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET ||
            __HAL_UART_GET_FLAG(&huart2, UART_FLAG_FE)  != RESET)
        {
            volatile uint32_t sr = huart2.Instance->SR;
            volatile uint32_t dr = huart2.Instance->DR;
            (void)sr; (void)dr;
        }
        if (strstr(USART2_RX_BUF, wait_str))
        {
            /* 成功：重开IT接收 */
            HAL_UART_Receive_IT(&huart2,
                (uint8_t *)&USART2_RX_BUF[USART2_RX_LEN], 1);
            return 1;
        }
    }

    /* 超时：重开IT接收 */
    HAL_UART_Receive_IT(&huart2,
        (uint8_t *)&USART2_RX_BUF[USART2_RX_LEN], 1);

    char debug[128];
    snprintf(debug, sizeof(debug), "Wait %s timeout, buf:%s...", wait_str, USART2_RX_BUF);
    HAL_UART_Transmit(&huart1, (uint8_t*)debug, strlen(debug), HAL_MAX_DELAY);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  ESP8266_PollReceive: 轮询读UART2数据寄存器
 *  （仅在ESP_WaitResp关闭IT后使用，避免与ISR冲突）
 * ═══════════════════════════════════════════════════════════════ */
void ESP8266_PollReceive(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)(huart2.Instance->DR & 0xFF);
        if (USART2_RX_LEN < 1023)
        {
            USART2_RX_BUF[USART2_RX_LEN++] = ch;
            USART2_RX_BUF[USART2_RX_LEN] = '\0';
        }
    }
}

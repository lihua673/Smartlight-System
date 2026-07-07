#include "stm32f4xx_it.h"
#include <stdio.h>
#include "esp8266.h"
#include <string.h>
#include "OLED.h"
#include "led_pwm.h"

extern char USART1_RX_BUF[1024];
extern uint16_t USART1_RX_LEN;
extern uint8_t USART1_RX_FINISH;

extern char USART2_RX_BUF[1024];
extern uint16_t USART2_RX_LEN;
extern uint8_t USART2_RX_FINISH;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

extern void HAL_Delay_us(uint32_t us);

#define UART_SEND(huart, str) HAL_UART_Transmit(huart, (uint8_t*)str, sizeof(str)-1, HAL_MAX_DELAY)
// 调用


// 全局变量


uint8_t ESP8266_Init_Success=0;//esp-01s初始化成功标志位


//回调
#define USART2_RX_RxCpltCallback  128
uint8_t usart2_rx_buf[USART2_RX_RxCpltCallback]; // 接收缓冲区
uint16_t usart2_rx_len = 0;           // 实际收到字节数
// 串口接收完成回调
//void ESP8266_UART_RxProcess(void)
//{
//    // 写入前判断剩余空间
//    if(USART2_RX_LEN >= (sizeof(USART2_RX_BUF)-2))
//    {
//        USART2_RX_LEN = 0;
//        memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
//    }
//    // 存入字节并补0
//    USART2_RX_BUF[USART2_RX_LEN++] = usart2_rx_buf[0];
//    USART2_RX_BUF[USART2_RX_LEN] = '\0';
//    
//    // 帧结束标记（仅标记，不处理）
//    if(usart2_rx_buf[0] == '\n' || usart2_rx_buf[0] == '\r')
//    {
//        USART2_RX_FINISH = 1;
//    }
//    // 重新开启中断
//    HAL_UART_Receive_IT(&huart2, usart2_rx_buf, 1);
//}

/*ESP8266初始化
*/
void ESP8266_Init()
{
	//串口一和二的初始化
	HAL_Delay(500);
	
	/*1: AT测试 */
USART2_RX_LEN = 0;
memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
UART_SEND(&huart2, "AT\r\n");
// 不要放任何东西在这里！立即进入 WaitResp

if (ESP_WaitResp("OK", 6000))
{
    UART_SEND(&huart1, "AT测试正常\r\n");
    OLED_Clear();          // ← 移到这里
    OLED_ShowString(1, 1, "OK");  // ← 移到这里
}
else
{
    UART_SEND(&huart1, "AT测试失败\r\n");
    OLED_Clear();          // ← 失败时也移到这里
    OLED_ShowString(1, 1, "ERROR");
    HAL_Delay(500);
    return;
}

	
	/*2:模块复位*/
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart2, "AT+RST\r\n");
	UART_SEND(&huart1, "模块复位\r\n");
	OLED_Clear();
	OLED_ShowString(1,1,"AT+RST");
	HAL_Delay(1000);
	
	/*3: 开启STA模式 — 需要等待回复！*/
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));
    UART_SEND(&huart2, "AT+CWMODE=1\r\n");
    // 立即进入 WaitResp，中间不要有任何打印或延迟
    if (ESP_WaitResp("OK", 3000))
    {
        UART_SEND(&huart1, "开启STA模式成功\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "CWMODE OK");
    }
    else
    {
        UART_SEND(&huart1, "开启STA模式失败\r\n");
        OLED_Clear();
        OLED_ShowString(1, 1, "ERROR");
        HAL_Delay(500);
        return;
    }

	
	/*4:连接网络*/
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart2, "AT+CWJAP=\""WIFI_NAME"\",\""WIFI_PASSWORD"\"\r\n");
	
	uint8_t conn_flag = 0;
    uint8_t ip_flag = 0;
    uint32_t start_tick = HAL_GetTick();   // ★ 使用 HAL_GetTick

	while ((HAL_GetTick() - start_tick) < 10000)  // 10秒超时
	{
			ESP8266_PollReceive();
			if (strstr(USART2_RX_BUF, "WIFI CONNECTED") != NULL) conn_flag = 1;
			if (strstr(USART2_RX_BUF, "WIFI GOT IP") != NULL) ip_flag = 1;
			if (conn_flag && ip_flag) break;
			// 不加任何 HAL_Delay
	}
	UART_SEND(&huart1, "连接网络\r\n");
	OLED_Clear();
	OLED_ShowString(1,1,"AT+CWJAP");
	//UART_SEND(&huart1, USART2_RX_BUF);
	if (conn_flag && ip_flag)
	{
		UART_SEND(&huart1, "网络连接成功\r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"OK");
	}
	else
	{
		UART_SEND(&huart1, "网络连接失败\r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"ERROR");
		HAL_Delay(500);
		return;
	}
	
	ESP8266_Init_Success=1;
	HAL_Delay(2000);
}

/*MQTT初始化函数
*/
void MQTT_Init()
{
	/*1:设置用户属性*/
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart1, "设置用户属性 \r\n");
	HAL_Delay_us(200);
	UART_SEND(&huart2,
	"AT+MQTTUSERCFG=0,1,\"mytest\",\"ZIJxyYhSR6\",\"version=2018-10-31&res=products%2FZIJxyYhSR6%2Fdevices%2Fmytest&et=1909135998&method=md5&sign=v4Y%2B%2BoPTIzzRC%2BH8U4g%2Bnw%3D%3D\",0,0,\"\"\r\n");
	
//	OLED_Clear();
//	OLED_ShowString(1,1,"AT+MQTTUSERCFG");
	
	if (ESP_WaitResp("OK",6000))
	{
		UART_SEND(&huart1, "设置用户属性成功 \r\n");
		HAL_Delay_us(200);
		OLED_Clear();
		OLED_ShowString(1,1,"OK");
	}
	else
	{
		UART_SEND(&huart1, "设置用户属性失败 \r\n");
		HAL_Delay_us(200);
		OLED_Clear();
		OLED_ShowString(1,1,"ERROR");
		HAL_Delay(500);
		return;
	}
	
	/*2:连接OneNET服务器*/
	HAL_Delay(3000);
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart1, "连接OneNET服务器 \r\n");
	HAL_Delay_us(200);
	UART_SEND(&huart2, "AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n");
	
//	OLED_Clear();
//	OLED_ShowString(1,1,"AT+MQTTCONN");
	
	if (ESP_WaitResp("OK",8000))
	{
		UART_SEND(&huart1, "OneNET服务器连接成功 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"OK");
	}
	else
	{
		UART_SEND(&huart1, "OneNET服务器连接失败 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"ERROR");
		HAL_Delay(500);
		return;
	}
	
	/*3:订阅主题*/
	HAL_Delay(1000);
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart1, "订阅“设备属性上报响应”主题 \r\n");
	UART_SEND(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/post/reply\",0\r\n");
	HAL_Delay_us(200);
//	OLED_Clear();
//	OLED_ShowString(1,1,"AT+MQTTSUB:reply");
	
	if (ESP_WaitResp("OK",6000))
	{
		UART_SEND(&huart1, "订阅成功 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"OK");
	}
	else
	{
		UART_SEND(&huart1, "订阅失败 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"ERROR");
		HAL_Delay(500);
		return;
	}
	
	HAL_Delay(1000);
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	UART_SEND(&huart1, "订阅“设备属性设置请求”主题 \r\n");
	UART_SEND(&huart2, "AT+MQTTSUB=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/set\",0\r\n");
	
	if (ESP_WaitResp("OK",6000))
	{
		UART_SEND(&huart1, "订阅成功 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"AT+MQTTSUB:set");
	}
	else
	{
		UART_SEND(&huart1, "订阅失败 \r\n");
		OLED_Clear();
		OLED_ShowString(1,1,"AT+MQTTSUB:ERROR");
		HAL_Delay(500);
		return;
	}
	USART2_RX_LEN=0;
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
}

void MQTT_Publish_Data(int light_state)
{
	char data[128];
	snprintf(data,sizeof(data),"{\"id\":\"123456\",\"params\":{\"light_state\":{\"value\":%d}}}",light_state);
	uint16_t data_len=strlen(data);//计算长度，不需要+1
	char at_cmd[128];
	snprintf(at_cmd,sizeof(at_cmd),"AT+MQTTPUBRAW=0,\"$sys/ZIJxyYhSR6/mytest/thing/property/post\",%d,0,0\r\n",data_len);
	
//	USART2_RX_LEN=0;
//	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
		
	UART_SEND(&huart1, "准备向云平台发送数据 \r\n");
	
	HAL_UART_Transmit(&huart2, (uint8_t *)at_cmd, strlen(at_cmd), HAL_MAX_DELAY);	//动态缓冲区手动强转，不使用UART_SEND宏
	HAL_Delay_us(200);
	if (ESP_WaitResp(">",6000))
	{
		UART_SEND(&huart1, "准备发送数据成功 \r\n");		HAL_Delay(5);
		HAL_Delay_us(200);
		
	}
	else
	{
		UART_SEND(&huart1, "准备发送数据失败 \r\n");

		HAL_Delay(500);
		return;
	}
	
	USART2_RX_LEN=0;    //保证接下来串口二里面的数据干净只有light。。
	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
	
	UART_SEND(&huart1, "正在发送数据 \r\n");

	HAL_Delay_us(200);
	HAL_UART_Transmit(&huart2, (uint8_t *)data, data_len, HAL_MAX_DELAY);

	if (ESP_WaitResp("OK",6000))
	{
		UART_SEND(&huart1, "数据上传成功 \r\n");

		HAL_UART_Transmit(&huart1, (uint8_t *)data, data_len, HAL_MAX_DELAY);

	}
	else
	{
		UART_SEND(&huart1, "数据上传失败 \r\n");
		HAL_Delay(5);

		HAL_Delay(500);
		return;
	}
	
//	USART2_RX_LEN=0;
//	memset(USART2_RX_BUF,0,sizeof(USART2_RX_BUF));
}
/*接收数据函数
*/
/**
 * @brief 从 USART2 缓冲区中提取指定字段的数值
 * @param name 要匹配的字段名，如 "light_state"
 * @return 提取到的数值，如果没有找到则返回 0
 */
//此函数我想接收缓存区里的light_state中所设置的数值，但是由于云平台下发数据设置led会与本地设置的传感器产生冲突，所以还未实现该功能
int8_t MQTT_Get_Data(char *name)
{
    // 第一步：先检查缓冲区里有没有目标字段名（如 "light_state"）
    char *p_name = strstr(USART2_RX_BUF, name);
    if (p_name == NULL)
    {
			HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
        return -1;  // 没找到，直接返回
    }
    // 第二步：在 name 后面找 "value": 这个字符串
    char *p_value = strstr(p_name, "\"value\":");
    if (p_value == NULL)
    {
        return -1;  // 没找到 value 字段，返回 -1
    }
    
		// 只有到这里，才说明数据完整，打印并解析
    UART_SEND(&huart1, "成功接收到数据: \r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)USART2_RX_BUF, strlen(USART2_RX_BUF), HAL_MAX_DELAY);
		HAL_Delay_us(20);
    
		// 第三步：跳过 "value": 这 8 个字符，来到数字起始位置
    p_value += 8;
    // 第四步：跳过可能的空白字符（空格等）
    while (*p_value == ' ' || *p_value == '\t')
    {
        p_value++;
    } 
    // 第五步：把数字字符串转成整数
    uint8_t result = 0;
    while (*p_value >= '0' && *p_value <= '9')  // 只要还是数字字符就继续
    {
        result = result * 10 + (*p_value - '0');  // 逐位累加
        p_value++;
    }       
    // 第七步：清空缓冲区，准备接收下一次
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, sizeof(USART2_RX_BUF));  
    return result;  // 返回提取到的数值
}


// 新增的轮询接收函数
void ESP8266_PollReceive(void)
{
    uint8_t temp;
    
    // 检查USART2的状态寄存器(SR)中的RXNE(接收数据寄存器非空)标志位
    // 当RXNE=1时，表示有新的数据已接收并准备好被读取
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
    {
        // 从数据寄存器(DR)中读取一个字节的数据，这会自动清除RXNE标志位
        temp = (uint8_t)(huart2.Instance->DR & 0xFF);
        
        // 将读取到的数据存入你的全局接收缓冲区
        if (USART2_RX_LEN < (sizeof(USART2_RX_BUF) - 1)) // 确保不会溢出
        {
            USART2_RX_BUF[USART2_RX_LEN++] = temp;
            USART2_RX_BUF[USART2_RX_LEN] = '\0'; // 每次都添加字符串结束符，方便调试
        }
        
    }
}


/*轮询检测函数
*/
uint8_t ESP_WaitResp(char *wait_str,uint16_t timeout_ms)
{
	uint32_t start_tick = HAL_GetTick();   // 记录起始时间
	
	while ((HAL_GetTick() - start_tick) < timeout_ms)  // ★ 用GetTick来做超时判断
    {
        // 紧循环快速读取UART硬件，不要加任何HAL_Delay
        ESP8266_PollReceive();
        
        if (strstr(USART2_RX_BUF, wait_str) != NULL)
        {
            return 1;  // 查找成功
					
        }
		}
	// 超时打印原始缓冲区，方便串口1调试
	char debug[128];
	snprintf(debug, sizeof(debug), "Wait %s timeout, buf:%s...", wait_str, USART2_RX_BUF);
	HAL_UART_Transmit(&huart1, (uint8_t*)debug, strlen(debug), HAL_MAX_DELAY);
	return 0;//超时
}


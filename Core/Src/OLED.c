#include "OLED.h"
#include "OLED_Font.h"
#include "BH1750.h"
/*==================== 引脚配置宏定义 ====================*/
/**
 * @brief 软件I2C引脚电平控制宏
 * OLED SCL时钟引脚：PA2；SDA数据引脚：PA4
 * x=1输出高电平，x=0输出低电平
 */
#define OLED_W_SCL(x)		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define OLED_W_SDA(x)		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

/*==================== 编译期常量查表（替代运行时OLED_Pow） ====================*/
static const uint32_t pow10_table[] = {
	1, 10, 100, 1000, 10000, 100000,
	1000000, 10000000, 100000000, 1000000000
};
static const uint32_t pow16_table[] = {
	0x1, 0x10, 0x100, 0x1000, 0x10000,
	0x100000, 0x1000000, 0x10000000
};
static const uint32_t pow2_table[] = {
	1, 2, 4, 8, 16, 32, 64, 128,
	256, 512, 1024, 2048, 4096, 8192, 16384, 32768
};

/*==================== I2C底层时序函数 ====================*/
/**
  * @brief  软件I2C引脚初始化
  * @param  无
  * @retval 无
  * @note   I2C总线空闲状态要求SCL、SDA全部拉高
  */
void OLED_I2C_Init(void)
{
	HAL_Delay(100); // 屏幕电源稳定等待
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C起始信号
  * @param  无
  * @retval 无
  * @时序  SDA高→SCL高→SDA拉低→SCL拉低
  * @原理 SCL高电平时SDA产生下降沿，代表通讯启动
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	HAL_Delay_us(5);
	OLED_W_SDA(0);
	HAL_Delay_us(5);
	OLED_W_SCL(0);
	HAL_Delay_us(5);
}

/**
  * @brief  I2C停止信号
  * @param  无
  * @retval 无
  * @时序  SDA低→SCL高→SDA拉高
  * @原理 SCL高电平时SDA产生上升沿，代表通讯结束
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	HAL_Delay_us(5);
	OLED_W_SCL(1);
	HAL_Delay_us(5);
	OLED_W_SDA(1);
	HAL_Delay_us(5);
}

/**
  * @brief  I2C发送单字节数据（高位先行）
  * @param  Byte：待发送8位字节
  * @retval 无
  * @note 本驱动忽略从机应答ACK，仅补充时钟脉冲
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	// 循环发送8bit，最高位优先
	for (i = 0; i < 8; i++)
	{
		// 取出当前bit电平赋值给SDA
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		HAL_Delay_us(3);
		OLED_W_SCL(1);	// 时钟高，从机读取数据
		HAL_Delay_us(5);
		OLED_W_SCL(0);	// 时钟低，准备切换下一位
		HAL_Delay_us(3);
	}
	// 额外时钟脉冲，跳过应答阶段
	OLED_W_SCL(1);
	HAL_Delay_us(5);
	OLED_W_SCL(0);
	HAL_Delay_us(3);
}

/*==================== OLED读写底层接口 ====================*/
/**
  * @brief  OLED写入控制命令（单条命令）
  * @param  Command：SSD1306寄存器命令码
  * @retval 无
  * @通讯格式 I2C起始→从机地址0x78→标志0x00(命令)→命令字节→停止
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		// OLED I2C写地址
	OLED_I2C_SendByte(0x00);		// 0x00代表后续为控制命令
	OLED_I2C_SendByte(Command);
	OLED_I2C_Stop();
}

/**
  * @brief  【新增批量接口】连续写入多段显存显示数据
  * @param  Data：待发送数据缓冲区指针
  * @param  Count：需要连续发送的字节总数
  * @retval 无
  * @优势 仅发起一次I2C起始/地址，连续传输大量像素，大幅降低总线开销、刷屏更快
  * @防护 空指针/长度为0直接退出，避免程序异常
  */
void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
	uint8_t i;
	// 安全校验：无效参数直接返回，不执行I2C操作
	if (Data == NULL || Count == 0)
	{
		return;
	}

	OLED_I2C_Start();				// I2C通讯起始信号
	OLED_I2C_SendByte(0x78);		// 发送OLED从机写地址
	OLED_I2C_SendByte(0x40);		// 控制字节0x40：标识后续为显示像素数据
	// 循环批量发送缓冲区所有数据
	for (i = 0; i < Count; i ++)
	{
		OLED_I2C_SendByte(Data[i]);
	}
	OLED_I2C_Stop();				// I2C通讯终止信号
}

/**
  * @brief  兼容旧版接口：单次写入1字节显存数据
  * @param  Data：单字节像素数据
  * @retval 无
  * @note 为兼容原有未批量改造的代码，封装调用批量函数
  */
void OLED_WriteData_Single(uint8_t Data)
{
	OLED_WriteData(&Data, 1);
}

/*==================== OLED屏幕控制函数 ====================*/
/**
  * @brief  设置OLED显存光标位置（页寻址模式）
  * @param  Y：页号0~7，屏幕纵向分为8页，每页8像素行
  * @param  X：列号0~127，屏幕横向128像素
  * @retval 无
  * @原理 SSD1306通过三条命令设置页、列高4位、列低4位
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					// 设置页地址Y
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	// 设置列地址高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			// 设置列地址低4位
}

/**
  * @brief  OLED全屏清屏（已批量优化，性能大幅提升）
  * @param  无
  * @retval 无
  * @优化点 一页128个0x00统一存入缓冲区，一次性批量写入，无需循环128次I2C
  */
void OLED_Clear(void)
{
	uint8_t j;
	// 整页填充灭屏数据0x00
	uint8_t clear_buf[128] = {0};
	// 遍历全部8个页面
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);		// 光标定位到当前页首列
		OLED_WriteData(clear_buf, 128); // 一次性写入整页128字节
	}
}

/*==================== OLED字符、数字显示函数 ====================*/
/**
  * @brief  显示单个8*16点阵ASCII字符（批量发送优化）
  * @param  Line：屏幕文本行 1~4，每行占用2个硬件页(16像素高度)
  * @param  Column：文本列 1~16，每列占用8像素宽度
  * @param  Char：ASCII可见字符（空格~波浪号）
  * @retval 无
  * @优化 一次性发送8字节点阵，减少8次I2C启停，刷新更快
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
	// 取出对应字符8*16点阵数组首地址；字库是const常量，指针必须加const匹配限定符
	const uint8_t *font_ptr = OLED_F8x16[Char - ' '];
	// 计算硬件页坐标、列像素坐标
	uint8_t page = (Line - 1) * 2;
	uint8_t col = (Column - 1) * 8;

	// 1. 显示字符上半8行点阵
	OLED_SetCursor(page, col);
	OLED_WriteData((uint8_t *)font_ptr, 8);
	// 2. 显示字符下半8行点阵
	OLED_SetCursor(page + 1, col);
	OLED_WriteData((uint8_t *)(font_ptr + 8), 8);
}

/**
  * @brief  连续显示字符串
  * @param  Line：起始文本行1~4
  * @param  Column：起始文本列1~16
  * @param  String：以'\0'结尾的ASCII字符串
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  显示无符号十进制数字
  * @param  Line：起始行1~4
  * @param  Column：起始列1~16
  * @param  Number：32位无符号数字 0~4294967295
  * @param  Length：固定显示位数，不足高位补0
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		uint32_t bit_val = Number / pow10_table[Length - i - 1] % 10;
		OLED_ShowChar(Line, Column + i, bit_val + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)
	{
		SingleNumber = Number / pow16_table[Length - i - 1] % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  显示二进制无符号数字
  * @param  Line：起始行1~4
  * @param  Column：起始列1~16
  * @param  Number：16位以内二进制数值
  * @param  Length：固定显示位数1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(Line, Column + i, Number / pow2_table[Length - i - 1] % 2 + '0');
	}
}

/*==================== OLED初始化函数 ====================*/
/**
  * @brief  SSD1306 OLED屏幕初始化
  * @param  无
  * @retval 无
  * @流程 上电延时→I2C引脚初始化→发送一整套配置命令→开启显示→清屏
  */
void OLED_Init(void)
{
	HAL_Delay(100);  // SSD1306上电稳定延时

	OLED_I2C_Init();			//端口初始化

	OLED_WriteCommand(0xAE);	//关闭显示

	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);

	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);

	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);

	OLED_WriteCommand(0x40);	//设置显示开始行

	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置

	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);

	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/倒转显示

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示

	OLED_Clear();				//OLED清屏
}

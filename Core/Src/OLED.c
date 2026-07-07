#include "OLED.h"
#include "OLED_Font.h"
#include "soft_i2c.h"
#include "delay.h"

/* OLED专用I2C总线实例 (PA2=SCL, PA4=SDA) */
static SoftI2C_Bus_t oled_i2c = {
    .scl_port = GPIOA,
    .scl_pin  = GPIO_PIN_2,
    .sda_port = GPIOA,
    .sda_pin  = GPIO_PIN_4,
    .delay_us = 5
};

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

/*==================== OLED读写底层接口 ====================*/
/**
  * @brief  OLED写入控制命令（单条命令）
  * @通讯格式 I2C起始→从机地址0x78→标志0x00(命令)→命令字节→停止
  */
void OLED_WriteCommand(uint8_t Command)
{
    SoftI2C_Start(&oled_i2c);
    SoftI2C_SendByte(&oled_i2c, 0x78);
    SoftI2C_SendByte(&oled_i2c, 0x00);
    SoftI2C_SendByte(&oled_i2c, Command);
    SoftI2C_Stop(&oled_i2c);
}

/**
  * @brief  连续写入多段显存显示数据
  * @优势 仅发起一次I2C起始/地址，连续传输大量像素，大幅降低总线开销
  */
void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
    uint8_t i;
    if (Data == NULL || Count == 0) return;

    SoftI2C_Start(&oled_i2c);
    SoftI2C_SendByte(&oled_i2c, 0x78);
    SoftI2C_SendByte(&oled_i2c, 0x40);
    for (i = 0; i < Count; i++)
    {
        SoftI2C_SendByte(&oled_i2c, Data[i]);
    }
    SoftI2C_Stop(&oled_i2c);
}

/**
  * @brief  兼容旧版接口：单次写入1字节显存数据
  */
void OLED_WriteData_Single(uint8_t Data)
{
    OLED_WriteData(&Data, 1);
}

/*==================== OLED屏幕控制函数 ====================*/
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Clear(void)
{
    uint8_t j;
    uint8_t clear_buf[128] = {0};
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        OLED_WriteData(clear_buf, 128);
    }
}

/*==================== OLED字符、数字显示函数 ====================*/
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    const uint8_t *font_ptr = OLED_F8x16[Char - ' '];
    uint8_t page = (Line - 1) * 2;
    uint8_t col = (Column - 1) * 8;

    OLED_SetCursor(page, col);
    OLED_WriteData((uint8_t *)font_ptr, 8);
    OLED_SetCursor(page + 1, col);
    OLED_WriteData((uint8_t *)(font_ptr + 8), 8);
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        uint32_t bit_val = Number / pow10_table[Length - i - 1] % 10;
        OLED_ShowChar(Line, Column + i, bit_val + '0');
    }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / pow16_table[Length - i - 1] % 16;
        if (SingleNumber < 10)
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        else
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / pow2_table[Length - i - 1] % 2 + '0');
    }
}

/*==================== OLED初始化函数 ====================*/
void OLED_Init(void)
{
    HAL_Delay(100);  // SSD1306上电稳定延时

    // I2C引脚初始化
    SoftI2C_Init(&oled_i2c);

    OLED_WriteCommand(0xAE);  // 关闭显示
    OLED_WriteCommand(0xD5);  // 设置显示时钟分频比/振荡器频率
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);  // 设置多路复用率
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);  // 设置显示偏移
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);  // 设置显示开始行
    OLED_WriteCommand(0xA1);  // 设置左右方向
    OLED_WriteCommand(0xC8);  // 设置上下方向
    OLED_WriteCommand(0xDA);  // 设置COM引脚硬件配置
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);  // 设置对比度控制
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);  // 设置预充电周期
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);  // 设置VCOMH取消选择级别
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);  // 设置整个显示打开/关闭
    OLED_WriteCommand(0xA6);  // 设置正常/倒转显示
    OLED_WriteCommand(0x8D);  // 设置充电泵
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);  // 开启显示

    OLED_Clear();
}

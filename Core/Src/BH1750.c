#include "BH1750.h"
#include "soft_i2c.h"
#include "delay.h"

typedef unsigned char BYTE;

/* BH1750专用I2C总线实例 (PB6=SCL, PB7=SDA) */
static SoftI2C_Bus_t bh1750_i2c = {
    .scl_port = GPIOB,
    .scl_pin  = GPIO_PIN_6,
    .sda_port = GPIOB,
    .sda_pin  = GPIO_PIN_7,
    .delay_us = 5
};

/* 通过统一I2C驱动发送单字节地址 */
void Single_Write_BH1750(uchar REG_Address)
{
    SoftI2C_Start(&bh1750_i2c);
    SoftI2C_SendByte(&bh1750_i2c, BHAddWrite);
    SoftI2C_SendByte(&bh1750_i2c, REG_Address);
    SoftI2C_Stop(&bh1750_i2c);
}

void BH1750_Init(void)
{
    Single_Write_BH1750(0x01);
    bh_data_send(BHPowOn);   // BH1750上电
    bh_data_send(BHReset);   // BH1750复位
    bh_data_send(BHModeH2);  // BH1750高分辨率模式2: 0.5lx, 120ms
}

/* 发送命令（带重试保护，防止传感器故障时死循环） */
void bh_data_send(uint8_t command)
{
    uint8_t retry = 0;
    do {
        SoftI2C_Start(&bh1750_i2c);
        SoftI2C_SendByte(&bh1750_i2c, BHAddWrite);
        if (SoftI2C_WaitAck(&bh1750_i2c) == 0) {
            SoftI2C_SendByte(&bh1750_i2c, command);
            SoftI2C_WaitAck(&bh1750_i2c);
            SoftI2C_Stop(&bh1750_i2c);
            return;
        }
        SoftI2C_Stop(&bh1750_i2c);
        retry++;
    } while (retry < 3);
}

/* 读取光照数据（带重试保护） */
uint16_t bh_data_read(void)
{
    uint16_t buf;
    uint8_t retry = 0;

    do {
        SoftI2C_Start(&bh1750_i2c);
        SoftI2C_SendByte(&bh1750_i2c, BHAddRead);
        if (SoftI2C_WaitAck(&bh1750_i2c) == 0) {
            buf = SoftI2C_ReadByte(&bh1750_i2c, 1);
            buf = buf << 8;
            buf += 0x00FF & SoftI2C_ReadByte(&bh1750_i2c, 0);
            SoftI2C_Stop(&bh1750_i2c);
            return buf;
        }
        SoftI2C_Stop(&bh1750_i2c);
        retry++;
    } while (retry < 3);

    return 0;  // 读取失败返回0
}

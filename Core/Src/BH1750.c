#include "BH1750.h"
#include "soft_i2c.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>

/* BH1750专用I2C总线实例 (PB6=SCL, PB7=SDA) */
static SoftI2C_Bus_t bh1750_i2c = {
    .scl_port = GPIOB,
    .scl_pin  = GPIO_PIN_6,
    .sda_port = GPIOB,
    .sda_pin  = GPIO_PIN_7,
    .delay_us = 5
};

extern UART_HandleTypeDef huart1;

/* 写单字节寄存器（带ACK时钟，返回0=成功） */
static uint8_t Single_Write_BH1750_ex(uchar REG_Address)
{
    SoftI2C_Start(&bh1750_i2c);
    SoftI2C_SendByte(&bh1750_i2c, BHAddWrite);
    if (SoftI2C_WaitAck(&bh1750_i2c) != 0) { SoftI2C_Stop(&bh1750_i2c); return 1; }
    SoftI2C_SendByte(&bh1750_i2c, REG_Address);
    if (SoftI2C_WaitAck(&bh1750_i2c) != 0) { SoftI2C_Stop(&bh1750_i2c); return 1; }
    SoftI2C_Stop(&bh1750_i2c);
    return 0;
}

/* 写单字节寄存器（带ACK时钟）—— 旧接口保留兼容 */
void Single_Write_BH1750(uchar REG_Address)
{
    Single_Write_BH1750_ex(REG_Address);
}

void BH1750_Init(void)
{
    char dbg[64];
    SoftI2C_Init(&bh1750_i2c);

    /* 诊断：测试BH1750从机是否存在 */
    {
        SoftI2C_Start(&bh1750_i2c);
        SoftI2C_SendByte(&bh1750_i2c, BHAddWrite);
        uint8_t ack = SoftI2C_WaitAck(&bh1750_i2c);
        SoftI2C_Stop(&bh1750_i2c);
        if (ack == 0) {
            snprintf(dbg, sizeof(dbg), "BH1750: ACK OK (0x46)\r\n");
        } else {
            snprintf(dbg, sizeof(dbg), "BH1750: NO ACK! Check wiring PB6/PB7\r\n");
        }
        HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);
    }

    if (Single_Write_BH1750_ex(0x01) == 0) {
        snprintf(dbg, sizeof(dbg), "BH1750: PowerOn OK\r\n");
    } else {
        snprintf(dbg, sizeof(dbg), "BH1750: PowerOn FAIL\r\n");
    }
    HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);

    bh_data_send(BHPowOn);   // BH1750上电
    bh_data_send(BHReset);   // BH1750复位
    bh_data_send(BHModeH2);  // BH1750高分辨率模式2: 0.5lx, 120ms
    snprintf(dbg, sizeof(dbg), "BH1750: Init done, mode=H2\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);
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

/* 读取光照数据（带重试保护 + 诊断打印），返回lux值 */
float bh_data_read(void)
{
    uint16_t buf;
    uint8_t retry = 0;
    static uint8_t first_run = 1;

    do {
        SoftI2C_Start(&bh1750_i2c);
        SoftI2C_SendByte(&bh1750_i2c, BHAddRead);
        if (SoftI2C_WaitAck(&bh1750_i2c) == 0) {
            buf = SoftI2C_ReadByte(&bh1750_i2c, 1);
            buf = buf << 8;
            buf += 0x00FF & SoftI2C_ReadByte(&bh1750_i2c, 0);
            SoftI2C_Stop(&bh1750_i2c);
            /* 首次成功读取时打印确认 */
            if (first_run) {
                char dbg[48];
                snprintf(dbg, sizeof(dbg), "BH1750: raw=%u lux=%.1f\r\n", buf, (float)buf / 1.2f);
                HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);
                first_run = 0;
            }
            /* H2模式分辨率0.5lx，实际lux = raw / 1.2 */
            return (float)buf / 1.2f;
        }
        SoftI2C_Stop(&bh1750_i2c);
        retry++;
    } while (retry < 3);

    /* 读取失败：首次打印警告 */
    if (first_run) {
        char dbg[48];
        snprintf(dbg, sizeof(dbg), "BH1750: Read ACK fail after 3 retries\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);
        first_run = 0;
    }
    return 0.0f;  /* 读取失败返回0 */
}

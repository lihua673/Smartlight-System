#include "soft_i2c.h"

extern void Delay_us(uint32_t us);

/* 初始化：总线空闲状态SCL、SDA均拉高 */
void SoftI2C_Init(SoftI2C_Bus_t *bus)
{
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
}

/* I2C起始信号：SCL高时SDA产生下降沿 */
void SoftI2C_Start(SoftI2C_Bus_t *bus)
{
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_RESET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
    Delay_us(bus->delay_us);
}

/* I2C停止信号：SCL高时SDA产生上升沿 */
void SoftI2C_Stop(SoftI2C_Bus_t *bus)
{
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_RESET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
}

/* 发送一个字节，高位先行 */
void SoftI2C_SendByte(SoftI2C_Bus_t *bus, uint8_t data)
{
    uint8_t i;
    SoftI2C_SDA_Out(bus);
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin,
            (data & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        data <<= 1;
        Delay_us(2);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
        Delay_us(2);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
        Delay_us(2);
    }
}

/* 读取一个字节，ack=1时发送ACK，ack=0发送NACK */
uint8_t SoftI2C_ReadByte(SoftI2C_Bus_t *bus, uint8_t ack)
{
    uint8_t i, receive = 0;
    SoftI2C_SDA_In(bus);
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
        Delay_us(2);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
        receive <<= 1;
        if (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin))
            receive++;
        Delay_us(1);
    }
    if (!ack)
        SoftI2C_NAck(bus);
    else
        SoftI2C_Ack(bus);
    return receive;
}

/* 等待从机ACK，超时返回1 */
uint8_t SoftI2C_WaitAck(SoftI2C_Bus_t *bus)
{
    uint8_t err_cnt = 0;
    SoftI2C_SDA_In(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(1);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(1);
    while (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin))
    {
        err_cnt++;
        if (err_cnt > 250)
        {
            SoftI2C_Stop(bus);
            return 1;
        }
    }
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
    return 0;
}

/* 发送ACK */
void SoftI2C_Ack(SoftI2C_Bus_t *bus)
{
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_RESET);
    Delay_us(2);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(2);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
}

/* 发送NACK */
void SoftI2C_NAck(SoftI2C_Bus_t *bus)
{
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(2);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(2);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
}

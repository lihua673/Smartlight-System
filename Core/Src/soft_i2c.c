#include "soft_i2c.h"

extern void Delay_us(uint32_t us);

/* 初始化：配置总线引脚并启用内部上拉（外部上拉仍需4.7kΩ更可靠） */
void SoftI2C_Init(SoftI2C_Bus_t *bus)
{
    /* 使能内部上拉作为后备（外部4.7kΩ上拉电阻仍强烈建议） */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = bus->scl_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->scl_port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = bus->sda_pin;
    HAL_GPIO_Init(bus->sda_port, &GPIO_InitStruct);

    /* 总线空闲：SCL、SDA 均拉高 */
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
}

/* I2C起始信号：SCL高时SDA产生下降沿 */
void SoftI2C_Start(SoftI2C_Bus_t *bus)
{
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
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

/* 发送一个字节（高位先行），调用者必须在之后调用 WaitAck 产生第9个时钟 */
void SoftI2C_SendByte(SoftI2C_Bus_t *bus, uint8_t data)
{
    uint8_t i;
    SoftI2C_SDA_Out(bus);
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin,
            (data & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        data <<= 1;
        Delay_us(bus->delay_us);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
        Delay_us(bus->delay_us);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
        Delay_us(bus->delay_us);
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
        Delay_us(bus->delay_us);
        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
        Delay_us(1);   /* 等待从机输出数据稳定 */
        receive <<= 1;
        if (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin))
            receive++;
        Delay_us(bus->delay_us);
    }
    if (!ack)
        SoftI2C_NAck(bus);
    else
        SoftI2C_Ack(bus);
    return receive;
}

/* 等待从机ACK（产生第9个SCL时钟并读取SDA），超时返回1 */
uint8_t SoftI2C_WaitAck(SoftI2C_Bus_t *bus)
{
    uint16_t err_cnt = 0;
    SoftI2C_SDA_In(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(1);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(1);
    while (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin))
    {
        err_cnt++;
        if (err_cnt > 1000)   /* 超时但不停机，返回失败 */
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
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
}

/* 发送NACK */
void SoftI2C_NAck(SoftI2C_Bus_t *bus)
{
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
    SoftI2C_SDA_Out(bus);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    Delay_us(bus->delay_us);
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
}

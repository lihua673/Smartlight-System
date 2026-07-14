#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "stm32f4xx_hal.h"

/* 软件I2C总线句柄 */
typedef struct {
    GPIO_TypeDef *scl_port;
    uint16_t      scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t      sda_pin;
    uint32_t      delay_us;   // I2C时序延时(us)，标准模式建议5
} SoftI2C_Bus_t;

/* 初始化总线GPIO */
void SoftI2C_Init(SoftI2C_Bus_t *bus);

/* I2C基本时序 */
void SoftI2C_Start(SoftI2C_Bus_t *bus);
void SoftI2C_Stop(SoftI2C_Bus_t *bus);
void SoftI2C_SendByte(SoftI2C_Bus_t *bus, uint8_t data);
uint8_t SoftI2C_ReadByte(SoftI2C_Bus_t *bus, uint8_t ack);
uint8_t SoftI2C_WaitAck(SoftI2C_Bus_t *bus);
void SoftI2C_Ack(SoftI2C_Bus_t *bus);
void SoftI2C_NAck(SoftI2C_Bus_t *bus);

/*
 * GPIO方向快速切换（内联，直接寄存器操作）
 * bus->sda_pin 是GPIO_PIN_x掩码(如GPIO_PIN_6=0x0040)，需转为位号(0-15)计算MODER偏移
 * pin_mask_to_pos在编译期常量输入时会被优化为编译期常量，零运行时开销
 */
static inline uint32_t pin_mask_to_pos(uint16_t mask) {
    uint32_t pos = 0;
    while ((mask & 1) == 0) { mask >>= 1; pos++; }
    return pos;
}

static inline void SoftI2C_SDA_Out(SoftI2C_Bus_t *bus) {
    uint32_t pos = pin_mask_to_pos(bus->sda_pin);
    bus->sda_port->MODER = (bus->sda_port->MODER & ~(0x3 << (pos * 2)))
                         | (0x1 << (pos * 2));
}

static inline void SoftI2C_SDA_In(SoftI2C_Bus_t *bus) {
    uint32_t pos = pin_mask_to_pos(bus->sda_pin);
    bus->sda_port->MODER = (bus->sda_port->MODER & ~(0x3 << (pos * 2)));
}

#endif

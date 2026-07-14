#ifndef LD2402_UART_H
#define LD2402_UART_H

#include "stm32f4xx_hal.h"

typedef enum {
  HUMAN_NONE = 0,
  HUMAN_MOVE
} LD2402_HumanState_t;

typedef struct {
  LD2402_HumanState_t human_state;
  uint16_t distance;
  uint32_t last_human_tick;  // 最后一次检测到人体的时间戳（用于超时复位）
  uint16_t bg_distance;      // 背景距离（指数平滑学习）
  uint32_t bg_stable_since;  // 距离回到背景范围的起始时间
} LD2402_Data_t;

extern uint8_t rx_buf[128];
extern uint8_t rx_idx;

void LD2402_UART_Init(void);
void LD2402_ParseData(void);
void LD2402_UART_RxProcess(void);

#endif

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
} LD2402_Data_t;

extern LD2402_Data_t ld24_data;
extern uint8_t rx_buf[64];
extern uint8_t rx_idx;

void LD2402_UART_Init(void);
void LD2402_ParseData(void);
void LD2402_UART_RxProcess(void);

#endif

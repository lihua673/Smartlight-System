#include "app_state.h"
#include "ld2402_uart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "led_pwm.h"

#define DEVIATION_THRESHOLD  6     // 偏离背景阈值(cm)，越小越灵敏
#define STABLE_TIME_MS        12000 // 回到背景后稳定多久算无人(ms)，12秒
#define MIN_HUMAN_HOLD_MS     3000  // 一旦检测到人，至少保持3秒（防闪烁）
#define BG_LEARN_SAMPLES      30    // 启动时采集样本数

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart1;

uint8_t  rx_buf[128];
uint8_t  rx_idx;
volatile uint32_t rx_last_time;

static uint8_t  bg_learn_count = 0;
static uint16_t bg_learn_min  = 0xFFFF;
static uint8_t  bg_ready      = 0;

void LD2402_UART_Init(void)
{
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_idx = 0;
    rx_last_time = HAL_GetTick();
    g_app.radar.human_state = HUMAN_NONE;
    g_app.radar.distance = 0;
    g_app.radar.bg_distance = 0;
    g_app.radar.bg_stable_since = 0;
    bg_learn_count = 0;
    bg_learn_min = 0xFFFF;
    bg_ready = 0;
    HAL_UART_Receive_IT(&huart3, rx_buf, 1);
}

void LD2402_UART_RxProcess(void)
{
    rx_last_time = HAL_GetTick();
    rx_idx++;
    if (rx_idx >= sizeof(rx_buf))
    {
        rx_idx = 0;
        memset(rx_buf, 0, sizeof(rx_buf));
    }
    HAL_UART_Receive_IT(&huart3, &rx_buf[rx_idx], 1);
}

void LD2402_ParseData(void)
{
    uint32_t now = HAL_GetTick();

    // 心跳诊断：每秒打印缓冲区状态
    {
        static uint32_t last_hb = 0;
        if (now - last_hb >= 1000)
        {
            last_hb = now;
            char dbg[48];
            snprintf(dbg, sizeof(dbg), "[HB] rx_idx=%d bg_ready=%d state=%d\r\n",
                rx_idx, bg_ready, g_app.radar.human_state);
            HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
        }
    }

    while (rx_idx > 0)
    {
        uint8_t i;
        uint8_t *lf = NULL;
        for (i = 0; i < rx_idx; i++)
        {
            if (rx_buf[i] == '\n' || rx_buf[i] == '\r')
            {
                lf = &rx_buf[i];
                break;
            }
        }
        if (lf == NULL)
        {
            if (rx_idx >= sizeof(rx_buf) - 2)
            {
                memset(rx_buf, 0, sizeof(rx_buf));
                rx_idx = 0;
            }
            break;
        }

        *lf = '\0';
        if ((char *)rx_buf != (char *)lf)
        {
            char *line = (char *)rx_buf;

            if (strstr(line, "distance") != NULL)
            {
                char *p = strstr(line, "distance:");
                if (p)
                {
                    uint16_t cur_dist = (uint16_t)atoi(p + 9);  // 跳过"distance:"
                    g_app.radar.distance = cur_dist;

                    // === 背景学习 ===
                    if (!bg_ready)
                    {
                        if (cur_dist < bg_learn_min)
                            bg_learn_min = cur_dist;
                        bg_learn_count++;

                        if (bg_learn_count >= BG_LEARN_SAMPLES)
                        {
                            g_app.radar.bg_distance = bg_learn_min;
                            bg_ready = 1;
                            {
                                char dbg[40];
                                snprintf(dbg, sizeof(dbg), "[RADAR] BG=%dcm (N=%d)\r\n",
                                    bg_learn_min, bg_learn_count);
                                HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
                            }
                        }
                        goto next_line;
                    }

                    // === 检测 ===
                    int16_t dev = (int16_t)cur_dist - (int16_t)g_app.radar.bg_distance;
                    if (dev < 0) dev = -dev;

                    // 诊断：每秒最多打1次偏差明细
                    {
                        static uint32_t last_dev_print = 0;
                        if (now - last_dev_print >= 1000)
                        {
                            last_dev_print = now;
                            char dbg[48];
                            snprintf(dbg, sizeof(dbg), "[DBG] cur=%d bg=%d dev=%d th=%d\r\n",
                                cur_dist, g_app.radar.bg_distance, dev, DEVIATION_THRESHOLD);
                            HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
                        }
                    }

                    if (dev > DEVIATION_THRESHOLD)
                    {
                        g_app.radar.last_human_tick = now;
                        if (g_app.radar.human_state == HUMAN_NONE)
                        {
                            // 无人→有人
                            char dbg[32];
                            snprintf(dbg, sizeof(dbg), "[RADAR] HUMAN=1 (dev=%d)\r\n", dev);
                            HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
                        }
                        g_app.radar.human_state = HUMAN_MOVE;
                        g_app.radar.bg_stable_since = 0;
                    }
                    else
                    {
                        // 有人状态才检测退场
                        if (g_app.radar.human_state == HUMAN_MOVE)
                        {
                            // 防抖：刚检测到人时，保持至少 MIN_HUMAN_HOLD_MS
                            if (now - g_app.radar.last_human_tick < MIN_HUMAN_HOLD_MS)
                            {
                                goto next_line;
                            }

                            if (g_app.radar.bg_stable_since == 0)
                            {
                                g_app.radar.bg_stable_since = now;
                            }
                            else if (now - g_app.radar.bg_stable_since >= STABLE_TIME_MS)
                            {
                                // 有人→无人
                                char dbg[32];
                                snprintf(dbg, sizeof(dbg), "[RADAR] HUMAN=0 (stable 12s)\r\n");
                                HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
                                g_app.radar.human_state = HUMAN_NONE;
                                g_app.radar.bg_stable_since = 0;
                            }
                        }
                    }
                }
            }

next_line:
            ;
        }

        uint8_t consumed = (uint8_t)(lf - rx_buf) + 1;
        while (consumed < rx_idx && (rx_buf[consumed] == '\n' || rx_buf[consumed] == '\r'))
        {
            consumed++;
        }
        uint8_t remaining = rx_idx - consumed;
        if (remaining > 0)
        {
            memmove(rx_buf, rx_buf + consumed, remaining);
        }
        rx_idx = remaining;
    }

    if (rx_idx > 0 && (now - rx_last_time) >= 500)
    {
        memset(rx_buf, 0, sizeof(rx_buf));
        rx_idx = 0;
    }
}

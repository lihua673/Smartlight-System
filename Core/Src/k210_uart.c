#include "k210_uart.h"
#include "app_state.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;   // 调试串口
extern UART_HandleTypeDef huart4;   // K210通信串口

K210_FaceData_t g_k210_face;
uint8_t k210_rx_buf[K210_RX_BUF_SIZE];
volatile uint8_t k210_rx_idx;

/* 协议解析状态机 */
typedef enum {
    K210_STATE_IDLE,      // 等待起始字节0x24
    K210_STATE_HEADER,    // 接收帧头: length, class_num, class_group, data_num
    K210_STATE_DATA,      // 接收数据字节
    K210_STATE_CRC,       // 接收CRC校验字节
    K210_STATE_END        // 等待结束字节0x23
} K210_RxState_t;

static K210_RxState_t rx_state = K210_STATE_IDLE;
static uint8_t  frame_len;       // 帧中的length字段
static uint8_t  frame_data_num;  // 帧中的data_num字段
static uint8_t  data_idx;        // 当前已接收的数据字节数
static uint8_t  data_bytes[256]; // 存放解析出的纯数据(去掉逗号后的有效字节)
static uint8_t  data_byte_count; // 有效数据字节数(去逗号)
static uint8_t  calc_crc;        // 计算出的CRC

/* 时间戳(用于超时检测) */
static volatile uint32_t k210_last_byte_tick;

/* ── 初始化K210 UART ── */
void K210_UART_Init(void)
{
    memset(&g_k210_face, 0, sizeof(g_k210_face));
    memset(k210_rx_buf, 0, sizeof(k210_rx_buf));
    k210_rx_idx = 0;
    rx_state = K210_STATE_IDLE;
    k210_last_byte_tick = HAL_GetTick();

    /* 启动UART4中断接收 */
    HAL_UART_Receive_IT(&huart4, k210_rx_buf, 1);

    {
        char msg[40];
        snprintf(msg, sizeof(msg), "[K210] UART4 init done, baud=115200\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    }
}

/* ── UART中断回调: 逐字节驱动状态机 ── */
void K210_UART_RxProcess(void)
{
    uint8_t byte = k210_rx_buf[0];
    k210_last_byte_tick = HAL_GetTick();

    switch (rx_state)
    {
    case K210_STATE_IDLE:
        if (byte == K210_FRAME_START)
        {
            /* 检测到帧头，初始化 */
            calc_crc = 0;
            data_byte_count = 0;
            memset(data_bytes, 0, sizeof(data_bytes));
            rx_state = K210_STATE_HEADER;
        }
        /* 非0x24的字节直接丢弃 */
        break;

    case K210_STATE_HEADER:
        /* 按顺序接收4个头部字节:
         *   [0] length
         *   [1] class_num (应等于0x05)
         *   [2] class_group (应等于0xBB)
         *   [3] data_num
         */
        {
            static uint8_t header_pos = 0;
            static uint8_t header_buf[4];

            header_buf[header_pos] = byte;
            calc_crc += byte;
            header_pos++;

            if (header_pos >= 4)
            {
                header_pos = 0;
                frame_len      = header_buf[0];
                /* header_buf[1] = class_num, 可验证==0x05 */
                /* header_buf[2] = class_group, 可验证==0xBB */
                frame_data_num = header_buf[3];
                data_idx = 0;

                if (frame_data_num > 0 && frame_data_num <= sizeof(data_bytes) * 2)
                {
                    /* 有数据段 */
                    rx_state = K210_STATE_DATA;
                }
                else if (frame_data_num == 0)
                {
                    /* 无数据段，直接进入CRC */
                    rx_state = K210_STATE_CRC;
                }
                else
                {
                    /* 数据量异常，丢弃整帧 */
                    rx_state = K210_STATE_IDLE;
                }
            }
        }
        break;

    case K210_STATE_DATA:
        /* 接收数据字节(含逗号分隔符) */
        calc_crc += byte;
        data_idx++;

        /* 只保留非逗号的有效字节 */
        if (byte != K210_SEPARATOR)
        {
            if (data_byte_count < sizeof(data_bytes))
            {
                data_bytes[data_byte_count++] = byte;
            }
        }

        if (data_idx >= frame_data_num)
        {
            rx_state = K210_STATE_CRC;
        }
        break;

    case K210_STATE_CRC:
        /* CRC = 前面所有字节之和 % 256 */
        if (byte == (calc_crc % 256))
        {
            /* CRC校验通过 */
            rx_state = K210_STATE_END;
        }
        else
        {
            /* CRC失败，丢帧 */
            {
                char dbg[48];
                snprintf(dbg, sizeof(dbg), "[K210] CRC fail: calc=%d recv=%d\r\n",
                    calc_crc % 256, byte);
                HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
            }
            rx_state = K210_STATE_IDLE;
        }
        break;

    case K210_STATE_END:
        if (byte == K210_FRAME_END)
        {
            /* ── 完整帧接收成功，新协议解析 ──
             * data_bytes[0] = face_count (人数)
             * data_bytes[1..8]   = 第1张脸: x_lo,x_hi, y_lo,y_hi, w_lo,w_hi, h_lo,h_hi
             * data_bytes[9..16]  = 第2张脸 ...
             * 期望总长度: 1 + face_count × 8
             */
            if (data_byte_count >= 1)
            {
                uint8_t face_count = data_bytes[0];
                uint8_t expected = 1 + face_count * 8;

                if (face_count == 0)
                {
                    /* 空帧：无人脸 */
                    g_k210_face.face_count = 0;
                    g_k210_face.detected  = 0;
                    g_k210_face.x = 0; g_k210_face.y = 0;
                    g_k210_face.w = 0; g_k210_face.h = 0;
                    g_k210_face.last_tick = HAL_GetTick();
                    g_app.face_detected = 0;
                    g_app.face_count    = 0;
                }
                else if (face_count <= K210_MAX_FACES &&
                         data_byte_count >= expected)
                {
                    /* 遍历所有人脸，找面积最大的作为主目标 */
                    uint32_t max_area = 0;
                    uint8_t  max_i    = 0;

                    for (uint8_t i = 0; i < face_count; i++)
                    {
                        uint8_t *p = &data_bytes[1 + i * 8];
                        uint16_t fx = p[0] | (p[1] << 8);
                        uint16_t fy = p[2] | (p[3] << 8);
                        uint16_t fw = p[4] | (p[5] << 8);
                        uint16_t fh = p[6] | (p[7] << 8);
                        uint32_t area = (uint32_t)fw * fh;

                        if (area > max_area)
                        {
                            max_area = area;
                            max_i = i;
                        }
                    }

                    /* 取最大面积人脸坐标 */
                    uint8_t *pmax = &data_bytes[1 + max_i * 8];
                    g_k210_face.face_count = face_count;
                    g_k210_face.detected  = 1;
                    g_k210_face.x = pmax[0] | (pmax[1] << 8);
                    g_k210_face.y = pmax[2] | (pmax[3] << 8);
                    g_k210_face.w = pmax[4] | (pmax[5] << 8);
                    g_k210_face.h = pmax[6] | (pmax[7] << 8);
                    g_k210_face.last_tick = HAL_GetTick();

                    /* 同步更新全局状态 */
                    g_app.face_detected = 1;
                    g_app.face_count    = face_count;
                }
                /* else: 数据量不匹配 → 静默丢弃（CRC已通过但格式异常） */
            }
        }
        /* 无论是否收到正确的结束字节，回到IDLE等待下一帧 */
        rx_state = K210_STATE_IDLE;
        break;
    }

    /* 重新启动中断接收 */
    k210_rx_buf[0] = 0;
    HAL_UART_Receive_IT(&huart4, k210_rx_buf, 1);
}

/* ── 主循环调用: 超时自动清零 + 心跳诊断 ── */
void K210_ParseFrame(void)
{
    uint32_t now = HAL_GetTick();

    /* 超时：超过FACE_TIMEOUT_MS未收到有效帧，判定无人脸 */
    if (g_k210_face.detected &&
        (now - g_k210_face.last_tick) >= K210_FACE_TIMEOUT_MS)
    {
        g_k210_face.face_count = 0;
        g_k210_face.detected = 0;
        g_k210_face.x = 0;
        g_k210_face.y = 0;
        g_k210_face.w = 0;
        g_k210_face.h = 0;
        g_app.face_detected = 0;
        g_app.face_count    = 0;
    }

    /* 心跳诊断: 每5秒打印一次K210状态 */
    {
        static uint32_t last_hb = 0;
        if (now - last_hb >= 5000)
        {
            last_hb = now;
            if (g_k210_face.detected)
            {
                char dbg[64];
                snprintf(dbg, sizeof(dbg),
                    "[K210] FACE: count=%d max=(%d,%d %dx%d)\r\n",
                    g_k210_face.face_count,
                    g_k210_face.x, g_k210_face.y,
                    g_k210_face.w, g_k210_face.h);
                HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
            }
        }
    }
}

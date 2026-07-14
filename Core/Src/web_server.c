#include "web_server.h"
#include "app_state.h"
#include "esp8266.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* ═══════════════════════════════════════════════════════════════
 *  内嵌 Web 仪表盘 HTML (~1900 bytes, 纯ASCII)
 *
 *  深色主题，自适应手机屏幕
 *  三种数据卡片 + 亮度百分比 + 自动/手动切换 + 滑块 + 开关
 *  JS每2秒轮询 /api/status，按钮调用 /api/set
 * ═══════════════════════════════════════════════════════════════ */
static const char INDEX_HTML[] =
"<meta charset=UTF-8><style>"
"*{margin:0;padding:0}"
"body{background:#0a0a14;color:#ccc;padding:12px}"
".r{display:flex;gap:7px}"
".r>div,.ga,.gi{background:#16162a}"
".r>div{flex:1;border-radius:12px;padding:12px 4px;text-align:center;border:1px solid #252545}"
".v,.pv{color:#818cf8;font-weight:700}"
".v{font-size:24px}"
".pv{font-size:48px}"
".l,.pm{font-size:10px;color:#666}"
".p{text-align:center;margin:16px 0}"
".g{display:flex;gap:7px;margin:12px 0}"
".g>*{flex:1;padding:12px;border-radius:8px;text-align:center;font-size:14px;font-weight:600}"
".ga{border:2px solid #818cf8;color:#818cf8}"
".gi{border:2px solid #333;color:#555}"
"button{border:none;color:#fff}"
".bn{background:#059669}.bo{background:#dc2626}"
"#s{width:100%;margin:6px 0;display:none;accent-color:#818cf8}"
"</style>"
"<div class=r><div><div class=v id=x>--</div><div class=l>Lux</div></div>"
"<div><div class=v id=h>--</div><div class=l>Human</div></div>"
"<div><div class=v id=f>0</div><div class=l>Face</div></div></div>"
"<div class=p><div class=pv id=br>--%</div><div class=pm id=mn></div></div>"
"<div class=g><div class=ga id=ba onclick=sM(0)>Auto</div><div class=gi id=bm onclick=sM(1)>Manual</div></div>"
"<input type=range min=0 max=100 value=50 id=s oninput=sB(this.value)>"
"<div class=g><button class=bn onclick=sB(100)>ON</button><button class=bo onclick=sB(0)>OFF</button></div>"
"<script>"
"function $(id){return document.getElementById(id)}"
"function poll(){"
"fetch('/api/status').then(r=>r.json()).then(d=>{"
"$('x').textContent=d.lux.toFixed(1);$('h').textContent=d.human?'Y':'N';"
"$('f').textContent=d.faces;$('br').textContent=d.bri+'%';"
"$('mn').textContent=d.mname||'';"
"var m=d.mode=='manual';"
"$('ba').className=m?'gi':'ga';$('bm').className=m?'ga':'gi';"
"$('s').style.display=m?'block':'none';if(m)$('s').value=d.bri;"
"}).catch(e=>{});}"
"function sM(m){fetch('/api/set?m='+(m?'manual':'auto')).then(r=>r.json()).then(d=>{if(d.ok)poll()}).catch(e=>{});}"
"function sB(v){fetch('/api/set?b='+v).then(r=>r.json()).then(d=>{if(d.ok)poll()}).catch(e=>{});}"
"setInterval(poll,2000);poll();"
"</script>";


/* ── 发送一次 CIPSEND ── */
static uint8_t cipsend_send(uint8_t link_id, const char *data, uint16_t len)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d\r\n",
             (int)link_id, (int)len);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    if (!ESP_WaitResp(">", 1000))
    {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "[WEB] CIPSEND fail link=%d len=%d\r\n",
            (int)link_id, (int)len);
        HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
        return 0;
    }

    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, HAL_MAX_DELAY);

    {
        uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < 500) {
            if (strstr(USART2_RX_BUF, "SEND OK") ||
                strstr(USART2_RX_BUF, "Recv ")) break;
        }
    }
    return 1;
}


/* ── 构造并发送 HTTP 响应 ── */
static void http_respond(uint8_t link_id, const char *content_type,
                         const char *body)
{
    uint16_t body_len = (uint16_t)strlen(body);

    char header[192];
    uint16_t header_len = (uint16_t)snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        content_type, (int)body_len);

    /* 清缓冲 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, 128);
    HAL_UART_AbortReceive_IT(&huart2);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);

    /* 分两次发送 */
    if (cipsend_send(link_id, header, header_len))
    {
        USART2_RX_LEN = 0;
        memset(USART2_RX_BUF, 0, 128);
        HAL_UART_AbortReceive_IT(&huart2);
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);
        cipsend_send(link_id, body, body_len);
    }

    /* 清缓冲 */
    USART2_RX_LEN = 0;
    memset(USART2_RX_BUF, 0, 128);
    HAL_UART_AbortReceive_IT(&huart2);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&USART2_RX_BUF[0], 1);
}


/* ── GET /api/status → JSON ── */
static void handle_api_status(uint8_t link_id)
{
    uint8_t  human  = (g_app.radar.human_state != HUMAN_NONE) ? 1 : 0;
    uint8_t  faces  = g_app.face_count;
    float    lux    = g_app.lux;
    uint8_t  bri_pct = (uint8_t)(g_led_duty * 100 / 999);
    uint8_t  manual = g_app.manual_mode;

    const char *mname;
    if (manual)
        mname = "manual mode";
    else if (!human)
        mname = "no one - off";
    else if (faces >= 2)
        mname = "multi - full";
    else if (faces == 1)
        mname = "single - auto";
    else
        mname = "radar only";

    char json[160];
    snprintf(json, sizeof(json),
        "{"
        "\"lux\":%.1f,"
        "\"human\":%d,"
        "\"faces\":%d,"
        "\"bri\":%d,"
        "\"mode\":\"%s\","
        "\"mname\":\"%s\""
        "}",
        (double)lux, human, (int)faces, (int)bri_pct,
        manual ? "manual" : "auto",
        mname);

    http_respond(link_id, "application/json", json);
}


/* ── GET /api/set?b=<0-100>&m=<auto|manual> → JSON ── */
static void handle_api_set(uint8_t link_id, const char *query)
{
    char *p;

    p = strstr(query, "b=");
    if (p) {
        int b = atoi(p + 2);
        if (b < 0) b = 0;
        if (b > 100) b = 100;
        g_app.manual_brightness = (uint16_t)(b * 999 / 100);
    }

    p = strstr(query, "m=");
    if (p) {
        if (strncmp(p + 2, "manual", 6) == 0) {
            g_app.manual_mode = 1;
        } else if (strncmp(p + 2, "auto", 4) == 0) {
            g_app.manual_mode = 0;
        }
    }

    http_respond(link_id, "application/json", "{\"ok\":1}");
}


/* ── GET / → HTML 仪表盘 ── */
static void handle_root(uint8_t link_id)
{
    http_respond(link_id, "text/html; charset=utf-8", INDEX_HTML);
}


/* ── 其他路径 → 404 ── */
static void handle_404(uint8_t link_id)
{
    http_respond(link_id, "text/plain", "404 Not Found");
}


/* ═══════════════════════════════════════════════════════════════
 *  WebServer_Init
 * ═══════════════════════════════════════════════════════════════ */
void WebServer_Init(void)
{
    if (!g_app.wifi_ready) return;

    {
        char msg[80];
        snprintf(msg, sizeof(msg),
            "\r\n========================================\r\n"
            "  Web控制面板: http://%s\r\n"
            "  手机连同一WiFi, 浏览器打开上面地址\r\n"
            "========================================\r\n",
            g_app.esp_ip[0] ? g_app.esp_ip : "???");
        HAL_UART_Transmit(&huart1, (uint8_t *)msg,
            strlen(msg), HAL_MAX_DELAY);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  WebServer_Poll: 主循环调用，检测并处理 +IPD HTTP 请求
 * ═══════════════════════════════════════════════════════════════ */
void WebServer_Poll(void)
{
    if (!g_app.wifi_ready) return;

    char *ipd = strstr(USART2_RX_BUF, "+IPD,");
    if (!ipd) return;

    char *p = ipd + 5;

    uint8_t link_id = (uint8_t)atoi(p);

    p = strchr(p, ',');
    if (!p) return;
    p++;
    uint16_t data_len = (uint16_t)atoi(p);

    p = strchr(p, ':');
    if (!p) return;
    p++;

    uint16_t avail = (uint16_t)((uint8_t *)USART2_RX_BUF + USART2_RX_LEN
                              - (uint8_t *)p);
    if (avail < data_len) return;

    /* 调试 */
    {
        char dbg[80];
        char first_line[64] = {0};
        uint16_t copy_len = data_len < 63 ? data_len : 63;
        memcpy(first_line, p, copy_len);
        char *cr = strstr(first_line, "\r\n");
        if (cr) *cr = '\0';
        snprintf(dbg, sizeof(dbg), "[WEB] +IPD link=%d len=%d: %s\r\n",
            link_id, data_len, first_line);
        HAL_UART_Transmit(&huart1, (uint8_t *)dbg, strlen(dbg), HAL_MAX_DELAY);
    }

    char method[8]  = {0};
    char path[64]   = {0};
    char query[64]  = {0};
    sscanf(p, "%7s %63s", method, path);

    {
        char *qmark = strchr(path, '?');
        if (qmark) {
            *qmark = '\0';
            strncpy(query, qmark + 1, sizeof(query) - 1);
        }
    }

    if (strcmp(method, "GET") == 0)
    {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
            handle_root(link_id);
        else if (strcmp(path, "/api/status") == 0)
            handle_api_status(link_id);
        else if (strcmp(path, "/api/set") == 0)
            handle_api_set(link_id, query);
        else
            handle_404(link_id);
    }

    /* 从缓冲中移除已处理的数据 */
    {
        uint16_t consumed = (uint16_t)((uint8_t *)p - (uint8_t *)USART2_RX_BUF)
                          + data_len;
        uint16_t remaining = USART2_RX_LEN;
        if (consumed < remaining) {
            remaining -= consumed;
            memmove(USART2_RX_BUF, USART2_RX_BUF + consumed, remaining);
        } else {
            remaining = 0;
        }
        USART2_RX_LEN = remaining;
        if (remaining < sizeof(USART2_RX_BUF))
            USART2_RX_BUF[remaining] = '\0';

        HAL_UART_AbortReceive_IT(&huart2);
        HAL_UART_Receive_IT(&huart2,
            (uint8_t *)&USART2_RX_BUF[remaining], 1);
    }
}


/* ── 获取 IP 地址 ── */
const char* WebServer_GetIP(void)
{
    return g_app.esp_ip[0] ? g_app.esp_ip : NULL;
}

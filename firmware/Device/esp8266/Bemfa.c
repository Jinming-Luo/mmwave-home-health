#include "Bemfa.h"
#include "ESP8266.h"
#include "Delay.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "OLED.h"

/* 把 wifi.rxbuff 前 32 字节用十六进制 + ASCII 显示到 OLED */
static void dump_rxbuff(const char *title)
{
    OLED_Clear();
    OLED_ShowString(0, 0, (char *)title, OLED_8X16);

    /* 打印前 32 个可见字符，控制字符替换为'.' */
    char line[17];
    for (int row = 0; row < 3; row++) {
        int base = row * 16;
        for (int i = 0; i < 16; i++) {
            int idx = base + i;
            char c = (idx < wifi.rxcount) ? (char)wifi.rxbuff[idx] : ' ';
            if (c < 0x20 || c > 0x7E) c = '.';
            line[i] = c;
        }
        line[16] = 0;
        OLED_ShowString(0, 16 + row * 16, line, OLED_8X16);
    }
    OLED_Update();
    Delay_ms(4000);   /* 给你 4 秒看清楚 */
}


/* ================ 全局命令标志 ================ */
Bemfa_CmdFlags_t Bemfa_Cmd = {0};

/* SysTick 心跳计时器：bemfa_tick_ms 在 Bemfa.h 中 extern，需要由 SysTick_Handler 喂 */
volatile uint32_t bemfa_tick_ms  = 0;
static uint32_t   last_heartbeat = 0;

void Bemfa_TickInc1ms(void) { bemfa_tick_ms++; }
static uint32_t bemfa_now(void) { return bemfa_tick_ms; }

/* ================ 内部工具 ================ */
static void clear_wifi_buf(void)
{
    memset(wifi.rxbuff, 0, sizeof(wifi.rxbuff));
    wifi.rxcount = 0;
    wifi.rxover  = 0;
}

/* 等指定字符串出现，timeout_ms 内没等到返回 0 */
static uint8_t wait_keyword(const char *kw, uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if (wifi.rxover) {
            if (strstr((char *)wifi.rxbuff, kw) != NULL) return 1;
        }
        Delay_ms(1);
    }
    return 0;
}

/* ================ 连接巴法 ================ */
//uint8_t Bemfa_Connect(void)
//{
//    char cmd[128];

//    /* 1. AT 测试 */
//    if (!EspSendCmdAndCheckRecvData("AT\r\n", "OK", 1000)) return 0;

//    /* 2. STA 模式 */
//    if (!EspSendCmdAndCheckRecvData("AT+CWMODE=1\r\n", "OK", 1000)) return 0;

//    /* 3. 关闭已有连接（忽略返回） */
//    EspSendCmdAndCheckRecvData("AT+CIPCLOSE\r\n", "OK", 500);

//    /* 4. 关闭多连接 */
//    EspSendCmdAndCheckRecvData("AT+CIPMUX=0\r\n", "OK", 1000);

//    /* 5. 关闭透传，使用 +IPD 接收方式 */
//    EspSendCmdAndCheckRecvData("AT+CIPMODE=0\r\n", "OK", 1000);

//    /* 6. 连 WiFi */
//    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
//             BEMFA_WIFI_SSID, BEMFA_WIFI_PSWD);
//    if (!EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)"OK", 15000)) return 0;

//    /* 7. 连 巴法 TCP */
//    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",
//             BEMFA_SERVER_IP, BEMFA_SERVER_PORT);
//    if (!EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)"CONNECT", 8000) &&
//        !EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)"ALREADY", 1000)) return 0;

//    /* 8. 订阅多个主题（巴法订阅协议：cmd=1&uid=xxx&topic=t1,t2,t3\r\n） */
//    char payload[160];
//    snprintf(payload, sizeof(payload),
//             "cmd=1&uid=%s&topic=%s,%s\r\n",
//             BEMFA_UID, BEMFA_TOPIC_LIGHT, BEMFA_TOPIC_FAN);

//    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int)strlen(payload));
//    if (!EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)">", 2000)) return 0;
//    if (!EspSendCmdAndCheckRecvData((uint8_t *)payload, (uint8_t *)"cmd=1&res=1", 3000)) return 0;

//    last_heartbeat = bemfa_now();
//    return 1;
//}

/* ============================================================
 * 诊断版连接函数：把这段代码替换 Bemfa.c 里的 Bemfa_Connect
 * 每一步会在 OLED 上显示进度，方便定位卡在哪一步
 * ============================================================ */

#include "OLED.h"   /* 在 Bemfa.c 顶部加上 */

/* 显示一行状态到 OLED 调试区（第 56 行起的最后一行） */
static void dbg_step(const char *step, uint8_t ok)
{
    OLED_Clear();
    OLED_ShowString(0, 0,  "BemfaDebug:", OLED_8X16);
    OLED_ShowString(0, 24, (char *)step, OLED_8X16);
    OLED_ShowString(0, 48, ok ? "OK" : "FAIL", OLED_8X16);
    OLED_Update();
    Delay_ms(800);    /* 让你看清楚 */
}

uint8_t Bemfa_Connect(void)
{
    char cmd[128];
    uint8_t ret;

    /* Step 1: AT 测试 - 检查 STM32 与 ESP8266 串口通信 */
    ret = EspSendCmdAndCheckRecvData("AT\r\n", "OK", 1000);
    dbg_step("1.AT test", ret);
    if (!ret) return 0;

    /* Step 2: 查询固件版本（看 ESP8266 是否真的回应得上） */
    ret = EspSendCmdAndCheckRecvData("AT+GMR\r\n", "OK", 1000);
    dbg_step("2.AT+GMR", ret);

    /* Step 3: STA 模式 */
    ret = EspSendCmdAndCheckRecvData("AT+CWMODE=1\r\n", "OK", 1000);
    dbg_step("3.STA mode", ret);
    if (!ret) return 0;

    /* Step 4: 重启使模式生效（重要！很多人忘记） */
    EspSendCmdAndCheckRecvData("AT+RST\r\n", "OK", 500);
    Delay_ms(2000);   /* 等待 ESP 重启完成 */

/* === 新增：设置可用的 DNS 服务器（阿里 + 114），防止 DNS Fail === */
    EspSendCmdAndCheckRecvData(
        "AT+CIPDNS_CUR=1,\"223.5.5.5\",\"114.114.114.114\"\r\n",
        "OK", 1000);
    /* 老固件可能不认 CIPDNS_CUR，再试老命令格式 */
    EspSendCmdAndCheckRecvData(
        "AT+CIPDNS=1,\"223.5.5.5\",\"114.114.114.114\"\r\n",
        "OK", 1000);

    /* Step 5: 关闭多连接、关闭透传 */
    EspSendCmdAndCheckRecvData("AT+CIPMUX=0\r\n",  "OK", 1000);
    EspSendCmdAndCheckRecvData("AT+CIPMODE=0\r\n", "OK", 1000);

    /* Step 6: 连 WiFi（最容易卡住的地方，给 20 秒超时） */
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
             BEMFA_WIFI_SSID, BEMFA_WIFI_PSWD);
    ret = EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)"GOT IP", 20000);
    dbg_step("6.WiFi join", ret);
    if (!ret) return 0;

    /* Step 7: 查一下 IP，确认真的拿到了 */
    ret = EspSendCmdAndCheckRecvData("AT+CIFSR\r\n", "OK", 2000);
    dbg_step("7.Get IP", ret);

    /* Step 8: 连巴法 TCP（带返回内容显示） */
EspSendCmdAndCheckRecvData("AT+CIPCLOSE\r\n", "OK", 1000);   /* 强制关旧连接 */
Delay_ms(500);

snprintf(cmd, sizeof(cmd),
             "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",
             BEMFA_SERVER_IP, BEMFA_SERVER_PORT);

memset(wifi.rxbuff, 0, sizeof(wifi.rxbuff));
wifi.rxcount = 0;
wifi.rxover  = 0;
Usart2_SendString((uint8_t *)cmd);

/* 等到任意一个关键字出现，最多 10 秒 */
for (uint16_t t = 0; t < 10000; t++) {
    Delay_ms(1);
    if (wifi.rxover &&
        (strstr((char *)wifi.rxbuff, "CONNECT") ||
         strstr((char *)wifi.rxbuff, "ALREADY") ||
         strstr((char *)wifi.rxbuff, "ERROR")   ||
         strstr((char *)wifi.rxbuff, "FAIL")    ||
         strstr((char *)wifi.rxbuff, "CLOSED")  ||
         strstr((char *)wifi.rxbuff, "DNS"))) {
        break;
    }
}

dump_rxbuff("8.TCP resp:");   /* 把 ESP8266 真正返回的内容显示出来 */

ret = (strstr((char *)wifi.rxbuff, "CONNECT") != NULL) ||
      (strstr((char *)wifi.rxbuff, "ALREADY") != NULL);
if (!ret) return 0;


    /* Step 9: 订阅主题 */
    char payload[160];
    snprintf(payload, sizeof(payload),
             "cmd=1&uid=%s&topic=%s,%s\r\n",
             BEMFA_UID, BEMFA_TOPIC_LIGHT, BEMFA_TOPIC_FAN);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int)strlen(payload));
    ret = EspSendCmdAndCheckRecvData((uint8_t *)cmd, (uint8_t *)">", 2000);
    dbg_step("9.CIPSEND", ret);
    if (!ret) return 0;

    ret = EspSendCmdAndCheckRecvData((uint8_t *)payload, (uint8_t *)"res=1", 3000);
    dbg_step("10.Subscribe", ret);
    if (!ret) return 0;

    last_heartbeat = bemfa_now();
    return 1;
}



/* ================ 上报一条消息 ================ */
uint8_t Bemfa_PublishMsg(const char *topic, const char *msg)
{
    char payload[160];
    char cmd[40];

    snprintf(payload, sizeof(payload),
             "cmd=2&uid=%s&topic=%s&msg=%s\r\n",
             BEMFA_UID, topic, msg);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int)strlen(payload));

    clear_wifi_buf();
    Usart2_SendString((uint8_t *)cmd);

    if (!wait_keyword(">", 1500)) {
        return 0;
    }

    clear_wifi_buf();
    Usart2_SendString((uint8_t *)payload);

    /*
     * 说明：
     * 1. res=1 表示巴法云应用层确认收到；
     * 2. SEND OK 表示 ESP8266 已经把 TCP 数据发出；
     * 3. 实际运行中 res=1 偶尔可能没及时等到，但 SEND OK 通常也说明链路未断。
     */
    if (wait_keyword("res=1", 2500)) {
        return 1;
    }

    if (strstr((char *)wifi.rxbuff, "SEND OK") != NULL) {
        return 1;
    }

    return 0;
}


/* ================ 一次性上报所有传感器 ================ */
void Bemfa_PublishSensors(uint8_t temp, uint8_t humi,
                          uint8_t hr,   uint8_t br,
                          uint8_t presence)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%u_%u", temp, humi);
    Bemfa_PublishMsg(BEMFA_TOPIC_TEMP, buf);

    if (hr > 0) {
        snprintf(buf, sizeof(buf), "%u", hr);
        Bemfa_PublishMsg(BEMFA_TOPIC_HEART, buf);
    }
    if (br > 0) {
        snprintf(buf, sizeof(buf), "%u", br);
        Bemfa_PublishMsg(BEMFA_TOPIC_BREATH, buf);
    }
    Bemfa_PublishMsg(BEMFA_TOPIC_PRES, presence ? "1" : "0");
}

static uint8_t bemfa_msg_is_on(const char *msg)
{
    return (strncmp(msg, "on", 2) == 0 ||
            strncmp(msg, "1", 1) == 0 ||
            strncmp(msg, "true", 4) == 0);
}

static uint8_t bemfa_msg_is_off(const char *msg)
{
    return (strncmp(msg, "off", 3) == 0 ||
            strncmp(msg, "0", 1) == 0 ||
            strncmp(msg, "false", 5) == 0);
}

static void parse_downstream(void)
{
    if (!wifi.rxover) return;

    char *buf = (char *)wifi.rxbuff;

    char *p_topic = strstr(buf, "topic=");
    char *p_msg   = strstr(buf, "msg=");

    if (p_topic && p_msg) {
        p_topic += 6;
        p_msg   += 4;

        if (strncmp(p_topic, BEMFA_TOPIC_LIGHT, strlen(BEMFA_TOPIC_LIGHT)) == 0) {
            if (bemfa_msg_is_on(p_msg)) {
                Bemfa_Cmd.light_cmd = 1;
                Bemfa_Cmd.light_cmd_pending = 1;
            } else if (bemfa_msg_is_off(p_msg)) {
                Bemfa_Cmd.light_cmd = 0;
                Bemfa_Cmd.light_cmd_pending = 1;
            }
        }
        else if (strncmp(p_topic, BEMFA_TOPIC_FAN, strlen(BEMFA_TOPIC_FAN)) == 0) {
            if (bemfa_msg_is_on(p_msg)) {
                Bemfa_Cmd.fan_cmd = 1;
                Bemfa_Cmd.fan_cmd_pending = 1;
            } else if (bemfa_msg_is_off(p_msg)) {
                Bemfa_Cmd.fan_cmd = 0;
                Bemfa_Cmd.fan_cmd_pending = 1;
            }
        }
    }

    clear_wifi_buf();
}


void Bemfa_Process(void)
{
    /* 1. 处理 ESP8266 上行的下行命令 */
    parse_downstream();

    /* 2. 心跳，巴法 TCP 端口要求 60s 内必须有数据 */
    if (bemfa_now() - last_heartbeat > BEMFA_HEARTBEAT_MS) {
        const char *ping = "ping\r\n";
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int)strlen(ping));

        clear_wifi_buf();
        Usart2_SendString((uint8_t *)cmd);
        if (wait_keyword(">", 500)) {
            Usart2_SendString((uint8_t *)ping);
        }
        last_heartbeat = bemfa_now();
    }
}

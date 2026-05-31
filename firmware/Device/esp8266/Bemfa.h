#ifndef __BEMFA_H
#define __BEMFA_H

#include "stm32f10x.h"

/* ================ 用户配置区 ================ */
/* WiFi 信息（） */
#define BEMFA_WIFI_SSID     "LAPTOP"
#define BEMFA_WIFI_PSWD     "123456789"

/* 巴法云 私钥 UID（在 https://cloud.bemfa.com 注册后获取，32 位字符串） */
#define BEMFA_UID           "**"

/* 主题（topic）名字
 * 巴法官方小程序约定：
 *   xxx001 → 插座（开关）
 *   xxx002 → 灯
 *   xxx003 → 风扇
 *   xxx004 → 传感器（仅用于上报）
 * 任意字母+数字组合，需要先在巴法云控制台创建。
 */
#define BEMFA_TOPIC_LIGHT   "light002"   /* 控制照明 */
#define BEMFA_TOPIC_FAN     "fan003"     /* 控制风扇 */

/* ★ 温湿度拆成两个独立 topic，避免小程序显示 "2030 度" 的问题 */
#define BEMFA_TOPIC_TEMP    "temp004"    /* 仅温度，单位 ℃ */
#define BEMFA_TOPIC_HUMI    "humi004"    /* 仅湿度，单位 %RH */

#define BEMFA_TOPIC_HEART   "heart004"   /* 心率，单位 bpm */
#define BEMFA_TOPIC_BREATH  "breath004"  /* 呼吸，单位 rpm */
#define BEMFA_TOPIC_PRES    "pres004"    /* 人体存在 0/1 */

/* 服务器 */
#define BEMFA_SERVER_IP     "bemfa.com"
#define BEMFA_SERVER_PORT   "8344"        /* TCP 创客云端口 */

/* 心跳间隔（毫秒），巴法要求 ≤ 60s 否则掉线，30s 比较稳 */
#define BEMFA_HEARTBEAT_MS  30000

/* 接收到控制命令时的回调标志（主程序轮询） */
typedef struct {
    uint8_t  light_cmd_pending;   /* 1 表示有未处理命令 */
    uint8_t  light_cmd;           /* 1=on, 0=off */
    uint8_t  fan_cmd_pending;
    uint8_t  fan_cmd;
} Bemfa_CmdFlags_t;

extern Bemfa_CmdFlags_t Bemfa_Cmd;

/* ================ API ================ */
uint8_t Bemfa_Connect(void);                          /* 完整连接流程：联网→订阅 */
void    Bemfa_Process(void);                          /* 主循环里轮询：解析下行 + 心跳 */
uint8_t Bemfa_PublishMsg(const char *topic,
                         const char *msg);            /* 主动上报数据 */
void    Bemfa_PublishSensors(uint8_t temp, uint8_t humi,
                             uint8_t hr,   uint8_t br,
                             uint8_t presence);       /* 一次性上报所有传感器 */

/* ms 计数器（由 SysTick_Handler 喂 1ms 节拍） */
extern volatile uint32_t bemfa_tick_ms;
void Bemfa_TickInc1ms(void);

#endif


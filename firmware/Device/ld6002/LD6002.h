#ifndef __LD6002_H
#define __LD6002_H

#include "stm32f10x.h"

/* ================ LD6002 协议常量（实测验证）================
 *  实测帧结构：
 *    SOF(0x01) | SEQ(2B大端) | LEN(2B大端) | MARK(0x0A) | DATA(LEN) | TRAILER(3B)
 *  心率/呼吸数据来自 LEN=12 的综合帧。
 * ============================================================== */

#define LD6002_SOF              0x01

#define LD6002_LEN_COMBO        12     /* 综合数据帧 */
#define LD6002_LEN_WAVE         4      /* 波形采样帧（不解析） */

#define LD6002_RX_BUF_SIZE      2048

/* 解析后的数据 */
typedef struct {
    float   heart_rate;
    float   breath_rate;
    float   distance;
    uint8_t has_target;
} LD6002_Data_t;

extern LD6002_Data_t LD6002_Data;

/* 调试统计（OLED 心率页用） */
extern volatile uint32_t ld6002_byte_count;
extern volatile uint32_t ld6002_frame_count;
extern volatile uint16_t ld6002_last_id;
extern volatile uint8_t  ld6002_last_data[32];
extern volatile uint8_t  ld6002_last_data_len;

void    LD6002_Init(void);
void    LD6002_Process(void);
uint8_t LD6002_GetHeartRate(void);
uint8_t LD6002_GetBreathRate(void);
float   LD6002_GetDistance(void);

#endif

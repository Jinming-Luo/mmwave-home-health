#ifndef __SEDENTARY_H
#define __SEDENTARY_H

#include "stm32f10x.h"

/* ======================= 阈值配置 ======================= 
 * 正式阈值（论文最终版用这个）：
 *   轻度提醒 45 min，重度提醒 75 min
 * 调试/演示用：把上面两行注释掉，启用下面两行（30 秒 / 60 秒）
 *   方便录视频和现场答辩演示
 * ======================================================== */
//#define SED_LIGHT_THRESHOLD_MS   (45UL * 60 * 1000)   /* 45 min */
//#define SED_HEAVY_THRESHOLD_MS   (75UL * 60 * 1000)   /* 75 min */

/* === 调试用阈值（演示前打开，演示完一定要切回正式阈值！） === */
#define SED_LIGHT_THRESHOLD_MS   (30UL * 1000)        /* 30 s */
#define SED_HEAVY_THRESHOLD_MS   (60UL * 1000)        /* 60 s */

/* 连续运动 30 s 才算"已经离座/起来活动了"，清零累积 */
#define SED_RESET_MOTION_MS      (30UL * 1000)

/* 巴法云 topic（请先去 cloud.bemfa.com 控制台新建一个 sed004） */
#define BEMFA_TOPIC_SED          "sed004"

/* ======================= 状态机 ======================= */
typedef enum {
    SED_IDLE = 0,        /* 无人 / 刚有人，尚未开始累积 */
    SED_SITTING,         /* 正在累积静止时间 */
    SED_LIGHT_WARN,      /* 已触发轻度提醒（45 min） */
    SED_HEAVY_WARN       /* 已触发重度提醒（75 min） */
} Sed_State_t;

typedef struct {
    Sed_State_t state;
    uint32_t    static_acc_ms;     /* 累积静止时间 */
    uint32_t    motion_acc_ms;     /* 当前运动连续时间，用于判断"真起来了" */
    uint32_t    last_tick_ms;
    uint8_t     warn_pending;      /* 1 = 主循环需要消费一次提醒事件 */
    uint8_t     warn_level;        /* 1 = 轻度, 2 = 重度 */
} Sed_Ctx_t;

extern Sed_Ctx_t g_sed;

/* ======================= API ======================= */
void     Sedentary_Init(void);

/* 主循环里每解析到 LD2410B 帧就调一次，
 * 入参 = LD2410B 数据帧 byte7（目标状态）：
 *   0x00 无人 / 0x01 仅运动 / 0x02 仅静止 / 0x03 运动+静止 */
void     Sedentary_Process(uint8_t ld2410_target_state);

/* 查询当前已累积静止时间（秒），用于 OLED 显示 */
uint32_t Sedentary_GetSittingSeconds(void);

#endif /* __SEDENTARY_H */

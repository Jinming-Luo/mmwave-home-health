#include "Sedentary.h"

/* 来自 Bemfa.c 的 1ms 软件节拍计数器（SysTick_Handler 喂） */
extern volatile uint32_t bemfa_tick_ms;

Sed_Ctx_t g_sed = {0};

/* -------------------------------------------------------------
 * 久坐识别状态机
 *
 * 设计要点：
 *  1. 仅当 LD2410B 上报"纯静止 0x02"时累积静止时间，
 *     遇到任何含运动的状态(0x01/0x03)都视为"用户在动"。
 *  2. 不是一动就清零 —— 必须连续动满 SED_RESET_MOTION_MS（30s）
 *     才认为"用户真离座/起来活动了"，避免小幅伸懒腰被误判。
 *  3. 越过阈值时只触发一次 warn_pending = 1，主循环消费后清零，
 *     不会重复推送。
 *  4. 时间累计基于 bemfa_tick_ms (SysTick 1ms 节拍)，
 *     使用 uint32_t 减法天然支持回绕（49.7 天）。
 * ------------------------------------------------------------- */

void Sedentary_Init(void)
{
    g_sed.state          = SED_IDLE;
    g_sed.static_acc_ms  = 0;
    g_sed.motion_acc_ms  = 0;
    g_sed.last_tick_ms   = bemfa_tick_ms;
    g_sed.warn_pending   = 0;
    g_sed.warn_level     = 0;
}

void Sedentary_Process(uint8_t state)
{
    uint32_t now   = bemfa_tick_ms;
    uint32_t delta = now - g_sed.last_tick_ms;
    g_sed.last_tick_ms = now;

    /* delta 异常保护：
     *   - 启动初期/首次调用，last_tick_ms 可能与当前差异极大
     *   - LD2410B 100ms 上报一次，正常 delta ≈ 100ms
     *   - 大于 2s 视为异常，按 100ms 计 */
    if (delta > 2000) delta = 100;

    /* === 无人：全部清零，回到 IDLE === */
    if (state == 0x00) {
        g_sed.static_acc_ms = 0;
        g_sed.motion_acc_ms = 0;
        g_sed.state         = SED_IDLE;
        return;
    }

    /* === 纯静止：累积静止时间 === */
    if (state == 0x02) {
        g_sed.static_acc_ms += delta;
        g_sed.motion_acc_ms  = 0;       /* 一回到静止，运动累计就清零 */

        /* 重度阈值：75 min */
        if (g_sed.state != SED_HEAVY_WARN &&
            g_sed.static_acc_ms >= SED_HEAVY_THRESHOLD_MS) {
            g_sed.state        = SED_HEAVY_WARN;
            g_sed.warn_level   = 2;
            g_sed.warn_pending = 1;
        }
        /* 轻度阈值：45 min（且尚未到重度） */
        else if (g_sed.state != SED_LIGHT_WARN &&
                 g_sed.state != SED_HEAVY_WARN &&
                 g_sed.static_acc_ms >= SED_LIGHT_THRESHOLD_MS) {
            g_sed.state        = SED_LIGHT_WARN;
            g_sed.warn_level   = 1;
            g_sed.warn_pending = 1;
        }
        /* 还没跨过任何阈值，从 IDLE 进入 SITTING */
        else if (g_sed.state == SED_IDLE) {
            g_sed.state = SED_SITTING;
        }
        return;
    }

    /* === 含运动：累积运动时间，连续动满 30s 才清零累积 === */
    if (state == 0x01 || state == 0x03) {
        g_sed.motion_acc_ms += delta;
        if (g_sed.motion_acc_ms >= SED_RESET_MOTION_MS) {
            g_sed.static_acc_ms = 0;
            g_sed.motion_acc_ms = 0;
            g_sed.state         = SED_IDLE;
        }
    }
}

uint32_t Sedentary_GetSittingSeconds(void)
{
    return g_sed.static_acc_ms / 1000;
}

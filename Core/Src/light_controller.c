#include "light_controller.h"
#include "app_state.h"

/* ═══════════════════════════════════════════════════════════════
 *  智能灯光模式控制
 *
 *  手动模式优先级最高，自动模式按以下判定:
 *
 *  1. 无人模式:  human_present = 0
 *     → 关灯 (0%) — 节能
 *
 *  2. 多人模式:  face_count ≥ 2
 *     → 全亮 (100%)，忽略lux
 *     — 会议/聚会场景优先保证光照充足
 *
 *  3. 单人模式:  face_count = 1
 *     → lux → PWM分档映射（精细调光）
 *         0~50 lux   → 90%
 *        50~150 lux  → 60%
 *       150~300 lux  → 30%
 *       300~500 lux  → 10%
 *         >500 lux   →  0%
 *     — 单人阅读/工作时眼睛对光线敏感，需根据环境光自动调节
 *
 *  4. 仅雷达模式: human_present = 1, face_count = 0
 *     → 固定 50%，不随lux变化
 *     — 人确实在但摄像头没拍到脸（背对/侧对），给舒适基础照明
 * ═══════════════════════════════════════════════════════════════ */
uint16_t LightCtrl_Update(SensorData_t *sensor)
{
    if (sensor == NULL) return 0;

    /* ── 手动模式: 直接返回用户设定的亮度 ── */
    if (g_app.manual_mode)
        return g_app.manual_brightness;

    /* ── 自动模式 1: 无人 → 关灯 ── */
    if (!sensor->human_present)
        return 0;

    /* ── 自动模式 2: 多人(≥2) → 全亮 ── */
    if (sensor->face_count >= 2)
        return 999;

    /* ── 自动模式 3: 单人 → lux分档精细调光 ── */
    if (sensor->face_count == 1)
    {
        if (sensor->lux < 50)        return 999 * 90 / 100;  //  90%
        else if (sensor->lux < 150)  return 999 * 60 / 100;  //  60%
        else if (sensor->lux < 300)  return 999 * 30 / 100;  //  30%
        else if (sensor->lux < 500)  return 999 * 10 / 100;  //  10%
        else                         return 0;                //   0%
    }

    /* ── 自动模式 4: 仅雷达(human=1, face=0) → 固定50% ── */
    return 999 * 50 / 100;
}

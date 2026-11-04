#include "pm_health.h"

#include <string.h>

/* Below this current the battery is considered idle for time estimates. */
#define PM_HEALTH_MIN_CURRENT_MA (5.0f)
/* Sanity limit for capacities (Flipper battery is 2100 mAh). */
#define PM_HEALTH_MAX_CAPACITY_MAH (20000u)

void pm_health_eval(const PmBatteryInfo* info, PmHealth* health) {
    memset(health, 0, sizeof(*health));

    /* The firmware driver returns 0 on I2C error. */
    if(info->soh_pct > 0 && info->soh_pct <= 100) {
        health->soh_valid = true;
        health->soh_pct = info->soh_pct;
    }

    const bool design_ok = info->design_mah > 0 && info->design_mah < PM_HEALTH_MAX_CAPACITY_MAH;
    const bool full_ok = info->full_mah > 0 && info->full_mah < PM_HEALTH_MAX_CAPACITY_MAH;
    health->capacity_valid = design_ok && full_ok && info->remaining_mah <= info->full_mah;

    if(design_ok && full_ok) {
        const float ratio = (float)info->full_mah * 100.0f / (float)info->design_mah;
        /* A learned FCC well above the design capacity means bad data. */
        if(ratio <= 150.0f) {
            health->ratio_valid = true;
            health->ratio_pct = ratio;
        }
    }
}

bool pm_health_time_to_empty(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated) {
    if(info->ext_valid) {
        if(info->tte_min == PM_GAUGE_TIME_NA) return false;
        *minutes = info->tte_min;
        *estimated = false;
        return true;
    }
    if(bat_current_ma > -PM_HEALTH_MIN_CURRENT_MA || info->remaining_mah == 0) return false;
    *minutes = (uint32_t)((float)info->remaining_mah * 60.0f / -bat_current_ma);
    *estimated = true;
    return true;
}

bool pm_health_time_to_full(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated) {
    if(info->ext_valid) {
        if(info->ttf_min == PM_GAUGE_TIME_NA) return false;
        *minutes = info->ttf_min;
        *estimated = false;
        return true;
    }
    if(bat_current_ma < PM_HEALTH_MIN_CURRENT_MA || info->full_mah <= info->remaining_mah) {
        return false;
    }
    *minutes = (uint32_t)((float)(info->full_mah - info->remaining_mah) * 60.0f / bat_current_ma);
    *estimated = true;
    return true;
}

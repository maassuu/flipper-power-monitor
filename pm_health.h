/**
 * @file pm_health.h
 * Battery health analysis based on BQ27220 data.
 *
 * Only values that the gauge really reports are shown. The FCC/design ratio
 * is computed only when both capacities are plausible. Note that the gauge
 * learns FullChargeCapacity only after qualified charge/discharge cycles, so
 * on a battery that never went through one, SOH stays close to 100 %.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pm_types.h"

typedef struct {
    bool soh_valid;
    uint8_t soh_pct; /**< StateOfHealth() reported by the gauge */

    bool ratio_valid;
    float ratio_pct; /**< FullChargeCapacity / DesignCapacity * 100 */

    bool capacity_valid; /**< remaining/full/design are plausible */
} PmHealth;

/** All flags stay false when info->gauge_ok is false. */
void pm_health_eval(const PmBatteryInfo* info, PmHealth* health);

/**
 * Time to empty, only while discharging (current below -5 mA) and with a
 * healthy gauge: gauge TimeToEmpty() when available (extended reads),
 * otherwise an estimate remaining / |current|.
 * @return false if not applicable (not discharging / gauge not OK / no data)
 *         or if bat_current_ma is NaN or infinite.
 */
bool pm_health_time_to_empty(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated);

/**
 * Time to full, only while charging (current above +5 mA) and with a
 * healthy gauge: gauge TimeToFull() when available, otherwise an estimate
 * (full - remaining) / current. The estimate ignores the constant-voltage
 * taper and is therefore optimistic. Returns false for NaN or infinite current.
 */
bool pm_health_time_to_full(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated);

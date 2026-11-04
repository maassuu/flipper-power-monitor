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

void pm_health_eval(const PmBatteryInfo* info, PmHealth* health);

/**
 * Time to empty: gauge TimeToEmpty() when available (extended reads),
 * otherwise an estimate remaining / |current| while discharging.
 * @return false if not applicable (not discharging / no data).
 */
bool pm_health_time_to_empty(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated);

/**
 * Time to full: gauge TimeToFull() when available, otherwise an estimate
 * (full - remaining) / current while charging. The estimate ignores the
 * constant-voltage taper and is therefore optimistic.
 */
bool pm_health_time_to_full(
    const PmBatteryInfo* info,
    float bat_current_ma,
    uint32_t* minutes,
    bool* estimated);

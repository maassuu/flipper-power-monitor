/**
 * @file pm_csv.h
 * CSV formatting (pure, no file I/O). File handling is in pm_logger.c.
 *
 * Columns:
 *   date, time, uptime_ms, usb_v, bat_v, bat_current_ma, charge_current_ma,
 *   bat_power_w, temp_c, soc_pct, charge_state, usb_present,
 *   e_charge_mwh, e_discharge_mwh
 *
 * bat_current_ma / bat_power_w are the BATTERY current/power (gauge),
 * positive when the battery is charging. They are not USB input values.
 */
#pragma once

#include <stddef.h>

#include "pm_charge.h"
#include "pm_energy.h"
#include "pm_types.h"

/** Maximum length of one CSV line including newline and NUL. */
#define PM_CSV_LINE_MAX (192u)

/** Header line (with trailing newline). Returns length or 0 on truncation. */
size_t pm_csv_header(char* buf, size_t size);

/** Data line (with trailing newline). Returns length or 0 on truncation. */
size_t pm_csv_row(
    char* buf,
    size_t size,
    const PmClock* clock,
    const PmSample* sample,
    PmChargePhase phase,
    const PmEnergy* energy);

/** Session file name: "pm_YYYYMMDD_HHMMSS.csv". Returns length or 0. */
size_t pm_csv_filename(char* buf, size_t size, const PmClock* clock);

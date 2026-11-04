/**
 * @file pm_hw.h
 * Hardware access. Read-only.
 *
 * Primary source is the official furi_hal_power API (exported in the SDK).
 * Values that furi_hal_power does not expose (gauge cycle count, time to
 * empty/full, BatteryStatus flags, charger REG0B/REG13) are read with the
 * exported furi_hal_i2c API on the internal power I2C bus, the same way the
 * firmware drivers do it. Only register READS are performed. No register is
 * ever written, and BQ25896 REG0C (clear-on-read fault latch, used by the
 * Power service for OTG protection) is never touched.
 */
#pragma once

#include <stdbool.h>
#include "pm_types.h"

/**
 * Read fast-changing values. Fills s->tick_ms with the given time.
 * @param ext  also read BQ25896 REG0B (charge phase, input type)
 */
void pm_hw_read_fast(PmSample* s, uint32_t now_ms, bool ext);

/**
 * Read slow-changing values.
 * @param ext  also perform extended read-only register access
 */
void pm_hw_read_slow(PmBatteryInfo* info, bool ext);

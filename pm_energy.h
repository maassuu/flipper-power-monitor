/**
 * @file pm_energy.h
 * Battery power and energy integration.
 *
 * Energy is the time integral of battery power P = U * I (trapezoidal rule
 * on the real time between samples, so a variable sampling interval is
 * handled correctly). Segments where power changes sign are split at the
 * zero crossing, so charge and discharge energy are accounted separately.
 *
 * This is BATTERY energy only. Energy drawn from USB cannot be measured on
 * Flipper Zero, because the BQ25896 charger has no input current ADC.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Gaps longer than this are not integrated (no data = no guessing). */
#define PM_ENERGY_MAX_GAP_MS (10000u)

typedef struct {
    double charge_mwh; /**< Energy that went into the battery, >= 0 */
    double discharge_mwh; /**< Energy taken out of the battery, >= 0 */
    double charge_mah; /**< Charge that went into the battery, >= 0 */
    double discharge_mah; /**< Charge taken out of the battery, >= 0 */
    uint32_t integrated_ms; /**< Total time covered by integration */
    uint32_t gaps; /**< Number of skipped segments (gap or invalid data) */

    bool has_prev;
    uint32_t prev_tick_ms;
    float prev_power_w;
    float prev_current_ma;
} PmEnergy;

/** Battery power in watts. Positive = into the battery. */
float pm_power_w(float voltage_v, float current_ma);

void pm_energy_reset(PmEnergy* energy);

/** Add a valid sample. */
void pm_energy_add(PmEnergy* energy, uint32_t tick_ms, float power_w, float current_ma);

/** Mark a discontinuity (invalid sample): the next sample starts a new segment. */
void pm_energy_break(PmEnergy* energy);

/** Net energy (charge - discharge) in mWh. */
double pm_energy_net_mwh(const PmEnergy* energy);

/**
 * Integrate a linear segment a->b over dt and split the area into the
 * positive and the negative part (both returned as non-negative numbers).
 * Unit of the result is unit(a) * unit(dt).
 */
void pm_split_trapezoid(float a, float b, double dt, double* positive, double* negative);

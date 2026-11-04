/**
 * @file pm_types.h
 * Shared data types. This header has no Furi dependencies so that the
 * calculation modules can be unit-tested on a host computer.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Sentinel used by the BQ27220 for "time not applicable" (TimeToEmpty/TimeToFull). */
#define PM_GAUGE_TIME_NA (0xFFFFu)

/** Charging phase, as far as the hardware lets us tell. */
typedef enum {
    PmPhaseNotCharging = 0, /**< CHRG_STAT = 00 */
    PmPhasePreCharge, /**< CHRG_STAT = 01 (needs extended reads) */
    PmPhaseFastCharge, /**< CHRG_STAT = 10 (needs extended reads) */
    PmPhaseCharging, /**< Charging, pre/fast phase unknown (extended reads off) */
    PmPhaseDone, /**< CHRG_STAT = 11, charge termination done */
    PmPhaseCount,
} PmChargePhase;

/** Fast-changing values, read every sampling period. */
typedef struct {
    uint32_t tick_ms; /**< Monotonic time of the sample (kernel tick, 1 kHz) */
    bool valid; /**< Gauge answered with plausible values */

    float usb_v; /**< VBUS from BQ25896 ADC [V], 0 when no valid input, 100 mV step */
    float bat_v; /**< Battery voltage from BQ27220 [V], 1 mV step */
    float bat_i_ma; /**< Battery current from BQ27220 [mA], + charging, - discharging */
    float chg_i_ma; /**< Charge current from BQ25896 ADC [mA], >= 0, 50 mA step */
    float bat_p_w; /**< Battery power P = U * I [W], + into battery */
    float temp_c; /**< Battery temperature from BQ27220 [C] */
    uint8_t soc_pct; /**< State of charge from BQ27220 [%] */

    bool usb_present; /**< VBUS_GD reported by charger (usb_v > 0) */
    bool otg_enabled; /**< Flipper is supplying 5 V on USB (OTG boost) */
    bool charging; /**< furi_hal_power_is_charging() (CHRG_STAT != 00) */
    bool charge_done; /**< furi_hal_power_is_charging_done() (CHRG_STAT == 11) */

    bool reg0b_valid; /**< chg_reg0b was read (extended reads enabled and OK) */
    uint8_t chg_reg0b; /**< BQ25896 REG0B raw (system status, no read side effects) */
} PmSample;

/** Slow-changing values, read once per second. */
typedef struct {
    bool gauge_ok; /**< furi_hal_power_gauge_is_ok() */
    uint16_t remaining_mah; /**< RemainingCapacity() */
    uint16_t full_mah; /**< FullChargeCapacity() */
    uint16_t design_mah; /**< DesignCapacity() */
    uint8_t soh_pct; /**< StateOfHealth() as reported by the gauge */
    float charge_v_limit; /**< Charger VREG setting [V] (read only) */

    /* Extended, read-only register access over the internal I2C bus.
     * Only filled when ext_valid is true. */
    bool ext_valid;
    uint16_t cycle_count; /**< BQ27220 CycleCount() */
    uint16_t tte_min; /**< BQ27220 TimeToEmpty() [min], PM_GAUGE_TIME_NA if n/a */
    uint16_t ttf_min; /**< BQ27220 TimeToFull() [min], PM_GAUGE_TIME_NA if n/a */
    uint16_t bat_status; /**< BQ27220 BatteryStatus() raw */
    uint8_t chg_reg13; /**< BQ25896 REG13 raw (DPM status) */
} PmBatteryInfo;

/** Wall clock time, independent from Furi's DateTime. */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} PmClock;

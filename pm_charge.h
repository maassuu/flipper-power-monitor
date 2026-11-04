/**
 * @file pm_charge.h
 * Charging diagnostics: register decoding, USB/charge events, charging
 * time, charge current trend.
 *
 * Register layouts follow the firmware drivers (lib/drivers/bq25896_reg.h,
 * lib/drivers/bq27220.h). Only status registers without side effects are
 * decoded. BQ25896 REG0C (faults) is intentionally never read: its bits are
 * latched and cleared on read, and the firmware Power service relies on it
 * to detect OTG faults.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pm_types.h"

/* ---- BQ25896 REG0B (system status) ---- */
#define PM_REG0B_VSYS_STAT(r) ((uint8_t)((r) & 0x01u))
#define PM_REG0B_PG_STAT(r)   ((uint8_t)(((r) >> 2) & 0x01u))
#define PM_REG0B_CHRG_STAT(r) ((uint8_t)(((r) >> 3) & 0x03u))
#define PM_REG0B_VBUS_STAT(r) ((uint8_t)(((r) >> 5) & 0x07u))

/* ---- BQ25896 REG13 (DPM status) ---- */
#define PM_REG13_IDPM_LIM(r)  ((uint8_t)((r) & 0x3Fu))
#define PM_REG13_IDPM_STAT(r) ((uint8_t)(((r) >> 6) & 0x01u))
#define PM_REG13_VDPM_STAT(r) ((uint8_t)(((r) >> 7) & 0x01u))

/* ---- BQ27220 BatteryStatus bits ---- */
#define PM_BS_DSG      (1u << 0) /**< Discharging */
#define PM_BS_SYSDWN   (1u << 1) /**< System should shut down */
#define PM_BS_TDA      (1u << 2) /**< Terminate discharge alarm */
#define PM_BS_BATTPRES (1u << 3) /**< Battery present */
#define PM_BS_TCA      (1u << 6) /**< Terminate charge alarm */
#define PM_BS_CHGINH   (1u << 8) /**< Charge inhibit (temperature) */
#define PM_BS_FC       (1u << 9) /**< Full charge detected */
#define PM_BS_OTD      (1u << 10) /**< Over-temperature in discharge */
#define PM_BS_OTC      (1u << 11) /**< Over-temperature in charge */
#define PM_BS_FD       (1u << 15) /**< Full discharge detected */

/** Input current limit in effect [mA], from REG13 IDPM_LIM (100 mA + 50 mA/LSB). */
uint16_t pm_charger_input_limit_ma(uint8_t reg13);

/** Charge phase from the official HAL flags, refined by REG0B when available. */
PmChargePhase pm_charge_phase_from(bool charging, bool done, bool ext_valid, uint8_t reg0b);

const char* pm_charge_phase_name(PmChargePhase phase);
/** Short (<= 4 chars) name for the dashboard. */
const char* pm_charge_phase_short(PmChargePhase phase);
/** Machine friendly name for CSV. */
const char* pm_charge_phase_csv(PmChargePhase phase);

/** Name of the BQ25896 VBUS_STAT input type. */
const char* pm_vbus_type_name(uint8_t vbus_stat);

/**
 * Human readable list of active status/alarm flags (gauge BatteryStatus,
 * charger DPM, gauge health). Writes "none" when nothing is active.
 */
void pm_charge_flags_text(
    char* buf,
    size_t size,
    bool gauge_ok,
    bool ext_valid,
    uint16_t bat_status,
    uint8_t reg13);

typedef enum {
    PmEventUsbConnected,
    PmEventUsbDisconnected,
    PmEventChargeStarted,
    PmEventFastCharge,
    PmEventChargeDone,
    PmEventChargeStopped,
    PmEventOtgOn,
    PmEventOtgOff,
    PmEventCount,
} PmEventType;

const char* pm_event_name(PmEventType type);

typedef struct {
    PmEventType type;
    uint32_t tick_ms;
} PmEvent;

#define PM_EVENT_LOG_SIZE (6u)
/** Trend buffer: one point per second, compare last 10 s with previous 10 s. */
#define PM_TREND_POINTS    (20u)
#define PM_TREND_THRESHOLD (25.0f) /* mA */

typedef enum {
    PmTrendUnknown,
    PmTrendRising,
    PmTrendFalling,
    PmTrendStable,
} PmTrend;

const char* pm_trend_name(PmTrend trend);

typedef struct {
    bool initialized;
    bool usb_present;
    bool otg;
    PmChargePhase phase;

    bool session_active; /**< Currently charging (pre/fast/unknown phase) */
    uint32_t session_start_ms;
    uint32_t last_session_ms; /**< Duration of the last finished session */
    bool has_last_session;
    float peak_current_ma; /**< Peak battery current in current/last session */

    float trend[PM_TREND_POINTS];
    uint8_t trend_head;
    uint8_t trend_count;
    uint32_t trend_last_ms;

    PmEvent events[PM_EVENT_LOG_SIZE];
    uint8_t event_head;
    uint8_t event_count;
} PmCharge;

void pm_charge_reset(PmCharge* charge);

/** Clear only the event log. */
void pm_charge_clear_events(PmCharge* charge);

/**
 * Feed one sample.
 * @param bat_current_ma signed gauge current (+ = charging)
 */
void pm_charge_update(
    PmCharge* charge,
    uint32_t now_ms,
    bool usb_present,
    bool otg,
    PmChargePhase phase,
    float bat_current_ma);

/** Elapsed time of the running session, or duration of the last one. */
uint32_t pm_charge_session_ms(const PmCharge* charge, uint32_t now_ms);

PmTrend pm_charge_trend(const PmCharge* charge);

uint8_t pm_charge_event_count(const PmCharge* charge);
/** Event by index: 0 = newest. */
const PmEvent* pm_charge_event(const PmCharge* charge, uint8_t index);

/** True for phases in which the charger is actively charging. */
bool pm_phase_is_charging(PmChargePhase phase);

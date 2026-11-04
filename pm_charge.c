#include "pm_charge.h"

#include <stdio.h>
#include <string.h>

uint16_t pm_charger_input_limit_ma(uint8_t reg13) {
    return (uint16_t)(100u + 50u * PM_REG13_IDPM_LIM(reg13));
}

PmChargePhase pm_charge_phase_from(bool charging, bool done, bool ext_valid, uint8_t reg0b) {
    if(ext_valid) {
        switch(PM_REG0B_CHRG_STAT(reg0b)) {
        case 1:
            return PmPhasePreCharge;
        case 2:
            return PmPhaseFastCharge;
        case 3:
            return PmPhaseDone;
        default:
            return PmPhaseNotCharging;
        }
    }
    if(!charging) return PmPhaseNotCharging;
    if(done) return PmPhaseDone;
    return PmPhaseCharging;
}

bool pm_phase_is_charging(PmChargePhase phase) {
    return phase == PmPhasePreCharge || phase == PmPhaseFastCharge || phase == PmPhaseCharging;
}

const char* pm_charge_phase_name(PmChargePhase phase) {
    switch(phase) {
    case PmPhaseNotCharging:
        return "Not charging";
    case PmPhasePreCharge:
        return "Pre-charge";
    case PmPhaseFastCharge:
        return "Fast charge";
    case PmPhaseCharging:
        return "Charging";
    case PmPhaseDone:
        return "Charge done";
    default:
        return "?";
    }
}

const char* pm_charge_phase_short(PmChargePhase phase) {
    switch(phase) {
    case PmPhaseNotCharging:
        return "DIS";
    case PmPhasePreCharge:
        return "PRE";
    case PmPhaseFastCharge:
        return "FAST";
    case PmPhaseCharging:
        return "CHG";
    case PmPhaseDone:
        return "FULL";
    default:
        return "?";
    }
}

const char* pm_charge_phase_csv(PmChargePhase phase) {
    switch(phase) {
    case PmPhaseNotCharging:
        return "not_charging";
    case PmPhasePreCharge:
        return "precharge";
    case PmPhaseFastCharge:
        return "fast";
    case PmPhaseCharging:
        return "charging";
    case PmPhaseDone:
        return "done";
    default:
        return "unknown";
    }
}

const char* pm_vbus_type_name(uint8_t vbus_stat) {
    switch(vbus_stat) {
    case 0:
        return "none";
    case 1:
        return "USB host";
    case 2:
        return "adapter";
    case 7:
        return "OTG";
    default:
        return "unknown";
    }
}

static void pm_flags_append(char* buf, size_t size, size_t* len, const char* text) {
    if(*len >= size) return;
    const int n = snprintf(buf + *len, size - *len, "%s%s", (*len > 0) ? " " : "", text);
    if(n > 0) {
        *len += (size_t)n;
        if(*len >= size) *len = size - 1;
    }
}

void pm_charge_flags_text(
    char* buf,
    size_t size,
    bool gauge_ok,
    bool ext_valid,
    uint16_t bat_status,
    uint8_t reg13) {
    if(size == 0) return;
    buf[0] = '\0';
    size_t len = 0;

    if(!gauge_ok) pm_flags_append(buf, size, &len, "GAUGE!");
    if(ext_valid) {
        if(!(bat_status & PM_BS_BATTPRES)) pm_flags_append(buf, size, &len, "NOBAT");
        if(bat_status & PM_BS_OTC) pm_flags_append(buf, size, &len, "OTC");
        if(bat_status & PM_BS_OTD) pm_flags_append(buf, size, &len, "OTD");
        if(bat_status & PM_BS_CHGINH) pm_flags_append(buf, size, &len, "CHGINH");
        if(bat_status & PM_BS_TCA) pm_flags_append(buf, size, &len, "TCA");
        if(bat_status & PM_BS_TDA) pm_flags_append(buf, size, &len, "TDA");
        if(bat_status & PM_BS_SYSDWN) pm_flags_append(buf, size, &len, "SYSDWN");
        if(bat_status & PM_BS_FC) pm_flags_append(buf, size, &len, "FC");
        if(bat_status & PM_BS_FD) pm_flags_append(buf, size, &len, "FD");
        if(PM_REG13_IDPM_STAT(reg13)) pm_flags_append(buf, size, &len, "IINDPM");
        if(PM_REG13_VDPM_STAT(reg13)) pm_flags_append(buf, size, &len, "VINDPM");
    }
    if(len == 0) snprintf(buf, size, "none");
}

const char* pm_event_name(PmEventType type) {
    switch(type) {
    case PmEventUsbConnected:
        return "USB connected";
    case PmEventUsbDisconnected:
        return "USB removed";
    case PmEventChargeStarted:
        return "Charge start";
    case PmEventFastCharge:
        return "Fast charge";
    case PmEventChargeDone:
        return "Charge done";
    case PmEventChargeStopped:
        return "Charge stop";
    case PmEventOtgOn:
        return "OTG on";
    case PmEventOtgOff:
        return "OTG off";
    default:
        return "?";
    }
}

const char* pm_trend_name(PmTrend trend) {
    switch(trend) {
    case PmTrendRising:
        return "rising";
    case PmTrendFalling:
        return "falling";
    case PmTrendStable:
        return "stable";
    default:
        return "--";
    }
}

void pm_charge_reset(PmCharge* charge) {
    memset(charge, 0, sizeof(*charge));
}

void pm_charge_clear_events(PmCharge* charge) {
    charge->event_head = 0;
    charge->event_count = 0;
}

static void pm_charge_log(PmCharge* charge, PmEventType type, uint32_t now_ms) {
    charge->events[charge->event_head].type = type;
    charge->events[charge->event_head].tick_ms = now_ms;
    charge->event_head = (uint8_t)((charge->event_head + 1u) % PM_EVENT_LOG_SIZE);
    if(charge->event_count < PM_EVENT_LOG_SIZE) charge->event_count++;
}

static void pm_charge_trend_reset(PmCharge* charge) {
    charge->trend_head = 0;
    charge->trend_count = 0;
}

static void pm_charge_session_start(PmCharge* charge, uint32_t now_ms) {
    charge->session_active = true;
    charge->session_start_ms = now_ms;
    charge->peak_current_ma = 0.0f;
    pm_charge_trend_reset(charge);
}

static void pm_charge_session_end(PmCharge* charge, uint32_t now_ms) {
    if(!charge->session_active) return;
    charge->session_active = false;
    charge->last_session_ms = now_ms - charge->session_start_ms;
    charge->has_last_session = true;
}

void pm_charge_update(
    PmCharge* charge,
    uint32_t now_ms,
    bool usb_present,
    bool otg,
    PmChargePhase phase,
    float bat_current_ma) {
    if(!charge->initialized) {
        charge->initialized = true;
        charge->usb_present = usb_present;
        charge->otg = otg;
        charge->phase = phase;
        if(pm_phase_is_charging(phase)) pm_charge_session_start(charge, now_ms);
    } else {
        if(otg != charge->otg) {
            pm_charge_log(charge, otg ? PmEventOtgOn : PmEventOtgOff, now_ms);
            charge->otg = otg;
        }
        if(usb_present != charge->usb_present) {
            pm_charge_log(
                charge, usb_present ? PmEventUsbConnected : PmEventUsbDisconnected, now_ms);
            charge->usb_present = usb_present;
            pm_charge_trend_reset(charge);
        }

        const PmChargePhase old = charge->phase;
        if(phase != old) {
            const bool was = pm_phase_is_charging(old);
            const bool is = pm_phase_is_charging(phase);
            if(!was && is) {
                pm_charge_log(charge, PmEventChargeStarted, now_ms);
                pm_charge_session_start(charge, now_ms);
            } else if(was && !is) {
                pm_charge_log(
                    charge,
                    (phase == PmPhaseDone) ? PmEventChargeDone : PmEventChargeStopped,
                    now_ms);
                pm_charge_session_end(charge, now_ms);
            } else if(!was && !is && phase == PmPhaseDone) {
                pm_charge_log(charge, PmEventChargeDone, now_ms);
            }
            if(phase == PmPhaseFastCharge && old == PmPhasePreCharge) {
                pm_charge_log(charge, PmEventFastCharge, now_ms);
            }
            charge->phase = phase;
        }
    }

    if(charge->session_active && bat_current_ma > charge->peak_current_ma) {
        charge->peak_current_ma = bat_current_ma;
    }

    /* Trend: one point per second while external power is present */
    if(usb_present &&
       (charge->trend_count == 0 || (now_ms - charge->trend_last_ms) >= 1000u)) {
        charge->trend[charge->trend_head] = bat_current_ma;
        charge->trend_head = (uint8_t)((charge->trend_head + 1u) % PM_TREND_POINTS);
        if(charge->trend_count < PM_TREND_POINTS) charge->trend_count++;
        charge->trend_last_ms = now_ms;
    }
}

uint32_t pm_charge_session_ms(const PmCharge* charge, uint32_t now_ms) {
    if(charge->session_active) return now_ms - charge->session_start_ms;
    if(charge->has_last_session) return charge->last_session_ms;
    return 0;
}

PmTrend pm_charge_trend(const PmCharge* charge) {
    if(charge->trend_count < PM_TREND_POINTS) return PmTrendUnknown;
    const uint8_t half = PM_TREND_POINTS / 2u;
    double older = 0;
    double newer = 0;
    /* trend_head points at the oldest entry when the buffer is full */
    for(uint8_t i = 0; i < PM_TREND_POINTS; i++) {
        const float v = charge->trend[(charge->trend_head + i) % PM_TREND_POINTS];
        if(i < half) {
            older += (double)v;
        } else {
            newer += (double)v;
        }
    }
    const double diff = (newer - older) / (double)half;
    if(diff > (double)PM_TREND_THRESHOLD) return PmTrendRising;
    if(diff < -(double)PM_TREND_THRESHOLD) return PmTrendFalling;
    return PmTrendStable;
}

uint8_t pm_charge_event_count(const PmCharge* charge) {
    return charge->event_count;
}

const PmEvent* pm_charge_event(const PmCharge* charge, uint8_t index) {
    if(index >= charge->event_count) return NULL;
    const uint8_t pos =
        (uint8_t)((charge->event_head + PM_EVENT_LOG_SIZE - 1u - index) % PM_EVENT_LOG_SIZE);
    return &charge->events[pos];
}

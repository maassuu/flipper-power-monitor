#include "pm_csv.h"

#include <stdio.h>

static size_t pm_csv_result(int n, size_t size) {
    if(n < 0 || (size_t)n >= size) return 0;
    return (size_t)n;
}

size_t pm_csv_header(char* buf, size_t size) {
    const int n = snprintf(
        buf,
        size,
        "date,time,uptime_ms,usb_v,bat_v,bat_current_ma,charge_current_ma,bat_power_w,"
        "temp_c,soc_pct,charge_state,usb_present,e_charge_mwh,e_discharge_mwh\n");
    return pm_csv_result(n, size);
}

size_t pm_csv_row(
    char* buf,
    size_t size,
    const PmClock* clock,
    const PmSample* sample,
    PmChargePhase phase,
    const PmEnergy* energy) {
    /* Gauge read failed: the values are not measurements, do not log them */
    if(!sample->valid) {
        if(size > 0) buf[0] = '\0';
        return 0;
    }
    const int n = snprintf(
        buf,
        size,
        "%04u-%02u-%02u,%02u:%02u:%02u,%lu,%.1f,%.3f,%.0f,%.0f,%.3f,%.1f,%u,%s,%u,%.3f,%.3f\n",
        (unsigned)clock->year,
        (unsigned)clock->month,
        (unsigned)clock->day,
        (unsigned)clock->hour,
        (unsigned)clock->minute,
        (unsigned)clock->second,
        (unsigned long)sample->tick_ms,
        (double)sample->usb_v,
        (double)sample->bat_v,
        (double)sample->bat_i_ma,
        (double)sample->chg_i_ma,
        (double)sample->bat_p_w,
        (double)sample->temp_c,
        (unsigned)sample->soc_pct,
        pm_charge_phase_csv(phase),
        sample->usb_present ? 1u : 0u,
        energy->charge_mwh,
        energy->discharge_mwh);
    return pm_csv_result(n, size);
}

size_t pm_csv_filename(char* buf, size_t size, const PmClock* clock) {
    const int n = snprintf(
        buf,
        size,
        "pm_%04u%02u%02u_%02u%02u%02u.csv",
        (unsigned)clock->year,
        (unsigned)clock->month,
        (unsigned)clock->day,
        (unsigned)clock->hour,
        (unsigned)clock->minute,
        (unsigned)clock->second);
    return pm_csv_result(n, size);
}

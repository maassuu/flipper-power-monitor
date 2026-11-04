#include "pm_format.h"

#include <stdio.h>

void pm_fmt_duration(char* buf, size_t size, uint32_t ms) {
    const uint32_t total_s = ms / 1000u;
    const uint32_t days = total_s / 86400u;
    const uint32_t h = (total_s / 3600u) % 24u;
    const uint32_t m = (total_s / 60u) % 60u;
    const uint32_t s = total_s % 60u;
    if(days > 0) {
        snprintf(buf, size, "%lud %02lu:%02lu", (unsigned long)days, (unsigned long)h, (unsigned long)m);
    } else {
        snprintf(buf, size, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    }
}

void pm_fmt_minutes(char* buf, size_t size, uint32_t minutes) {
    if(minutes == UINT32_MAX) {
        snprintf(buf, size, "--");
    } else if(minutes >= 60u) {
        snprintf(
            buf, size, "%luh%02lum", (unsigned long)(minutes / 60u), (unsigned long)(minutes % 60u));
    } else {
        snprintf(buf, size, "%lum", (unsigned long)minutes);
    }
}

void pm_fmt_energy(char* buf, size_t size, double mwh) {
    const double mag = (mwh < 0.0) ? -mwh : mwh;
    if(mag >= 1000.0) {
        snprintf(buf, size, "%.4fWh", mwh / 1000.0);
    } else {
        snprintf(buf, size, "%.2fmWh", mwh);
    }
}

void pm_fmt_current(char* buf, size_t size, float ma) {
    const long v = (long)((ma >= 0.0f) ? (ma + 0.5f) : (ma - 0.5f));
    if(v > 0) {
        snprintf(buf, size, "+%ldmA", v);
    } else {
        snprintf(buf, size, "%ldmA", v);
    }
}

void pm_fmt_power(char* buf, size_t size, float w) {
    /* Avoid "-0.00W" */
    if(w > -0.005f && w < 0.005f) w = 0.0f;
    snprintf(buf, size, (w > 0.0f) ? "+%.2fW" : "%.2fW", (double)w);
}

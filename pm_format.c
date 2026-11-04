#include "pm_format.h"

#include <stdio.h>
#include <string.h>

void pm_fmt_duration(char* buf, size_t size, uint64_t ms) {
    const uint64_t total_s = ms / 1000u;
    const unsigned long days = (unsigned long)(total_s / 86400u);
    const unsigned long h = (unsigned long)((total_s / 3600u) % 24u);
    const unsigned long m = (unsigned long)((total_s / 60u) % 60u);
    const unsigned long s = (unsigned long)(total_s % 60u);
    if(days > 0) {
        snprintf(buf, size, "%lud %02lu:%02lu", days, h, m);
    } else {
        snprintf(buf, size, "%02lu:%02lu:%02lu", h, m, s);
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
    /* integer literals: the SDK builds with -fsingle-precision-constant */
    const double mag = (mwh < 0) ? -mwh : mwh;
    if(mag >= 1000) {
        snprintf(buf, size, "%.4fWh", mwh / 1000);
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
    /* Format the magnitude first and decide the sign from the rounded text,
     * so values that round to zero never print as "-0.00W" or "+0.00W". */
    char mag[16];
    const float a = (w < 0.0f) ? -w : w;
    snprintf(mag, sizeof(mag), "%.2f", (double)a);
    if(strcmp(mag, "0.00") == 0) {
        snprintf(buf, size, "0.00W");
    } else {
        snprintf(buf, size, "%c%sW", (w < 0.0f) ? '-' : '+', mag);
    }
}

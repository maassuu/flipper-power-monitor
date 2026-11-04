#include "pm_stats.h"
#include "pm_energy.h" /* PM_ENERGY_MAX_GAP_MS */

#include <string.h>

void pm_stats_reset(PmStats* stats, uint32_t now_ms) {
    memset(stats, 0, sizeof(*stats));
    stats->start_tick_ms = now_ms;
    stats->last_tick_ms = now_ms;
}

void pm_stats_break(PmStats* stats) {
    stats->has_prev = false;
}

void pm_stats_add(PmStats* stats, uint32_t tick_ms, const float values[PmChannelCount]) {
    uint32_t dt_ms = 0;
    if(stats->has_prev) {
        dt_ms = tick_ms - stats->last_tick_ms;
        if(dt_ms > PM_ENERGY_MAX_GAP_MS) dt_ms = 0;
    }

    for(size_t i = 0; i < PmChannelCount; i++) {
        PmStat* st = &stats->ch[i];
        const float v = values[i];
        if(stats->samples == 0) {
            st->min = v;
            st->max = v;
        } else {
            if(v < st->min) st->min = v;
            if(v > st->max) st->max = v;
        }
        if(dt_ms > 0) {
            st->integral += ((double)st->last + (double)v) * 0.5 * (double)dt_ms;
        }
        st->last = v;
    }

    stats->weighted_ms += dt_ms;
    stats->samples++;
    stats->last_tick_ms = tick_ms;
    stats->has_prev = true;
}

float pm_stats_avg(const PmStats* stats, PmChannel channel) {
    if(stats->samples == 0) return 0.0f;
    if(stats->weighted_ms == 0) return stats->ch[channel].last;
    return (float)(stats->ch[channel].integral / (double)stats->weighted_ms);
}

uint32_t pm_stats_duration_ms(const PmStats* stats, uint32_t now_ms) {
    return now_ms - stats->start_tick_ms;
}

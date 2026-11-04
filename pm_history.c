#include "pm_history.h"

#include <string.h>

void pm_history_reset(PmHistory* history) {
    history->head = 0;
    history->count = 0;
}

void pm_history_push(PmHistory* history, uint32_t tick_ms, const float values[PmChannelCount]) {
    PmHistoryPoint* p = &history->points[history->head];
    p->tick_ms = tick_ms;
    memcpy(p->value, values, sizeof(p->value));
    history->head = (uint16_t)((history->head + 1u) % PM_HISTORY_CAPACITY);
    if(history->count < PM_HISTORY_CAPACITY) history->count++;
}

uint16_t pm_history_count(const PmHistory* history) {
    return history->count;
}

const PmHistoryPoint* pm_history_get(const PmHistory* history, uint16_t index) {
    if(index >= history->count) return NULL;
    const uint16_t oldest =
        (uint16_t)((history->head + PM_HISTORY_CAPACITY - history->count) % PM_HISTORY_CAPACITY);
    return &history->points[(oldest + index) % PM_HISTORY_CAPACITY];
}

uint32_t pm_history_age_ms(uint32_t now_ms, uint32_t tick_ms) {
    const int32_t age = (int32_t)(now_ms - tick_ms);
    return (age > 0) ? (uint32_t)age : 0u;
}

bool pm_history_range(
    const PmHistory* history,
    PmChannel channel,
    uint32_t now_ms,
    uint32_t window_ms,
    float* min,
    float* max) {
    bool found = false;
    for(uint16_t i = 0; i < history->count; i++) {
        const PmHistoryPoint* p = pm_history_get(history, i);
        if(pm_history_age_ms(now_ms, p->tick_ms) > window_ms) continue;
        const float v = p->value[channel];
        if(!found) {
            *min = v;
            *max = v;
            found = true;
        } else {
            if(v < *min) *min = v;
            if(v > *max) *max = v;
        }
    }
    return found;
}

void pm_autoscale(float data_min, float data_max, float min_span, float* lo, float* hi) {
    if(data_max < data_min) {
        const float t = data_min;
        data_min = data_max;
        data_max = t;
    }
    float span = data_max - data_min;
    if(span < min_span) {
        const float center = (data_max + data_min) * 0.5f;
        data_min = center - min_span * 0.5f;
        data_max = center + min_span * 0.5f;
        span = min_span;
    }
    const float pad = span * 0.1f;
    *lo = data_min - pad;
    *hi = data_max + pad;
}

int32_t pm_scale_y(float value, float lo, float hi, int32_t y_top, int32_t y_bottom) {
    if(hi <= lo) return (y_top + y_bottom) / 2;
    float rel = (value - lo) / (hi - lo);
    if(rel < 0.0f) rel = 0.0f;
    if(rel > 1.0f) rel = 1.0f;
    const float y = (float)y_bottom - rel * (float)(y_bottom - y_top);
    return (int32_t)(y + 0.5f);
}

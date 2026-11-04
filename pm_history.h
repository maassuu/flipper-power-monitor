/**
 * @file pm_history.h
 * Ring buffer of recent samples for the live graphs, plus axis autoscaling.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pm_stats.h" /* PmChannel */

/** Graph time window. */
#define PM_HISTORY_WINDOW_MS (60000u)
/** 60 s at the fastest 250 ms interval = 240 points, plus some headroom. */
#define PM_HISTORY_CAPACITY (256u)

typedef struct {
    uint32_t tick_ms;
    float value[PmChannelCount];
} PmHistoryPoint;

typedef struct {
    PmHistoryPoint points[PM_HISTORY_CAPACITY];
    uint16_t head; /**< Index of the next write */
    uint16_t count;
} PmHistory;

void pm_history_reset(PmHistory* history);

void pm_history_push(PmHistory* history, uint32_t tick_ms, const float values[PmChannelCount]);

/** Number of stored points. */
uint16_t pm_history_count(const PmHistory* history);

/** Point by age order: 0 = oldest, count-1 = newest. */
const PmHistoryPoint* pm_history_get(const PmHistory* history, uint16_t index);

/**
 * Age of a point in ms (wrap-safe). A point stamped slightly after now_ms
 * (now read before the newest sample was stored) has age 0 instead of a
 * huge wrapped value.
 */
uint32_t pm_history_age_ms(uint32_t now_ms, uint32_t tick_ms);

/**
 * Min / max of a channel over points not older than window_ms before now_ms.
 * @return false if there are no points in the window.
 */
bool pm_history_range(
    const PmHistory* history,
    PmChannel channel,
    uint32_t now_ms,
    uint32_t window_ms,
    float* min,
    float* max);

/**
 * Compute a display range from data min/max: enforces a minimum span (so
 * sensor noise is not magnified into full-scale swings) and adds 10 %
 * padding on both sides.
 */
void pm_autoscale(float data_min, float data_max, float min_span, float* lo, float* hi);

/** Map a value into pixel row: lo -> y_bottom, hi -> y_top. Clamped. */
int32_t pm_scale_y(float value, float lo, float hi, int32_t y_top, int32_t y_bottom);

/**
 * @file pm_stats.h
 * MIN / MAX / time-weighted AVG statistics for voltage, current and power.
 *
 * The average is weighted by the real time between samples (trapezoidal
 * rule), so it stays correct when the sampling interval is changed during a
 * session.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PmChannelVoltage = 0,
    PmChannelCurrent,
    PmChannelPower,
    PmChannelCount,
} PmChannel;

typedef struct {
    float min;
    float max;
    float last;
    double integral; /**< value * ms */
} PmStat;

typedef struct {
    PmStat ch[PmChannelCount];
    uint32_t samples;
    uint32_t start_tick_ms;
    uint32_t last_tick_ms;
    uint64_t weighted_ms; /**< Time covered by the integral (64-bit: no wrap after 49.7 days) */
    bool has_prev;
} PmStats;

void pm_stats_reset(PmStats* stats, uint32_t now_ms);

/** Add one valid sample (values indexed by PmChannel). */
void pm_stats_add(PmStats* stats, uint32_t tick_ms, const float values[PmChannelCount]);

/** Mark a discontinuity: next sample will not be connected to the previous one. */
void pm_stats_break(PmStats* stats);

/** Time-weighted average; falls back to the last value with a single sample. */
float pm_stats_avg(const PmStats* stats, PmChannel channel);

/** Session duration since reset. */
uint32_t pm_stats_duration_ms(const PmStats* stats, uint32_t now_ms);

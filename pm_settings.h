/**
 * @file pm_settings.h
 * Application settings: value tables and validation (pure).
 * Persistence is in pm_settings_store.c.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t sample_idx; /**< Index into the sampling interval table */
    uint8_t csv_idx; /**< Index into the CSV interval table */
    uint8_t ext_reads; /**< 1 = extended read-only register access */
    uint8_t backlight; /**< 1 = keep backlight on */
} PmSettings;

typedef enum {
    PmSettingSample = 0,
    PmSettingCsvInterval,
    PmSettingCsvLog, /**< Runtime toggle, not persisted */
    PmSettingExtReads,
    PmSettingBacklight,
    PmSettingCount,
} PmSettingItem;

void pm_settings_defaults(PmSettings* settings);

/** Fix out-of-range values (e.g. after loading a file from an older version). */
void pm_settings_sanitize(PmSettings* settings);

uint32_t pm_settings_sample_ms(const PmSettings* settings);
uint32_t pm_settings_csv_ms(const PmSettings* settings);

/** Advance the given item to its next value (wraps around). CsvLog is not handled here. */
void pm_settings_cycle(PmSettings* settings, PmSettingItem item);

const char* pm_settings_label(PmSettingItem item);

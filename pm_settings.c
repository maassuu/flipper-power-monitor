#include "pm_settings.h"

#define PM_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const uint32_t pm_sample_ms[] = {250, 500, 1000};
static const uint32_t pm_csv_ms[] = {1000, 5000, 10000, 30000, 60000};

void pm_settings_defaults(PmSettings* settings) {
    settings->sample_idx = 1; /* 500 ms */
    settings->csv_idx = 1; /* 5 s */
    settings->ext_reads = 1;
    settings->backlight = 0;
}

void pm_settings_sanitize(PmSettings* settings) {
    if(settings->sample_idx >= PM_COUNT_OF(pm_sample_ms)) settings->sample_idx = 1;
    if(settings->csv_idx >= PM_COUNT_OF(pm_csv_ms)) settings->csv_idx = 1;
    settings->ext_reads = settings->ext_reads ? 1 : 0;
    settings->backlight = settings->backlight ? 1 : 0;
}

uint32_t pm_settings_sample_ms(const PmSettings* settings) {
    const uint8_t i = (settings->sample_idx < PM_COUNT_OF(pm_sample_ms)) ? settings->sample_idx : 1;
    return pm_sample_ms[i];
}

uint32_t pm_settings_csv_ms(const PmSettings* settings) {
    const uint8_t i = (settings->csv_idx < PM_COUNT_OF(pm_csv_ms)) ? settings->csv_idx : 1;
    return pm_csv_ms[i];
}

void pm_settings_cycle(PmSettings* settings, PmSettingItem item) {
    switch(item) {
    case PmSettingSample:
        settings->sample_idx = (uint8_t)((settings->sample_idx + 1u) % PM_COUNT_OF(pm_sample_ms));
        break;
    case PmSettingCsvInterval:
        settings->csv_idx = (uint8_t)((settings->csv_idx + 1u) % PM_COUNT_OF(pm_csv_ms));
        break;
    case PmSettingExtReads:
        settings->ext_reads = settings->ext_reads ? 0 : 1;
        break;
    case PmSettingBacklight:
        settings->backlight = settings->backlight ? 0 : 1;
        break;
    default:
        break;
    }
}

const char* pm_settings_label(PmSettingItem item) {
    switch(item) {
    case PmSettingSample:
        return "Sample every";
    case PmSettingCsvInterval:
        return "CSV every";
    case PmSettingCsvLog:
        return "CSV logging";
    case PmSettingExtReads:
        return "Ext. regs";
    case PmSettingBacklight:
        return "Backlight";
    default:
        return "?";
    }
}

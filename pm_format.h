/**
 * @file pm_format.h
 * Text formatting helpers for the UI and CSV.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/** "HH:MM:SS", or "Dd HH:MM" from 24 h up. */
void pm_fmt_duration(char* buf, size_t size, uint32_t ms);

/** "5h12m", "45m", or "--" when minutes is UINT32_MAX. */
void pm_fmt_minutes(char* buf, size_t size, uint32_t minutes);

/** Energy: "12.34mWh" below 1000 mWh, "1.2345Wh" above. Sign kept for negatives. */
void pm_fmt_energy(char* buf, size_t size, double mwh);

/** Signed integer milliamps: "+420mA", "-35mA", "0mA". */
void pm_fmt_current(char* buf, size_t size, float ma);

/** Signed watts with 2 decimals: "+1.73W". */
void pm_fmt_power(char* buf, size_t size, float w);

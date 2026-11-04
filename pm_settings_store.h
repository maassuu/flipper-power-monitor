/**
 * @file pm_settings_store.h
 * Settings persistence on the SD card (saved_struct format).
 */
#pragma once

#include <stdbool.h>
#include <storage/storage.h>

#include "pm_settings.h"

/** Load settings; on any error (no card, no file, old version) use defaults. */
void pm_settings_load(PmSettings* settings);

bool pm_settings_save(Storage* storage, const PmSettings* settings);

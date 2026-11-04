/**
 * @file pm_logger.h
 * CSV session files on the microSD card.
 * Files go to /ext/apps_data/power_monitor/pm_YYYYMMDD_HHMMSS.csv
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#include "pm_types.h"

#define PM_DATA_DIR EXT_PATH("apps_data/power_monitor")

typedef enum {
    PmLogOk,
    PmLogNoCard,
    PmLogDirError,
    PmLogOpenError,
    PmLogWriteError,
} PmLogStatus;

typedef struct PmLogger PmLogger;

PmLogger* pm_logger_alloc(Storage* storage);

/** Stops logging (flush + close) and frees the logger. */
void pm_logger_free(PmLogger* logger);

/** Create a new session file and write the CSV header. */
PmLogStatus pm_logger_start(PmLogger* logger, const PmClock* clock);

/**
 * Flush and close the current file. Safe to call when not active (returns
 * PmLogOk). Returns PmLogWriteError when the final sync or close failed;
 * logging is stopped either way.
 */
PmLogStatus pm_logger_stop(PmLogger* logger);

/**
 * Append one line. On a write or sync error the file is closed and logging
 * stops. Data is synced to the card every few lines to limit loss on power
 * cut; PmLogOk means the line was written and, if due, synced.
 */
PmLogStatus pm_logger_write(PmLogger* logger, const char* line);

bool pm_logger_is_active(const PmLogger* logger);
uint32_t pm_logger_rows(const PmLogger* logger);
/** File name (without directory) of the current/last session. */
const char* pm_logger_file_name(const PmLogger* logger);

const char* pm_log_status_text(PmLogStatus status);

/** Ensure PM_DATA_DIR exists. */
bool pm_logger_ensure_dir(Storage* storage);

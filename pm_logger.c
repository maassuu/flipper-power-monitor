#include "pm_logger.h"
#include "pm_csv.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#define TAG "PowerMonLog"

/* Sync to card every N rows. */
#define PM_LOG_SYNC_EVERY (10u)

struct PmLogger {
    Storage* storage;
    File* file;
    bool active;
    uint32_t rows;
    char name[32];
    char path[96];
};

PmLogger* pm_logger_alloc(Storage* storage) {
    PmLogger* logger = malloc(sizeof(PmLogger));
    memset(logger, 0, sizeof(*logger));
    logger->storage = storage;
    logger->file = storage_file_alloc(storage);
    return logger;
}

void pm_logger_free(PmLogger* logger) {
    pm_logger_stop(logger);
    storage_file_free(logger->file);
    free(logger);
}

bool pm_logger_ensure_dir(Storage* storage) {
    const FS_Error e1 = storage_common_mkdir(storage, EXT_PATH("apps_data"));
    const FS_Error e2 = storage_common_mkdir(storage, PM_DATA_DIR);
    return (e1 == FSE_OK || e1 == FSE_EXIST) && (e2 == FSE_OK || e2 == FSE_EXIST);
}

PmLogStatus pm_logger_start(PmLogger* logger, const PmClock* clock) {
    pm_logger_stop(logger);

    if(storage_sd_status(logger->storage) != FSE_OK) return PmLogNoCard;
    if(!pm_logger_ensure_dir(logger->storage)) return PmLogDirError;

    char base[32];
    if(pm_csv_filename(base, sizeof(base), clock) == 0) return PmLogOpenError;

    /* Avoid overwriting when started twice within the same second */
    snprintf(logger->name, sizeof(logger->name), "%s", base);
    snprintf(logger->path, sizeof(logger->path), "%s/%s", PM_DATA_DIR, logger->name);
    for(uint8_t i = 1; i < 10 && storage_common_exists(logger->storage, logger->path); i++) {
        /* base ends with ".csv": insert "_<i>" before the extension */
        snprintf(logger->name, sizeof(logger->name), "%.*s_%u.csv", 18, base, (unsigned)i);
        snprintf(logger->path, sizeof(logger->path), "%s/%s", PM_DATA_DIR, logger->name);
    }

    if(!storage_file_open(logger->file, logger->path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        FURI_LOG_E(TAG, "open %s: %s", logger->path, storage_file_get_error_desc(logger->file));
        storage_file_close(logger->file);
        return PmLogOpenError;
    }

    logger->active = true;
    logger->rows = 0;

    char header[PM_CSV_LINE_MAX];
    pm_csv_header(header, sizeof(header));
    const PmLogStatus st = pm_logger_write(logger, header);
    logger->rows = 0; /* header is not a data row */
    if(st == PmLogOk) FURI_LOG_I(TAG, "logging to %s", logger->path);
    return st;
}

void pm_logger_stop(PmLogger* logger) {
    if(!logger->active) return;
    storage_file_sync(logger->file);
    storage_file_close(logger->file);
    logger->active = false;
    FURI_LOG_I(TAG, "closed %s, %lu rows", logger->path, (unsigned long)logger->rows);
}

PmLogStatus pm_logger_write(PmLogger* logger, const char* line) {
    if(!logger->active) return PmLogWriteError;
    const size_t len = strlen(line);
    const size_t written = storage_file_write(logger->file, line, len);
    if(written != len) {
        FURI_LOG_E(TAG, "write: %s", storage_file_get_error_desc(logger->file));
        /* Card removed or full: close the file, keep what was written */
        storage_file_close(logger->file);
        logger->active = false;
        return PmLogWriteError;
    }
    logger->rows++;
    if((logger->rows % PM_LOG_SYNC_EVERY) == 0) storage_file_sync(logger->file);
    return PmLogOk;
}

bool pm_logger_is_active(const PmLogger* logger) {
    return logger->active;
}

uint32_t pm_logger_rows(const PmLogger* logger) {
    return logger->rows;
}

const char* pm_logger_file_name(const PmLogger* logger) {
    return logger->name;
}

const char* pm_log_status_text(PmLogStatus status) {
    switch(status) {
    case PmLogOk:
        return "OK";
    case PmLogNoCard:
        return "No SD card";
    case PmLogDirError:
        return "SD: mkdir failed";
    case PmLogOpenError:
        return "SD: open failed";
    case PmLogWriteError:
        return "SD write error";
    default:
        return "SD error";
    }
}

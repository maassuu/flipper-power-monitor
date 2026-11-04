#include "pm_settings_store.h"
#include "pm_logger.h" /* PM_DATA_DIR, pm_logger_ensure_dir */

#include <toolbox/saved_struct.h>

#define PM_SETTINGS_PATH    PM_DATA_DIR "/settings.bin"
#define PM_SETTINGS_MAGIC   (0x50u)
#define PM_SETTINGS_VERSION (1u)

void pm_settings_load(PmSettings* settings) {
    PmSettings loaded;
    if(saved_struct_load(
           PM_SETTINGS_PATH, &loaded, sizeof(loaded), PM_SETTINGS_MAGIC, PM_SETTINGS_VERSION)) {
        *settings = loaded;
        pm_settings_sanitize(settings);
    } else {
        pm_settings_defaults(settings);
    }
}

bool pm_settings_save(Storage* storage, const PmSettings* settings) {
    if(storage_sd_status(storage) != FSE_OK) return false;
    if(!pm_logger_ensure_dir(storage)) return false;
    return saved_struct_save(
        PM_SETTINGS_PATH, settings, sizeof(*settings), PM_SETTINGS_MAGIC, PM_SETTINGS_VERSION);
}

/* Host fake of the Flipper storage API used by pm_logger.c.
 * Behaviour is driven by pm_fake_storage (see test_logger.c). */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EXT_PATH(path) "/ext/" path

typedef enum {
    FSE_OK,
    FSE_NOT_READY,
    FSE_EXIST,
    FSE_INTERNAL,
} FS_Error;

typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_CREATE_NEW = 4,
} FS_OpenMode;

typedef struct Storage Storage;
typedef struct File File;

typedef struct {
    FS_Error sd_status;
    bool open_fails;
    /* Number of successful writes left; then writes come back short.
     * Negative means never fail. */
    int writes_left;
    bool sync_fails;
    bool close_fails;
    /* Observations */
    bool is_open;
    unsigned writes;
    unsigned syncs;
    unsigned closes;
} PmFakeStorage;

extern PmFakeStorage pm_fake_storage;
void pm_fake_storage_reset(void);

File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode);
bool storage_file_close(File* file);
size_t storage_file_write(File* file, const void* buff, size_t bytes_to_write);
bool storage_file_sync(File* file);
const char* storage_file_get_error_desc(File* file);
FS_Error storage_common_mkdir(Storage* storage, const char* path);
bool storage_common_exists(Storage* storage, const char* path);
FS_Error storage_sd_status(Storage* storage);

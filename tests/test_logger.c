#include "test_framework.h"
#include "../pm_logger.h"

/* ---------------- fake storage ---------------- */

struct File {
    int unused;
};

static File pm_fake_file;
PmFakeStorage pm_fake_storage;

void pm_fake_storage_reset(void) {
    memset(&pm_fake_storage, 0, sizeof(pm_fake_storage));
    pm_fake_storage.sd_status = FSE_OK;
    pm_fake_storage.writes_left = -1;
}

File* storage_file_alloc(Storage* storage) {
    (void)storage;
    return &pm_fake_file;
}

void storage_file_free(File* file) {
    (void)file;
}

bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode) {
    (void)file;
    (void)path;
    (void)access;
    (void)mode;
    if(pm_fake_storage.open_fails) return false;
    pm_fake_storage.is_open = true;
    return true;
}

bool storage_file_close(File* file) {
    (void)file;
    pm_fake_storage.closes++;
    pm_fake_storage.is_open = false;
    return !pm_fake_storage.close_fails;
}

size_t storage_file_write(File* file, const void* buff, size_t bytes_to_write) {
    (void)file;
    (void)buff;
    pm_fake_storage.writes++;
    if(pm_fake_storage.writes_left == 0) return bytes_to_write / 2u;
    if(pm_fake_storage.writes_left > 0) pm_fake_storage.writes_left--;
    return bytes_to_write;
}

bool storage_file_sync(File* file) {
    (void)file;
    pm_fake_storage.syncs++;
    return !pm_fake_storage.sync_fails;
}

const char* storage_file_get_error_desc(File* file) {
    (void)file;
    return "fake error";
}

FS_Error storage_common_mkdir(Storage* storage, const char* path) {
    (void)storage;
    (void)path;
    return FSE_EXIST;
}

bool storage_common_exists(Storage* storage, const char* path) {
    (void)storage;
    (void)path;
    return false;
}

FS_Error storage_sd_status(Storage* storage) {
    (void)storage;
    return pm_fake_storage.sd_status;
}

/* ---------------- tests ---------------- */

static const PmClock pm_test_clock = {2026, 11, 4, 18, 5, 9};

/* Fresh fake storage and a started logger (header already written). */
static PmLogger* pm_test_started_logger(void) {
    pm_fake_storage_reset();
    PmLogger* logger = pm_logger_alloc(NULL);
    CHECK(pm_logger_start(logger, &pm_test_clock) == PmLogOk);
    CHECK(pm_logger_is_active(logger));
    CHECK(pm_logger_rows(logger) == 0u);
    CHECK_STR(pm_logger_file_name(logger), "pm_20261104_180509.csv");
    return logger;
}

static void test_start_errors(void) {
    pm_fake_storage_reset();
    PmLogger* logger = pm_logger_alloc(NULL);
    pm_fake_storage.sd_status = FSE_NOT_READY;
    CHECK(pm_logger_start(logger, &pm_test_clock) == PmLogNoCard);
    CHECK(!pm_logger_is_active(logger));

    pm_fake_storage_reset();
    pm_fake_storage.open_fails = true;
    CHECK(pm_logger_start(logger, &pm_test_clock) == PmLogOpenError);
    CHECK(!pm_logger_is_active(logger));
    pm_logger_free(logger);
}

static void test_periodic_sync_ok(void) {
    PmLogger* logger = pm_test_started_logger();
    const unsigned syncs0 = pm_fake_storage.syncs;
    for(unsigned i = 0; i < 9u; i++) CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    CHECK(pm_fake_storage.syncs == syncs0); /* not due yet */
    CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    CHECK(pm_fake_storage.syncs == syncs0 + 1u);
    CHECK(pm_logger_rows(logger) == 10u);
    CHECK(pm_logger_is_active(logger));
    pm_logger_free(logger);
}

static void test_periodic_sync_failure_ends_session(void) {
    PmLogger* logger = pm_test_started_logger();
    pm_fake_storage.sync_fails = true;
    for(unsigned i = 0; i < 9u; i++) CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    const unsigned closes0 = pm_fake_storage.closes;
    CHECK(pm_logger_write(logger, "row\n") == PmLogWriteError);
    CHECK(!pm_logger_is_active(logger));
    CHECK(!pm_fake_storage.is_open);
    CHECK(pm_fake_storage.closes == closes0 + 1u);
    /* session is over: further writes are refused, stop is a no-op */
    CHECK(pm_logger_write(logger, "row\n") == PmLogWriteError);
    CHECK(pm_logger_stop(logger) == PmLogOk);
    CHECK(pm_fake_storage.closes == closes0 + 1u);
    pm_logger_free(logger);
}

static void test_short_write_ends_session(void) {
    PmLogger* logger = pm_test_started_logger();
    pm_fake_storage.writes_left = 2;
    CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    CHECK(pm_logger_write(logger, "row\n") == PmLogWriteError);
    CHECK(!pm_logger_is_active(logger));
    CHECK(!pm_fake_storage.is_open);
    CHECK(pm_logger_rows(logger) == 2u);
    pm_logger_free(logger);
}

static void test_stop_inactive_is_ok(void) {
    pm_fake_storage_reset();
    PmLogger* logger = pm_logger_alloc(NULL);
    CHECK(pm_logger_stop(logger) == PmLogOk);
    CHECK(pm_fake_storage.syncs == 0u);
    CHECK(pm_fake_storage.closes == 0u);
    pm_logger_free(logger);
}

static void test_stop_ok(void) {
    PmLogger* logger = pm_test_started_logger();
    CHECK(pm_logger_write(logger, "row\n") == PmLogOk);
    const unsigned syncs0 = pm_fake_storage.syncs;
    CHECK(pm_logger_stop(logger) == PmLogOk);
    CHECK(pm_fake_storage.syncs == syncs0 + 1u);
    CHECK(!pm_fake_storage.is_open);
    CHECK(!pm_logger_is_active(logger));
    CHECK(pm_logger_rows(logger) == 1u); /* still readable for the toast */
    pm_logger_free(logger);
}

static void test_stop_sync_failure(void) {
    PmLogger* logger = pm_test_started_logger();
    pm_fake_storage.sync_fails = true;
    const unsigned closes0 = pm_fake_storage.closes;
    CHECK(pm_logger_stop(logger) == PmLogWriteError);
    CHECK(!pm_logger_is_active(logger));
    CHECK(!pm_fake_storage.is_open); /* closed anyway */
    CHECK(pm_fake_storage.closes == closes0 + 1u);
    pm_logger_free(logger);
}

static void test_stop_close_failure(void) {
    PmLogger* logger = pm_test_started_logger();
    pm_fake_storage.close_fails = true;
    CHECK(pm_logger_stop(logger) == PmLogWriteError);
    CHECK(!pm_logger_is_active(logger));
    pm_logger_free(logger);
}

void test_logger_suite(void) {
    printf("[logger]\n");
    RUN(test_start_errors);
    RUN(test_periodic_sync_ok);
    RUN(test_periodic_sync_failure_ends_session);
    RUN(test_short_write_ends_session);
    RUN(test_stop_inactive_is_ok);
    RUN(test_stop_ok);
    RUN(test_stop_sync_failure);
    RUN(test_stop_close_failure);
}

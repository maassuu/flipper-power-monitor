/**
 * @file power_monitor.c
 * Flipper Power Monitor - application entry point, main loop and input.
 *
 * Threads:
 *  - main thread: sampling (I2C via furi_hal), CSV writes, input handling
 *  - GUI thread:  draw callback (pm_ui_draw)
 * Shared state is protected by app->mutex.
 *
 * Buttons:
 *  Left/Right  previous/next view
 *  Up/Down     graph: switch V/I/P, lists: scroll, settings: select
 *  OK          dashboard: CSV on/off, graph: pause, stats/energy: reset,
 *              charging: clear events, health: refresh, settings: change
 *  Back        go to dashboard, from dashboard: exit; long Back: exit
 */
#include "power_monitor_i.h"

#include "pm_csv.h"
#include "pm_hw.h"
#include "pm_settings_store.h"
#include "pm_ui.h"

#include <furi_hal_rtc.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#define TAG "PowerMon"

#define PM_SLOW_PERIOD_MS (1000u)
#define PM_QUEUE_SIZE     (8u)

static uint32_t pm_now_ms(void) {
    const uint32_t freq = furi_kernel_get_tick_frequency();
    const uint32_t tick = furi_get_tick();
    if(freq == 1000u) return tick;
    return (uint32_t)((uint64_t)tick * 1000u / freq);
}

static void pm_get_clock(PmClock* clock) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    clock->year = dt.year;
    clock->month = dt.month;
    clock->day = dt.day;
    clock->hour = dt.hour;
    clock->minute = dt.minute;
    clock->second = dt.second;
}

/* Must be called with the mutex held. */
static void pm_toast(PowerMonitorApp* app, const char* text) {
    snprintf(app->toast, sizeof(app->toast), "%s", text);
    app->toast_until_ms = pm_now_ms() + PM_TOAST_MS;
}

static void pm_draw_callback(Canvas* canvas, void* context) {
    PowerMonitorApp* app = context;
    /* Always wait: returning without drawing would show an empty frame
     * (flicker). The main thread only holds the mutex briefly. */
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    pm_ui_draw(canvas, app, pm_now_ms());
    furi_mutex_release(app->mutex);
}

static void pm_input_callback(InputEvent* event, void* context) {
    PowerMonitorApp* app = context;
    /* Never block the input service: drop the event if the queue is full */
    furi_message_queue_put(app->input_queue, event, 0);
}

/* ---------------- backlight ---------------- */

static bool pm_backlight_forced = false;

static void pm_apply_backlight(PowerMonitorApp* app, bool on) {
    /* enforce_on / enforce_auto are reference counted by the notification
     * service: keep the calls balanced. */
    if(on && !pm_backlight_forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        pm_backlight_forced = true;
    } else if(!on && pm_backlight_forced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        pm_backlight_forced = false;
    }
}

/* ---------------- CSV logging (main thread) ---------------- */

/* Mutex held by caller. */
static void pm_logging_toggle(PowerMonitorApp* app, uint32_t* last_csv_ms) {
    char msg[40];
    if(pm_logger_is_active(app->logger)) {
        const PmLogStatus stop_st = pm_logger_stop(app->logger);
        app->log_active = false;
        if(stop_st == PmLogOk) {
            snprintf(
                msg, sizeof(msg), "Saved %lu rows", (unsigned long)pm_logger_rows(app->logger));
        } else {
            snprintf(msg, sizeof(msg), "Save failed: %s", pm_log_status_text(stop_st));
        }
        pm_toast(app, msg);
        return;
    }

    PmClock clock;
    pm_get_clock(&clock);
    const PmLogStatus st = pm_logger_start(app->logger, &clock);
    if(st == PmLogOk) {
        app->log_active = true;
        app->log_rows = 0;
        snprintf(app->log_name, sizeof(app->log_name), "%s", pm_logger_file_name(app->logger));
        *last_csv_ms = pm_now_ms() - pm_settings_csv_ms(&app->settings); /* write first row now */
        pm_toast(app, app->log_name);
    } else {
        app->log_active = false;
        pm_toast(app, pm_log_status_text(st));
    }
}

/* ---------------- sampling ---------------- */

typedef struct {
    uint32_t last_slow_ms;
    uint32_t last_csv_ms;
    bool force_slow;
    bool first;
} PmLoopState;

static void pm_sample(PowerMonitorApp* app, PmLoopState* ls) {
    /* settings are only modified by this (main) thread: no lock needed to read */
    const bool ext = app->settings.ext_reads != 0;
    const uint32_t now = pm_now_ms();

    PmSample s;
    pm_hw_read_fast(&s, now, ext);

    PmBatteryInfo info;
    const bool slow = ls->first || ls->force_slow || (now - ls->last_slow_ms) >= PM_SLOW_PERIOD_MS;
    if(slow) {
        pm_hw_read_slow(&info, ext);
        ls->last_slow_ms = now;
        ls->force_slow = false;
    }
    ls->first = false;

    char line[PM_CSV_LINE_MAX];
    bool write_line = false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(slow) {
        app->battery = info;
        app->have_battery = true;
    }
    app->sample = s;
    app->have_sample = true;
    app->phase = pm_charge_phase_from(s.charging, s.charge_done, s.reg0b_valid, s.chg_reg0b);

    if(s.valid) {
        const float vals[PmChannelCount] = {s.bat_v, s.bat_i_ma, s.bat_p_w};
        pm_stats_add(&app->stats, now, vals);
        pm_energy_add(&app->energy, now, s.bat_p_w, s.bat_i_ma);
        if(!app->graph_paused) pm_history_push(&app->history, now, vals);
    } else {
        pm_stats_break(&app->stats);
        pm_energy_break(&app->energy);
    }
    pm_charge_update(&app->charge, now, s.usb_present, s.otg_enabled, app->phase, s.bat_i_ma);

    if(app->log_active && s.valid &&
       (now - ls->last_csv_ms) >= pm_settings_csv_ms(&app->settings)) {
        PmClock clock;
        pm_get_clock(&clock);
        write_line = pm_csv_row(line, sizeof(line), &clock, &s, app->phase, &app->energy) > 0;
        ls->last_csv_ms = now;
    }
    furi_mutex_release(app->mutex);

    /* SD I/O outside the lock so the GUI never waits for the card */
    if(write_line) {
        const PmLogStatus st = pm_logger_write(app->logger, line);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(st == PmLogOk) {
            app->log_rows = pm_logger_rows(app->logger);
        } else {
            app->log_active = false;
            pm_toast(app, pm_log_status_text(st));
        }
        furi_mutex_release(app->mutex);
    }
}

/* ---------------- input ---------------- */

static void pm_scroll(PowerMonitorApp* app, bool down) {
    uint8_t* sc = &app->scroll[app->view];
    if(down) {
        if(*sc < UINT8_MAX) (*sc)++; /* upper bound clamped when drawing */
    } else if(*sc > 0) {
        (*sc)--;
    }
}

static void pm_settings_ok(PowerMonitorApp* app, PmLoopState* ls) {
    const PmSettingItem item = (PmSettingItem)app->settings_cursor;
    if(item == PmSettingCsvLog) {
        pm_logging_toggle(app, &ls->last_csv_ms);
        return;
    }
    pm_settings_cycle(&app->settings, item);
    if(item == PmSettingBacklight) pm_apply_backlight(app, app->settings.backlight != 0);
    if(item == PmSettingExtReads) ls->force_slow = true;
    if(!pm_settings_save(app->storage, &app->settings)) pm_toast(app, "Not saved (no SD)");
}

/** @return false when the application should exit. */
static bool pm_handle_input(PowerMonitorApp* app, const InputEvent* ev, PmLoopState* ls) {
    if(ev->key == InputKeyBack && ev->type == InputTypeLong) return false;

    const bool is_short = ev->type == InputTypeShort;
    const bool is_repeat = ev->type == InputTypeRepeat;
    if(!is_short && !is_repeat) return true;

    bool running = true;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->splash) {
        /* First key press only closes the splash screen */
        const bool visible = (int32_t)(app->splash_until_ms - pm_now_ms()) > 0;
        app->splash = false;
        if(visible) {
            furi_mutex_release(app->mutex);
            return true;
        }
    }
    switch(ev->key) {
    case InputKeyBack:
        if(!is_short) break;
        if(app->view != PmViewDashboard) {
            app->view = PmViewDashboard;
        } else {
            running = false;
        }
        break;

    case InputKeyLeft:
    case InputKeyRight:
        if(!is_short) break;
        app->view = (PmView)(
            (app->view + (ev->key == InputKeyRight ? 1u : PmViewCount - 1u)) % PmViewCount);
        break;

    case InputKeyUp:
    case InputKeyDown: {
        const bool down = ev->key == InputKeyDown;
        switch(app->view) {
        case PmViewGraphs:
            if(is_short) {
                app->graph_channel = (PmChannel)(
                    (app->graph_channel + (down ? 1u : PmChannelCount - 1u)) % PmChannelCount);
            }
            break;
        case PmViewSettings:
            if(down && app->settings_cursor + 1u < PmSettingCount) app->settings_cursor++;
            if(!down && app->settings_cursor > 0) app->settings_cursor--;
            break;
        case PmViewEnergy:
        case PmViewCharging:
        case PmViewHealth:
            pm_scroll(app, down);
            break;
        default:
            break;
        }
        break;
    }

    case InputKeyOk:
        if(!is_short) break;
        switch(app->view) {
        case PmViewDashboard:
            pm_logging_toggle(app, &ls->last_csv_ms);
            break;
        case PmViewGraphs:
            app->graph_paused = !app->graph_paused;
            break;
        case PmViewStats:
            pm_stats_reset(&app->stats, pm_now_ms());
            pm_toast(app, "Statistics reset");
            break;
        case PmViewEnergy:
            pm_energy_reset(&app->energy);
            pm_toast(app, "Energy counters reset");
            break;
        case PmViewCharging:
            pm_charge_clear_events(&app->charge);
            pm_toast(app, "Events cleared");
            break;
        case PmViewHealth:
            ls->force_slow = true;
            pm_toast(app, "Refreshing...");
            break;
        case PmViewSettings:
            pm_settings_ok(app, ls);
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
    furi_mutex_release(app->mutex);
    return running;
}

/* ---------------- lifecycle ---------------- */

static PowerMonitorApp* pm_app_alloc(void) {
    PowerMonitorApp* app = malloc(sizeof(PowerMonitorApp));
    memset(app, 0, sizeof(*app));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(PM_QUEUE_SIZE, sizeof(InputEvent));

    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->logger = pm_logger_alloc(app->storage);

    pm_settings_load(&app->settings);
    const uint32_t now = pm_now_ms();
    pm_stats_reset(&app->stats, now);
    pm_energy_reset(&app->energy);
    pm_history_reset(&app->history);
    pm_charge_reset(&app->charge);
    app->view = PmViewDashboard;
    app->graph_channel = PmChannelVoltage;
    app->splash = true;
    app->splash_until_ms = now + PM_SPLASH_MS;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, pm_draw_callback, app);
    view_port_input_callback_set(app->view_port, pm_input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void pm_app_free(PowerMonitorApp* app) {
    /* Detach UI first so the draw callback can no longer run */
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    /* Close the CSV file cleanly and persist settings */
    pm_logger_free(app->logger);
    pm_settings_save(app->storage, &app->settings);
    pm_apply_backlight(app, false);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t power_monitor_app(void* p) {
    UNUSED(p);
    if(furi_kernel_get_tick_frequency() != 1000u) {
        FURI_LOG_W(TAG, "tick %lu Hz", (unsigned long)furi_kernel_get_tick_frequency());
    }

    PowerMonitorApp* app = pm_app_alloc();
    pm_backlight_forced = false;
    pm_apply_backlight(app, app->settings.backlight != 0);

    PmLoopState ls = {.last_slow_ms = 0, .last_csv_ms = 0, .force_slow = false, .first = true};
    uint32_t next_sample_ms = pm_now_ms();
    bool running = true;

    while(running) {
        const uint32_t now = pm_now_ms();
        int32_t wait_ms = (int32_t)(next_sample_ms - now);
        if(wait_ms < 0) wait_ms = 0;

        InputEvent event;
        if(furi_message_queue_get(app->input_queue, &event, furi_ms_to_ticks((uint32_t)wait_ms)) ==
           FuriStatusOk) {
            running = pm_handle_input(app, &event, &ls);
            view_port_update(app->view_port);
            if(!running) break;
        }

        const uint32_t t = pm_now_ms();
        if((int32_t)(t - next_sample_ms) >= 0) {
            pm_sample(app, &ls);
            const uint32_t interval = pm_settings_sample_ms(&app->settings);
            next_sample_ms += interval;
            /* Fell behind (e.g. slow SD card): resynchronise instead of bursting */
            if((int32_t)(t - next_sample_ms) >= 0) next_sample_ms = t + interval;
            view_port_update(app->view_port);
        }
    }

    pm_app_free(app);
    return 0;
}

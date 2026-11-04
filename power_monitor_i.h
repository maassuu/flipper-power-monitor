/**
 * @file power_monitor_i.h
 * Application state shared between the main loop and the draw callback.
 * All fields below `mutex` are protected by it.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <storage/storage.h>

#include "pm_charge.h"
#include "pm_energy.h"
#include "pm_history.h"
#include "pm_logger.h"
#include "pm_settings.h"
#include "pm_stats.h"
#include "pm_types.h"

typedef enum {
    PmViewDashboard = 0,
    PmViewGraphs,
    PmViewStats,
    PmViewEnergy,
    PmViewCharging,
    PmViewHealth,
    PmViewSettings,
    PmViewCount,
} PmView;

#define PM_TOAST_MS  (2000u)
#define PM_SPLASH_MS (2500u)

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    NotificationApp* notifications;
    Storage* storage;
    PmLogger* logger; /* main thread only */

    FuriMutex* mutex;

    /* --- protected by mutex --- */
    PmSettings settings;
    bool have_sample;
    PmSample sample;
    PmBatteryInfo battery;
    bool have_battery;
    PmChargePhase phase;

    PmStats stats;
    PmEnergy energy;
    PmHistory history;
    PmCharge charge;

    PmView view;
    PmChannel graph_channel;
    bool graph_paused;
    uint8_t scroll[PmViewCount];
    uint8_t settings_cursor;

    bool log_active;
    uint32_t log_rows;
    char log_name[32];

    char toast[32];
    uint32_t toast_until_ms;

    bool splash; /**< Splash screen shown until splash_until_ms or a key press */
    uint32_t splash_until_ms;
} PowerMonitorApp;

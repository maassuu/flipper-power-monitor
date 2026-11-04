#include "pm_ui.h"

#include "pm_format.h"
#include "pm_health.h"
#include "pm_pope.h"

#include <gui/elements.h>
#include <stdio.h>

#define PM_UI_ROW_Y0    (21)
#define PM_UI_ROW_PITCH (10)
#define PM_UI_MAX_LINES (20u)

typedef struct {
    char left[24];
    char right[24];
} PmLine;

/* Only used from the draw callback, which runs in the GUI thread with the
 * app mutex held: a static buffer keeps the GUI thread stack small. */
static PmLine pm_lines[PM_UI_MAX_LINES];

static const char* const pm_view_titles[PmViewCount] = {
    "Papa Monitor",
    "Graphs",
    "Statistics",
    "Battery energy",
    "Charging",
    "Battery health",
    "Settings",
};

static int32_t pm_row_y(uint8_t row) {
    return PM_UI_ROW_Y0 + (int32_t)row * PM_UI_ROW_PITCH;
}

static void pm_draw_header(Canvas* canvas, const PowerMonitorApp* app, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 1, 9, title);

    canvas_set_font(canvas, FontSecondary);
    char page[8];
    snprintf(page, sizeof(page), "%u/%u", (unsigned)app->view + 1u, (unsigned)PmViewCount);
    canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, page);

    if(app->log_active) {
        const int32_t x = 127 - (int32_t)canvas_string_width(canvas, page) - 21;
        canvas_draw_box(canvas, x, 1, 18, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x + 1, 9, "REC");
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

static void pm_draw_row(Canvas* canvas, uint8_t row, const char* left, const char* right, int32_t right_x) {
    const int32_t y = pm_row_y(row);
    canvas_draw_str(canvas, 1, y, left);
    if(right && right[0]) canvas_draw_str_aligned(canvas, right_x, y, AlignRight, AlignBottom, right);
}

static void pm_draw_toast(Canvas* canvas, const PowerMonitorApp* app, uint32_t now_ms) {
    if(app->toast[0] == '\0' || (int32_t)(app->toast_until_ms - now_ms) <= 0) return;
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 52, 128, 12);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 0, 52, 128, 12);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, app->toast);
}

/* Draws a scrollable list. cursor < 0 means no selection. */
static void pm_draw_list(Canvas* canvas, PowerMonitorApp* app, uint8_t count, int32_t cursor) {
    uint8_t* scroll = &app->scroll[app->view];
    const uint8_t max_scroll = (count > PM_UI_LIST_ROWS) ? (uint8_t)(count - PM_UI_LIST_ROWS) : 0;
    if(cursor >= 0) {
        /* keep the cursor visible */
        if((uint8_t)cursor < *scroll) *scroll = (uint8_t)cursor;
        if((uint8_t)cursor >= *scroll + PM_UI_LIST_ROWS) *scroll = (uint8_t)(cursor - PM_UI_LIST_ROWS + 1);
    }
    if(*scroll > max_scroll) *scroll = max_scroll;

    const bool scrollbar = count > PM_UI_LIST_ROWS;
    const int32_t right_x = scrollbar ? 122 : 127;

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t row = 0; row < PM_UI_LIST_ROWS; row++) {
        const uint8_t idx = (uint8_t)(*scroll + row);
        if(idx >= count) break;
        const bool selected = (cursor >= 0) && (idx == (uint8_t)cursor);
        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, pm_row_y(row) - 8, (size_t)(right_x + 2), 10);
            canvas_set_color(canvas, ColorWhite);
        }
        pm_draw_row(canvas, row, pm_lines[idx].left, pm_lines[idx].right, right_x);
        canvas_set_color(canvas, ColorBlack);
    }
    if(scrollbar) elements_scrollbar_pos(canvas, 127, 13, 51, *scroll, (size_t)max_scroll + 1u);
}

static uint8_t pm_line(uint8_t n, const char* left, const char* right) {
    if(n >= PM_UI_MAX_LINES) return n;
    snprintf(pm_lines[n].left, sizeof(pm_lines[n].left), "%s", left);
    snprintf(pm_lines[n].right, sizeof(pm_lines[n].right), "%s", right ? right : "");
    return (uint8_t)(n + 1u);
}

/* ------------------------------------------------------------------ */

static void pm_draw_dashboard(Canvas* canvas, PowerMonitorApp* app) {
    canvas_set_font(canvas, FontSecondary);
    if(!app->have_sample) {
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, "Reading...");
        return;
    }
    const PmSample* s = &app->sample;
    char v[24];
    char x[16];

    /* USB: VBUS from charger ADC. Not shown while Flipper itself drives VBUS. */
    if(s->otg_enabled) {
        snprintf(v, sizeof(v), "OTG out");
    } else if(s->usb_present) {
        snprintf(v, sizeof(v), "%.1f V", (double)s->usb_v);
    } else {
        snprintf(v, sizeof(v), "-- V");
    }
    pm_draw_row(canvas, 0, "USB", v, 86);
    canvas_draw_str_aligned(
        canvas, 127, pm_row_y(0), AlignRight, AlignBottom, pm_charge_phase_short(app->phase));

    if(s->valid) {
        snprintf(v, sizeof(v), "%.3f V", (double)s->bat_v);
    } else {
        snprintf(v, sizeof(v), "gauge err");
    }
    snprintf(x, sizeof(x), "%.1fC", (double)s->temp_c);
    pm_draw_row(canvas, 1, "BAT", v, 86);
    canvas_draw_str_aligned(canvas, 127, pm_row_y(1), AlignRight, AlignBottom, x);

    pm_fmt_current(v, sizeof(v), s->bat_i_ma);
    pm_draw_row(canvas, 2, "CURRENT", v, 86);

    pm_fmt_power(v, sizeof(v), s->bat_p_w);
    pm_draw_row(canvas, 3, "POWER", v, 86);
    canvas_draw_str_aligned(canvas, 127, pm_row_y(3), AlignRight, AlignBottom, "bat");

    snprintf(v, sizeof(v), "%u%%", (unsigned)s->soc_pct);
    pm_draw_row(canvas, 4, "BATTERY", v, 86);
    if(app->have_battery) {
        snprintf(x, sizeof(x), "%umAh", (unsigned)app->battery.remaining_mah);
        canvas_draw_str_aligned(canvas, 127, pm_row_y(4), AlignRight, AlignBottom, x);
    }
}

/* ------------------------------------------------------------------ */

typedef struct {
    const char* title;
    const char* unit;
    const char* fmt;
    float min_span;
} PmGraphDef;

static const PmGraphDef pm_graphs[PmChannelCount] = {
    {"Bat voltage", "V", "%.3f", 0.02f},
    {"Bat current", "mA", "%.0f", 20.0f},
    {"Bat power", "W", "%.2f", 0.05f},
};

static void pm_draw_graph(Canvas* canvas, PowerMonitorApp* app, uint32_t now_ms) {
    const PmChannel ch = app->graph_channel;
    const PmGraphDef* def = &pm_graphs[ch];

    canvas_set_font(canvas, FontSecondary);

    const int32_t x0 = 30; /* plot area */
    const int32_t x1 = 127;
    const int32_t y0 = 14;
    const int32_t y1 = 62;

    /* Freeze the time axis while paused */
    uint32_t ref_ms = now_ms;
    const uint16_t count = pm_history_count(&app->history);
    if(app->graph_paused && count > 0) ref_ms = pm_history_get(&app->history, count - 1u)->tick_ms;

    float mn, mx;
    if(!pm_history_range(&app->history, ch, ref_ms, PM_HISTORY_WINDOW_MS, &mn, &mx)) {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "No data yet");
        return;
    }
    float lo, hi;
    pm_autoscale(mn, mx, def->min_span, &lo, &hi);

    /* Axis labels */
    char lbl[16];
    snprintf(lbl, sizeof(lbl), def->fmt, (double)hi);
    canvas_draw_str(canvas, 0, y0 + 7, lbl);
    snprintf(lbl, sizeof(lbl), def->fmt, (double)lo);
    canvas_draw_str(canvas, 0, y1 + 1, lbl);
    canvas_draw_str(canvas, 0, 41, def->unit);

    /* Frame + zero line */
    canvas_draw_line(canvas, x0 - 1, y0, x0 - 1, y1);
    canvas_draw_line(canvas, x0 - 1, y1, x1, y1);
    if(lo < 0.0f && hi > 0.0f) {
        const int32_t zy = pm_scale_y(0.0f, lo, hi, y0, y1);
        for(int32_t x = x0; x <= x1; x += 3) canvas_draw_dot(canvas, x, zy);
    }

    /* Connect points closer than this; larger gaps are left open */
    const uint32_t interval = pm_settings_sample_ms(&app->settings);
    const uint32_t max_gap = (interval * 3u > 1500u) ? interval * 3u : 1500u;
    const int32_t w = x1 - x0;

    bool have_prev = false;
    int32_t px = 0, py = 0;
    uint32_t pt = 0;
    for(uint16_t i = 0; i < count; i++) {
        const PmHistoryPoint* p = pm_history_get(&app->history, i);
        const uint32_t age = pm_history_age_ms(ref_ms, p->tick_ms);
        if(age > PM_HISTORY_WINDOW_MS) continue;
        const int32_t x = x1 - (int32_t)((uint64_t)age * (uint64_t)w / PM_HISTORY_WINDOW_MS);
        const int32_t y = pm_scale_y(p->value[ch], lo, hi, y0, y1);
        if(have_prev && (p->tick_ms - pt) <= max_gap) {
            canvas_draw_line(canvas, px, py, x, y);
        } else {
            canvas_draw_dot(canvas, x, y);
        }
        have_prev = true;
        px = x;
        py = y;
        pt = p->tick_ms;
    }

    if(app->graph_paused) {
        canvas_draw_str_aligned(canvas, x1, y0 + 8, AlignRight, AlignBottom, "PAUSED");
    }
}

static void pm_draw_graphs(Canvas* canvas, PowerMonitorApp* app, uint32_t now_ms) {
    const PmGraphDef* def = &pm_graphs[app->graph_channel];
    char title[32];
    if(app->have_sample) {
        const float vals[PmChannelCount] = {app->sample.bat_v, app->sample.bat_i_ma, app->sample.bat_p_w};
        char val[16];
        snprintf(val, sizeof(val), def->fmt, (double)vals[app->graph_channel]);
        snprintf(title, sizeof(title), "%s %s%s", def->title, val, def->unit);
    } else {
        snprintf(title, sizeof(title), "%s", def->title);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 9, title);
    if(app->log_active) canvas_draw_box(canvas, 124, 2, 4, 4); /* REC marker */
    canvas_draw_line(canvas, 0, 11, 127, 11);
    pm_draw_graph(canvas, app, now_ms);
}

/* ------------------------------------------------------------------ */

static void pm_stat_cell(char* buf, size_t size, PmChannel ch, float v) {
    switch(ch) {
    case PmChannelVoltage:
        snprintf(buf, size, "%.3f", (double)v);
        break;
    case PmChannelCurrent:
        snprintf(buf, size, "%.0f", (double)v);
        break;
    default:
        snprintf(buf, size, "%.2f", (double)v);
        break;
    }
}

static void pm_draw_stats(Canvas* canvas, PowerMonitorApp* app, uint32_t now_ms) {
    const PmStats* st = &app->stats;
    canvas_set_font(canvas, FontSecondary);
    if(st->samples == 0) {
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, "No data yet");
        return;
    }
    static const int32_t col_x[3] = {58, 93, 127};
    static const char* const heads[3] = {"MIN", "AVG", "MAX"};
    static const char* const labels[PmChannelCount] = {"V", "mA", "W"};

    for(uint8_t c = 0; c < 3; c++) {
        canvas_draw_str_aligned(canvas, col_x[c], pm_row_y(0), AlignRight, AlignBottom, heads[c]);
    }
    char cell[16];
    for(uint8_t ch = 0; ch < PmChannelCount; ch++) {
        const int32_t y = pm_row_y((uint8_t)(ch + 1u));
        canvas_draw_str(canvas, 1, y, labels[ch]);
        const float vals[3] = {
            st->ch[ch].min, pm_stats_avg(st, (PmChannel)ch), st->ch[ch].max};
        for(uint8_t c = 0; c < 3; c++) {
            pm_stat_cell(cell, sizeof(cell), (PmChannel)ch, vals[c]);
            canvas_draw_str_aligned(canvas, col_x[c], y, AlignRight, AlignBottom, cell);
        }
    }
    char dur[16];
    pm_fmt_duration(dur, sizeof(dur), pm_stats_duration_ms(st, now_ms));
    char line[32];
    snprintf(line, sizeof(line), "t %s", dur);
    pm_draw_row(canvas, 4, line, "OK=reset", 127);
}

/* ------------------------------------------------------------------ */

static uint8_t pm_build_energy(PowerMonitorApp* app) {
    const PmEnergy* e = &app->energy;
    char v[24];
    uint8_t n = 0;

    pm_fmt_energy(v, sizeof(v), e->charge_mwh);
    n = pm_line(n, "Into battery", v);
    pm_fmt_energy(v, sizeof(v), e->discharge_mwh);
    n = pm_line(n, "Out of battery", v);
    pm_fmt_energy(v, sizeof(v), pm_energy_net_mwh(e));
    n = pm_line(n, "Net", v);
    snprintf(v, sizeof(v), "+%.1f/-%.1fmAh", e->charge_mah, e->discharge_mah);
    n = pm_line(n, "Charge", v);
    pm_fmt_duration(v, sizeof(v), e->integrated_ms);
    n = pm_line(n, "Measured", v);
    n = pm_line(n, "USB input", "not measurable");
    n = pm_line(n, " (BQ25896: no IBUS ADC)", NULL);
    snprintf(v, sizeof(v), "%lu", (unsigned long)e->gaps);
    n = pm_line(n, "Data gaps", v);
    n = pm_line(n, "OK = reset counters", NULL);
    return n;
}

static uint8_t pm_build_charging(PowerMonitorApp* app, uint32_t now_ms) {
    const PmSample* s = &app->sample;
    const PmBatteryInfo* b = &app->battery;
    const bool ext = app->have_battery && b->ext_valid;
    char v[24];
    uint8_t n = 0;

    n = pm_line(n, "State", pm_charge_phase_name(app->phase));
    if(s->otg_enabled) {
        n = pm_line(n, "USB", "OTG output on");
    } else if(s->usb_present) {
        snprintf(v, sizeof(v), "%.1f V", (double)s->usb_v);
        n = pm_line(n, "USB", v);
    } else {
        n = pm_line(n, "USB", "not connected");
    }
    if(s->reg0b_valid) {
        n = pm_line(n, "Input type", pm_vbus_type_name(PM_REG0B_VBUS_STAT(s->chg_reg0b)));
        n = pm_line(n, "Power good", PM_REG0B_PG_STAT(s->chg_reg0b) ? "yes" : "no");
    }
    if(ext) {
        snprintf(v, sizeof(v), "%umA", (unsigned)pm_charger_input_limit_ma(b->chg_reg13));
        n = pm_line(n, "Input limit", v);
    }
    snprintf(v, sizeof(v), "%.0fmA", (double)s->chg_i_ma);
    n = pm_line(n, "I charger ADC", v);
    pm_fmt_current(v, sizeof(v), s->bat_i_ma);
    n = pm_line(n, "I battery", v);
    snprintf(v, sizeof(v), "%.0fmA", (double)app->charge.peak_current_ma);
    n = pm_line(n, "Peak (session)", v);
    n = pm_line(n, "Trend", pm_trend_name(pm_charge_trend(&app->charge)));
    pm_fmt_duration(v, sizeof(v), pm_charge_session_ms(&app->charge, now_ms));
    n = pm_line(n, app->charge.session_active ? "Charging for" : "Last charge", v);
    if(app->have_battery) {
        snprintf(v, sizeof(v), "%.3f V", (double)b->charge_v_limit);
        n = pm_line(n, "Charge V limit", v);
        char flags[48];
        pm_charge_flags_text(flags, sizeof(flags), b->gauge_ok, ext, b->bat_status, b->chg_reg13);
        n = pm_line(n, "Flags", flags);
    }
    const uint8_t ev = pm_charge_event_count(&app->charge);
    if(ev == 0) n = pm_line(n, "No events yet", NULL);
    for(uint8_t i = 0; i < ev; i++) {
        const PmEvent* e = pm_charge_event(&app->charge, i);
        char age[16];
        pm_fmt_duration(age, sizeof(age), now_ms - e->tick_ms);
        snprintf(v, sizeof(v), "-%s", age);
        n = pm_line(n, pm_event_name(e->type), v);
    }
    return n;
}

static uint8_t pm_build_health(PowerMonitorApp* app) {
    char v[24];
    uint8_t n = 0;
    if(!app->have_battery) return pm_line(n, "Reading...", NULL);

    const PmBatteryInfo* b = &app->battery;
    PmHealth h;
    pm_health_eval(b, &h);

    snprintf(v, sizeof(v), "%umAh", (unsigned)b->remaining_mah);
    n = pm_line(n, "Remaining", v);
    snprintf(v, sizeof(v), "%umAh", (unsigned)b->full_mah);
    n = pm_line(n, "Full charge", v);
    snprintf(v, sizeof(v), "%umAh", (unsigned)b->design_mah);
    n = pm_line(n, "Design", v);
    if(h.soh_valid) {
        snprintf(v, sizeof(v), "%u%%", (unsigned)h.soh_pct);
    } else {
        snprintf(v, sizeof(v), "n/a");
    }
    n = pm_line(n, "SOH (gauge)", v);
    if(h.ratio_valid) {
        snprintf(v, sizeof(v), "%.1f%%", (double)h.ratio_pct);
    } else {
        snprintf(v, sizeof(v), "n/a");
    }
    n = pm_line(n, "Full/Design", v);
    if(b->ext_valid) {
        snprintf(v, sizeof(v), "%u", (unsigned)b->cycle_count);
    } else {
        snprintf(v, sizeof(v), "n/a (ext off)");
    }
    n = pm_line(n, "Cycles", v);

    uint32_t minutes;
    bool est;
    char t[16];
    const float i_ma = app->sample.bat_i_ma;
    if(pm_health_time_to_empty(b, i_ma, &minutes, &est)) {
        pm_fmt_minutes(t, sizeof(t), minutes);
        snprintf(v, sizeof(v), "%s%s", est ? "~" : "", t);
    } else {
        snprintf(v, sizeof(v), "--");
    }
    n = pm_line(n, "To empty", v);
    if(pm_health_time_to_full(b, i_ma, &minutes, &est)) {
        pm_fmt_minutes(t, sizeof(t), minutes);
        snprintf(v, sizeof(v), "%s%s", est ? "~" : "", t);
    } else {
        snprintf(v, sizeof(v), "--");
    }
    n = pm_line(n, "To full", v);
    snprintf(v, sizeof(v), "%.1f C", (double)app->sample.temp_c);
    n = pm_line(n, "Temperature", v);
    snprintf(v, sizeof(v), "%u%%", (unsigned)app->sample.soc_pct);
    n = pm_line(n, "Charge level", v);
    n = pm_line(n, "Gauge", b->gauge_ok ? "OK" : "NOT OK");
    n = pm_line(n, "~ = estimate", NULL);
    return n;
}

static uint8_t pm_build_settings(PowerMonitorApp* app) {
    char v[24];
    uint8_t n = 0;
    for(uint8_t i = 0; i < PmSettingCount; i++) {
        switch((PmSettingItem)i) {
        case PmSettingSample:
            snprintf(v, sizeof(v), "%lu ms", (unsigned long)pm_settings_sample_ms(&app->settings));
            break;
        case PmSettingCsvInterval:
            snprintf(v, sizeof(v), "%lu s", (unsigned long)(pm_settings_csv_ms(&app->settings) / 1000u));
            break;
        case PmSettingCsvLog:
            snprintf(v, sizeof(v), "%s", app->log_active ? "ON" : "OFF");
            break;
        case PmSettingExtReads:
            snprintf(v, sizeof(v), "%s", app->settings.ext_reads ? "ON" : "OFF");
            break;
        case PmSettingBacklight:
            snprintf(v, sizeof(v), "%s", app->settings.backlight ? "ON" : "AUTO");
            break;
        default:
            v[0] = '\0';
            break;
        }
        n = pm_line(n, pm_settings_label((PmSettingItem)i), v);
    }
    return n;
}

static void pm_draw_splash(Canvas* canvas) {
    /* Portrait on the left (full height), title block on the right */
    const int32_t ox = 0;
    const int32_t oy = 64 - (int32_t)PM_POPE_H;
    for(uint8_t y = 0; y < PM_POPE_H; y++) {
        for(uint8_t x = 0; x < PM_POPE_W; x++) {
            if(pm_pope_pixel(x, y)) canvas_draw_dot(canvas, ox + x, oy + y);
        }
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 60, 17, "Papa");
    canvas_draw_str(canvas, 60, 29, "Monitor");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 60, 41, "v1.0");
    canvas_draw_str(canvas, 60, 51, "read-only");
    /* Keep the hint fully on screen whatever the font width is */
    const char* hint = "press any key";
    const int32_t hint_w = (int32_t)canvas_string_width(canvas, hint);
    canvas_draw_str(canvas, (60 + hint_w <= 128) ? 60 : 128 - hint_w, 62, hint);
    canvas_draw_str_aligned(canvas, 127, 7, AlignRight, AlignBottom, "+3");
}

void pm_ui_draw(Canvas* canvas, PowerMonitorApp* app, uint32_t now_ms) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->splash && (int32_t)(app->splash_until_ms - now_ms) > 0) {
        pm_draw_splash(canvas);
        return;
    }

    if(app->view == PmViewGraphs) {
        pm_draw_graphs(canvas, app, now_ms);
    } else {
        pm_draw_header(canvas, app, pm_view_titles[app->view]);
        switch(app->view) {
        case PmViewDashboard:
            pm_draw_dashboard(canvas, app);
            break;
        case PmViewStats:
            pm_draw_stats(canvas, app, now_ms);
            break;
        case PmViewEnergy:
            pm_draw_list(canvas, app, pm_build_energy(app), -1);
            break;
        case PmViewCharging:
            pm_draw_list(canvas, app, pm_build_charging(app, now_ms), -1);
            break;
        case PmViewHealth:
            pm_draw_list(canvas, app, pm_build_health(app), -1);
            break;
        case PmViewSettings:
            pm_draw_list(canvas, app, pm_build_settings(app), app->settings_cursor);
            break;
        default:
            break;
        }
    }
    pm_draw_toast(canvas, app, now_ms);
}

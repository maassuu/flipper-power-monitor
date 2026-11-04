#include "test_framework.h"
#include "../pm_csv.h"
#include "../pm_format.h"
#include "../pm_settings.h"

static void test_duration(void) {
    char b[24];
    pm_fmt_duration(b, sizeof(b), 0);
    CHECK_STR(b, "00:00:00");
    pm_fmt_duration(b, sizeof(b), 3723999);
    CHECK_STR(b, "01:02:03");
    pm_fmt_duration(b, sizeof(b), 90061000u); /* 1 d 1 h 1 m 1 s */
    CHECK_STR(b, "1d 01:01");
}

static void test_minutes(void) {
    char b[16];
    pm_fmt_minutes(b, sizeof(b), 45);
    CHECK_STR(b, "45m");
    pm_fmt_minutes(b, sizeof(b), 312);
    CHECK_STR(b, "5h12m");
    pm_fmt_minutes(b, sizeof(b), UINT32_MAX);
    CHECK_STR(b, "--");
}

static void test_energy_fmt(void) {
    char b[24];
    pm_fmt_energy(b, sizeof(b), 12.345);
    CHECK_STR(b, "12.35mWh");
    pm_fmt_energy(b, sizeof(b), 1234.5);
    CHECK_STR(b, "1.2345Wh");
    pm_fmt_energy(b, sizeof(b), -33.333);
    CHECK_STR(b, "-33.33mWh");
    pm_fmt_energy(b, sizeof(b), -2500.0);
    CHECK_STR(b, "-2.5000Wh");
}

static void test_current_power_fmt(void) {
    char b[16];
    pm_fmt_current(b, sizeof(b), 419.6f);
    CHECK_STR(b, "+420mA");
    pm_fmt_current(b, sizeof(b), -35.4f);
    CHECK_STR(b, "-35mA");
    pm_fmt_current(b, sizeof(b), 0.2f);
    CHECK_STR(b, "0mA");
    pm_fmt_power(b, sizeof(b), 1.734f);
    CHECK_STR(b, "+1.73W");
    pm_fmt_power(b, sizeof(b), -0.004f);
    CHECK_STR(b, "0.00W");
    pm_fmt_power(b, sizeof(b), -0.5f);
    CHECK_STR(b, "-0.50W");
}

static void test_csv(void) {
    char line[PM_CSV_LINE_MAX];
    CHECK(pm_csv_header(line, sizeof(line)) > 0);
    CHECK(strncmp(line, "date,time,uptime_ms,usb_v,bat_v,bat_current_ma", 46) == 0);
    CHECK(line[strlen(line) - 1] == '\n');

    const PmClock clk = {2026, 11, 4, 18, 5, 9};
    PmSample s;
    memset(&s, 0, sizeof(s));
    s.tick_ms = 123456;
    s.usb_v = 5.1f;
    s.bat_v = 4.123f;
    s.bat_i_ma = -420.0f;
    s.chg_i_ma = 0.0f;
    s.bat_p_w = -1.7317f;
    s.temp_c = 24.46f;
    s.soc_pct = 78;
    s.usb_present = true;
    PmEnergy e;
    pm_energy_reset(&e);
    e.charge_mwh = 1.5;
    e.discharge_mwh = 12.25;

    const size_t n = pm_csv_row(line, sizeof(line), &clk, &s, PmPhaseNotCharging, &e);
    CHECK(n > 0);
    CHECK_STR(
        line,
        "2026-11-04,18:05:09,123456,5.1,4.123,-420,0,-1.732,24.5,78,not_charging,1,1.500,12.250\n");

    /* same number of columns as the header */
    char header[PM_CSV_LINE_MAX];
    pm_csv_header(header, sizeof(header));
    int commas_h = 0, commas_r = 0;
    for(const char* p = header; *p; p++) commas_h += (*p == ',');
    for(const char* p = line; *p; p++) commas_r += (*p == ',');
    CHECK(commas_h == commas_r);

    /* truncation reported */
    char tiny[16];
    CHECK(pm_csv_row(tiny, sizeof(tiny), &clk, &s, PmPhaseDone, &e) == 0);

    char name[32];
    CHECK(pm_csv_filename(name, sizeof(name), &clk) > 0);
    CHECK_STR(name, "pm_20261104_180509.csv");
}

static void test_settings(void) {
    PmSettings st;
    pm_settings_defaults(&st);
    CHECK(pm_settings_sample_ms(&st) == 500u);
    CHECK(pm_settings_csv_ms(&st) == 5000u);
    CHECK(st.ext_reads == 1);
    CHECK(st.backlight == 0);

    pm_settings_cycle(&st, PmSettingSample);
    CHECK(pm_settings_sample_ms(&st) == 1000u);
    pm_settings_cycle(&st, PmSettingSample);
    CHECK(pm_settings_sample_ms(&st) == 250u); /* wraps */
    for(int i = 0; i < 4; i++) pm_settings_cycle(&st, PmSettingCsvInterval);
    CHECK(pm_settings_csv_ms(&st) == 1000u);
    pm_settings_cycle(&st, PmSettingExtReads);
    CHECK(st.ext_reads == 0);
    pm_settings_cycle(&st, PmSettingBacklight);
    CHECK(st.backlight == 1);

    st.sample_idx = 99;
    st.csv_idx = 42;
    st.ext_reads = 7;
    CHECK(pm_settings_sample_ms(&st) == 500u); /* safe even before sanitize */
    pm_settings_sanitize(&st);
    CHECK(st.sample_idx == 1);
    CHECK(st.csv_idx == 1);
    CHECK(st.ext_reads == 1);
    CHECK_STR(pm_settings_label(PmSettingCsvLog), "CSV logging");
}

void test_format_suite(void) {
    printf("[format]\n");
    RUN(test_duration);
    RUN(test_minutes);
    RUN(test_energy_fmt);
    RUN(test_current_power_fmt);
}

void test_csv_suite(void) {
    printf("[csv]\n");
    RUN(test_csv);
}

void test_settings_suite(void) {
    printf("[settings]\n");
    RUN(test_settings);
}

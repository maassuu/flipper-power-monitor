#include "test_framework.h"
#include "../pm_charge.h"

/* Build REG0B: VBUS_STAT[7:5] CHRG_STAT[4:3] PG_STAT[2] RES[1] VSYS_STAT[0] */
static uint8_t reg0b(uint8_t vbus, uint8_t chrg, uint8_t pg) {
    return (uint8_t)((vbus << 5) | (chrg << 3) | (pg << 2) | 0x02u);
}

static void test_reg_decoding(void) {
    const uint8_t r = reg0b(2, 2, 1);
    CHECK(PM_REG0B_VBUS_STAT(r) == 2);
    CHECK(PM_REG0B_CHRG_STAT(r) == 2);
    CHECK(PM_REG0B_PG_STAT(r) == 1);
    CHECK(PM_REG0B_VSYS_STAT(r) == 0);
    CHECK_STR(pm_vbus_type_name(PM_REG0B_VBUS_STAT(r)), "adapter");
    CHECK_STR(pm_vbus_type_name(1), "USB host");
    CHECK_STR(pm_vbus_type_name(7), "OTG");
    CHECK_STR(pm_vbus_type_name(4), "unknown");

    /* REG13: VDPM[7] IDPM[6] IDPM_LIM[5:0] */
    CHECK(pm_charger_input_limit_ma(0x00) == 100);
    CHECK(pm_charger_input_limit_ma(0x1C) == 1500); /* 100 + 28*50 */
    CHECK(pm_charger_input_limit_ma(0xFF) == 3250); /* 100 + 63*50, status bits ignored */
    CHECK(PM_REG13_IDPM_STAT(0x40) == 1);
    CHECK(PM_REG13_VDPM_STAT(0x80) == 1);
}

static void test_phase_from(void) {
    /* HAL only */
    CHECK(pm_charge_phase_from(false, false, false, 0) == PmPhaseNotCharging);
    CHECK(pm_charge_phase_from(true, false, false, 0) == PmPhaseCharging);
    CHECK(pm_charge_phase_from(true, true, false, 0) == PmPhaseDone);
    /* With REG0B */
    CHECK(pm_charge_phase_from(true, false, true, reg0b(1, 1, 1)) == PmPhasePreCharge);
    CHECK(pm_charge_phase_from(true, false, true, reg0b(1, 2, 1)) == PmPhaseFastCharge);
    CHECK(pm_charge_phase_from(true, true, true, reg0b(1, 3, 1)) == PmPhaseDone);
    CHECK(pm_charge_phase_from(false, false, true, reg0b(0, 0, 0)) == PmPhaseNotCharging);
    CHECK(pm_phase_is_charging(PmPhaseFastCharge));
    CHECK(!pm_phase_is_charging(PmPhaseDone));
    CHECK_STR(pm_charge_phase_short(PmPhaseDone), "FULL");
    CHECK_STR(pm_charge_phase_csv(PmPhaseFastCharge), "fast");
}

static void test_flags_text(void) {
    char buf[64];
    pm_charge_flags_text(buf, sizeof(buf), true, false, 0, 0);
    CHECK_STR(buf, "none");
    pm_charge_flags_text(buf, sizeof(buf), false, false, 0, 0);
    CHECK_STR(buf, "GAUGE!");
    pm_charge_flags_text(buf, sizeof(buf), true, true, PM_BS_BATTPRES, 0);
    CHECK_STR(buf, "none");
    pm_charge_flags_text(
        buf, sizeof(buf), true, true, PM_BS_BATTPRES | PM_BS_OTC | PM_BS_FC, 0x40);
    CHECK_STR(buf, "OTC FC IINDPM");
    pm_charge_flags_text(buf, sizeof(buf), true, true, 0, 0);
    CHECK_STR(buf, "NOBAT");
    /* truncation stays NUL terminated */
    char small[6];
    pm_charge_flags_text(
        small, sizeof(small), false, true, PM_BS_OTC | PM_BS_OTD | PM_BS_CHGINH, 0xC0);
    CHECK(strlen(small) < sizeof(small));
}

static void test_events_full_cycle(void) {
    PmCharge c;
    pm_charge_reset(&c);
    pm_charge_update(&c, 0, false, false, PmPhaseNotCharging, -40.0f);
    CHECK(pm_charge_event_count(&c) == 0); /* initial state produces no events */
    CHECK(pm_charge_session_ms(&c, 0) == 0);

    pm_charge_update(&c, 1000, true, false, PmPhaseNotCharging, -40.0f);
    pm_charge_update(&c, 2000, true, false, PmPhasePreCharge, 100.0f);
    pm_charge_update(&c, 3000, true, false, PmPhaseFastCharge, 900.0f);
    pm_charge_update(&c, 4000, true, false, PmPhaseFastCharge, 950.0f);
    CHECK(pm_charge_session_ms(&c, 5000) == 3000u);
    CHECK_NEAR(c.peak_current_ma, 950.0, 1e-6);

    pm_charge_update(&c, 62000, true, false, PmPhaseDone, 0.0f);
    CHECK(pm_charge_session_ms(&c, 99999) == 60000u); /* finished session is frozen */
    pm_charge_update(&c, 63000, false, false, PmPhaseNotCharging, -40.0f);

    CHECK(pm_charge_event_count(&c) == 5);
    CHECK(pm_charge_event(&c, 0)->type == PmEventUsbDisconnected);
    CHECK(pm_charge_event(&c, 1)->type == PmEventChargeDone);
    CHECK(pm_charge_event(&c, 1)->tick_ms == 62000u);
    CHECK(pm_charge_event(&c, 2)->type == PmEventFastCharge);
    CHECK(pm_charge_event(&c, 3)->type == PmEventChargeStarted);
    CHECK(pm_charge_event(&c, 4)->type == PmEventUsbConnected);
    CHECK(pm_charge_event(&c, 5) == NULL);

    pm_charge_clear_events(&c);
    CHECK(pm_charge_event_count(&c) == 0);
}

static void test_charge_stopped_and_otg(void) {
    PmCharge c;
    pm_charge_reset(&c);
    pm_charge_update(&c, 0, true, false, PmPhaseCharging, 500.0f);
    CHECK(c.session_active); /* already charging when monitoring started */
    pm_charge_update(&c, 5000, false, false, PmPhaseNotCharging, -30.0f);
    CHECK(!c.session_active);
    CHECK(pm_charge_event(&c, 0)->type == PmEventChargeStopped);
    CHECK(pm_charge_session_ms(&c, 10000) == 5000u);

    pm_charge_update(&c, 6000, true, true, PmPhaseNotCharging, -200.0f);
    CHECK(pm_charge_event(&c, 0)->type == PmEventUsbConnected);
    CHECK(pm_charge_event(&c, 1)->type == PmEventOtgOn);
}

static void test_event_log_wraps(void) {
    PmCharge c;
    pm_charge_reset(&c);
    pm_charge_update(&c, 0, false, false, PmPhaseNotCharging, 0.0f);
    for(uint32_t i = 1; i <= 10; i++) {
        pm_charge_update(&c, i * 1000u, (i % 2u) == 1u, false, PmPhaseNotCharging, 0.0f);
    }
    CHECK(pm_charge_event_count(&c) == PM_EVENT_LOG_SIZE);
    CHECK(pm_charge_event(&c, 0)->tick_ms == 10000u); /* newest first */
    CHECK(pm_charge_event(&c, PM_EVENT_LOG_SIZE - 1u)->tick_ms == 5000u);
}

static void feed_trend(PmCharge* c, uint32_t start, float first, float second) {
    for(uint32_t i = 0; i < PM_TREND_POINTS; i++) {
        const float v = (i < PM_TREND_POINTS / 2u) ? first : second;
        pm_charge_update(c, start + i * 1000u, true, false, PmPhaseFastCharge, v);
    }
}

static void test_trend(void) {
    PmCharge c;
    pm_charge_reset(&c);
    pm_charge_update(&c, 0, true, false, PmPhaseFastCharge, 800.0f);
    CHECK(pm_charge_trend(&c) == PmTrendUnknown);

    pm_charge_reset(&c);
    feed_trend(&c, 0, 900.0f, 700.0f);
    CHECK(pm_charge_trend(&c) == PmTrendFalling);

    pm_charge_reset(&c);
    feed_trend(&c, 0, 500.0f, 600.0f);
    CHECK(pm_charge_trend(&c) == PmTrendRising);

    pm_charge_reset(&c);
    feed_trend(&c, 0, 500.0f, 510.0f);
    CHECK(pm_charge_trend(&c) == PmTrendStable);

    /* sub-second updates do not add trend points */
    pm_charge_reset(&c);
    for(uint32_t i = 0; i < 40; i++) {
        pm_charge_update(&c, i * 250u, true, false, PmPhaseFastCharge, 500.0f);
    }
    CHECK(c.trend_count == 10);
    CHECK_STR(pm_trend_name(PmTrendFalling), "falling");
}

void test_charge_suite(void) {
    printf("[charge]\n");
    RUN(test_reg_decoding);
    RUN(test_phase_from);
    RUN(test_flags_text);
    RUN(test_events_full_cycle);
    RUN(test_charge_stopped_and_otg);
    RUN(test_event_log_wraps);
    RUN(test_trend);
}

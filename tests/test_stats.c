#include "test_framework.h"
#include "../pm_stats.h"

static void add3(PmStats* s, uint32_t t, float v, float i, float p) {
    const float vals[PmChannelCount] = {v, i, p};
    pm_stats_add(s, t, vals);
}

static void test_empty(void) {
    PmStats s;
    pm_stats_reset(&s, 1000);
    CHECK(s.samples == 0);
    CHECK_NEAR(pm_stats_avg(&s, PmChannelVoltage), 0.0, 1e-9);
    CHECK(pm_stats_duration_ms(&s, 6000) == 5000u);
}

static void test_min_max(void) {
    PmStats s;
    pm_stats_reset(&s, 0);
    add3(&s, 0, 4.10f, -50.0f, -0.2f);
    add3(&s, 500, 4.20f, 300.0f, 1.2f);
    add3(&s, 1000, 4.00f, 100.0f, 0.4f);
    CHECK_NEAR(s.ch[PmChannelVoltage].min, 4.00, 1e-6);
    CHECK_NEAR(s.ch[PmChannelVoltage].max, 4.20, 1e-6);
    CHECK_NEAR(s.ch[PmChannelCurrent].min, -50.0, 1e-6);
    CHECK_NEAR(s.ch[PmChannelCurrent].max, 300.0, 1e-6);
    CHECK_NEAR(s.ch[PmChannelPower].min, -0.2, 1e-6);
    CHECK_NEAR(s.ch[PmChannelPower].max, 1.2, 1e-6);
    CHECK(s.samples == 3);
}

static void test_single_sample_avg(void) {
    PmStats s;
    pm_stats_reset(&s, 0);
    add3(&s, 0, 3.9f, 12.0f, 0.05f);
    CHECK_NEAR(pm_stats_avg(&s, PmChannelVoltage), 3.9, 1e-6);
    CHECK_NEAR(pm_stats_avg(&s, PmChannelCurrent), 12.0, 1e-6);
}

static void test_time_weighted_avg(void) {
    PmStats s;
    pm_stats_reset(&s, 0);
    /* 100 mA for 9 s, then 1000 mA for 1 s (step at t=9 s, sampled densely) */
    add3(&s, 0, 4.0f, 100.0f, 0.4f);
    add3(&s, 9000, 4.0f, 100.0f, 0.4f);
    add3(&s, 9000, 4.0f, 1000.0f, 4.0f); /* dt = 0: no weight */
    add3(&s, 10000, 4.0f, 1000.0f, 4.0f);
    /* time-weighted: (100*9 + 1000*1)/10 = 190; arithmetic mean would be 550 */
    CHECK_NEAR(pm_stats_avg(&s, PmChannelCurrent), 190.0, 1e-4);
    CHECK_NEAR(pm_stats_avg(&s, PmChannelVoltage), 4.0, 1e-6);
}

static void test_break_skips_segment(void) {
    PmStats s;
    pm_stats_reset(&s, 0);
    add3(&s, 0, 4.0f, 0.0f, 0.0f);
    add3(&s, 1000, 4.0f, 0.0f, 0.0f);
    pm_stats_break(&s);
    add3(&s, 5000, 4.0f, 500.0f, 2.0f);
    add3(&s, 6000, 4.0f, 500.0f, 2.0f);
    /* segment 1000..5000 not integrated: (0*1 + 500*1)/2 */
    CHECK(s.weighted_ms == 2000u);
    CHECK_NEAR(pm_stats_avg(&s, PmChannelCurrent), 250.0, 1e-4);
}

static void test_reset(void) {
    PmStats s;
    pm_stats_reset(&s, 0);
    add3(&s, 0, 4.0f, 10.0f, 0.04f);
    add3(&s, 1000, 4.1f, 20.0f, 0.08f);
    pm_stats_reset(&s, 2000);
    CHECK(s.samples == 0);
    CHECK(s.weighted_ms == 0);
    add3(&s, 2000, 3.5f, -5.0f, -0.02f);
    CHECK_NEAR(s.ch[PmChannelVoltage].max, 3.5, 1e-6);
    CHECK(pm_stats_duration_ms(&s, 3000) == 1000u);
}

void test_stats_suite(void) {
    printf("[stats]\n");
    RUN(test_empty);
    RUN(test_min_max);
    RUN(test_single_sample_avg);
    RUN(test_time_weighted_avg);
    RUN(test_break_skips_segment);
    RUN(test_reset);
}

#include "test_framework.h"
#include "../pm_health.h"

static PmBatteryInfo info_default(void) {
    PmBatteryInfo i;
    memset(&i, 0, sizeof(i));
    i.gauge_ok = true;
    i.remaining_mah = 1000;
    i.full_mah = 2000;
    i.design_mah = 2100;
    i.soh_pct = 95;
    i.tte_min = PM_GAUGE_TIME_NA;
    i.ttf_min = PM_GAUGE_TIME_NA;
    return i;
}

static void test_eval_valid(void) {
    PmBatteryInfo i = info_default();
    PmHealth h;
    pm_health_eval(&i, &h);
    CHECK(h.soh_valid);
    CHECK(h.soh_pct == 95);
    CHECK(h.ratio_valid);
    CHECK_NEAR(h.ratio_pct, 2000.0 * 100.0 / 2100.0, 1e-3);
    CHECK(h.capacity_valid);
}

static void test_eval_invalid(void) {
    PmBatteryInfo i = info_default();
    PmHealth h;
    i.soh_pct = 0; /* driver returns 0 on I2C error */
    i.design_mah = 0;
    pm_health_eval(&i, &h);
    CHECK(!h.soh_valid);
    CHECK(!h.ratio_valid);
    CHECK(!h.capacity_valid);

    i = info_default();
    i.full_mah = 4000; /* > 150 % of design: implausible */
    pm_health_eval(&i, &h);
    CHECK(!h.ratio_valid);

    i = info_default();
    i.soh_pct = 120;
    pm_health_eval(&i, &h);
    CHECK(!h.soh_valid);
}

static void test_time_from_gauge(void) {
    PmBatteryInfo i = info_default();
    uint32_t m;
    bool est;
    i.ext_valid = true;
    i.tte_min = 312;
    CHECK(pm_health_time_to_empty(&i, -200.0f, &m, &est));
    CHECK(m == 312u);
    CHECK(!est);
    /* gauge says n/a -> no estimate even if current is negative */
    CHECK(!pm_health_time_to_full(&i, -200.0f, &m, &est));
    i.ttf_min = 45;
    CHECK(pm_health_time_to_full(&i, 800.0f, &m, &est));
    CHECK(m == 45u);
}

static void test_time_estimates(void) {
    PmBatteryInfo i = info_default();
    uint32_t m;
    bool est;
    CHECK(pm_health_time_to_empty(&i, -100.0f, &m, &est));
    CHECK(est);
    CHECK(m == 600u); /* 1000 mAh / 100 mA = 10 h */
    CHECK(!pm_health_time_to_empty(&i, 50.0f, &m, &est)); /* charging */
    CHECK(!pm_health_time_to_empty(&i, -2.0f, &m, &est)); /* idle */

    CHECK(pm_health_time_to_full(&i, 500.0f, &m, &est));
    CHECK(m == 120u); /* 1000 mAh / 500 mA = 2 h */
    CHECK(!pm_health_time_to_full(&i, -100.0f, &m, &est));
    i.remaining_mah = 2000;
    CHECK(!pm_health_time_to_full(&i, 500.0f, &m, &est)); /* already full */
}

void test_health_suite(void) {
    printf("[health]\n");
    RUN(test_eval_valid);
    RUN(test_eval_invalid);
    RUN(test_time_from_gauge);
    RUN(test_time_estimates);
}

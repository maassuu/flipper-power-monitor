#include "test_framework.h"
#include "../pm_history.h"

static PmHistory history;

static void push(uint32_t t, float v) {
    const float vals[PmChannelCount] = {v, v * 100.0f, v * 2.0f};
    pm_history_push(&history, t, vals);
}

static void test_order_and_wrap(void) {
    pm_history_reset(&history);
    CHECK(pm_history_count(&history) == 0);
    CHECK(pm_history_get(&history, 0) == NULL);

    for(uint32_t i = 0; i < PM_HISTORY_CAPACITY + 10u; i++) push(i * 250u, (float)i);
    CHECK(pm_history_count(&history) == PM_HISTORY_CAPACITY);
    /* oldest is entry #10 */
    CHECK_NEAR(pm_history_get(&history, 0)->value[PmChannelVoltage], 10.0, 1e-6);
    CHECK_NEAR(
        pm_history_get(&history, PM_HISTORY_CAPACITY - 1u)->value[PmChannelVoltage],
        (double)(PM_HISTORY_CAPACITY + 9u),
        1e-6);
    CHECK(pm_history_get(&history, PM_HISTORY_CAPACITY) == NULL);
}

static void test_capacity_covers_window(void) {
    /* 60 s at 250 ms needs 241 points (both ends) */
    CHECK(PM_HISTORY_CAPACITY >= PM_HISTORY_WINDOW_MS / 250u + 1u);
}

static void test_range_window(void) {
    pm_history_reset(&history);
    push(0, 9.0f); /* outside window when now = 70 s */
    push(20000, 4.0f);
    push(40000, 4.2f);
    push(70000, 3.9f);
    float mn, mx;
    CHECK(pm_history_range(&history, PmChannelVoltage, 70000, PM_HISTORY_WINDOW_MS, &mn, &mx));
    CHECK_NEAR(mn, 3.9, 1e-6);
    CHECK_NEAR(mx, 4.2, 1e-6);
    CHECK(!pm_history_range(&history, PmChannelVoltage, 200000, 1000, &mn, &mx));
}

static void test_future_point_in_window(void) {
    /* review PR #2: a point newer than now is in the window, not "very old" */
    CHECK(pm_history_age_ms(1000, 1500) == 0u);
    CHECK(pm_history_age_ms(1500, 1000) == 500u);
    CHECK(pm_history_age_ms(0x00000100u, 0xFFFFFF00u) == 0x200u); /* across wrap */
    pm_history_reset(&history);
    push(10000, 4.0f);
    push(10250, 4.3f); /* stamped after "now" below */
    float mn, mx;
    CHECK(pm_history_range(&history, PmChannelVoltage, 10100, PM_HISTORY_WINDOW_MS, &mn, &mx));
    CHECK_NEAR(mx, 4.3, 1e-6);
    CHECK_NEAR(mn, 4.0, 1e-6);
}

static void test_autoscale(void) {
    float lo, hi;
    /* flat signal gets the minimum span, centered */
    pm_autoscale(4.1f, 4.1f, 0.02f, &lo, &hi);
    CHECK_NEAR(hi - lo, 0.024, 1e-5);
    CHECK_NEAR((hi + lo) / 2.0f, 4.1, 1e-5);
    /* wide signal: 10 % padding */
    pm_autoscale(-100.0f, 300.0f, 20.0f, &lo, &hi);
    CHECK_NEAR(lo, -140.0, 1e-4);
    CHECK_NEAR(hi, 340.0, 1e-4);
    /* swapped arguments are handled */
    pm_autoscale(300.0f, -100.0f, 20.0f, &lo, &hi);
    CHECK_NEAR(lo, -140.0, 1e-4);
}

static void test_scale_y(void) {
    CHECK(pm_scale_y(0.0f, 0.0f, 10.0f, 10, 60) == 60);
    CHECK(pm_scale_y(10.0f, 0.0f, 10.0f, 10, 60) == 10);
    CHECK(pm_scale_y(5.0f, 0.0f, 10.0f, 10, 60) == 35);
    CHECK(pm_scale_y(99.0f, 0.0f, 10.0f, 10, 60) == 10); /* clamped */
    CHECK(pm_scale_y(-9.0f, 0.0f, 10.0f, 10, 60) == 60);
    CHECK(pm_scale_y(1.0f, 5.0f, 5.0f, 10, 60) == 35); /* degenerate range */
}

void test_history_suite(void) {
    printf("[history]\n");
    RUN(test_order_and_wrap);
    RUN(test_capacity_covers_window);
    RUN(test_range_window);
    RUN(test_future_point_in_window);
    RUN(test_autoscale);
    RUN(test_scale_y);
}

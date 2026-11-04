#include "test_framework.h"

int pm_test_failures = 0;
int pm_test_checks = 0;

/* The framework itself: CHECK_NEAR must reject NaN and infinities. */
static void test_framework_suite(void) {
    printf("[framework]\n");
    CHECK(pm_test_near(1.0, 1.0 + 1e-9, 1e-6));
    CHECK(!pm_test_near(1.0, 1.1, 1e-6));
    const double nan_v = (double)NAN;
    const double inf_v = (double)INFINITY;
    CHECK(!pm_test_near(nan_v, 1.0, 1e-6));
    CHECK(!pm_test_near(1.0, nan_v, 1e-6));
    CHECK(!pm_test_near(nan_v, nan_v, 1e-6));
    CHECK(!pm_test_near(inf_v, -inf_v, 1e-6));
}

int main(void) {
    test_framework_suite();
    test_energy_suite();
    test_stats_suite();
    test_history_suite();
    test_charge_suite();
    test_health_suite();
    test_format_suite();
    test_csv_suite();
    test_settings_suite();
    test_pope_suite();

    printf("\n%d checks, %d failures\n", pm_test_checks, pm_test_failures);
    return pm_test_failures == 0 ? 0 : 1;
}

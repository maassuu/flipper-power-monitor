#include "test_framework.h"

int pm_test_failures = 0;
int pm_test_checks = 0;

int main(void) {
    test_energy_suite();
    test_stats_suite();
    test_history_suite();
    test_charge_suite();
    test_health_suite();
    test_format_suite();
    test_csv_suite();
    test_settings_suite();

    printf("\n%d checks, %d failures\n", pm_test_checks, pm_test_failures);
    return pm_test_failures == 0 ? 0 : 1;
}

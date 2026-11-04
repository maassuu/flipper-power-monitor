#include "test_framework.h"
#include "../pm_energy.h"

static void test_power_formula(void) {
    CHECK_NEAR(pm_power_w(4.0f, 500.0f), 2.0, 1e-6);
    CHECK_NEAR(pm_power_w(3.7f, -100.0f), -0.37, 1e-6);
    CHECK_NEAR(pm_power_w(4.2f, 0.0f), 0.0, 1e-9);
}

static void test_constant_charge_one_hour(void) {
    PmEnergy e;
    pm_energy_reset(&e);
    /* 1 W, 250 mA for 1 h sampled every second */
    for(uint32_t t = 0; t <= 3600000u; t += 1000u) {
        pm_energy_add(&e, t, 1.0f, 250.0f);
    }
    CHECK_NEAR(e.charge_mwh, 1000.0, 1e-6);
    CHECK_NEAR(e.discharge_mwh, 0.0, 1e-9);
    CHECK_NEAR(e.charge_mah, 250.0, 1e-6);
    CHECK_NEAR(pm_energy_net_mwh(&e), 1000.0, 1e-6);
    CHECK(e.integrated_ms == 3600000u);
}

static void test_discharge(void) {
    PmEnergy e;
    pm_energy_reset(&e);
    pm_energy_add(&e, 0, -0.36f, -100.0f);
    pm_energy_add(&e, 10000, -0.36f, -100.0f);
    /* 0.36 W * 10 s = 3.6 J = 1 mWh */
    CHECK_NEAR(e.discharge_mwh, 1.0, 1e-6);
    CHECK_NEAR(e.charge_mwh, 0.0, 1e-9);
    CHECK_NEAR(e.discharge_mah, 100.0 * 10.0 / 3600.0, 1e-6);
    CHECK_NEAR(pm_energy_net_mwh(&e), -1.0, 1e-6);
}

static void test_variable_interval(void) {
    PmEnergy a, b;
    pm_energy_reset(&a);
    pm_energy_reset(&b);
    /* Same constant power, different sampling: same energy */
    for(uint32_t t = 0; t <= 8000u; t += 250u) pm_energy_add(&a, t, 2.0f, 500.0f);
    uint32_t t = 0;
    const uint32_t steps[] = {250, 500, 1000, 250, 1000, 2000, 1000, 500, 1500};
    pm_energy_add(&b, 0, 2.0f, 500.0f);
    for(size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        t += steps[i];
        pm_energy_add(&b, t, 2.0f, 500.0f);
    }
    CHECK(t == 8000u);
    CHECK_NEAR(a.charge_mwh, b.charge_mwh, 1e-9);
    CHECK_NEAR(a.charge_mwh, 2.0 * 8000.0 / 3600.0, 1e-9);
}

static void test_zero_crossing_split(void) {
    double pos, neg;
    /* +1 -> -1 over 2 s: 0.5 positive, 0.5 negative */
    pm_split_trapezoid(1.0f, -1.0f, 2.0, &pos, &neg);
    CHECK_NEAR(pos, 0.5, 1e-9);
    CHECK_NEAR(neg, 0.5, 1e-9);
    /* -3 -> +1 over 4: crossing at t=3, neg area 4.5, pos area 0.5 */
    pm_split_trapezoid(-3.0f, 1.0f, 4.0, &pos, &neg);
    CHECK_NEAR(neg, 4.5, 1e-9);
    CHECK_NEAR(pos, 0.5, 1e-9);
    /* zero dt */
    pm_split_trapezoid(5.0f, 5.0f, 0.0, &pos, &neg);
    CHECK_NEAR(pos, 0.0, 1e-12);
    CHECK_NEAR(neg, 0.0, 1e-12);
}

static void test_gap_and_break(void) {
    PmEnergy e;
    pm_energy_reset(&e);
    pm_energy_add(&e, 0, 1.0f, 100.0f);
    pm_energy_add(&e, PM_ENERGY_MAX_GAP_MS + 1u, 1.0f, 100.0f);
    CHECK_NEAR(e.charge_mwh, 0.0, 1e-12);
    CHECK(e.gaps == 1);

    pm_energy_break(&e);
    CHECK(e.gaps == 2);
    pm_energy_add(&e, 50000, 1.0f, 100.0f); /* starts a new segment */
    CHECK_NEAR(e.charge_mwh, 0.0, 1e-12);
    pm_energy_add(&e, 53600, 1.0f, 100.0f);
    CHECK_NEAR(e.charge_mwh, 1.0, 1e-9);
}

static void test_tick_wraparound(void) {
    PmEnergy e;
    pm_energy_reset(&e);
    pm_energy_add(&e, 0xFFFFFC18u, 1.0f, 0.0f); /* 1000 ms before wrap */
    pm_energy_add(&e, 0x00000DACu, 1.0f, 0.0f); /* 3500 ms after wrap */
    CHECK(e.integrated_ms == 4500u);
    CHECK_NEAR(e.charge_mwh, 4500.0 / 3600.0, 1e-9);
}

void test_energy_suite(void) {
    printf("[energy]\n");
    RUN(test_power_formula);
    RUN(test_constant_charge_one_hour);
    RUN(test_discharge);
    RUN(test_variable_interval);
    RUN(test_zero_crossing_split);
    RUN(test_gap_and_break);
    RUN(test_tick_wraparound);
}

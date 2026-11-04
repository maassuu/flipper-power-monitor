#include "pm_energy.h"

#include <string.h>

/* W * ms = mJ; 1 mWh = 3600 mJ.
 * Note: the Flipper SDK builds with -fsingle-precision-constant, so literals
 * like 0.5 are float. Double math below uses integer literals on purpose. */
#define PM_MS_PER_HOUR (3600000)
#define PM_MJ_PER_MWH  (3600)

float pm_power_w(float voltage_v, float current_ma) {
    return voltage_v * current_ma / 1000.0f;
}

void pm_energy_reset(PmEnergy* energy) {
    memset(energy, 0, sizeof(*energy));
}

void pm_energy_break(PmEnergy* energy) {
    if(energy->has_prev) energy->gaps++;
    energy->has_prev = false;
}

double pm_energy_net_mwh(const PmEnergy* energy) {
    return energy->charge_mwh - energy->discharge_mwh;
}

void pm_split_trapezoid(float a, float b, double dt, double* positive, double* negative) {
    double pos = 0;
    double neg = 0;

    if(dt > 0) {
        if((a >= 0.0f && b >= 0.0f) || (a <= 0.0f && b <= 0.0f)) {
            const double area = ((double)a + (double)b) * dt / 2;
            if(area >= 0) {
                pos = area;
            } else {
                neg = -area;
            }
        } else {
            /* Sign change inside the segment: split at the zero crossing */
            const double fa = (a < 0.0f) ? -(double)a : (double)a;
            const double fb = (b < 0.0f) ? -(double)b : (double)b;
            const double t0 = dt * fa / (fa + fb);
            const double area_a = fa * t0 / 2;
            const double area_b = fb * (dt - t0) / 2;
            if(a > 0.0f) {
                pos = area_a;
                neg = area_b;
            } else {
                neg = area_a;
                pos = area_b;
            }
        }
    }

    *positive = pos;
    *negative = neg;
}

void pm_energy_add(PmEnergy* energy, uint32_t tick_ms, float power_w, float current_ma) {
    if(energy->has_prev) {
        const uint32_t dt_ms = tick_ms - energy->prev_tick_ms; /* wrap-safe */
        if(dt_ms > 0 && dt_ms <= PM_ENERGY_MAX_GAP_MS) {
            double pos, neg;
            pm_split_trapezoid(energy->prev_power_w, power_w, (double)dt_ms, &pos, &neg);
            energy->charge_mwh += pos / PM_MJ_PER_MWH;
            energy->discharge_mwh += neg / PM_MJ_PER_MWH;

            pm_split_trapezoid(energy->prev_current_ma, current_ma, (double)dt_ms, &pos, &neg);
            energy->charge_mah += pos / PM_MS_PER_HOUR;
            energy->discharge_mah += neg / PM_MS_PER_HOUR;

            energy->integrated_ms += dt_ms;
        } else if(dt_ms > PM_ENERGY_MAX_GAP_MS) {
            energy->gaps++;
        }
    }

    energy->has_prev = true;
    energy->prev_tick_ms = tick_ms;
    energy->prev_power_w = power_w;
    energy->prev_current_ma = current_ma;
}

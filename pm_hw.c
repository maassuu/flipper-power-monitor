#include "pm_hw.h"
#include "pm_energy.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_power.h>
#include <string.h>

/* 8-bit I2C addresses, as in lib/drivers/bq27220_reg.h and bq25896_reg.h */
#define PM_GAUGE_ADDR   (0xAAu)
#define PM_CHARGER_ADDR (0xD6u)
#define PM_I2C_TIMEOUT  (50u)

/* BQ27220 standard commands (read-only) */
#define PM_GAUGE_CMD_BATTERY_STATUS (0x0Au)
#define PM_GAUGE_CMD_TIME_TO_EMPTY  (0x16u)
#define PM_GAUGE_CMD_TIME_TO_FULL   (0x18u)
#define PM_GAUGE_CMD_CYCLE_COUNT    (0x2Au)

/* BQ25896 status registers without read side effects */
#define PM_CHARGER_REG_STATUS (0x0Bu)
#define PM_CHARGER_REG_DPM    (0x13u)

/* Plausibility window for the gauge voltage. The driver returns 0 on error. */
#define PM_BAT_V_MIN (2.5f)
#define PM_BAT_V_MAX (4.6f)

void pm_hw_read_fast(PmSample* s, uint32_t now_ms, bool ext) {
    memset(s, 0, sizeof(*s));
    s->tick_ms = now_ms;

    s->usb_v = furi_hal_power_get_usb_voltage();
    s->bat_v = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    s->bat_i_ma = furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge) * 1000.0f;
    s->chg_i_ma = furi_hal_power_get_battery_current(FuriHalPowerICCharger) * 1000.0f;
    s->temp_c = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    s->soc_pct = furi_hal_power_get_pct();
    s->charging = furi_hal_power_is_charging();
    s->charge_done = furi_hal_power_is_charging_done();
    s->otg_enabled = furi_hal_power_is_otg_enabled();

    if(ext) {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        s->reg0b_valid = furi_hal_i2c_read_reg_8(
            &furi_hal_i2c_handle_power,
            PM_CHARGER_ADDR,
            PM_CHARGER_REG_STATUS,
            &s->chg_reg0b,
            PM_I2C_TIMEOUT);
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    }

    /* bq25896_get_vbus_voltage() returns 0 unless VBUS_GD is set */
    s->usb_present = (s->usb_v > 0.0f) && !s->otg_enabled;
    s->valid = (s->bat_v >= PM_BAT_V_MIN) && (s->bat_v <= PM_BAT_V_MAX);
    s->bat_p_w = s->valid ? pm_power_w(s->bat_v, s->bat_i_ma) : 0.0f;
}

static bool pm_gauge_read_word(uint8_t cmd, uint16_t* out) {
    /* BQ27220 words are little endian. furi_hal_i2c_read_reg_16 assumes big
     * endian, so use a plain write-address/read transaction like the driver. */
    uint8_t buf[2] = {0};
    if(!furi_hal_i2c_trx(
           &furi_hal_i2c_handle_power, PM_GAUGE_ADDR, &cmd, 1, buf, sizeof(buf), PM_I2C_TIMEOUT)) {
        return false;
    }
    *out = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    return true;
}

static bool pm_hw_read_ext(PmBatteryInfo* info) {
    bool ok = true;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    ok &= pm_gauge_read_word(PM_GAUGE_CMD_CYCLE_COUNT, &info->cycle_count);
    ok &= pm_gauge_read_word(PM_GAUGE_CMD_TIME_TO_EMPTY, &info->tte_min);
    ok &= pm_gauge_read_word(PM_GAUGE_CMD_TIME_TO_FULL, &info->ttf_min);
    ok &= pm_gauge_read_word(PM_GAUGE_CMD_BATTERY_STATUS, &info->bat_status);
    ok &= furi_hal_i2c_read_reg_8(
        &furi_hal_i2c_handle_power,
        PM_CHARGER_ADDR,
        PM_CHARGER_REG_DPM,
        &info->chg_reg13,
        PM_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);

    return ok;
}

void pm_hw_read_slow(PmBatteryInfo* info, bool ext) {
    memset(info, 0, sizeof(*info));

    info->gauge_ok = furi_hal_power_gauge_is_ok();
    info->remaining_mah = (uint16_t)furi_hal_power_get_battery_remaining_capacity();
    info->full_mah = (uint16_t)furi_hal_power_get_battery_full_capacity();
    info->design_mah = (uint16_t)furi_hal_power_get_battery_design_capacity();
    info->soh_pct = furi_hal_power_get_bat_health_pct();
    info->charge_v_limit = furi_hal_power_get_battery_charge_voltage_limit();

    info->tte_min = PM_GAUGE_TIME_NA;
    info->ttf_min = PM_GAUGE_TIME_NA;
    if(ext) {
        info->ext_valid = pm_hw_read_ext(info);
        if(!info->ext_valid) {
            info->tte_min = PM_GAUGE_TIME_NA;
            info->ttf_min = PM_GAUGE_TIME_NA;
        }
    }
}

#include "bms.h"

#include <limits.h>
#include <stddef.h>

static BMS_Tracking_t g_bms_tracking;

static void BMS_ResetTracking(void);
static void BMS_UpdateCells(BMS_Tracking_t *tracking);
static void BMS_UpdateTemperatures(BMS_Tracking_t *tracking);
static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking);

void BMS_Init(void)
{
    BMS_ResetTracking();
    bq76952_init();

    g_bms_tracking.connected = bq76952_isConnected();
    g_bms_tracking.initialized = g_bms_tracking.connected;

    if (!g_bms_tracking.connected) {
        BMS_Error_Handler();
        return;
    }

    BMS_Update();
}

void BMS_Update(void)
{
    bq76952_protection_t protection;
    bq76952_temp_t temperature_status;

    g_bms_tracking.connected = bq76952_isConnected();
    if (!g_bms_tracking.connected) {
        BMS_Error_Handler();
        return;
    }

    BMS_UpdateCells(&g_bms_tracking);
    BMS_UpdateTemperatures(&g_bms_tracking);

    g_bms_tracking.stackVoltage = (uint16_t)bq76952_getStackVoltage();
    g_bms_tracking.current = (int16_t)bq76952_getCurrentNow();
    g_bms_tracking.charging = bq76952_isCharging();
    g_bms_tracking.discharging = bq76952_isDischarging();
    g_bms_tracking.fetsEnabled = bq76952_areFETs_Enabled();

    protection = bq76952_getProtectionStatus();
    temperature_status = bq76952_getTemperatureStatus();

    BMS_UpdateFaultFlags(&g_bms_tracking);

    g_bms_tracking.overVoltage = g_bms_tracking.overVoltage || protection.bits.CELL_OV;
    g_bms_tracking.underVoltage = g_bms_tracking.underVoltage || protection.bits.CELL_UV;
    g_bms_tracking.overCurrent = g_bms_tracking.overCurrent ||
                                 protection.bits.OC_CHG ||
                                 protection.bits.OC1_DCHG ||
                                 protection.bits.OC2_DCHG ||
                                 protection.bits.SC_DCHG;
    g_bms_tracking.overChargeTemperature = g_bms_tracking.overChargeTemperature ||
                                           temperature_status.bits.OVERTEMP_CHG;
    g_bms_tracking.underChargeTemperature = g_bms_tracking.underChargeTemperature ||
                                            temperature_status.bits.UNDERTEMP_CHG;
    g_bms_tracking.overDischargeTemperature = g_bms_tracking.overDischargeTemperature ||
                                              temperature_status.bits.OVERTEMP_DCHG;
    g_bms_tracking.underDischargeTemperature = g_bms_tracking.underDischargeTemperature ||
                                               temperature_status.bits.UNDERTEMP_DCHG;

    if (BMS_IsFaultActive()) {
        bq76952_setFET(ALL, OFF);
        g_bms_tracking.charging = false;
        g_bms_tracking.discharging = false;
    }

    g_bms_tracking.circle_counter++;
    g_bms_tracking.initialized = true;
}

const BMS_Tracking_t *BMS_GetTracking(void)
{
    return &g_bms_tracking;
}

bool BMS_IsFaultActive(void)
{
    return g_bms_tracking.overVoltage ||
           g_bms_tracking.underVoltage ||
           g_bms_tracking.overChargeTemperature ||
           g_bms_tracking.underChargeTemperature ||
           g_bms_tracking.overDischargeTemperature ||
           g_bms_tracking.underDischargeTemperature ||
           g_bms_tracking.overCurrent;
}

void BMS_Error_Handler(void)
{
    g_bms_tracking.connected = false;
    g_bms_tracking.fetsEnabled = false;
    g_bms_tracking.charging = false;
    g_bms_tracking.discharging = false;
    bq76952_setFET(ALL, OFF);
}

static void BMS_ResetTracking(void)
{
    for (uint8_t i = 0U; i < NUMBER_OF_CELLS; ++i) {
        g_bms_tracking.cellVoltages[i] = 0U;
    }

    for (uint8_t i = 0U; i < NUMBER_OF_THERMISTORS; ++i) {
        g_bms_tracking.temperature[i] = 0;
    }

    g_bms_tracking.initialized = false;
    g_bms_tracking.connected = false;
    g_bms_tracking.circle_counter = 0U;
    g_bms_tracking.stackVoltage = 0U;
    g_bms_tracking.minCellVoltage = 0U;
    g_bms_tracking.maxCellVoltage = 0U;
    g_bms_tracking.averageCellVoltage = 0U;
    g_bms_tracking.deltaCellVoltage = 0U;
    g_bms_tracking.current = 0;
    g_bms_tracking.charging = false;
    g_bms_tracking.discharging = false;
    g_bms_tracking.fetsEnabled = false;
    g_bms_tracking.overVoltage = false;
    g_bms_tracking.underVoltage = false;
    g_bms_tracking.overChargeTemperature = false;
    g_bms_tracking.underChargeTemperature = false;
    g_bms_tracking.overDischargeTemperature = false;
    g_bms_tracking.underDischargeTemperature = false;
    g_bms_tracking.overCurrent = false;
    g_bms_tracking.balanceRequired = false;
}

static void BMS_UpdateCells(BMS_Tracking_t *tracking)
{
    int raw_cell_voltage[NUMBER_OF_CELLS];
    uint32_t voltage_sum = 0U;
    uint16_t min_voltage = UINT16_MAX;
    uint16_t max_voltage = 0U;

    if (tracking == NULL) {
        return;
    }

    bq76952_getOnlyConnectedCellVoltages(raw_cell_voltage);

    for (uint8_t i = 0U; i < NUMBER_OF_CELLS; ++i) {
        uint16_t voltage = 0U;

        if (raw_cell_voltage[i] > 0) {
            voltage = (uint16_t)raw_cell_voltage[i];
        }

        tracking->cellVoltages[i] = voltage;
        voltage_sum += voltage;

        if (voltage < min_voltage) {
            min_voltage = voltage;
        }

        if (voltage > max_voltage) {
            max_voltage = voltage;
        }
    }

    tracking->minCellVoltage = (min_voltage == UINT16_MAX) ? 0U : min_voltage;
    tracking->maxCellVoltage = max_voltage;
    tracking->averageCellVoltage = (uint16_t)(voltage_sum / NUMBER_OF_CELLS);
    tracking->deltaCellVoltage = tracking->maxCellVoltage - tracking->minCellVoltage;
    tracking->balanceRequired = tracking->deltaCellVoltage >= DELTA_VOLTAGE_BALANCE_THRESHOLD;
}

static void BMS_UpdateTemperatures(BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }

    tracking->temperature[0] = (int16_t)bq76952_getThermistorTemp(TS1);
    tracking->temperature[1] = (int16_t)bq76952_getThermistorTemp(TS2);
}

static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking)
{
    int32_t abs_current;

    if (tracking == NULL) {
        return;
    }

    tracking->overVoltage = false;
    tracking->underVoltage = false;
    tracking->overChargeTemperature = false;
    tracking->underChargeTemperature = false;
    tracking->overDischargeTemperature = false;
    tracking->underDischargeTemperature = false;
    tracking->overCurrent = false;

    for (uint8_t i = 0U; i < NUMBER_OF_CELLS; ++i) {
        if (tracking->cellVoltages[i] >= OVER_VOLTAGE_THRESHOLD) {
            tracking->overVoltage = true;
        }

        if ((tracking->cellVoltages[i] > 0U) &&
            (tracking->cellVoltages[i] <= UNDER_VOLTAGE_THRESHOLD)) {
            tracking->underVoltage = true;
        }
    }

    for (uint8_t i = 0U; i < NUMBER_OF_THERMISTORS; ++i) {
        if (tracking->temperature[i] >= (int16_t)OVER_CHARG_TEMPERATURE_THRESHOLD) {
            tracking->overChargeTemperature = true;
        }

        if (tracking->temperature[i] <= (int16_t)UNDER_CHARG_TEMPERATURE_THRESHOLD) {
            tracking->underChargeTemperature = true;
        }

        if (tracking->temperature[i] >= (int16_t)OVER_DISCHARG_TEMPERATURE_THRESHOLD) {
            tracking->overDischargeTemperature = true;
        }

        if (tracking->temperature[i] <= (int16_t)UNDER_DISCHARG_TEMPERATURE_THRESHOLD) {
            tracking->underDischargeTemperature = true;
        }
    }

    abs_current = tracking->current;
    if (abs_current < 0) {
        abs_current = -abs_current;
    }

    if ((uint32_t)abs_current >= OVER_CURRENT_THRESHOLD) {
        tracking->overCurrent = true;
    }
}

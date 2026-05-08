#include "bms.h"

#include <limits.h>
#include <stddef.h>

static BMS_Tracking_t g_bms_tracking;
static uint32_t g_last_update_tick;
static uint32_t g_last_balance_tick;

static void BMS_ResetTracking(void);
static void BMS_ConfigureMonitor(void);
static void BMS_ReadMeasurements(BMS_Tracking_t *tracking);
static void BMS_UpdateCellStatistics(BMS_Tracking_t *tracking);
static void BMS_UpdateCurrentDirection(BMS_Tracking_t *tracking);
static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking);
static void BMS_MergeBQFaultFlags(BMS_Tracking_t *tracking);
static void BMS_UpdateState(BMS_Tracking_t *tracking);
static void BMS_ApplyFetPolicy(BMS_Tracking_t *tracking);
static void BMS_UpdateCoulombCounter(BMS_Tracking_t *tracking, uint32_t dt_ms);
static void BMS_UpdateBalancing(BMS_Tracking_t *tracking, uint32_t now);
static bool BMS_AllCellsAtOrBelow(const BMS_Tracking_t *tracking, uint16_t threshold_mV);
static bool BMS_AllCellsAtOrAbove(const BMS_Tracking_t *tracking, uint16_t threshold_mV);
static bool BMS_AllTemperaturesAtOrBelow(const BMS_Tracking_t *tracking, int16_t threshold_C);
static bool BMS_AllTemperaturesAtOrAbove(const BMS_Tracking_t *tracking, int16_t threshold_C);
static int32_t BMS_AbsCurrent(int32_t current_mA);

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

    BMS_ConfigureMonitor();
    g_last_update_tick = HAL_GetTick();
    g_last_balance_tick = g_last_update_tick;

    BMS_Update();
}

void BMS_Update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t dt_ms = now - g_last_update_tick;

    if (dt_ms == 0U) {
        dt_ms = 1U;
    }
    g_last_update_tick = now;

    g_bms_tracking.connected = bq76952_isConnected();
    if (!g_bms_tracking.connected) {
        g_bms_tracking.faults.communicationFault = true;
        BMS_Error_Handler();
        return;
    }

    BMS_ReadMeasurements(&g_bms_tracking);
    BMS_UpdateCellStatistics(&g_bms_tracking);
    BMS_UpdateCurrentDirection(&g_bms_tracking);
    BMS_UpdateFaultFlags(&g_bms_tracking);
    BMS_MergeBQFaultFlags(&g_bms_tracking);
    BMS_UpdateCoulombCounter(&g_bms_tracking, dt_ms);
    BMS_UpdateState(&g_bms_tracking);
    BMS_ApplyFetPolicy(&g_bms_tracking);
    BMS_UpdateBalancing(&g_bms_tracking, now);

    g_bms_tracking.circle_counter++;
    g_bms_tracking.initialized = true;
}

const BMS_Tracking_t *BMS_GetTracking(void)
{
    return &g_bms_tracking;
}

bool BMS_IsFaultActive(void)
{
    return g_bms_tracking.faults.cellOverVoltage ||
           g_bms_tracking.faults.cellUnderVoltage ||
           g_bms_tracking.faults.chargeOverTemperature ||
           g_bms_tracking.faults.dischargeOverTemperature ||
           g_bms_tracking.faults.underTemperature ||
           g_bms_tracking.faults.chargeOverCurrent ||
           g_bms_tracking.faults.dischargeOverCurrent ||
           g_bms_tracking.faults.shortCircuit ||
           g_bms_tracking.faults.bqSafetyFault ||
           g_bms_tracking.faults.communicationFault;
}

void BMS_Error_Handler(void)
{
    g_bms_tracking.connected = false;
    g_bms_tracking.fetsEnabled = false;
    g_bms_tracking.chargeFetEnabled = false;
    g_bms_tracking.dischargeFetEnabled = false;
    g_bms_tracking.charging = false;
    g_bms_tracking.discharging = false;
    g_bms_tracking.chargeDisabled = true;
    g_bms_tracking.dischargeDisabled = true;
    g_bms_tracking.state = BMS_STATE_FAULT;
    g_bms_tracking.balanceMask = 0U;
    g_bms_tracking.balanceRequired = false;
    bq76952_setCellBalanceMask(0U);
    bq76952_setFET(ALL, OFF);
}

static void BMS_ResetTracking(void)
{
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        g_bms_tracking.cellVoltages[i] = 0U;
    }

    for (uint8_t i = 0U; i < BMS_NUMBER_OF_THERMISTORS; ++i) {
        g_bms_tracking.temperature[i] = 0;
    }

    g_bms_tracking.initialized = false;
    g_bms_tracking.connected = false;
    g_bms_tracking.state = BMS_STATE_INIT;
    g_bms_tracking.currentDirection = BMS_CURRENT_IDLE;
    g_bms_tracking.circle_counter = 0U;
    g_bms_tracking.stackVoltage = 0U;
    g_bms_tracking.minCellVoltage = 0U;
    g_bms_tracking.maxCellVoltage = 0U;
    g_bms_tracking.averageCellVoltage = 0U;
    g_bms_tracking.deltaCellVoltage = 0U;
    g_bms_tracking.current_mA = 0;
    g_bms_tracking.charging = false;
    g_bms_tracking.discharging = false;
    g_bms_tracking.chargeFetEnabled = false;
    g_bms_tracking.dischargeFetEnabled = false;
    g_bms_tracking.fetsEnabled = false;
    g_bms_tracking.faults = (BMS_FaultFlags_t){0};
    g_bms_tracking.chargeDisabled = true;
    g_bms_tracking.dischargeDisabled = true;
    g_bms_tracking.balanceRequired = false;
    g_bms_tracking.balanceMask = 0U;
    g_bms_tracking.chargeAccumulated_mAs = 0ULL;
    g_bms_tracking.dischargeAccumulated_mAs = 0ULL;
    g_bms_tracking.chargeThroughput_mAh = 0UL;
    g_bms_tracking.dischargeThroughput_mAh = 0UL;
    g_bms_tracking.equivalentCycle_milliCycles = 0UL;
}

static void BMS_ConfigureMonitor(void)
{
    uint32_t over_current_sense_mV;

    bq76952_setVcellMode(BMS_BQ_VCELL_MODE_10S);
    bq76952_setDA_Config();
    bq76952_setEnableProtectionsA();
    bq76952_setEnableProtectionsB();
    bq76952_setEnableProtectionsC();
    bq76952_setProtectionConfiguration();
    bq76952_setEnableCHG_FET_Protection();
    bq76952_setDSGFETProtectionsA();
    bq76952_setDSGFETProtectionsB();
    bq76952_setDSGFETProtectionsC();
    bq76952_setFET_Options();
    bq76952_setFET_PredischargeTimeout();
    bq76952_setFET_PredischargeStopDelta();
    bq76952_setCellInterconnectResistances();
    bq76952_setEnableTS1();
    bq76952_setEnableTS2();
    bq76952_setCellOvervoltageProtection(BMS_CELL_OV_CUTOFF_MV, BMS_BQ_PROTECTION_DELAY_MS);
    bq76952_setCellUndervoltageProtection(BMS_CELL_UV_CUTOFF_MV, BMS_BQ_PROTECTION_DELAY_MS);
    bq76952_setChargingTemperatureMaxLimit(BMS_CHARGE_OT_CUTOFF_C, 2U);
    bq76952_setDischargingTemperatureMaxLimit(BMS_DISCHARGE_OT_CUTOFF_C, 2U);
    bq76952_setCellBalancingEnabled(true);

    over_current_sense_mV = ((uint32_t)BMS_OVER_CURRENT_MA * BMS_BQ_SENSE_RESISTOR_UOHM) / 1000000UL;
    if (over_current_sense_mV < 4UL) {
        over_current_sense_mV = 4UL;
    }
    bq76952_setChargingOvercurrentProtection((unsigned int)over_current_sense_mV, 50U);
    bq76952_setDischargingOvercurrentProtection((unsigned int)over_current_sense_mV, 50U);
    bq76952_setDischargingShortcircuitProtection(SCD_80, 30U);

    if (!bq76952_areFETs_Enabled()) {
        bq76952_setFET_ENABLE();
    }
    bq76952_setCellBalanceMask(0U);
    bq76952_setFET(ALL, ON);
}

static void BMS_ReadMeasurements(BMS_Tracking_t *tracking)
{
    int raw_cell_voltage[BMS_NUMBER_OF_CELLS];

    if (tracking == NULL) {
        return;
    }

    bq76952_getOnlyConnectedCellVoltages(raw_cell_voltage);
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        if (raw_cell_voltage[i] < 0) {
            tracking->cellVoltages[i] = 0U;
        } else {
            tracking->cellVoltages[i] = (uint16_t)raw_cell_voltage[i];
        }
    }

    tracking->stackVoltage = (uint16_t)bq76952_getStackVoltage();
    tracking->current_mA = (int32_t)bq76952_getCurrentNow();
    tracking->temperature[0] = (int16_t)bq76952_getThermistorTemp(TS1);
    tracking->temperature[1] = (int16_t)bq76952_getThermistorTemp(TS2);
    tracking->charging = bq76952_isCharging();
    tracking->discharging = bq76952_isDischarging();
    tracking->chargeFetEnabled = tracking->charging;
    tracking->dischargeFetEnabled = tracking->discharging;
    tracking->fetsEnabled = bq76952_areFETs_Enabled();
}

static void BMS_UpdateCellStatistics(BMS_Tracking_t *tracking)
{
    uint32_t voltage_sum = 0U;
    uint16_t min_voltage = UINT16_MAX;
    uint16_t max_voltage = 0U;

    if (tracking == NULL) {
        return;
    }

    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        uint16_t voltage = tracking->cellVoltages[i];

        voltage_sum += voltage;
        if ((voltage > 0U) && (voltage < min_voltage)) {
            min_voltage = voltage;
        }
        if (voltage > max_voltage) {
            max_voltage = voltage;
        }
    }

    tracking->minCellVoltage = (min_voltage == UINT16_MAX) ? 0U : min_voltage;
    tracking->maxCellVoltage = max_voltage;
    tracking->averageCellVoltage = (uint16_t)(voltage_sum / BMS_NUMBER_OF_CELLS);
    tracking->deltaCellVoltage = tracking->maxCellVoltage - tracking->minCellVoltage;
}

static void BMS_UpdateCurrentDirection(BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }

#if BMS_CURRENT_CHARGE_IS_POSITIVE
    if (tracking->current_mA > BMS_CURRENT_DEADBAND_MA) {
        tracking->currentDirection = BMS_CURRENT_CHARGE;
    } else if (tracking->current_mA < -BMS_CURRENT_DEADBAND_MA) {
        tracking->currentDirection = BMS_CURRENT_DISCHARGE;
    } else
#else
    if (tracking->current_mA < -BMS_CURRENT_DEADBAND_MA) {
        tracking->currentDirection = BMS_CURRENT_CHARGE;
    } else if (tracking->current_mA > BMS_CURRENT_DEADBAND_MA) {
        tracking->currentDirection = BMS_CURRENT_DISCHARGE;
    } else
#endif
    {
        tracking->currentDirection = BMS_CURRENT_IDLE;
    }
}

static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking)
{
    int32_t abs_current;

    if (tracking == NULL) {
        return;
    }

    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        if (tracking->cellVoltages[i] >= BMS_CELL_OV_CUTOFF_MV) {
            tracking->faults.cellOverVoltage = true;
        }
        if ((tracking->cellVoltages[i] > 0U) &&
            (tracking->cellVoltages[i] <= BMS_CELL_UV_CUTOFF_MV)) {
            tracking->faults.cellUnderVoltage = true;
        }
    }

    if (tracking->faults.cellOverVoltage &&
        BMS_AllCellsAtOrBelow(tracking, BMS_CELL_OV_RECOVER_MV)) {
        tracking->faults.cellOverVoltage = false;
    }
    if (tracking->faults.cellUnderVoltage &&
        BMS_AllCellsAtOrAbove(tracking, BMS_CELL_UV_RECOVER_MV)) {
        tracking->faults.cellUnderVoltage = false;
    }

    for (uint8_t i = 0U; i < BMS_NUMBER_OF_THERMISTORS; ++i) {
        if (tracking->temperature[i] >= BMS_CHARGE_OT_CUTOFF_C) {
            tracking->faults.chargeOverTemperature = true;
        }
        if (tracking->temperature[i] >= BMS_DISCHARGE_OT_CUTOFF_C) {
            tracking->faults.dischargeOverTemperature = true;
        }
        if (tracking->temperature[i] <= BMS_UNDERTEMP_CUTOFF_C) {
            tracking->faults.underTemperature = true;
        }
    }

    if (tracking->faults.chargeOverTemperature &&
        BMS_AllTemperaturesAtOrBelow(tracking, BMS_CHARGE_OT_RECOVER_C)) {
        tracking->faults.chargeOverTemperature = false;
    }
    if (tracking->faults.dischargeOverTemperature &&
        BMS_AllTemperaturesAtOrBelow(tracking, BMS_DISCHARGE_OT_RECOVER_C)) {
        tracking->faults.dischargeOverTemperature = false;
    }
    if (tracking->faults.underTemperature &&
        BMS_AllTemperaturesAtOrAbove(tracking, BMS_UNDERTEMP_RECOVER_C)) {
        tracking->faults.underTemperature = false;
    }

    abs_current = BMS_AbsCurrent(tracking->current_mA);
    if (abs_current >= BMS_SHORT_CIRCUIT_MA) {
        tracking->faults.shortCircuit = true;
    }
    if (abs_current >= BMS_OVER_CURRENT_MA) {
        if (tracking->currentDirection == BMS_CURRENT_CHARGE) {
            tracking->faults.chargeOverCurrent = true;
        } else if (tracking->currentDirection == BMS_CURRENT_DISCHARGE) {
            tracking->faults.dischargeOverCurrent = true;
        }
    } else if (abs_current <= BMS_CURRENT_DEADBAND_MA) {
        tracking->faults.chargeOverCurrent = false;
        tracking->faults.dischargeOverCurrent = false;
    }
}

static void BMS_MergeBQFaultFlags(BMS_Tracking_t *tracking)
{
    bq76952_protection_t protection;
    bq76952_temp_t temperature_status;

    if (tracking == NULL) {
        return;
    }

    protection = bq76952_getProtectionStatus();
    temperature_status = bq76952_getTemperatureStatus();

    tracking->faults.cellOverVoltage = tracking->faults.cellOverVoltage || protection.bits.CELL_OV;
    tracking->faults.cellUnderVoltage = tracking->faults.cellUnderVoltage || protection.bits.CELL_UV;
    tracking->faults.chargeOverCurrent = tracking->faults.chargeOverCurrent || protection.bits.OC_CHG;
    tracking->faults.dischargeOverCurrent = tracking->faults.dischargeOverCurrent ||
                                            protection.bits.OC1_DCHG ||
                                            protection.bits.OC2_DCHG;
    tracking->faults.shortCircuit = tracking->faults.shortCircuit || protection.bits.SC_DCHG;
    tracking->faults.chargeOverTemperature = tracking->faults.chargeOverTemperature ||
                                             temperature_status.bits.OVERTEMP_CHG;
    tracking->faults.dischargeOverTemperature = tracking->faults.dischargeOverTemperature ||
                                                temperature_status.bits.OVERTEMP_DCHG;
    tracking->faults.underTemperature = tracking->faults.underTemperature ||
                                        temperature_status.bits.UNDERTEMP_CHG ||
                                        temperature_status.bits.UNDERTEMP_DCHG;
    tracking->faults.bqSafetyFault = tracking->faults.shortCircuit;
}

static void BMS_UpdateState(BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }

    tracking->chargeDisabled = tracking->faults.cellOverVoltage ||
                               tracking->faults.chargeOverTemperature ||
                               tracking->faults.underTemperature ||
                               tracking->faults.chargeOverCurrent ||
                               tracking->faults.shortCircuit ||
                               tracking->faults.bqSafetyFault ||
                               tracking->faults.communicationFault;

    tracking->dischargeDisabled = tracking->faults.cellUnderVoltage ||
                                  tracking->faults.dischargeOverTemperature ||
                                  tracking->faults.underTemperature ||
                                  tracking->faults.dischargeOverCurrent ||
                                  tracking->faults.shortCircuit ||
                                  tracking->faults.bqSafetyFault ||
                                  tracking->faults.communicationFault;

    if (tracking->faults.shortCircuit || tracking->faults.communicationFault) {
        tracking->state = BMS_STATE_FAULT;
    } else if (tracking->chargeDisabled && tracking->dischargeDisabled) {
        tracking->state = BMS_STATE_FAULT;
    } else if (tracking->chargeDisabled) {
        tracking->state = BMS_STATE_CHARGE_PROTECT;
    } else if (tracking->dischargeDisabled) {
        tracking->state = BMS_STATE_DISCHARGE_PROTECT;
    } else {
        tracking->state = BMS_STATE_NORMAL;
    }
}

static void BMS_ApplyFetPolicy(BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }

    if (tracking->chargeDisabled && tracking->dischargeDisabled) {
        bq76952_setFET(ALL, OFF);
        tracking->chargeFetEnabled = false;
        tracking->dischargeFetEnabled = false;
        return;
    }

    if (tracking->chargeDisabled) {
        bq76952_setFET(CHG, OFF);
        tracking->chargeFetEnabled = false;
        return;
    }

    if (tracking->dischargeDisabled) {
        bq76952_setFET(DCH, OFF);
        tracking->dischargeFetEnabled = false;
        return;
    }

    bq76952_setFET(ALL, ON);
    tracking->chargeFetEnabled = true;
    tracking->dischargeFetEnabled = true;
}

static void BMS_UpdateCoulombCounter(BMS_Tracking_t *tracking, uint32_t dt_ms)
{
    uint64_t sample_mAs;
    int32_t abs_current;

    if (tracking == NULL) {
        return;
    }

    abs_current = BMS_AbsCurrent(tracking->current_mA);
    if (abs_current <= BMS_CURRENT_DEADBAND_MA) {
        return;
    }

    sample_mAs = ((uint64_t)abs_current * (uint64_t)dt_ms) / 1000ULL;
    if (tracking->currentDirection == BMS_CURRENT_CHARGE) {
        tracking->chargeAccumulated_mAs += sample_mAs;
    } else if (tracking->currentDirection == BMS_CURRENT_DISCHARGE) {
        tracking->dischargeAccumulated_mAs += sample_mAs;
    }

    tracking->chargeThroughput_mAh = (uint32_t)(tracking->chargeAccumulated_mAs / 3600ULL);
    tracking->dischargeThroughput_mAh = (uint32_t)(tracking->dischargeAccumulated_mAs / 3600ULL);
    tracking->equivalentCycle_milliCycles =
        (uint32_t)((tracking->chargeThroughput_mAh * 1000ULL) / BMS_NOMINAL_CAPACITY_MAH);
}

static void BMS_UpdateBalancing(BMS_Tracking_t *tracking, uint32_t now)
{
    uint16_t requested_mask = 0U;
    bool previous_selected = false;

    if (tracking == NULL) {
        return;
    }

    tracking->balanceRequired = tracking->deltaCellVoltage >= BMS_BALANCE_DELTA_MV;

    if ((now - g_last_balance_tick) < BMS_BALANCE_REFRESH_MS) {
        return;
    }
    g_last_balance_tick = now;

    if (tracking->state != BMS_STATE_NORMAL ||
        !tracking->balanceRequired ||
        tracking->currentDirection == BMS_CURRENT_DISCHARGE) {
        tracking->balanceMask = 0U;
        bq76952_setCellBalanceMask(0U);
        return;
    }

    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        bool select_cell = false;

        if (tracking->cellVoltages[i] < BMS_BALANCE_MIN_CELL_MV) {
            previous_selected = false;
            continue;
        }

        if ((tracking->cellVoltages[i] + BMS_BALANCE_DELTA_MV) <= tracking->maxCellVoltage &&
            tracking->cellVoltages[i] < tracking->maxCellVoltage) {
            previous_selected = false;
            continue;
        }

        if (tracking->cellVoltages[i] >= (uint16_t)(tracking->minCellVoltage + BMS_BALANCE_DELTA_MV)) {
            select_cell = true;
        }

        if (select_cell && !previous_selected) {
            requested_mask |= (uint16_t)(1U << i);
            previous_selected = true;
        } else {
            previous_selected = false;
        }
    }

    tracking->balanceMask = requested_mask;
    bq76952_setCellBalanceMask(requested_mask);
}

static bool BMS_AllCellsAtOrBelow(const BMS_Tracking_t *tracking, uint16_t threshold_mV)
{
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        if (tracking->cellVoltages[i] > threshold_mV) {
            return false;
        }
    }
    return true;
}

static bool BMS_AllCellsAtOrAbove(const BMS_Tracking_t *tracking, uint16_t threshold_mV)
{
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_CELLS; ++i) {
        if ((tracking->cellVoltages[i] > 0U) && (tracking->cellVoltages[i] < threshold_mV)) {
            return false;
        }
    }
    return true;
}

static bool BMS_AllTemperaturesAtOrBelow(const BMS_Tracking_t *tracking, int16_t threshold_C)
{
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_THERMISTORS; ++i) {
        if (tracking->temperature[i] > threshold_C) {
            return false;
        }
    }
    return true;
}

static bool BMS_AllTemperaturesAtOrAbove(const BMS_Tracking_t *tracking, int16_t threshold_C)
{
    for (uint8_t i = 0U; i < BMS_NUMBER_OF_THERMISTORS; ++i) {
        if (tracking->temperature[i] < threshold_C) {
            return false;
        }
    }
    return true;
}

static int32_t BMS_AbsCurrent(int32_t current_mA)
{
    if (current_mA < 0) {
        return -current_mA;
    }
    return current_mA;
}

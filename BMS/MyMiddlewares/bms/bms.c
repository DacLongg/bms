#include "bms.h"
#include "main.h"
#include "adc.h"
#include "MyDrivers/power/power_manager.h"

#include <limits.h>
#include <stddef.h>

#include "debug_log.h"
#include "storage_flash.h"

#define BMS_ALERT_POLL_FALLBACK_MS 1000U
#define BMS_SHUT_HOLD_MS 1100U
#define BMS_BAT_ADC_SAMPLE_MS 1000U
#define BMS_BAT_ADC_SETTLE_MS 5U
#define BMS_BAT_ADC_REF_MV 3300UL
#define BMS_BAT_ADC_COUNTS 4095UL
#define BMS_BAT_ADC_DIVIDER_NUM 678300UL
#define BMS_BAT_ADC_DIVIDER_DEN 13300UL

static BMS_Tracking_t g_bms_tracking;
static uint32_t g_last_update_tick;
static uint32_t g_last_balance_tick;
static uint32_t g_last_flash_save_tick;
static uint32_t g_last_saved_charge_mAh;
static uint32_t g_last_saved_discharge_mAh;
static BMS_State_t g_last_logged_state = BMS_STATE_INIT;
static uint16_t g_last_logged_balance_mask;
static volatile bool g_alert_irq_pending;
static volatile uint32_t g_alert_irq_counter;
static volatile bool g_dchg_signal_active;
static volatile bool g_ddsg_signal_active;
static uint32_t g_last_alert_service_tick;
static uint32_t g_last_bat_adc_sample_tick;
static bool g_shutdown_pulse_active;
static uint32_t g_shutdown_pulse_tick;

static void BMS_ResetTracking(void);
static void BMS_ConfigureMonitor(void);
static void BMS_ConfigureHardwarePins(void);
static void BMS_ReadMeasurements(BMS_Tracking_t *tracking);
static void BMS_HandleHardwareSignals(BMS_Tracking_t *tracking, uint32_t now);
static void BMS_UpdateBatteryAdc(BMS_Tracking_t *tracking, uint32_t now);
static void BMS_UpdateShutdownPulse(uint32_t now);
static void BMS_SetFetoff(bool asserted);
static void BMS_SetBatSenseEnable(bool enabled);
static void BMS_UpdateCellStatistics(BMS_Tracking_t *tracking);
static void BMS_UpdateCurrentDirection(BMS_Tracking_t *tracking);
static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking);
static void BMS_MergeBQFaultFlags(BMS_Tracking_t *tracking);
static void BMS_UpdateState(BMS_Tracking_t *tracking);
static void BMS_ApplyFetPolicy(BMS_Tracking_t *tracking);
static void BMS_UpdateCoulombCounter(BMS_Tracking_t *tracking, uint32_t dt_ms);
static void BMS_UpdateBalancing(BMS_Tracking_t *tracking, uint32_t now);
static void BMS_LoadPersistedData(BMS_Tracking_t *tracking);
static void BMS_SavePersistedDataIfNeeded(const BMS_Tracking_t *tracking, uint32_t now);
#if BMS_DEBUG_LOG_ENABLE
const char *BMS_StateName(BMS_State_t state);
#endif
static bool BMS_AllCellsAtOrBelow(const BMS_Tracking_t *tracking, uint16_t threshold_mV);
static bool BMS_AllCellsAtOrAbove(const BMS_Tracking_t *tracking, uint16_t threshold_mV);
static bool BMS_AllTemperaturesAtOrBelow(const BMS_Tracking_t *tracking, int16_t threshold_C);
static bool BMS_AllTemperaturesAtOrAbove(const BMS_Tracking_t *tracking, int16_t threshold_C);
static int32_t BMS_AbsCurrent(int32_t current_mA);

void BMS_Init(void)
{
    BMS_ResetTracking();
    BMS_ConfigureHardwarePins();
    BMS_LOG_INFO("bms init");
    bq76952_init();

    g_bms_tracking.connected = bq76952_isConnected();
    g_bms_tracking.initialized = g_bms_tracking.connected;

    if (!g_bms_tracking.connected) {
        BMS_LOG_ERROR("bq76952 not connected");
        BMS_Error_Handler();
        return;
    }

    BMS_LoadPersistedData(&g_bms_tracking);
    BMS_ConfigureMonitor();
    g_last_update_tick = HAL_GetTick();
    g_last_balance_tick = g_last_update_tick;
    g_last_flash_save_tick = g_last_update_tick;
    g_last_alert_service_tick = g_last_update_tick;
    g_last_bat_adc_sample_tick = g_last_update_tick;

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
    BMS_UpdateShutdownPulse(now);

    g_bms_tracking.connected = bq76952_isConnected();
    if (!g_bms_tracking.connected) {
        g_bms_tracking.faults.communicationFault = true;
        BMS_LOG_ERROR("bq76952 communication fault");
        BMS_Error_Handler();
        return;
    }

    BMS_ReadMeasurements(&g_bms_tracking);
    BMS_HandleHardwareSignals(&g_bms_tracking, now);
    BMS_UpdateBatteryAdc(&g_bms_tracking, now);
    BMS_UpdateCellStatistics(&g_bms_tracking); // Cập nhật min/max/average/delta cell voltage và stack voltage
    BMS_UpdateCurrentDirection(&g_bms_tracking); // Cập nhật chiều dòng điện dựa trên giá trị current_mA
    BMS_UpdateFaultFlags(&g_bms_tracking); // Cập nhật các cờ lỗi dựa trên ngưỡng điện áp, nhiệt độ, dòng điện
    BMS_MergeBQFaultFlags(&g_bms_tracking); // Kết hợp các cờ lỗi từ BQ76952 vào tracking
    BMS_UpdateCoulombCounter(&g_bms_tracking, dt_ms); // Cập nhật tích trữ mAs và throughput mAh dựa trên current và dt
    BMS_UpdateState(&g_bms_tracking); // Cập nhật trạng thái BMS dựa trên các cờ lỗi và điều kiện hoạt động
    BMS_ApplyFetPolicy(&g_bms_tracking); // Điều khiển FET sạc/xả dựa trên trạng thái và cờ lỗi
    BMS_UpdateBalancing(&g_bms_tracking, now); // Cập nhật trạng thái cân bằng cell và mask dựa trên delta cell voltage và ngưỡng
    BMS_SavePersistedDataIfNeeded(&g_bms_tracking, now);

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
    BMS_LOG_ERROR("bms error handler");
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
    g_bms_tracking.fetOffAsserted = true;
    g_bms_tracking.batSenseEnabled = false;
    bq76952_setCellBalanceMask(0U);
    bq76952_setFET(ALL, OFF);
    BMS_SetFetoff(true);
    BMS_SetBatSenseEnable(false);
}

void BMS_NotifyAlertInterrupt(void)
{
    g_alert_irq_pending = true;
    g_alert_irq_counter++;
}

void BMS_RequestShutdown(void)
{
    if (g_shutdown_pulse_active) {
        return;
    }
    HAL_GPIO_WritePin(SHUT_GPIO_Port, SHUT_Pin, GPIO_PIN_SET);
    g_shutdown_pulse_tick = HAL_GetTick();
    g_shutdown_pulse_active = true;
    BMS_LOG_WARN("shutdown pulse start");
}

static void BMS_ResetTracking(void)
{
    /* Reset all cell voltages */
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
    g_bms_tracking.packVoltage = 0U;
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
    g_bms_tracking.chargeGateFaultSignal = false;
    g_bms_tracking.dischargeGateFaultSignal = false;
    g_bms_tracking.fetOffAsserted = false;
    g_bms_tracking.alertActive = false;
    g_bms_tracking.alertCounter = 0UL;
    g_bms_tracking.bqSleepMode = false;
    g_bms_tracking.bqSleepAllowed = false;
    g_bms_tracking.batSenseEnabled = false;
    g_bms_tracking.batAdcRaw = 0U;
    g_bms_tracking.batAdcPin_mV = 0U;
    g_bms_tracking.batAdcEstimatedPack_mV = 0U;
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

    BMS_LOG_INFO("configure bq76952");
    bq76952_configurePowerOutputs();
    bq76952_setVcellMode(BMS_BQ_VCELL_MODE_10S);
    bq76952_setDA_Config();
    bq76952_setCurrentSenseCalibration();
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
    bq76952_setEnableTS3();
#if BMS_DEBUG_LOG_ENABLE
    {
        bq76952_write_verify_t verify;
        uint16_t ts1_cfg = (uint16_t)bq76952_readDataMemory(TS1_CONFIG, 1);
        uint16_t ts3_cfg = (uint16_t)bq76952_readDataMemory(TS3_CONFIG, 1);

        if (bq76952_getLastWriteVerify(&verify)) {
            if (verify.verified) {
                BMS_LOG_INFO("bq write ok addr=0x%04lx val=0x%04x",
                             (unsigned long)verify.addr,
                             verify.expected);
            } else {
                BMS_LOG_ERROR("bq write fail addr=0x%04lx exp=0x%04x got=0x%04x i2c=%u cfg=%u rd=%u",
                              (unsigned long)verify.addr,
                              verify.expected,
                              verify.actual,
                              verify.i2cOk ? 1U : 0U,
                              verify.configUpdateOk ? 1U : 0U,
                              verify.readbackOk ? 1U : 0U);
            }
        }
        BMS_LOG_INFO("bq ts cfg ts1=0x%02x ts3=0x%02x", ts1_cfg, ts3_cfg);
    }
#endif
    bq76952_setAlertPinConfig();
    bq76952_setDFETOFFPinConfig(true, false);
    bq76952_setDCHGPinConfig(false);
    bq76952_setDDSGPinConfig(false);
    bq76952_setDefaultAlarmMaskConfig();
    bq76952_setSF_AlertMask_A();
    bq76952_setSF_AlertMask_B();
    bq76952_setSF_AlertMask_C();
    bq76952_setCellOvervoltageProtection(BMS_CELL_OV_CUTOFF_MV, BMS_BQ_PROTECTION_DELAY_MS);
    bq76952_setCellUndervoltageProtection(BMS_CELL_UV_CUTOFF_MV, BMS_BQ_PROTECTION_DELAY_MS);
    bq76952_setChargingTemperatureMaxLimit(BMS_CHARGE_OT_CUTOFF_C,
                                           BMS_BQ_TEMPERATURE_PROTECTION_DELAY_SEC);
    bq76952_setDischargingTemperatureMaxLimit(BMS_DISCHARGE_OT_CUTOFF_C,
                                              BMS_BQ_TEMPERATURE_PROTECTION_DELAY_SEC);
    bq76952_setChargingTemperatureMinLimit(BMS_UNDERTEMP_CUTOFF_C,
                                           BMS_UNDERTEMP_RECOVER_C,
                                           BMS_BQ_TEMPERATURE_PROTECTION_DELAY_SEC);
    bq76952_setDischargingTemperatureMinLimit(BMS_UNDERTEMP_CUTOFF_C,
                                              BMS_UNDERTEMP_RECOVER_C,
                                              BMS_BQ_TEMPERATURE_PROTECTION_DELAY_SEC);
    bq76952_setCellBalancingEnabled(true);

    over_current_sense_mV = (((uint32_t)BMS_OVER_CURRENT_MA * BMS_BQ_SENSE_RESISTOR_UOHM) + 500000UL) / 1000000UL;
    if (over_current_sense_mV < 4UL) {
        over_current_sense_mV = 4UL;
    }
    bq76952_setChargingOvercurrentProtection((unsigned int)over_current_sense_mV, 50U);
    bq76952_setDischargingOvercurrentProtection((unsigned int)over_current_sense_mV, 50U);
    bq76952_setDischargingShortcircuitProtection(SCD_60, 30U);

    if (!bq76952_areFETs_Enabled()) {
        bq76952_setFET_ENABLE();
    }
    bq76952_setCellBalanceMask(0U);
    bq76952_setFET(ALL, ON);
    BMS_LOG_INFO("bq configured oc=%lu mV", (unsigned long)over_current_sense_mV);
}

static void BMS_ConfigureHardwarePins(void)
{
    HAL_GPIO_WritePin(FETOFF_GPIO_Port, FETOFF_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SHUT_GPIO_Port, SHUT_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BATS_EN_GPIO_Port, BATS_EN_Pin, GPIO_PIN_RESET);
    g_alert_irq_pending = false;
    g_alert_irq_counter = 0UL;
    g_dchg_signal_active = (HAL_GPIO_ReadPin(DCHG_GPIO_Port, DCHG_Pin) == GPIO_PIN_SET);
    g_ddsg_signal_active = (HAL_GPIO_ReadPin(DDSG_GPIO_Port, DDSG_Pin) == GPIO_PIN_SET);
    g_shutdown_pulse_active = false;
    g_shutdown_pulse_tick = 0UL;
}

static void BMS_UpdateShutdownPulse(uint32_t now)
{
    if (!g_shutdown_pulse_active) {
        return;
    }
    if ((now - g_shutdown_pulse_tick) < BMS_SHUT_HOLD_MS) {
        return;
    }
    HAL_GPIO_WritePin(SHUT_GPIO_Port, SHUT_Pin, GPIO_PIN_RESET);
    g_shutdown_pulse_active = false;
    BMS_LOG_WARN("shutdown pulse done");
}

static void BMS_SetFetoff(bool asserted)
{
    HAL_GPIO_WritePin(FETOFF_GPIO_Port, FETOFF_Pin, asserted ? GPIO_PIN_SET : GPIO_PIN_RESET);
    g_bms_tracking.fetOffAsserted = asserted;
}

static void BMS_SetBatSenseEnable(bool enabled)
{
    HAL_GPIO_WritePin(BATS_EN_GPIO_Port, BATS_EN_Pin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
    g_bms_tracking.batSenseEnabled = enabled;
}

static void BMS_HandleHardwareSignals(BMS_Tracking_t *tracking, uint32_t now)
{
    bool should_service_alert;
    bool alert_signal_active;

    if (tracking == NULL) {
        return;
    }

    g_dchg_signal_active = (HAL_GPIO_ReadPin(DCHG_GPIO_Port, DCHG_Pin) == GPIO_PIN_SET);
    g_ddsg_signal_active = (HAL_GPIO_ReadPin(DDSG_GPIO_Port, DDSG_Pin) == GPIO_PIN_SET);
    alert_signal_active = (HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin) == GPIO_PIN_SET);

    /* DCHG/DDSG high means the corresponding BQ FET output is disabled.
     * It is a status/wakeup signal, not a standalone pack fault.
     */
    tracking->chargeGateFaultSignal = g_dchg_signal_active;
    tracking->dischargeGateFaultSignal = g_ddsg_signal_active;

    should_service_alert = g_alert_irq_pending ||
                           alert_signal_active ||
                           ((now - g_last_alert_service_tick) >= BMS_ALERT_POLL_FALLBACK_MS);
    if (!should_service_alert) {
        tracking->alertActive = false;
        return;
    }

    g_alert_irq_pending = false;
    g_last_alert_service_tick = now;
    tracking->alertCounter = g_alert_irq_counter;
    tracking->alertActive = alert_signal_active || (bq76952_getAlertStatusRegister() != 0U);
    (void)bq76952_getAlertRawStatusRegister();
}

static void BMS_UpdateBatteryAdc(BMS_Tracking_t *tracking, uint32_t now)
{
    uint32_t pin_mv;
    uint32_t pack_mv;

    if (tracking == NULL) {
        return;
    }
    if ((now - g_last_bat_adc_sample_tick) < BMS_BAT_ADC_SAMPLE_MS) {
        return;
    }
    g_last_bat_adc_sample_tick = now;

    BMS_SetBatSenseEnable(true);
    HAL_Delay(BMS_BAT_ADC_SETTLE_MS);

    if (HAL_ADC_Start(&hadc) != HAL_OK) {
        BMS_SetBatSenseEnable(false);
        return;
    }
    if (HAL_ADC_PollForConversion(&hadc, 5U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc);
        BMS_SetBatSenseEnable(false);
        return;
    }

    tracking->batAdcRaw = (uint16_t)HAL_ADC_GetValue(&hadc);
    (void)HAL_ADC_Stop(&hadc);
    BMS_SetBatSenseEnable(false);

    pin_mv = ((uint32_t)tracking->batAdcRaw * BMS_BAT_ADC_REF_MV) / BMS_BAT_ADC_COUNTS;
    pack_mv = (pin_mv * BMS_BAT_ADC_DIVIDER_NUM) / BMS_BAT_ADC_DIVIDER_DEN;

    tracking->batAdcPin_mV = (uint16_t)pin_mv;
    tracking->batAdcEstimatedPack_mV = (uint16_t)pack_mv;
}

static void BMS_ReadMeasurements(BMS_Tracking_t *tracking)
{
    // int raw_cell_voltage[BMS_NUMBER_OF_CELLS];
    bq76952_battery_status_t batt_status;

    if (tracking == NULL) {
        return;
    }

    bq76952_getOnlyConnectedCellVoltages(tracking->cellVoltages);


    tracking->stackVoltage = (uint16_t)bq76952_getStackVoltage();
    tracking->packVoltage = (uint16_t)bq76952_getPackVoltage();
    tracking->current_mA = (int32_t)bq76952_getCurrentAvg();
    tracking->temperature[0] = (int16_t)bq76952_getThermistorTemp(TS1);
    tracking->temperature[1] = (int16_t)bq76952_getThermistorTemp(TS3);
    tracking->charging = bq76952_isCharging();
    tracking->discharging = bq76952_isDischarging();
    tracking->chargeFetEnabled = tracking->charging;
    tracking->dischargeFetEnabled = tracking->discharging;
    tracking->fetsEnabled = bq76952_areFETs_Enabled();
    batt_status = bq76952_getBatteryStatusRegister();
    tracking->bqSleepMode = batt_status.bits.SLEEP_MODE != 0U;
    tracking->bqSleepAllowed = batt_status.bits.SLEEP_EN_ALLOWED != 0U;
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

    if (tracking->state != g_last_logged_state) {
        BMS_LOG_WARN("state %s -> %s chg_dis=%u dch_dis=%u faults=0x%02lx",
                     BMS_StateName(g_last_logged_state),
                     BMS_StateName(tracking->state),
                     tracking->chargeDisabled ? 1U : 0U,
                     tracking->dischargeDisabled ? 1U : 0U,
                     (unsigned long)(
                         (tracking->faults.cellOverVoltage ? 0x001U : 0U) |
                         (tracking->faults.cellUnderVoltage ? 0x002U : 0U) |
                         (tracking->faults.chargeOverTemperature ? 0x004U : 0U) |
                         (tracking->faults.dischargeOverTemperature ? 0x008U : 0U) |
                         (tracking->faults.underTemperature ? 0x010U : 0U) |
                         (tracking->faults.chargeOverCurrent ? 0x020U : 0U) |
                         (tracking->faults.dischargeOverCurrent ? 0x040U : 0U) |
                         (tracking->faults.shortCircuit ? 0x080U : 0U) |
                         (tracking->faults.bqSafetyFault ? 0x100U : 0U) |
                         (tracking->faults.communicationFault ? 0x200U : 0U)));
        g_last_logged_state = tracking->state;
    }
}

static void BMS_ApplyFetPolicy(BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }

    if (tracking->chargeDisabled && tracking->dischargeDisabled) {
        BMS_SetFetoff(true);
        bq76952_setFET(ALL, OFF);
        tracking->chargeFetEnabled = false;
        tracking->dischargeFetEnabled = false;
        return;
    }

    if (tracking->chargeDisabled) {
        BMS_SetFetoff(false);
        bq76952_setFET(CHG, OFF);
        tracking->chargeFetEnabled = false;
        return;
    }

    if (tracking->dischargeDisabled) {
        BMS_SetFetoff(false);
        bq76952_setFET(DCH, OFF);
        tracking->dischargeFetEnabled = false;
        return;
    }

    BMS_SetFetoff(false);
    bq76952_setFET(ALL, ON);
    tracking->chargeFetEnabled = true;
    tracking->dischargeFetEnabled = true;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ALERT_Pin) {
        BMS_NotifyAlertInterrupt();
        power_manager_notify_gpio_wakeup();
    } else if (GPIO_Pin == DCHG_Pin) {
        g_dchg_signal_active = (HAL_GPIO_ReadPin(DCHG_GPIO_Port, DCHG_Pin) == GPIO_PIN_SET);
        power_manager_notify_gpio_wakeup();
    } else if (GPIO_Pin == DDSG_Pin) {
        g_ddsg_signal_active = (HAL_GPIO_ReadPin(DDSG_GPIO_Port, DDSG_Pin) == GPIO_PIN_SET);
        power_manager_notify_gpio_wakeup();
    }
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
        if (g_last_logged_balance_mask != 0U) {
            BMS_LOG_INFO("balance off");
            g_last_logged_balance_mask = 0U;
        }
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
    if (requested_mask != g_last_logged_balance_mask) {
        BMS_LOG_INFO("balance mask=0x%04x delta=%u", requested_mask, tracking->deltaCellVoltage);
        g_last_logged_balance_mask = requested_mask;
    }
}

static void BMS_LoadPersistedData(BMS_Tracking_t *tracking)
{
    storage_flash_record_t record;

    if (tracking == NULL) {
        return;
    }
    /* Load persisted data from flash */
    if (!storage_flash_load(&record)) {
        BMS_LOG_WARN("flash record invalid, use defaults");
        storage_flash_make_default(&record);
    }

    tracking->chargeThroughput_mAh = record.chargeThroughput_mAh;   // Note: discharge throughput and equivalent cycle may be inconsistent with charge throughput, but it's acceptable for estimation purpose
    tracking->dischargeThroughput_mAh = record.dischargeThroughput_mAh;
    tracking->equivalentCycle_milliCycles = record.equivalentCycle_milliCycles; // chu kì sạc xả
    tracking->chargeAccumulated_mAs = (uint64_t)record.chargeThroughput_mAh * 3600ULL; // tích trữ mAs dựa trên charge throughput đã lưu, vì discharge throughput có thể không chính xác nếu có lỗi ghi flash trước đó
    tracking->dischargeAccumulated_mAs = (uint64_t)record.dischargeThroughput_mAh * 3600ULL;
    g_last_saved_charge_mAh = tracking->chargeThroughput_mAh;
    g_last_saved_discharge_mAh = tracking->dischargeThroughput_mAh;

    BMS_LOG_INFO("flash load chg=%lu dch=%lu cyc=%lu",
                 (unsigned long)tracking->chargeThroughput_mAh,
                 (unsigned long)tracking->dischargeThroughput_mAh,
                 (unsigned long)tracking->equivalentCycle_milliCycles);
}

static void BMS_SavePersistedDataIfNeeded(const BMS_Tracking_t *tracking, uint32_t now)
{
    storage_flash_record_t record;
    storage_flash_record_t old_record;
    uint32_t charge_delta;
    uint32_t discharge_delta;

    if (tracking == NULL) {
        return;
    }

    if ((now - g_last_flash_save_tick) < BMS_FLASH_SAVE_INTERVAL_MS) {
        return;
    }

    charge_delta = (tracking->chargeThroughput_mAh >= g_last_saved_charge_mAh) ?
                   (tracking->chargeThroughput_mAh - g_last_saved_charge_mAh) : 0U;
    discharge_delta = (tracking->dischargeThroughput_mAh >= g_last_saved_discharge_mAh) ?
                      (tracking->dischargeThroughput_mAh - g_last_saved_discharge_mAh) : 0U;
    if ((charge_delta < BMS_FLASH_SAVE_DELTA_MAH) &&
        (discharge_delta < BMS_FLASH_SAVE_DELTA_MAH)) {
        g_last_flash_save_tick = now;
        return;
    }

    storage_flash_make_default(&record);
    if (storage_flash_load(&old_record)) {
        record.writeCounter = old_record.writeCounter + 1U;
    } else {
        record.writeCounter = 1U;
    }
    record.chargeThroughput_mAh = tracking->chargeThroughput_mAh;
    record.dischargeThroughput_mAh = tracking->dischargeThroughput_mAh;
    record.equivalentCycle_milliCycles = tracking->equivalentCycle_milliCycles;
    record.nominalCapacity_mAh = BMS_NOMINAL_CAPACITY_MAH;

    if (storage_flash_save(&record)) {
        g_last_flash_save_tick = now;
        g_last_saved_charge_mAh = tracking->chargeThroughput_mAh;
        g_last_saved_discharge_mAh = tracking->dischargeThroughput_mAh;
        BMS_LOG_INFO("flash save chg=%lu dch=%lu cyc=%lu",
                     (unsigned long)record.chargeThroughput_mAh,
                     (unsigned long)record.dischargeThroughput_mAh,
                     (unsigned long)record.equivalentCycle_milliCycles);
    } else {
        BMS_LOG_ERROR("flash save failed");
    }
}

#if BMS_DEBUG_LOG_ENABLE
const char *BMS_StateName(BMS_State_t state)
{
    switch (state) {
    case BMS_STATE_INIT:
        return "INIT";
    case BMS_STATE_NORMAL:
        return "NORMAL";
    case BMS_STATE_CHARGE_PROTECT:
        return "CHG_PROTECT";
    case BMS_STATE_DISCHARGE_PROTECT:
        return "DCH_PROTECT";
    case BMS_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}
#endif

/* Helper functions to check if all cells  are above/below certain thresholds for fault recovery conditions */
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

void BMS_Set_5V_Output(bool enabled)
{
    if (enabled) {
        bq76952_setEnableRegulator(true, true); // enable 5V regulator in auto mode (enabled when either charge or discharge FET is on)
    } else {
        bq76952_setEnableRegulator(false, false); // disable 5V regulator
    }
}

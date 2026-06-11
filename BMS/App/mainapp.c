
#include "mainapp.h"
#include "main.h"
#include "bms_uart.h"
#include "debug_log.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;
extern LPTIM_HandleTypeDef hlptim1;

#define MAINAPP_BMS_UPDATE_MS 100U
#define MAINAPP_IDLE_BEFORE_SLEEP_MINUTES 3U
#define MAINAPP_SLEEP_WAKEUP_MINUTE 5U
#define MAINAPP_ALERT_WAKE_CLEAR_TRIES 3U
#define MAINAPP_ALERT_WAKE_SETTLE_MS 2U
#define MAINAPP_ALERT_IDLE_LEVEL GPIO_PIN_RESET

#define MAINAPP_IDLE_BEFORE_SLEEP_MS ((uint32_t)(MAINAPP_IDLE_BEFORE_SLEEP_MINUTES) * 60UL * 1000UL)
#define MAINAPP_SLEEP_WAKEUP_MS ((uint32_t)(MAINAPP_SLEEP_WAKEUP_MINUTE) * 60UL * 1000UL)

#if BMS_DEBUG_LOG_ENABLE
// static const char *BMS_StateName(BMS_State_t state)
// {
//     switch (state) {
//     case BMS_STATE_INIT:
//         return "INIT";
//     case BMS_STATE_NORMAL:
//         return "NORMAL";
//     case BMS_STATE_CHARGE_PROTECT:
//         return "CHG_PROT";
//     case BMS_STATE_DISCHARGE_PROTECT:
//         return "DCHG_PROT";
//     case BMS_STATE_FAULT:
//         return "FAULT";
//     default:
//         return "UNKNOWN";
//     }
// }

static void MainApp_LogBatteryInfo(const BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return;
    }
    if(tracking->cellVoltages.IndexAccumulated[0] > 0)
    {
        return;
    }


    BMS_LOG_INFO("bat c=%lu st=%s pack=%u adcPack=%u cur=%ld dir=%u chgNow=%u dchNow=%u chgFet=%u dchFet=%u fetEn=%u",
                 (unsigned long)tracking->circle_counter,
                 BMS_StateName(tracking->state),
                 tracking->packVoltage,
                 tracking->batAdcEstimatedPack_mV,
                 (long)tracking->current_mA,
                 (unsigned int)tracking->currentDirection,
                 tracking->charging ? 1U : 0U,
                 tracking->discharging ? 1U : 0U,
                 tracking->chargeFetEnabled ? 1U : 0U,
                 tracking->dischargeFetEnabled ? 1U : 0U,
                 tracking->fetsEnabled ? 1U : 0U);
    BMS_LOG_INFO("Temp1 = %u : Temp2 = %u",
                 (int)tracking->temperature[0],
                 (int)tracking->temperature[1]);
    BMS_LOG_INFO("bq alarmRaw=0x%04x xchg=%u xdsg=%u, SSBC=%u, SSA=%u, CB=%u",
                 (unsigned int)tracking->bqAlarmRawStatus.raw,
                 tracking->bqChargeFetBlocked ? 1U : 0U,
                 tracking->bqDischargeFetBlocked ? 1U : 0U,
                (uint8_t)tracking->bqAlarmRawStatus.bit.SSBC,
                (uint8_t)tracking->bqAlarmRawStatus.bit.SSA,
                (uint8_t)tracking->bqAlarmRawStatus.bit.CB);
    BMS_LOG_INFO("cell min = %u : avg = %u : max = %u : delta = %u : bal = 0x%03x",
                 tracking->cellVoltages.minCellVoltage,
                 tracking->cellVoltages.averageCellVoltage,
                 tracking->cellVoltages.maxCellVoltage,
                 tracking->cellVoltages.deltaCellVoltage,
                 tracking->balanceMask);
    BMS_LOG_INFO("cell %u %u %u %u %u %u %u %u %u %u",
                 tracking->cellVoltages.cellNum[0],
                 tracking->cellVoltages.cellNum[1],
                 tracking->cellVoltages.cellNum[2],
                 tracking->cellVoltages.cellNum[3],
                 tracking->cellVoltages.cellNum[4],
                 tracking->cellVoltages.cellNum[5],
                 tracking->cellVoltages.cellNum[6],
                 tracking->cellVoltages.cellNum[7],
                 tracking->cellVoltages.cellNum[8],
                 tracking->cellVoltages.cellNum[9]);
}
#else
static void MainApp_LogBatteryInfo(const BMS_Tracking_t *tracking)
{
    (void)tracking;
}
#endif

static bool MainApp_PrepareAlertWakeLine(void)
{
    unsigned int alarm_status = 0U;
    unsigned int alarm_raw_status = 0U;
    GPIO_PinState alert_pin;

    for (uint8_t attempt = 0U; attempt < MAINAPP_ALERT_WAKE_CLEAR_TRIES; ++attempt) {
        (void)bq76952_clearAlertStatusRegister(0xFFFFU);
        HAL_Delay(MAINAPP_ALERT_WAKE_SETTLE_MS);

        alarm_status = bq76952_getAlertStatusRegister();
        alert_pin = HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin);
        if ((alarm_status == 0U) && (alert_pin == MAINAPP_ALERT_IDLE_LEVEL)) {
            return true;
        }
    }

    alarm_raw_status = bq76952_getAlertRawStatusRegister();
    alert_pin = HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin);
    BMS_LOG_WARN("sleep blocked alert status=0x%04x raw=0x%04x pin=%u",
                 (unsigned int)alarm_status,
                 (unsigned int)alarm_raw_status,
                 (unsigned int)alert_pin);
    return false;
}

static bool MainApp_IsPackSleepEligible(const BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return false;
    }
    if (!tracking->initialized || !tracking->connected) {
        return false;
    }
    if (tracking->state != BMS_STATE_NORMAL) {
        return false;
    }
    if (BMS_IsFaultActive()) {
        return false;
    }
    // if (tracking->balanceRequired || (tracking->balanceMask != 0U)) {
    //     return false;
    // }
    if (tracking->currentDirection != BMS_CURRENT_IDLE) {
        return false;
    }
    if (tracking->charging || tracking->discharging) {
        return false;
    }
// #if !BQ76952_LOW_POWER_MODE_IS_SHUTDOWN
//     if (!tracking->bqSleepAllowed) {
//         return false;
//     }
// #endif
    return true;
}

static bool MainApp_HasChargeDischargeActivity(const BMS_Tracking_t *tracking)
{
    if (tracking == NULL) {
        return false;
    }
    return (tracking->currentDirection != BMS_CURRENT_IDLE) ||
           tracking->charging ||
           tracking->discharging;
}

void mainapp(void)
{
    static bool initialized = false;
    static uint32_t last_update_tick = 0U;
    static uint32_t last_activity_tick = 0U;
    // static bool allow_immediate_sleep = false;
    uint32_t now;
    const BMS_Tracking_t *tracking;

    if (!initialized) {
        Enable_Power_Battery();
        BMS_Init();
        now = HAL_GetTick();
        last_activity_tick = now;
        last_update_tick = now;
        initialized = true;
        BMS_LOG_INFO("mainapp init sleepX=%lu min wakeY=%lu s",
                     (unsigned long)MAINAPP_IDLE_BEFORE_SLEEP_MINUTES,
                     (unsigned long)MAINAPP_SLEEP_WAKEUP_MINUTE);
        // BMS_Set_5V_Output(false);
    }

    bms_uart_task();

    now = HAL_GetTick();
    if ((now - last_update_tick) >= MAINAPP_BMS_UPDATE_MS) {
        BMS_Update();
        last_update_tick = now;
        tracking = BMS_GetTracking();
        bms_uart_task();
        MainApp_LogBatteryInfo(tracking);

        if (MainApp_HasChargeDischargeActivity(tracking)) {
            last_activity_tick = now;
            // allow_immediate_sleep = false;
        }

        if (MainApp_IsPackSleepEligible(tracking) &&
            (((now - last_activity_tick) >= MAINAPP_IDLE_BEFORE_SLEEP_MS))) {
            power_manager_wakeup_source_t wake_source;
            HAL_StatusTypeDef sleep_rc;
            bool alert_ready;
            bool bq_sleep_ready = false;

            BMS_LOG_INFO("sleep enter bq_sleep=%u idle_ms=%lu",
                         tracking->bqSleepMode ? 1U : 0U,
                         (unsigned long)(now - last_activity_tick));
            HAL_Delay(1000);
            alert_ready = MainApp_PrepareAlertWakeLine();
            if (alert_ready) {
                bq_sleep_ready = bq76952_prepareSleepWithReg2();
            }

            if ((alert_ready == true) &&
                (bq_sleep_ready == true))
            {
#if !BQ76952_LOW_POWER_MODE_IS_SHUTDOWN
                Disable_Power_Battery();
#endif
                sleep_rc = power_manager_enter_low_power_sleep(MAINAPP_SLEEP_WAKEUP_MS);
                wake_source = power_manager_get_and_clear_wakeup_source();
#if !BQ76952_LOW_POWER_MODE_IS_SHUTDOWN
                Enable_Power_Battery();
#endif
                bq76952_resumeFromSleep();
                BMS_Update();
                bms_uart_task();
                now = HAL_GetTick();
                last_update_tick = now;

                if (sleep_rc != HAL_OK) {
                    // allow_immediate_sleep = false;
                    BMS_LOG_ERROR("sleep enter failed");
                    return;
                }

                if ((wake_source & POWER_MANAGER_WAKEUP_GPIO) != 0U) {
                    // allow_immediate_sleep = false;
                    last_activity_tick = now;
                    BMS_LOG_INFO("wake gpio");
                } else if ((wake_source & POWER_MANAGER_WAKEUP_UART) != 0U) {
                    // allow_immediate_sleep = false;
                    last_activity_tick = now;
                    BMS_LOG_INFO("wake uart");
                } else if ((wake_source & POWER_MANAGER_WAKEUP_RTC) != 0U) {
                    // allow_immediate_sleep = true;
                    BMS_LOG_INFO("wake rtc");
                } else {
                    // allow_immediate_sleep = false;
                    last_activity_tick = now;
                    BMS_LOG_WARN("wake unknown");
                }
            }
            else{
                if (bq_sleep_ready) {
                    bq76952_resumeFromSleep();
                }
                last_activity_tick = now;
                BMS_LOG_ERROR("sleep enter failed");
            }
            
        }
        if(tracking->cellVoltages.IndexAccumulated[0] == 0)
        {
            BMS_LOG_INFO("update done state=%s chgDis=%s dchDis=%s faults:Ov:%s, Uv:%s, Ot:%s,Dt:%s,Ut:%s,",
                        BMS_StateName(tracking->state),
                        tracking->chargeDisabled ? "true" : "false",
                        tracking->dischargeDisabled ? "true" : "false",
                        tracking->faults.cellOverVoltage ? "true" : "false",
                        tracking->faults.cellUnderVoltage ? "true" : "false",
                        tracking->faults.chargeOverTemperature ? "true" : "false",
                        tracking->faults.dischargeOverTemperature ? "true" : "false",
                        tracking->faults.underTemperature ? "true" : "false");
        // Occ:%s,Dcc:%s,CGF:%s,DGF:%s,SC:%s,BQF:%s,Commu:%s
            BMS_LOG_INFO("OCChg = %s, OCDsg = %s ,SC:%s,BQF:%s,Commu:%s, delat t = %u", 
                        tracking->faults.chargeOverCurrent ? "true" : "false",
                        tracking->faults.dischargeOverCurrent ? "true" : "false",
                        tracking->faults.shortCircuit ? "true" : "false",
                        tracking->faults.bqSafetyFault ? "true" : "false",
                        tracking->faults.communicationFault ? "true" : "false",
                    (now - last_activity_tick));
        }
    }
}

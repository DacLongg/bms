
#include "mainapp.h"
#include "main.h"
#include "bms_uart.h"
#include "debug_log.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;

#define MAINAPP_BMS_UPDATE_MS 100U
#define MAINAPP_IDLE_BEFORE_SLEEP_MINUTES 1U
#define MAINAPP_SLEEP_WAKEUP_MINUTES 5U
#define MAINAPP_DEEP_SLEEP_WAKEUP_DAYS 5U /* Change to 15U or 30U if required. */
#define MAINAPP_DEEP_SLEEP_AFTER_DAYS 30U
#define MAINAPP_STOP_RTC_CHUNK_SECONDS (12UL * 60UL * 60UL)
#define MAINAPP_ALERT_WAKE_CLEAR_TRIES 3U
#define MAINAPP_ALERT_WAKE_SETTLE_MS 2U
#define MAINAPP_ALERT_IDLE_LEVEL GPIO_PIN_RESET

#define MAINAPP_IDLE_BEFORE_SLEEP_MS ((uint32_t)(MAINAPP_IDLE_BEFORE_SLEEP_MINUTES) * 60UL * 1000UL)
#define MAINAPP_SLEEP_WAKEUP_SECONDS ((uint32_t)(MAINAPP_SLEEP_WAKEUP_MINUTES) * 60UL)
#define MAINAPP_DEEP_SLEEP_WAKEUP_SECONDS ((uint32_t)(MAINAPP_DEEP_SLEEP_WAKEUP_DAYS) * 24UL * 60UL * 60UL)
#define MAINAPP_DEEP_SLEEP_AFTER_SECONDS ((uint32_t)(MAINAPP_DEEP_SLEEP_AFTER_DAYS) * 24UL * 60UL * 60UL)

#if (MAINAPP_SLEEP_WAKEUP_MINUTES == 0U) || (MAINAPP_SLEEP_WAKEUP_MINUTES > 1440U)
#error "SLEEP wake interval must be between 1 and 1440 minutes"
#endif

#if (MAINAPP_DEEP_SLEEP_WAKEUP_DAYS == 0U) || (MAINAPP_DEEP_SLEEP_WAKEUP_DAYS > 30U)
#error "DEEPSLEEP wake interval must be between 1 and 30 days"
#endif

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


    BMS_LOG_INFO("bat cycles=%lu st=%s pack=%u adcPack=%u cur=%ld dir=%u chgNow=%u dchNow=%u chgFet=%u dchFet=%u fetEn=%u",
                 (unsigned long)tracking->completedCycleCount,
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
    (void)alarm_raw_status;
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
    if (!tracking->initialized || !tracking->connected ||
        (tracking->cellVoltages.minCellVoltage < BMS_BALANCE_MIN_CELL_MV)) {
        return false;
    }
    if (tracking->balanceMask != 0U) {
        return false;
    }
    if (tracking->currentDirection != BMS_CURRENT_IDLE) {
        return false;
    }
    if (tracking->charging || tracking->discharging) {
        return false;
    }
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

static bool MainApp_RefreshAfterWake(void)
{
    uint32_t start = HAL_GetTick();
    uint16_t alarm_enable = (uint16_t)bq76952_getAlarmEnableRegister();
    bool full_scan = false;

    /* DEEPSLEEP has no measurements. Wait for a fresh BQ scan before using
     * the ten-sample cell filter for balance or shutdown decisions. FULLSCAN
     * is normally masked to avoid waking the MCU on every scan. */
    if ((alarm_enable == 0U) ||
        !bq76952_setAlarmEnableRegister((uint16_t)(alarm_enable | 0x0080U))) {
        return false;
    }
    if (bq76952_clearAlertStatusRegister(0x0080U)) {
        while ((HAL_GetTick() - start) < 1500U) {
            if ((bq76952_getAlertStatusRegister() & 0x0080U) != 0U) {
                full_scan = true;
                break;
            }
            HAL_Delay(20U);
        }
    }
    if (!bq76952_setAlarmEnableRegister(alarm_enable)) {
        return false;
    }
    (void)bq76952_clearAlertStatusRegister(0x0080U);
    if (!full_scan) {
        return false;
    }
    BMS_ResetCellAveraging();
    for (uint8_t i = 0U; i < BMS_CELL_TIMES_ACUMMULATE; ++i) {
        BMS_Update();
        if ((i + 1U) < BMS_CELL_TIMES_ACUMMULATE) {
            HAL_Delay(MAINAPP_BMS_UPDATE_MS);
        }
    }
    return BMS_HasFreshCellMeasurements() && BMS_GetTracking()->connected &&
           (BMS_GetTracking()->cellVoltages.minCellVoltage != 0U);
}

void mainapp(void)
{
    static bool initialized = false;
    static uint32_t last_update_tick = 0U;
    static uint32_t last_activity_tick = 0U;
    static uint32_t sleep_elapsed_s = 0U;
    static bool deep_sleep_phase = false;
    static uint32_t last_shutdown_attempt_tick = 0U;
    static uint32_t last_refresh_retry_tick = 0U;
    static bool measurements_ready = true;
    uint32_t now;
    const BMS_Tracking_t *tracking;

    if (!initialized) {
        Enable_Power_Battery();
        BMS_Init();
        now = HAL_GetTick();
        last_activity_tick = now;
        last_update_tick = now;
        initialized = true;
        BMS_LOG_INFO("mainapp init idle=%lu min sleepWake=%lu min deepWake=%lu days",
                     (unsigned long)MAINAPP_IDLE_BEFORE_SLEEP_MINUTES,
                     (unsigned long)MAINAPP_SLEEP_WAKEUP_MINUTES,
                     (unsigned long)MAINAPP_DEEP_SLEEP_WAKEUP_DAYS);
        // BMS_Set_5V_Output(false);
    }

    bms_uart_task();

    now = HAL_GetTick();
    if ((now - last_update_tick) >= MAINAPP_BMS_UPDATE_MS) {
        if (measurements_ready) {
            BMS_Update();
        }
        last_update_tick = now;
        tracking = BMS_GetTracking();
        if (!measurements_ready &&
            ((now - last_refresh_retry_tick) >= 5000U)) {
            last_refresh_retry_tick = now;
            if (deep_sleep_phase ? bq76952_exitDeepSleep() :
                                   bq76952_exitSleep()) {
                measurements_ready = MainApp_RefreshAfterWake();
            }
            now = HAL_GetTick();
            last_update_tick = now;
        }
        bms_uart_task();
        MainApp_LogBatteryInfo(tracking);

        if (MainApp_HasChargeDischargeActivity(tracking)) {
            last_activity_tick = now;
            sleep_elapsed_s = 0U;
            deep_sleep_phase = false;
        } else if (tracking->balanceMask != 0U) {
            last_activity_tick = now;
        }

        if (measurements_ready && tracking->initialized && tracking->connected &&
            (tracking->cellVoltages.minCellVoltage != 0U) &&
            (tracking->cellVoltages.minCellVoltage < BMS_BALANCE_MIN_CELL_MV) &&
            ((last_shutdown_attempt_tick == 0U) ||
             ((now - last_shutdown_attempt_tick) >= MAINAPP_IDLE_BEFORE_SLEEP_MS))) {
            last_shutdown_attempt_tick = now;
            bq76952_setCellBalanceMask(0U);
            if (tracking->cellVoltages.deltaCellVoltage > BMS_BALANCE_DELTA_MV_RECOVERY) {
                /* Below the balancing floor, continuing to bleed cells can
                 * damage the weakest cell. Protect the pack first. */
                BMS_LOG_WARN("shutdown with imbalance delta=%u mV",
                             tracking->cellVoltages.deltaCellVoltage);
            }
            BMS_LOG_WARN("low cell %u mV: request shutdown",
                         tracking->cellVoltages.minCellVoltage);
            (void)BMS_UpdateUsageCounter();
            if (!BMS_SaveUsageNow()) {
                BMS_LOG_ERROR("usage checkpoint before shutdown failed");
            }
            if (!bq76952_requestShutdown()) {
                BMS_LOG_ERROR("BQ shutdown request failed");
            }
            return;
        }

        if (measurements_ready && MainApp_IsPackSleepEligible(tracking) &&
            (deep_sleep_phase ||
             ((now - last_activity_tick) >= MAINAPP_IDLE_BEFORE_SLEEP_MS))) {
            power_manager_wakeup_source_t wake_source;
            HAL_StatusTypeDef sleep_rc;
            bool alert_ready;
            bool bq_sleep_ready = false;
            bool bq_wake_ready;
            bool bq_exited_early = false;
            uint32_t remaining_seconds;

            if (sleep_elapsed_s >= MAINAPP_DEEP_SLEEP_AFTER_SECONDS) {
                deep_sleep_phase = true;
            }
            remaining_seconds = deep_sleep_phase ? MAINAPP_DEEP_SLEEP_WAKEUP_SECONDS :
                                                   MAINAPP_SLEEP_WAKEUP_SECONDS;

            alert_ready = MainApp_PrepareAlertWakeLine();
            if (alert_ready) {
                bq_sleep_ready = deep_sleep_phase ? bq76952_enterDeepSleep() :
                                                    bq76952_enterSleep();
            }
            BMS_LOG_INFO("sleep enter deep=%u alert=%u bq=%u idle_ms=%lu",
                         deep_sleep_phase ? 1U : 0U,
                         alert_ready ? 1U : 0U,
                         bq_sleep_ready ? 1U : 0U,
                         (unsigned long)(now - last_activity_tick));

            if (alert_ready && bq_sleep_ready) {
                Disable_Power_Battery();
                do {
                    uint32_t chunk_seconds = remaining_seconds;
                    if (chunk_seconds > MAINAPP_STOP_RTC_CHUNK_SECONDS) {
                        chunk_seconds = MAINAPP_STOP_RTC_CHUNK_SECONDS;
                    }
                    sleep_rc = power_manager_enter_low_power_sleep(chunk_seconds * 1000UL);
                    wake_source = power_manager_get_and_clear_wakeup_source();
                    if (!deep_sleep_phase) {
                        (void)BMS_UpdateUsageCounter();
                    }
                    if ((sleep_rc != HAL_OK) ||
                        ((wake_source & POWER_MANAGER_WAKEUP_RTC) == 0U) ||
                        ((wake_source & POWER_MANAGER_WAKEUP_GPIO) != 0U)) {
                        break;
                    }
                    remaining_seconds -= chunk_seconds;
                    if ((remaining_seconds > 0U) &&
                        !(deep_sleep_phase ? bq76952_isDeepSleep() :
                                              bq76952_getBatteryStatusRegister().bits.SLEEP_MODE != 0U)) {
                        /* The BQ can wake without an MCU GPIO edge. */
                        bq_exited_early = true;
                        break;
                    }
                } while (remaining_seconds > 0U);
                Enable_Power_Battery();
                bq_wake_ready = deep_sleep_phase ? bq76952_exitDeepSleep() :
                                                   bq76952_exitSleep();
                if (bq_wake_ready) {
                    bq_wake_ready = MainApp_RefreshAfterWake();
                }
                measurements_ready = bq_wake_ready;
                bms_uart_task();
                now = HAL_GetTick();
                last_update_tick = now;

                if ((sleep_rc != HAL_OK) || !bq_wake_ready) {
                    last_activity_tick = now;
                    BMS_LOG_ERROR("sleep/wake failed");
                    return;
                }

                if (((wake_source & POWER_MANAGER_WAKEUP_GPIO) != 0U) ||
                    bq_exited_early) {
                    last_activity_tick = now;
                    sleep_elapsed_s = 0U;
                    deep_sleep_phase = false;
                    BMS_LOG_INFO("wake external");
                } else if ((wake_source & POWER_MANAGER_WAKEUP_RTC) != 0U) {
                    if (!deep_sleep_phase) {
                        uint32_t until_deep_sleep = MAINAPP_DEEP_SLEEP_AFTER_SECONDS - sleep_elapsed_s;
                        sleep_elapsed_s += (MAINAPP_SLEEP_WAKEUP_SECONDS < until_deep_sleep) ?
                                           MAINAPP_SLEEP_WAKEUP_SECONDS : until_deep_sleep;
                    }
                    BMS_LOG_INFO("wake rtc");
                } else {
                    last_activity_tick = now;
                    BMS_LOG_WARN("wake unknown");
                }
            } else {
                if (bq_sleep_ready) {
                    (void)(deep_sleep_phase ? bq76952_exitDeepSleep() :
                                              bq76952_exitSleep());
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

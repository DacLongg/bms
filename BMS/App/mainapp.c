
#include "mainapp.h"
#include "bms_uart.h"
#include "debug_log.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;
extern LPTIM_HandleTypeDef hlptim1;

#define MAINAPP_BMS_UPDATE_MS 100U
#define MAINAPP_IDLE_BEFORE_SLEEP_MINUTES 5U
#define MAINAPP_SLEEP_WAKEUP_HOURS 2U

#define MAINAPP_IDLE_BEFORE_SLEEP_MS ((uint32_t)(MAINAPP_IDLE_BEFORE_SLEEP_MINUTES) * 60UL * 1000UL)
#define MAINAPP_SLEEP_WAKEUP_MS ((uint32_t)(MAINAPP_SLEEP_WAKEUP_HOURS) * 60UL * 60UL * 1000UL)

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
    if (tracking->balanceRequired || (tracking->balanceMask != 0U)) {
        return false;
    }
    if (tracking->currentDirection != BMS_CURRENT_IDLE) {
        return false;
    }
    if (tracking->charging || tracking->discharging) {
        return false;
    }
    if (!tracking->bqSleepAllowed) {
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

void mainapp(void)
{
    static bool initialized = false;
    static uint32_t last_update_tick = 0U;
    static uint32_t last_activity_tick = 0U;
    static bool allow_immediate_sleep = false;
    uint32_t now;
    const BMS_Tracking_t *tracking;

    if (!initialized) {
        BMS_Init();
        now = HAL_GetTick();
        last_activity_tick = now;
        last_update_tick = now;
        initialized = true;
        BMS_LOG_INFO("mainapp init sleepX=%lu min wakeY=%lu h",
                     (unsigned long)MAINAPP_IDLE_BEFORE_SLEEP_MINUTES,
                     (unsigned long)MAINAPP_SLEEP_WAKEUP_HOURS);
    }

    bms_uart_task();

    now = HAL_GetTick();
    if ((now - last_update_tick) >= MAINAPP_BMS_UPDATE_MS) {
        BMS_Update();
        last_update_tick = now;
        tracking = BMS_GetTracking();
        bms_uart_task();

        if (MainApp_HasChargeDischargeActivity(tracking)) {
            last_activity_tick = now;
            allow_immediate_sleep = false;
        }

        if (MainApp_IsPackSleepEligible(tracking) &&
            (allow_immediate_sleep || ((now - last_activity_tick) >= MAINAPP_IDLE_BEFORE_SLEEP_MS))) {
            power_manager_wakeup_source_t wake_source;
            HAL_StatusTypeDef sleep_rc;

            BMS_LOG_INFO("sleep enter bq_sleep=%u idle_ms=%lu",
                         tracking->bqSleepMode ? 1U : 0U,
                         (unsigned long)(now - last_activity_tick));
            sleep_rc = power_manager_enter_low_power_sleep(MAINAPP_SLEEP_WAKEUP_MS);
            wake_source = power_manager_get_and_clear_wakeup_source();
            BMS_Update();
            bms_uart_task();
            now = HAL_GetTick();
            last_update_tick = now;

            if (sleep_rc != HAL_OK) {
                allow_immediate_sleep = false;
                BMS_LOG_ERROR("sleep enter failed");
                return;
            }

            if ((wake_source & POWER_MANAGER_WAKEUP_GPIO) != 0U) {
                allow_immediate_sleep = false;
                last_activity_tick = now;
                BMS_LOG_INFO("wake gpio");
            } else if ((wake_source & POWER_MANAGER_WAKEUP_UART) != 0U) {
                allow_immediate_sleep = false;
                last_activity_tick = now;
                BMS_LOG_INFO("wake uart");
            } else if ((wake_source & POWER_MANAGER_WAKEUP_RTC) != 0U) {
                allow_immediate_sleep = true;
                BMS_LOG_INFO("wake rtc");
            } else {
                allow_immediate_sleep = false;
                last_activity_tick = now;
                BMS_LOG_WARN("wake unknown");
            }
        }
    }
}

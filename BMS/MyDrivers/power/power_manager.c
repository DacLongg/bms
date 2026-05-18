#include "power_manager.h"

extern RTC_HandleTypeDef hrtc;

#define POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER 0xFFFFUL

static volatile bool g_power_manager_sleeping;
static volatile power_manager_wakeup_source_t g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;

static uint32_t power_manager_rtc_wakeup_counter_from_ms(uint32_t timeout_ms)
{
    uint32_t counter;

    if (timeout_ms == 0U) {
        return 0U;
    }
    counter = (timeout_ms + 999U) / 1000U;
    if (counter > POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER) {
        counter = POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER;
    }
    return counter;
}


HAL_StatusTypeDef power_manager_enter_low_power_sleep(uint32_t auto_wakeup_ms)
{
    uint32_t wakeup_counter;

    wakeup_counter = power_manager_rtc_wakeup_counter_from_ms(auto_wakeup_ms);

    if (wakeup_counter > 0U) {
        if (HAL_RTCEx_DeactivateWakeUpTimer(&hrtc) != HAL_OK) {
            return HAL_ERROR;
        }

        if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc,
                                        wakeup_counter,
                                        RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
            return HAL_ERROR;
        }

    } else {
        (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    }

    g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    g_power_manager_sleeping = true;
    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    g_power_manager_sleeping = false;
    power_manager_exit_low_power_sleep_to_run();

    return HAL_OK;
}

void power_manager_exit_low_power_sleep_to_run(void)
{
    HAL_ResumeTick();
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    g_power_manager_wakeup_source = (power_manager_wakeup_source_t)(g_power_manager_wakeup_source |
                                                                     POWER_MANAGER_WAKEUP_RTC);
}

bool power_manager_is_sleeping(void)
{
    return g_power_manager_sleeping;
}

void power_manager_notify_gpio_wakeup(void)
{
    g_power_manager_wakeup_source = (power_manager_wakeup_source_t)(g_power_manager_wakeup_source |
                                                                     POWER_MANAGER_WAKEUP_GPIO);
}

void power_manager_notify_uart_wakeup(void)
{
    g_power_manager_wakeup_source = (power_manager_wakeup_source_t)(g_power_manager_wakeup_source |
                                                                     POWER_MANAGER_WAKEUP_UART);
}

power_manager_wakeup_source_t power_manager_get_and_clear_wakeup_source(void)
{
    power_manager_wakeup_source_t source = g_power_manager_wakeup_source;
    g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    return source;

}

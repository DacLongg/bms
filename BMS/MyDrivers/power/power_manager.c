#include "power_manager.h"

extern RTC_HandleTypeDef hrtc;


static uint32_t power_manager_rtc_wakeup_counter_from_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return 0U;
    }
    return (timeout_ms + 999U) / 1000U;
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

    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    power_manager_exit_low_power_sleep_to_run();

    return HAL_OK;
}

void power_manager_exit_low_power_sleep_to_run(void)
{
    HAL_ResumeTick();

    if ((&hrtc != NULL)) {
        (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    }

}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{

}


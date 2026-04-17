
#include "mainapp.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;
extern LPTIM_HandleTypeDef hlptim1;

void mainapp(void)
{
    static uint8_t initialized = 0U;

    if (initialized == 0U) {
        power_manager_config_t power_config = {
            .rtc = &hrtc,
            .uart = &huart2,
            .lptim = &hlptim1
        };

        power_manager_init(&power_config);
        (void)power_manager_enable_uart_wakeup();
        initialized = 1U;
    }

    /* Example:
     * - UART RX interrupt, GPIO EXTI, LPTIM interrupt can wake the MCU.
     * - auto_wakeup_ms = 5000 means RTC wakes the MCU after about 5 s.
     * Replace 5000 with 0 if you only want external interrupts to wake it.
     */
    (void)power_manager_enter_low_power_sleep(5000U);
}

#include "power_manager.h"

typedef struct {
    power_manager_mode_t mode;
    RTC_HandleTypeDef *rtc;
    UART_HandleTypeDef *uart;
    LPTIM_HandleTypeDef *lptim;
    volatile uint32_t wakeup_source;
    volatile uint16_t last_gpio_pin;
    volatile uint8_t last_uart_byte;
    volatile bool sleeping;
    volatile bool rtc_wakeup_armed;
    volatile bool uart_wakeup_armed;
    uint8_t uart_rx_byte;
} power_manager_context_t;

static power_manager_context_t g_power_manager = {
    .mode = POWER_MANAGER_MODE_RUN
};

static uint32_t power_manager_rtc_wakeup_counter_from_ms(uint32_t timeout_ms);

void power_manager_init(const power_manager_config_t *config)
{
    if (config == NULL) {
        return;
    }

    g_power_manager.rtc = config->rtc;
    g_power_manager.uart = config->uart;
    g_power_manager.lptim = config->lptim;
    g_power_manager.mode = POWER_MANAGER_MODE_RUN;
    g_power_manager.wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    g_power_manager.last_gpio_pin = 0U;
    g_power_manager.last_uart_byte = 0U;
    g_power_manager.sleeping = false;
    g_power_manager.rtc_wakeup_armed = false;
}

power_manager_mode_t power_manager_get_mode(void)
{
    return g_power_manager.mode;
}

bool power_manager_is_sleeping(void)
{
    return g_power_manager.sleeping;
}

uint32_t power_manager_get_wakeup_source(void)
{
    return g_power_manager.wakeup_source;
}

void power_manager_clear_wakeup_source(void)
{
    g_power_manager.wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    g_power_manager.last_gpio_pin = 0U;
    g_power_manager.last_uart_byte = 0U;
}

uint16_t power_manager_get_last_gpio_pin(void)
{
    return g_power_manager.last_gpio_pin;
}

uint8_t power_manager_get_last_uart_byte(void)
{
    return g_power_manager.last_uart_byte;
}

static uint32_t power_manager_rtc_wakeup_counter_from_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return 0U;
    }

    return (timeout_ms + 999U) / 1000U;
}

HAL_StatusTypeDef power_manager_enable_uart_wakeup(void)
{
    if (g_power_manager.uart == NULL) {
        return HAL_ERROR;
    }

    g_power_manager.uart_rx_byte = 0U;
    g_power_manager.uart_wakeup_armed = true;
    return HAL_UART_Receive_IT(g_power_manager.uart, &g_power_manager.uart_rx_byte, 1U);
}

HAL_StatusTypeDef power_manager_disable_uart_wakeup(void)
{
    if (g_power_manager.uart == NULL) {
        return HAL_ERROR;
    }

    g_power_manager.uart_wakeup_armed = false;
    return HAL_UART_AbortReceive_IT(g_power_manager.uart);
}

HAL_StatusTypeDef power_manager_enter_low_power_sleep(uint32_t auto_wakeup_ms)
{
    uint32_t wakeup_counter;

    if (g_power_manager.rtc == NULL) {
        return HAL_ERROR;
    }

    power_manager_clear_wakeup_source();
    wakeup_counter = power_manager_rtc_wakeup_counter_from_ms(auto_wakeup_ms);

    if (wakeup_counter > 0U) {
        if (HAL_RTCEx_DeactivateWakeUpTimer(g_power_manager.rtc) != HAL_OK) {
            return HAL_ERROR;
        }

        if (HAL_RTCEx_SetWakeUpTimer_IT(g_power_manager.rtc,
                                        wakeup_counter,
                                        RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
            return HAL_ERROR;
        }

        g_power_manager.rtc_wakeup_armed = true;
    } else {
        (void)HAL_RTCEx_DeactivateWakeUpTimer(g_power_manager.rtc);
        g_power_manager.rtc_wakeup_armed = false;
    }

    g_power_manager.mode = POWER_MANAGER_MODE_LOW_POWER_SLEEP;
    g_power_manager.sleeping = true;

    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    power_manager_exit_low_power_sleep_to_run();

    return HAL_OK;
}

void power_manager_exit_low_power_sleep_to_run(void)
{
    HAL_ResumeTick();

    if ((g_power_manager.rtc != NULL) && g_power_manager.rtc_wakeup_armed) {
        (void)HAL_RTCEx_DeactivateWakeUpTimer(g_power_manager.rtc);
        g_power_manager.rtc_wakeup_armed = false;
    }

    g_power_manager.sleeping = false;
    g_power_manager.mode = POWER_MANAGER_MODE_RUN;
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    if (hrtc == g_power_manager.rtc) {
        g_power_manager.wakeup_source |= POWER_MANAGER_WAKEUP_RTC;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    g_power_manager.wakeup_source |= POWER_MANAGER_WAKEUP_GPIO;
    g_power_manager.last_gpio_pin = GPIO_Pin;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((huart == g_power_manager.uart) && g_power_manager.uart_wakeup_armed) {
        g_power_manager.wakeup_source |= POWER_MANAGER_WAKEUP_UART;
        g_power_manager.last_uart_byte = g_power_manager.uart_rx_byte;
        (void)HAL_UART_Receive_IT(g_power_manager.uart, &g_power_manager.uart_rx_byte, 1U);
    }
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    if (hlptim == g_power_manager.lptim) {
        g_power_manager.wakeup_source |= POWER_MANAGER_WAKEUP_LPTIM;
    }
}

void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    if (hlptim == g_power_manager.lptim) {
        g_power_manager.wakeup_source |= POWER_MANAGER_WAKEUP_LPTIM;
    }
}

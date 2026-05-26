#include "power_manager.h"

#include "adc.h"
#include "bms_uart_channel.h"

extern ADC_HandleTypeDef hadc;
extern LPTIM_HandleTypeDef hlptim1;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;
extern void SystemClock_Config(void);

#define POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER 0xFFFFUL

static volatile bool g_power_manager_sleeping;
static volatile power_manager_wakeup_source_t g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;

static HAL_StatusTypeDef power_manager_config_low_power_sleep_clock(void);
static void power_manager_restore_run_clock(void);
static void power_manager_disable_sleep_peripheral_clocks(void);
static void power_manager_enable_sleep_peripheral_clocks(void);

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

static HAL_StatusTypeDef power_manager_config_low_power_sleep_clock(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    osc.MSIState = RCC_MSI_ON;
    osc.MSIClockRange = RCC_MSIRANGE_0;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return HAL_ERROR;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV2;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        return HAL_ERROR;
    }

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    osc = (RCC_OscInitTypeDef){0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_OFF;
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void power_manager_restore_run_clock(void)
{
    SystemClock_Config();

    if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_MSI) {
        __HAL_RCC_MSI_DISABLE();
    }
}

static void power_manager_disable_sleep_peripheral_clocks(void)
{
    if ((hadc.Instance->CR & ADC_CR_ADEN) != 0U) {
        __HAL_ADC_DISABLE(&hadc);
    }
    __HAL_RCC_ADC1_CLK_DISABLE();
    __HAL_RCC_LPTIM1_CLK_DISABLE();

#if !BMS_UART_PROTOCOL_ENABLE
    __HAL_RCC_USART2_CLK_DISABLE();
#endif
}

static void power_manager_enable_sleep_peripheral_clocks(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_LPTIM1_CLK_ENABLE();

#if !BMS_UART_PROTOCOL_ENABLE
    __HAL_RCC_USART2_CLK_ENABLE();
#endif
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
    power_manager_disable_sleep_peripheral_clocks();
    if (power_manager_config_low_power_sleep_clock() != HAL_OK) {
        g_power_manager_sleeping = false;
        power_manager_restore_run_clock();
        power_manager_enable_sleep_peripheral_clocks();
        return HAL_ERROR;
    }

    __HAL_FLASH_SLEEP_POWERDOWN_ENABLE();
    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    HAL_ResumeTick();
    __HAL_FLASH_SLEEP_POWERDOWN_DISABLE();
    g_power_manager_sleeping = false;
    power_manager_restore_run_clock();
    power_manager_enable_sleep_peripheral_clocks();
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

void Enable_Power_Battery(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void Disable_Power_Battery(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

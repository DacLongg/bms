#include "power_manager.h"

#include "adc.h"
#include "bms_uart.h"
#include "bms_uart_channel.h"

extern ADC_HandleTypeDef hadc;
extern LPTIM_HandleTypeDef hlptim1;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;

#define POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER 0xFFFFUL
#define POWER_MANAGER_RCC_WAIT_LOOP_COUNT 1000000UL

#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
#define POWER_MANAGER_SLEEP_REGULATOR_MODE PWR_LOWPOWERREGULATOR_ON
#else
#define POWER_MANAGER_SLEEP_REGULATOR_MODE PWR_MAINREGULATOR_ON
#endif

static volatile bool g_power_manager_sleeping;
static volatile power_manager_wakeup_source_t g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
static volatile power_manager_debug_stage_t g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_IDLE;
static volatile uint32_t g_power_manager_wfi_enter_count;
static volatile uint32_t g_power_manager_wfi_exit_count;
static volatile uint32_t g_power_manager_rtc_callback_count;
static volatile uint32_t g_power_manager_debug_scr;
static volatile uint32_t g_power_manager_debug_rtc_isr;
static volatile uint32_t g_power_manager_debug_rtc_cr;
static volatile uint32_t g_power_manager_debug_exti_pr;

#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
static HAL_StatusTypeDef power_manager_config_low_power_sleep_clock(void);
static HAL_StatusTypeDef power_manager_restore_run_clock(void);
static HAL_StatusTypeDef power_manager_wait_rcc_flag(uint32_t flag, FlagStatus state);
static HAL_StatusTypeDef power_manager_wait_sysclk_source(uint32_t source);
static HAL_StatusTypeDef power_manager_reconfigure_systick(void);
static uint32_t power_manager_msi_clock_hz(uint32_t msi_range);
#endif
static void power_manager_disable_sleep_peripheral_clocks(void);
static void power_manager_enable_sleep_peripheral_clocks(void);
static HAL_StatusTypeDef power_manager_configure_rtc_wakeup(uint32_t auto_wakeup_ms);
static HAL_StatusTypeDef power_manager_set_uart_sleep_baud(void);
static HAL_StatusTypeDef power_manager_set_uart_run_baud(void);
static void power_manager_wait_for_wakeup_source(void);
static void power_manager_capture_debug_registers(void);
static void power_manager_set_wakeup_source(power_manager_wakeup_source_t source);

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

#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
static HAL_StatusTypeDef power_manager_config_low_power_sleep_clock(void)
{
    __HAL_RCC_MSI_ENABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_MSIRDY, SET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    __HAL_RCC_MSI_RANGE_CONFIG(POWER_MANAGER_LOW_POWER_SLEEP_MSI_RANGE);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE, RCC_SYSCLK_DIV1);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE1, RCC_HCLK_DIV1);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE2, (RCC_HCLK_DIV1 << 3U));
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_MSI);
    if (power_manager_wait_sysclk_source(RCC_SYSCLKSOURCE_STATUS_MSI) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);
    if (__HAL_FLASH_GET_LATENCY() != FLASH_LATENCY_0) {
        return HAL_ERROR;
    }

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    __HAL_RCC_PLL_DISABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_PLLRDY, RESET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    __HAL_RCC_HSI_DISABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_HSIRDY, RESET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    SystemCoreClock = power_manager_msi_clock_hz(POWER_MANAGER_LOW_POWER_SLEEP_MSI_RANGE);
    return power_manager_reconfigure_systick();
}

static HAL_StatusTypeDef power_manager_restore_run_clock(void)
{
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    __HAL_RCC_HSI_ENABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_HSIRDY, SET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    __HAL_RCC_PLL_DISABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_PLLRDY, RESET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    __HAL_RCC_PLL_CONFIG(RCC_PLLSOURCE_HSI, RCC_PLLMUL_4, RCC_PLLDIV_2);
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);
    if (__HAL_FLASH_GET_LATENCY() != FLASH_LATENCY_1) {
        return HAL_ERROR;
    }

    __HAL_RCC_PLL_ENABLE();
    if (power_manager_wait_rcc_flag(RCC_FLAG_PLLRDY, SET) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE, RCC_SYSCLK_DIV1);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE1, RCC_HCLK_DIV1);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE2, (RCC_HCLK_DIV1 << 3U));
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_PLLCLK);
    if (power_manager_wait_sysclk_source(RCC_SYSCLKSOURCE_STATUS_PLLCLK) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    SystemCoreClock = HAL_RCC_GetSysClockFreq();
    if (power_manager_reconfigure_systick() != HAL_OK) {
        return HAL_ERROR;
    }

    if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_MSI) {
        __HAL_RCC_MSI_DISABLE();
    }
    return HAL_OK;
}

static HAL_StatusTypeDef power_manager_wait_rcc_flag(uint32_t flag, FlagStatus state)
{
    uint32_t timeout = POWER_MANAGER_RCC_WAIT_LOOP_COUNT;

    while (__HAL_RCC_GET_FLAG(flag) != state) {
        if (timeout == 0U) {
            return HAL_TIMEOUT;
        }
        timeout--;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef power_manager_wait_sysclk_source(uint32_t source)
{
    uint32_t timeout = POWER_MANAGER_RCC_WAIT_LOOP_COUNT;

    while (__HAL_RCC_GET_SYSCLK_SOURCE() != source) {
        if (timeout == 0U) {
            return HAL_TIMEOUT;
        }
        timeout--;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef power_manager_reconfigure_systick(void)
{
    uint32_t ticks = SystemCoreClock / (1000U / (uint32_t)uwTickFreq);

    if ((ticks == 0U) || ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk)) {
        return HAL_ERROR;
    }

    SysTick->LOAD = ticks - 1UL;
    SysTick->VAL = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    return HAL_OK;
}

static uint32_t power_manager_msi_clock_hz(uint32_t msi_range)
{
    return 32768UL * (1UL << (((msi_range & RCC_ICSCR_MSIRANGE) >> RCC_ICSCR_MSIRANGE_Pos) + 1UL));
}
#endif

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

static HAL_StatusTypeDef power_manager_configure_rtc_wakeup(uint32_t auto_wakeup_ms)
{
    uint32_t wakeup_counter = power_manager_rtc_wakeup_counter_from_ms(auto_wakeup_ms);
    HAL_StatusTypeDef status;

    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_CONFIG_RTC;
    status = HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    if (status != HAL_OK) {
        return status;
    }

    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_IRQn);

    if (wakeup_counter == 0U) {
        return HAL_OK;
    }

    status = HAL_RTCEx_SetWakeUpTimer_IT(&hrtc,
                                         wakeup_counter,
                                         RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
    if (status != HAL_OK) {
        return status;
    }

    HAL_NVIC_EnableIRQ(RTC_IRQn);
    return HAL_OK;
}

static HAL_StatusTypeDef power_manager_set_uart_sleep_baud(void)
{
#if BMS_UART_PROTOCOL_ENABLE
    HAL_StatusTypeDef status = MX_USART2_UART_SetSleepBaudRate();

    if (status == HAL_OK) {
        bms_uart_restart_rx();
    }
    return status;
#else
    return HAL_OK;
#endif
}

static HAL_StatusTypeDef power_manager_set_uart_run_baud(void)
{
#if BMS_UART_PROTOCOL_ENABLE
    HAL_StatusTypeDef status = MX_USART2_UART_SetRunBaudRate();

    if (status == HAL_OK) {
        bms_uart_restart_rx();
    }
    return status;
#else
    return HAL_OK;
#endif
}

static void power_manager_wait_for_wakeup_source(void)
{
    HAL_PWR_DisableSleepOnExit();
    while (g_power_manager_wakeup_source == POWER_MANAGER_WAKEUP_NONE) {
        HAL_SuspendTick();
        HAL_PWR_DisableSleepOnExit();
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_WFI_ENTER;
        g_power_manager_wfi_enter_count++;
        power_manager_capture_debug_registers();
        __DSB();
        __ISB();
        HAL_PWR_EnterSLEEPMode(POWER_MANAGER_SLEEP_REGULATOR_MODE, PWR_SLEEPENTRY_WFI);
        g_power_manager_wfi_exit_count++;
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_WFI_EXIT;
        power_manager_capture_debug_registers();
        HAL_ResumeTick();
    }
}

static void power_manager_capture_debug_registers(void)
{
    g_power_manager_debug_scr = SCB->SCR;
    g_power_manager_debug_rtc_isr = RTC->ISR;
    g_power_manager_debug_rtc_cr = RTC->CR;
    g_power_manager_debug_exti_pr = EXTI->PR;
}

static void power_manager_set_wakeup_source(power_manager_wakeup_source_t source)
{
    g_power_manager_wakeup_source = (power_manager_wakeup_source_t)(g_power_manager_wakeup_source |
                                                                     source);
    HAL_PWR_DisableSleepOnExit();
    CLEAR_BIT(SCB->SCR, ((uint32_t)SCB_SCR_SLEEPONEXIT_Msk | (uint32_t)SCB_SCR_SLEEPDEEP_Msk));
    __DSB();
    __ISB();
}

HAL_StatusTypeDef power_manager_enter_low_power_sleep(uint32_t auto_wakeup_ms)
{
    g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    g_power_manager_sleeping = true;

    power_manager_disable_sleep_peripheral_clocks();
#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_SLEEP_CLOCK;
    if (power_manager_config_low_power_sleep_clock() != HAL_OK) {
        g_power_manager_sleeping = false;
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        (void)power_manager_restore_run_clock();
        power_manager_enable_sleep_peripheral_clocks();
        return HAL_ERROR;
    }
#endif

    if (power_manager_set_uart_sleep_baud() != HAL_OK) {
        g_power_manager_sleeping = false;
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
        (void)power_manager_restore_run_clock();
#endif
        (void)power_manager_set_uart_run_baud();
        power_manager_enable_sleep_peripheral_clocks();
        return HAL_ERROR;
    }

    if (power_manager_configure_rtc_wakeup(auto_wakeup_ms) != HAL_OK) {
        g_power_manager_sleeping = false;
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
        (void)power_manager_restore_run_clock();
#endif
        (void)power_manager_set_uart_run_baud();
        power_manager_enable_sleep_peripheral_clocks();
        return HAL_ERROR;
    }

    __HAL_FLASH_SLEEP_POWERDOWN_ENABLE();
    power_manager_wait_for_wakeup_source();
    __HAL_FLASH_SLEEP_POWERDOWN_DISABLE();
    g_power_manager_sleeping = false;
#if POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_RESTORE_CLOCK;
    if (power_manager_restore_run_clock() != HAL_OK) {
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        power_manager_enable_sleep_peripheral_clocks();
        power_manager_exit_low_power_sleep_to_run();
        return HAL_ERROR;
    }
#endif
    if (power_manager_set_uart_run_baud() != HAL_OK) {
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        power_manager_enable_sleep_peripheral_clocks();
        power_manager_exit_low_power_sleep_to_run();
        return HAL_ERROR;
    }
    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ENABLE_PERIPHERALS;
    power_manager_enable_sleep_peripheral_clocks();
    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_EXIT;
    power_manager_exit_low_power_sleep_to_run();

    return HAL_OK;
}

void power_manager_exit_low_power_sleep_to_run(void)
{
    HAL_ResumeTick();
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_IRQn);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    g_power_manager_rtc_callback_count++;
    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_RTC_CALLBACK;
    power_manager_capture_debug_registers();
    __HAL_RTC_WAKEUPTIMER_EXTI_DISABLE_IT();
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(RTC_IRQn);
    power_manager_set_wakeup_source(POWER_MANAGER_WAKEUP_RTC);
}

bool power_manager_is_sleeping(void)
{
    return g_power_manager_sleeping;
}

void power_manager_notify_gpio_wakeup(void)
{
    power_manager_set_wakeup_source(POWER_MANAGER_WAKEUP_GPIO);
}

void power_manager_notify_uart_wakeup(void)
{
    power_manager_set_wakeup_source(POWER_MANAGER_WAKEUP_UART);
}

power_manager_wakeup_source_t power_manager_get_and_clear_wakeup_source(void)
{
    power_manager_wakeup_source_t source = g_power_manager_wakeup_source;
    g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    return source;

}

void power_manager_get_debug_snapshot(power_manager_debug_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->stage = g_power_manager_debug_stage;
    snapshot->wakeup_source = g_power_manager_wakeup_source;
    snapshot->wfi_enter_count = g_power_manager_wfi_enter_count;
    snapshot->wfi_exit_count = g_power_manager_wfi_exit_count;
    snapshot->rtc_callback_count = g_power_manager_rtc_callback_count;
    snapshot->scr = g_power_manager_debug_scr;
    snapshot->rtc_isr = g_power_manager_debug_rtc_isr;
    snapshot->rtc_cr = g_power_manager_debug_rtc_cr;
    snapshot->exti_pr = g_power_manager_debug_exti_pr;
}

void Enable_Power_Battery(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void Disable_Power_Battery(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

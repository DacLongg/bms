#include "power_manager.h"
#include "debug_log.h"
#include "adc.h"
#include "bms_uart.h"
#include "bms_uart_channel.h"
#include "gpio.h"

extern ADC_HandleTypeDef hadc;
extern LPTIM_HandleTypeDef hlptim1;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;

#define POWER_MANAGER_RTC_WAKEUP_MAX_COUNTER 0xFFFFUL
#define POWER_MANAGER_STOP_REGULATOR_MODE PWR_LOWPOWERREGULATOR_ON
#define POWER_MANAGER_ALERT_ACTIVE_LEVEL GPIO_PIN_SET
#define POWER_MANAGER_ALERT_EXTI_MASK ALERT_Pin
#define POWER_MANAGER_NON_WAKE_EXTI_MASK (GPIO_PIN_0 | GPIO_PIN_1)
#define POWER_MANAGER_GPIOA_SLEEP_KEEP_PINS (ALERT_Pin | FETOFF_Pin | POW_EN_Pin)
#define POWER_MANAGER_GPIOB_SLEEP_KEEP_PINS (BATS_EN_Pin | SHUT_Pin)

typedef struct {
    uint32_t moder;
    uint32_t otyper;
    uint32_t ospeedr;
    uint32_t pupdr;
    uint32_t odr;
    uint32_t afr[2];
} power_manager_gpio_context_t;

typedef struct {
    power_manager_gpio_context_t gpioa;
    power_manager_gpio_context_t gpiob;
    power_manager_gpio_context_t gpioh;
    uint32_t rcc_iopenr;
    uint32_t rcc_iopsmenr;
    uint32_t rcc_ahbsmenr;
    uint32_t rcc_apb1smenr;
    uint32_t rcc_apb2smenr;
    uint32_t exti_imr;
    uint32_t exti_emr;
    uint32_t exti_rtsr;
    uint32_t exti_ftsr;
    uint32_t syscfg_exticr[4];
    uint8_t exti0_1_irq_enabled;
    uint8_t exti2_3_irq_enabled;
} power_manager_stop_context_t;

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
static power_manager_stop_context_t g_power_manager_stop_context;

extern void SystemClock_Config(void);

static HAL_StatusTypeDef power_manager_restore_run_clock(void);
static HAL_StatusTypeDef power_manager_reconfigure_systick(void);
static void power_manager_save_stop_context(void);
static void power_manager_restore_stop_context(void);
static void power_manager_save_gpio_context(GPIO_TypeDef *port,
                                            power_manager_gpio_context_t *context);
static void power_manager_restore_gpio_context(GPIO_TypeDef *port,
                                               const power_manager_gpio_context_t *context);
static void power_manager_disable_sleep_peripheral_clocks(void);
static void power_manager_enable_sleep_peripheral_clocks(void);
static HAL_StatusTypeDef power_manager_suspend_run_peripherals(void);
static HAL_StatusTypeDef power_manager_resume_run_peripherals(void);
static void power_manager_configure_stop_gpio(void);
static void power_manager_configure_sleep_output(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level);
static void power_manager_configure_sleep_analog(GPIO_TypeDef *port, uint16_t pins);
static void power_manager_configure_alert_wakeup_pin(void);
static void power_manager_prepare_stop_power(void);
static bool power_manager_is_alert_pin_active(void);
static HAL_StatusTypeDef power_manager_configure_rtc_wakeup(uint32_t auto_wakeup_ms);
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

static HAL_StatusTypeDef power_manager_restore_run_clock(void)
{
    SystemClock_Config();
    SystemCoreClockUpdate();
    return power_manager_reconfigure_systick();
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

static void power_manager_disable_sleep_peripheral_clocks(void)
{
    RCC->IOPSMENR = 0U;
    RCC->AHBSMENR = RCC_AHBSMENR_SRAMSMEN;
    RCC->APB1SMENR = RCC_APB1SMENR_PWRSMEN;
    RCC->APB2SMENR = 0U;
}

static void power_manager_enable_sleep_peripheral_clocks(void)
{
    RCC->IOPSMENR = g_power_manager_stop_context.rcc_iopsmenr;
    RCC->AHBSMENR = g_power_manager_stop_context.rcc_ahbsmenr;
    RCC->APB1SMENR = g_power_manager_stop_context.rcc_apb1smenr;
    RCC->APB2SMENR = g_power_manager_stop_context.rcc_apb2smenr;
}

static void power_manager_save_stop_context(void)
{
    g_power_manager_stop_context.rcc_iopenr = RCC->IOPENR;
    g_power_manager_stop_context.rcc_iopsmenr = RCC->IOPSMENR;
    g_power_manager_stop_context.rcc_ahbsmenr = RCC->AHBSMENR;
    g_power_manager_stop_context.rcc_apb1smenr = RCC->APB1SMENR;
    g_power_manager_stop_context.rcc_apb2smenr = RCC->APB2SMENR;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    power_manager_save_gpio_context(GPIOA, &g_power_manager_stop_context.gpioa);
    power_manager_save_gpio_context(GPIOB, &g_power_manager_stop_context.gpiob);
    power_manager_save_gpio_context(GPIOH, &g_power_manager_stop_context.gpioh);

    g_power_manager_stop_context.exti_imr = EXTI->IMR;
    g_power_manager_stop_context.exti_emr = EXTI->EMR;
    g_power_manager_stop_context.exti_rtsr = EXTI->RTSR;
    g_power_manager_stop_context.exti_ftsr = EXTI->FTSR;
    for (uint8_t i = 0U; i < 4U; ++i) {
        g_power_manager_stop_context.syscfg_exticr[i] = SYSCFG->EXTICR[i];
    }

    g_power_manager_stop_context.exti0_1_irq_enabled =
        (uint8_t)(NVIC_GetEnableIRQ(EXTI0_1_IRQn) != 0U);
    g_power_manager_stop_context.exti2_3_irq_enabled =
        (uint8_t)(NVIC_GetEnableIRQ(EXTI2_3_IRQn) != 0U);
}

static void power_manager_restore_stop_context(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    power_manager_restore_gpio_context(GPIOA, &g_power_manager_stop_context.gpioa);
    power_manager_restore_gpio_context(GPIOB, &g_power_manager_stop_context.gpiob);
    power_manager_restore_gpio_context(GPIOH, &g_power_manager_stop_context.gpioh);

    EXTI->IMR = g_power_manager_stop_context.exti_imr;
    EXTI->EMR = g_power_manager_stop_context.exti_emr;
    EXTI->RTSR = g_power_manager_stop_context.exti_rtsr;
    EXTI->FTSR = g_power_manager_stop_context.exti_ftsr;
    for (uint8_t i = 0U; i < 4U; ++i) {
        SYSCFG->EXTICR[i] = g_power_manager_stop_context.syscfg_exticr[i];
    }

    RCC->IOPENR = g_power_manager_stop_context.rcc_iopenr;

    if (g_power_manager_stop_context.exti0_1_irq_enabled != 0U) {
        HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
    }
    if (g_power_manager_stop_context.exti2_3_irq_enabled != 0U) {
        HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    }
}

static void power_manager_save_gpio_context(GPIO_TypeDef *port,
                                            power_manager_gpio_context_t *context)
{
    context->moder = port->MODER;
    context->otyper = port->OTYPER;
    context->ospeedr = port->OSPEEDR;
    context->pupdr = port->PUPDR;
    context->odr = port->ODR;
    context->afr[0] = port->AFR[0];
    context->afr[1] = port->AFR[1];
}

static void power_manager_restore_gpio_context(GPIO_TypeDef *port,
                                               const power_manager_gpio_context_t *context)
{
    port->MODER = context->moder;
    port->OTYPER = context->otyper;
    port->OSPEEDR = context->ospeedr;
    port->PUPDR = context->pupdr;
    port->ODR = context->odr;
    port->AFR[0] = context->afr[0];
    port->AFR[1] = context->afr[1];
}

static HAL_StatusTypeDef power_manager_suspend_run_peripherals(void)
{
    HAL_StatusTypeDef status = HAL_OK;

    (void)HAL_ADC_Stop(&hadc);
    if (HAL_ADC_DeInit(&hadc) != HAL_OK) {
        status = HAL_ERROR;
    }

    if (HAL_LPTIM_DeInit(&hlptim1) != HAL_OK) {
        status = HAL_ERROR;
    }

    (void)HAL_UART_Abort(&huart2);
    if (HAL_UART_DeInit(&huart2) != HAL_OK) {
        status = HAL_ERROR;
    }

    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    return status;
}

static HAL_StatusTypeDef power_manager_resume_run_peripherals(void)
{
    MX_ADC_Init();
    MX_LPTIM1_Init();
    MX_USART2_UART_Init();

#if BMS_UART_PROTOCOL_ENABLE
    bms_uart_restart_rx();
#endif
    return HAL_OK;
}

static void power_manager_configure_stop_gpio(void)
{
    HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI0_1_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
    HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(ALERT_Pin);

    power_manager_configure_sleep_output(FETOFF_GPIO_Port, FETOFF_Pin, GPIO_PIN_RESET);
    power_manager_configure_sleep_output(POW_EN_GPIO_Port, POW_EN_Pin, GPIO_PIN_RESET);
    power_manager_configure_sleep_output(BATS_EN_GPIO_Port, BATS_EN_Pin, GPIO_PIN_RESET);
    power_manager_configure_sleep_output(SHUT_GPIO_Port, SHUT_Pin, GPIO_PIN_RESET);

    power_manager_configure_sleep_analog(GPIOA,
                                         (uint16_t)(GPIO_PIN_All &
                                                    (uint16_t)~POWER_MANAGER_GPIOA_SLEEP_KEEP_PINS));
    power_manager_configure_sleep_analog(GPIOB,
                                         (uint16_t)(GPIO_PIN_All &
                                                    (uint16_t)~POWER_MANAGER_GPIOB_SLEEP_KEEP_PINS));
    power_manager_configure_sleep_analog(GPIOH, GPIO_PIN_All);
    power_manager_configure_alert_wakeup_pin();

    CLEAR_BIT(EXTI->IMR, POWER_MANAGER_NON_WAKE_EXTI_MASK);
    CLEAR_BIT(EXTI->EMR, POWER_MANAGER_NON_WAKE_EXTI_MASK);
    CLEAR_BIT(EXTI->FTSR, (POWER_MANAGER_NON_WAKE_EXTI_MASK | POWER_MANAGER_ALERT_EXTI_MASK));
    SET_BIT(EXTI->RTSR, POWER_MANAGER_ALERT_EXTI_MASK);
    SET_BIT(EXTI->IMR, POWER_MANAGER_ALERT_EXTI_MASK);

    __HAL_GPIO_EXTI_CLEAR_IT(ALERT_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

    if (power_manager_is_alert_pin_active()) {
        power_manager_set_wakeup_source(POWER_MANAGER_WAKEUP_GPIO);
    }

    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();
}

static void power_manager_configure_sleep_output(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(port, pin, level);
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

static void power_manager_configure_sleep_analog(GPIO_TypeDef *port, uint16_t pins)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (pins == 0U) {
        return;
    }
    GPIO_InitStruct.Pin = pins;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

static void power_manager_configure_alert_wakeup_pin(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = ALERT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ALERT_GPIO_Port, &GPIO_InitStruct);
}

static void power_manager_prepare_stop_power(void)
{
#if defined(PWR_PVD_SUPPORT)
    HAL_PWR_DisablePVD();
#endif
    HAL_PWREx_EnableUltraLowPower();
    HAL_PWREx_DisableFastWakeUp();
    HAL_DBGMCU_DisableDBGSleepMode();
    HAL_DBGMCU_DisableDBGStopMode();
    HAL_DBGMCU_DisableDBGStandbyMode();
}

static bool power_manager_is_alert_pin_active(void)
{
    return HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin) == POWER_MANAGER_ALERT_ACTIVE_LEVEL;
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

static void power_manager_wait_for_wakeup_source(void)
{
    HAL_PWR_DisableSleepOnExit();
    while (g_power_manager_wakeup_source == POWER_MANAGER_WAKEUP_NONE) {
        HAL_SuspendTick();
        HAL_PWR_DisableSleepOnExit();
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_WFI_ENTER;
        g_power_manager_wfi_enter_count++;
        power_manager_capture_debug_registers();
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        __DSB();
        __ISB();
        HAL_PWR_EnterSTOPMode(POWER_MANAGER_STOP_REGULATOR_MODE, PWR_STOPENTRY_WFI);
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
    HAL_StatusTypeDef status = HAL_OK;

    g_power_manager_wakeup_source = POWER_MANAGER_WAKEUP_NONE;
    g_power_manager_sleeping = true;

    power_manager_save_stop_context();
    power_manager_disable_sleep_peripheral_clocks();

    if (power_manager_suspend_run_peripherals() != HAL_OK) {
        BMS_LOG_ERROR("power_manager_suspend_run_peripherals failed");
        status = HAL_ERROR;
    }

    if ((status == HAL_OK) &&
        (power_manager_configure_rtc_wakeup(auto_wakeup_ms) != HAL_OK)) {
            BMS_LOG_ERROR("power_manager_configure_rtc_wakeup failed");
        status = HAL_ERROR;
    }

    if (status == HAL_OK) {
        power_manager_configure_stop_gpio();
        power_manager_prepare_stop_power();
    }

    if (status == HAL_OK) {
        __HAL_FLASH_SLEEP_POWERDOWN_ENABLE();
        power_manager_wait_for_wakeup_source();
    }

    __HAL_FLASH_SLEEP_POWERDOWN_DISABLE();
    g_power_manager_sleeping = false;

    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_RESTORE_CLOCK;
    if ((status == HAL_OK) && (power_manager_restore_run_clock() != HAL_OK)) {
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        power_manager_enable_sleep_peripheral_clocks();
        power_manager_restore_stop_context();
        (void)power_manager_resume_run_peripherals();
        power_manager_exit_low_power_sleep_to_run();
        BMS_LOG_ERROR("power_manager_restore_run_clock failed");
        return HAL_ERROR;
    }

    power_manager_enable_sleep_peripheral_clocks();
    power_manager_restore_stop_context();

    if (power_manager_resume_run_peripherals() != HAL_OK) {
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        power_manager_exit_low_power_sleep_to_run();
        BMS_LOG_ERROR("power_manager_resume_run_peripherals failed");
        return HAL_ERROR;
    }

    if (status != HAL_OK) {
        g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ERROR;
        BMS_LOG_ERROR("g_power_manager_debug_stage failed");

        power_manager_exit_low_power_sleep_to_run();
        return HAL_ERROR;
    }

    g_power_manager_debug_stage = POWER_MANAGER_DEBUG_STAGE_ENABLE_PERIPHERALS;
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
    // bool gpioa_clock_enabled;

    // if (!g_power_manager_sleeping) {
    //     return;
    // }

    // gpioa_clock_enabled = __HAL_RCC_GPIOA_IS_CLK_ENABLED();
    // if (!gpioa_clock_enabled) {
    //     __HAL_RCC_GPIOA_CLK_ENABLE();
    // }
    // if (!power_manager_is_alert_pin_active()) {
    //     if (!gpioa_clock_enabled) {
    //         __HAL_RCC_GPIOA_CLK_DISABLE();
    //     }
    //     return;
    // }
    // if (!gpioa_clock_enabled) {
    //     __HAL_RCC_GPIOA_CLK_DISABLE();
    // }

    power_manager_set_wakeup_source(POWER_MANAGER_WAKEUP_GPIO);
}

void power_manager_notify_uart_wakeup(void)
{
    /* UART is intentionally not a STOP wake source in the optimized sleep path. */
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

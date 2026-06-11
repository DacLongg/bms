#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32l0xx_hal.h"
#include "lptim.h"
#include "rtc.h"
#include "usart.h"

#ifndef POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE
#define POWER_MANAGER_LOW_POWER_SLEEP_CLOCK_ENABLE 1
#endif

#ifndef POWER_MANAGER_LOW_POWER_SLEEP_MSI_RANGE
#define POWER_MANAGER_LOW_POWER_SLEEP_MSI_RANGE RCC_MSIRANGE_0
#endif

typedef enum {
    POWER_MANAGER_MODE_RUN = 0,
    POWER_MANAGER_MODE_SLEEP,
    POWER_MANAGER_MODE_LOW_POWER_RUN,
    POWER_MANAGER_MODE_LOW_POWER_SLEEP,
    POWER_MANAGER_MODE_STOP,
    POWER_MANAGER_MODE_STANDBY
} power_manager_mode_t;

typedef enum {
    POWER_MANAGER_WAKEUP_NONE = 0x00U,
    POWER_MANAGER_WAKEUP_RTC = 0x01U,
    POWER_MANAGER_WAKEUP_GPIO = 0x02U,
    POWER_MANAGER_WAKEUP_UART = 0x04U,
    POWER_MANAGER_WAKEUP_LPTIM = 0x08U
} power_manager_wakeup_source_t;

typedef enum {
    POWER_MANAGER_DEBUG_STAGE_IDLE = 0,
    POWER_MANAGER_DEBUG_STAGE_CONFIG_RTC,
    POWER_MANAGER_DEBUG_STAGE_SLEEP_CLOCK,
    POWER_MANAGER_DEBUG_STAGE_WFI_ENTER,
    POWER_MANAGER_DEBUG_STAGE_RTC_CALLBACK,
    POWER_MANAGER_DEBUG_STAGE_WFI_EXIT,
    POWER_MANAGER_DEBUG_STAGE_RESTORE_CLOCK,
    POWER_MANAGER_DEBUG_STAGE_ENABLE_PERIPHERALS,
    POWER_MANAGER_DEBUG_STAGE_EXIT,
    POWER_MANAGER_DEBUG_STAGE_ERROR
} power_manager_debug_stage_t;

typedef struct {
    RTC_HandleTypeDef *rtc;
    UART_HandleTypeDef *uart;
    LPTIM_HandleTypeDef *lptim;
} power_manager_config_t;

typedef struct {
    power_manager_debug_stage_t stage;
    power_manager_wakeup_source_t wakeup_source;
    uint32_t wfi_enter_count;
    uint32_t wfi_exit_count;
    uint32_t rtc_callback_count;
    uint32_t scr;
    uint32_t rtc_isr;
    uint32_t rtc_cr;
    uint32_t exti_pr;
} power_manager_debug_snapshot_t;

HAL_StatusTypeDef power_manager_enter_low_power_sleep(uint32_t auto_wakeup_ms);
void power_manager_exit_low_power_sleep_to_run(void);
bool power_manager_is_sleeping(void);
void power_manager_notify_gpio_wakeup(void);
void power_manager_notify_uart_wakeup(void);
power_manager_wakeup_source_t power_manager_get_and_clear_wakeup_source(void);
void power_manager_get_debug_snapshot(power_manager_debug_snapshot_t *snapshot);

void Enable_Power_Battery(void);
void Disable_Power_Battery(void);

#endif

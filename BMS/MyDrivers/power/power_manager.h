#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "lptim.h"
#include "rtc.h"
#include "usart.h"

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

typedef struct {
    RTC_HandleTypeDef *rtc;
    UART_HandleTypeDef *uart;
    LPTIM_HandleTypeDef *lptim;
} power_manager_config_t;


HAL_StatusTypeDef power_manager_enter_low_power_sleep(uint32_t auto_wakeup_ms);
void power_manager_exit_low_power_sleep_to_run(void);
bool power_manager_is_sleeping(void);




#endif

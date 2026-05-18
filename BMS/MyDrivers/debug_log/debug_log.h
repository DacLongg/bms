#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdint.h>

#include "stm32l0xx_hal.h"
#include "bms_uart_channel.h"

#ifndef BMS_DEBUG_LOG_BUFFER_SIZE
#define BMS_DEBUG_LOG_BUFFER_SIZE 128U
#endif

#ifndef BMS_DEBUG_LOG_UART_TIMEOUT_MS
#define BMS_DEBUG_LOG_UART_TIMEOUT_MS 20U
#endif

void debug_log_init(UART_HandleTypeDef *uart);
void debug_log_write(const char *level, const char *fmt, ...);

#if BMS_DEBUG_LOG_ENABLE
#define BMS_LOG_INFO(...)  debug_log_write("I", __VA_ARGS__)
#define BMS_LOG_WARN(...)  debug_log_write("W", __VA_ARGS__)
#define BMS_LOG_ERROR(...) debug_log_write("E", __VA_ARGS__)
#else
#define BMS_LOG_INFO(...)  do { } while (0)
#define BMS_LOG_WARN(...)  do { } while (0)
#define BMS_LOG_ERROR(...) do { } while (0)
#endif

#endif

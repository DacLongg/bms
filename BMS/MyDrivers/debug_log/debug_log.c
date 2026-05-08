#include "debug_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *g_debug_uart;

void debug_log_init(UART_HandleTypeDef *uart)
{
    g_debug_uart = uart;
}

void debug_log_write(const char *level, const char *fmt, ...)
{
#if BMS_DEBUG_LOG_ENABLE
    char line[BMS_DEBUG_LOG_BUFFER_SIZE];
    int len;
    va_list args;

    if ((g_debug_uart == NULL) || (fmt == NULL)) {
        return;
    }

    len = snprintf(line, sizeof(line), "[%lu][%s] ", (unsigned long)HAL_GetTick(), level);
    if (len < 0) {
        return;
    }
    if ((uint32_t)len >= sizeof(line)) {
        len = (int)(sizeof(line) - 1U);
    }

    va_start(args, fmt);
    len += vsnprintf(&line[len], sizeof(line) - (uint32_t)len, fmt, args);
    va_end(args);

    if (len < 0) {
        return;
    }
    if ((uint32_t)len > (sizeof(line) - 3U)) {
        len = (int)(sizeof(line) - 3U);
    }

    line[len++] = '\r';
    line[len++] = '\n';
    line[len] = '\0';

    (void)HAL_UART_Transmit(g_debug_uart,
                            (uint8_t *)line,
                            (uint16_t)strlen(line),
                            BMS_DEBUG_LOG_UART_TIMEOUT_MS);
#else
    (void)level;
    (void)fmt;
#endif
}

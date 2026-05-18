#ifndef BMS_UART_CHANNEL_H
#define BMS_UART_CHANNEL_H

/*
 * USART2 is shared by debug log and the external UART protocol.
 *
 * Default build keeps the existing debug log behavior. Build with
 * -DBMS_UART_PROTOCOL_ENABLE=1 to use USART2 for the protocol instead; debug
 * log is then disabled unless explicitly overridden, and enabling both modes
 * is treated as a configuration error.
 */

#ifndef BMS_UART_PROTOCOL_ENABLE
#define BMS_UART_PROTOCOL_ENABLE 0
#endif

#ifndef BMS_DEBUG_LOG_ENABLE
#if BMS_UART_PROTOCOL_ENABLE
#define BMS_DEBUG_LOG_ENABLE 0
#else
#define BMS_DEBUG_LOG_ENABLE 1
#endif
#endif

#if BMS_DEBUG_LOG_ENABLE && BMS_UART_PROTOCOL_ENABLE
#error "USART2 cannot be used for both BMS_DEBUG_LOG_ENABLE and BMS_UART_PROTOCOL_ENABLE"
#endif

#endif

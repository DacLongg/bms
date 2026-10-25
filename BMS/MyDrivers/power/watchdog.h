#ifndef BMS_WATCHDOG_H
#define BMS_WATCHDOG_H

#include <stdbool.h>

/* WWDG is clocked by PCLK1 and stops in STOP mode; unlike IWDG it does not
 * force periodic wakeups during the five-minute low-power interval. */
void watchdog_init(void);
void watchdog_note_main_progress(void);
void watchdog_on_systick(void);
void watchdog_refresh_for_stop_transition(void);
bool watchdog_was_reset(void);

#endif

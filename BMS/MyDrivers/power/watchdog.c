#include "watchdog.h"
#include "main.h"

/* At PCLK1 = 32 MHz, prescaler /8 and counter 0x7f give about 65 ms
 * before reset. SysTick refreshes it every 10 ms only while the application
 * has returned from mainapp() within the progress deadline. */
#define WATCHDOG_PROGRESS_TIMEOUT_MS 15000U
#define WATCHDOG_REFRESH_INTERVAL_MS 10U
#define WATCHDOG_RELOAD 0x7FU

static volatile uint32_t g_last_main_progress_tick;
static volatile uint32_t g_last_refresh_tick;
static volatile bool g_watchdog_started;
static bool g_watchdog_was_reset;

static void watchdog_refresh_if_healthy(uint32_t now)
{
    if (g_watchdog_started &&
        (uint32_t)(now - g_last_main_progress_tick) < WATCHDOG_PROGRESS_TIMEOUT_MS) {
        WWDG->CR = WWDG_CR_WDGA | WATCHDOG_RELOAD;
        g_last_refresh_tick = now;
    }
}

void watchdog_init(void)
{
    g_watchdog_was_reset = (RCC->CSR & RCC_CSR_WWDGRSTF) != 0U;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    g_last_main_progress_tick = HAL_GetTick();
    g_last_refresh_tick = g_last_main_progress_tick;
    __HAL_RCC_WWDG_CLK_ENABLE();
    /* Window 0x7f permits refreshing at any counter value; /8 maximizes
     * the hardware timeout without changing the existing PCLK1 or SysTick. */
    WWDG->CFR = WWDG_CFR_WDGTB | WWDG_CFR_W;
    WWDG->CR = WWDG_CR_WDGA | WATCHDOG_RELOAD;
    g_watchdog_started = true;
}

void watchdog_note_main_progress(void)
{
    g_last_main_progress_tick = HAL_GetTick();
}

void watchdog_on_systick(void)
{
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - g_last_refresh_tick) >= WATCHDOG_REFRESH_INTERVAL_MS) {
        watchdog_refresh_if_healthy(now);
    }
}

void watchdog_refresh_for_stop_transition(void)
{
    /* SysTick is suspended across WFI. The HAL tick also pauses, so the
     * application progress deadline does not include time spent in STOP. */
    watchdog_refresh_if_healthy(HAL_GetTick());
}

bool watchdog_was_reset(void)
{
    return g_watchdog_was_reset;
}

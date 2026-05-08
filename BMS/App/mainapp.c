
#include "mainapp.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;
extern LPTIM_HandleTypeDef hlptim1;

void mainapp(void)
{
    static bool initialized = false;
    static uint32_t last_update_tick = 0U;
    uint32_t now;

    if (!initialized) {
        BMS_Init();
        initialized = true;
    }

    now = HAL_GetTick();
    if ((now - last_update_tick) >= 100U) {
        BMS_Update();
        last_update_tick = now;
    }
}

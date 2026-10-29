# BMS power-state flow

## Three states

| State | STM32 | BQ76952 | Entry / exit |
| --- | --- | --- | --- |
| SLEEP | STOP, RTC wake every `MAINAPP_SLEEP_WAKEUP_MINUTES` (default 5 minutes) | SLEEP | Enter after 1 minute without charge, discharge or active balancing. |
| DEEPSLEEP | STOP, RTC wake every `MAINAPP_DEEP_SLEEP_WAKEUP_DAYS` (default 5 days) | DEEPSLEEP | Enter after 30 cumulative days of SLEEP. |
| SHUTDOWN | Power removed by BQ REG2 shutdown, if the board is wired accordingly | SHUTDOWN | Request when any valid measured cell is below 3500 mV. Charger/TS2/RST_SHUT hardware is needed to start again. |

STM32 STOP and the BQ power state are independent. SLEEP and DEEPSLEEP have separate RTC wake intervals. Intervals longer than 12 hours are divided into 12-hour STOP chunks. Between chunks, firmware checks whether the BQ has left its low-power state; otherwise it re-enters STOP without requesting a measurement. A GPIO wake also interrupts the wait early.

After a scheduled wake, firmware exits the BQ low-power state, waits for a new FULLSCAN and rebuilds the ten-sample cell-voltage average before deciding about balancing, sleep or shutdown. If the new scan is unavailable, sleep and voltage-based shutdown decisions remain blocked while firmware retries. DEEPSLEEP does not measure cells or balance them; balancing occurs only while the BQ is awake in NORMAL mode. When balancing is finished and there is no charge/discharge activity, firmware returns to DEEPSLEEP.

Balancing may run while charging or idle, but not while discharging. An individual cell is only selected when its voltage exceeds 3500 mV and its difference from the minimum cell is large enough. The start/recovery delta remains 30/20 mV; temperature and protection conditions still gate balancing. Active balancing prevents entry to either low-power state.

At a minimum cell voltage below 3500 mV, firmware stops balancing and requests SHUTDOWN. If the cells still differ by more than 20 mV, it emits a warning when the debug-log build is enabled and **prioritizes preventing further discharge of the weakest cell**. The warning is not stored across power loss. Balancing cannot be guaranteed at that point: BQ undervoltage protection is also configured around 3500 mV and can inhibit balancing. Balancing must therefore happen earlier, while cells are above the floor.

## Hardware and timing limitations

- `MAINAPP_SLEEP_WAKEUP_MINUTES` defaults to 5 minutes. `MAINAPP_DEEP_SLEEP_WAKEUP_DAYS` defaults to 5 days and can be set to 15 or 30. Only intervals longer than 12 hours are split into RTC STOP chunks. GPIO wake interrupts either interval early.
- The 30-day counter counts completed SLEEP RTC intervals while the MCU remains powered. It resets on MCU reboot or charge/discharge activity; it is not a persistent calendar timestamp. After the first SLEEP wake at or beyond day 30, firmware enters DEEPSLEEP on its next sleep entry. The transition delay is less than one SLEEP interval (default 5 minutes), plus the awake processing time. The RTC currently uses LSI, so these times are approximate rather than precise wall-clock periods.
- Host-controlled balancing and firmware voltage-based shutdown checks do not run while STM32 is in STOP, including the SLEEP phase. The default SLEEP interval delays these checks by up to about 5 minutes; BQ hardware protections are the independent safety layer and must be validated on the real pack. The configured BQ SLEEP wake-current comparator is 500 mA, so smaller charge/discharge currents may not produce an early wake; validate the ALERT wiring and wake sources.
- BQ DEEPSLEEP disables voltage measurement and balancing. If a cell crosses 3500 mV between scheduled checks, firmware cannot detect it until the next wake. A 15- or 30-day setting increases this latency; it must be validated against pack self-discharge and standby current.
- Firmware can request BQ SHUTDOWN but cannot guarantee STM32 power is actually removed without checking the board's REG2 power path. TI also warns that a thermistor attached from TS2 to VSS can prevent true SHUTDOWN; verify the actual TS2/WAKE wiring on the board.
- USART2 is disabled in STM32 STOP, so UART requests are not a wake source. BQ ALERT/GPIO and RTC remain the configured wake paths.

Parameters and flow: `BMS/App/mainapp.c`; BQ state transitions: `BMS/MyDrivers/bq76952/bq76952.c`; balance threshold: `BMS/MyMiddlewares/bms/bms.h`.

# BMS Sleep Strategy (STM32 low-power sleep + BQ sleep)

## Dieu kien vao ngu

Pack chi vao ngu khi dong thoi:

- `tracking->state == BMS_STATE_NORMAL`
- khong co fault (`BMS_IsFaultActive() == false`)
- khong can can bang (`balanceRequired == false` va `balanceMask == 0`)
- khong sac/xa (`currentDirection == IDLE`, `charging == false`, `discharging == false`)
- BQ cho phep sleep (`bqSleepAllowed == true`)
- khong co activity sac/xa trong `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` phut, hien tai la `5` phut

## Chu ky ngu/thuc

- MCU dung `power_manager_enter_low_power_sleep(Y_ms)` voi `Y = MAINAPP_SLEEP_WAKEUP_SECONDS`, hien tai la `5` giay.
- Wake source:
  - `RTC`: wake dinh ky 5 giay de kiem tra lai pack, lam fallback neu dong sac/xa khong tao canh ALERT.
  - `GPIO`: wake ngay khi co canh tren cac chan EXTI (`ALERT/DCHG/DDSG`). ALERT duoc cau hinh open-drain active-low.
  - `UART`: wake khi host giao tiep.

## Hanh vi sau wake

- Wake boi `RTC`:
  - cap nhat BMS ngay.
  - neu co dong sac/xa thi reset moc idle.
  - neu pack van OK, khong co dong va khong can can bang thi co the ngu lai sau khi du thoi gian idle.
- Wake boi `GPIO` hoac `UART`:
  - reset moc idle.
  - yeu cau khoang im lang lai `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` phut truoc khi ngu.

## Lien quan BQ sleep

- Theo TRM BQ76952, BQ co the tu thoat Sleep khi current vuot nguong `Power:Sleep:Wake Comparator Current`.
- Nguong wake comparator hop le toi thieu cua BQ76952 la `500 mA`; firmware set `0x924B = 500`.
- Khi BQ thoat Sleep vi dong vuot nguong, BQ set `WAKE` trong `Alarm Status`; vi `Default Alarm Mask` da bat bit `WAKE`, ALERT se keo low de tao EXTI cho MCU.
- Neu dong sac/xa nho hon 500 mA, BQ co the khong tao `WAKE` alarm tren ALERT. Khi do MCU van thuc bang RTC fallback toi da sau 5 giay.
- Truoc khi MCU vao sleep, firmware clear `Alarm Status` va chi sleep khi ALERT da ve idle high. Neu ALERT dang bi giu active-low thi bo qua sleep de tranh mat canh wake.
- Sau khi wake, firmware goi `bq76952_resumeFromSleep()` de gui `SLEEP_DISABLE` va apply lai REG1/REG2.

## Tham so chinh

- `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` (hien tai `5`)
- `MAINAPP_SLEEP_WAKEUP_SECONDS` (hien tai `5`)
- `MAINAPP_BMS_UPDATE_MS` (chu ky update khi awake)

Vi tri code: `BMS/App/mainapp.c`

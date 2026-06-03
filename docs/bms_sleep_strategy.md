# BMS Sleep Strategy (STM32 low-power sleep + BQ sleep)

## Dieu kien vao ngu

Pack chi vao ngu khi dong thoi:

- `tracking->state == BMS_STATE_NORMAL`
- khong co fault (`BMS_IsFaultActive() == false`)
- khong can can bang (`balanceRequired == false` va `balanceMask == 0`)
- khong sac/xa (`currentDirection == IDLE`, `charging == false`, `discharging == false`)
- BQ cho phep sleep (`bqSleepAllowed == true`)
- khong co activity sac/xa trong `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` phut, hien tai la `1` phut

## Chu kỳ ngủ/thức
- MCU dùng `power_manager_enter_low_power_sleep(Y_ms)` với `Y` giờ (mặc định `2` giờ).
- Wake source:
  - `RTC`: wake dinh ky de kiem tra lai pack.
  - `GPIO`: wake ngay khi co hoat dong phan cung da noi vao EXTI (`ALERT/DCHG/DDSG`).
  - `UART`: wake khi host giao tiep.

## Hanh vi sau wake

- Wake boi `RTC`:
  - cap nhat BMS ngay.
  - neu co dong sac/xa thi huy co che ngu lai ngay va reset moc idle.
  - neu pack van OK, khong co dong va khong can can bang thi co the ngu lai ngay.
- Wake boi `GPIO` hoac `UART`:
  - huy co che ngu lai ngay.
  - yeu cau khoang im lang lai `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` phut truoc khi ngu.

## Lien quan BQ sleep

- Theo TRM BQ76952, BQ co the tu thoat Sleep khi current vuot nguong `Power:Sleep:Wake Comparator Current`.
- Viec BQ tu thoat Sleep theo dong khong dong nghia MCU se tu thuc. MCU chi thuc neu co tin hieu wake duoc noi vao EXTI, RTC wake, hoac UART wake.
- Firmware hien dung RTC wake 5 giay de poll lai BQ/current, tranh truong hop cam tai xa nhung khong co canh GPIO lam MCU thuc ngay.
- Sau khi wake, firmware goi `bq76952_resumeFromSleep()` de gui `SLEEP_DISABLE` va apply lai REG1/REG2.

## Tham so chinh

- `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` (hien tai `1`)
- `MAINAPP_SLEEP_WAKEUP_SECONDS` (hien tai `5`)
- `MAINAPP_BMS_UPDATE_MS` (chu ky update khi awake)

Vi tri code: `BMS/App/mainapp.c`

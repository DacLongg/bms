# BMS Sleep Strategy (STM32 low-power sleep + BQ deep-sleep)

## Điều kiện vào ngủ
Pack chỉ vào ngủ khi đồng thời:
- `tracking->state == BMS_STATE_NORMAL`
- không có fault (`BMS_IsFaultActive() == false`)
- không cần cân bằng (`balanceRequired == false` và `balanceMask == 0`)
- không sạc/xả (`currentDirection == IDLE`, `charging == false`, `discharging == false`)
- BQ cho phép sleep (`bqSleepAllowed == true`)
- không có activity sạc/xả trong `X` phút (mặc định `5` phút)

## Chu kỳ ngủ/thức
- MCU dùng `power_manager_enter_low_power_sleep(Y_ms)` với `Y` giờ (mặc định `2` giờ).
- Wake source:
  - `RTC`: wake định kỳ để kiểm tra lại pack.
  - `GPIO`: wake ngay khi có hoạt động phần cứng (`ALERT/DCHG/DDSG` EXTI).

## Hành vi sau wake
- Wake bởi `RTC`:
  - cập nhật BMS ngay.
  - nếu pack vẫn OK và không cần cân bằng -> ngủ lại ngay (không cần chờ lại X phút).
  - nếu có fault/cần cân bằng -> xử lý bình thường; khi điều kiện OK trở lại thì ngủ lại.
- Wake bởi `GPIO` (cắm tải xả hoặc cắm sạc):
  - hủy cơ chế ngủ lại ngay.
  - yêu cầu khoảng im lặng lại X phút trước khi ngủ.

## Liên quan BQ deep-sleep
- Cơ chế này dùng trạng thái runtime `bqSleepAllowed`/`bqSleepMode` từ `BatteryStatus` của BQ để đồng bộ quyết định ngủ.
- BQ sẽ tự vào deep-sleep khi điều kiện nội bộ thỏa (current thấp, không active state ngăn sleep).

## Tham số chính
- `MAINAPP_IDLE_BEFORE_SLEEP_MINUTES` (X)
- `MAINAPP_SLEEP_WAKEUP_HOURS` (Y)
- `MAINAPP_BMS_UPDATE_MS` (chu kỳ update khi awake)

Vị trí code: `BMS/App/mainapp.c`

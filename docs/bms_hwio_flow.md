# BMS HW-IO flow (ALERT / DCHG / DDSG / FETOFF / SHUT / BAT_ADC)

## 1) Mapping chân
- `PA1` -> `FETOFF` (MCU output)
- `PA2` -> `ALERT` (MCU input EXTI)
- `PB0` -> `DCHG` (MCU input EXTI)
- `PB1` -> `DDSG` (MCU input EXTI)
- `PB10` -> `SHUT` (MCU output)
- `PB4` -> `BATS_EN` (MCU output enable mạch chia áp)
- `PA4` -> `BAT_ADC` (ADC1_IN4)

## 2) Cơ chế ngắt cho toàn bộ input
- `ALERT`, `DCHG`, `DDSG` đều chạy `GPIO_MODE_IT_RISING_FALLING`.
- `DCHG/DDSG` được cập nhật cả trong `HAL_GPIO_EXTI_Callback()` và được poll lại trong mỗi chu kỳ `BMS_Update()`.
- BQ76952 U1 pin 31/32 la `DCHG`/`DDSG`; firmware cau hinh data memory `DCHG Pin Config` va `DDSG Pin Config` = `0x2A` de hai chan nay la output chuc nang BQ active-high.
- `ALERT` set cờ xử lý nhanh qua `BMS_NotifyAlertInterrupt()`; ngoài ra pin ALERT cũng được poll theo chu kỳ và vẫn có fallback 1s để chống miss IRQ.

## 3) Luồng phần cứng trong `BMS_Update`
1. `BMS_UpdateShutdownPulse()` xử lý pulse `SHUT` nếu đã request.
2. `BMS_ReadMeasurements()` đọc dữ liệu BQ cơ bản.
3. `BMS_HandleHardwareSignals()` đồng bộ trạng thái `DCHG/DDSG/ALERT` từ cả IRQ và polling GPIO.
4. `BMS_UpdateBatteryAdc()`:
- bật `BATS_EN`
- chờ settle ngắn
- đọc `BAT_ADC`
- tắt `BATS_EN`
- cập nhật `batAdcRaw`, `batAdcPin_mV`, `batAdcEstimatedPack_mV`
5. `BMS_UpdateState()` gộp thêm `chargeGateFaultSignal/dischargeGateFaultSignal` vào điều kiện disable CHG/DSG.
6. `BMS_ApplyFetPolicy()` phối hợp `FETOFF` + lệnh I2C để đóng/cắt FET.

## 4) API liên quan
- `BMS_NotifyAlertInterrupt()` cho ngắt `ALERT`.
- `BMS_RequestShutdown()` phát xung `SHUT` (>1s).

## 5) Lưu ý
- `BAT_ADC` là ước lượng theo tỉ lệ cầu chia R86/R85 = 665k/13.3k, dùng cho giám sát nhanh.
- Điện áp PACK chính trong `packVoltage` đọc từ direct command BQ; firmware cấu hình BQ trả user-volts theo centivolt rồi scale về mV để tránh tràn trên pack 10S.
- Nếu đo thực tế lệch, cần hiệu chuẩn lại hệ số divider và `Vref` theo board thực.

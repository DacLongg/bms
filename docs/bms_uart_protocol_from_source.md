# Giao thức UART BMS — hiện trạng từ source

> Bản trình bày gọn, phù hợp gửi đối tác và chuyển sang Word: [BMS_UART_Protocol_For_Partner.md](BMS_UART_Protocol_For_Partner.md). Tài liệu này giữ các chi tiết debug và offset của raw struct để đối chiếu source.

Ngày đối chiếu: 28/10/2026. Tài liệu này mô tả **firmware hiện đang triển khai**, không phải đề xuất giao thức `BMS_UART_Communication_Protocol_V1_0.md`. Nguồn chính: `BMS/MyDrivers/bms_uart/bms_uart.c`, `bms_uart.h`, `BMS/Core/Inc/bms_uart_channel.h`, `BMS/Core/Src/usart.c`, `BMS/MyMiddlewares/bms/bms.h` và `bms.c`.

## 1. Cổng vật lý và phạm vi hoạt động

- USART2: TX PA9, RX PA10; **115200 baud, 8N1**, không flow control. Hai phía cần chung GND và mức logic tương thích.
- Cấu hình source mặc định `BMS_UART_PROTOCOL_ENABLE=1`, `BMS_DEBUG_LOG_ENABLE=0`. USART2 không thể vừa truyền protocol vừa debug log. `USART2_SLEEP_BAUDRATE=2400` có khai báo hàm hỗ trợ nhưng **không được gọi trong luồng hiện tại**.
- Khi STM32 vào STOP, USART2 bị deinit/ngắt IRQ; **UART không đánh thức thiết bị** và không thể trả lời lệnh cho tới khi RTC hoặc GPIO khác đánh thức. Trong SHUTDOWN, MCU mất nguồn theo thiết kế phần cứng.
- BMS có thể chủ động phát sự kiện bảo vệ `0x40`; vì vậy Host không được giả định mọi khung nhận được đều là phản hồi yêu cầu.

## 2. Khung truyền

```text
AA 55 | CMD | LEN | PAYLOAD[LEN] | CRC_LO CRC_HI
 2 B  | 1 B | 1 B |    0..160 B  |      2 B
```

Tổng độ dài `LEN + 6` byte; `LEN` tối đa 160. CRC-16/MODBUS khởi tạo `0xFFFF`, đa thức phản chiếu `0xA001`, không XOR cuối; tính trên `CMD, LEN, PAYLOAD`, **không tính `AA 55`**, gửi byte thấp trước. Số nguyên nhiều byte dùng little-endian; số âm dùng bù hai. Không có địa chỉ thiết bị, sequence ID hay trường phiên bản trong khung.

Yêu cầu dùng mã lệnh ở bảng dưới. Phản hồi dùng `CMD_yêu_cầu | 0x80`; **byte đầu tiên của payload phản hồi là `STATUS`**, các byte sau là dữ liệu lệnh. Khung sự kiện `0x40` không có byte `STATUS`.

| STATUS | Tên chung trong source | Ý nghĩa thông thường |
| --- | --- | --- |
| `00` | OK | Thành công |
| `01` | BAD_LENGTH | Độ dài yêu cầu sai |
| `02` | BAD_COMMAND | Mã lệnh không hỗ trợ |
| `03` | BUSY | Dữ liệu đo chưa sẵn sàng ở các lệnh `14`–`19` |
| `04` | INTERNAL_ERROR | Lỗi nội bộ/không thể tạo phản hồi |
| `05` | BAD_PAYLOAD | Payload không hợp lệ |

Ví dụ PING không có dữ liệu: Host `AA 55 01 00 00 20` → BMS `AA 55 81 01 00 20 78`. CRC sai thì firmware bỏ khung và **không gửi lỗi**.

## 3. Danh mục lệnh thực tế

Độ dài ở cột phản hồi là số byte **dữ liệu sau STATUS** khi thành công; toàn bộ `LEN` phản hồi bằng giá trị đó + 1.

| CMD yêu cầu → phản hồi | Chức năng | LEN yêu cầu | Dữ liệu phản hồi |
| --- | --- | ---: | ---: |
| `01 → 81` | PING/echo | 0–159 | Bằng LEN yêu cầu |
| `10 → 90` | READ_SUMMARY | 0 | 124 byte cấu trúc C hiện tại |
| `11 → 91` | READ_CELLS | 0 | 21 byte với 10 cell |
| `12 → 92` | READ_FAULTS | 0 | 8 byte |
| `13 → 93` | READ_LIMITS | 0 | 32 byte |
| `14 → 94` | READ_CURRENT | 0 | 4 byte |
| `15 → 95` | READ_STATUS (SOC ước lượng và trạng thái) | 0 | 4 byte |
| `16 → 96` | READ_PACK_VOLTAGE | 0 | 4 byte |
| `17 → 97` | READ_CELL_COUNT | 0 | 1 byte |
| `18 → 98` | READ_TEMPERATURE | 0 | 2 byte |
| `19 → 99` | READ_CYCLE_COUNT | 0 | 4 byte |
| `20 → A0` | OTP_CHECK | 0 | 27 byte |
| `21 → A1` | OTP_WRITE | 4 | 27 byte |
| `22 → A2` | OTP_READ | 0 | 27 byte |
| `30 → B0` | CALIBRATE_CURRENT | 4 | 12 byte |
| `31 → B1` | SET_BALANCING | 1 | 1 byte |
| `40` | PROTECTION_EVENT, **BMS tự phát** | Không phải yêu cầu | 1 byte, không STATUS |

PING với 160 byte đầu vào được parser chấp nhận nhưng handler trả `BAD_LENGTH`, vì phản hồi echo cần thêm một byte `STATUS`. Mọi lệnh đọc yêu cầu `LEN=0`; sai độ dài trả `STATUS=01`. Mã lệnh không nhận diện trả `STATUS=02` với `CMD_phản_hồi = CMD_yêu_cầu | 80`.

### 3.1. READ_SUMMARY (`10`)

Firmware gửi trực tiếp `sizeof(BMS_Tracking_t)` byte từ RAM, **không tuần tự hóa từng trường**. Với bản build hiện tại, kích thước là **124 byte**; phản hồi `LEN=125`, tổng 131 byte. Các offset dưới đây tính từ **byte dữ liệu đầu tiên sau STATUS**, theo layout GCC/ARM hiện tại:

| Offset | Trường | Kiểu/kích thước |
| ---: | --- | --- |
| 0, 1 | `initialized`, `connected` | bool, mỗi trường 1 B |
| 2, 3 | `state`, `currentDirection` | enum, mỗi trường 1 B trong build hiện tại |
| 4–13 | `cellVoltages.IndexAccumulated[10]` | 10 × u8, bộ đếm lọc nội bộ |
| 14–33 | `cellVoltages.RealTimeAccumulated[10]` | 10 × u16, dữ liệu lọc nội bộ |
| 34–53 | `cellVoltages.cellNum[10]` | 10 × u16, mV |
| 54, 56, 58, 60 | min, max, average, delta cell voltage | 4 × u16, mV |
| 62 | `packVoltage` | u16, mV |
| 64 | `current_mA` | i32, mA |
| 68–71 | `temperature[2]` | 2 × i16, °C theo tracking |
| 72–78 | `charging`, `discharging`, `chargeFetEnabled`, `dischargeFetEnabled`, `fetsEnabled`, `bqChargeFetBlocked`, `bqDischargeFetBlocked` | 7 × bool |
| 79 | padding | Không có ý nghĩa giao thức |
| 80 | `bqAlarmRawStatus.raw` | u16 |
| 82–91 | `faults` | 10 × bool, thứ tự như bitmap ở mục 3.3 |
| 92–97 | `chargeDisabled`, `dischargeDisabled`, `chargeGateFaultSignal`, `dischargeGateFaultSignal`, `fetOffAsserted`, `alertActive` | 6 × bool |
| 98–99 | padding | Không có ý nghĩa giao thức |
| 100 | `alertCounter` | u32 |
| 104–106 | `bqSleepMode`, `bqSleepAllowed`, `batSenseEnabled` | 3 × bool |
| 107 | padding | Không có ý nghĩa giao thức |
| 108 | `batAdcEstimatedPack_mV` | u16, mV |
| 110 | `balanceRequired` | bool |
| 111 | padding | Không có ý nghĩa giao thức |
| 112 | `balanceMask` | u16 |
| 114–115 | padding | Không có ý nghĩa giao thức |
| 116, 120 | `completedCycleCount`, `currentCalibrationGainPpm` | 2 × u32 |

`state`: `0 INIT`, `1 NORMAL`, `2 CHARGE_PROTECT`, `3 DISCHARGE_PROTECT`, `4 FAULT`. `currentDirection`: `0 IDLE`, `1 CHARGE`, `2 DISCHARGE`. `stackVoltage`, `circle_counter` và hai tổng lượng sạc/xả suốt đời đã bị loại khỏi tracking. `completedCycleCount` ở offset 116 là số **chu kỳ xả tương đương đã hoàn tất**; phần dư Q32 để tính chu kỳ kế tiếp chỉ lưu nội bộ và trong flash, không nằm trong `READ_SUMMARY`. Có thể đọc số chu kỳ độc lập bằng lệnh `19`. **Không có `protocolVersion`, `uptime_ms` hay `faultBitmap` trong phản hồi này**, dù tài liệu protocol cũ mô tả như vậy. Trường `BMS_UART_PROTOCOL_VERSION=0x08` chỉ được khai báo, chưa đưa vào payload. Không nên dùng cấu trúc raw này làm ABI ổn định cho Host; padding/layout có thể đổi khi biên dịch hoặc sửa struct.

### 3.2. READ_CELLS (`11`)

```text
STATUS:u8 | cellCount:u8 (=10) | cellVoltage_mV[0..9]:u16[10]
```

`LEN` phản hồi 22; số thứ tự phần tử bắt đầu từ 0 trong mảng, tương ứng cell 1–10. Ví dụ yêu cầu: `AA 55 11 00 0D E0`.

### 3.3. READ_FAULTS (`12`)

```text
STATUS:u8 | faultBitmap:u16 | gateSignalBitmap:u8 | alertActive:u8 | alertCounter:u32
```

`LEN` phản hồi 9. `faultBitmap` bit 0–9 lần lượt: quá áp cell, thấp áp cell, quá nhiệt sạc, quá nhiệt xả, nhiệt độ thấp, quá dòng sạc, quá dòng xả, ngắn mạch, lỗi an toàn BQ, lỗi giao tiếp. `gateSignalBitmap` bit 0 là mức chân DCHG, bit 1 là mức chân DDSG; đây là **mức tín hiệu**, không tự nó là cờ lỗi. Các bit còn lại không được gán.

### 3.4. READ_LIMITS (`13`)

`LEN` phản hồi 33. Dữ liệu sau STATUS theo đúng thứ tự:

| Trường | Kiểu | Giá trị source hiện tại |
| --- | --- | ---: |
| `cellCount`, `thermistorCount` | u8, u8 | 10, 2 |
| `cellOvCutoff_mV`, `cellOvRecover_mV` | u16, u16 | 4150, 4100 |
| `cellUvCutoff_mV`, `cellUvRecover_mV` | u16, u16 | 3550, 3650 |
| `balanceDelta_mV`, `balanceMinCell_mV` | u16, u16 | 30, 3500 |
| `overCurrent_mA`, `shortCircuit_mA` | i32, i32 | 65000, 120000 |
| `chargeOtCutoff_C`, `dischargeOtCutoff_C`, `undertempCutoff_C` | i16 × 3 | 45, 60, 0 |
| `nominalCapacity_mAh` | u32 | 1000 |

Đây là hằng số firmware của lệnh, không nhất thiết là ảnh chụp chính xác các ngưỡng bảo vệ phần cứng BQ đang hoạt động.

### 3.5. OTP_CHECK (`20`), OTP_WRITE (`21`), OTP_READ (`22`)

Ba lệnh đều trả cùng 27 byte dữ liệu sau STATUS, `LEN` phản hồi 28:

```text
otpFlags:u16 | securityState:u8 | checkResult:u8 | checkDataFailAddr:u16 |
writeResult:u8 | writeDataFailAddr:u16 | batteryStatusRaw:u16 |
staticConfigSignature:u16 | stackVoltage_mV:u16 | packVoltage_mV:u16 |
internalTemp_C:i16 | reg0Config:u8 | reg12Control:u8 | daConfig:u8 |
vcellMode:u16 | dchgPinConfig:u8 | ddsgPinConfig:u8 | dfetoffPinConfig:u8
```

`OTP_CHECK` chạy kiểm tra khả năng ghi OTP; `OTP_READ` đọc trạng thái/cấu hình hiện hành, **không đọc trực tiếp dữ liệu OTP thô**. `OTP_WRITE` chỉ nhận chính xác bốn byte `4F 54 50 21` (ASCII `OTP!`), rồi gọi ghi OTP BQ. Đây là thao tác phần cứng khó hoặc không thể đảo ngược; **không thử trên pack sản xuất nếu chưa xác nhận cấu hình và điều kiện nguồn**. Byte magic chỉ chống gửi nhầm, không phải xác thực bảo mật.

`otpFlags` bit 0–15 lần lượt: `fullAccessOk`, `configUpdateOk`, `checkOk`, `writeOk`, `otpBlocked`, `otpPending`, `dchgPinConfigOk`, `ddsgPinConfigOk`, `daConfigUsesCentivolt`, `checkLock`, `checkNoSignature`, `checkNoData`, `checkHighTemp`, `checkLowVoltage`, `checkHighVoltage`, `writeFailed`. `checkResult`/`writeResult` là byte kết quả BQ: bit 7 OK, bit 5 LOCK, bit 4 NOSIG, bit 3 NODATA, bit 2 HT, bit 1 LV, bit 0 HV. **Không chỉ dựa vào STATUS=00 để kết luận OTP đã ghi thành công**; cần xem `otpFlags.writeOk` và `writeResult`.

### 3.6. CALIBRATE_CURRENT (`30`)

Yêu cầu `LEN=4`, `actualCurrent_mA:i32` little-endian. BMS đọc dòng đo từ BQ; giá trị đầu vào phải khác 0 và chênh lệch trị tuyệt đối không vượt 30%. Thành công sẽ ghi hệ số vào BQ và lưu STM32 flash.

Phản hồi hiện tại `LEN=13`: `STATUS:u8 | measured_mA:i32 | deviation_ppm:u32 | newGain_ppm:u32`. **`measured_mA` hiện luôn bằng 0** vì `BMS_CalibrateCurrent()` không gán trường này trong result; không dùng nó làm số đo thực.

Riêng lệnh này, source ép trực tiếp enum kết quả hiệu chuẩn sang `STATUS`, nên giá trị byte thực tế là:

| STATUS thực nhận | Ý nghĩa thực của CALIBRATE_CURRENT | Tên STATUS chung (không phù hợp) |
| --- | --- | --- |
| `00` | Thành công | OK |
| `01` | `actualCurrent_mA=0` | BAD_LENGTH |
| `02` | BQ đo dòng bằng 0 | BAD_COMMAND |
| `03` | Sai lệch dòng >30% | BUSY |
| `04` | Ghi BQ hoặc flash thất bại | INTERNAL_ERROR |

Host cần diễn giải bảng riêng cho lệnh `30`; đây là điểm cần sửa nếu muốn protocol nhất quán.

### 3.7. PROTECTION_EVENT (`40`)

BMS tự phát `AA 55 40 01 reason CRC_LO CRC_HI`, **không có STATUS**. `reason` từ `01` đến `09`: quá áp cell, thấp áp cell, quá nhiệt sạc, quá nhiệt xả, nhiệt độ thấp, quá dòng sạc, quá dòng xả, ngắn mạch, lỗi giao tiếp. Firmware chặn phát lặp cùng nguyên nhân khi cờ lỗi vẫn đang tồn tại; sau khi cờ xóa, lần xuất hiện mới có thể phát lại. Host vẫn nên đọc `READ_FAULTS` để lấy trạng thái hiện tại; sự kiện có thể mất khi Host/MCU đang ngủ hoặc UART lỗi.

### 3.8. READ_CURRENT (`14`)

Yêu cầu `LEN=0`: `AA 55 14 00 0E B0`. Thành công, phản hồi `CMD=94`, `LEN=5`, payload `STATUS=00 | current_mA:i32` little-endian. Theo cấu hình hiện tại `BMS_CURRENT_CHARGE_IS_POSITIVE=1`: **dương là sạc, âm là xả, 0 là gần như không có dòng**. Bốn byte là dòng có dấu từ `tracking.current_mA`, không phải hai giá trị dòng sạc/xả riêng. Nếu BMS chưa khởi tạo hoặc mất kết nối BQ, trả `STATUS=03` và không có dữ liệu dòng.

Ví dụ đọc được `+1000 mA`: `AA 55 94 05 00 E8 03 00 00 0B 3C`; đọc được `−1000 mA`: `AA 55 94 05 00 18 FC FF FF 09 BC`. Quy ước chiều có thể thay đổi nếu sửa `BMS_CURRENT_CHARGE_IS_POSITIVE` trong firmware; Host nên đối chiếu thêm cờ đang sạc/xả của lệnh `15`.

### 3.9. Các lệnh bổ sung theo nhóm mã hiện tại (`15`–`19`, `31`)

Các chức năng này lấy ý tưởng từ tài liệu V1.0, nhưng dùng **khung hiện tại `AA 55`** và nhóm ID hiện tại: `0x10`–`0x19` cho lệnh đọc, `0x20`–`0x22` cho OTP, `0x30`–`0x31` cho lệnh cấu hình/hiệu chuẩn, `0x40` cho sự kiện. Phản hồi có `CMD_yêu_cầu | 0x80` và byte đầu payload là `STATUS`.

| Lệnh | Yêu cầu | Phản hồi khi `STATUS=00` |
| --- | --- | --- |
| `15 → 95` | `LEN=0` | `SOC:u8, stateFlags:u8, faultFlags:u16`; `LEN=5` gồm STATUS |
| `16 → 96` | `LEN=0` | `packVoltage_mV:u32`; `LEN=5` gồm STATUS |
| `17 → 97` | `LEN=0` | `cellCount:u8` (hiện là 10); `LEN=2` gồm STATUS |
| `18 → 98` | `LEN=0` | `temperature_deciC:i16`; `LEN=3` gồm STATUS |
| `19 → 99` | `LEN=0` | `completedCycleCount:u32`; `LEN=5` gồm STATUS |
| `31 → B1` | `LEN=1`, `MODE=00/01` | Echo `MODE:u8`; `LEN=2` gồm STATUS |

`READ_STATUS (15).SOC` là **ước lượng thô từ điện áp cell yếu nhất**: `0%` khi ≤3500 mV, `100%` khi ≥4200 mV, ở giữa nội suy tuyến tính `floor((Vmin−3500)×100/700)`. Nó không phải SOC được hiệu chuẩn theo hóa học pin, dòng tải, nhiệt độ hay tích phân coulomb; khi đang sạc/xả có thể lệch đáng kể. Khi chưa có điện áp cell hợp lệ, BMS trả `STATUS=03` không có dữ liệu.

`READ_STATUS (15).stateFlags` bit 0–5: đang sạc, đang xả, CHG FET bật, DSG FET bật, có cell thực sự đang cân bằng, chế độ cân bằng host đang được cho phép. `faultFlags` bit 0–6: quá áp cell, thấp áp cell, quá dòng sạc, quá dòng xả, quá nhiệt (sạc **hoặc** xả), nhiệt độ thấp, ngắn mạch. Bit 7 “sensor fault” của tài liệu V1.0 hiện luôn 0 vì source không có cờ lỗi cảm biến riêng; lỗi BQ và giao tiếp xem ở `READ_FAULTS (12)`.

`SET_BALANCING (31)` có `MODE=00` tắt host balancing (xóa mask hiện hành), `MODE=01` bật lại thuật toán chọn cell; giá trị khác trả `STATUS=05`. Để **giữ luồng cân bằng hiện tại**, mặc định sau boot là **bật** (khác bản đề xuất V1.0 mặc định tắt). Cờ chế độ chỉ ở RAM, không lưu flash; thao tác bật không ép cân bằng nếu điện áp/nhiệt độ/bảo vệ không cho phép. Cần đọc `READ_STATUS (15).stateFlags` bit 4 để biết đang cân bằng thực tế. Lệnh này không thay đổi BQ hardware protection.

`READ_PACK_VOLTAGE (16)` dùng tổng điện áp 10 cell đã lọc (`tracking.packVoltage`), mở rộng thành u32 little-endian. `READ_TEMPERATURE (18)` dùng cảm biến TS1, nhân giá trị °C nguyên trong tracking với 10 để ra 0,1°C; độ phân giải thực vẫn là **1°C**. Giá trị TS1 ngoài −40 đến 125°C trả `STATUS=03`.

`READ_CYCLE_COUNT (19)` trả số **chu kỳ xả tương đương hoàn tất**, đơn vị **chu kỳ nguyên**. Ví dụ sau khi xả tích lũy 1,2 lần dung lượng danh định, kết quả là `1`; 0,2 chu kỳ còn dư được giữ để tính tiếp, kể cả sau khởi động lại nếu đã lưu flash. Đây không phải số phiên sạc hoàn tất/số lần cắm sạc; không thể suy ra chính xác tổng lượng đã sạc chỉ từ số chu kỳ.

Ví dụ yêu cầu hợp lệ: `15` = `AA 55 15 00 0F 20`; `16` = `AA 55 16 00 0F D0`; `17` = `AA 55 17 00 0E 40`; `18` = `AA 55 18 00 0B B0`; `19` = `AA 55 19 00 0A 20`; `31` bật cân bằng = `AA 55 31 01 01 E0 5F`. Với 10 cell, phản hồi `17` thành công là `AA 55 97 02 00 0A 0D 97`.

## 4. Quy tắc Host và giới hạn hiện tại

- Đồng bộ theo `AA 55`, đọc `LEN` rồi chờ đúng `LEN + 2` byte CRC; kiểm CRC trước khi dùng payload. Parser firmware bỏ khung CRC sai hoặc `LEN>160` mà không phản hồi.
- RX dùng ngắt từng byte và ring buffer 192 byte (chứa tối đa 191 byte chưa xử lý); tràn buffer làm rơi byte. Parser **không có timeout khung dở dang**. Host nên tự đặt timeout, bỏ khung thiếu và đồng bộ lại.
- TX dùng `HAL_UART_Transmit` chặn với timeout 50 ms; source không kiểm tra kết quả truyền và không tự gửi lại. Host nên gửi từng yêu cầu, xử lý sự kiện xen vào, timeout/retry phù hợp cho lệnh chỉ đọc. **Không tự động retry `OTP_WRITE`** khi không biết kết quả trước đó.
- Không có ACK/sequence ID; phản hồi muộn cùng mã lệnh khó phân biệt với phản hồi mới. Không có lệnh `A1` đọc từng cell; dùng `READ_CELLS (11)` để lấy cả 10 cell một lần.
- Tài liệu `docs/BMS_UART_Communication_Protocol_V1_0.md` dùng khung `AA CMD LEN...`, các mã `F0`–`F5`, phản hồi cùng mã và lỗi `FF`. Firmware **không nhận các mã này**; chức năng tương ứng dùng `15`–`19`/`31` trên khung `AA 55` và phản hồi có `STATUS`. Các bản tin mẫu cũ không dùng trực tiếp để kiểm thử firmware hiện tại.

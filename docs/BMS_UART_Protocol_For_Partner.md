# Giao thức UART BMS – tài liệu tích hợp

**Phiên bản tài liệu:** 1.3  
**Ngày đối chiếu firmware:** 28/10/2026  
**Phạm vi:** Các lệnh UART đang có trong firmware hiện tại. Tài liệu được bố trí theo thứ tự CMD tăng dần để dễ chuyển sang Word và tra cứu.

Tất cả mã lệnh, giá trị STATUS và bản tin mẫu viết ở hệ thập lục phân (hex). Tài liệu thiết kế V1.0 cũ dùng khung `AA ...` và các CMD `F0`–`F5`; **không dùng tài liệu đó để giao tiếp với firmware hiện tại**.

## 1. Kết nối UART

| Thông số | Giá trị |
| --- | --- |
| Cổng | USART2; chân TX PA9, RX PA10 |
| Tốc độ | 115200 baud |
| Định dạng | 8 bit dữ liệu, không parity, 1 stop bit (8N1) |
| Điều khiển luồng | Không có |
| Kết nối điện | Hai phía phải dùng mức logic tương thích và chung GND |

MCU không nhận UART khi đang ở chế độ STOP; UART không đánh thức thiết bị. Khi BMS SHUTDOWN và mất nguồn, thiết bị không phản hồi. BMS cũng có thể chủ động gửi sự kiện bảo vệ CMD `40` mà không cần Host hỏi.

## 2. Quy tắc khung truyền

| Thứ tự | Trường | Độ dài | Mô tả |
| ---: | --- | ---: | --- |
| 1 | SOF | 2 byte | Cố định `AA 55` |
| 2 | CMD | 1 byte | Mã lệnh hoặc mã sự kiện |
| 3 | LEN | 1 byte | Số byte DATA, tối đa 160 |
| 4 | DATA | LEN byte | Nội dung lệnh/phản hồi |
| 5 | CRC16 | 2 byte | Byte thấp trước, rồi byte cao |

Tổng độ dài khung là **LEN + 6 byte**. CRC-16/MODBUS khởi tạo `FFFF`, đa thức phản chiếu `A001`, không XOR cuối. CRC tính trên **CMD, LEN, DATA**; không tính `AA 55`. Các số nguyên nhiều byte dùng **little-endian**; số âm dùng bù hai.

CMD phản hồi bằng CMD yêu cầu OR `80`. DATA phản hồi **luôn bắt đầu bằng STATUS**, trừ sự kiện CMD `40` không có STATUS. Ví dụ: yêu cầu `14` có CMD phản hồi `94`. Khung không có địa chỉ thiết bị, số thứ tự yêu cầu hay trường phiên bản; mã phiên bản `0x08` trong source không được truyền qua UART.

| STATUS | Tên | Ý nghĩa |
| --- | --- | --- |
| `00` | OK | Xử lý thành công; đọc các byte dữ liệu sau STATUS |
| `01` | BAD_LENGTH | LEN yêu cầu không đúng |
| `02` | BAD_COMMAND | CMD không được hỗ trợ |
| `03` | BUSY | Dữ liệu chưa sẵn sàng hoặc BMS/BQ chưa kết nối |
| `04` | INTERNAL_ERROR | Lỗi nội bộ |
| `05` | BAD_PAYLOAD | DATA yêu cầu không hợp lệ |

Với lỗi thông thường, phản hồi có LEN=`01` và DATA chỉ gồm STATUS. Khung CRC sai hoặc LEN lớn hơn 160 bị bỏ, **không có phản hồi**. CMD `30` có cách dùng STATUS đặc biệt; xem mục 4.15.

## 3. Danh mục lệnh theo thứ tự CMD

Trong bảng, “DATA khi OK” là số byte **sau STATUS**. LEN phản hồi khi OK bằng số đó cộng 1. Mọi lệnh đọc và kiểm tra có LEN yêu cầu=`00`, trừ khi bảng ghi khác.

| CMD yêu cầu | CMD phản hồi | Tên lệnh | LEN yêu cầu | DATA khi OK |
| --- | --- | --- | ---: | ---: |
| `01` | `81` | PING | 0–159 | Bằng LEN yêu cầu |
| `10` | `90` | READ_SUMMARY | 0 | 124 byte |
| `11` | `91` | READ_CELLS | 0 | 21 byte |
| `12` | `92` | READ_FAULTS | 0 | 8 byte |
| `13` | `93` | READ_LIMITS | 0 | 32 byte |
| `14` | `94` | READ_CURRENT | 0 | 4 byte |
| `15` | `95` | READ_STATUS | 0 | 4 byte |
| `16` | `96` | READ_PACK_VOLTAGE | 0 | 4 byte |
| `17` | `97` | READ_CELL_COUNT | 0 | 1 byte |
| `18` | `98` | READ_TEMPERATURE | 0 | 2 byte |
| `19` | `99` | READ_CYCLE_COUNT | 0 | 4 byte |
| `20` | `A0` | OTP_CHECK | 0 | 27 byte |
| `21` | `A1` | OTP_WRITE | 4 | 27 byte |
| `22` | `A2` | OTP_READ | 0 | 27 byte |
| `30` | `B0` | CALIBRATE_CURRENT | 4 | 12 byte |
| `31` | `B1` | SET_BALANCING | 1 | 1 byte |
| `40` | Không có | PROTECTION_EVENT do BMS tự gửi | Không áp dụng | 1 byte; không có STATUS |

## 4. Định nghĩa từng lệnh theo thứ tự CMD

Mỗi lệnh bên dưới dùng cùng thứ tự: **bảng yêu cầu/phản hồi → bảng dữ liệu → lưu ý xử lý**. “Vị trí dữ liệu” đếm từ **byte đầu tiên sau STATUS** trong phản hồi thành công. Nếu DATA phản hồi là `00 0A`, thì `00` là STATUS và `0A` ở vị trí dữ liệu 0. Riêng sự kiện CMD `40` không có STATUS.

### 4.1. PING – CMD `01`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `01` | `00`–`9F` | Dữ liệu bất kỳ, tối đa 159 byte |
| BMS → Host, thành công | `81` | LEN yêu cầu + 1 | STATUS=`00` + dữ liệu Host đã gửi |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 đến LEN yêu cầu − 1 | `echoData` | u8[] | Sao chép nguyên DATA yêu cầu; nếu LEN=`00` thì không có byte nào sau STATUS |

LEN yêu cầu=`A0` (160 byte) bị từ chối với STATUS=`01` (BAD_LENGTH), vì phản hồi cần thêm một byte STATUS và vượt giới hạn payload 160 byte.

### 4.2. READ_SUMMARY – CMD `10`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `10` | `00` | Không có |
| BMS → Host, thành công | `90` | `7D` | STATUS=`00` + **124 byte raw `BMS_Tracking_t`** |

Bảng sau ghi đầy đủ layout của bản build GCC/ARM hiện tại.

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `initialized` | bool | Tracking đã khởi tạo |
| 1 | `connected` | bool | BQ đang kết nối |
| 2 | `state` | u8 enum | Trạng thái BMS; xem bảng dưới |
| 3 | `currentDirection` | u8 enum | Chiều dòng; xem bảng dưới |
| 4–13 | `cellVoltages.IndexAccumulated[10]` | 10 × u8 | Bộ đếm lọc nội bộ, không phải điện áp |
| 14–33 | `cellVoltages.RealTimeAccumulated[10]` | 10 × u16 | Tổng mẫu lọc nội bộ |
| 34–53 | `cellVoltages.cellNum[10]` | 10 × u16 | Điện áp cell 1–10, mV |
| 54–55 | `minCellVoltage` | u16 | Điện áp cell nhỏ nhất, mV |
| 56–57 | `maxCellVoltage` | u16 | Điện áp cell lớn nhất, mV |
| 58–59 | `averageCellVoltage` | u16 | Điện áp cell trung bình, mV |
| 60–61 | `deltaCellVoltage` | u16 | Chênh lệch cell lớn nhất–nhỏ nhất, mV |
| 62–63 | `packVoltage` | u16 | Tổng điện áp 10 cell, mV |
| 64–67 | `current_mA` | i32 | Dòng điện, mA |
| 68–69 | `temperature[0]` | i16 | TS1, °C nguyên |
| 70–71 | `temperature[1]` | i16 | TS3, °C nguyên |
| 72 | `charging` | bool | Đang sạc |
| 73 | `discharging` | bool | Đang xả |
| 74 | `chargeFetEnabled` | bool | CHG FET bật |
| 75 | `dischargeFetEnabled` | bool | DSG FET bật |
| 76 | `fetsEnabled` | bool | Chế độ FET được bật |
| 77 | `bqChargeFetBlocked` | bool | BQ chặn CHG FET |
| 78 | `bqDischargeFetBlocked` | bool | BQ chặn DSG FET |
| 79 | Padding | 1 byte | Không có ý nghĩa giao thức |
| 80–81 | `bqAlarmRawStatus.raw` | u16 | Thanh ghi cảnh báo thô của BQ |
| 82–91 | `faults` | 10 × bool | Thứ tự 10 lỗi giống bit 0–9 của `faultBitmap` ở mục 4.4 |
| 92 | `chargeDisabled` | bool | Đường sạc bị vô hiệu hóa |
| 93 | `dischargeDisabled` | bool | Đường xả bị vô hiệu hóa |
| 94 | `chargeGateFaultSignal` | bool | Mức chân DCHG thô; tên trường là tên cũ |
| 95 | `dischargeGateFaultSignal` | bool | Mức chân DDSG thô; tên trường là tên cũ |
| 96 | `fetOffAsserted` | bool | Tín hiệu FETOFF đang được áp dụng |
| 97 | `alertActive` | bool | Cảnh báo đang hoạt động |
| 98–99 | Padding | 2 byte | Không có ý nghĩa giao thức |
| 100–103 | `alertCounter` | u32 | Bộ đếm ngắt ALERT |
| 104 | `bqSleepMode` | bool | BQ báo đang ở SLEEP |
| 105 | `bqSleepAllowed` | bool | BQ cho phép SLEEP |
| 106 | `batSenseEnabled` | bool | Đường đo BAT đang bật |
| 107 | Padding | 1 byte | Không có ý nghĩa giao thức |
| 108–109 | `batAdcEstimatedPack_mV` | u16 | Điện áp pack ước lượng từ ADC, mV |
| 110 | `balanceRequired` | bool | Thuật toán yêu cầu cân bằng |
| 111 | Padding | 1 byte | Không có ý nghĩa giao thức |
| 112–113 | `balanceMask` | u16 | Mặt nạ cell logic được chọn cân bằng; bit 0–9 tương ứng cell 1–10, không phải số chân VC của BQ |
| 114–115 | Padding | 2 byte | Không có ý nghĩa giao thức |
| 116–119 | `completedCycleCount` | u32 | Số chu kỳ xả tương đương đã hoàn tất |
| 120–123 | `currentCalibrationGainPpm` | u32 | Hệ số hiệu chuẩn dòng, ppm |

| `state` | Ý nghĩa | `currentDirection` | Ý nghĩa |
| ---: | --- | ---: | --- |
| 0 | INIT | 0 | IDLE |
| 1 | NORMAL | 1 | CHARGE |
| 2 | CHARGE_PROTECT | 2 | DISCHARGE |
| 3 | DISCHARGE_PROTECT | — | — |
| 4 | FAULT | — | — |

**Lưu ý tương thích:** Đây là raw struct, không được tuần tự hóa theo một ABI độc lập. Offset và padding có thể đổi khi sửa firmware hoặc đổi compiler. Host có thể dùng bảng trên cho đúng bản build hiện tại, nhưng với sản phẩm cần tương thích lâu dài nên dùng các lệnh có payload riêng `11`–`19`. Lệnh `10` không tự trả BUSY khi các phép đo chưa đủ mẫu; các giá trị mới khởi động có thể bằng 0. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.3. READ_CELLS – CMD `11`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `11` | `00` | Không có |
| BMS → Host, thành công | `91` | `16` | STATUS=`00` + 21 byte dữ liệu |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `cellCount` | u8 | Số cell; hiện là 10 (`0A`) |
| 1–20 | `cellVoltage_mV[10]` | 10 × u16 | Điện áp cell 1–10, mỗi cell 2 byte little-endian, đơn vị mV |

Khi bộ lọc điện áp chưa đủ mẫu, một hoặc nhiều giá trị cell có thể bằng 0; lệnh này không tự trả BUSY. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.4. READ_FAULTS – CMD `12`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `12` | `00` | Không có |
| BMS → Host, thành công | `92` | `09` | STATUS=`00` + 8 byte dữ liệu |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–1 | `faultBitmap` | u16 | Các lỗi hiện tại; xem bảng bit bên dưới |
| 2 | `gateSignalBitmap` | u8 | Bit 0: mức DCHG; bit 1: mức DDSG |
| 3 | `alertActive` | u8 | 0 hoặc 1 |
| 4–7 | `alertCounter` | u32 | Bộ đếm ngắt ALERT |

| Bit `faultBitmap` | Mặt nạ | Lỗi khi bit = 1 |
| ---: | --- | --- |
| 0 | `0001` | Quá áp cell |
| 1 | `0002` | Thấp áp cell |
| 2 | `0004` | Quá nhiệt sạc |
| 3 | `0008` | Quá nhiệt xả |
| 4 | `0010` | Nhiệt độ thấp |
| 5 | `0020` | Quá dòng sạc |
| 6 | `0040` | Quá dòng xả |
| 7 | `0080` | Ngắn mạch |
| 8 | `0100` | Lỗi an toàn do BQ báo |
| 9 | `0200` | Lỗi giao tiếp BQ |

| Bit `gateSignalBitmap` | Mặt nạ | Ý nghĩa khi bit = 1 |
| ---: | --- | --- |
| 0 | `01` | Chân DCHG ở mức cao; đầu ra CHG FET của BQ bị vô hiệu hóa |
| 1 | `02` | Chân DDSG ở mức cao; đầu ra DSG FET của BQ bị vô hiệu hóa |

`gateSignalBitmap` thể hiện **mức chân**, không tự nó kết luận có lỗi FET. `faultBitmap` là lỗi tại thời điểm đọc, không phải lịch sử lỗi. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.5. READ_LIMITS – CMD `13`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `13` | `00` | Không có |
| BMS → Host, thành công | `93` | `21` | STATUS=`00` + 32 byte dữ liệu |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `cellCount` | u8 | 10 |
| 1 | `thermistorCount` | u8 | 2 |
| 2–3 | `cellOvCutoff_mV` | u16 | 4150 mV |
| 4–5 | `cellOvRecover_mV` | u16 | 4100 mV |
| 6–7 | `cellUvCutoff_mV` | u16 | 3550 mV |
| 8–9 | `cellUvRecover_mV` | u16 | 3650 mV |
| 10–11 | `balanceDelta_mV` | u16 | 30 mV |
| 12–13 | `balanceMinCell_mV` | u16 | 3500 mV |
| 14–17 | `overCurrent_mA` | i32 | 65000 mA |
| 18–21 | `shortCircuit_mA` | i32 | 120000 mA |
| 22–23 | `chargeOtCutoff_C` | i16 | 45 °C |
| 24–25 | `dischargeOtCutoff_C` | i16 | 60 °C |
| 26–27 | `undertempCutoff_C` | i16 | 0 °C |
| 28–31 | `nominalCapacity_mAh` | u32 | 1000 mAh |

Các giá trị trên là hằng số firmware báo ra, **không phải** bản đọc lại toàn bộ cấu hình bảo vệ hiện hành trong IC BQ. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.6. READ_CURRENT – CMD `14`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `14` | `00` | Không có |
| BMS → Host, thành công | `94` | `05` | STATUS=`00` + 4 byte dòng điện |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–3 | `current_mA` | i32 | mA; dương: sạc, âm: xả, gần 0: dòng nhỏ/không tải |

Ví dụ +1000 mA là `E8 03 00 00`; −1000 mA là `18 FC FF FF`. Nếu BMS chưa khởi tạo hoặc mất kết nối BQ, lệnh trả STATUS=`03` (BUSY). LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH). Quy ước dấu cần kiểm tra với phần cứng thực tế.

### 4.7. READ_STATUS – CMD `15`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `15` | `00` | Không có |
| BMS → Host, thành công | `95` | `05` | STATUS=`00` + 4 byte dữ liệu |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `socPercent` | u8 | SOC ước lượng, 0–100% |
| 1 | `stateFlags` | u8 | Cờ trạng thái; xem bảng dưới |
| 2–3 | `faultFlags` | u16 | Cờ lỗi rút gọn; xem bảng dưới |

SOC được ước lượng từ **cell có điện áp thấp nhất**: ≤3500 mV là 0%, ≥4200 mV là 100%, ở giữa nội suy tuyến tính. Đây không phải SOC được đo bằng tích phân dòng; khi đang sạc/xả có thể lệch đáng kể. Nếu chưa có điện áp cell hợp lệ, lệnh trả BUSY.

| Bit `stateFlags` | Mặt nạ | Trạng thái khi bit = 1 |
| ---: | --- | --- |
| 0 | `01` | Đang sạc |
| 1 | `02` | Đang xả |
| 2 | `04` | CHG FET bật |
| 3 | `08` | DSG FET bật |
| 4 | `10` | Có cell được chọn cân bằng (`balanceMask ≠ 0`) |
| 5 | `20` | Cho phép thuật toán cân bằng |

| Bit `faultFlags` | Mặt nạ | Lỗi khi bit = 1 |
| ---: | --- | --- |
| 0 | `0001` | Quá áp cell |
| 1 | `0002` | Thấp áp cell |
| 2 | `0004` | Quá dòng sạc |
| 3 | `0008` | Quá dòng xả |
| 4 | `0010` | Quá nhiệt sạc **hoặc** xả |
| 5 | `0020` | Nhiệt độ thấp |
| 6 | `0040` | Ngắn mạch |

Lỗi an toàn BQ và lỗi giao tiếp không có bit riêng ở đây; đọc lệnh `12` để có đủ cờ lỗi. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.8. READ_PACK_VOLTAGE – CMD `16`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `16` | `00` | Không có |
| BMS → Host, thành công | `96` | `05` | STATUS=`00` + 4 byte điện áp |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–3 | `packVoltage_mV` | u32 | Điện áp mV, little-endian; ví dụ `88 90 00 00` = 37000 mV = 37 V |

Giá trị là **tổng điện áp 10 cell đã lọc** trong `BMS_Tracking_t.packVoltage`, không phải phép đo chân PACK của BQ. Trường nội bộ hiện là u16 và được đưa lên 4 byte trong phản hồi. Khi BMS chưa khởi tạo, mất kết nối BQ hoặc tổng điện áp chưa hợp lệ (`packVoltage=0`), phản hồi LEN=`01`, STATUS=`03` (BUSY). LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.9. READ_CELL_COUNT – CMD `17`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `17` | `00` | Không có |
| BMS → Host, thành công | `97` | `02` | STATUS=`00` + 1 byte số cell |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `cellCount` | u8 | Số cell logic; `0A` = 10 cell |

Đây là **số cell logic được cấu hình cố định trong firmware**, không phải số cell được phát hiện động từ IC BQ. Host dùng giá trị này để giải mã số phần tử của lệnh `READ_CELLS (11)`. Lệnh không kiểm tra trạng thái kết nối BQ; nếu MCU còn nhận UART thì vẫn có thể trả `0A`. LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.10. READ_TEMPERATURE – CMD `18`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `18` | `00` | Không có |
| BMS → Host, thành công | `98` | `03` | STATUS=`00` + 2 byte nhiệt độ |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–1 | `temperature_deciC` | i16 | Nhiệt độ 0,1 °C, little-endian có dấu; chia cho 10 để ra °C |

Nguồn đo là **TS1** (`temperature[0]`), không phải TS3 hay nhiệt độ trong IC BQ. Firmware đang lưu nhiệt độ °C nguyên rồi nhân 10 khi trả lời, nên `22 01` = 290 = **29 °C**, còn `CE FF` = −50 = **−5 °C**; các chữ số lẻ 0,1 °C không có độ phân giải thực trong lệnh này. TS3 chỉ có trong `READ_SUMMARY (10)`. Nếu BMS chưa khởi tạo, mất kết nối BQ hoặc TS1 nằm ngoài **−40 đến 125 °C**, phản hồi LEN=`01`, STATUS=`03` (BUSY). LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

### 4.11. READ_CYCLE_COUNT – CMD `19`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `19` | `00` | Không có |
| BMS → Host, thành công | `99` | `05` | STATUS=`00` + 4 byte số chu kỳ |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–3 | `completedCycleCount` | u32 | Chu kỳ xả tương đương **đã hoàn tất**, little-endian; `01 00 00 00` = 1 chu kỳ |

Firmware cộng **điện lượng xả** từ bộ đếm Passed Charge của BQ. Cứ xả tích lũy đủ dung lượng danh định hiện cấu hình **1000 mAh**, bộ đếm tăng 1; không cần xả hết trong một phiên. Phần chưa đủ một chu kỳ được giữ riêng và lưu flash theo cơ chế lưu định kỳ, **không có trong phản hồi**. Ví dụ đã xả tích lũy 1200 mAh thì lệnh trả `1`, còn phần tương đương 200 mAh được giữ để tính tiếp. Lệnh **không đếm phiên sạc**, không cộng lượng sạc, không trả SOC/SOH và không thể dùng số chu kỳ để suy ra chính xác lượng đã sạc.

Nếu BMS chưa khởi tạo hoặc mất kết nối BQ, phản hồi LEN=`01`, STATUS=`03` (BUSY). LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH). Khi thiết bị vừa khởi động, giá trị trả về là bộ đếm đã khôi phục từ flash cộng phần cập nhật sau đó; phần điện lượng chưa kịp lưu trước khi mất nguồn có thể không được giữ lại.

### 4.12. OTP_CHECK – CMD `20`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `20` | `00` | Không có |
| BMS → Host, khung hợp lệ | `A0` | `1C` | STATUS=`00` + 27 byte trạng thái OTP |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–1 | `otpFlags` | u16 | Cần xem bit 0 (Full Access), 1 (Config Update), 2 (`checkOk`) và 4 (`otpBlocked`) |
| 3 | `checkResult` | u8 | Các bit OK/lỗi do BQ trả về |
| 4–5 | `checkDataFailAddr` | u16 | Địa chỉ lỗi nếu BQ báo lỗi dữ liệu |

Lệnh yêu cầu BQ chuyển quyền **Full Access**, cấu hình nguồn REG cần cho OTP, vào **Config Update**, chạy kiểm tra `OTP_WR_CHECK`, rồi thoát Config Update và đọc lại trạng thái. Vì vậy đây **không phải lệnh chỉ đọc thụ động**. Không dùng khi thiết bị đang vận hành tải mà việc đổi trạng thái BQ có thể ảnh hưởng hệ thống.

STATUS=`00` chỉ nói firmware đã xử lý yêu cầu và gửi bản tin; **không có nghĩa kiểm tra OTP đã đạt**. Phải kiểm `checkOk=1`, `otpBlocked=0` và xem `checkResult`. Nếu kiểm tra thất bại, firmware vẫn có thể trả khung 27 byte với STATUS=`00`. LEN yêu cầu khác `00` trả khung lỗi LEN=`01`, STATUS=`01` (BAD_LENGTH). Layout đầy đủ 27 byte ở mục **4.14.1**, bảng bit `otpFlags` ở **4.14.2** và `checkResult` ở **4.14.3**.

### 4.13. OTP_WRITE – CMD `21`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `21` | `04` | `4F 54 50 21` (ASCII `OTP!`) |
| BMS → Host, khung hợp lệ | `A1` | `1C` | STATUS=`00` + 27 byte trạng thái OTP |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–1 | `otpFlags` | u16 | Cần xem bit 2 (`checkOk`), 3 (`writeOk`) và 4 (`otpBlocked`); bit 3 là **cờ kết luận ghi thành công** |
| 3 | `checkResult` | u8 | Kết quả bước kiểm tra trước ghi |
| 4–5 | `checkDataFailAddr` | u16 | Địa chỉ lỗi khi kiểm tra |
| 6 | `writeResult` | u8 | Kết quả bước ghi do BQ trả về; phải xem cùng `writeOk` |
| 7–8 | `writeDataFailAddr` | u16 | Địa chỉ lỗi khi ghi |

Firmware thực hiện kiểm tra điều kiện tương tự CMD `20`; **chỉ gửi lệnh ghi OTP xuống BQ nếu kết quả kiểm tra đạt và không bị chặn**. Đây là thao tác phần cứng **khó hoặc không thể hoàn tác**. Chỉ thực hiện theo quy trình đã phê duyệt, khi đã xác nhận đúng cấu hình, điện áp, nhiệt độ và nguồn cấp. Chuỗi `OTP!` chỉ giảm thao tác nhầm, **không phải xác thực bảo mật**.

STATUS=`00` **không xác nhận OTP đã ghi thành công**: ngay cả khi điều kiện không đạt và firmware bỏ qua bước ghi, phản hồi vẫn có thể là STATUS=`00`. Host chỉ kết luận thành công khi `writeOk=1`, đồng thời kiểm `writeResult` có bit OK và không có bit lỗi. LEN khác `04` trả khung lỗi LEN=`01`, STATUS=`01` (BAD_LENGTH); DATA 4 byte không đúng `OTP!` trả LEN=`01`, STATUS=`05` (BAD_PAYLOAD). **Không tự động gửi lại CMD `21`** nếu chưa xác định kết quả lần trước. Layout đầy đủ 27 byte ở mục **4.14.1**, bảng bit ở **4.14.2–4.14.3**.

### 4.14. OTP_READ – CMD `22`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `22` | `00` | Không có |
| BMS → Host, thành công | `A2` | `1C` | STATUS=`00` + 27 byte trạng thái OTP |

#### 4.14.1. Payload phản hồi chung của CMD `20`, `21`, `22`

Vị trí dưới đây là byte sau STATUS. Trường nhiều byte dùng little-endian.

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–1 | `otpFlags` | u16 | Các cờ OTP; xem mục 4.14.2 |
| 2 | `securityState` | u8 | Trạng thái bảo mật BQ |
| 3 | `checkResult` | u8 | Kết quả kiểm tra OTP; xem mục 4.14.3 |
| 4–5 | `checkDataFailAddr` | u16 | Địa chỉ lỗi khi kiểm tra dữ liệu |
| 6 | `writeResult` | u8 | Kết quả ghi OTP; xem mục 4.14.3 |
| 7–8 | `writeDataFailAddr` | u16 | Địa chỉ lỗi khi ghi |
| 9–10 | `batteryStatusRaw` | u16 | Thanh ghi Battery Status thô |
| 11–12 | `staticConfigSignature` | u16 | Chữ ký cấu hình tĩnh |
| 13–14 | `stackVoltage_mV` | u16 | Điện áp stack do BQ báo, mV |
| 15–16 | `packVoltage_mV` | u16 | Điện áp pack do BQ báo, mV |
| 17–18 | `internalTemp_C` | i16 | Nhiệt độ bên trong BQ, °C |
| 19 | `reg0Config` | u8 | Cấu hình REG0 |
| 20 | `reg12Control` | u8 | Điều khiển REG1/REG2 |
| 21 | `daConfig` | u8 | Cấu hình DA |
| 22–23 | `vcellMode` | u16 | Mặt nạ cell BQ đang cấu hình |
| 24 | `dchgPinConfig` | u8 | Cấu hình chân DCHG |
| 25 | `ddsgPinConfig` | u8 | Cấu hình chân DDSG |
| 26 | `dfetoffPinConfig` | u8 | Cấu hình chân DFETOFF |

Lệnh `OTP_READ (22)` đọc ảnh chụp trạng thái/cấu hình, **không** đọc dữ liệu OTP thô và **không chạy lại** bước kiểm tra hoặc ghi OTP. Trong handler hiện tại, `checkResult`, `writeResult` và các địa chỉ lỗi được khởi tạo bằng 0 trước khi đọc ảnh chụp; giá trị 0 **không chứng minh** lần kiểm tra/ghi trước thành công. Muốn kiểm tra điều kiện ghi mới thì dùng CMD `20`; muốn biết kết quả một lần ghi thì xem phản hồi CMD `21` của chính lần ghi đó. Nếu nhánh đọc trạng thái báo lỗi, phản hồi LEN=`01`, STATUS=`04` (INTERNAL_ERROR); LEN yêu cầu khác `00` trả STATUS=`01` (BAD_LENGTH).

#### 4.14.2. Bảng bit `otpFlags` (u16)

Giá trị bit = 1 khi điều kiện ở cột cuối đúng; bit 0 là bit thấp nhất của byte tại vị trí dữ liệu 0.

| Bit | Mặt nạ | Tên trong source | Ý nghĩa khi bit = 1 |
| ---: | --- | --- | --- |
| 0 | `0001` | `fullAccessOk` | BQ ở quyền Full Access phù hợp thao tác OTP |
| 1 | `0002` | `configUpdateOk` | BQ ở trạng thái Config Update phù hợp |
| 2 | `0004` | `checkOk` | Kiểm tra điều kiện ghi OTP đạt |
| 3 | `0008` | `writeOk` | BQ báo ghi OTP thành công |
| 4 | `0010` | `otpBlocked` | Ghi OTP đang bị chặn |
| 5 | `0020` | `otpPending` | Có trạng thái OTP đang chờ xử lý |
| 6 | `0040` | `dchgPinConfigOk` | Cấu hình DCHG là active-high theo kiểm tra firmware |
| 7 | `0080` | `ddsgPinConfigOk` | Cấu hình DDSG là active-high theo kiểm tra firmware |
| 8 | `0100` | `daConfigUsesCentivolt` | DA Config chọn đơn vị centivolt |
| 9 | `0200` | `checkLock` | BQ báo lỗi LOCK khi kiểm tra |
| 10 | `0400` | `checkNoSignature` | BQ báo thiếu chữ ký cấu hình |
| 11 | `0800` | `checkNoData` | BQ báo không có dữ liệu để ghi |
| 12 | `1000` | `checkHighTemp` | BQ báo nhiệt độ quá cao |
| 13 | `2000` | `checkLowVoltage` | BQ báo điện áp quá thấp |
| 14 | `4000` | `checkHighVoltage` | BQ báo điện áp quá cao |
| 15 | `8000` | `writeFailed` | Có kết quả ghi khác 0 nhưng không có bit OK |

#### 4.14.3. Bảng bit `checkResult` và `writeResult` (u8)

Hai trường kết quả dùng cùng các bit sau. Bit không liệt kê là bit dự phòng/không được tài liệu này giải mã.

| Bit | Mặt nạ | Tên BQ | Ý nghĩa khi bit = 1 |
| ---: | --- | --- | --- |
| 0 | `01` | HV | Điện áp quá cao |
| 1 | `02` | LV | Điện áp quá thấp |
| 2 | `04` | HT | Nhiệt độ quá cao |
| 3 | `08` | NODATA | Không có dữ liệu |
| 4 | `10` | NOSIG | Thiếu chữ ký |
| 5 | `20` | LOCK | Bị khóa |
| 7 | `80` | OK | Thao tác tương ứng đạt/thành công |

### 4.15. CALIBRATE_CURRENT – CMD `30`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `30` | `04` | `actualCurrent_mA:i32`, little-endian |
| BMS → Host, đã xử lý | `B0` | `0D` | STATUS kết quả hiệu chuẩn + 12 byte dữ liệu |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0–3 | `measured_mA` | i32 | Hiện **luôn bằng 0** do firmware chưa gán trường này; không dùng để hiển thị dòng đo |
| 4–7 | `deviation_ppm` | u32 | Độ lệch tương đối, ppm |
| 8–11 | `newGain_ppm` | u32 | Hệ số hiệu chuẩn mới, ppm |

Dòng thực nhập vào phải khác 0; firmware từ chối khi độ lệch trị tuyệt đối dòng BQ đo được lớn hơn 30%. Khi hiệu chuẩn thành công, hệ số được ghi xuống BQ và lưu flash STM32.

Riêng CMD `30`, STATUS trong khung có LEN=`0D` là mã kết quả hiệu chuẩn, **khác ý nghĩa bảng STATUS chung**:

| STATUS | Kết quả hiệu chuẩn |
| --- | --- |
| `00` | Thành công |
| `01` | `actualCurrent_mA` bằng 0 |
| `02` | BQ đo dòng bằng 0 |
| `03` | Độ lệch dòng >30% |
| `04` | Ghi hệ số xuống BQ hoặc flash thất bại |

Nếu yêu cầu sai độ dài, firmware trả khung lỗi thông thường LEN=`01`, STATUS=`01`, **không có** 12 byte kết quả. Host phải xét cả LEN để phân biệt.

### 4.16. SET_BALANCING – CMD `31`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| Host → BMS | `31` | `01` | `MODE:u8`, chỉ chấp nhận `00` hoặc `01` |
| BMS → Host, thành công | `B1` | `02` | STATUS=`00` + MODE đã nhận |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 | `MODE` | u8 | `00`: tắt thuật toán cân bằng và xóa mặt nạ hiện tại; `01`: cho phép thuật toán cân bằng |

MODE khác `00`/`01` trả STATUS=`05` (BAD_PAYLOAD); LEN khác `01` trả STATUS=`01` (BAD_LENGTH). Mặc định sau bật nguồn là **cho phép cân bằng**. MODE chỉ ở RAM, không lưu flash. MODE=`01` không ép cell cân bằng nếu điều kiện điện áp, nhiệt độ hoặc bảo vệ không cho phép. Lệnh này không thay đổi bảo vệ phần cứng BQ. Có thể xem bit 4 và 5 của `stateFlags` ở CMD `15` để phân biệt “đang chọn cell cân bằng” và “đang cho phép thuật toán”.

### 4.17. PROTECTION_EVENT – CMD `40`

| Chiều truyền | CMD | LEN | DATA |
| --- | --- | --- | --- |
| BMS → Host, tự phát | `40` | `01` | `reason:u8`; **không có STATUS** |

| Vị trí dữ liệu | Trường | Kiểu | Ý nghĩa/đơn vị |
| --- | --- | --- | --- |
| 0 của DATA sự kiện | `reason` | u8 | Mã nguyên nhân bảo vệ; xem bảng dưới |

| reason | Ý nghĩa |
| --- | --- |
| `01` | Quá áp cell |
| `02` | Thấp áp cell |
| `03` | Quá nhiệt sạc |
| `04` | Quá nhiệt xả |
| `05` | Nhiệt độ thấp |
| `06` | Quá dòng sạc |
| `07` | Quá dòng xả |
| `08` | Ngắn mạch |
| `09` | Lỗi giao tiếp BQ |

Sự kiện có thể xen giữa lúc Host chờ phản hồi lệnh khác. Sau khi nhận, Host nên đọc CMD `12` để biết các cờ lỗi hiện tại. Sự kiện có thể mất khi hai bên đang ngủ hoặc đường UART lỗi.

## 5. Ví dụ bản tin hoàn chỉnh

Mỗi chuỗi dưới đây đã gồm SOF, CMD, LEN, DATA và CRC16. Các ví dụ sắp theo CMD tăng dần.

| CMD | Trường hợp | Bản tin |
| --- | --- | --- |
| `01` | Host gửi PING rỗng | `AA 55 01 00 00 20` |
| `01` | BMS phản hồi PING | `AA 55 81 01 00 20 78` |
| `11` | Host đọc 10 cell | `AA 55 11 00 0D E0` |
| `14` | Host đọc dòng | `AA 55 14 00 0E B0` |
| `14` | BMS trả +1000 mA | `AA 55 94 05 00 E8 03 00 00 0B 3C` |
| `16` | Host đọc điện áp pack | `AA 55 16 00 0F D0` |
| `16` | BMS trả 37000 mV | `AA 55 96 05 00 88 90 00 00 C6 D1` |
| `17` | Host đọc số cell | `AA 55 17 00 0E 40` |
| `17` | BMS trả 10 cell | `AA 55 97 02 00 0A 0D 97` |
| `18` | Host đọc nhiệt độ TS1 | `AA 55 18 00 0B B0` |
| `18` | BMS trả 29 °C | `AA 55 98 03 00 22 01 DC F8` |
| `19` | Host đọc số chu kỳ | `AA 55 19 00 0A 20` |
| `19` | BMS trả 1 chu kỳ | `AA 55 99 05 00 01 00 00 00 13 A0` |
| `20` | Host kiểm tra điều kiện OTP | `AA 55 20 00 18 70` |
| `31` | Host tắt cân bằng | `AA 55 31 01 00 21 9F` |
| `31` | BMS xác nhận đã tắt | `AA 55 B1 02 00 00 86 D8` |
| `40` | BMS báo thấp áp cell | `AA 55 40 01 02 F0 45` |

## 6. Quy tắc triển khai phía Host

1. Đồng bộ theo `AA 55`, đọc CMD và LEN, chờ đủ LEN byte DATA cùng hai byte CRC rồi mới giải mã. Nếu khung thiếu hoặc sai CRC, bỏ khung và đồng bộ lại.
2. Với phản hồi, kiểm CMD bằng CMD đã gửi OR `80`, sau đó kiểm STATUS. CMD `40` là sự kiện độc lập, có thể đến xen giữa và không có STATUS.
3. Chỉ có tối đa một yêu cầu đang chờ phản hồi. Đặt timeout ở Host; firmware không có sequence ID, ACK hay tự gửi lại.
4. Chỉ thử lại tự động các lệnh đọc khi thích hợp. **Không tự động thử lại OTP_WRITE `21`** nếu chưa rõ lệnh trước đã ghi thành công hay chưa.
5. UART không đánh thức MCU khỏi STOP. Các lệnh đọc có thể không được trả lời khi BMS ngủ; `READ_SUMMARY (10)` và `READ_CELLS (11)` có thể có số 0 trước khi mẫu đo ổn định. SOC `15` chỉ là ước lượng theo điện áp.

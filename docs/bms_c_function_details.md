# BMS.c - Mô tả chi tiết từng hàm

Tài liệu này mô tả cách hoạt động của tất cả hàm trong file `BMS/MyMiddlewares/bms/bms.c`.

## 1. `void BMS_Init(void)`
- Mục đích: Khởi tạo toàn bộ module BMS và IC giám sát `bq76952`.
- Luồng xử lý:
1. Gọi `BMS_ResetTracking()` để đưa toàn bộ biến tracking về mặc định an toàn.
2. Gọi `bq76952_init()` để khởi tạo phần cứng BQ.
3. Kiểm tra kết nối bằng `bq76952_isConnected()`.
4. Nếu mất kết nối: set lỗi và gọi `BMS_Error_Handler()`, dừng khởi tạo.
5. Nếu kết nối OK: nạp dữ liệu tích lũy từ flash (`BMS_LoadPersistedData`).
6. Cấu hình toàn bộ ngưỡng/protection của BQ (`BMS_ConfigureMonitor`).
7. Khởi tạo các mốc thời gian (`g_last_update_tick`, `g_last_balance_tick`, `g_last_flash_save_tick`).
8. Gọi `BMS_Update()` ngay để có bộ dữ liệu đo đầu tiên.
- Tác dụng phụ:
- Thay đổi trạng thái toàn cục `g_bms_tracking`.
- Có thể bật/tắt FET thông qua `BMS_Update` nếu có lỗi.

## 2. `void BMS_Update(void)`
- Mục đích: Hàm vòng lặp chính, cập nhật toàn bộ trạng thái BMS theo chu kỳ.
- Luồng xử lý:
1. Lấy `now = HAL_GetTick()`, tính `dt_ms = now - g_last_update_tick`.
2. Chặn trường hợp `dt_ms == 0` bằng cách ép `dt_ms = 1` để tránh mất mẫu coulomb.
3. Kiểm tra kết nối BQ:
- Nếu mất kết nối: set `communicationFault`, gọi `BMS_Error_Handler()`, return.
4. Khi kết nối tốt, gọi tuần tự:
- `BMS_ReadMeasurements()`
- `BMS_UpdateCellStatistics()`
- `BMS_UpdateCurrentDirection()`
- `BMS_UpdateFaultFlags()`
- `BMS_MergeBQFaultFlags()`
- `BMS_UpdateCoulombCounter(dt_ms)`
- `BMS_UpdateState()`
- `BMS_ApplyFetPolicy()`
- `BMS_UpdateBalancing(now)`
- `BMS_SavePersistedDataIfNeeded(now)`
5. Tăng `circle_counter`, đánh dấu `initialized = true`.
- Ý nghĩa: Đây là nơi gom đủ sensor + bảo vệ + điều khiển FET + cân bằng + lưu flash.

## 3. `const BMS_Tracking_t *BMS_GetTracking(void)`
- Mục đích: Trả về con trỏ đọc-only tới trạng thái tracking hiện tại.
- Input: Không có.
- Output: `&g_bms_tracking`.
- Lưu ý: Dữ liệu có thể thay đổi sau mỗi lần `BMS_Update()`.

## 4. `bool BMS_IsFaultActive(void)`
- Mục đích: Kiểm tra nhanh có bất kỳ lỗi quan trọng nào đang active không.
- Cách làm: OR tất cả cờ lỗi trong `g_bms_tracking.faults`:
- `cellOverVoltage`, `cellUnderVoltage`, `chargeOverTemperature`, `dischargeOverTemperature`, `underTemperature`, `chargeOverCurrent`, `dischargeOverCurrent`, `shortCircuit`, `bqSafetyFault`, `communicationFault`.
- Output:
- `true`: Có lỗi.
- `false`: Không có lỗi.

## 5. `void BMS_Error_Handler(void)`
- Mục đích: Đưa hệ thống về trạng thái an toàn khi lỗi nghiêm trọng (đặc biệt lỗi giao tiếp).
- Luồng xử lý:
1. Set `connected = false`.
2. Disable toàn bộ FET state logic (`fetsEnabled`, `chargeFetEnabled`, `dischargeFetEnabled` = false).
3. Đặt `chargeDisabled = true`, `dischargeDisabled = true`.
4. Set state về `BMS_STATE_FAULT`.
5. Tắt cân bằng (`balanceMask = 0`, `balanceRequired = false`).
6. Gửi lệnh xuống BQ:
- `bq76952_setCellBalanceMask(0U)` để tắt balancing.
- `bq76952_setFET(ALL, OFF)` để tắt cả CHG/DCH FET.
- Ý nghĩa: Fail-safe cứng, ưu tiên an toàn pin.

## 6. `static void BMS_ResetTracking(void)`
- Mục đích: Khởi tạo tất cả field trong `g_bms_tracking` về giá trị mặc định.
- Cách làm:
- Zero mảng điện áp cell và nhiệt độ.
- Reset state về `BMS_STATE_INIT`.
- Reset toàn bộ cờ dòng điện/FET/lỗi/cân bằng.
- Reset các bộ tích lũy coulomb (`chargeAccumulated_mAs`, `dischargeAccumulated_mAs`) và throughput/cycle.
- Ý nghĩa: Tạo trạng thái sạch trước khi khởi tạo monitor.

## 7. `static void BMS_ConfigureMonitor(void)`
- Mục đích: Cấu hình toàn bộ IC BQ76952 theo ngưỡng của project.
- Thiết lập chính:
- Chế độ đọc cell 10S (`BMS_BQ_VCELL_MODE_10S`).
- Bật các nhóm bảo vệ A/B/C và cấu hình FET/protection.
- Cấu hình OV/UV theo:
- `BMS_CELL_OV_CUTOFF_MV` (4150mV), delay `BMS_BQ_PROTECTION_DELAY_MS`.
- `BMS_CELL_UV_CUTOFF_MV` (3500mV), delay `BMS_BQ_PROTECTION_DELAY_MS`.
- Cấu hình giới hạn nhiệt độ sạc/xả.
- Bật tính năng cell balancing.
- Tính ngưỡng OC theo sense resistor:
- `over_current_sense_mV = BMS_OVER_CURRENT_MA * BMS_BQ_SENSE_RESISTOR_UOHM / 1e6`.
- Ép tối thiểu 4mV trước khi ghi vào BQ.
- Cấu hình short-circuit protection (`SCD_80`, delay 30).
- Bật FET_ENABLE nếu chưa enabled, clear balance mask, bật cả FET (`ALL, ON`).
- Ý nghĩa: Đồng bộ protection cứng trong BQ với policy mềm của BMS.

## 8. `static void BMS_ReadMeasurements(BMS_Tracking_t *tracking)`
- Mục đích: Đọc dữ liệu tức thời từ BQ và ghi vào tracking.
- Xử lý chính:
1. Guard `tracking == NULL` thì return.
2. Đọc mảng cell voltage (`bq76952_getOnlyConnectedCellVoltages`).
3. Nếu cell voltage âm thì ép 0, ngược lại cast sang `uint16_t`.
4. Đọc:
- `stackVoltage` tam thoi khong xu ly trong runtime tracking.
- `packVoltage` duoc tinh bang tong `cellVoltages[]` trong `BMS_UpdateCellStatistics()`.
- `current_mA`
- `temperature[0] = TS1`, `temperature[1] = TS3`
- trạng thái `charging`, `discharging`
- trạng thái FET global `fetsEnabled`
5. Map tạm `chargeFetEnabled = charging`, `dischargeFetEnabled = discharging`.
- Lưu ý: Giá trị FET này có thể bị điều chỉnh lại bởi `BMS_ApplyFetPolicy`.

## 9. `static void BMS_UpdateCellStatistics(BMS_Tracking_t *tracking)`
- Mục đích: Tính min/max/average/delta cell voltage.
- Cách làm:
1. Duyệt toàn bộ `cellVoltages`.
2. Cộng tổng để tính average.
3. Tìm `min_voltage` nhưng bỏ qua cell = 0 (coi như chưa có số đo).
4. Tìm `max_voltage`.
5. Gán:
- `minCellVoltage` (0 nếu không có cell hợp lệ),
- `maxCellVoltage`,
- `averageCellVoltage = sum / BMS_NUMBER_OF_CELLS`,
- `deltaCellVoltage = max - min`.
- Ý nghĩa: Cung cấp dữ liệu nền cho fault và balancing.

## 10. `static void BMS_UpdateCurrentDirection(BMS_Tracking_t *tracking)`
- Mục đích: Xác định chiều dòng (`CHARGE`, `DISCHARGE`, `IDLE`).
- Logic:
- Vì `BMS_CURRENT_CHARGE_IS_POSITIVE = 1`, nên:
- `current_mA > +deadband` -> `BMS_CURRENT_CHARGE`.
- `current_mA < -deadband` -> `BMS_CURRENT_DISCHARGE`.
- còn lại -> `BMS_CURRENT_IDLE`.
- Deadband dùng `BMS_CURRENT_DEADBAND_MA` (300mA).
- Ý nghĩa: Loại nhiễu quanh 0mA trước khi xử lý OC/coulomb/balancing.

## 11. `static void BMS_UpdateFaultFlags(BMS_Tracking_t *tracking)`
- Mục đích: Cập nhật cờ lỗi mềm từ dữ liệu đo.
- Nhóm điện áp cell:
- Set `cellOverVoltage` nếu có cell >= `BMS_CELL_OV_CUTOFF_MV`.
- Set `cellUnderVoltage` nếu có cell >0 và <= `BMS_CELL_UV_CUTOFF_MV`.
- Recovery hysteresis:
- Clear OV nếu tất cả cell <= `BMS_CELL_OV_RECOVER_MV`.
- Clear UV nếu tất cả cell >= `BMS_CELL_UV_RECOVER_MV`.
- Nhóm nhiệt độ:
- Set charge/discharge over-temp theo ngưỡng cutoff.
- Set under-temp nếu nhiệt độ <= `BMS_UNDERTEMP_CUTOFF_C`.
- Recovery:
- Clear theo các ngưỡng recover tương ứng.
- Nhóm dòng điện:
- `abs_current >= BMS_SHORT_CIRCUIT_MA` -> `shortCircuit = true`.
- `abs_current >= BMS_OVER_CURRENT_MA`:
- nếu đang charge -> `chargeOverCurrent = true`.
- nếu đang discharge -> `dischargeOverCurrent = true`.
- Nếu `abs_current <= deadband`: clear cả OC charge/discharge.
- Lưu ý: Các cờ này giữ trạng thái giữa các chu kỳ và chỉ clear khi đạt điều kiện recover.

## 12. `static void BMS_MergeBQFaultFlags(BMS_Tracking_t *tracking)`
- Mục đích: Gộp fault cứng bên trong BQ vào cờ fault mềm của BMS.
- Nguồn dữ liệu:
- `bq76952_getProtectionStatus()`
- `bq76952_getTemperatureStatus()`
- Cách gộp: dùng OR vào các cờ fault tương ứng (OV/UV/OC/SC/OT/UT).
- `bqSafetyFault` được gán theo `shortCircuit`.
- Ý nghĩa: Không bỏ sót fault do BQ latch mà dữ liệu đo tức thời có thể chưa phản ánh.

## 13. `static void BMS_UpdateState(BMS_Tracking_t *tracking)`
- Mục đích: Suy ra state điều hành từ fault flags.
- Bước 1: Tính cờ chặn charge/discharge:
- `chargeDisabled` nếu dính các lỗi ảnh hưởng đường sạc.
- `dischargeDisabled` nếu dính các lỗi ảnh hưởng đường xả.
- Bước 2: Quyết định state:
1. `shortCircuit` hoặc `communicationFault` -> `BMS_STATE_FAULT`.
2. Cả charge và discharge đều disabled -> `BMS_STATE_FAULT`.
3. Chỉ charge disabled -> `BMS_STATE_CHARGE_PROTECT`.
4. Chỉ discharge disabled -> `BMS_STATE_DISCHARGE_PROTECT`.
5. Còn lại -> `BMS_STATE_NORMAL`.
- Bước 3: Nếu state đổi, ghi log transition + bitmap fault.
- Ý nghĩa: Đây là policy trung tâm quyết định chế độ hoạt động.

## 14. `static void BMS_ApplyFetPolicy(BMS_Tracking_t *tracking)`
- Mục đích: Áp policy state/fault xuống phần cứng FET.
- Logic:
1. Nếu cả charge và discharge bị disable:
- `bq76952_setFET(ALL, OFF)`, set cả hai cờ FET false.
2. Nếu chỉ charge disable:
- `bq76952_setFET(CHG, OFF)`, set `chargeFetEnabled=false`.
3. Nếu chỉ discharge disable:
- `bq76952_setFET(DCH, OFF)`, set `dischargeFetEnabled=false`.
4. Nếu không disable gì:
- `bq76952_setFET(ALL, ON)`, set cả hai cờ true.
- Ý nghĩa: Cơ chế thực thi cuối cùng của bảo vệ mềm.

## 15. `static void BMS_UpdateCoulombCounter(BMS_Tracking_t *tracking, uint32_t dt_ms)`
- Mục đích: Tích lũy dung lượng đã sạc/xả theo phương pháp coulomb counting đơn giản.
- Cách tính:
1. Lấy `abs_current`; nếu <= deadband thì bỏ qua mẫu.
2. `sample_mAs = abs_current * dt_ms / 1000`.
3. Nếu chiều dòng là charge thì cộng vào `chargeAccumulated_mAs`.
4. Nếu chiều dòng là discharge thì cộng vào `dischargeAccumulated_mAs`.
5. Quy đổi:
- `chargeThroughput_mAh = chargeAccumulated_mAs / 3600`
- `dischargeThroughput_mAh = dischargeAccumulated_mAs / 3600`
6. Tính vòng đời tương đương:
- `equivalentCycle_milliCycles = chargeThroughput_mAh * 1000 / BMS_NOMINAL_CAPACITY_MAH`.
- Ý nghĩa: Theo dõi độ hao mòn/cycle ở mức ước lượng.

## 16. `static void BMS_UpdateBalancing(BMS_Tracking_t *tracking, uint32_t now)`
- Mục đích: Tạo `balanceMask` và gửi xuống BQ để cân bằng cell.
- Điều kiện bật balancing:
- `deltaCellVoltage >= BMS_BALANCE_DELTA_MV`.
- Chỉ chạy lại theo chu kỳ `BMS_BALANCE_REFRESH_MS` (1s).
- Điều kiện buộc tắt balancing:
- State khác `BMS_STATE_NORMAL`.
- Không cần balance (`balanceRequired == false`).
- Đang discharge.
- Khi đó gửi mask = 0.
- Logic chọn cell:
1. Bỏ qua cell thấp hơn `BMS_BALANCE_MIN_CELL_MV`.
2. Bỏ qua cell thấp xa max (điều kiện `(cell + delta) <= max`).
3. Chọn cell nếu `cell >= min + delta`.
4. Chặn chọn 2 cell liền kề bằng `previous_selected`.
- Cập nhật:
- `tracking->balanceMask = requested_mask`.
- Gửi `bq76952_setCellBalanceMask(requested_mask)`.
- Ghi log khi mask thay đổi.
- Ý nghĩa: Cân bằng có điều kiện, tránh ảnh hưởng khi pack đang xả hoặc có lỗi.

## 17. `static void BMS_LoadPersistedData(BMS_Tracking_t *tracking)`
- Mục đích: Nạp dữ liệu throughput/cycle đã lưu từ flash.
- Cách làm:
1. `storage_flash_load(&record)`.
2. Nếu record invalid -> tạo mặc định (`storage_flash_make_default`).
3. Nạp `chargeThroughput_mAh`, `dischargeThroughput_mAh`, `equivalentCycle_milliCycles`.
4. Nội suy lại accumulator:
- `chargeAccumulated_mAs = chargeThroughput_mAh * 3600`.
- `dischargeAccumulated_mAs = dischargeThroughput_mAh * 3600`.
5. Cập nhật mốc so sánh save (`g_last_saved_charge_mAh`, `g_last_saved_discharge_mAh`).
- Ý nghĩa: Duy trì thống kê vòng đời qua mỗi lần reset nguồn.

## 18. `static void BMS_SavePersistedDataIfNeeded(const BMS_Tracking_t *tracking, uint32_t now)`
- Mục đích: Lưu dữ liệu throughput/cycle ra flash theo chu kỳ và theo ngưỡng thay đổi.
- Điều kiện chạy:
1. Chỉ xét khi đã qua `BMS_FLASH_SAVE_INTERVAL_MS` (10 phút).
2. Tính delta charge/discharge so với lần lưu gần nhất.
3. Nếu cả 2 delta < `BMS_FLASH_SAVE_DELTA_MAH` (100mAh), không ghi flash, chỉ cập nhật tick.
- Khi cần ghi:
1. Tạo record mặc định.
2. Nạp record cũ để tăng `writeCounter`.
3. Ghi throughput/cycle hiện tại + nominal capacity.
4. `storage_flash_save(&record)`.
5. Nếu thành công: cập nhật tick + mốc saved.
6. Nếu thất bại: log lỗi, giữ mốc cũ.
- Ý nghĩa: Giảm wear flash nhưng vẫn giữ dữ liệu đủ sát.

## 19. `static const char *BMS_StateName(BMS_State_t state)`
- Mục đích: Đổi enum state sang chuỗi để log.
- Mapping:
- `INIT`, `NORMAL`, `CHG_PROTECT`, `DCH_PROTECT`, `FAULT`, mặc định `UNKNOWN`.

## 20. `static bool BMS_AllCellsAtOrBelow(const BMS_Tracking_t *tracking, uint16_t threshold_mV)`
- Mục đích: Helper kiểm tra tất cả cell <= ngưỡng.
- Dùng trong điều kiện recover OV.

## 21. `static bool BMS_AllCellsAtOrAbove(const BMS_Tracking_t *tracking, uint16_t threshold_mV)`
- Mục đích: Helper kiểm tra tất cả cell hợp lệ (cell > 0) đều >= ngưỡng.
- Dùng trong điều kiện recover UV.
- Lưu ý: Cell = 0 sẽ được bỏ qua (coi như chưa đo được), không làm fail điều kiện.

## 22. `static bool BMS_AllTemperaturesAtOrBelow(const BMS_Tracking_t *tracking, int16_t threshold_C)`
- Mục đích: Helper kiểm tra tất cả nhiệt độ <= ngưỡng.
- Dùng để clear cờ over-temperature.

## 23. `static bool BMS_AllTemperaturesAtOrAbove(const BMS_Tracking_t *tracking, int16_t threshold_C)`
- Mục đích: Helper kiểm tra tất cả nhiệt độ >= ngưỡng.
- Dùng để clear cờ under-temperature.

## 24. `static int32_t BMS_AbsCurrent(int32_t current_mA)`
- Mục đích: Trả về trị tuyệt đối của dòng điện.
- Dùng bởi fault current và coulomb counter.

## Tóm tắt quan hệ gọi hàm
- `BMS_Init` gọi:
- `BMS_ResetTracking`
- `BMS_LoadPersistedData`
- `BMS_ConfigureMonitor`
- `BMS_Update`

- `BMS_Update` gọi:
- `BMS_ReadMeasurements`
- `BMS_UpdateCellStatistics`
- `BMS_UpdateCurrentDirection`
- `BMS_UpdateFaultFlags`
- `BMS_MergeBQFaultFlags`
- `BMS_UpdateCoulombCounter`
- `BMS_UpdateState`
- `BMS_ApplyFetPolicy`
- `BMS_UpdateBalancing`
- `BMS_SavePersistedDataIfNeeded`

- Nhóm helper chỉ được gọi nội bộ:
- `BMS_StateName`
- `BMS_AllCellsAtOrBelow`
- `BMS_AllCellsAtOrAbove`
- `BMS_AllTemperaturesAtOrBelow`
- `BMS_AllTemperaturesAtOrAbove`
- `BMS_AbsCurrent`

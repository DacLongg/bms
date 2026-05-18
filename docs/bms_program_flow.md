# BMS Program Flow

Tai lieu nay mo ta luong hoat dong cua firmware BMS hien tai. Thiet ke duoc chia theo nguyen tac:

- `BMS/MyMiddlewares/bms`: quan ly trang thai pack/cell, protection policy, FET policy, coulomb counter va cell balancing.
- `BMS/MyDrivers/bq76952`: chi cung cap API giao tiep voi BQ76952, doc du lieu, cau hinh data memory, dieu khien FET va nhan mask cell balancing.
- `BMS/MyDrivers/debug_log`: log debug qua USART2, co the tat bang macro.
- `BMS/MyDrivers/storage_flash`: luu cac thong so can giu lai sau mat nguon vao Flash cuoi chip.
- `BMS/App/mainapp.c`: goi init mot lan va goi `BMS_Update()` theo chu ky 100 ms.

## 1. Entry Point

`mainapp()` la vong service chinh cua ung dung:

1. Lan dau chay, goi `BMS_Init()`.
2. Moi 100 ms, goi `BMS_Update()`.

```c
if (!initialized) {
    BMS_Init();
    initialized = true;
}

if ((now - last_update_tick) >= 100U) {
    BMS_Update();
    last_update_tick = now;
}
```

Chu ky 100 ms dung de doc cell voltage, pack voltage, current, temperature, cap nhat fault va dieu khien FET. Cell balancing duoc refresh cham hon bang `BMS_BALANCE_REFRESH_MS`.

## 2. BMS Initialization

`BMS_Init()` thuc hien cac buoc:

1. Reset toan bo bien tracking bang `BMS_ResetTracking()`.
2. Khoi tao driver BQ/I2C bang `bq76952_init()`.
3. Kiem tra BQ76952 con phan hoi I2C bang `bq76952_isConnected()`.
4. Neu mat ket noi, goi `BMS_Error_Handler()` de tat FET va tat balancing.
5. Neu ket noi OK, goi `BMS_ConfigureMonitor()` de cau hinh BQ theo policy cua BMS.
6. Luu tick ban dau va goi `BMS_Update()` mot lan de nap du lieu tracking.

Truoc khi vao vong `mainapp()`, `main.c` khoi tao USART2 va goi `debug_log_init(&huart2)`. Sau do cac macro `BMS_LOG_INFO`, `BMS_LOG_WARN`, `BMS_LOG_ERROR` co the ghi log ra USART2.

## 3. BQ Configuration Owned By BMS

`BMS_ConfigureMonitor()` la noi cau hinh BQ76952. Driver BQ khong tu quyet dinh policy pack nua.

Cac cau hinh chinh:

- Cell mapping 10S: `BMS_BQ_VCELL_MODE_10S = 0xAAAF`.
- Enable protection group A/B/C cua BQ.
- Cau hinh FET options, predischarge timeout va stop delta.
- Cau hinh TS1/TS2 la thermistor theo mach nguyen ly.
- Cau hinh over-voltage cell: `BMS_CELL_OV_CUTOFF_MV = 4150 mV`.
- Cau hinh under-voltage cell: `BMS_CELL_UV_CUTOFF_MV = 3500 mV`.
- Cau hinh qua nhiet sac: `BMS_CHARGE_OT_CUTOFF_C = 45 C`.
- Cau hinh qua nhiet xa: `BMS_DISCHARGE_OT_CUTOFF_C = 60 C`.
- Cau hinh hardware over-current theo dien ap sense quy doi tu `BMS_OVER_CURRENT_MA` va `BMS_BQ_SENSE_RESISTOR_UOHM`.
- Enable host-controlled cell balancing.
- Tat balancing ban dau va bat FET neu khong co fault.

Luu y: BQ76952 cau hinh OCD1/OCD2 bang mV tren shunt, khong nhan truc tiep mA. Neu thay doi shunt, can cap nhat `BMS_BQ_SENSE_RESISTOR_UOHM`.

## 4. Main Update Flow

Moi lan `BMS_Update()` chay theo thu tu co dinh:

1. Tinh `dt_ms` tu `HAL_GetTick()` de phuc vu coulomb counter.
2. Kiem tra ket noi BQ.
3. Doc measurement bang `BMS_ReadMeasurements()`.
4. Tinh thong ke cell bang `BMS_UpdateCellStatistics()`.
5. Xac dinh chieu dong bang `BMS_UpdateCurrentDirection()`.
6. Cap nhat fault firmware co hysteresis bang `BMS_UpdateFaultFlags()`.
7. Hop nhat fault do BQ bao bang `BMS_MergeBQFaultFlags()`.
8. Tich luy dong sac/xa bang `BMS_UpdateCoulombCounter()`.
9. Cap nhat state pack bang `BMS_UpdateState()`.
10. Ap dung FET policy bang `BMS_ApplyFetPolicy()`.
11. Cap nhat cell balancing bang `BMS_UpdateBalancing()`.
12. Tang `circle_counter`.

## 5. Measurement Model

`BMS_ReadMeasurements()` doc cac du lieu:

- `cellVoltages[10]`: dien ap tung cell theo mapping 10S cua pack.
- `stackVoltage`: dien ap pack/stack.
- `current_mA`: dong hien tai tu BQ.
- `temperature[0]`: TS1.
- `temperature[1]`: TS2.
- `charging`, `discharging`, `fetsEnabled`: trang thai FET/BQ.

Mapping cell theo mach:

| Cell logic | BQ VC input |
| --- | --- |
| Cell 1 | VC0 |
| Cell 2 | VC1 |
| Cell 3 | VC2 |
| Cell 4 | VC3 |
| Cell 5 | VC5 |
| Cell 6 | VC7 |
| Cell 7 | VC9 |
| Cell 8 | VC11 |
| Cell 9 | VC13 |
| Cell 10 | VC15 |

Mapping nay duoc driver BQ dung khi doc cell va khi doi `balanceMask` logic thanh mask VC cua BQ.

## 6. Cell And Pack Statistics

`BMS_UpdateCellStatistics()` tinh:

- `minCellVoltage`
- `maxCellVoltage`
- `averageCellVoltage`
- `deltaCellVoltage = maxCellVoltage - minCellVoltage`

`deltaCellVoltage` la dau vao de quyet dinh co can balancing hay khong.

## 7. Current Direction

`BMS_UpdateCurrentDirection()` chia dong thanh 3 trang thai:

- `BMS_CURRENT_CHARGE`
- `BMS_CURRENT_DISCHARGE`
- `BMS_CURRENT_IDLE`

Neu `BMS_CURRENT_CHARGE_IS_POSITIVE = 1`:

- `current_mA > BMS_CURRENT_DEADBAND_MA`: dang sac.
- `current_mA < -BMS_CURRENT_DEADBAND_MA`: dang xa.
- Con lai: idle.

Neu do thuc te thay chieu dong nguoc, doi `BMS_CURRENT_CHARGE_IS_POSITIVE` ve `0`.

## 8. Fault And Hysteresis

Firmware duy tri fault co hysteresis de tranh dong/ngat FET lien tuc tai nguong.

### Cell voltage

- Over-voltage set khi bat ky cell nao `>= 4150 mV`.
- Over-voltage clear khi tat ca cell `<= 4100 mV`.
- Under-voltage set khi bat ky cell nao `<= 3500 mV`.
- Under-voltage clear khi tat ca cell `>= 3600 mV`.

### Temperature

- Charge over-temperature set khi bat ky TS nao `>= 45 C`.
- Charge over-temperature clear khi tat ca TS `<= 40 C`.
- Discharge over-temperature set khi bat ky TS nao `>= 60 C`.
- Discharge over-temperature clear khi tat ca TS `<= 55 C`.
- Under-temperature set khi bat ky TS nao `<= 0 C`.
- Under-temperature clear khi tat ca TS `>= 5 C`.

### Current

- Over-current set khi `abs(current_mA) >= 75000 mA`.
- Neu dang sac, set `chargeOverCurrent`.
- Neu dang xa, set `dischargeOverCurrent`.
- Short-circuit firmware set khi `abs(current_mA) >= BMS_SHORT_CIRCUIT_MA`.
- Over-current firmware clear khi dong tro ve deadband.

Ngoai fault firmware, `BMS_MergeBQFaultFlags()` con OR them fault do BQ bao:

- `CELL_OV`
- `CELL_UV`
- `OC_CHG`
- `OC1_DCHG`
- `OC2_DCHG`
- `SC_DCHG`
- `OVERTEMP_CHG`
- `OVERTEMP_DCHG`
- `UNDERTEMP_CHG`
- `UNDERTEMP_DCHG`

## 9. State Machine

`BMS_UpdateState()` sinh state tu cac fault:

| State | Dieu kien |
| --- | --- |
| `BMS_STATE_NORMAL` | Khong co fault can khoa sac/xa |
| `BMS_STATE_CHARGE_PROTECT` | Chi can khoa CHG FET |
| `BMS_STATE_DISCHARGE_PROTECT` | Chi can khoa DCH FET |
| `BMS_STATE_FAULT` | Can khoa ca CHG va DCH, hoac short/communication fault |

Quy tac khoa sac:

- Cell over-voltage.
- Charge over-temperature.
- Under-temperature.
- Charge over-current.
- Short-circuit.
- BQ safety fault.
- Communication fault.

Quy tac khoa xa:

- Cell under-voltage.
- Discharge over-temperature.
- Under-temperature.
- Discharge over-current.
- Short-circuit.
- BQ safety fault.
- Communication fault.

## 10. FET Policy

`BMS_ApplyFetPolicy()` dieu khien FET theo state:

- Neu `chargeDisabled && dischargeDisabled`: `bq76952_setFET(ALL, OFF)`.
- Neu chi `chargeDisabled`: `bq76952_setFET(CHG, OFF)`.
- Neu chi `dischargeDisabled`: `bq76952_setFET(DCH, OFF)`.
- Neu khong disabled: `bq76952_setFET(ALL, ON)`.

Cach nay cho phep:

- Cell day qua nguong 4150 mV: ngat sac, van cho xa neu an toan.
- Cell can 3500 mV: ngat xa, van cho sac neu an toan.

## 11. Coulomb Counter And Cycle Count

`BMS_UpdateCoulombCounter()` dung `dt_ms` va `abs(current_mA)` de tich luy:

- `chargeAccumulated_mAs`
- `dischargeAccumulated_mAs`
- `chargeThroughput_mAh`
- `dischargeThroughput_mAh`
- `equivalentCycle_milliCycles`

Cong thuc:

```c
sample_mAs = abs(current_mA) * dt_ms / 1000;
mAh = accumulated_mAs / 3600;
equivalentCycle_milliCycles = chargeThroughput_mAh * 1000 / BMS_NOMINAL_CAPACITY_MAH;
```

Hien tai gia tri nay nam trong RAM. Neu can giu sau khi mat nguon, can them co che ghi Flash/EEPROM co gioi han tan suat ghi.

## 12. Cell Balancing

Balancing duoc BMS dieu khien host-controlled bang `bq76952_setCellBalanceMask()`.

Dieu kien cho phep balancing:

- `state == BMS_STATE_NORMAL`.
- `deltaCellVoltage >= BMS_BALANCE_DELTA_MV`.
- Khong dang xa.
- Cell duoc chon phai `>= BMS_BALANCE_MIN_CELL_MV`.

Policy chon cell:

1. Tim cac cell cao hon min cell it nhat `BMS_BALANCE_DELTA_MV`.
2. Chi chon cell co dien ap du cao de xa can bang.
3. Tranh chon 2 cell logic lien ke trong cung mot refresh de giam stress nhiet/mach bypass.
4. Doi mask logic 10 cell sang mask VC cua BQ trong driver.
5. Gui subcommand `CB_ACTIVE_CELLS (0x0083)` cho BQ.

Neu co fault, khong can balancing, hoac dang xa, BMS gui mask `0` de tat balancing.

## 13. Error Handling

`BMS_Error_Handler()` duoc goi khi mat ket noi BQ hoac loi nghiem trong:

- Set state `BMS_STATE_FAULT`.
- Tat `CHG` va `DCH`.
- Tat cell balancing.
- Danh dau `chargeDisabled` va `dischargeDisabled`.
- Cap nhat tracking ve trang thai khong an toan.

## 14. USART2 Debug Log / External Protocol

`USART2` da duoc CubeMX cau hinh san:

- Baudrate: `115200`.
- TX/RX: theo cau hinh `MX_USART2_UART_Init()`.
- Mac dinh dung debug log: `debug_log_init()`, `BMS_LOG_INFO()`, `BMS_LOG_WARN()`, `BMS_LOG_ERROR()`.
- Neu build voi `BMS_UART_PROTOCOL_ENABLE=1`, `USART2` duoc dung cho protocol doc thong so BMS va log tu dong tat.

Build protocol:

```sh
make -C BMS all USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1
```

Neu ep `BMS_DEBUG_LOG_ENABLE=1` va `BMS_UART_PROTOCOL_ENABLE=1` cung luc thi build se dung bang `#error`, vi log va protocol dung chung channel UART.

Chi tiet frame, CRC va command doc thong so nam trong `docs/bms_uart_protocol.md`.

Log hien tai duoc ghi tai cac su kien:

- Boot.
- BMS init.
- BQ khong ket noi hoac mat communication.
- Cau hinh BQ xong.
- Doi BMS state.
- Bat/tat hoac doi mask balancing.
- Load/save Flash persistent data.

Log khong ghi moi 100 ms de tranh spam UART va lam cham control loop.

## 15. Flash Persistent Storage

`storage_flash` chua 1 record nho de luu cac thong so can giu sau mat nguon:

- `chargeThroughput_mAh`
- `dischargeThroughput_mAh`
- `equivalentCycle_milliCycles`
- `nominalCapacity_mAh`
- `writeCounter`

Vung Flash dung de luu:

- Base address: `0x08007C00`.
- Size: `1024 bytes`.
- Page size STM32L0: `128 bytes`.
- Linker script da giam `FLASH` xuong `31K`, de lai 1 KB cuoi Flash cho storage.

`BMS_LoadPersistedData()` doc record khi khoi dong. Neu record sai magic/version/checksum thi dung default.

`BMS_SavePersistedDataIfNeeded()` chi ghi lai khi:

- Da qua `BMS_FLASH_SAVE_INTERVAL_MS`, va
- `chargeThroughput_mAh` hoac `dischargeThroughput_mAh` tang it nhat `BMS_FLASH_SAVE_DELTA_MAH`.

Muc dich la giam so lan erase/write Flash.

## 16. Configuration Values To Review On Hardware

Cac gia tri can xac nhan bang do thuc te:

- `BMS_BQ_SENSE_RESISTOR_UOHM`: gia tri shunt that tren board.
- `BMS_CURRENT_CHARGE_IS_POSITIVE`: chieu dong sac/xa cua BQ tren board.
- `BMS_NOMINAL_CAPACITY_MAH`: dung luong danh dinh cua pack de tinh cycle.
- `BMS_SHORT_CIRCUIT_MA`: nguong firmware cho short-circuit.
- `BMS_BALANCE_MIN_CELL_MV`: nguong cho phep xa can bang.
- TS1/TS2 co dung NTC 10 k va model `0x07` hay can custom thermistor coefficients.

## 17. Runtime Data For Debug

Lay con tro tracking bang:

```c
const BMS_Tracking_t *tracking = BMS_GetTracking();
```

Cac truong nen quan sat khi debug:

- `state`
- `currentDirection`
- `cellVoltages[]`
- `stackVoltage`
- `current_mA`
- `temperature[]`
- `faults`
- `chargeDisabled`
- `dischargeDisabled`
- `balanceRequired`
- `balanceMask`
- `chargeThroughput_mAh`
- `dischargeThroughput_mAh`
- `equivalentCycle_milliCycles`

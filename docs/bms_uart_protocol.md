# BMS UART Protocol

USART2 is shared between debug log and the external BMS protocol.

Build modes:

```sh
make -C BMS all
make -C BMS all USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1
```

- Default: `BMS_DEBUG_LOG_ENABLE=1`, `BMS_UART_PROTOCOL_ENABLE=0`.
- Protocol build: `BMS_UART_PROTOCOL_ENABLE=1`, debug log is disabled.
- If both `BMS_DEBUG_LOG_ENABLE` and `BMS_UART_PROTOCOL_ENABLE` are forced to `1`, build stops with `#error`.

## Frame

All multi-byte numbers are little endian.

```text
SOF0 SOF1 CMD LEN PAYLOAD[LEN] CRC16_LO CRC16_HI
0xAA 0x55 ... ... ...          ...
```

- `CMD`: request command. Response command is `CMD | 0x80`.
- `LEN`: payload length, max `160` bytes.
- `CRC16`: Modbus/IBM CRC16, init `0xFFFF`, polynomial `0xA001`, calculated over `CMD LEN PAYLOAD`.

Every response payload starts with `STATUS`:

```text
STATUS DATA...
```

Status values:

| Value | Name |
| --- | --- |
| `0x00` | OK |
| `0x01` | Bad length |
| `0x02` | Bad command |
| `0x03` | Busy |
| `0x04` | Internal error |
| `0x05` | Bad payload |

## Commands

The firmware can also send asynchronous event frames. Event frames use the same
frame format and CRC, but they are not responses and therefore their payload
does not start with `STATUS`.

### `0x01` Ping

Request payload: optional echo data, max 63 bytes.

Response data:

```text
echo bytes
```

### `0x10` Read Summary

Request payload: empty.

Response data after `STATUS`:

Current `protocolVersion` is `0x04`.

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | protocolVersion |
| 1 | u32 | uptime_ms |
| 5 | u8 | initialized |
| 6 | u8 | connected |
| 7 | u8 | state |
| 8 | u8 | currentDirection |
| 9 | u16 | faultBitmap |
| 11 | u16 | reservedStackVoltage_mV, currently 0 |
| 13 | u16 | packVoltage_mV, sum of cellVoltages |
| 15 | u16 | batAdcEstimatedPack_mV |
| 17 | i32 | current_mA |
| 21 | u16 | minCellVoltage_mV |
| 23 | u16 | maxCellVoltage_mV |
| 25 | u16 | averageCellVoltage_mV |
| 27 | u16 | deltaCellVoltage_mV |
| 29 | i16 | temperature0_C |
| 31 | i16 | temperature1_C |
| 33 | u32 | chargeThroughput_mAh |
| 37 | u32 | dischargeThroughput_mAh |
| 41 | u32 | equivalentCycle_milliCycles |
| 45 | u8 | fetBitmap |
| 46 | u8 | balanceRequired |
| 47 | u16 | balanceMask |
| 49 | u32 | alertCounter |
| 53 | u16 | circleCounter |

`balanceRequired` is 1 when BQ reports active autonomous cell balancing. `balanceMask` is read from BQ `CB_ACTIVE_CELLS (0x0083)`.

State values:

| Value | State |
| --- | --- |
| `0` | INIT |
| `1` | NORMAL |
| `2` | CHARGE_PROTECT |
| `3` | DISCHARGE_PROTECT |
| `4` | FAULT |

Current direction:

| Value | Direction |
| --- | --- |
| `0` | IDLE |
| `1` | CHARGE |
| `2` | DISCHARGE |

Fault bitmap:

| Bit | Fault |
| --- | --- |
| 0 | cellOverVoltage |
| 1 | cellUnderVoltage |
| 2 | chargeOverTemperature |
| 3 | dischargeOverTemperature |
| 4 | underTemperature |
| 5 | chargeOverCurrent |
| 6 | dischargeOverCurrent |
| 7 | shortCircuit |
| 8 | bqSafetyFault |
| 9 | communicationFault |
| 10 | reserved |
| 11 | reserved |

FET bitmap:

| Bit | Field |
| --- | --- |
| 0 | fetsEnabled |
| 1 | chargeFetEnabled |
| 2 | dischargeFetEnabled |
| 3 | chargeDisabled |
| 4 | dischargeDisabled |
| 5 | fetOffAsserted |

### `0x11` Read Cells

Request payload: empty.

Response data after `STATUS`:

```text
cellCount:u8 cell0_mV:u16 cell1_mV:u16 ... cellN_mV:u16
```

The current firmware reports 10 cells.

### `0x12` Read Faults

Request payload: empty.

Response data after `STATUS`:

```text
faultBitmap:u16 gateSignalBitmap:u8 alertActive:u8 alertCounter:u32
```

`gateSignalBitmap` bit 0 is DCHG pin level, bit 1 is DDSG pin level. With the current BQ pin config, high means the corresponding BQ FET output is disabled; these bits are status signals, not standalone faults.

### `0x13` Read Limits

Request payload: empty.

Response data after `STATUS`:

```text
cellCount:u8
thermistorCount:u8
cellOvCutoff_mV:u16
cellOvRecover_mV:u16
cellUvCutoff_mV:u16
cellUvRecover_mV:u16
balanceDelta_mV:u16
balanceMinCell_mV:u16
overCurrent_mA:i32
shortCircuit_mA:i32
chargeOtCutoff_C:i16
dischargeOtCutoff_C:i16
undertempCutoff_C:i16
nominalCapacity_mAh:u32
```

### `0x20` OTP Check

Request payload: empty.

Runs BQ76952 `OTP_WR_CHECK` after entering Full Access and Config Update mode. The command does not write OTP.

Response data after `STATUS`:

```text
otpFlags:u16
securityState:u8
checkResult:u8
checkDataFailAddr:u16
writeResult:u8
writeDataFailAddr:u16
batteryStatusRaw:u16
staticConfigSignature:u16
stackVoltage_mV:u16      // runtime tracking currently reports 0
packVoltage_mV:u16       // runtime tracking uses sum of cell voltages
internalTemp_C:i16
reg0Config:u8
reg12Control:u8
daConfig:u8
vcellMode:u16
dchgPinConfig:u8
ddsgPinConfig:u8
dfetoffPinConfig:u8
```

`checkResult` and `writeResult` use BQ76952 OTP result bits:

| Bit | Field |
| --- | --- |
| 7 | OK |
| 5 | LOCK |
| 4 | NOSIG |
| 3 | NODATA |
| 2 | HT |
| 1 | LV |
| 0 | HV |

`otpFlags`:

| Bit | Field |
| --- | --- |
| 0 | fullAccessOk |
| 1 | configUpdateOk |
| 2 | checkOk |
| 3 | writeOk |
| 4 | otpBlocked |
| 5 | otpPending |
| 6 | dchgPinConfigOk (`0x2A`) |
| 7 | ddsgPinConfigOk (`0x2A`) |
| 8 | daConfigUsesCentivolt |
| 9 | checkLock |
| 10 | checkNoSignature |
| 11 | checkNoData |
| 12 | checkHighTemp |
| 13 | checkLowVoltage |
| 14 | checkHighVoltage |
| 15 | writeFailed |

### `0x21` OTP Write

Request payload:

```text
0x4F 0x54 0x50 0x21
```

The payload is ASCII `OTP!` and is required to avoid accidental OTP programming. Response data is the same layout as `0x20`. `STATUS=OK` means the command was parsed; use `otpFlags.writeOk` and `writeResult` to know whether OTP programming succeeded.

### `0x22` OTP Read

Request payload: empty.

Response data is the same layout as `0x20`, but no `OTP_WR_CHECK` or `OTP_WRITE` is issued. This reads the active BQ76952 configuration RAM/status snapshot. BQ76952 does not provide a raw direct-read of OTP contents; after boot, valid OTP contents are reflected through these data-memory/config fields.

### `0x30` Calibrate Current

Request payload:

```text
actualCurrent_mA:i32
```

Firmware reads the current BQ value, compares absolute current values, and calculates a new current calibration gain. If the BQ reading is zero, or the difference is greater than 30% of `actualCurrent_mA`, calibration fails and flash is not updated.

Response data after `STATUS`:

```text
calStatus:u8
actualCurrent_mA:i32
measuredCurrent_mA:i32
deviation_ppm:u32
oldGain_ppm:u32
newGain_ppm:u32
```

`STATUS=OK` means the new gain was written to BQ and saved to flash. `STATUS=Bad payload` means the input/current validation failed. `STATUS=Internal error` means writing BQ or flash failed.

`calStatus`:

| Value | Meaning |
| --- | --- |
| `0` | OK |
| `1` | Bad actual current input |
| `2` | BQ measured current is zero |
| `3` | Difference is greater than 30% |
| `4` | BQ or flash write failed |

### Async `0x40` Protection Reason

Sent automatically when firmware detects a new protection cause. This frame is
not a response.

Payload:

```text
reason:u8
```

Reason values:

| Value | Reason |
| --- | --- |
| `0x01` | cellOverVoltage |
| `0x02` | cellUnderVoltage |
| `0x03` | chargeOverTemperature |
| `0x04` | dischargeOverTemperature |
| `0x05` | underTemperature |
| `0x06` | chargeOverCurrent |
| `0x07` | dischargeOverCurrent |
| `0x08` | shortCircuit |
| `0x09` | communicationFault |

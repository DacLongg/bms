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
- `LEN`: payload length, max `64` bytes.
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

## Commands

### `0x01` Ping

Request payload: optional echo data, max 63 bytes.

Response data:

```text
echo bytes
```

### `0x10` Read Summary

Request payload: empty.

Response data after `STATUS`:

Current `protocolVersion` is `0x02`.

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | protocolVersion |
| 1 | u32 | uptime_ms |
| 5 | u8 | initialized |
| 6 | u8 | connected |
| 7 | u8 | state |
| 8 | u8 | currentDirection |
| 9 | u16 | faultBitmap |
| 11 | u16 | stackVoltage_mV |
| 13 | u16 | packVoltage_mV |
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
| 10 | chargeGateFaultSignal |
| 11 | dischargeGateFaultSignal |

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

`gateSignalBitmap` bit 0 is charge gate fault signal, bit 1 is discharge gate fault signal.

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

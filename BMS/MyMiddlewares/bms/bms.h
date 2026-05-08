#ifndef BMS_H
#define BMS_H

#include <stdbool.h>
#include <stdint.h>

#include "bq76952.h"

#define BMS_NUMBER_OF_CELLS                         10U
#define BMS_NUMBER_OF_THERMISTORS                   2U

#define BMS_CELL_OV_CUTOFF_MV                       4150U
#define BMS_CELL_OV_RECOVER_MV                      4100U
#define BMS_CELL_UV_CUTOFF_MV                       3500U
#define BMS_CELL_UV_RECOVER_MV                      3600U

#define BMS_BALANCE_DELTA_MV                        30U
#define BMS_BALANCE_MIN_CELL_MV                     3800U
#define BMS_BALANCE_REFRESH_MS                      1000U

#define BMS_CURRENT_DEADBAND_MA                     300L
#define BMS_OVER_CURRENT_MA                         75000L
#define BMS_SHORT_CIRCUIT_MA                        120000L
#define BMS_NOMINAL_CAPACITY_MAH                    20000UL

#define BMS_CHARGE_OT_CUTOFF_C                      45
#define BMS_CHARGE_OT_RECOVER_C                     40
#define BMS_DISCHARGE_OT_CUTOFF_C                   60
#define BMS_DISCHARGE_OT_RECOVER_C                  55
#define BMS_UNDERTEMP_CUTOFF_C                      0
#define BMS_UNDERTEMP_RECOVER_C                     5

#define BMS_BQ_VCELL_MODE_10S                       0xAAAFU
#define BMS_BQ_SENSE_RESISTOR_UOHM                  50UL
#define BMS_BQ_PROTECTION_DELAY_MS                  100U
#define BMS_CURRENT_CHARGE_IS_POSITIVE              1

#define NUMBER_OF_CELLS                             BMS_NUMBER_OF_CELLS
#define NUMBER_OF_THERMISTORS                       BMS_NUMBER_OF_THERMISTORS

typedef enum {
    BMS_STATE_INIT = 0,
    BMS_STATE_NORMAL,
    BMS_STATE_CHARGE_PROTECT,
    BMS_STATE_DISCHARGE_PROTECT,
    BMS_STATE_FAULT
} BMS_State_t;

typedef enum {
    BMS_CURRENT_IDLE = 0,
    BMS_CURRENT_CHARGE,
    BMS_CURRENT_DISCHARGE
} BMS_CurrentDirection_t;

typedef struct {
    bool cellOverVoltage;
    bool cellUnderVoltage;
    bool chargeOverTemperature;
    bool dischargeOverTemperature;
    bool underTemperature;
    bool chargeOverCurrent;
    bool dischargeOverCurrent;
    bool shortCircuit;
    bool bqSafetyFault;
    bool communicationFault;
} BMS_FaultFlags_t;

typedef struct {
    bool                    initialized;
    bool                    connected;
    BMS_State_t             state;
    BMS_CurrentDirection_t  currentDirection;

    uint16_t cellVoltages[BMS_NUMBER_OF_CELLS];
    uint16_t minCellVoltage;
    uint16_t maxCellVoltage;
    uint16_t averageCellVoltage;
    uint16_t deltaCellVoltage;
    uint16_t stackVoltage;
    uint16_t circle_counter;

    int32_t  current_mA;
    int16_t  temperature[BMS_NUMBER_OF_THERMISTORS];

    bool     charging;
    bool     discharging;
    bool     chargeFetEnabled;
    bool     dischargeFetEnabled;
    bool     fetsEnabled;

    BMS_FaultFlags_t faults;
    bool     chargeDisabled;
    bool     dischargeDisabled;
    bool     balanceRequired;
    uint16_t balanceMask;

    uint64_t chargeAccumulated_mAs;
    uint64_t dischargeAccumulated_mAs;
    uint32_t chargeThroughput_mAh;
    uint32_t dischargeThroughput_mAh;
    uint32_t equivalentCycle_milliCycles;
} BMS_Tracking_t;

void BMS_Init(void);
void BMS_Update(void);
const BMS_Tracking_t *BMS_GetTracking(void);
bool BMS_IsFaultActive(void);
void BMS_Error_Handler(void);

#endif

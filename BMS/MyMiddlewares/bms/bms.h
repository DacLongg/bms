#ifndef BMS_H
#define BMS_H

#include <stdint.h>
#include <stdbool.h>
#include "bq76952.h"

#define NUMBER_OF_CELLS                         10
#define OVER_VOLTAGE_THRESHOLD                  4130U
#define UNDER_VOLTAGE_THRESHOLD                 3500U
#define NUMBER_OF_THERMISTORS                   2
#define OVER_CHARG_TEMPERATURE_THRESHOLD        60U
#define UNDER_CHARG_TEMPERATURE_THRESHOLD       0U
#define OVER_DISCHARG_TEMPERATURE_THRESHOLD     60U
#define UNDER_DISCHARG_TEMPERATURE_THRESHOLD    0U
#define OVER_CURRENT_THRESHOLD                  65000U
#define DELTA_VOLTAGE_BALANCE_THRESHOLD         30U


typedef struct {
    bool        initialized;
    bool        connected;
    uint16_t    cellVoltages[NUMBER_OF_CELLS];
    uint16_t    circle_counter;
    uint16_t    stackVoltage;
    int16_t     current;
    int16_t     temperature[NUMBER_OF_THERMISTORS];
    bool        charging;
    bool        discharging;
    bool        fetsEnabled;

    bool overVoltage;
    bool underVoltage;
    bool overChargeTemperature;
    bool underChargeTemperature;
    bool overDischargeTemperature;
    bool underDischargeTemperature;
    bool overCurrent;
    bool balanceRequired;
} BMS_Tracking_t;

// Function prototypes
void BMS_Init(void);
void BMS_Update(void);
const BMS_Tracking_t *BMS_GetTracking(void);
bool BMS_IsFaultActive(void);
void BMS_Error_Handler(void);

#endif // BMS_H

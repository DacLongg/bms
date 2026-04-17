#ifndef BQ76952_H
#define BQ76952_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c_soft.h"
#include "stm32l0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte;

typedef enum {
    TS1,
    TS2,
    TS3,
    HDQ,
    DCHG,
    DDSG
} bq76952_thermistor_t;

typedef enum {
    CHG,
    DCH,
    ALL
} bq76952_fet_t;

typedef enum {
    OFF,
    ON
} bq76952_fet_state_t;

typedef enum {
    SCD_10,
    SCD_20,
    SCD_40,
    SCD_60,
    SCD_80,
    SCD_100,
    SCD_125,
    SCD_150,
    SCD_175,
    SCD_200,
    SCD_250,
    SCD_300,
    SCD_350,
    SCD_400,
    SCD_450,
    SCD_500
} bq76952_scd_thresh_t;

#define REG0_CONFIG 0x9237U
#define REG12_CONTROL 0x9236U
#define ALERT_PIN_CONFIG 0x92FCU
#define DEFAULT_ALARM_MASK_CONFIG 0x926DU
#define DA_CONFIGURATION 0x9303U
#define SHUTDOWN_STACK_VOLTAGE 0x9241U
#define VCELL_MODE 0x9304U
#define PROTECTION_CONFIGURATION 0x925FU
#define ENABLE_PROTECTIONS_A 0x9261U
#define ENABLE_PROTECTIONS_B 0x9262U
#define CHG_FET_PROTECTION_A 0x9265U
#define CHG_FET_PROTECTION_B 0x9266U
#define CHG_FET_PROTECTION_C 0x9267U
#define ENABLE_PROTECTIONS_C 0x9263U
#define CHG_FET_PROTECTIONS_A 0x9265U
#define CELL_INTERCONNECT_RESISTANCE 0x9315U
#define DSG_FET_PROTECTION_A 0x9269U
#define DSG_FET_PROTECTION_B 0x926AU
#define DSG_FET_PROTECTION_C 0x926BU
#define SF_ALERT_MASK_A 0x926FU
#define SF_ALERT_MASK_B 0x9270U
#define SF_ALERT_MASK_C 0x9271U
#define SCD_THRESHOLD_CONFIG 0x9286U
#define SCD_DELAY_CONFIG 0x9287U
#define FET_OPTIONS 0x9308U
#define FET_PREDISCHARGE_TIMEOUT 0x930EU
#define FET_PREDISCHARGE_STOP_DELTA 0x930FU
#define CC3_SAMPLES 0x9307U
#define TS1_CONFIG 0x92FDU
#define TS2_CONFIG 0x92FEU
#define TS3_CONFIG 0x92FFU

#define UNSEAL_KEY_STEP_1 0x0414U
#define UNSEAL_KEY_STEP_2 0x3672U
#define FULL_ACCESS_KEY_STEP_1 0x1234U
#define FULL_ACCESS_KEY_STEP_2 0xABCDU

#define CELL_INTERCONNECT_RESISTANCE_MOHM 0U

typedef union {
    struct {
        uint8_t SC_DCHG : 1;
        uint8_t OC2_DCHG : 1;
        uint8_t OC1_DCHG : 1;
        uint8_t OC_CHG : 1;
        uint8_t CELL_OV : 1;
        uint8_t CELL_UV : 1;
    } bits;
} bq76952_protection_t;

typedef union {
    struct {
        uint8_t OCD3 : 1;
        uint8_t SCDL : 1;
        uint8_t OCDL : 1;
        uint8_t COVL : 1;
        uint8_t PTOS : 1;
    } bits;
} bq76952_safety_alert_c_t;

typedef union {
    struct {
        uint8_t OVERTEMP_FET : 1;
        uint8_t OVERTEMP_INTERNAL : 1;
        uint8_t OVERTEMP_DCHG : 1;
        uint8_t OVERTEMP_CHG : 1;
        uint8_t UNDERTEMP_INTERNAL : 1;
        uint8_t UNDERTEMP_DCHG : 1;
        uint8_t UNDERTEMP_CHG : 1;
    } bits;
} bq76952_temp_t;

typedef union {
    struct {
        uint16_t SLEEP_MODE : 1;
        uint16_t BIT14_RESERVED : 1;
        uint16_t SHUTDOWN_PENDING : 1;
        uint16_t PERMANENT_FAULT : 1;
        uint16_t SAFETY_FAULT : 1;
        uint16_t FUSE_PIN : 1;
        uint16_t SECURITY_STATE : 2;
        uint16_t WR_TO_OTP_BLOCKED : 1;
        uint16_t WR_TO_OTP_PENDING : 1;
        uint16_t OPEN_WIRE_CHECK : 1;
        uint16_t WD_WAS_TRIGGERED : 1;
        uint16_t FULL_RESET_OCCURED : 1;
        uint16_t SLEEP_EN_ALLOWED : 1;
        uint16_t PRECHARGE_MODE : 1;
        uint16_t CONFIG_UPDATE_MODE : 1;
    } bits;
} bq76952_battery_status_t;

void bq76952_init(I2C_HandleTypeDef *hi2c);
void bq76952_begin(void);
void bq76952_reset(void);
void bq76952_enterConfigUpdate(void);
void bq76952_exitConfigUpdate(void);
bool bq76952_isConnected(void);
byte bq76952_getMfgStatusInitRegister(void);
int bq76952_getCellVoltage(byte cellNumber);
void bq76952_getAllCellVoltages(int *cellArray);
void bq76952_getOnlyConnectedCellVoltages(int *cellArray);
int bq76952_getCurrent(void);
int bq76952_getCurrentNow(void);
int bq76952_getCurrentAvg(void);
bool bq76952_areFETs_Enabled(void);
unsigned int bq76952_getManufacturingStatus(void);
unsigned int bq76952_getStackVoltage(void);
float bq76952_getInternalTemp(void);
float bq76952_getThermistorTemp(bq76952_thermistor_t thermistor);
bq76952_protection_t bq76952_getProtectionStatus(void);
bq76952_safety_alert_c_t bq76952_getSafetyAlert_C(void);
bq76952_temp_t bq76952_getTemperatureStatus(void);
void bq76952_setFET(bq76952_fet_t fet, bq76952_fet_state_t state);
void bq76952_setFET_ENABLE(void);
bool bq76952_isDischarging(void);
bool bq76952_isCharging(void);
void bq76952_setCellOvervoltageProtection(unsigned int mv, unsigned int ms);
void bq76952_setCellUndervoltageProtection(unsigned int mv, unsigned int ms);
void bq76952_setShortCircuitThreshold(void);
void bq76952_setProtectionConfiguration(void);
void bq76952_setShutdownStackVoltage(unsigned int voltage);
void bq76952_setChargingOvercurrentProtection(unsigned int mv, byte ms);
void bq76952_setChargingTemperatureMaxLimit(int temp, byte sec);
void bq76952_setDischargingOvercurrentProtection(unsigned int mv, byte ms);
void bq76952_setDischargingOvercurrentProtection_OCD3(int16_t mA);
void bq76952_setDischargingOvercurrentProtection_Recovery(int16_t mA);
void bq76952_setDischargingShortcircuitProtection(bq76952_scd_thresh_t thresh, unsigned int us);
void bq76952_setDischargingTemperatureMaxLimit(int temp, byte sec);
unsigned int bq76952_getDeviceNumber(void);
unsigned int bq76952_getHWVersion(void);
bool bq76952_checkSecurityKeys(void);
unsigned int bq76952_getCOVSnapshot(byte cell);
bool bq76952_Enter_FullAccessMode(void);
bool bq76952_configure_before_OTP_write(void);
bool bq76952_is_OTP_already_programmed(void);
bool bq76952_program_OTP(void);
void bq76952_setEnablePreRegulator(void);
void bq76952_setDA_Config(void);
void bq76952_setSF_AlertMask_A(void);
void bq76952_setSF_AlertMask_B(void);
void bq76952_setSF_AlertMask_C(void);
void bq76952_setEnableRegulator(bool enable_reg1, bool enable_reg2);
void bq76952_setAlertPinConfig(void);
void bq76952_setDefaultAlarmMaskConfig(void);
void bq76952_setVcellMode(uint16_t vcell_mode);
void bq76952_setEnableCHG_FET_Protection(void);
void bq76952_setEnableProtectionsA(void);
void bq76952_setEnableProtectionsB(void);
void bq76952_setEnableProtectionsC(void);
void bq76952_setCHGFETProtectionsA(byte val);
void bq76952_setDSGFETProtectionsA(void);
void bq76952_setDSGFETProtectionsB(void);
void bq76952_setDSGFETProtectionsC(void);
void bq76952_setFET_Options(void);
void bq76952_setFET_PredischargeTimeout(void);
void bq76952_setFET_PredischargeStopDelta(void);
void bq76952_setCellInterconnectResistances(void);
unsigned int bq76952_getAlertRawStatusRegister(void);
void bq76952_setEnableTS1(void);
void bq76952_setEnableTS2(void);
void bq76952_setEnableTS3(void);
unsigned int bq76952_getAlertStatusRegister(void);
byte bq76952_HandleAlarm(void);
bq76952_battery_status_t bq76952_getBatteryStatusRegister(void);
unsigned int bq76952_readDataMemory(unsigned int addr, int size);
void bq76952_handle_alarm(void);
void bq76952_check_batt_status(void);

#ifdef __cplusplus
}
#endif

#endif

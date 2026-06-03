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

/* Các đầu đo nhiệt mà IC có thể đọc trực tiếp.
 * TS1..TS3 là các chân thermistor ngoài.
 * HDQ/DCHG/DDSG là các kênh nhiệt độ nội bộ hoặc suy ra từ mạch FET theo sơ đồ phần cứng của BQ76952.
 */
typedef enum {
    TS1,
    TS2,
    TS3,
    HDQ,
    DCHG,
    DDSG
} bq76952_thermistor_t;

/* Nhóm FET điều khiển bởi BQ76952.
 * CHG: FET sạc, DCH: FET xả, ALL: tác động cả hai.
 */
typedef enum {
    CHG,
    DCH,
    ALL
} bq76952_fet_t;

/* Trạng thái đóng/ngắt FET. */
typedef enum {
    OFF,
    ON
} bq76952_fet_state_t;

typedef struct {
    bool attempted;
    bool i2cOk;
    bool configUpdateOk;
    bool readbackOk;
    bool verified;
    unsigned int addr;
    uint16_t expected;
    uint16_t actual;
    byte size;
} bq76952_write_verify_t;

/* Mức ngưỡng SCD (Short Circuit in Discharge) dạng mã hóa theo datasheet.
 * Giá trị enum không phải đơn vị mA trực tiếp, mà là mã ghi vào data memory.
 * Hậu tố số thể hiện mức ngưỡng tương ứng theo mV trên shunt/điện áp sense của BQ76952.
 */
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

/* Data memory address dùng trong quá trình cấu hình IC.
 * Các giá trị này là địa chỉ thật trong data memory của BQ76952.
 */
#define REG0_CONFIG 0x9237U
#define REG12_CONTROL 0x9236U
#define CC_GAIN 0x91A8U
#define CAPACITY_GAIN 0x91ACU
#define CFETOFF_PIN_CONFIG 0x92FBU
#define ALERT_PIN_CONFIG 0x92FCU
#define DFETOFF_PIN_CONFIG 0x9300U
#define DCHG_PIN_CONFIG 0x9301U
#define DDSG_PIN_CONFIG 0x9302U
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
#define BALANCING_CONFIGURATION 0x9335U
#define CELL_BALANCE_MIN_CELL_TEMP 0x9336U
#define CELL_BALANCE_MAX_CELL_TEMP 0x9337U
#define CELL_BALANCE_MAX_INTERNAL_TEMP 0x9338U
#define CELL_BALANCE_INTERVAL 0x9339U
#define CELL_BALANCE_MAX_CELLS 0x933AU
#define CELL_BALANCE_MIN_CELL_V_CHARGE 0x933BU
#define CELL_BALANCE_MIN_DELTA_CHARGE 0x933DU
#define CELL_BALANCE_STOP_DELTA_CHARGE 0x933EU
#define CELL_BALANCE_MIN_CELL_V_RELAX 0x933FU
#define CELL_BALANCE_MIN_DELTA_RELAX 0x9341U
#define CELL_BALANCE_STOP_DELTA_RELAX 0x9342U

#define UNSEAL_KEY_STEP_1 0x0414U
#define UNSEAL_KEY_STEP_2 0x3672U
#define FULL_ACCESS_KEY_STEP_1 0x1234U
#define FULL_ACCESS_KEY_STEP_2 0xABCDU

/* Điện trở liên kết giữa các cell, đơn vị mOhm.
 * Giá trị 0 nghĩa là bỏ qua hiệu chỉnh sụt áp do interconnect.
 */
#define CELL_INTERCONNECT_RESISTANCE_MOHM 0U

#define BQ_REG1_VOLTAGE_CODE 0x06U
#define BQ_REG2_VOLTAGE_CODE 0x07U
#define BQ_DA_CONFIG_USER_VOLTS_CV 0x04U
#define BQ_DA_CONFIG_USER_AMPS_MA 0x01U
#define BQ_DA_CONFIG_DEFAULT (BQ_DA_CONFIG_USER_VOLTS_CV | BQ_DA_CONFIG_USER_AMPS_MA)

#define BQ_PIN_CONFIG_DCHG_ACTIVE_HIGH 0x2AU
#define BQ_PIN_CONFIG_DDSG_ACTIVE_HIGH 0x2AU

#define BQ_OTP_RESULT_OK     0x80U
#define BQ_OTP_RESULT_LOCK   0x20U
#define BQ_OTP_RESULT_NOSIG  0x10U
#define BQ_OTP_RESULT_NODATA 0x08U
#define BQ_OTP_RESULT_HT     0x04U
#define BQ_OTP_RESULT_LV     0x02U
#define BQ_OTP_RESULT_HV     0x01U

/* Các cờ bảo vệ đang active trong Safety Status A. */
typedef union {
    struct {
        /* Short-circuit khi xả. */
        uint8_t SC_DCHG : 1;
        /* Overcurrent discharge mức 2. */
        uint8_t OC2_DCHG : 1;
        /* Overcurrent discharge mức 1. */
        uint8_t OC1_DCHG : 1;
        /* Overcurrent khi sạc. */
        uint8_t OC_CHG : 1;
        /* Một cell vượt ngưỡng overvoltage. */
        uint8_t CELL_OV : 1;
        /* Một cell xuống dưới ngưỡng undervoltage. */
        uint8_t CELL_UV : 1;
    } bits;
} bq76952_protection_t;

/* Các cờ cảnh báo thuộc Safety Alert C. Đây là cảnh báo/latched alert,
 * hữu ích để biết lý do lỗi đã từng xuất hiện ngay cả khi trạng thái tức thời đã mất.
 */
typedef union {
    struct {
        /* Cảnh báo overcurrent discharge mức 3. */
        uint8_t OCD3 : 1;
        /* SCD đã bị latch. */
        uint8_t SCDL : 1;
        /* OCD đã bị latch. */
        uint8_t OCDL : 1;
        /* COV đã bị latch. */
        uint8_t COVL : 1;
        /* Precharge timeout/sequence status theo cấu hình protection. */
        uint8_t PTOS : 1;
    } bits;
} bq76952_safety_alert_c_t;

/* Trạng thái nhiệt độ tổng hợp từ thanh ghi FTEMP. */
typedef union {
    struct {
        /* Nhiệt độ mạch FET vượt ngưỡng. */
        uint8_t OVERTEMP_FET : 1;
        /* Nhiệt độ trong IC vượt ngưỡng. */
        uint8_t OVERTEMP_INTERNAL : 1;
        /* Nhiệt độ khi xả vượt ngưỡng cho phép. */
        uint8_t OVERTEMP_DCHG : 1;
        /* Nhiệt độ khi sạc vượt ngưỡng cho phép. */
        uint8_t OVERTEMP_CHG : 1;
        /* Nhiệt độ trong IC thấp hơn ngưỡng. */
        uint8_t UNDERTEMP_INTERNAL : 1;
        /* Nhiệt độ khi xả thấp hơn ngưỡng cho phép. */
        uint8_t UNDERTEMP_DCHG : 1;
        /* Nhiệt độ khi sạc thấp hơn ngưỡng cho phép. */
        uint8_t UNDERTEMP_CHG : 1;
    } bits;
} bq76952_temp_t;

/* Battery Status là thanh ghi trạng thái hệ thống mức cao của BQ76952. */
typedef union {
    struct {
        /* IC đang ở sleep mode. */
        uint16_t SLEEP_MODE : 1;
        uint16_t BIT14_RESERVED : 1;
        /* Đã nhận yêu cầu shutdown và đang chờ hoàn tất. */
        uint16_t SHUTDOWN_PENDING : 1;
        /* Có permanent fault. */
        uint16_t PERMANENT_FAULT : 1;
        /* Có safety fault. */
        uint16_t SAFETY_FAULT : 1;
        /* Mức hiện tại của chân fuse. */
        uint16_t FUSE_PIN : 1;
        /* Trạng thái bảo mật:
         * 3 = sealed, 2 = unsealed, 1 = full access.
         */
        uint16_t SECURITY_STATE : 2;
        /* OTP write bị chặn do điều kiện không hợp lệ. */
        uint16_t WR_TO_OTP_BLOCKED : 1;
        /* Đang có yêu cầu ghi OTP. */
        uint16_t WR_TO_OTP_PENDING : 1;
        /* Đang thực hiện kiểm tra open-wire. */
        uint16_t OPEN_WIRE_CHECK : 1;
        /* Watchdog từng reset hệ thống. */
        uint16_t WD_WAS_TRIGGERED : 1;
        /* Vừa xảy ra full reset. */
        uint16_t FULL_RESET_OCCURED : 1;
        /* Điều kiện sleep được phép. */
        uint16_t SLEEP_EN_ALLOWED : 1;
        /* Đang ở precharge/predischarge mode. */
        uint16_t PRECHARGE_MODE : 1;
        /* Đang ở config update mode để ghi data memory. */
        uint16_t CONFIG_UPDATE_MODE : 1;
    } bits;
} bq76952_battery_status_t;

typedef struct {
    bool fullAccessOk;
    bool configUpdateOk;
    bool checkOk;
    bool writeOk;
    uint8_t checkResult;
    uint8_t writeResult;
    uint16_t checkDataFailAddr;
    uint16_t writeDataFailAddr;
    uint16_t batteryStatusRaw;
    uint8_t securityState;
    bool otpBlocked;
    bool otpPending;
    uint16_t stackVoltage_mV;
    uint16_t packVoltage_mV;
    int16_t internalTemp_C;
    uint16_t staticConfigSignature;
    uint8_t reg0Config;
    uint8_t reg12Control;
    uint8_t daConfig;
    uint16_t vcellMode;
    uint8_t dchgPinConfig;
    uint8_t ddsgPinConfig;
    uint8_t dfetoffPinConfig;
} bq76952_otp_status_t;

/* Khởi tạo driver/I2C và kiểm tra metadata cơ bản; BMS chịu trách nhiệm cấu hình runtime. */
void bq76952_init(void);
/* Khởi tạo tầng I2C software dùng bởi driver này. */
void bq76952_begin(void);
/* Reset mềm IC bằng subcommand reset. */
void bq76952_reset(void);
/* Chuyển IC vào Config Update mode trước khi ghi data memory. */
void bq76952_enterConfigUpdate(void);
/* Thoát Config Update mode để áp dụng cấu hình mới. */
void bq76952_exitConfigUpdate(void);
/* Kiểm tra IC còn phản hồi trên bus I2C hay không. */
bool bq76952_isConnected(void);
/* Đọc Manufacturing Status Init từ data memory, thường dùng để kiểm tra cấu hình khởi động. */
byte bq76952_getMfgStatusInitRegister(void);
/* Đọc điện áp của 1 cell theo chỉ số 0..15, đơn vị mV. */
int bq76952_getCellVoltage(byte cellNumber);
/* Đọc toàn bộ 16 kênh cell voltage thô của IC. */
void bq76952_getAllCellVoltages(uint16_t *cellArray, uint8_t numCells);
/* Đọc các cell thực sự đang được nối trên pack theo mapping phần cứng hiện tại. */
void bq76952_getOnlyConnectedCellVoltages(uint16_t *cellArray);
/* Đọc dòng tức thời từ thanh ghi CC2 Current, dấu phụ thuộc chiều shunt. */
int bq76952_getCurrent(void);
/* Đọc dòng tức thời thông qua subcommand DA status snapshot. */
int bq76952_getCurrentNow(void);
/* Đọc dòng trung bình thông qua cùng snapshot với CurrentNow. */
int bq76952_getCurrentAvg(void);
/* Kiểm tra manufacturing status có bật quyền điều khiển FET hay chưa. */
bool bq76952_areFETs_Enabled(void);
/* Đọc thanh ghi Manufacturing Status hiện tại. */
unsigned int bq76952_getManufacturingStatus(void);
/* Read raw FET Status() direct command. Bit0=CHG FET, bit2=DSG FET. */
byte bq76952_getFetStatusRaw(void);
/* Đọc tổng điện áp stack/battery, đơn vị mV. */
unsigned int bq76952_getStackVoltage(void);
/* Đọc điện áp tại chân PACK của BQ76952, đơn vị mV. */
unsigned int bq76952_getPackVoltage(void);
/* Đọc nhiệt độ die nội bộ của IC, trả về độ C. */
int16_t bq76952_getInternalTemp(void);
/* Đọc nhiệt độ từ kênh thermistor/chân cảm biến được chọn, trả về độ C. */
int16_t bq76952_getThermistorTemp(bq76952_thermistor_t thermistor);
/* Đọc các cờ bảo vệ đang kích hoạt. */
bq76952_protection_t bq76952_getProtectionStatus(void);
/* Đọc các cờ alert latched nhóm C. */
bq76952_safety_alert_c_t bq76952_getSafetyAlert_C(void);
/* Đọc trạng thái bảo vệ nhiệt độ. */
bq76952_temp_t bq76952_getTemperatureStatus(void);
/* Điều khiển FET sạc/xả bằng subcommand của BQ76952. */
void bq76952_setFET(bq76952_fet_t fet, bq76952_fet_state_t state);
/* Bật quyền cho khối FET hoạt động sau khi khởi tạo. */
void bq76952_setFET_ENABLE(void);
/* Return true if BQ reports the discharge FET output is on. */
bool bq76952_isDischargeFetOn(void);
/* Return true if BQ reports the charge FET output is on. */
bool bq76952_isChargeFetOn(void);
/* Legacy alias for bq76952_isDischargeFetOn(). */
bool bq76952_isDischarging(void);
/* Legacy alias for bq76952_isChargeFetOn(). */
bool bq76952_isCharging(void);
/* Bật/tắt host/manual cell balancing trong data memory của BQ. */
bool bq76952_setCellBalancingEnabled(bool enabled);
/* Cấu hình BQ tự chọn cell balancing trong charge/relax/sleep theo policy truyền vào. */
bool bq76952_configureAutonomousCellBalancing(uint16_t min_cell_mv,
                                              uint8_t start_delta_mv,
                                              uint8_t stop_delta_mv,
                                              int8_t min_cell_temp_c,
                                              int8_t max_cell_temp_c,
                                              int8_t max_internal_temp_c,
                                              uint8_t interval_s,
                                              uint8_t max_cells);
/* Gửi mask cân bằng theo cell logic 0..9 của pack, driver tự đổi sang bit VC của BQ. */
void bq76952_setCellBalanceMask(uint16_t logical_cell_mask);
/* Đọc mask cell balancing active dạng bit VC gốc của BQ. */
uint16_t bq76952_getCellBalanceActiveMask(void);
/* Đọc thời gian BQ đang cân bằng trong phiên hiện tại, đơn vị giây. */
uint16_t bq76952_getCellBalanceActiveSeconds(void);
/* Cấu hình ngưỡng Cell OV và thời gian debounce, đầu vào theo mV/ms. */
bool bq76952_setCellOvervoltageProtection(unsigned int mv, unsigned int ms);
/* Cấu hình ngưỡng Cell UV và thời gian debounce, đầu vào theo mV/ms. */
bool bq76952_setCellUndervoltageProtection(unsigned int mv, unsigned int ms);
/* Ghi cấu hình SCD mặc định hard-code của thư viện. */
bool bq76952_setShortCircuitThreshold(void);
/* Cấu hình chung cho khối protection theo giá trị bitmask của thư viện. */
bool bq76952_setProtectionConfiguration(void);
/* Đặt ngưỡng điện áp stack để IC cho phép vào shutdown. */
bool bq76952_setShutdownStackVoltage(unsigned int voltage);
/* Cấu hình bảo vệ quá dòng khi sạc, đầu vào theo mV trên shunt và ms. */
bool bq76952_setChargingOvercurrentProtection(unsigned int mv, byte ms);
/* Cấu hình ngưỡng nhiệt độ tối đa cho sạc, độ C và thời gian giữ lỗi. */
bool bq76952_setChargingTemperatureMaxLimit(int temp, byte sec);
/* Cấu hình ngưỡng nhiệt độ thấp khi sạc, độ C, recovery và thời gian giữ lỗi. */
bool bq76952_setChargingTemperatureMinLimit(int threshold, int recovery, byte sec);
/* Cấu hình bảo vệ quá dòng khi xả, đầu vào theo mV trên shunt và ms. */
bool bq76952_setDischargingOvercurrentProtection(unsigned int mv, byte ms);
/* Cấu hình ngưỡng OCD3 recovery/trigger theo dòng mA quy đổi nội bộ của IC. */
bool bq76952_setDischargingOvercurrentProtection_OCD3(int16_t mA);
/* Cấu hình dòng phục hồi sau lỗi overcurrent discharge. */
bool bq76952_setDischargingOvercurrentProtection_Recovery(int16_t mA);
/* Cấu hình short-circuit discharge với mã ngưỡng enum và độ trễ micro giây. */
bool bq76952_setDischargingShortcircuitProtection(bq76952_scd_thresh_t thresh, unsigned int us);
/* Cấu hình ngưỡng nhiệt độ tối đa cho xả, độ C và thời gian giữ lỗi. */
bool bq76952_setDischargingTemperatureMaxLimit(int temp, byte sec);
/* Cấu hình ngưỡng nhiệt độ thấp khi xả, độ C, recovery và thời gian giữ lỗi. */
bool bq76952_setDischargingTemperatureMinLimit(int threshold, int recovery, byte sec);
/* Đọc device number để xác minh đúng loại IC. */
unsigned int bq76952_getDeviceNumber(void);
/* Đọc phiên bản phần cứng của IC. */
unsigned int bq76952_getHWVersion(void);
/* Đọc lại security keys hiện có trong IC và so sánh với bộ key của thư viện. */
bool bq76952_checkSecurityKeys(void);
/* Đọc snapshot cell voltage tại thời điểm lỗi COV, cell là offset dữ liệu snapshot. */
unsigned int bq76952_getCOVSnapshot(byte cell);
/* Đưa IC vào Full Access Mode để sửa data memory hoặc ghi OTP. */
bool bq76952_Enter_FullAccessMode(void);
/* Chuẩn bị các điều kiện nguồn/regulator trước khi ghi OTP. */
bool bq76952_configure_before_OTP_write(void);
/* Kiểm tra OTP đã từng được cấu hình/chương trình hay chưa. */
bool bq76952_is_OTP_already_programmed(void);
/* Đọc snapshot cấu hình/trạng thái liên quan OTP và pin config. */
bool bq76952_readOTPStatus(bq76952_otp_status_t *status);
/* Chạy OTP_WR_CHECK và trả chi tiết điều kiện ghi OTP. */
bool bq76952_checkOTPWriteReady(bq76952_otp_status_t *status);
/* Ghi OTP và trả chi tiết kết quả OTP_WRITE. */
bool bq76952_program_OTP_with_status(bq76952_otp_status_t *status);
/* Chạy chuỗi OTP write, chỉ nên dùng sau khi đã xác minh cấu hình cuối cùng. */
bool bq76952_program_OTP(void);
/* Bật pre-regulator nội bộ. */
bool bq76952_setEnablePreRegulator(void);
/* Bật REG0, REG1 và REG2 theo nguồn trên board. */
bool bq76952_configurePowerOutputs(void);
/* Cho phép BQ vào SLEEP tự động nhưng giữ REG2 cấp nguồn hệ thống. */
void bq76952_prepareSleepWithReg2(void);
/* Không cho BQ tự vào SLEEP nữa sau khi hệ thống thức. */
void bq76952_resumeFromSleep(void);
/* Cấu hình gain đo dòng cho shunt 0.5 mOhm trên board. */
bool bq76952_setCurrentSenseCalibration(void);
/* Cấu hình DA (digital/analog) block theo preset của thư viện. */
bool bq76952_setDA_Config(void);
/* Bỏ mask alert nhóm A để lỗi tương ứng có thể kéo chân ALERT. */
bool bq76952_setSF_AlertMask_A(void);
/* Bỏ mask alert nhóm B để lỗi tương ứng có thể kéo chân ALERT. */
bool bq76952_setSF_AlertMask_B(void);
/* Bỏ mask alert nhóm C để lỗi tương ứng có thể kéo chân ALERT. */
bool bq76952_setSF_AlertMask_C(void);
/* Bật/tắt REG1 và REG2 trong REG12_CONTROL. */
bool bq76952_setEnableRegulator(bool enable_reg1, bool enable_reg2);
/* Cấu hình chân ALERT theo bit pattern được hard-code. */
bool bq76952_setAlertPinConfig(void);
/* Cấu hình DFETOFF/BOTHOFF pin để MCU có thể cắt nhanh đường FET qua chân cứng. */
bool bq76952_setDFETOFFPinConfig(bool both_off_mode, bool active_low);
/* Cấu hình DCHG pin thành output trạng thái fault liên quan nhánh charge. */
bool bq76952_setDCHGPinConfig(bool active_low);
/* Cấu hình DDSG pin thành output trạng thái fault liên quan nhánh discharge. */
bool bq76952_setDDSGPinConfig(bool active_low);
/* Cấu hình mask mặc định của alarm status. */
bool bq76952_setDefaultAlarmMaskConfig(void);
/* Chọn cell nào được tham gia phép đo điện áp bằng bitmask VCELL_MODE. */
bool bq76952_setVcellMode(uint16_t vcell_mode);
/* Cấu hình bảo vệ để CHG FET bị ảnh hưởng bởi các fault cần thiết. */
bool bq76952_setEnableCHG_FET_Protection(void);
/* Bật nhóm protection A theo bitmask của thư viện. */
bool bq76952_setEnableProtectionsA(void);
/* Bật nhóm protection B theo bitmask của thư viện. */
bool bq76952_setEnableProtectionsB(void);
/* Bật nhóm protection C theo bitmask của thư viện. */
bool bq76952_setEnableProtectionsC(void);
/* Ghi trực tiếp thanh ghi CHG_FET_PROTECTIONS_A với giá trị caller cung cấp. */
bool bq76952_setCHGFETProtectionsA(byte val);
/* Cấu hình protection mapping cho DSG FET, nhóm A. */
bool bq76952_setDSGFETProtectionsA(void);
/* Cấu hình protection mapping cho DSG FET, nhóm B. */
bool bq76952_setDSGFETProtectionsB(void);
/* Cấu hình protection mapping cho DSG FET, nhóm C. */
bool bq76952_setDSGFETProtectionsC(void);
/* Ghi FET_OPTIONS theo preset của thư viện. */
bool bq76952_setFET_Options(void);
/* Đặt timeout cho pha pre-discharge. */
bool bq76952_setFET_PredischargeTimeout(void);
/* Đặt delta điện áp để kết thúc pre-discharge. */
bool bq76952_setFET_PredischargeStopDelta(void);
/* Ghi điện trở interconnect cho từng cell sense input. */
bool bq76952_setCellInterconnectResistances(void);
/* Đọc raw alarm status trước khi qua lớp mask/clear. */
unsigned int bq76952_getAlertRawStatusRegister(void);
/* Bật chức năng đo nhiệt ở chân TS1. */
bool bq76952_setEnableTS1(void);
/* Bật chức năng đo nhiệt ở chân TS2. */
bool bq76952_setEnableTS2(void);
/* Bật chức năng đo nhiệt ở chân TS3. */
bool bq76952_setEnableTS3(void);
/* Đọc alarm status sau khi IC tổng hợp các nguồn cảnh báo. */
unsigned int bq76952_getAlertStatusRegister(void);
/* Đọc mask runtime quyết định Alarm Raw Status bit nào được latch ra ALERT. */
unsigned int bq76952_getAlarmEnableRegister(void);
/* Ghi mask runtime cho Alarm Enable direct command. */
bool bq76952_setAlarmEnableRegister(uint16_t mask);
/* Ghi 1 vào các bit Alarm Status cần clear để nhả chân ALERT. */
bool bq76952_clearAlertStatusRegister(uint16_t mask);
/* Hàm helper đọc alarm status và trả byte thấp để lớp trên xử lý nhanh. */
byte bq76952_HandleAlarm(void);
/* Đọc battery status và giải mã ra bitfield. */
bq76952_battery_status_t bq76952_getBatteryStatusRegister(void);
/* Đọc raw Battery Status direct-command để debug/protocol. */
unsigned int bq76952_getBatteryStatusRaw(void);
/* Lay ket qua verify cua lan ghi data memory gan nhat. */
bool bq76952_getLastWriteVerify(bq76952_write_verify_t *status);
/* Đọc trực tiếp data memory tại địa chỉ addr với kích thước 1 hoặc 2 byte. */
unsigned int bq76952_readDataMemory(unsigned int addr, int size);
/* Hàm service đơn giản cho alarm, hiện tại chỉ dùng để clear/read trạng thái. */
void bq76952_handle_alarm(void);
/* Hàm debug/chẩn đoán, đọc nhiều trạng thái pin để quan sát khi chạy hệ thống. */
void bq76952_check_batt_status(void);

#ifdef __cplusplus
}
#endif

#endif

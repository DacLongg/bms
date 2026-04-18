#include "bq76952.h"

/* Địa chỉ I2C 7-bit của BQ76952 khi giao tiếp ở chế độ chuẩn. */
#define BQ_I2C_ADDR 0x08U

/* Vùng thanh ghi command/response trực tiếp của BQ76952.
 * 0x3E/0x3F: ghi subcommand hoặc data-memory address.
 * 0x40..   : vùng phản hồi/subcommand data.
 * 0x60..   : checksum + độ dài khi ghi data memory.
 */
#define CMD_DIR_SUBCMD_LOW 0x3EU
#define CMD_DIR_RESP_START 0x40U
#define CMD_DIR_RESP_CHKSUM 0x60U

/* Direct commands đọc dữ liệu tức thời. */
#define CMD_READ_VOLTAGE_STACK 0x34U

#define CMD_DIR_SAFETY_STATUS_A 0x03U
#define CMD_DIR_SAFETY_ALERT_C 0x06U
#define CMD_DIR_FTEMP 0x05U
#define CMD_DIR_BATTERY_STATUS 0x12U
#define CMD_DIR_CC2_CUR 0x3AU
#define CMD_DIR_ALARM_STATUS 0x62U
#define CMD_DIR_ALARM_RAW_STATUS 0x64U
#define CMD_DIR_INT_TEMP 0x68U
#define CMD_DIR_FET_STAT 0x7FU

#define CMD_DEVICE_NUMBER 0x0001U
#define CMD_HW_VERSION 0x0003U
#define CMD_COV_SNAPSHOT 0x0081U
/* Các subcommand phục vụ chuỗi ghi OTP. */
#define SUBCMD_OTP_WR_CHECK 0x00A0U
#define SUBCMD_OTP_WRITE 0x00A1U

/* Vị trí bit trong Safety Status A. */
#define BIT_SA_SC_DCHG 7U
#define BIT_SA_OC2_DCHG 6U
#define BIT_SA_OC1_DCHG 5U
#define BIT_SA_OC_CHG 4U
#define BIT_SA_CELL_OV 3U
#define BIT_SA_CELL_UV 2U

#define BIT_SB_OTINT 6U
#define BIT_SB_OTD 5U
#define BIT_SB_OTC 4U
#define BIT_SB_UTINT 2U
#define BIT_SB_UTD 1U
#define BIT_SB_UTC 0U

/* Mỗi điện áp cell chiếm 2 byte, bắt đầu từ direct command 0x14. */
#define CELL_NO_TO_ADDR(cell_no) ((byte)(0x14U + ((cell_no) * 2U)))
#define LOW_BYTE(data) ((byte)((data) & 0x00FFU))
#define HIGH_BYTE(data) ((byte)(((data) >> 8) & 0x00FFU))
#define BQ_READ_STATUS_BIT(value, bit) (((value) >> (bit)) & 0x01U)

static I2C_HandleTypeDef *g_bq76952_hi2c;

static bq76952_protection_t g_protection_status;
static bq76952_safety_alert_c_t g_safety_alert_c;
static uint16_t g_unseal_key_step_1;
static uint16_t g_unseal_key_step_2;
static uint16_t g_full_access_key_step_1;
static uint16_t g_full_access_key_step_2;

static bool bq76952_write_register(byte reg, const byte *data, uint16_t len);
static bool bq76952_read_register(byte reg, byte *data, uint16_t len);
static bool bq76952_read_raw(byte *data, uint16_t len);
static unsigned int bq76952_directCommand(byte command);
static void bq76952_subCommand(unsigned int data);
static int16_t bq76952_subCommandResponseInt(byte offset);
static byte bq76952_calculateChecksum(byte oldChecksum, byte data);
static void bq76952_writeDataMemory(unsigned int addr, int16_t data, byte noOfBytes);
static void bq76952_writeDataMemoryWithoutConfigUpdate(unsigned int addr, int16_t data, byte noOfBytes);

/* Driver hiện tại dùng lớp I2C software riêng thay vì HAL I2C trực tiếp. */
void bq76952_begin(void)
{
    I2C_Soft_Init();
}

/* Ghi liên tiếp len byte vào một thanh ghi bắt đầu tại reg. */
static bool bq76952_write_register(byte reg, const byte *data, uint16_t len)
{
    return I2C_Soft_WriteDataFromAddress(BQ_I2C_ADDR, reg, (uint8_t *)data, len) == E_OK;
}

/* Đọc liên tiếp len byte từ một thanh ghi bắt đầu tại reg. */
static bool bq76952_read_register(byte reg, byte *data, uint16_t len)
{
    return I2C_Soft_ReadDataFromAddress(BQ_I2C_ADDR, reg, data, len) == E_OK;
}

/* Đọc thô từ bus sau khi đã gửi địa chỉ data memory/subcommand trước đó. */
static bool bq76952_read_raw(byte *data, uint16_t len)
{
    return I2c_Soft_ReadData(BQ_I2C_ADDR, data, len) == E_OK;
}

/* Gửi direct command loại 2 byte rồi ghép little-endian thành unsigned int. */
static unsigned int bq76952_directCommand(byte command)
{
    byte data[2] = {0};

    if (!bq76952_read_register(command, data, 2U)) {
        return 0U;
    }

    return (unsigned int)(((unsigned int)data[1] << 8) | data[0]);
}

/* Gửi subcommand 16-bit vào cặp thanh ghi 0x3E/0x3F. */
static void bq76952_subCommand(unsigned int data)
{
    byte payload[2];

    payload[0] = LOW_BYTE(data);
    payload[1] = HIGH_BYTE(data);
    (void)bq76952_write_register(CMD_DIR_SUBCMD_LOW, payload, 2U);
}

/* Đọc phản hồi 16-bit trong vùng response với offset byte tương ứng. */
static int16_t bq76952_subCommandResponseInt(byte offset)
{
    byte data[2] = {0};

    if (!bq76952_read_register((byte)(CMD_DIR_RESP_START + offset), data, 2U)) {
        return 0;
    }

    return (int16_t)(((uint16_t)data[1] << 8) | data[0]);
}

/* 0x0090 là subcommand chuẩn để vào Config Update mode. */
void bq76952_enterConfigUpdate(void)
{
    bq76952_subCommand(0x0090U);
    HAL_Delay(2U);
}

/* 0x0092 yêu cầu IC thoát Config Update mode và commit cấu hình. */
void bq76952_exitConfigUpdate(void)
{
    bq76952_subCommand(0x0092U);
    HAL_Delay(1U);
}

/* Checksum của BQ76952 là phép cộng dồn rồi đảo bit ở cuối.
 * Hàm này được gọi tuần tự cho từng byte trong payload.
 */
static byte bq76952_calculateChecksum(byte oldChecksum, byte data)
{
    if (oldChecksum == 0U) {
        oldChecksum = data;
    } else {
        oldChecksum = (byte)((~oldChecksum) + data);
    }

    return (byte)(~oldChecksum);
}

/* Ghi một mục data memory.
 * noOfBytes = 1 hoặc 2, footer[1] là tổng độ dài khung theo giao thức BQ76952.
 * Hàm này tự vào/ra Config Update mode cho từng lần ghi.
 */
static void bq76952_writeDataMemory(unsigned int addr, int16_t data, byte noOfBytes)
{
    byte checksum = 0U;
    byte payload[4];
    byte footer[2];
    uint16_t payload_len = (noOfBytes == 1U) ? 3U : 4U;

    checksum = bq76952_calculateChecksum(checksum, LOW_BYTE(addr));
    checksum = bq76952_calculateChecksum(checksum, HIGH_BYTE(addr));
    checksum = bq76952_calculateChecksum(checksum, LOW_BYTE(data));
    checksum = bq76952_calculateChecksum(checksum, HIGH_BYTE(data));

    bq76952_enterConfigUpdate();
    payload[0] = LOW_BYTE(addr);
    payload[1] = HIGH_BYTE(addr);
    payload[2] = LOW_BYTE(data);
    payload[3] = HIGH_BYTE(data);
    footer[0] = checksum;
    footer[1] = (noOfBytes == 1U) ? 0x05U : 0x06U;

    (void)bq76952_write_register(CMD_DIR_SUBCMD_LOW, payload, payload_len);
    (void)bq76952_write_register(CMD_DIR_RESP_CHKSUM, footer, 2U);

    bq76952_exitConfigUpdate();
}

/* Giống hàm trên nhưng dùng khi caller đã xử lý mode hoặc cần ghi nhanh nhiều lệnh liên tiếp. */
static void bq76952_writeDataMemoryWithoutConfigUpdate(unsigned int addr, int16_t data, byte noOfBytes)
{
    byte checksum = 0U;
    byte payload[4];
    byte footer[2];
    uint16_t payload_len = (noOfBytes == 1U) ? 3U : 4U;

    checksum = bq76952_calculateChecksum(checksum, LOW_BYTE(addr));
    checksum = bq76952_calculateChecksum(checksum, HIGH_BYTE(addr));
    checksum = bq76952_calculateChecksum(checksum, LOW_BYTE(data));
    checksum = bq76952_calculateChecksum(checksum, HIGH_BYTE(data));

    payload[0] = LOW_BYTE(addr);
    payload[1] = HIGH_BYTE(addr);
    payload[2] = LOW_BYTE(data);
    payload[3] = HIGH_BYTE(data);
    footer[0] = checksum;
    footer[1] = (noOfBytes == 1U) ? 0x05U : 0x06U;

    (void)bq76952_write_register(CMD_DIR_SUBCMD_LOW, payload, payload_len);
    (void)bq76952_write_register(CMD_DIR_RESP_CHKSUM, footer, 2U);
}

/* Đọc 1 hoặc 2 byte data memory tại địa chỉ addr.
 * size nên là 1 hoặc 2; nếu 2 thì kết quả được ghép little-endian.
 */
unsigned int bq76952_readDataMemory(unsigned int addr, int size)
{
    byte request[2];
    byte data[2] = {0};

    request[0] = LOW_BYTE(addr);
    request[1] = HIGH_BYTE(addr);
    if (!bq76952_write_register(CMD_DIR_SUBCMD_LOW, request, 2U)) {
        return 0U;
    }

    HAL_Delay(2U);
    if (!bq76952_read_raw(data, (uint16_t)size)) {
        return 0U;
    }

    if (size == 1) {
        return data[0];
    }

    return (unsigned int)(((unsigned int)data[1] << 8) | data[0]);
}

bool bq76952_isConnected(void)
{
    return I2C_Soft_WriteData(BQ_I2C_ADDR, NULL, 0U) == E_OK;
}

/* 0x0012 là reset mềm thiết bị. */
void bq76952_reset(void)
{
    bq76952_subCommand(0x0012U);
}

byte bq76952_getMfgStatusInitRegister(void)
{
    return (byte)bq76952_readDataMemory(0x9343U, 1);
}

int bq76952_getCellVoltage(byte cellNumber)
{
    return (int)bq76952_directCommand(CELL_NO_TO_ADDR(cellNumber));
}

/* Đọc toàn bộ 16 kênh cell của IC, kể cả các kênh không dùng. */
void bq76952_getAllCellVoltages(int *cellArray)
{
    if (cellArray == NULL) {
        return;
    }

    for (byte index = 0U; index < 16U; ++index) {
        cellArray[index] = bq76952_getCellVoltage(index);
    }
}

/* Mapping này phản ánh cách pack hiện tại nối 10 cell vào 16 kênh đo của BQ76952.
 * Các phần tử bị bỏ qua là các cell sense không được dùng trong phần cứng này.
 */
void bq76952_getOnlyConnectedCellVoltages(int *cellArray)
{
    int allcells[16];

    if (cellArray == NULL) {
        return;
    }

    bq76952_getAllCellVoltages(allcells);
    cellArray[0] = allcells[0];
    cellArray[1] = allcells[1];
    cellArray[2] = allcells[2];
    cellArray[3] = allcells[3];
    cellArray[4] = allcells[5];
    cellArray[5] = allcells[7];
    cellArray[6] = allcells[9];
    cellArray[7] = allcells[11];
    cellArray[8] = allcells[13];
    cellArray[9] = allcells[15];
}

int bq76952_getCurrent(void)
{
    return (int)bq76952_directCommand(CMD_DIR_CC2_CUR);
}

int bq76952_getCurrentNow(void)
{
    /* 0x0075 trả về một block dữ liệu phép đo tức thời; offset 22 là current now. */
    bq76952_subCommand(0x0075U);
    HAL_Delay(1U);
    return bq76952_subCommandResponseInt(22U);
}

int bq76952_getCurrentAvg(void)
{
    /* Cùng block 0x0075, offset 20 là averaged current. */
    bq76952_subCommand(0x0075U);
    HAL_Delay(1U);
    return bq76952_subCommandResponseInt(20U);
}

unsigned int bq76952_getManufacturingStatus(void)
{
    /* 0x0057 đọc Manufacturing Status, chứa nhiều cờ vận hành như FET_EN, fuse, sleep. */
    bq76952_subCommand(0x0057U);
    HAL_Delay(1U);
    return (unsigned int)bq76952_subCommandResponseInt(0U);
}

bool bq76952_areFETs_Enabled(void)
{
    return (bq76952_getManufacturingStatus() & 0x10U) != 0U;
}

unsigned int bq76952_getStackVoltage(void)
{
    return bq76952_directCommand(CMD_READ_VOLTAGE_STACK);
}

unsigned int bq76952_getDeviceNumber(void)
{
    bq76952_subCommand(CMD_DEVICE_NUMBER);
    HAL_Delay(1U);
    return (unsigned int)bq76952_subCommandResponseInt(0U);
}

unsigned int bq76952_getHWVersion(void)
{
    bq76952_subCommand(CMD_HW_VERSION);
    HAL_Delay(1U);
    return (unsigned int)bq76952_subCommandResponseInt(0U);
}

unsigned int bq76952_getCOVSnapshot(byte cell)
{
    /* Sau lỗi COV, IC lưu lại snapshot để biết cell nào đạt bao nhiêu điện áp tại thời điểm fault. */
    bq76952_subCommand(CMD_COV_SNAPSHOT);
    HAL_Delay(1U);
    return (unsigned int)bq76952_subCommandResponseInt(cell);
}

bool bq76952_is_OTP_already_programmed(void)
{
    byte reg0 = (byte)bq76952_readDataMemory(REG0_CONFIG, 1);
    byte reg12 = (byte)bq76952_readDataMemory(REG12_CONTROL, 1);

    return (reg0 != 0U) || (reg12 != 0U);
}

bool bq76952_checkSecurityKeys(void)
{
    /* 0x0035 đọc liên tiếp 4 word key: unseal step1/2 và full-access step1/2. */
    bq76952_subCommand(0x0035U);
    HAL_Delay(1U);
    g_unseal_key_step_1 = (uint16_t)bq76952_subCommandResponseInt(0U);

    bq76952_subCommand(0x0035U);
    HAL_Delay(1U);
    g_unseal_key_step_2 = (uint16_t)bq76952_subCommandResponseInt(2U);

    bq76952_subCommand(0x0035U);
    HAL_Delay(1U);
    g_full_access_key_step_1 = (uint16_t)bq76952_subCommandResponseInt(4U);

    bq76952_subCommand(0x0035U);
    HAL_Delay(1U);
    g_full_access_key_step_2 = (uint16_t)bq76952_subCommandResponseInt(6U);

    return (g_full_access_key_step_1 == FULL_ACCESS_KEY_STEP_1) &&
           (g_full_access_key_step_2 == FULL_ACCESS_KEY_STEP_2);
}

bq76952_battery_status_t bq76952_getBatteryStatusRegister(void)
{
    bq76952_battery_status_t batt_stat = {0};
    unsigned int regData = bq76952_directCommand(CMD_DIR_BATTERY_STATUS);

    batt_stat.bits.SLEEP_MODE = BQ_READ_STATUS_BIT(regData, 15U);
    batt_stat.bits.SHUTDOWN_PENDING = BQ_READ_STATUS_BIT(regData, 13U);
    batt_stat.bits.PERMANENT_FAULT = BQ_READ_STATUS_BIT(regData, 12U);
    batt_stat.bits.SAFETY_FAULT = BQ_READ_STATUS_BIT(regData, 11U);
    batt_stat.bits.FUSE_PIN = BQ_READ_STATUS_BIT(regData, 10U);
    batt_stat.bits.SECURITY_STATE = (uint16_t)((regData >> 8U) & 0x03U);
    batt_stat.bits.WR_TO_OTP_BLOCKED = BQ_READ_STATUS_BIT(regData, 7U);
    batt_stat.bits.WR_TO_OTP_PENDING = BQ_READ_STATUS_BIT(regData, 6U);
    batt_stat.bits.OPEN_WIRE_CHECK = BQ_READ_STATUS_BIT(regData, 5U);
    batt_stat.bits.WD_WAS_TRIGGERED = BQ_READ_STATUS_BIT(regData, 4U);
    batt_stat.bits.FULL_RESET_OCCURED = BQ_READ_STATUS_BIT(regData, 3U);
    batt_stat.bits.SLEEP_EN_ALLOWED = BQ_READ_STATUS_BIT(regData, 2U);
    batt_stat.bits.PRECHARGE_MODE = BQ_READ_STATUS_BIT(regData, 1U);
    batt_stat.bits.CONFIG_UPDATE_MODE = BQ_READ_STATUS_BIT(regData, 0U);

    return batt_stat;
}

bool bq76952_Enter_FullAccessMode(void)
{
    bq76952_battery_status_t batt_st;

    /* Nếu key trong chip chưa khớp bộ key của thư viện thì ghi lại bộ key mặc định trước. */
    if (!bq76952_checkSecurityKeys()) {
        bq76952_writeDataMemoryWithoutConfigUpdate(0x925BU, FULL_ACCESS_KEY_STEP_1, 2U);
        bq76952_writeDataMemoryWithoutConfigUpdate(0x925DU, FULL_ACCESS_KEY_STEP_2, 2U);
        g_full_access_key_step_1 = FULL_ACCESS_KEY_STEP_1;
        g_full_access_key_step_2 = FULL_ACCESS_KEY_STEP_2;
    }

    batt_st = bq76952_getBatteryStatusRegister();
    if (batt_st.bits.SECURITY_STATE == 3U) {
        /* Sealed: cần unseal rồi mới vào full access. */
        bq76952_subCommand(g_unseal_key_step_1);
        bq76952_subCommand(g_unseal_key_step_2);
        bq76952_subCommand(g_full_access_key_step_1);
        bq76952_subCommand(g_full_access_key_step_2);
    } else if (batt_st.bits.SECURITY_STATE == 2U) {
        /* Unsealed: chỉ cần gửi full-access key. */
        bq76952_subCommand(g_full_access_key_step_1);
        bq76952_subCommand(g_full_access_key_step_2);
    }

    batt_st = bq76952_getBatteryStatusRegister();
    return batt_st.bits.SECURITY_STATE == 1U;
}

bool bq76952_configure_before_OTP_write(void)
{
    bq76952_setEnablePreRegulator();
    bq76952_setEnableRegulator(true, true);
    return true;
}

bool bq76952_program_OTP(void)
{
    byte otp_wr_check;
    byte otp_write_response;
    bq76952_battery_status_t batt_st;

    if (bq76952_is_OTP_already_programmed()) {
        return false;
    }

    if (!bq76952_configure_before_OTP_write()) {
        return false;
    }

    bq76952_enterConfigUpdate();
    bq76952_subCommand(SUBCMD_OTP_WR_CHECK);
    HAL_Delay(1000U);

    /* bit7 của otp_wr_check báo điều kiện ghi hợp lệ; battery status báo có bị block hay không. */
    otp_wr_check = (byte)bq76952_subCommandResponseInt(0U);
    batt_st = bq76952_getBatteryStatusRegister();
    if (((otp_wr_check & 0x80U) == 0U) || batt_st.bits.WR_TO_OTP_BLOCKED) {
        bq76952_exitConfigUpdate();
        return false;
    }

    bq76952_subCommand(SUBCMD_OTP_WRITE);
    otp_write_response = (byte)bq76952_subCommandResponseInt(0U);
    HAL_Delay(10U);
    bq76952_exitConfigUpdate();

    /* 0x81 nghĩa là lệnh đã chấp nhận và hoàn tất thành công theo response bitmask hiện tại. */
    return (otp_write_response & 0x81U) == 0x81U;
}

float bq76952_getInternalTemp(void)
{
    /* BQ76952 trả nhiệt độ theo 0.1 Kelvin, cần đổi sang Celsius. */
    float raw = (float)bq76952_directCommand(CMD_DIR_INT_TEMP) / 10.0f;
    return raw - 273.15f;
}

float bq76952_getThermistorTemp(bq76952_thermistor_t thermistor)
{
    byte command = 0x70U;

    /* Mỗi nguồn nhiệt độ có một direct command riêng, cách nhau 2 byte. */
    switch (thermistor) {
    case TS1:
        command = 0x70U;
        break;
    case TS2:
        command = 0x72U;
        break;
    case TS3:
        command = 0x74U;
        break;
    case HDQ:
        command = 0x76U;
        break;
    case DCHG:
        command = 0x78U;
        break;
    case DDSG:
        command = 0x7AU;
        break;
    default:
        break;
    }

    return ((float)bq76952_directCommand(command) / 10.0f) - 273.15f;
}

bq76952_protection_t bq76952_getProtectionStatus(void)
{
    byte regData = (byte)bq76952_directCommand(CMD_DIR_SAFETY_STATUS_A);

    g_protection_status.bits.SC_DCHG = BQ_READ_STATUS_BIT(regData, BIT_SA_SC_DCHG);
    g_protection_status.bits.OC2_DCHG = BQ_READ_STATUS_BIT(regData, BIT_SA_OC2_DCHG);
    g_protection_status.bits.OC1_DCHG = BQ_READ_STATUS_BIT(regData, BIT_SA_OC1_DCHG);
    g_protection_status.bits.OC_CHG = BQ_READ_STATUS_BIT(regData, BIT_SA_OC_CHG);
    g_protection_status.bits.CELL_OV = BQ_READ_STATUS_BIT(regData, BIT_SA_CELL_OV);
    g_protection_status.bits.CELL_UV = BQ_READ_STATUS_BIT(regData, BIT_SA_CELL_UV);

    return g_protection_status;
}

bq76952_safety_alert_c_t bq76952_getSafetyAlert_C(void)
{
    byte regData = (byte)bq76952_directCommand(CMD_DIR_SAFETY_ALERT_C);

    g_safety_alert_c.bits.OCD3 = BQ_READ_STATUS_BIT(regData, 7U);
    g_safety_alert_c.bits.SCDL = BQ_READ_STATUS_BIT(regData, 6U);
    g_safety_alert_c.bits.OCDL = BQ_READ_STATUS_BIT(regData, 5U);
    g_safety_alert_c.bits.COVL = BQ_READ_STATUS_BIT(regData, 4U);
    g_safety_alert_c.bits.PTOS = BQ_READ_STATUS_BIT(regData, 3U);

    return g_safety_alert_c;
}

bq76952_temp_t bq76952_getTemperatureStatus(void)
{
    bq76952_temp_t status = {0};
    byte regData = (byte)bq76952_directCommand(CMD_DIR_FTEMP);

    /* FTEMP là bitmap trạng thái nhiệt, không phải giá trị nhiệt độ tuyệt đối. */
    status.bits.OVERTEMP_FET = BQ_READ_STATUS_BIT(regData, BIT_SB_OTC);
    status.bits.OVERTEMP_INTERNAL = BQ_READ_STATUS_BIT(regData, BIT_SB_OTINT);
    status.bits.OVERTEMP_DCHG = BQ_READ_STATUS_BIT(regData, BIT_SB_OTD);
    status.bits.OVERTEMP_CHG = BQ_READ_STATUS_BIT(regData, BIT_SB_OTC);
    status.bits.UNDERTEMP_INTERNAL = BQ_READ_STATUS_BIT(regData, BIT_SB_UTINT);
    status.bits.UNDERTEMP_DCHG = BQ_READ_STATUS_BIT(regData, BIT_SB_UTD);
    status.bits.UNDERTEMP_CHG = BQ_READ_STATUS_BIT(regData, BIT_SB_UTC);

    return status;
}

void bq76952_setFET(bq76952_fet_t fet, bq76952_fet_state_t state)
{
    /* Mặc định 0x0096 là ALL_FETS_ON; các nhánh OFF dùng subcommand riêng cho CHG/DSG/ALL. */
    unsigned int subcmd = 0x0096U;

    if (state == OFF) {
        switch (fet) {
        case DCH:
            subcmd = 0x0093U;
            break;
        case CHG:
            subcmd = 0x0094U;
            break;
        case ALL:
        default:
            subcmd = 0x0095U;
            break;
        }
    }

    bq76952_subCommand(subcmd);
}

void bq76952_setFET_ENABLE(void)
{
    bq76952_subCommand(0x0022U);
}

bool bq76952_isCharging(void)
{
    return ((byte)bq76952_directCommand(CMD_DIR_FET_STAT) & 0x01U) != 0U;
}

bool bq76952_isDischarging(void)
{
    return ((byte)bq76952_directCommand(CMD_DIR_FET_STAT) & 0x04U) != 0U;
}

void bq76952_setCellOvervoltageProtection(unsigned int mv, unsigned int ms)
{
    /* Cell OV threshold trong data memory dùng bước ~50.6 mV/LSB.
     * Delay dùng bước ~3.3 ms và mã hóa theo công thức datasheet: code = time/3.3 - 2.
     */
    byte thresh = (byte)(mv / 50.6f);
    uint16_t dly = (uint16_t)(ms / 3.3f) - 2U;

    /* 86 ~ 4.35 V, 74 ~ khoảng 250 ms: đây là fallback an toàn của thư viện. */
    if (thresh < 20U || thresh > 110U) {
        thresh = 86U;
    }
    if (dly < 1U || dly > 2047U) {
        dly = 74U;
    }

    bq76952_writeDataMemory(0x9278U, thresh, 1U);
    bq76952_writeDataMemory(0x9279U, (int16_t)dly, 2U);
}

void bq76952_setCellUndervoltageProtection(unsigned int mv, unsigned int ms)
{
    /* UV cũng dùng bước ~50.6 mV/LSB và delay cùng công thức với OV. */
    byte thresh = (byte)(mv / 50.6f);
    uint16_t dly = (uint16_t)(ms / 3.3f) - 2U;

    if (thresh < 20U || thresh > 90U) {
        thresh = 50U;
    }
    if (dly < 1U || dly > 2047U) {
        dly = 74U;
    }

    bq76952_writeDataMemory(0x9275U, thresh, 1U);
    bq76952_writeDataMemory(0x9276U, (int16_t)dly, 2U);
}

void bq76952_setShortCircuitThreshold(void)
{
    /* Preset hiện tại:
     * threshold code = 2 và delay code = 30.
     * Ý nghĩa vật lý phụ thuộc Rsense và bảng mã SCD trong datasheet.
     */
    bq76952_writeDataMemory(SCD_THRESHOLD_CONFIG, 2, 1U);
    bq76952_writeDataMemory(SCD_DELAY_CONFIG, 30, 1U);
}

void bq76952_setProtectionConfiguration(void)
{
    /* 0x0600 là bitmask cấu hình cách protection phản ứng/latch theo lựa chọn của dự án. */
    bq76952_writeDataMemory(PROTECTION_CONFIGURATION, 0x0600, 2U);
}

void bq76952_setShutdownStackVoltage(unsigned int voltage)
{
    bq76952_writeDataMemory(SHUTDOWN_STACK_VOLTAGE, (int16_t)voltage, 2U);
}

void bq76952_setChargingOvercurrentProtection(unsigned int mv, byte ms)
{
    /* COC dùng bước 2 mV/LSB trên shunt; delay cũng dùng khoảng 3.3 ms/LSB. */
    byte thresh = (byte)(mv / 2U);
    byte dly = (byte)(ms / 3.3f) - 2U;

    if (thresh < 2U || thresh > 62U) {
        thresh = 2U;
    }
    if (dly < 1U || dly > 127U) {
        dly = 4U;
    }

    bq76952_writeDataMemory(0x9280U, thresh, 1U);
    bq76952_writeDataMemory(0x9281U, dly, 1U);
}

void bq76952_setDischargingOvercurrentProtection(unsigned int mv, byte ms)
{
    /* Ghi cùng ngưỡng cho OCD1 và OCD2, nhưng delay được ghi ở vùng OCD1 delay. */
    byte thresh = (byte)(mv / 2U);
    byte dly = (byte)(ms / 3.3f) - 2U;

    if (thresh < 2U || thresh > 100U) {
        thresh = 2U;
    }
    if (dly < 1U || dly > 127U) {
        dly = 1U;
    }

    bq76952_writeDataMemory(0x9282U, thresh, 1U);
    HAL_Delay(2U);
    bq76952_writeDataMemory(0x9284U, thresh, 1U);
    bq76952_writeDataMemory(0x9283U, dly, 1U);
}

void bq76952_setDischargingOvercurrentProtection_OCD3(int16_t mA)
{
    /* OCD3 dùng giá trị dòng theo đơn vị và dấu được BQ76952 định nghĩa trong data memory. */
    bq76952_writeDataMemory(0x928AU, mA, 2U);
}

void bq76952_setDischargingOvercurrentProtection_Recovery(int16_t mA)
{
    bq76952_writeDataMemory(0x928DU, mA, 2U);
}

void bq76952_setDischargingShortcircuitProtection(bq76952_scd_thresh_t thresh, unsigned int us)
{
    /* Delay SCD xấp xỉ 15 us/LSB, thư viện cộng 1 để tránh code 0. */
    byte dly = (byte)(us / 15U) + 1U;

    if (dly < 1U || dly > 31U) {
        dly = 2U;
    }

    bq76952_writeDataMemory(0x9286U, (int16_t)thresh, 1U);
    bq76952_writeDataMemory(0x9287U, dly, 1U);
}

void bq76952_setChargingTemperatureMaxLimit(int temp, byte sec)
{
    /* Giới hạn temp lưu trực tiếp theo độ C nguyên; sec là thời gian xác nhận lỗi. */
    if (temp < -40 || temp > 120) {
        temp = 55;
    }

    bq76952_writeDataMemory(0x929AU, (int16_t)temp, 1U);
    bq76952_writeDataMemory(0x929BU, sec, 1U);
}

void bq76952_setDischargingTemperatureMaxLimit(int temp, byte sec)
{
    if (temp < -40 || temp > 120) {
        temp = 60;
    }

    bq76952_writeDataMemory(0x929DU, (int16_t)temp, 1U);
    bq76952_writeDataMemory(0x929EU, sec, 1U);
}

void bq76952_setEnablePreRegulator(void)
{
    /* REG0 bit0 = bật pre-regulator. */
    bq76952_writeDataMemory(REG0_CONFIG, 0x01, 1U);
}

void bq76952_setDA_Config(void)
{
    /* 0x01 là preset DA configuration mà project này đang dùng. */
    bq76952_writeDataMemory(DA_CONFIGURATION, 0x01, 1U);
}

void bq76952_setSF_AlertMask_A(void)
{
    bq76952_writeDataMemory(SF_ALERT_MASK_A, 0x00, 1U);
}

void bq76952_setSF_AlertMask_B(void)
{
    bq76952_writeDataMemory(SF_ALERT_MASK_B, 0x00, 1U);
}

void bq76952_setSF_AlertMask_C(void)
{
    bq76952_writeDataMemory(SF_ALERT_MASK_C, 0x00, 1U);
}

void bq76952_setEnableRegulator(bool enable_reg1, bool enable_reg2)
{
    /* 0xCC giữ các bit nền của REG12_CONTROL, sau đó OR thêm bit enable cho REG1/REG2. */
    byte reg12 = 0xCCU;

    reg12 |= enable_reg1 ? 0x01U : 0x00U;
    reg12 |= enable_reg2 ? 0x10U : 0x00U;
    bq76952_writeDataMemory(REG12_CONTROL, reg12, 1U);
}

void bq76952_setAlertPinConfig(void)
{
    /* 0x2A quy định mode chân ALERT theo cấu hình phần cứng mong muốn của dự án. */
    bq76952_writeDataMemory(ALERT_PIN_CONFIG, 0x2A, 1U);
}

void bq76952_setDefaultAlarmMaskConfig(void)
{
    /* 0xF800 mask/bật nhóm alarm tương ứng trong Default Alarm Mask. */
    bq76952_writeDataMemory(DEFAULT_ALARM_MASK_CONFIG, (int16_t)0xF800U, 2U);
}

void bq76952_setVcellMode(uint16_t vcell_mode)
{
    /* Mỗi bit tương ứng một kênh VCx; bit = 1 nghĩa là tham gia đo điện áp cell. */
    bq76952_writeDataMemory(VCELL_MODE, (int16_t)vcell_mode, 2U);
}

void bq76952_setEnableCHG_FET_Protection(void)
{
    /* Ghi 0 để bỏ chặn các protection trên CHG FET theo preset hiện tại. */
    bq76952_writeDataMemory(CHG_FET_PROTECTION_A, 0x00, 1U);
    bq76952_writeDataMemory(CHG_FET_PROTECTION_B, 0x00, 1U);
    bq76952_writeDataMemory(CHG_FET_PROTECTION_C, 0x00, 1U);
}

void bq76952_setEnableProtectionsA(void)
{
    /* 0xFC bật hầu hết protection nhóm A, trừ các bit thấp không dùng. */
    bq76952_writeDataMemory(ENABLE_PROTECTIONS_A, 0xFC, 1U);
}

void bq76952_setEnableProtectionsB(void)
{
    bq76952_writeDataMemory(ENABLE_PROTECTIONS_B, 0xF7, 1U);
}

void bq76952_setEnableProtectionsC(void)
{
    /* 0x80 hiện chỉ bật protection bit cao của nhóm C. */
    bq76952_writeDataMemory(ENABLE_PROTECTIONS_C, 0x80, 1U);
}

void bq76952_setCHGFETProtectionsA(byte val)
{
    bq76952_writeDataMemory(CHG_FET_PROTECTIONS_A, val, 1U);
}

void bq76952_setCellInterconnectResistances(void)
{
    /* Ghi tuần tự 16 mục vì BQ76952 cho phép hiệu chỉnh riêng từng liên kết cell sense. */
    for (byte cell = 0U; cell < 16U; ++cell) {
        bq76952_writeDataMemory(CELL_INTERCONNECT_RESISTANCE + ((unsigned int)cell * 2U),
                                CELL_INTERCONNECT_RESISTANCE_MOHM,
                                2U);
    }
}

void bq76952_setDSGFETProtectionsA(void)
{
    /* 0xA4 là preset mapping fault -> DSG FET action của dự án. */
    bq76952_writeDataMemory(DSG_FET_PROTECTION_A, 0xA4, 1U);
}

void bq76952_setDSGFETProtectionsB(void)
{
    bq76952_writeDataMemory(DSG_FET_PROTECTION_B, 0x00, 1U);
}

void bq76952_setDSGFETProtectionsC(void)
{
    bq76952_writeDataMemory(DSG_FET_PROTECTION_C, 0x80, 1U);
}

void bq76952_setFET_Options(void)
{
    /* 0x1D cấu hình hành vi FET/precharge/predischarge theo thiết kế board hiện tại. */
    bq76952_writeDataMemory(FET_OPTIONS, 0x1D, 1U);
}

void bq76952_setFET_PredischargeTimeout(void)
{
    bq76952_writeDataMemory(FET_PREDISCHARGE_TIMEOUT, 0x00, 1U);
}

void bq76952_setFET_PredischargeStopDelta(void)
{
    /* 100 là delta điện áp kết thúc predischarge theo đơn vị mã hóa của BQ76952. */
    bq76952_writeDataMemory(FET_PREDISCHARGE_STOP_DELTA, 100, 1U);
}

void bq76952_setEnableTS1(void)
{
    bq76952_writeDataMemory(TS1_CONFIG, 0x00, 1U);
}

void bq76952_setEnableTS2(void)
{
    bq76952_writeDataMemory(TS2_CONFIG, 0x00, 1U);
}

void bq76952_setEnableTS3(void)
{
    bq76952_writeDataMemory(TS3_CONFIG, 0x00, 1U);
}

unsigned int bq76952_getAlertStatusRegister(void)
{
    return bq76952_directCommand(CMD_DIR_ALARM_STATUS);
}

unsigned int bq76952_getAlertRawStatusRegister(void)
{
    return bq76952_directCommand(CMD_DIR_ALARM_RAW_STATUS);
}

byte bq76952_HandleAlarm(void)
{
    return (byte)bq76952_getAlertStatusRegister();
}

void bq76952_init(I2C_HandleTypeDef *hi2c)
{
    unsigned int hwVersion;
    unsigned int devNumber;

    g_bq76952_hi2c = hi2c;
    bq76952_begin();

    /* Chờ IC hoàn tất power-up trước khi đọc metadata và ghi cấu hình. */
    HAL_Delay(1000U);

    devNumber = bq76952_getDeviceNumber();
    hwVersion = bq76952_getHWVersion();
    (void)devNumber;
    (void)hwVersion;
    (void)g_bq76952_hi2c;

    /* Chuỗi dưới đây là preset khởi tạo mặc định cho pack hiện tại:
     * - bật đo cell theo bitmask 0xAAAF
     * - bật protection cần thiết
     * - cấu hình FET và một số ngưỡng an toàn mặc định
     */
    bq76952_setVcellMode(0xAAAFU);
    bq76952_setDA_Config();
    bq76952_setEnableProtectionsA();
    bq76952_setEnableProtectionsC();
    bq76952_setEnableCHG_FET_Protection();
    bq76952_setProtectionConfiguration();
    bq76952_setEnableTS1();
    bq76952_setShortCircuitThreshold();
    bq76952_setFET_Options();
    bq76952_setFET_PredischargeTimeout();
    bq76952_setFET_PredischargeStopDelta();
    bq76952_setDischargingOvercurrentProtection(4U, 255U);
    bq76952_setDischargingOvercurrentProtection_Recovery(-3000);
    bq76952_setDischargingOvercurrentProtection_OCD3(-1000);
    bq76952_setCellOvervoltageProtection(4200U, 100U);
    bq76952_setCellUndervoltageProtection(1500U, 100U);
    bq76952_setDSGFETProtectionsA();
    bq76952_setDSGFETProtectionsB();
    bq76952_setDSGFETProtectionsC();

    HAL_Delay(500U);
    if (!bq76952_areFETs_Enabled()) {
        bq76952_setFET_ENABLE();
    }

    /* Đọc raw alert một lần để đồng bộ trạng thái ban đầu. */
    HAL_Delay(500U);
    (void)bq76952_getAlertRawStatusRegister();
}

void bq76952_handle_alarm(void)
{
    (void)bq76952_HandleAlarm();
}

void bq76952_check_batt_status(void)
{
    int cellArray[16];
    bool isDischarging;
    bool isCharging;
    bq76952_battery_status_t batt_status;
    unsigned int alarmStatus;
    unsigned int cov[16] = {0};
    unsigned int manufacturing_status;
    int current_now;
    bq76952_temp_t temperature_status;
    float internal_temp;
    unsigned int stack_voltage;
    int cell_voltage;
    unsigned int alert_raw;
    bq76952_protection_t status;
    bq76952_safety_alert_c_t safety_alert_c;

    /* Hàm này chủ yếu phục vụ debug/runtime inspection, chưa có logic xử lý fault hoàn chỉnh. */
    HAL_Delay(50U);
    bq76952_getAllCellVoltages(cellArray);
    alarmStatus = bq76952_getAlertStatusRegister();
    isDischarging = bq76952_isDischarging();
    isCharging = bq76952_isCharging();
    batt_status = bq76952_getBatteryStatusRegister();

    if (batt_status.bits.FULL_RESET_OCCURED) {
    }

    (void)bq76952_readDataMemory(0x9236U, 1);
    (void)bq76952_readDataMemory(0x9286U, 1);

    manufacturing_status = bq76952_getManufacturingStatus();
    current_now = bq76952_getCurrentNow();
    temperature_status = bq76952_getTemperatureStatus();
    internal_temp = bq76952_getInternalTemp();
    stack_voltage = bq76952_getStackVoltage();
    cell_voltage = bq76952_getCellVoltage(15U);
    alert_raw = bq76952_getAlertRawStatusRegister();
    status = bq76952_getProtectionStatus();
    safety_alert_c = bq76952_getSafetyAlert_C();

    (void)alarmStatus;
    (void)isDischarging;
    (void)isCharging;
    (void)manufacturing_status;
    (void)current_now;
    (void)temperature_status;
    (void)internal_temp;
    (void)stack_voltage;
    (void)cell_voltage;
    (void)alert_raw;
    (void)safety_alert_c;

    if (status.bits.CELL_OV) {
        for (int i = 0; i < 16; ++i) {
            cov[i] = bq76952_getCOVSnapshot((byte)i);
        }
    }

    if (status.bits.SC_DCHG) {
    }

    (void)cov;
}

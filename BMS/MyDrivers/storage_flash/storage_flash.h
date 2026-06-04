#ifndef STORAGE_FLASH_H
#define STORAGE_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l0xx_hal.h"

#define STORAGE_FLASH_BASE_ADDRESS       0x08007C00UL
#define STORAGE_FLASH_SIZE_BYTES         1024UL
#define STORAGE_FLASH_MAGIC              0x424D5355UL
#define STORAGE_FLASH_VERSION            2UL
#define STORAGE_FLASH_CURRENT_CALIBRATION_DEFAULT_PPM 1000000UL

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t writeCounter;
    uint32_t chargeThroughput_mAh;
    uint32_t dischargeThroughput_mAh;
    uint32_t equivalentCycle_milliCycles;
    uint32_t nominalCapacity_mAh;
    uint32_t currentCalibrationGainPpm;
    uint32_t checksum;
} storage_flash_record_t;

bool storage_flash_load(storage_flash_record_t *record);
bool storage_flash_save(const storage_flash_record_t *record);
void storage_flash_make_default(storage_flash_record_t *record);

#endif

#include "storage_flash.h"

#include <stddef.h>
#include <string.h>

static uint32_t storage_flash_checksum(const storage_flash_record_t *record);
static bool storage_flash_is_valid_address_range(void);

void storage_flash_make_default(storage_flash_record_t *record)
{
    if (record == NULL) {
        return;
    }

    memset(record, 0, sizeof(*record));
    record->magic = STORAGE_FLASH_MAGIC;
    record->version = STORAGE_FLASH_VERSION;
    record->checksum = storage_flash_checksum(record);
}

bool storage_flash_load(storage_flash_record_t *record)
{
    const storage_flash_record_t *stored = (const storage_flash_record_t *)STORAGE_FLASH_BASE_ADDRESS;

    if (record == NULL) {
        return false;
    }

    memcpy(record, stored, sizeof(*record));
    if (record->magic != STORAGE_FLASH_MAGIC) {
        return false;
    }
    if (record->version != STORAGE_FLASH_VERSION) {
        return false;
    }
    if (record->checksum != storage_flash_checksum(record)) {
        return false;
    }

    return true;
}

bool storage_flash_save(const storage_flash_record_t *record)
{
    FLASH_EraseInitTypeDef erase = {0};
    storage_flash_record_t write_record;
    uint32_t page_error = 0U;
    const uint32_t *words;
    uint32_t address = STORAGE_FLASH_BASE_ADDRESS;
    HAL_StatusTypeDef status;

    if ((record == NULL) || !storage_flash_is_valid_address_range()) {
        return false;
    }

    write_record = *record;
    write_record.magic = STORAGE_FLASH_MAGIC;
    write_record.version = STORAGE_FLASH_VERSION;
    write_record.checksum = storage_flash_checksum(&write_record);

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return false;
    }

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = STORAGE_FLASH_BASE_ADDRESS;
    erase.NbPages = STORAGE_FLASH_SIZE_BYTES / FLASH_PAGE_SIZE;

    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status == HAL_OK) {
        words = (const uint32_t *)&write_record;
        for (uint32_t i = 0U; i < (sizeof(write_record) / sizeof(uint32_t)); ++i) {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, words[i]);
            if (status != HAL_OK) {
                break;
            }
            address += sizeof(uint32_t);
        }
    }

    (void)HAL_FLASH_Lock();
    return status == HAL_OK;
}

static uint32_t storage_flash_checksum(const storage_flash_record_t *record)
{
    const uint32_t *words = (const uint32_t *)record;
    uint32_t checksum = 0xA5A55A5AU;
    uint32_t word_count = sizeof(*record) / sizeof(uint32_t);

    for (uint32_t i = 0U; i < (word_count - 1U); ++i) {
        checksum ^= words[i] + 0x9E3779B9UL + (checksum << 6U) + (checksum >> 2U);
    }

    return checksum;
}

static bool storage_flash_is_valid_address_range(void)
{
    uint32_t flash_end = FLASH_BASE + FLASH_SIZE;

    return (STORAGE_FLASH_BASE_ADDRESS >= FLASH_BASE) &&
           ((STORAGE_FLASH_BASE_ADDRESS + STORAGE_FLASH_SIZE_BYTES) <= flash_end) &&
           ((STORAGE_FLASH_BASE_ADDRESS % FLASH_PAGE_SIZE) == 0U) &&
           ((STORAGE_FLASH_SIZE_BYTES % FLASH_PAGE_SIZE) == 0U);
}

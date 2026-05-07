#include "sequence_storage.h"
#include "sequence_engine.h"
#include "supervisor_comms.h"
#include "main.h"
#include "iwdg.h"
#include <string.h>

// STM32F446RE sector 7: 128 KB at 0x08060000.
// Used for sequence persistence — safely above typical firmware footprint.
#define STORAGE_MAGIC   0xA55A1234UL
#define STORAGE_SECTOR  FLASH_SECTOR_7
#define STORAGE_ADDR    0x08060000UL

typedef struct __attribute__((packed)) {
    uint32_t           magic;
    uint8_t            length;
    uint8_t            _pad[3];
    SequenceMetadata_t metadata;                   // 40 bytes
    SequenceStep_t     steps[MAX_SEQUENCE_STEPS];  // 32 * 5 = 160 bytes
} PersistedSequence_t;                             // total: 208 bytes

void SequenceStorage_Save(SequenceStep_t *steps, uint8_t length, const SequenceMetadata_t *meta) {
    // Update in-memory table immediately so the engine can use it
    SequenceStep_t *table = SequenceEngine_GetTable();
    memcpy(table, steps, length * sizeof(SequenceStep_t));
    SequenceEngine_SetLength(length);

    // Build flash record
    PersistedSequence_t record;
    record.magic     = STORAGE_MAGIC;
    record.length    = length;
    record._pad[0]   = record._pad[1] = record._pad[2] = 0;
    memcpy(&record.metadata, meta, sizeof(SequenceMetadata_t));
    memcpy(record.steps, steps, length * sizeof(SequenceStep_t));
    memset(&record.steps[length], 0,
           (MAX_SEQUENCE_STEPS - length) * sizeof(SequenceStep_t));

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        .Sector       = STORAGE_SECTOR,
        .NbSectors    = 1,
    };
    uint32_t erase_error = 0;
    HAL_IWDG_Refresh(&hiwdg);          // sector erase can take ~1-2 s; keep watchdog fed
    HAL_FLASHEx_Erase(&erase, &erase_error);

    const uint32_t *src   = (const uint32_t *)&record;
    uint32_t        addr  = STORAGE_ADDR;
    size_t          words = (sizeof(record) + 3) / 4;
    for (size_t i = 0; i < words; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        addr += 4;
    }

    HAL_FLASH_Lock();
}

void SequenceStorage_Load(void) {
    const PersistedSequence_t *record = (const PersistedSequence_t *)STORAGE_ADDR;

    if (record->magic  != STORAGE_MAGIC ||
        record->length == 0              ||
        record->length >  MAX_SEQUENCE_STEPS) {
        return;  // flash is blank or corrupt — start with no sequence
    }

    SequenceStep_t *table = SequenceEngine_GetTable();
    memcpy(table, record->steps, record->length * sizeof(SequenceStep_t));
    SequenceEngine_SetLength(record->length);

    // Restore sequence name so heartbeats report correctly before next receive
    char name[16];
    memcpy(name, record->metadata.seq_name, 15);
    name[15] = '\0';
    SupervisorComms_SetActiveSeqName(name);
}

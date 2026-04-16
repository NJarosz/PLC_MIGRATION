#include "sequence_storage.h"
#include "sequence_engine.h"
#include <string.h>

void SequenceStorage_Save(SequenceStep_t *steps, uint8_t length) {
    SequenceStep_t *table = SequenceEngine_GetTable();

    for (int i = 0; i < length; i++) {
        table[i] = steps[i];
    }

    SequenceEngine_SetLength(length);
}

void SequenceStorage_Load(void) {
    // later: read from flash
}

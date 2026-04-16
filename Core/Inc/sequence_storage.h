#ifndef SEQUENCE_STORAGE_H
#define SEQUENCE_STORAGE_H

#include "sequence_engine.h"

void SequenceStorage_Save(SequenceStep_t *steps, uint8_t length);
void SequenceStorage_Load(void);

#endif

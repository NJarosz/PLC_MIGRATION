#ifndef SEQUENCE_STORAGE_H
#define SEQUENCE_STORAGE_H

#include "sequence_engine.h"
#include "supervisor_comms.h"   // for SequenceMetadata_t

void SequenceStorage_Save(SequenceStep_t *steps, uint8_t length, const SequenceMetadata_t *meta);
void SequenceStorage_Load(void);

#endif

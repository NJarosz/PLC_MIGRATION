#ifndef SUPERVISOR_COMMS_H
#define SUPERVISOR_COMMS_H

#include <stdint.h>
#include <stdbool.h>

// Metadata prepended to every sequence frame.
// Must match the struct packed by pi/compiler/compile_sequence.py.
#define META_SIZE 20

typedef struct __attribute__((packed)) {
    uint8_t  version;       // sequence version number
    uint8_t  step_count;    // number of steps — cross-checked against BLOB_LEN / STEP_SIZE
    uint8_t  seq_id[16];    // sequence ID string, null-padded ASCII (max 15 chars)
    uint8_t  reserved[2];   // reserved for future use
} SequenceMetadata_t;

void SupervisorComms_Init(void);
void SupervisorComms_Task(void);
bool SupervisorComms_IsConnected(void);

#endif

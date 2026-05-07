#ifndef SUPERVISOR_COMMS_H
#define SUPERVISOR_COMMS_H

#include <stdint.h>
#include <stdbool.h>

// Metadata prepended to every sequence frame.
// Must match the struct packed by pi/compiler/compile_sequence.py.
#define META_SIZE 40

typedef struct __attribute__((packed)) {
    uint8_t  version;        // sequence format version
    uint8_t  step_count;     // number of steps — cross-checked against BLOB_LEN / STEP_SIZE
    uint8_t  seq_name[16];   // sequence name, null-padded ASCII (max 15 chars)
    uint8_t  part_num[12];   // part number, null-padded ASCII (max 11 chars)
    uint8_t  machine_id[8];  // machine identifier, null-padded ASCII (max 7 chars)
    uint8_t  reserved[2];    // reserved for future use
} SequenceMetadata_t;

void SupervisorComms_Init(void);
void SupervisorComms_Task(void);
bool SupervisorComms_IsConnected(void);

// Request a log upload on the next SupervisorComms_Task tick.
// Safe to call from state_machine.c, sequence_engine.c, etc.
void SupervisorComms_RequestUpload(void);

// Override the active sequence name reported in heartbeats.
// Called by SequenceStorage_Load() to restore state after a reboot.
void SupervisorComms_SetActiveSeqName(const char *name);

// Ask the ESP32 to resolve an employee number to a display name.
// The response arrives asynchronously as an EMPLOYEE_NAME| line and is
// printed via printf (LCD stub) when received.
void SupervisorComms_LookupEmployee(uint32_t employee_id);

#endif

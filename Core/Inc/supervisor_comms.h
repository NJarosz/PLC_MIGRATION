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

// Returns the currently active sequence name (e.g. "seq_001").
const char* SupervisorComms_GetActiveSeqName(void);

// Active part number — set from sequence metadata on receive/load.
void        SupervisorComms_SetPartNum(const char *part_num);
const char* SupervisorComms_GetPartNum(void);

// Set when a new sequence frame is successfully received.
// Cleared when the operator leaves IDLE so the banner isn't shown again for the same sequence.
bool SupervisorComms_NewSequenceAvailable(void);
void SupervisorComms_ClearNewSequence(void);

// Ask the ESP32 to resolve an employee number to a display name.
// The response arrives asynchronously and is stored internally.
void SupervisorComms_LookupEmployee(uint32_t employee_id);

// Returns the operator name from the last successful EMPLOYEE_NAME response.
// Empty string if no lookup has completed yet.
const char* SupervisorComms_GetOperatorName(void);

// Clear the stored operator name (call on logoff).
void SupervisorComms_ClearOperatorName(void);

// Run count tracking — incremented by state machine on each sequence completion.
// goal = 0 means no goal is active.
void     SupervisorComms_IncrementCount(void);
void     SupervisorComms_ResetCount(void);
void     SupervisorComms_SetCount(uint16_t count);   // called on boot to restore Pi-persisted count
uint16_t SupervisorComms_GetCount(void);
uint16_t SupervisorComms_GetGoal(void);
bool     SupervisorComms_IsGoalReached(void);
void     SupervisorComms_SetGoal(uint16_t goal);

#endif

#include <stdbool.h>

#ifndef SUPERVISOR_COMMS_H
#define SUPERVISOR_COMMS_H

void SupervisorComms_Init(void);
void SupervisorComms_Task(void);       // Called every scan cycle

// Future: status / error reporting
bool SupervisorComms_IsConnected(void);

#endif

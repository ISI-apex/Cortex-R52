#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

// Command field length is limited to 4-bits right now
#define CMD_ECHO       0x1

void cmd_handle(void *cbarg, volatile uint32_t *mbox_base, uint32_t *msg);

#endif // COMMAND_H

#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include "mailbox.h"

#define MAX_CMD_ARG_LEN 16

#define CMD_ECHO       0x1
#define CMD_RESET_HPPS 0x3

struct cmd {
    uint32_t cmd;
    uint32_t arg[MAX_CMD_ARG_LEN];
    struct mbox *reply_mbox;
    volatile bool *reply_acked;
};

void cmd_handle(struct cmd *cmd);

int cmd_enqueue(struct cmd *cmd);
int cmd_dequeue(struct cmd *cmd);

// Implementation defined in a consumer's source
int server_process(struct cmd *cmd, uint32_t *reply, size_t reply_len);

#endif // COMMAND_H

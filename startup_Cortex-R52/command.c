#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

#include "command.h"

#define CMD_QUEUE_LEN 4
#define REPLY_SIZE MAX_CMD_ARG_LEN

static unsigned cmdq_head = 0;
static unsigned cmdq_tail = 0;
static struct cmd cmdq[CMD_QUEUE_LEN];

int cmd_enqueue(struct cmd *cmd)
{
    unsigned i;

    if (cmdq_head + 1 % CMD_QUEUE_LEN == cmdq_tail) {
        printf("cannot enqueue command: queue full\r\n");
        return 1;
    }
    cmdq_head = (cmdq_head + 1) % CMD_QUEUE_LEN;

    // cmdq[cmdq_head] = *cmd; // can't because GCC inserts a memcpy
    cmdq[cmdq_head].cmd = cmd->cmd;
    cmdq[cmdq_head].reply_mbox = cmd->reply_mbox;
    cmdq[cmdq_head].reply_acked = cmd->reply_acked;
    for (i = 0; i < MAX_CMD_ARG_LEN; ++i)
        cmdq[cmdq_head].arg[i] = cmd->arg[i];

    printf("enqueue command (tail %u head %u): cmd %u arg %u...\r\n",
           cmdq_tail, cmdq_head, cmdq[cmdq_head].cmd, cmdq[cmdq_head].arg[0]);
    return 0;
}

int cmd_dequeue(struct cmd *cmd)
{
    unsigned i;

    if (cmdq_head == cmdq_tail)
        return 1;

    cmdq_tail = (cmdq_tail + 1) % CMD_QUEUE_LEN;

    // *cmd = cmdq[cmdq_tail].cmd; // can't because GCC inserts a memcpy
    cmd->cmd = cmdq[cmdq_tail].cmd;
    cmd->reply_mbox = cmdq[cmdq_tail].reply_mbox;
    cmd->reply_acked = cmdq[cmdq_tail].reply_acked;
    for (i = 0; i < MAX_CMD_ARG_LEN; ++i)
        cmd->arg[i] = cmdq[cmdq_tail].arg[i];
    printf("dequeue command (tail %u head %u): cmd %u arg %u...\r\n",
           cmdq_tail, cmdq_head, cmdq[cmdq_tail].cmd, cmdq[cmdq_tail].arg[0]);
    return 0;
}

void cmd_handle(struct cmd *cmd)
{
    uint32_t reply[REPLY_SIZE];
    int reply_len;

    printf("CMD handle cmd %x arg %x...\r\n", cmd->cmd, cmd->arg[0]);

    reply_len = server_process(cmd, &reply[0], REPLY_SIZE - 1); // 1 word for header

    if (reply_len < 0) {
        printf("ERROR: failed to process request: server error\r\n");
        return;
    }

    if (!reply_len) {
        printf("server did not produce a reply for the request\r\n");
        return;
    }

    *cmd->reply_acked = false;
    if (mbox_send(cmd->reply_mbox, &reply[0], reply_len)) {
        printf("failed to send reply\r\n");
    } else {
        printf("waiting for ACK for our reply\r\n");
        while(!*cmd->reply_acked); // block
        printf("ACK for our reply received\r\n");
    }
}

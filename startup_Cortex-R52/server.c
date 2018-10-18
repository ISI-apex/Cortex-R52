#include <stdint.h>
#include <unistd.h>

#include "printf.h"
#include "command.h"

#define CMD_ECHO       0x1
#define CMD_NOP        0x2
#define CMD_RESET_HPPS 0x3

int server_process(struct cmd *cmd, uint32_t *reply, size_t reply_size)
{
    unsigned i;
    switch (cmd->cmd) {
        case CMD_ECHO:
            printf("ECHO %x...\r\n", cmd->arg[0]);
            for (i = 0; i < MAX_CMD_ARG_LEN && i < reply_size; ++i)
                reply[i] = cmd->arg[i];
            return i;
        case CMD_NOP:
            // do nothing command
            return 0;
        default:
            printf("ERROR: unknown cmd: %x\r\n", cmd->cmd);
            return -1;
    }
}

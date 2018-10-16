#include <stdint.h>
#include <unistd.h>

#include "printf.h"
#include "command.h"

#define CMD_ECHO       0x1
#define CMD_RESET_HPPS 0x3

int server_process(struct cmd *cmd, uint32_t *reply, size_t reply_len)
{
    unsigned i;
    switch (cmd->cmd) {
        case CMD_ECHO:
            printf("ECHO %x...\r\n", cmd->arg[0]);
            for (i = 0; i < MAX_CMD_ARG_LEN; ++i)
                reply[i] = cmd->arg[i];
            break;
        default:
            printf("ERROR: unknown cmd: %x\r\n", cmd->cmd);
            return 1;
    }

   return 0;
}

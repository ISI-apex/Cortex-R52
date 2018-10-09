#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

#include "command.h"

void cmd_handle(void *cbarg, volatile uint32_t *mbox_base, unsigned msg)
{
    unsigned cmd = (msg & 0xc) >> 2;
    unsigned arg = (msg & 0x3);

    printf("CMD handle cmd %x arg %x\r\n", cmd, arg);

    switch (cmd) {
        case CMD_ECHO:
            printf("ECHO %x\r\n", arg);
            mbox_reply(mbox_base, arg);
            break;
        default:
            printf("ERROR: unknown cmd: %x\r\n", cmd);
    }
}

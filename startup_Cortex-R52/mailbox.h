#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define HPSC_MBOX_AS_SIZE 0x10000 // address allocation in mem map/Qemu DT/Qemu model

#define HPSC_MBOX_DATA_REGS 16

typedef void (*rcv_cb_t)(void *arg, uint32_t *msg, size_t size);
typedef void (*ack_cb_t)(void *arg);

enum mbox_dir {
        MBOX_OUTGOING = 0,
        MBOX_INCOMING = 1,
};

union mbox_cb {
    rcv_cb_t rcv_cb;
    ack_cb_t ack_cb;
};

struct mbox;

struct mbox *mbox_claim(volatile uint32_t * ip_base, unsigned irq_base,
                        unsigned instance, unsigned int_idx,
                        uint32_t owner, uint32_t src, uint32_t dest,
                        enum mbox_dir dir, union mbox_cb cb, void *cb_arg);
int mbox_release(struct mbox *m);
int mbox_send(struct mbox *m, uint32_t *msg, size_t len);

void mbox_rcv_isr(unsigned int_idx);
void mbox_ack_isr(unsigned int_idx);

#endif // MAILBOX_H

#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define HPSC_MBOX_NUM_BLOCKS 2

#define LSIO_MBOX_BASE ((volatile uint32_t *)0x3000a000)
#define HPPS_MBOX_BASE ((volatile uint32_t *)0xf9220000)

#define LSIO_MBOX_IRQ_START         72
#define HPPS_MBOX_IRQ_START         136

#define REG_CONFIG              0x00
#define REG_EVENT_CAUSE         0x04
#define REG_EVENT_CLEAR         0x04
#define REG_EVENT_STATUS        0x08
#define REG_EVENT_SET           0x08
#define REG_INT_ENABLE          0x0C
#define REG_DATA                0x10

#define REG_CONFIG__UNSECURE      0x1
#define REG_CONFIG__OWNER__SHIFT  8
#define REG_CONFIG__OWNER__MASK   0x0000ff00
#define REG_CONFIG__SRC__SHIFT    16
#define REG_CONFIG__SRC__MASK     0x00ff0000
#define REG_CONFIG__DEST__SHIFT   24
#define REG_CONFIG__DEST__MASK    0xff000000

#define HPSC_MBOX_EVENT_A 0x1
#define HPSC_MBOX_EVENT_B 0x2

#define HPSC_MBOX_INT_A(idx) (1 << (2 * (idx)))      // rcv (map event A to int 'idx')
#define HPSC_MBOX_INT_B(idx) (1 << (2 * (idx) + 1))  // ack (map event B to int 'idx')

#define HPSC_MBOX_DATA_REGS 16
#define HPSC_MBOX_EVENTS 2
#define HPSC_MBOX_INTS   16
#define HPSC_MBOX_INSTANCES 32
#define HPSC_MBOX_INSTANCE_REGION (REG_DATA + HPSC_MBOX_DATA_REGS * 4)

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

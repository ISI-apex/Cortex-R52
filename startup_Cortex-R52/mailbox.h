#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define HPSC_MBOX_NUM_BLOCKS 2
#define MBOX_BLOCK_LSIO 0
#define MBOX_BLOCK_HPPS 1

#define LSIO_MBOX_BASE ((volatile uint32_t *)0x3000a000)
#define HPPS_MBOX_BASE ((volatile uint32_t *)0xf9220000)

#define LSIO_MAILBOX_IRQ_START         72
#define LSIO_MAILBOX_IRQ_A(instance)   (LSIO_MAILBOX_IRQ_START + (instance * 2))
#define LSIO_MAILBOX_IRQ_B(instance)   (LSIO_MAILBOX_IRQ_START + (instance * 2) + 1)

#define HPPS_MAILBOX_IRQ_START         136
#define HPPS_MAILBOX_IRQ_A(instance)   (HPPS_MAILBOX_IRQ_START + (instance * 2))
#define HPPS_MAILBOX_IRQ_B(instance)   (HPPS_MAILBOX_IRQ_START + (instance * 2) + 1)

#define REG_OWNER             0x00
#define REG_INT_ENABLE        0x04
#define REG_INT_CAUSE         0x08
#define REG_INT_STATUS        0x0C
#define REG_INT_CLEAR         0x08 /* TODO: is this overlap by design */
#define REG_INT_SET           0x0C
#define REG_DESTINATION       0x1C
#define REG_DATA              0x20

#define HPSC_MBOX_INT_A 0x1 // in our req-reply usage model, signifies request
#define HPSC_MBOX_INT_B 0x2 // in our req-reply usage model, signifies reply

#define HPSC_MBOX_DATA_REGS 16
#define HPSC_MBOX_INTS 2
#define HPSC_MBOX_INSTANCES 32
#define HPSC_MBOX_INSTANCE_REGION (REG_DATA + HPSC_MBOX_DATA_REGS * 4)

typedef void (*rcv_cb_t)(void *arg, uint32_t *msg, size_t size);
typedef void (*ack_cb_t)(void *arg);

struct mbox;

struct mbox *mbox_claim(volatile uint32_t * ip_base, unsigned instance, uint32_t owner, uint32_t dest);
int mbox_release(struct mbox *m);
int mbox_init_in(struct mbox *m, rcv_cb_t cb, void *cb_arg);
int mbox_init_out(struct mbox *m, ack_cb_t cb, void *cb_arg);
int mbox_send(struct mbox *m, uint32_t *msg, size_t len);

void mbox_rcv_isr(struct mbox *m);
void mbox_ack_isr(struct mbox *m);

int block_index_from_base(volatile uint32_t *base);
int irq_from_base(volatile uint32_t *base, unsigned index, unsigned interrupt);

#endif // MAILBOX_H

#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>

#define RTPS_TRCH_MBOX_BASE	((volatile uint32_t *)0x3000a000)
#define HPPS_RTPS_MBOX_BASE 	((volatile uint32_t *)0xf9230000)
#define HPPS_TRCH_MBOX_BASE 	((volatile uint32_t *)0xf9220000)


#define RTPS_TRCH_MBOX0_BASE	((volatile uint32_t *)((uint8_t *)RTPS_TRCH_MBOX_BASE + MBOX_MAIL0))
#define RTPS_TRCH_MBOX1_BASE	((volatile uint32_t *)((uint8_t *)RTPS_TRCH_MBOX_BASE + MBOX_MAIL1))

#define HPPS_TRCH_MBOX0_BASE 	((volatile uint32_t *)((uint8_t *)HPPS_TRCH_MBOX_BASE + MBOX_MAIL0))
#define HPPS_TRCH_MBOX1_BASE 	((volatile uint32_t *)((uint8_t *)HPPS_TRCH_MBOX_BASE + MBOX_MAIL1))

#define HPPS_RTPS_MBOX0_BASE 	((volatile uint32_t *)((uint8_t *)HPPS_RTPS_MBOX_BASE + MBOX_MAIL0))
#define HPPS_RTPS_MBOX1_BASE 	((volatile uint32_t *)((uint8_t *)HPPS_RTPS_MBOX_BASE + MBOX_MAIL1))

#define RTPS_TRCH_MAILBOX_IRQ_A  		161
#define RTPS_TRCH_MAILBOX_IRQ_B  		162
#define HPPS_TRCH_MAILBOX_IRQ_A  		163
#define HPPS_TRCH_MAILBOX_IRQ_B  		164
#define HPPS_RTPS_MAILBOX_IRQ_A  		165
#define HPPS_RTPS_MAILBOX_IRQ_B  		166

#define MBOX_MAIL0 0x80
#define MBOX_MAIL1 0xA0

#define MBOX_REG_MAIL     0x00
#define MBOX_REG_CNF      0x1C

#define MBOX_BIT_IHAVEDATAIRQEN 0x1

#define REG_OWNER             0x00
#define REG_INT_ENABLE        0x04
#define REG_INT_CAUSE         0x08
#define REG_INT_STATUS        0x0C
#define REG_INT_CLEAR         0x08 /* TODO: is this overlap by design */
#define REG_INT_SET           0x0C
#define REG_INT_A_INSTANCES   0x10
#define REG_INT_B_INSTANCES   0x14
#define REG_DESTINATION       0x1C
#define REG_DATA              0x20

#define HPSC_MBOX_INT_A 0x1 // in our req-reply usage model, signifies request
#define HPSC_MBOX_INT_B 0x2 // in our req-reply usage model, signifies reply

#define HPSC_MBOX_DATA_REGS 16
#define HPSC_MBOX_INTS 2
#define HPSC_MBOX_INSTANCES 32
#define HPSC_MBOX_INSTANCE_REGION (REG_DATA + HPSC_MBOX_DATA_REGS * 4)

typedef void (*cb_t)(void *arg, volatile uint32_t *base, uint32_t *msg);

int mbox_init_server(volatile uint32_t *ip_base, unsigned instance, uint32_t owner, uint32_t dest, cb_t req_cb, void *cb_arg);
int mbox_init_client(volatile uint32_t *ip_base, unsigned instance, uint32_t dest, cb_t reply_cb, void *cb_arg);
void mbox_request(volatile uint32_t *ip_base, uint32_t *msg, size_t len);
void mbox_reply(volatile uint32_t *ip_base, uint32_t *msg, size_t len);
void mbox_request_isr(volatile uint32_t *ip_base);
void mbox_reply_isr(volatile uint32_t *ip_base);

#endif // MAILBOX_H

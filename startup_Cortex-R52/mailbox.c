#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

#define OFFSET_PAYLOAD 4

#define MAX_HW_INSTANCES 128

struct mbox {
        volatile uint32_t *ip_base;
        volatile uint32_t *base;
        unsigned instance;
        bool owner; // whether this mailbox was claimed as owner
        union {
            rcv_cb_t rcv_cb;
            ack_cb_t ack_cb;
        } cb;
        void *cb_arg;
};

static struct mbox mboxes[MAX_HW_INSTANCES];
static volatile unsigned num_mboxes = 0; // number of registered HW mailbox instancees

static struct mbox *alloc_mbox(volatile uint32_t *ip_base, unsigned instance)
{
    if (num_mboxes == MAX_HW_INSTANCES)
        return NULL;
    mboxes[num_mboxes].ip_base = ip_base;
    mboxes[num_mboxes].instance = instance;
    mboxes[num_mboxes].base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);
    num_mboxes++;
    return &mboxes[num_mboxes - 1];
}

struct mbox *mbox_claim_owner(volatile uint32_t * ip_base, unsigned instance, uint32_t owner, uint32_t dest)
{
    // Assert: owner == self

    struct mbox *m = alloc_mbox(ip_base, instance);
    if (!m)
        return NULL;

    m->owner = true;

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_OWNER);
    uint32_t val = owner;
    printf("mbox_init: owner: %p <|- %08lx\r\n", addr, val);
    *addr = val;
    val = *addr;
    if (val != owner) {
        printf("mbox_init: failed to claim mailbox %u for %lx: already owned by %lx\r\n",
               instance, owner, val);
        return NULL;
    }

    addr = (volatile uint32_t *)((uint8_t *)m->base + REG_DESTINATION);
    val = dest;
    printf("mbox_init: dest: %p <|- %08lx\r\n", addr, val);
    *addr = val;

    return m;
}

struct mbox *mbox_claim_dest(volatile uint32_t * ip_base, unsigned instance, uint32_t dest)
{
    // Assert: self == DESTINATION

    struct mbox *m= alloc_mbox(ip_base, instance);
    if (!m)
        return NULL;

    m->owner = false;

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_DESTINATION);
    uint32_t val = *addr;
    printf("mbox_init: dest: %p -> %08lx\r\n", addr, val);
    if (val != dest) { // also enforced in HW
        printf("mbox_claim_dest: failed to claim mailbox as dest for %lx: reserved for %lx\r\n",
               instance, dest, val);
        return NULL;
    }

    return m;
}

int mbox_release(struct mbox *m)
{
    // We are the OWNER, so we can release

    if (m->owner) {
        volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_OWNER);
        uint32_t val = 0;
        printf("mbox_init: owner: %p <|- %08lx\r\n", addr, val);
        *addr = val;
    }

    return 0;
}

int mbox_init_in(struct mbox *m, rcv_cb_t cb, void *cb_arg)
{
    // Assert: self == OWNER || self == DESTINATION

    m->cb.rcv_cb = cb;
    m->cb_arg = cb_arg;

    uint32_t val = HPSC_MBOX_INT_A;

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_INT_ENABLE);
    printf("mbox_init: int A en: %p <|- %08lx\r\n", addr, val);
    *addr |= val;


    return 0;
}

int mbox_init_out(struct mbox *m, ack_cb_t cb, void *cb_arg)
{
    // Assert: self == OWNER || self == DESTINATION

    m->cb.ack_cb = cb;
    m->cb_arg = cb_arg;

    uint32_t val = HPSC_MBOX_INT_B;

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_INT_ENABLE);
    printf("mbox_init: int B en: %p <|- %08lx\r\n", addr, val);
    *addr |= val;
    return 0;
}

int mbox_send(struct mbox *m, uint32_t *msg, size_t len)
{
    unsigned i;

    if (len > HPSC_MBOX_DATA_REGS) {
        printf("ERROR: message too long: %u > %u\r\n", len, HPSC_MBOX_DATA_REGS);
        return 1;
    }

    printf("mbox_request: writing msg: ");
    volatile uint32_t *slot = (volatile uint32_t *)((uint8_t *)m->base + REG_DATA);
    for (i = 0; i < len; ++i) {
        slot[i] = msg[i];
        printf("%x ", msg[i]);
    }
    printf("\r\n");

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_INT_SET);
    uint32_t val = HPSC_MBOX_INT_A;
    printf("mbox_request: raise int A: %p <- %08lx\r\n", addr, val);
    *addr = val;

    return 0;
}

void mbox_rcv_isr(struct mbox *mbox)
{
    volatile uint32_t *addr;
    uint32_t val;
    uint32_t msg[HPSC_MBOX_DATA_REGS];

    printf("mbox_rcv_isr: base %p instance %u\r\n", mbox->base, mbox->instance);

    // We don't have to copy, but let's copy for simplicity and to go over the
    // bus to the IP block only once
    printf("mbox_receive: rcved: ", msg);
    volatile uint32_t *data = (volatile uint32_t *)((uint8_t *)mbox->base + REG_DATA);
    int len;
    for (len = 0; len < HPSC_MBOX_DATA_REGS; len++) {
        msg[len] = *data++;
        printf("%x ", msg[len]);
    }
    printf("\r\n");

    // ACK before the callback, because if callback wants to block,
    // we might have a deadlock.
    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_INT_SET);
    val = HPSC_MBOX_INT_B;
    printf("mbox_receive: set int B: %p <- %08lx\r\n", addr, val);
    *addr = val;

    if (mbox->cb.rcv_cb)
        mbox->cb.rcv_cb(mbox->cb_arg, &msg[0], HPSC_MBOX_DATA_REGS);

    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_INT_CLEAR);
    val = HPSC_MBOX_INT_A;
    printf("mbox_receive: clear int A: %p <- %08lx\r\n", addr, val);
    *addr = val;
}

void mbox_ack_isr(struct mbox *mbox)
{
    volatile uint32_t *addr;
    uint32_t val;

    printf("mbox_ack_isr: base %p instance %u\r\n", mbox->base, mbox->instance);

    if (mbox->cb.ack_cb)
        mbox->cb.ack_cb(mbox->cb_arg);

    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_INT_CLEAR);
    val = HPSC_MBOX_INT_B;
    printf("mbox_receive: clear int B: %p <- %08lx\r\n", addr, val);
    *addr = val;
}

#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

#define OFFSET_PAYLOAD 4

#define MAX_HW_INSTANCES 64

struct mbox {
        bool valid; // for storing in an array
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

int block_index_from_base(volatile uint32_t *base) {
    switch ((unsigned)base) {
        case (unsigned)LSIO_MBOX_BASE: return MBOX_BLOCK_LSIO;
        case (unsigned)HPPS_MBOX_BASE: return MBOX_BLOCK_HPPS;
                             /* assert: return value < NUM_BLOCKS */
        default: printf("ERROR: unrecognized mbox base addr: %p", base);
    }
    return -1;
}
int irq_from_base(volatile uint32_t *base, unsigned index, unsigned interrupt) {
    unsigned offset = index * 2 + (interrupt == HPSC_MBOX_INT_A ? 0 : 1);
    switch ((unsigned)base) {
        case (unsigned)HPPS_MBOX_BASE: return HPPS_MAILBOX_IRQ_START + offset;
        case (unsigned)LSIO_MBOX_BASE: return LSIO_MAILBOX_IRQ_START + offset;
        default: printf("ERROR: unrecognized mbox base addr: %p", base);
    }
    return -1;
}

static void mbox_clear(struct mbox *mbox)
{
    // Not strictly necessary, but to easy debugging
    // We don't have a memset
    mbox->ip_base = 0;
    mbox->base = 0;
    mbox->instance = 0;
    mbox->owner = false;
    mbox->cb.rcv_cb = NULL;
    mbox->cb_arg = NULL;
}

static struct mbox *mbox_alloc()
{
    struct mbox *mbox;
    unsigned i = 0;
    while (mboxes[i].valid && i < MAX_HW_INSTANCES)
        ++i;
    if (i == MAX_HW_INSTANCES)
        return NULL;
    mbox = &mboxes[i];
    mbox_clear(mbox);
    mbox->valid = true;
    return mbox;
}

static void mbox_free(struct mbox *mbox)
{
    mbox->valid = false;
    mbox_clear(mbox);
}

struct mbox *mbox_claim(volatile uint32_t * ip_base, unsigned instance, uint32_t owner, uint32_t dest)
{
    struct mbox *m = mbox_alloc();
    if (!m)
        return NULL;

    m->ip_base = ip_base;
    m->instance = instance;
    m->base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);
    m->owner = (owner != 0);

    if (m->owner) {
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
    } else { // not owner, just check the value in registers against the requested value
        volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_DESTINATION);
        uint32_t val = *addr;
        printf("mbox_init: dest: %p -> %08lx\r\n", addr, val);
        if (val != dest) { // also enforced in HW
            printf("mbox_claim_dest: failed to claim mailbox %u as dest for %lx: reserved for %lx\r\n",
                   instance, dest, val);
            return NULL;
        }
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

        // clearing owner also clears destination (resets the instance)
    }

    mbox_free(m);
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

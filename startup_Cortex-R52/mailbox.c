#include <stdint.h>

#include "printf.h"
#include "object.h"
#include "panic.h"
#include "intc.h"
#include "mailbox.h"

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

#define HPSC_MBOX_EVENTS 2
#define HPSC_MBOX_INTS   16
#define HPSC_MBOX_INSTANCES 32
#define HPSC_MBOX_INSTANCE_REGION (REG_DATA + HPSC_MBOX_DATA_REGS * 4)

#define MAX_BLOCKS 2
#define MAX_MBOXES (MAX_BLOCKS * HPSC_MBOX_INSTANCES)

struct irq {
        unsigned irq;
        unsigned refcount;
};

struct mbox_ip_block {
        struct object obj;
        volatile uint32_t *base;
        unsigned refcnt;
        unsigned irq_refcnt[HPSC_MBOX_EVENTS];
};

struct mbox {
        struct object obj;
        struct mbox_ip_block *block;
        volatile uint32_t *base;
        unsigned instance;
        int int_idx;
        int irq;
        bool owner; // whether this mailbox was claimed as owner
        union mbox_cb cb;
        void *cb_arg;
};

// The mboxes array is common across all mbox_ip_block's. We could let each
// block own its own mboxes array, and iterate over blocks in the ISR. Meh.
static struct mbox mboxes[MAX_MBOXES] = {0};
static struct mbox_ip_block blocks[MAX_BLOCKS] = {0};

static void mbox_irq_subscribe(struct mbox *mbox)
{
    if (mbox->block->irq_refcnt[mbox->int_idx]++ == 0)
        intc_int_enable(mbox->irq, IRQ_TYPE_EDGE);
}
static void mbox_irq_unsubscribe(struct mbox *mbox)
{
    if (--mbox->block->irq_refcnt[mbox->int_idx] == 0)
        intc_int_disable(mbox->irq);
}

static struct mbox_ip_block *block_get(volatile uint32_t *ip_base)
{
    struct mbox_ip_block *b;
    unsigned block = 0;
    while (block < MAX_BLOCKS &&
           (!blocks[block].obj.valid || blocks[block].base != ip_base))
        ++block;
    if (block == MAX_BLOCKS) { // no match
        b = OBJECT_ALLOC(blocks);
        if (!b)
            return NULL;
        b->base = ip_base;
    } else {
        b = &blocks[block];
    }
    ++b->refcnt;
    return b;
}
static void block_put(struct mbox_ip_block *b)
{
    ASSERT(b);
    ASSERT(b->refcnt);
    for (unsigned e = 0; e < HPSC_MBOX_EVENTS; ++e)
        ASSERT(!b->irq_refcnt[e]);
    if (!--b->refcnt)
        OBJECT_FREE(b);
}

struct mbox *mbox_claim(volatile uint32_t * ip_base, unsigned irq_base,
                        unsigned instance, unsigned int_idx,
                        uint32_t owner, uint32_t src, uint32_t dest,
                        enum mbox_dir dir, union mbox_cb cb, void *cb_arg)
{
    printf("mbox claim: ip %x irq base %u instance %u int %u owner %x src %x dest %x dir %u\r\n",
           ip_base, irq_base, instance, int_idx, owner, src, dest, dir);

    struct mbox *m = OBJECT_ALLOC(mboxes);
    if (!m)
        return NULL;

    m->block = block_get(ip_base);
    if (!m->block)
        goto cleanup;

    m->instance = instance;
    m->base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);
    m->int_idx = int_idx;
    m->irq = irq_base + int_idx;
    m->owner = (owner != 0);

    if (m->owner) {
        volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_CONFIG);
        uint32_t config = REG_CONFIG__UNSECURE |
                       ((owner << REG_CONFIG__OWNER__SHIFT) & REG_CONFIG__OWNER__MASK) |
                       ((src << REG_CONFIG__SRC__SHIFT)     & REG_CONFIG__SRC__MASK) |
                       ((dest  << REG_CONFIG__DEST__SHIFT)  & REG_CONFIG__DEST__MASK);
        uint32_t val = config;
        printf("mbox_init: config: %p <|- %08lx\r\n", addr, val);
        *addr = val;
        val = *addr;
        printf("mbox_init: config: %p -> %08lx\r\n", addr, val);
        if (val != config) {
            printf("mbox_init: failed to claim mailbox %u for %lx: already owned by %lx\r\n",
                   instance, owner, (val & REG_CONFIG__OWNER__MASK) >> REG_CONFIG__OWNER__SHIFT);
            goto cleanup;
        }
    } else { // not owner, just check the value in registers against the requested value
        volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_CONFIG);
        uint32_t val = *addr;
        printf("mbox_init: config: %p -> %08lx\r\n", addr, val);
        uint32_t src_hw =  (val & REG_CONFIG__SRC__MASK) >> REG_CONFIG__SRC__SHIFT;
        uint32_t dest_hw = (val & REG_CONFIG__DEST__MASK) >> REG_CONFIG__DEST__SHIFT;
        if ((dir == MBOX_OUTGOING && src  && src_hw != src) ||
            (dir == MBOX_INCOMING && dest && dest_hw != src)) {
            printf("mbox: failed to claim (instance %u dir %u): "
                   "src/dest mismatch: %lx/%lx (expected %lx/%lx)\r\n",
                   instance, dir, src, dest, src_hw, dest_hw);
            goto cleanup;
        }
    }

    m->cb = cb;
    m->cb_arg = cb_arg;

    uint32_t ie;
    switch (dir) {
        case MBOX_INCOMING:
            ie = HPSC_MBOX_INT_A(m->int_idx);
            break;
        case MBOX_OUTGOING:
            ie = HPSC_MBOX_INT_B(m->int_idx);
            break;
        default:
            printf("mbox: invalid direction: %u\r\n", dir);
            goto cleanup;
    }

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_INT_ENABLE);
    printf("mbox_init: int en: %p <|- %08lx\r\n", addr, ie);
    *addr |= ie;
    mbox_irq_subscribe(m);

    return m;
cleanup:
    OBJECT_FREE(m);
    return NULL;
}

int mbox_release(struct mbox *m)
{
    // We are the OWNER, so we can release

    if (m->owner) {
        volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_CONFIG);
        uint32_t val = 0;
        printf("mbox_init: owner: %p <|- %08lx\r\n", addr, val);
        *addr = val;

        // clearing owner also clears destination (resets the instance)
    }

    mbox_irq_unsubscribe(m);
    block_put(m->block);
    OBJECT_FREE(m);
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

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)m->base + REG_EVENT_SET);
    uint32_t val = HPSC_MBOX_EVENT_A;
    printf("mbox_request: raise int A: %p <- %08lx\r\n", addr, val);
    *addr = val;

    return 0;
}

static void mbox_instance_rcv_isr(struct mbox *mbox)
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
    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_EVENT_SET);
    val = HPSC_MBOX_EVENT_B;
    printf("mbox_receive: set int B: %p <- %08lx\r\n", addr, val);
    *addr = val;

    if (mbox->cb.rcv_cb)
        mbox->cb.rcv_cb(mbox->cb_arg, &msg[0], HPSC_MBOX_DATA_REGS);

    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_EVENT_CLEAR);
    val = HPSC_MBOX_EVENT_A;
    printf("mbox_receive: clear int A: %p <- %08lx\r\n", addr, val);
    *addr = val;
}

static void mbox_instance_ack_isr(struct mbox *mbox)
{
    volatile uint32_t *addr;
    uint32_t val;

    printf("mbox_ack_isr: base %p instance %u\r\n", mbox->base, mbox->instance);

    if (mbox->cb.ack_cb)
        mbox->cb.ack_cb(mbox->cb_arg);

    addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_EVENT_CLEAR);
    val = HPSC_MBOX_EVENT_B;
    printf("mbox_receive: clear int B: %p <- %08lx\r\n", addr, val);
    *addr = val;
}

static void mbox_isr(unsigned event, unsigned interrupt)
{
    volatile uint32_t *addr;
    uint32_t val;
    struct mbox *mbox;
    unsigned i;

    // Technically, could iterate only over one IP block if we care to split
    // the main mailbox array into multiple arrays, one per block.
    for (i = 0; i < MAX_MBOXES; ++i) {
        mbox = &mboxes[i];
        if (!mbox->obj.valid)
            continue;

        // Are we 'signed up' for this event (A) from this mailbox (i)?
        // Two criteria: (1) Cause is set, and (2) Mapped to our IRQ
        addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_EVENT_CAUSE);
        val = *addr;
        printf("mbox_receive: cause: %p -> %08lx\r\n", addr, val);
        if (!(val & event))
            continue; // this mailbox didn't raise the interrupt
        addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_INT_ENABLE);
        val = *addr;
        printf("mbox_receive: int enable: %p -> %08lx\r\n", addr, val);
        if (!(val & interrupt))
            continue; // this mailbox has an event but it's not ours

        switch (event) {
            case HPSC_MBOX_EVENT_A:
                mbox_instance_rcv_isr(mbox);
                break;
            case HPSC_MBOX_EVENT_B:
                mbox_instance_ack_isr(mbox);
                break;
            default:
                printf("ERROR: mbox_isr: invalid event %u\r\n", event);
        }
   }
}

void mbox_rcv_isr(unsigned int_idx)
{
    mbox_isr(HPSC_MBOX_EVENT_A, HPSC_MBOX_INT_A(int_idx));
}
void mbox_ack_isr(unsigned int_idx)
{
    mbox_isr(HPSC_MBOX_EVENT_B, HPSC_MBOX_INT_B(int_idx));
}

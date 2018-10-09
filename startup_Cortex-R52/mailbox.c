#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

// Mailbox has two slots:
//   Slot 0: outgoing direction (send writes to #0)
//   Slot 1: incoming direction (receive reads from #1)
// So neither endpoint ever reads and writes to the same slot.

// Layout of message inside a slot:
//   bits 4-8 bits are the payload
//   Bits 0-4 designate channel index (leftover from BCM2835 mbox)

#define OFFSET_PAYLOAD 4

#define MAX_HW_INSTANCES 128

typedef struct mbox_state {
        volatile uint32_t *ip_base;
        unsigned instance;
        cb_t cb;
        void *cb_arg;
} mbox_state_t;

static mbox_state_t mboxes[MAX_HW_INSTANCES];
static unsigned num_mboxes = 0; // number of registered HW mailbox instancees

static mbox_state_t *alloc_mbox(volatile uint32_t *ip_base, unsigned instance, cb_t cb, void *cb_arg)
{
    if (num_mboxes == MAX_HW_INSTANCES)
        return NULL;
    mboxes[num_mboxes].ip_base = ip_base;
    mboxes[num_mboxes].instance = instance;
    mboxes[num_mboxes].cb = cb;
    mboxes[num_mboxes].cb_arg = cb_arg;
    num_mboxes++;
    return &mboxes[num_mboxes - 1];
}

static mbox_state_t *find_mbox(volatile uint32_t *ip_base, unsigned instance)
{
   unsigned i;
   for (i = 0; i < num_mboxes; ++i)
       if (mboxes[i].ip_base == ip_base && mboxes[i].instance == instance)
           return &mboxes[i];
   return NULL;
}

int mbox_init_server(volatile uint32_t * ip_base, unsigned instance, uint32_t owner, uint32_t dest, cb_t req_cb, void *cb_arg)
{
    if (!alloc_mbox(ip_base, instance, req_cb, cb_arg)) {
        printf("failed to alloc mbox\r\n");
        return 1;
    }

    volatile uint32_t *base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_OWNER);
    uint32_t val = owner;
    printf("mbox_init: owner: %p <|- %08lx\r\n", addr, val);
    *addr = val;

    addr = (volatile uint32_t *)((uint8_t *)base + REG_DESTINATION);
    val = dest;
    printf("mbox_init: dest: %p <|- %08lx\r\n", addr, val);
    *addr = val;

    addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_ENABLE);
    val = MBOX_INT_A;
    printf("mbox_init: int A en: %p <|- %08lx\r\n", addr, val);
    *addr |= val;
    return 0;
}

int mbox_init_client(volatile uint32_t * ip_base, unsigned instance, uint32_t dest, cb_t reply_cb, void *cb_arg)
{
    if (!alloc_mbox(ip_base, instance, reply_cb, cb_arg)) {
        printf("failed to alloc mbox\r\n");
        return 1;
    }
    volatile uint32_t *base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_DESTINATION);
    if (*addr != dest) {
        printf("mbox_init_dest: we are not the destination\r\n");
        return 1;
    }

    addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_ENABLE);
    uint32_t val = MBOX_INT_B;
    printf("mbox_init: int B en: %p <|- %08lx\r\n", addr, val);
    *addr |= val;
    return 0;
}

void mbox_request(volatile uint32_t *base, uint8_t msg)
{
    // Prevent another request before the reply (receiver holds A high while replying)
    printf("mbox_request: waiting for INT A to fall...\r\n");
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_STATUS);
    while (*addr & MBOX_INT_A);
    printf("mbox_request: INT A low\r\n");

    volatile uint32_t *slot = (volatile uint32_t *)((uint8_t *)base + REG_DATA);
    uint32_t val = msg << OFFSET_PAYLOAD; // see layout above
    printf("mbox_request: %p <- %08lx\r\n", slot, val);
    *slot = val;
    addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_SET);
    val = MBOX_INT_A;
    printf("mbox_request: raise int A: %p <- %08lx\r\n", addr, val);
    *addr = val;

    // async wait for INT B (or wait for clearing of INT_A)?
    // TODO: timeout, in order to clear A (since receiver failed to clear it)
}

void mbox_reply(volatile uint32_t *base, uint8_t msg)
{
    volatile uint32_t *slot = (volatile uint32_t *)((uint8_t *)base + REG_DATA);
    uint32_t val = msg << OFFSET_PAYLOAD; // see layout above
    printf("mbox_reply: %p <- %08lx\r\n", slot, val);
    *slot = val;
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_SET);
    val = MBOX_INT_B;
    printf("mbox_reply: raise int B: %p <- %08lx\r\n", addr, val);
    *addr = val;
}

uint8_t mbox_receive(volatile uint32_t *base)
{
    volatile uint32_t *slot = (volatile uint32_t *)((uint8_t *)base + REG_DATA);
    uint32_t val = *slot;
    printf("mbox_receive: %p -> %08lx\r\n", slot, val);
    uint8_t msg = val >> OFFSET_PAYLOAD; // see layout above
    return msg;
}

uint8_t mbox_handle_request(volatile uint32_t *ip_base, volatile uint32_t *base, unsigned instance)
{
    uint8_t msg = mbox_receive(base);
    printf("MBOX ISR A: rcved req %x\r\n", msg);

    mbox_state_t *mbox = find_mbox(ip_base, instance);
    mbox->cb(mbox->cb_arg, base, msg);

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_CLEAR);
    uint32_t val = MBOX_INT_A;
    printf("MBOX ISR A: clear int A: %p <- %08lx\r\n", addr, val);
    *addr = val;
    return msg;
}
uint8_t mbox_handle_reply(volatile uint32_t *ip_base, volatile uint32_t *base,  unsigned instance)
{
    uint8_t msg = mbox_receive(base);
    printf("MBOX ISR B: rcved reply %x\r\n", msg);

    mbox_state_t *mbox = find_mbox(ip_base, instance);
    mbox->cb(mbox->cb_arg, base, msg);

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_CLEAR);
    uint32_t val = MBOX_INT_B;
    printf("MBOX ISR B: clear int B: %p <- %08lx\r\n", addr, val);
    *addr = val;
    return msg;
}

void mbox_isr(volatile uint32_t *ip_base, unsigned mbox_int_num)
{
    unsigned reg_instances = mbox_int_num ? REG_INT_B_INSTANCES : REG_INT_A_INSTANCES;
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)ip_base + reg_instances);
    uint32_t val = *addr;
    int i;

    printf("MBOX ISR (%u): int instances: %p -> %08lx\r\n", mbox_int_num, addr, val);

    for (i = 0; i < HPSC_MBOX_INSTANCES; ++i) { // could be replaced with find-first-set-bit instruction
        if (val & (1 << i)) {
            volatile uint32_t *base = (volatile uint32_t *)((uint8_t *)ip_base + i * HPSC_MBOX_INSTANCE_REGION);
            printf("MBOX ISR (%u): int instance %u: %p\r\n", mbox_int_num, i, base);
            if (mbox_int_num == 0)
                mbox_handle_request(ip_base, base, i);
            else
                mbox_handle_reply(ip_base, base, i);
        }
    }
}

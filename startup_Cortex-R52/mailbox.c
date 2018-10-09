#include <stdint.h>

#include "printf.h"
#include "mailbox.h"

#define OFFSET_PAYLOAD 4

#define MAX_HW_INSTANCES 128

typedef struct mbox_state {
        volatile uint32_t *ip_base;
        volatile uint32_t *base;
        unsigned instance;
        cb_t cb;
        void *cb_arg;
} mbox_t;

static mbox_t mboxes[MAX_HW_INSTANCES];
static unsigned num_mboxes = 0; // number of registered HW mailbox instancees

static mbox_t *alloc_mbox(volatile uint32_t *ip_base, unsigned instance, cb_t cb, void *cb_arg)
{
    if (num_mboxes == MAX_HW_INSTANCES)
        return NULL;
    mboxes[num_mboxes].ip_base = ip_base;
    mboxes[num_mboxes].instance = instance;
    mboxes[num_mboxes].base = (volatile uint32_t *)((uint8_t *)ip_base + instance * HPSC_MBOX_INSTANCE_REGION);
    mboxes[num_mboxes].cb = cb;
    mboxes[num_mboxes].cb_arg = cb_arg;
    num_mboxes++;
    return &mboxes[num_mboxes - 1];
}

static mbox_t *find_mbox(volatile uint32_t *ip_base, unsigned instance)
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
    val = HPSC_MBOX_INT_A;
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
    uint32_t val = HPSC_MBOX_INT_B;
    printf("mbox_init: int B en: %p <|- %08lx\r\n", addr, val);
    *addr |= val;
    return 0;
}

static void mbox_send(volatile uint32_t *base, uint32_t *msg, size_t len, uint32_t mbox_int)
{
    unsigned i;

#if 0 // blocking logic
    // Prevent another request before the reply (receiver holds A high while replying)
    printf("mbox_request: waiting for INT A to fall...\r\n");
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_STATUS);
    while (*addr & HPSC_MBOX_INT_A);
    printf("mbox_request: INT A low\r\n");
#endif

    if (len > HPSC_MBOX_DATA_REGS) {
        printf("ERROR: message too long: %u > %u\r\n", len, HPSC_MBOX_DATA_REGS);
        return;
    }

    printf("mbox_request: writing msg: ");
    volatile uint32_t *slot = (volatile uint32_t *)((uint8_t *)base + REG_DATA);
    for (i = 0; i < len; ++i) {
        slot[i] = msg[i];
        printf("%x ", msg[i]);
    }
    printf("\r\n");

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)base + REG_INT_SET);
    uint32_t val = mbox_int;
    printf("mbox_request: raise int %u: %p <- %08lx\r\n", mbox_int, addr, val);
    *addr = val;

    // for blocking on send: async wait for INT B (or wait for clearing of INT_A)?
    // TODO: timeout, in order to clear A (since receiver failed to clear it)
}

static void mbox_receive(mbox_t *mbox, unsigned mbox_int)
{
    uint32_t msg[HPSC_MBOX_DATA_REGS];    

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

    mbox->cb(mbox->cb_arg, mbox->base, &msg[0]);

    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)mbox->base + REG_INT_CLEAR);
    uint32_t val = mbox_int;
    printf("mbox_receive: clear int %u: %p <- %08lx\r\n", mbox_int, addr, val);
    *addr = val;
}
static void mbox_isr(volatile uint32_t *ip_base, unsigned mbox_int)
{
    unsigned reg_instances = mbox_int == HPSC_MBOX_INT_B ? REG_INT_B_INSTANCES : REG_INT_A_INSTANCES;
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)ip_base + reg_instances);
    uint32_t val = *addr;
    int i;

    printf("MBOX ISR (%u): int instances: %p -> %08lx\r\n", mbox_int, addr, val);

    for (i = 0; i < HPSC_MBOX_INSTANCES; ++i) { // could be replaced with find-first-set-bit instruction
        if (val & (1 << i)) {
            volatile uint32_t *base = (volatile uint32_t *)((uint8_t *)ip_base + i * HPSC_MBOX_INSTANCE_REGION);
            printf("MBOX ISR (%u): int instance %u: %p\r\n", mbox_int, i, base);

            mbox_t *mbox = find_mbox(ip_base, i);
            if (!mbox) {
                printf("ERROR: cannot find mailbox by base addr: %p\r\n", ip_base);
                return;
            }
            mbox_receive(mbox, mbox_int);
        }
    }
}

void mbox_request(volatile uint32_t *base, uint32_t *msg, size_t len)
{
    mbox_send(base, msg, len, HPSC_MBOX_INT_A);
}
void mbox_reply(volatile uint32_t *base, uint32_t *msg, size_t len)
{
    mbox_send(base, msg, len, HPSC_MBOX_INT_B);
}

void mbox_request_isr(volatile uint32_t *ip_base)
{
    mbox_isr(ip_base, HPSC_MBOX_INT_A);
}
void mbox_reply_isr(volatile uint32_t *ip_base)
{
    mbox_isr(ip_base, HPSC_MBOX_INT_B);
}

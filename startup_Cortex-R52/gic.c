#include <stdint.h>

#include "printf.h"
#include "intc.h"

#define GIC_BASE ((volatile uint32_t *)0xf9a00000)

#define GICD_ISENABLER 0x0100
#define GICD_ICFGR     0x0C00

static const char *irq_type_name(irq_type_t t)
{
    switch (t) {
        case IRQ_TYPE_LEVEL: return "LEVEL";
        case IRQ_TYPE_EDGE:  return "EDGE";
        default: return "?";
   }
}

void intc_int_enable(unsigned irq, irq_type_t type) {
    volatile uint32_t *reg_addr;
    uint32_t val;

    unsigned intid = irq + 32; /* TODO: does this offset have a name? */

    reg_addr = (volatile uint32_t *)((volatile uint8_t *)GIC_BASE + GICD_ISENABLER + (intid / 32) * 4);
    val = 1 << (intid % 32);

    printf("GIC: enable IRQ #%u (INTID %u) type %s: %p |= %08x\r\n",
           irq, intid, irq_type_name(type), reg_addr, val);
    *reg_addr |= val;

    reg_addr = (volatile uint32_t *)((volatile uint8_t *)GIC_BASE + GICD_ICFGR + (intid / 16) * 4);
    val = type << (intid % 16 + 1);

    printf("GIC: configure IRQ #%u (INTID %u): %p |= %08x\r\n",
           irq, intid, reg_addr, val);
    *reg_addr |= val;
}

void intc_int_disable(unsigned irq) {
    volatile uint32_t *reg_addr;
    uint32_t val;

    unsigned intid = irq + 32; /* TODO: does this offset have a name? */

    reg_addr = (volatile uint32_t *)((volatile uint8_t *)GIC_BASE + GICD_ISENABLER + (intid / 32) * 4);
    val = 1 << (intid % 32);

    printf("GIC: disableIRQ #%u (INTID %u): %p &= ~%08x\r\n",
           irq, intid, reg_addr, val);
    *reg_addr &= ~val;
}

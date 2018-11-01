#ifndef HWINFO_H
#define HWINFO_H

// This file fulfills the role of a device tree

#define HPSC_MBOX_NUM_BLOCKS 2

#define LSIO_MBOX_BASE ((volatile uint32_t *)0x3000a000)
#define HPPS_MBOX_BASE ((volatile uint32_t *)0xf9220000)

#define LSIO_MBOX_IRQ_START         72
#define HPPS_MBOX_IRQ_START         136

// From QEMU device tree / HW spec
#define MASTER_ID_TRCH_CPU  0x2d

#define MASTER_ID_RTPS_CPU0 0x2e
#define MASTER_ID_RTPS_CPU1 0x2f

#define MASTER_ID_HPPS_CPU0 0x80
#define MASTER_ID_HPPS_CPU1 0x8d
#define MASTER_ID_HPPS_CPU2 0x8e
#define MASTER_ID_HPPS_CPU3 0x8f
#define MASTER_ID_HPPS_CPU4 0x90
#define MASTER_ID_HPPS_CPU5 0x9d
#define MASTER_ID_HPPS_CPU6 0x9e
#define MASTER_ID_HPPS_CPU7 0x9f

#endif // HWINFO_H

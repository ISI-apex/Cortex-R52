/*
** Copyright (c) 2016-2017 Arm Limited (or its affiliates). All rights reserved.
** Use, modification and redistribution of this file is subject to your possession of a
** valid End User License Agreement for the Arm Product of which these examples are part of 
** and your compliance with all applicable terms and conditions of such licence agreement.
*/

/* This file contains the main() program that displays a welcome message, enables the caches,
   performs a float calculation to demonstrate floating point, then runs the main application (sorts) */

#include "printf.h"
#include "uart.h"
#include "float.h"
#include "mailbox.h"
#include "command.h"
#include "busid.h"
#include "panic.h"
#include "test-mailbox.h"

// #define TEST_FLOAT
// #define TEST_SORT
#define TEST_RTPS_TRCH_MAILBOX
#define TEST_HPPS_RTPS_MAILBOX
// #define TEST_SOFT_RESET
// #define TEST_RTPS_HPPS_MMU

extern unsigned char _text_start;
extern unsigned char _text_end;
extern unsigned char _data_start;
extern unsigned char _data_end;
extern unsigned char _bss_start;
extern unsigned char _bss_end;

extern void enable_caches(void);
extern void compare_sorts(void);

void enable_interrupts (void)
{
	unsigned long temp;
	__asm__ __volatile__("mrs %0, cpsr\n"
			     "bic %0, %0, #0x80\n"
			     "msr cpsr_c, %0"
			     : "=r" (temp)
			     :
			     : "memory");
}

void soft_reset (void) 
{
	unsigned long temp;
	__asm__ __volatile__("mov r1, #2\n"
			     "mcr p15, 4, r1, c12, c0, 2\n"); 
}

int main(void)
{
//    asm(".global __use_hlt_semihosting");
    cdns_uart_startup(); 	// init UART
    printf("R52 is alive\r\n");


    /* Display a welcome message via semihosting */
    printf("Cortex-R52 bare-metal startup example\r\n");

    /* Enable the caches */
    enable_caches();

    /* Enable GIC */
/*    printf("start of arm_gic_setup()\n");
    arm_gic_setup();
    printf("end of arm_gic_setup()\n");
*/
    enable_interrupts();


#ifdef TEST_FLOAT
    float_test();
#endif // TEST_FLOAT

#ifdef TEST_SORT
    /* Run the main application (sorts) */
    compare_sorts();
#endif // TEST_SORT

#ifdef TEST_RTPS_TRCH_MAILBOX
    test_rtps_trch_mailbox();
#endif // TEST_RTPS_TRCH_MAILBOX

#ifdef TEST_HPPS_RTPS_MAILBOX
    setup_hpps_rtps_mailbox();
#endif // TEST_HPPS_RTPS_MAILBOX

    printf("Done.\r\n");

#ifdef TEST_SOFT_RESET
    printf("Resetting...\r\n");
    /* this will generate "Undefined Instruction exception because HRMR is accessible only at EL2 */
    soft_reset();
    printf("ERROR: reached unrechable code: soft reset failed\r\n");
#endif // TEST_SOFT_RESET

#ifdef TEST_RTPS_HPPS_MMU
    // Translated by MMU via identity map (in HPPS LOW DRAM)
    volatile uint32_t *addr = (volatile uint32_t *)0x8e001000;
    uint32_t val = 0xf00dcafe;
    printf("%p <- %08x\r\n", addr, val);
    *addr = val;
    val = *addr;
    printf("%p -> %08x\r\n", addr, val);

    // Translated by MMU (test configured to HPPS HIGH DRAM, 0x100000000)
    addr = (volatile uint32_t *)0xc0000000;
    val = 0xdeadbeef;
    printf("%p <- %08x\r\n", addr, val);
    *addr = val;
    val = *addr;
    printf("%p -> %08x\r\n", addr, val);
#endif // TEST_RTPS_HPPS_MMU

    while (1) {

#ifdef TEST_HPPS_RTPS_MAILBOX
        struct cmd cmd;
        if (!cmd_dequeue(&cmd))
            cmd_handle(&cmd);
#endif // TEST_HPPS_RTPS_MAILBOX

        printf("Waiting for interrupt...\r\n");
        asm("wfi");
    }
    
    return 0;
}

void irq_handler(unsigned irq) {
    printf("IRQ #%u\r\n", irq);
    switch (irq) {
#ifdef TEST_RTPS_TRCH_MAILBOX
        case LSIO_MAILBOX_IRQ_B(MBOX_TO_TRCH_INSTANCE):
            mbox_ack_isr(mbox_to_trch);
            break;
        case LSIO_MAILBOX_IRQ_A(MBOX_FROM_TRCH_INSTANCE):
            mbox_rcv_isr(mbox_from_trch);
            break;
#endif // TEST_RTPS_TRCH_MAILBOX
#ifdef TEST_HPPS_RTPS_MAILBOX
        case HPPS_MAILBOX_IRQ_B(MBOX_TO_HPPS_INSTANCE):
            mbox_ack_isr(mbox_to_hpps);
            break;
        case HPPS_MAILBOX_IRQ_A(MBOX_FROM_HPPS_INSTANCE):
            mbox_rcv_isr(mbox_from_hpps);
            break;
#endif // TEST_HPPS_RTPS_MAILBOX
        default:
            printf("No ISR registered for IRQ #%u\r\n", irq);
            break;
    }
}

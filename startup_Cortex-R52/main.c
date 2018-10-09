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
#include "gic.h"

// #define TEST_FLOAT
// #define TEST_SORT
#define TEST_RTPS_TRCH_MAILBOX
// #define TEST_HPPS_RTPS_MAILBOX
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

static void send_trch_request(unsigned cmd, unsigned arg)
{
    uint32_t msg = ((cmd & 0x3) << 2) | (arg & 0x3); // this protocol, must match the server-side on TRCH
    printf("sending request to TRCH: cmd %x arg %x (msg %x)\r\n", cmd, arg, msg);
    mbox_request(RTPS_TRCH_MBOX_BASE, msg);
}

static void handle_trch_reply(void *arg, volatile uint32_t *mbox_base, unsigned msg)
{
    printf("recved reply from TRCH: 0x%x\r\n", msg);
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

#ifdef TEST_RTPS_TRCH_MAILBOX /* Message flow: RTPS -> TRCH -> RTPS */
    gic_enable_irq(RTPS_TRCH_MAILBOX_IRQ_B, IRQ_TYPE_EDGE);
    mbox_init_client(RTPS_TRCH_MBOX_BASE, /* instance */ 0, MASTER_ID_RTPS_CPU0, handle_trch_reply, NULL);
    send_trch_request(CMD_ECHO, 3);
#endif // TEST_RTPS_TRCH_MAILBOX

#ifdef TEST_HPPS_RTPS_MAILBOX /* Message flow: HPPS -> RTPS -> HPPS */
    gic_enable_irq(HPPS_RTPS_MAILBOX_IRQ_A, IRQ_TYPE_EDGE);
    mbox_init_server(HPPS_RTPS_MBOX_BASE, /* instance */ 0, MASTER_ID_RTPS_CPU0, MASTER_ID_HPPS_CPU0, cmd_handle, NULL);
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

    printf("Waiting for interrupt...\r\n");
    while (1) {
        asm("wfi");
    }
    
    return 0;
}

// These are in main, not in mailbox.c, because different users of mailbox.c
// (sender vs. receiver) receive from different indexes. This way mailbox.c
// can be shared between sender and receiver.
#if 0
void mbox_trch_reply_isr()
{
     uint8_t msg = mbox_reply_isr(RTPS_TRCH_MBOX_BASE);
     printf("reply from TRCH: 0x%x\r\n", msg);
}
void mbox_hpps_request_isr()
{
     uint8_t msg = mbox_request_isr(HPPS_RTPS_MBOX_BASE);
     printf("request from HPPS: 0x%x\r\n", msg);
     cmd_handle(HPPS_RTPS_MBOX_BASE, msg);
}
#endif

void irq_handler(unsigned irq) {
    printf("IRQ #%u\r\n", irq);
    switch (irq) {
        case RTPS_TRCH_MAILBOX_IRQ_B:
            mbox_isr(RTPS_TRCH_MBOX_BASE, 1);
            break;
        case HPPS_RTPS_MAILBOX_IRQ_A:
            mbox_isr(HPPS_RTPS_MBOX_BASE, 0);
            break;
        default:
            printf("No ISR registered for IRQ #%u\r\n", irq);
            break;
    }
}

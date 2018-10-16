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
#define TEST_HPPS_RTPS_MAILBOX
// #define TEST_SOFT_RESET
// #define TEST_RTPS_HPPS_MMU

extern unsigned char _text_start;
extern unsigned char _text_end;
extern unsigned char _data_start;
extern unsigned char _data_end;
extern unsigned char _bss_start;
extern unsigned char _bss_end;


#if defined(TEST_RTPS_TRCH_MAILBOX) || defined(TEST_HPPS_RTPS_MAILBOX)
struct cmd_ctx {
    struct mbox *reply_mbox;
    volatile bool reply_acked;
    const char *origin; // for pretty-print
};
#endif

#ifdef TEST_RTPS_TRCH_MAILBOX

#define MBOX_TO_TRCH_INSTANCE   0
#define MBOX_FROM_TRCH_INSTANCE 1

static struct mbox *mbox_to_trch;
static struct mbox *mbox_from_trch;
#endif // TEST_RTPS_TRCH_MAILBOX

#ifdef TEST_HPPS_RTPS_MAILBOX

#define MBOX_FROM_HPPS_INSTANCE 2
#define MBOX_TO_HPPS_INSTANCE   3

static struct mbox *mbox_to_hpps;
static struct mbox *mbox_from_hpps;
#endif // TEST_HPPS_RTPS_MAILBOX

extern void enable_caches(void);
extern void compare_sorts(void);

static void panic(const char *msg)
{
    printf("PANIC HALT: %s\r\n", msg);
    while (1); // halt
}

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

#ifdef TEST_RTPS_TRCH_MAILBOX
static void handle_ack_from_trch(void *arg)
{
    volatile bool *trch_acked = arg;
    *trch_acked = true;
}
static void handle_reply_from_trch(void *arg, uint32_t *msg, size_t size)
{
    volatile bool *trch_replied = arg;
    size_t i;

    printf("received message from TRCH: ");
    for (i = 0; i < size; ++i)
        printf("%x ", msg[i]);
    printf("\r\n");

    *trch_replied = true; // unblock waiter
}
#endif // TEST_RTPS_TRCH_MAILBOX

#ifdef TEST_HPPS_RTPS_MAILBOX
static void handle_ack(void *arg)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)arg;
    printf("ACK from %s\r\n", ctx->origin);
    ctx->reply_acked = true;
}

void handle_cmd(void *cbarg, uint32_t *msg, size_t size)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)cbarg;

    struct cmd cmd = { .cmd = msg[0], .arg = msg[1],
                       .reply_mbox = ctx->reply_mbox,
                       .reply_acked = &ctx->reply_acked };
    printf("CMD (%u, %u) from %s\r\n",
           cmd.cmd, cmd.arg, ctx->origin);

    if (cmd_enqueue(&cmd))
        panic("failed to enqueue command");
}
#endif // TEST_HPPS_RTPS_MAILBOX

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

#ifdef TEST_RTPS_TRCH_MAILBOX // Message flow: RTPS (request) -> TRCH (reply) -> RTPS


    gic_enable_irq(LSIO_MAILBOX_IRQ_B(MBOX_TO_TRCH_INSTANCE), IRQ_TYPE_EDGE);

    mbox_to_trch = mbox_claim_dest(LSIO_MBOX_BASE, MBOX_TO_TRCH_INSTANCE, MASTER_ID_RTPS_CPU0);
    if (!mbox_to_trch)
        panic("failed to claim TO TRCH mailbox");

    gic_enable_irq(LSIO_MAILBOX_IRQ_A(MBOX_FROM_TRCH_INSTANCE), IRQ_TYPE_EDGE);

    mbox_from_trch = mbox_claim_dest(LSIO_MBOX_BASE, MBOX_FROM_TRCH_INSTANCE, MASTER_ID_RTPS_CPU0);
    if (!mbox_from_trch)
        panic("failed to claim TRCH OUT mailbox");

    bool trch_acked = false;
    bool trch_replied = false;

    if (mbox_init_out(mbox_to_trch, handle_ack_from_trch, &trch_acked))
        panic("failed to init TO TRCH mailbox for outgoing");
    if (mbox_init_in(mbox_from_trch, handle_reply_from_trch, &trch_replied))
        panic("failed to init TRCH mailbox for incomming");

    uint32_t msg[] = { CMD_ECHO, 42 }; // the protocol, must match the server-side on TRCH
    printf("sending message to TRCH: cmd %x arg %x\r\n", msg[0], msg[1]);
    int rc = mbox_send(mbox_to_trch, &msg[0], 2);
    if (rc) {
        printf("message send to TRCH failed: rc %u\r\n", rc);
        panic("TRCH mailbox test failed");
    } else {
        printf("waiting for ack from TRCH...\r\n");
        while (!trch_acked); // set by handle_ack_from_trch callback
        printf("ack from TRCH received\r\n");
    }

    printf("waiting for reply from TRCH...\r\n");
    while (!trch_replied); // set by handle_reply_from_trch callback
    printf("TRCH replied\r\n");

    rc = mbox_release(mbox_to_trch);
    if (rc)
        panic("failed to release TO TRCH mailbox");

    rc = mbox_release(mbox_from_trch);
    if (rc)
        panic("failed to release TO TRCH mailbox");
#endif // TEST_RTPS_TRCH_MAILBOX

#ifdef TEST_HPPS_RTPS_MAILBOX // Message flow: HPPS (request) -> RTPS (reply) -> HPPS
    gic_enable_irq(HPPS_MAILBOX_IRQ_B(MBOX_TO_HPPS_INSTANCE), IRQ_TYPE_EDGE);

    mbox_to_hpps = mbox_claim_owner(HPPS_MBOX_BASE, MBOX_TO_HPPS_INSTANCE, MASTER_ID_RTPS_CPU0, MASTER_ID_HPPS_CPU0);
    if (!mbox_to_hpps)
        panic("failed to claim TO HPPS mailbox");

    gic_enable_irq(HPPS_MAILBOX_IRQ_A(MBOX_FROM_HPPS_INSTANCE), IRQ_TYPE_EDGE);

    mbox_from_hpps = mbox_claim_owner(HPPS_MBOX_BASE, MBOX_FROM_HPPS_INSTANCE, MASTER_ID_RTPS_CPU0, MASTER_ID_HPPS_CPU0);
    if (!mbox_from_hpps)
        panic("failed to claim HPPS OUT mailbox");

    struct cmd_ctx hpps_cmd_ctx = {
        .origin = "HPPS",
        .reply_mbox = mbox_to_hpps,
        .reply_acked = false
    };

    if (mbox_init_out(mbox_to_hpps, handle_ack, &hpps_cmd_ctx))
        panic("failed to init TO HPPS mailbox for outgoing");
    if (mbox_init_in(mbox_from_hpps, handle_cmd, &hpps_cmd_ctx))
        panic("failed to init HPPS mailbox for incomming");

    // Don't release the mailbox, always listen, while executing the main loop
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

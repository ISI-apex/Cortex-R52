#include <stdbool.h>

#include "mailbox.h"
#include "command.h"
#include "intc.h"
#include "printf.h"
#include "busid.h"
#include "panic.h"

#include "test-mailbox.h"

struct cmd_ctx {
    struct mbox *reply_mbox;
    volatile bool reply_acked;
    const char *origin; // for pretty-print
};

struct mbox *mbox_to_trch;
struct mbox *mbox_from_trch;

struct mbox *mbox_to_hpps;
struct mbox *mbox_from_hpps;

static struct cmd_ctx hpps_cmd_ctx;

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

static void handle_ack(void *arg)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)arg;
    printf("ACK from %s\r\n", ctx->origin);
    ctx->reply_acked = true;
}

static void handle_cmd(void *cbarg, uint32_t *msg, size_t size)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)cbarg;
    unsigned i;

    struct cmd cmd = { .cmd = msg[0],
                       .reply_mbox = ctx->reply_mbox,
                       .reply_acked = &ctx->reply_acked };
    for (i = 0; i < MAX_CMD_ARG_LEN && i < size - 1; ++i)
        cmd.arg[i] = msg[1 + i];
    printf("CMD (%u, %u...) from %s\r\n",
           cmd.cmd, cmd.arg[0], ctx->origin);

    if (cmd_enqueue(&cmd))
        panic("failed to enqueue command");
}

void test_rtps_trch_mailbox() {
    int rc;

    // Message flow: RTPS (request) -> TRCH (reply) -> RTPS

    intc_int_enable(LSIO_MAILBOX_IRQ_B(MBOX_TO_TRCH_INSTANCE), IRQ_TYPE_EDGE);

    mbox_to_trch = mbox_claim_dest(LSIO_MBOX_BASE, MBOX_TO_TRCH_INSTANCE, MASTER_ID_RTPS_CPU0);
    if (!mbox_to_trch)
        panic("failed to claim TO TRCH mailbox");

    intc_int_enable(LSIO_MAILBOX_IRQ_A(MBOX_FROM_TRCH_INSTANCE), IRQ_TYPE_EDGE);

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
    rc = mbox_send(mbox_to_trch, &msg[0], 2);
    if (rc) {
        printf("message send to TRCH failed: rc %u\r\n", rc);
        panic("TRCH mailbox test failed");
    } else {
        printf("waiting for ack from TRCH...\r\n");
        while (!trch_acked) { // set by handle_ack_from_trch callback
        }
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
}

void setup_hpps_rtps_mailbox() {
    // Message flow: HPPS (request) -> RTPS (reply) -> HPPS

    intc_int_enable(HPPS_MAILBOX_IRQ_B(MBOX_TO_HPPS_INSTANCE), IRQ_TYPE_EDGE);

    mbox_to_hpps = mbox_claim_owner(HPPS_MBOX_BASE, MBOX_TO_HPPS_INSTANCE, MASTER_ID_RTPS_CPU0, MASTER_ID_HPPS_CPU0);
    if (!mbox_to_hpps)
        panic("failed to claim TO HPPS mailbox");

    intc_int_enable(HPPS_MAILBOX_IRQ_A(MBOX_FROM_HPPS_INSTANCE), IRQ_TYPE_EDGE);

    mbox_from_hpps = mbox_claim_owner(HPPS_MBOX_BASE, MBOX_FROM_HPPS_INSTANCE, MASTER_ID_RTPS_CPU0, MASTER_ID_HPPS_CPU0);
    if (!mbox_from_hpps)
        panic("failed to claim HPPS OUT mailbox");

    hpps_cmd_ctx.origin = "HPPS";
    hpps_cmd_ctx.reply_mbox = mbox_to_hpps;
    hpps_cmd_ctx.reply_acked = false;

    if (mbox_init_out(mbox_to_hpps, handle_ack, &hpps_cmd_ctx))
        panic("failed to init TO HPPS mailbox for outgoing");
    if (mbox_init_in(mbox_from_hpps, handle_cmd, &hpps_cmd_ctx))
        panic("failed to init HPPS mailbox for incomming");

    // Don't release the mailbox, always listen, while executing the main loop
}

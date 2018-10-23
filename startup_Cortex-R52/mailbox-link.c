#include <stdbool.h>

#include "mailbox.h"
#include "command.h"
#include "printf.h"
#include "intc.h"
#include "busid.h"
#include "panic.h"

#include "mailbox-link.h"

#define MAX_LINKS 8

struct cmd_ctx {
    struct mbox *reply_mbox;
    volatile bool tx_acked;
    uint32_t *reply;
    size_t reply_sz;
    size_t reply_len;
    const char *endpoint; // for pretty-print
};

struct mbox_link {
    bool valid; // is entry in the array full
    unsigned block;
    unsigned idx_to;
    unsigned idx_from;
    unsigned rcv_irq;
    unsigned ack_irq;
    struct mbox *mbox_from;
    struct mbox *mbox_to;
    struct cmd_ctx cmd_ctx;
};

static struct mbox_link links[MAX_LINKS] = {0};
static uint32_t req_msg[HPSC_MBOX_DATA_REGS];

// For ISRs, we need mbox pointers in a table
struct mbox *mboxes[HPSC_MBOX_NUM_BLOCKS][HPSC_MBOX_INSTANCES];

static void handle_ack(void *arg)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)arg;
    printf("ACK from %s\r\n", ctx->endpoint);
    ctx->tx_acked = true;
}

static void handle_cmd(void *arg, uint32_t *msg, size_t size)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)arg;
    unsigned i;

    struct cmd cmd; // can't use initializer, because GCC inserts a memset for initing .arg
    cmd.reply_mbox = ctx->reply_mbox;
    cmd.reply_acked = &ctx->tx_acked;
    cmd.cmd = msg[0];
    for (i = 0; i < HPSC_MBOX_DATA_REGS - 1 && i < size - 1; ++i)
        cmd.arg[i] = msg[1 + i];

    printf("CMD (%u, %u ...) from %s\r\n", cmd.cmd, cmd.arg[0], ctx->endpoint);

    if (cmd_enqueue(&cmd))
        panic("failed to enqueue command");
}

static void handle_reply(void *arg, uint32_t *msg, size_t size)
{
    struct cmd_ctx *ctx = (struct cmd_ctx *)arg;
    size_t i;

    for (i = 0; i < size && i < ctx->reply_sz; ++i)
        ctx->reply[i] = msg[i];
    ctx->reply_len = i;
}

static void link_clear(struct mbox_link *link)
{
    // We don't have a libc, so no memset
    link->block = 0;
    link->idx_to = 0;
    link->idx_from = 0;
    link->rcv_irq = 0;
    link->ack_irq = 0;
    link->mbox_from = NULL;
    link->mbox_to = NULL;
}

static struct mbox_link *link_alloc()
{
    unsigned i = 0;
    struct mbox_link *link;
    while (links[i].valid && i < MAX_LINKS)
        ++i;
    if (i == MAX_LINKS)
        return NULL;
    link = &links[i];
    link_clear(link);
    link->valid = true;
    return link;
}

static void link_free(struct mbox_link *link)
{
    link->valid = false;
    link_clear(link);
}

struct mbox_link *mbox_link_connect(volatile uint32_t *base,
        unsigned idx_from, unsigned idx_to,
        unsigned owner, unsigned dest, const char *endpoint)
{
    struct mbox_link *link = link_alloc();
    if (!link) {
        printf("ERROR: failed to allocate link state\r\n");
        return NULL;
    }

    link->block = block_index_from_base(base);
    link->idx_from = idx_from;
    link->idx_to = idx_to;

    link->rcv_irq = irq_from_base(base, idx_from, HPSC_MBOX_INT_A);
    link->ack_irq = irq_from_base(base, idx_to, HPSC_MBOX_INT_B);

    if (link->rcv_irq < 0 || link->ack_irq < 0)
        return NULL;

    intc_int_enable(link->rcv_irq, IRQ_TYPE_EDGE);

    link->mbox_from = mbox_claim(base, idx_from, owner, dest);
    if (!link->mbox_from)
        return NULL;

    intc_int_enable(link->ack_irq, IRQ_TYPE_EDGE);

    link->mbox_to = mbox_claim(base, idx_to, owner, dest);
    if (!link->mbox_to) {
        mbox_release(link->mbox_from);
        return NULL;
    }

    // For ISR
    mboxes[link->block][link->idx_from] = link->mbox_from;
    mboxes[link->block][link->idx_to] = link->mbox_to;
    printf("b %u i %u mp %p m %p m %p\r\n", link->block, link->idx_from, &mboxes[link->block][link->idx_from], mboxes[link->block][link->idx_from], link->mbox_from);

    link->cmd_ctx.endpoint = endpoint;
    link->cmd_ctx.reply_mbox = link->mbox_to;
    link->cmd_ctx.tx_acked = false;
    link->cmd_ctx.reply = NULL;

    if (mbox_init_in(link->mbox_from, owner ? handle_cmd : handle_reply, &link->cmd_ctx))
        goto fail;
    if (mbox_init_out(link->mbox_to, handle_ack, &link->cmd_ctx))
        goto fail;

    printf("b %u i %u mp %p m %p m %p\r\n", link->block, link->idx_from, &mboxes[link->block][link->idx_from], mboxes[link->block][link->idx_from], link->mbox_from);
    return link;

fail:
    mbox_release(link->mbox_from);
    mbox_release(link->mbox_to);
    return NULL;
}

int mbox_link_disconnect(struct mbox_link *link) {
    int rc;

    intc_int_disable(link->rcv_irq);
    intc_int_disable(link->ack_irq);

    // in case of failure, keep going and fwd code
    rc = mbox_release(link->mbox_from);
    rc = mbox_release(link->mbox_to);

    mboxes[link->block][link->idx_from] = NULL;
    mboxes[link->block][link->idx_to] = NULL;

    link_free(link);
    return rc;
}

int mbox_link_request(struct mbox_link *link, unsigned cmd,
                      uint32_t *arg, size_t arg_len,
                      uint32_t *reply, size_t reply_sz)
{
    int rc;
    size_t i;

    link->cmd_ctx.tx_acked = false;
    link->cmd_ctx.reply_len = 0;
    link->cmd_ctx.reply = reply;
    link->cmd_ctx.reply_sz = reply_sz;

    // TODO: remove this copy by removing cmd/arg from the iface
    req_msg[0] = cmd;
    for (i = 0; i < arg_len; ++i)
        req_msg[i + 1] = arg[i];

    printf("sending message: cmd %x arg %x..\r\n", req_msg[0], req_msg[1]);
    rc = mbox_send(link->mbox_to, req_msg, 1 + arg_len);
    if (rc) {
        printf("message send to TRCH failed: rc %u\r\n", rc);
        return 1;
    }

    printf("req: waiting for ACK...\r\n");
    while (!link->cmd_ctx.tx_acked);
    printf("req: ACK received\r\n");

    printf("req: waiting for reply...\r\n");
    while (!link->cmd_ctx.reply_len);
    printf("req: reply received\r\n");

    printf("REPLY from %s: ", link->cmd_ctx.endpoint);
    for (i = 0; i < link->cmd_ctx.reply_len; ++i)
        printf("%u ", reply[i]);
    printf("\r\n");

    return 0;
}

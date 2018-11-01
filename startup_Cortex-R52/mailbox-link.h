#ifndef MAILBOX_LINK_H
#define MAILBOX_LINK_H

#include <stdint.h>

#include "mailbox.h"

struct mbox_link;

// We use 'owner' to indicate both the ID (arbitrary value) to which the
// mailbox should be claimed (i.e. OWNER HW register should be set) and whether
// the connection originator is a server or a client: owner!=0 ==> server;
// owner=0 ==> client. However, these concepts are orthogonal, so it would be
// easy to decouple them if desired by adding another arg to this function.
//
// To claim as server: set both server and client to non-zero ID
// To claim as client: set server to 0 and set client to non-zero ID
struct mbox_link *mbox_link_connect(
        volatile uint32_t *base, unsigned irq_base,
        unsigned idx_from, unsigned idx_to,
        unsigned rcv_int_idx, unsigned ack_int_idx, /* interrupt index within IP block */
        unsigned server, unsigned client);
int mbox_link_disconnect(struct mbox_link *link);
int mbox_link_request(struct mbox_link *link, unsigned cmd,
                      uint32_t *arg, size_t arg_len,
                      uint32_t *reply, size_t reply_sz);

#endif // MAILBOX_LINK_H

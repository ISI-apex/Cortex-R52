#ifndef MAILBOX_LINK_H
#define MAILBOX_LINK_H

#include <stdint.h>

#include "mailbox.h"

struct mbox_link;

// Table with references to all mbox objects by instance number For ISRs
extern struct mbox *mboxes[HPSC_MBOX_NUM_BLOCKS][HPSC_MBOX_INSTANCES];

// We use 'owner' to indicate both the ID (arbitrary value) to which the
// mailbox should be claimed (i.e. OWNER HW register should be set) and whether
// the connection originator is a server or a client: owner!=0 ==> server;
// owner=0 ==> client. However, these concepts are orthogonal, so it would be
// easy to decouple them if desired by adding another arg to this function.
struct mbox_link *mbox_link_connect(volatile uint32_t *base,
        unsigned idx_from, unsigned idx_to,
        unsigned owner, unsigned dest, const char *endpoint);
int mbox_link_disconnect(struct mbox_link *link);
int mbox_link_request(struct mbox_link *link, unsigned cmd,
                      uint32_t *arg, size_t arg_len,
                      uint32_t *reply, size_t reply_sz);

#endif // MAILBOX_LINK_H

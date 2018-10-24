#ifndef MAILBOX_MAP_H
#define MAILBOX_MAP_H

// These allocations are completely outside the scope of the mailbox driver and
// of the mailbox link.  They are referenced only from the top-level app code
// (both TRCH and RTPS) -- namely, main and sever.
//
// This file included from both RTPS and TRCH code, to minimize possibility
// of conflicting assignments.

// For interrupt assignments: each interrupt is dedicated to one subsystem.

// The interrupt index is the index within the IP block (not global IRQ#).
// There are HPSC_MBOX_INTS interrupts per IP block, either event from any
// mailbox instance can be mapped to any interrupt.

// HPPS Mailbox IP Block

#define MBOX_HPPS_HPPS_TRCH 0
#define MBOX_HPPS_TRCH_HPPS 1

#define MBOX_HPPS_HPPS_RTPS 2
#define MBOX_HPPS_RTPS_HPPS 3

// Mailboxes owned by Linux (just a test)
#define MBOX_HPPS_HPPS_OWN_RTPS 4
#define MBOX_HPPS_HPPS_OWN_HPPS 5

#define MBOX_HPPS_TRCH_RCV_INT 0
#define MBOX_HPPS_TRCH_ACK_INT 1

#define MBOX_HPPS_RTPS_RCV_INT 2
#define MBOX_HPPS_RTPS_ACK_INT 3

#define MBOX_HPPS_HPPS_RCV_INT 4
#define MBOX_HPPS_HPPS_ACK_INT 5

// LSIO Mailbox IP Block
#define MBOX_LSIO_RTPS_TRCH 0
#define MBOX_LSIO_TRCH_RTPS 1

#define MBOX_LSIO_TRCH_RCV_INT 0
#define MBOX_LSIO_TRCH_ACK_INT 1

#define MBOX_LSIO_RTPS_RCV_INT 2
#define MBOX_LSIO_RTPS_ACK_INT 3

#endif // MAILBOX_MAP_H

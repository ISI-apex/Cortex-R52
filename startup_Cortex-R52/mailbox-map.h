#ifndef MAILBOX_MAP_H
#define MAILBOX_MAP_H

// Chiplet-wide allocations of the mailbox resources. This file is exists in
// code bases for all subsystems: TRCH, RTPS, HPPS (included from Linux DT).
// For practical reasons, there are multiple copies of the file, but they
// should all be in sync (i.e. identical).

// In the context of TRCH, RTPS, these allocations are completely outside the
// scope of the mailbox driver and of the mailbox link.  They are referenced
// only from the top-level app code -- namely, main and sever.
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
#define MBOX_HPPS_HPPS_OWN_TRCH 4
#define MBOX_HPPS_TRCH_HPPS_OWN 5

#define MBOX_HPPS_HPPS_TRCH_SSW 30
#define MBOX_HPPS_TRCH_HPPS_SSW 31

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

#ifndef TEST_MAILBOX_H
#define TEST_MAILBOX_H

// Need to expose these IDs and the symbols, because referenced in the IRQ
// handler, which is global, so should be in the global scope, like main.c

#define MBOX_TO_TRCH_INSTANCE   0
#define MBOX_FROM_TRCH_INSTANCE 1

#define MBOX_FROM_HPPS_INSTANCE 2
#define MBOX_TO_HPPS_INSTANCE   3

extern struct mbox *mbox_to_trch;
extern struct mbox *mbox_from_trch;

extern struct mbox *mbox_to_hpps;
extern struct mbox *mbox_from_hpps;

void test_rtps_trch_mailbox();
void setup_hpps_rtps_mailbox();

#endif // TEST_MAILBOX_H

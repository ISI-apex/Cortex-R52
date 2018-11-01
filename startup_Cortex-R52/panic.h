#ifndef PANIC_H
#define PANIC_H

#include "printf.h"

#define ASSERT(cond) if (!(cond)) { panic("ASSERT: " #cond "\r\n");  }

void panic(const char *msg);

#endif // PANIC_H

#include "printf.h"

void panic(const char *msg)
{
    printf("PANIC HALT: %s\r\n", msg);
    while (1); // halt
}

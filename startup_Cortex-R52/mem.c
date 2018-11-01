#include <stdint.h>

#include "mem.h"

void bzero(void *p, int sz)
{
    // assume p is word-aligned
    uint32_t *wp = p;
    while (sz >= sizeof(uint32_t)) {
        *wp++ = 0;
        sz -= sizeof(uint32_t);
    }
    uint8_t *bp = (uint8_t *)wp;
    while (sz-- >= 0)
        *bp++ = 0;
}

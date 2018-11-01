
#include <stdint.h>

#include "printf.h"
#include "panic.h"
#include "mem.h"
#include "object.h"

void *object_alloc(const char *name, void *array, unsigned elems, unsigned sz)
{
    unsigned idx = 0;
    while (idx < elems &&
            ((struct object *)((uint8_t *)array + idx * sz))->valid)
        ++idx;
    if (idx == elems) {
        printf("ERROR: failed to alloc object %s: out of mem\r\n", name);
        return NULL;
    }
    printf("OBJECT: alloced obj %s of sz %u\r\n", name, sz);
    struct object *obj = (struct object *)((uint8_t *)array + idx * sz);
    bzero(obj, sz);
    obj->valid = 1;
    ASSERT(idx <= ~(typeof(obj->index))0);
    obj->index = idx;
    return obj;
}

void object_free(void *obj, unsigned sz)
{
    bzero(obj, sz);
}

#ifndef OBJECT_H
#define OBJECT_H

// Helper for maintaining dynamic lists of objects in static arrays

// As long as the type of derived object satisfies this template:
// we can cast a pointer to that object to 'struct object *':
//   struct derived_object {
//       struct object obj;
//       /* fields of derived types */
//   };
struct object { // keep the metadata to one word
    uint16_t valid;
    uint16_t index;
};

// NOTE: 'array' must be an array type, not a pointer
#define OBJECT_ALLOC(array) (typeof(array[0]) *)object_alloc(#array, array, \
                            (sizeof(array) / sizeof(array[0])), sizeof(array[0]))
#define OBJECT_FREE(obj) object_free(obj, sizeof(*obj))

// Not for cosumer use -- use the above macros
void *object_alloc(const char *name, void *array, unsigned elems, unsigned sz);
void object_free(void *obj, unsigned sz);

#endif // OBJECT_H

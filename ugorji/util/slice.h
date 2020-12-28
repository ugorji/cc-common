#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// slice_bytes_v encapsulates a length and pointer
typedef struct slice_bytes_t {
    char* v;
    size_t len;
    //size_t something_random;
} slice_bytes_t;

// slice_bytes encapsulates a slice_bytes_v with cap and alloc.
// cap is how far it can expand.
typedef struct slice_bytes {
    slice_bytes_t bytes;
    size_t cap;
} slice_bytes;

extern void slice_bytes_append(slice_bytes* v, const void* b, size_t len);
extern void slice_bytes_expand(slice_bytes* v, size_t len);
extern void slice_bytes_append_1(slice_bytes* v, uint8_t bd);
extern void slice_bytes_zero(slice_bytes* v);
extern void slice_bytes_free(slice_bytes v);

#ifdef __cplusplus
}  // end extern "C" 
#endif

// uncomment to disable assert()
// #define NDEBUG
#include <assert.h>

#include "slice.h"

void slice_bytes_zero(slice_bytes* v) {
    v->bytes.v = NULL;
    v->bytes.len = 0;
    v->cap = 0;
}

void slice_bytes_expand(slice_bytes* v, size_t len) {
    size_t newlen = v->bytes.len+len;
    if(newlen <= v->cap) return;

    // cap should always be a multiple of a typical cache line ie n*64.
    // expand cap by 25%, or requested length, whichever is more.

    size_t cap25 = v->cap + v->cap/4;
    if(cap25 > newlen) newlen = cap25;
    size_t rem64 = newlen%64;
    if(rem64 != 0) newlen += 64 - rem64;
        
    v->bytes.v = realloc(v->bytes.v, newlen);
    assert(v->bytes.v != NULL);
    v->cap = newlen;
}

void slice_bytes_append(slice_bytes* v, const void* b, size_t len) {
    // always append a \0 to end of it, so c string handling works.
    // expand by a multiple of a typical cache line size i.e. n*64
    slice_bytes_expand(v, len+1);
    // memcpy(&v->bytes.v[v->bytes.len], b, len);
    memmove(&v->bytes.v[v->bytes.len], b, len);
    v->bytes.len += len;
    v->bytes.v[v->bytes.len] = 0;
} 

void slice_bytes_append_1(slice_bytes* v, uint8_t bd) {
    slice_bytes_append(v, &bd, 1); 
}

void slice_bytes_free(slice_bytes v) {
    if(v.bytes.v != NULL) free(v.bytes.v);
}


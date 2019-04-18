#include "slice.h"

void slice_bytes_expand(slice_bytes* v, size_t len) {
    if(len < (v->cap - v->bytes.len)) return;
    size_t newlen = v->bytes.len * 3 / 2;
    if(newlen <= (v->bytes.len+len)) newlen = (v->bytes.len+len) * 3 / 2;
    if(newlen < 16) newlen = 16;
    // TODO: error checking (if realloc fails)
    // fprintf(stderr, "hello - Realloc - %ld\n", newlen);
    v->bytes.v = realloc(v->bytes.v, newlen);
    v->cap = newlen;
}

void slice_bytes_append(slice_bytes* v, void* b, size_t len) {
    slice_bytes_expand(v, len);
    memcpy(&v->bytes.v[v->bytes.len], b, len);
    v->bytes.len += len;
} 

void slice_bytes_append_1(slice_bytes* v, uint8_t bd) {
    slice_bytes_append(v, &bd, 1); 
}


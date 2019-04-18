#include "codec.h"
#include <stdio.h>

void codec_value_free_ptr(void** ptr) {
    if(*ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

void codec_value_free(codec_value v) {
    void* ptr = NULL;
    switch(v.type) {
    case CODEC_VALUE_STRING:
    case CODEC_VALUE_BYTES: 
        ptr = (void*)(v.v.vBytes.bytes.v);
        codec_value_free_ptr(&ptr);
        break;
    case CODEC_VALUE_MAP:
        for(int i = 0; i < v.v.vMap.len; i++) {
            codec_value_free(v.v.vMap.v[i].k);
            codec_value_free(v.v.vMap.v[i].v);
        }
        ptr = (void*)(v.v.vMap.v);
        codec_value_free_ptr(&ptr);
        break;
    case CODEC_VALUE_ARRAY:
        for(int i = 0; i < v.v.vArray.len; i++) {
            codec_value_free(v.v.vArray.v[i]);
        }
        ptr = (void*)(v.v.vArray.v);
        codec_value_free_ptr(&ptr);
        break;
	case CODEC_VALUE_CUSTOMEXT:
        ptr = (void*)(v.v.vExt.v.bytes.v);
        codec_value_free_ptr(&ptr);
        break;
    }
}

void codec_value_print_r(codec_value* v, slice_bytes* b) {
    slice_bytes_expand(b, 4);
    b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 4, "%d:", v->type);
    int pv = 0;
    // fprintf(stderr, "hello - vtype:%d - pv:%d - len:%d - %s\n", v->type, pv++, b->bytes.len, b->v);
    // char* d;
    int x;
    switch(v->type) {
    case CODEC_VALUE_NIL:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "%d", v->v.vNil);
        break;
	case CODEC_VALUE_BOOL:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "%d", v->v.vBool);
        break;
	case CODEC_VALUE_POS_INT:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "%ld", v->v.vUint64);
        break;
	case CODEC_VALUE_NEG_INT:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "-%lu", v->v.vUint64);
        break;
	case CODEC_VALUE_FLOAT32:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "%f", (float)v->v.vFloat64);
        break;
	case CODEC_VALUE_FLOAT64:
        slice_bytes_expand(b, 16);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 16, "%f", v->v.vFloat64);
        break;
    case CODEC_VALUE_STRING:
        slice_bytes_append(b, v->v.vString.bytes.v, v->v.vString.bytes.len);
        // slice_bytes_expand(b, v->v.vString.bytes.len + 10);
        // b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], v->v.vString.bytes.len+1, "%s", v->v.vString.bytes.v);
        break;
    case CODEC_VALUE_BYTES: 
        // fprintf(stderr, "bytes: %d\n", v->v.vString.bytes.len);
        slice_bytes_expand(b, (v->v.vString.bytes.len * 2) + 8);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "0x");
        for(x = 0; x < v->v.vString.bytes.len; x++) {
            // fprintf(stderr, "bytes: %d: 0x%hhx\n", x, v->v.vString.bytes.v[x]);
            b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "%hhx", v->v.vString.bytes.v[x]);
        }
        break;
    case CODEC_VALUE_MAP:
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "{ ");
        for(int i = 0; i < v->v.vMap.len; i++) {
            if(i != 0) {
                slice_bytes_expand(b, 3);
                b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, ", ");
            }
            codec_value_print_r(&v->v.vMap.v[i].k, b);
            slice_bytes_expand(b, 4);
            b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 4, " = ");
            codec_value_print_r(&v->v.vMap.v[i].v, b);
        }
        // fprintf(stderr, "endofmap\n");
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, " }");
        break;
    case CODEC_VALUE_ARRAY:
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "[ ");
        for(int i = 0; i < v->v.vArray.len; i++) {
            if(i != 0) {
                slice_bytes_expand(b, 3);
                b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, ", ");
            }
            codec_value_print_r(&v->v.vArray.v[i], b);
        }
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, " ]");
        break;
	case CODEC_VALUE_CUSTOMEXT:
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "( ");
        slice_bytes_expand(b, 12);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 12, "0x%x, 0x", v->v.vExt.tag);
        slice_bytes_expand(b, (v->v.vExt.v.bytes.len * 2) + 8);
        for(x = 0; x < v->v.vExt.v.bytes.len; x++) {
            b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, "%0x", v->v.vExt.v.bytes.v[x]);
        }
        slice_bytes_expand(b, 3);
        b->bytes.len += snprintf(&b->bytes.v[b->bytes.len], 3, " )");
        break;
    }
}

void codec_value_print(codec_value* v, slice_bytes* b) {
    codec_value_print_r(v, b);
    uint8_t d = '\0';
    slice_bytes_append(b, &d, 1);
    b->bytes.len--; // don't include that last '\0' in the length
}

bool codec_decode_state_chk(codec_decode_state* b, size_t len, char** err) {
    if((len + b->pos) > b->b.bytes.len) {
        char* errmsg = (char*)malloc(128);
        snprintf(errmsg, 128, "codec_decode_state: past bytes bound: pos: %ld >= len: %ld", 
                 b->pos, b->b.bytes.len);
        *err = errmsg;
        return true;
    }
    return false;
}

bool codec_decode_state_chk2(codec_decode_state* b, char** err) {
    if(b->pos >= b->b.bytes.len) {
        char* errmsg = (char*)malloc(128);
        snprintf(errmsg, 128, "codec_decode_state: pos: %ld >= len: %ld", b->pos, b->b.bytes.len);
        *err = errmsg;
        return true;
    }
    return false;
}

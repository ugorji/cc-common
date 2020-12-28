#include "simplecodec.h"
#include <stdio.h>

// Note:
// - C-casts may not convert from float to uint as expected, due to endian issues.
// - Always specifically use big-endian.
// - Work appropriately for 32-bit and 64-bit OS
//   Ensure sizes will fit within (size_t checks)

void codec_simple_append_len(slice_bytes* b, uint8_t bd, size_t length) {
    // fprintf(stderr, "codec_simple_append_len: len: %ld\n", length);
    if(length == 0) {
        slice_bytes_append_1(b, bd);
    } else if(length <= CODEC_MaxUint8) {
        slice_bytes_append_1(b, bd+1);
        slice_bytes_append_1(b, (uint8_t)length);        
    } else if(length <= CODEC_MaxUint16) {
        slice_bytes_append_1(b, bd+2);
        slice_bytes_expand(b, 2);
        util_big_endian_write_uint16(&b->bytes.v[b->bytes.len], (uint16_t)length);
        b->bytes.len += 2;
    } else if(length <= CODEC_MaxUint32) {
        slice_bytes_append_1(b, bd+3);
        slice_bytes_expand(b, 4);
        util_big_endian_write_uint32(&b->bytes.v[b->bytes.len], (uint32_t)length);
        b->bytes.len += 4;
    } else {
        slice_bytes_append_1(b, bd+4);
        slice_bytes_expand(b, 8);
        util_big_endian_write_uint64(&b->bytes.v[b->bytes.len], (uint64_t)length);
        b->bytes.len += 8;
    }
}

uint64_t codec_simple_readn(codec_decode_state* b, uint8_t n, char** err) {
    if(codec_decode_state_chk(b, n, err)) return -1;
    uint64_t v = -1;
    switch(n) {
    case 0: v = 0; break;
    case 1: v = (uint8_t)(b->b.bytes.v[b->pos]); break;
    case 2: v = (uint16_t)(b->b.bytes.v[b->pos]); break;
    case 4: v = (uint32_t)(b->b.bytes.v[b->pos]); break;
    case 8: v = (uint64_t)(b->b.bytes.v[b->pos]); break;
    default: abort(); // should never happen. This is internal code
    }
    b->pos += n;
    return v;
}

size_t codec_simple_decode_len(codec_decode_state* b, uint8_t bd, char** err) {
    //if val < size_t max, then
    size_t len = -1;
    switch(bd%8) {
    case 0: len = 0; break;
    case 1: len = (size_t)codec_simple_readn(b, 1, err); break;
    case 2: len = (size_t)codec_simple_readn(b, 2, err); break;
    case 3: len = (size_t)codec_simple_readn(b, 4, err); break;
    case 4: 
        if(sizeof(size_t) < 8) {
            *err = (char*)malloc(128);
            snprintf(*err, 128, "codec_simple_decode_len: length must fit within %lu bytes", sizeof(size_t));
            break;
        } 
        len = (size_t)codec_simple_readn(b, 8, err); break;
    default: 
        *err = (char*)malloc(128);
        snprintf(*err, 128, "codec_simple_decode_len: invalid descriptor: %d", bd);
    }
    // fprintf(stderr, "codec_simple_decode_len: len: %ld\n", len);
    return len;
}

void codec_simple_encode(codec_value* v, slice_bytes* b, char** err) {
    // walk the value and encode it.
    uint8_t bd = 0;
    // uint8_t arr[8];
    switch(v->type) {
    case CODEC_VALUE_NIL:
        slice_bytes_append_1(b, CODEC_SIMPLE_DESC_NIL);
        break;
	case CODEC_VALUE_BOOL:
        if(v->v.vBool) bd = CODEC_SIMPLE_DESC_TRUE;
        else bd = CODEC_SIMPLE_DESC_FALSE;
        slice_bytes_append_1(b, bd);
        break;
	case CODEC_VALUE_POS_INT: 
        if(v->v.vUint64 <= CODEC_MaxUint8) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_POS_INT);
            slice_bytes_append_1(b, (uint8_t)v->v.vUint64);
        } else if(v->v.vUint64 <= CODEC_MaxUint16) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_POS_INT+1);
            slice_bytes_expand(b, 2);
            util_big_endian_write_uint16(&b->bytes.v[b->bytes.len], (uint16_t)v->v.vUint64);
            b->bytes.len += 2;
        } else if(v->v.vUint64 <= CODEC_MaxUint32) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_POS_INT+2);
            slice_bytes_expand(b, 4);
            util_big_endian_write_uint32(&b->bytes.v[b->bytes.len], (uint32_t)v->v.vUint64);
            b->bytes.len += 4;
        } else {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_POS_INT+3);
            slice_bytes_expand(b, 8);
            util_big_endian_write_uint64(&b->bytes.v[b->bytes.len], v->v.vUint64);
            b->bytes.len += 8;
        }
        break;
	case CODEC_VALUE_NEG_INT:
        if(v->v.vUint64 <= CODEC_MaxUint8) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_NEG_INT);
            slice_bytes_append_1(b, (uint8_t)v->v.vUint64);
        } else if(v->v.vUint64 <= CODEC_MaxUint16) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_NEG_INT+1);
            slice_bytes_expand(b, 2);
            util_big_endian_write_uint16(&b->bytes.v[b->bytes.len], (uint16_t)v->v.vUint64);
            b->bytes.len += 2;
        } else if(v->v.vUint64 <= CODEC_MaxUint32) {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_NEG_INT+2);
            slice_bytes_expand(b, 4);
            util_big_endian_write_uint32(&b->bytes.v[b->bytes.len], (uint32_t)v->v.vUint64);
            b->bytes.len += 4;
        } else {
            slice_bytes_append_1(b, CODEC_SIMPLE_DESC_NEG_INT+3);
            slice_bytes_expand(b, 8);
            util_big_endian_write_uint64(&b->bytes.v[b->bytes.len], v->v.vUint64);
            b->bytes.len += 8;
        }
        break;
	case CODEC_VALUE_FLOAT32:
        slice_bytes_append_1(b, CODEC_SIMPLE_DESC_FLOAT32);
        slice_bytes_append(b, &v->v.vFloat32, 4);
        break;
	case CODEC_VALUE_FLOAT64:
        slice_bytes_append_1(b, CODEC_SIMPLE_DESC_FLOAT64);
        slice_bytes_append(b, &v->v.vFloat64, 8);
        break;
	case CODEC_VALUE_STRING:
        bd = CODEC_SIMPLE_DESC_STRING;
        // fprintf(stderr, "CODEC_VALUE_STRING: len: %ld\n", v->v.vString.bytes.len);
	case CODEC_VALUE_BYTES:
        if(bd == 0) bd = CODEC_SIMPLE_DESC_BYTES;
        // fprintf(stderr, "CODEC_VALUE_BYTES: len: %ld\n", v->v.vBytes.bytes.len);
        codec_simple_append_len(b, bd, v->v.vBytes.bytes.len);
        if(v->v.vBytes.bytes.len > 0) slice_bytes_append(b, &v->v.vBytes.bytes.v[0], v->v.vBytes.bytes.len);
        break;
	case CODEC_VALUE_ARRAY:
        bd = CODEC_SIMPLE_DESC_ARRAY;
        codec_simple_append_len(b, bd, v->v.vArray.len);
        for(int i = 0; i < v->v.vArray.len; i++) {
            codec_simple_encode(&v->v.vArray.v[i], b, err);
            if(*err != NULL) return;
        }
        break;        
	case CODEC_VALUE_MAP:
        bd = CODEC_SIMPLE_DESC_MAP;
        codec_simple_append_len(b, bd, v->v.vMap.len);
        for(int i = 0; i < v->v.vMap.len; i++) {
            codec_simple_encode(&v->v.vMap.v[i].k, b, err);
            if(*err != NULL) return;
            codec_simple_encode(&v->v.vMap.v[i].v, b, err);
            if(*err != NULL) return;
        }
        break;
	case CODEC_VALUE_CUSTOMEXT:
        bd = CODEC_SIMPLE_DESC_CUSTOMEXT;
        codec_simple_append_len(b, bd, v->v.vExt.v.bytes.len);
        slice_bytes_append(b, &v->v.vExt.tag, 1);
        if(v->v.vExt.v.bytes.len > 0)slice_bytes_append(b, &v->v.vExt.v.bytes.v[0], v->v.vExt.v.bytes.len);
        break;
    }
}

void codec_simple_decode_r(codec_decode_state* b, codec_value* v, char** err) {
    if(codec_decode_state_chk2(b, err)) return;
    if(codec_decode_state_chk(b, 1, err)) return;
    uint8_t bd = b->b.bytes.v[b->pos++];
    uint64_t tmp64;
    uint32_t tmp32;
    switch(bd) {
    case CODEC_SIMPLE_DESC_NIL:
        v->v.vNil = true;
        v->type = CODEC_VALUE_NIL;
        break;
	case CODEC_SIMPLE_DESC_FALSE:
        v->v.vBool = false;
        v->type = CODEC_VALUE_BOOL;
        break;
	case CODEC_SIMPLE_DESC_TRUE:
        v->type = CODEC_VALUE_BOOL;
        v->v.vBool = true;
        break;

	case CODEC_SIMPLE_DESC_POS_INT:
	case CODEC_SIMPLE_DESC_POS_INT+1:
	case CODEC_SIMPLE_DESC_POS_INT+2:
	case CODEC_SIMPLE_DESC_POS_INT+3:
        switch(bd%4) {
        case 0: tmp64 = codec_simple_readn(b, 1, err); break;
        case 1: tmp64 = codec_simple_readn(b, 2, err); break;
        case 2: tmp64 = codec_simple_readn(b, 4, err); break;
        case 3: tmp64 = codec_simple_readn(b, 8, err); break;
        }
        if(*err != NULL) return;
        v->type = CODEC_VALUE_POS_INT;
        v->v.vUint64 = tmp64;
        break;

	case CODEC_SIMPLE_DESC_NEG_INT:
	case CODEC_SIMPLE_DESC_NEG_INT+1:
	case CODEC_SIMPLE_DESC_NEG_INT+2:
	case CODEC_SIMPLE_DESC_NEG_INT+3:
        switch(bd%4) {
        case 0: tmp64 = codec_simple_readn(b, 1, err); break;
        case 1: tmp64 = codec_simple_readn(b, 2, err); break;
        case 2: tmp64 = codec_simple_readn(b, 4, err); break;
        case 3: tmp64 = codec_simple_readn(b, 8, err); break;
        }
        if(*err != NULL) return;
        v->type = CODEC_VALUE_NEG_INT;
        v->v.vUint64 = tmp64;
        break;

	case CODEC_SIMPLE_DESC_FLOAT32:
        v->type = CODEC_VALUE_FLOAT32;        
        if(codec_decode_state_chk(b, 4, err)) return;
        tmp32 = util_big_endian_read_uint32(&b->b.bytes.v[b->pos]);
        b->pos += 4;
        v->v.vFloat32 = *(&tmp32);
        break;
	case CODEC_SIMPLE_DESC_FLOAT64:
        v->type = CODEC_VALUE_FLOAT64;
        if(codec_decode_state_chk(b, 8, err)) return;
        tmp64 = util_big_endian_read_uint64(&b->b.bytes.v[b->pos]);
        b->pos += 8;
        v->v.vFloat64 = *(&tmp64);
        break;

	case CODEC_SIMPLE_DESC_STRING:
	case CODEC_SIMPLE_DESC_STRING+1:
	case CODEC_SIMPLE_DESC_STRING+2:
	case CODEC_SIMPLE_DESC_STRING+3:
	case CODEC_SIMPLE_DESC_STRING+4:
        v->type = CODEC_VALUE_STRING;
        // fallthrough
	case CODEC_SIMPLE_DESC_BYTES:
	case CODEC_SIMPLE_DESC_BYTES+1:
	case CODEC_SIMPLE_DESC_BYTES+2:
	case CODEC_SIMPLE_DESC_BYTES+3:
	case CODEC_SIMPLE_DESC_BYTES+4:
        if(v->type == 0) v->type = CODEC_VALUE_BYTES;
        tmp64 = codec_simple_decode_len(b, bd, err);
        if(*err != NULL) return;
        v->v.vBytes.bytes.len = (size_t)tmp64;
        v->v.vBytes.cap = v->v.vBytes.bytes.len;
        if(codec_decode_state_chk(b, v->v.vBytes.bytes.len, err)) return;
        v->v.vBytes.bytes.v = &b->b.bytes.v[b->pos];
        b->pos += v->v.vBytes.bytes.len;
        break;

	case CODEC_SIMPLE_DESC_ARRAY:
	case CODEC_SIMPLE_DESC_ARRAY+1:
	case CODEC_SIMPLE_DESC_ARRAY+2:
	case CODEC_SIMPLE_DESC_ARRAY+3:
	case CODEC_SIMPLE_DESC_ARRAY+4:
        v->type = CODEC_VALUE_ARRAY;
        tmp64 = codec_simple_decode_len(b, bd, err);
        if(*err != NULL) return;
        v->v.vArray.len = (size_t)tmp64;
        v->v.vArray.v = calloc(v->v.vArray.len, sizeof (codec_value));
        for(int i = 0; i < v->v.vArray.len; i++) {
            codec_simple_decode_r(b, &v->v.vArray.v[i], err);
            if(*err != NULL) return;
        }
        break;        

	case CODEC_SIMPLE_DESC_MAP:
	case CODEC_SIMPLE_DESC_MAP+1:
	case CODEC_SIMPLE_DESC_MAP+2:
	case CODEC_SIMPLE_DESC_MAP+3:
	case CODEC_SIMPLE_DESC_MAP+4:
        v->type = CODEC_VALUE_MAP;
        tmp64 = codec_simple_decode_len(b, bd, err);
        v->v.vMap.len = (size_t)tmp64;
        v->v.vMap.v = calloc(v->v.vMap.len, sizeof (codec_value_kv));
        // fprintf(stderr, "map: len: %ld\n", v->v.vMap.len);
        for(int i = 0; i < v->v.vMap.len; i++) {
            codec_simple_decode_r(b, &v->v.vMap.v[i].k, err);
            if(*err != NULL) return;
            codec_simple_decode_r(b, &v->v.vMap.v[i].v, err);
            if(*err != NULL) return;
        }
        break;

	case CODEC_SIMPLE_DESC_CUSTOMEXT:
	case CODEC_SIMPLE_DESC_CUSTOMEXT+1:
	case CODEC_SIMPLE_DESC_CUSTOMEXT+2:
	case CODEC_SIMPLE_DESC_CUSTOMEXT+3:
	case CODEC_SIMPLE_DESC_CUSTOMEXT+4:
        v->type = CODEC_VALUE_CUSTOMEXT;
        tmp64 = codec_simple_decode_len(b, bd, err);
        v->v.vExt.v.bytes.len = (size_t)tmp64;
        if(codec_decode_state_chk(b, 1 + v->v.vExt.v.bytes.len, err)) return;
        v->v.vExt.tag = b->b.bytes.v[b->pos++];
        v->v.vExt.v.bytes.v = &b->b.bytes.v[b->pos];
        b->pos += v->v.vExt.v.bytes.len;
       break;
    default:
        *err = (char*)malloc(128);
        snprintf(*err, 128, "codec_simple_decode_r: invalid descriptor: %d", bd);
    }
}

size_t codec_simple_decode(slice_bytes b, codec_value* v, char** err) {
    codec_decode_state s;
    memset(&s, 0, sizeof (codec_decode_state));
    s.b = b;
    codec_simple_decode_r(&s, v, err);
    return s.pos;
}


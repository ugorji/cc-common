#include "binc.h"
#include <stdio.h>

// Limitations:
//   - Encode/Decode of de-dup'ed symbols are not supported

uint64_t codec_binc_prune_leading(uint8_t* v, uint8_t pruneVal, size_t length) {
    if(length < 2) return 0;
    uint64_t n = 0;
    for(; v[n] == pruneVal && (n+1) < length && (v[n+1] == pruneVal); n++) {}
	return n;
}

uint64_t codec_binc_prune_sign_ext(uint8_t* v, size_t length, bool pos) {
    uint64_t n = 0;
    if(length < 2) {
	} if(pos && v[0] == 0) {
		for(; v[n] == 0 && (n+1) < length && (v[n+1]&(1<<7) == 0); n++) {}
	} else if(!pos && v[0] == 0xff) {
        for(; v[n] == 0xff && (n+1) < length && (v[n+1]&(1<<7) != 0); n++) {}
    }
    return n;
}

void codec_binc_encode_uint_prune(uint64_t v, uint8_t bd, bool pos, const uint8_t lim, slice_bytes* b) {
    uint8_t x[lim];
    memset(&x, 0, lim);
    if(lim == 4) util_big_endian_write_uint32(x, (uint32_t)v);
    else util_big_endian_write_uint32(x, v);
    int i = codec_binc_prune_sign_ext(x, lim, pos);
    bd |= (uint8_t)lim-1-i;
    slice_bytes_append(b, &bd, 1);
    slice_bytes_append(b, &x[i], lim-i);
}

uint64_t codec_binc_decode_uint64(uint8_t vs, codec_decode_state* b, char** err) {
    if(vs > 7) {
        *err = malloc(64);
        snprintf(*err, 64, "uint with greater than 64 bits of precision not supported");
        return 0;
    }
    uint8_t limit = (7 - vs);
    memset(&b->x[0], 0, 8);
    memcpy(&b->x[limit], &b->b.bytes.v[b->pos], 8-limit);
    b->pos += (8-limit);
    return util_big_endian_read_uint64(&b->x[0]);
}

size_t codec_binc_decode_len(uint8_t vs, codec_decode_state* b, char** err) {
    if(vs <= 3) return (size_t)codec_binc_decode_uint64(vs, b, err);
    else return vs - 4;
}

void codec_binc_decode_float_pre(uint8_t vs, uint8_t defLen, codec_decode_state* b, char** err) {
    memset(&b->x[0], 0, 8);
    if((vs & 0x8) == 0) {
        memcpy(&b->x[0], &b->b.bytes.v[b->pos], defLen);
        b->pos += defLen;
    } else {
        uint8_t l = b->b.bytes.v[b->pos++];
        if(l > 8) {
            *err = malloc(64);
            snprintf(*err, 64, "At most 8 bytes used to represent float. Received: %d bytes", l);
            return;
        }
        memcpy(&b->x[0], &b->b.bytes.v[b->pos], l);
        b->pos += l;
    }
}

double codec_binc_decode_float(uint8_t vs, codec_decode_state* b, char** err) {
    float* ff;
    double* dd;
    switch(vs & 0x7) {
    case 1:
        codec_binc_decode_float_pre(vs, 4, b, err);
        if(*err != NULL) return 0.0;
        ff = (float*)&b->x[0];
        return (double)(*ff);
    case 2:
        codec_binc_decode_float_pre(vs, 8, b, err);
        if(*err != NULL) return 0.0;
        dd = (double*)&b->x[0];
        return *dd;
    default:
        *err = malloc(64);
        snprintf(*err, 64, "only float32 and float64 are supported. vs: 0x%x", vs);
        return 0.0;
    }
    return 0.0;
}

void codec_binc_encode_uint(uint8_t bd, bool pos, uint64_t v, slice_bytes* b, char** err) {
    if(v == 0) {
        bd = ((uint8_t)CODEC_BINC_DESC_SPECIAL << 4) | (uint8_t)CODEC_BINC_SPECIAL_ZERO;
        slice_bytes_append(b, &bd, 1);
    } else if(pos && v >= 1 && v <= 16) {
        bd = ((uint8_t)CODEC_BINC_DESC_SMALLINT << 4) | (uint8_t)v-1;
        slice_bytes_append(b, &bd, 1);
    } else if(v <= CODEC_MaxUint8) {
        slice_bytes_append(b, &bd, 1);
        bd = (uint8_t)v;
        slice_bytes_append(b, &bd, 1);
    } else if(v <= CODEC_MaxUint16) {
        bd |= 0x1;
        slice_bytes_expand(b, 2);
        util_big_endian_write_uint16(&b->bytes.v[b->bytes.len], (uint16_t)v);
        b->bytes.len += 2;
        // slice_bytes_append(b, &bd, 1);
        // uint16_t v2 = (uint16_t)v;
        // slice_bytes_append(b, &v2, 2);
    } else if(v <= CODEC_MaxUint32) {
        codec_binc_encode_uint_prune(v, bd, pos, 4, b);
    } else {
        codec_binc_encode_uint_prune(v, bd, pos, 8, b);
        // bd |= 0x3;
        // slice_bytes_append(b, &bd, 1);
        // slice_bytes_expand(b, 8);
        // util_big_endian_write_uint64(&b->bytes.v[b->bytes.len], v);
        // b->bytes.len += 8;
        
    }
}

void codec_binc_encode_len(uint8_t bd, uint64_t len, slice_bytes* b, char** err) {
    if(len < 12) {
        bd |= (uint8_t)(len+4);
        slice_bytes_append(b, &bd, 1);
    } else {
        codec_binc_encode_uint(bd, false, len, b, err);
    }
}
 
void codec_binc_encode(codec_value* v, slice_bytes* b, char** err) {
    // fprintf(stderr, "Size of y: %d\n", sizeof(y));
    uint8_t bd = 0;
    // uint8_t arr[8];
    switch(v->type) {
    case CODEC_VALUE_NIL:
        bd = ((uint8_t)CODEC_BINC_DESC_SPECIAL << 4) | (uint8_t)CODEC_BINC_SPECIAL_NIL;
        slice_bytes_append(b, &bd, 1);
        break;
	case CODEC_VALUE_BOOL:
        if(v->v.vBool) bd = ((uint8_t)CODEC_BINC_DESC_SPECIAL << 4) | (uint8_t)CODEC_BINC_SPECIAL_TRUE;
        else bd = ((uint8_t)CODEC_BINC_DESC_SPECIAL << 4) | (uint8_t)CODEC_BINC_SPECIAL_FALSE;
        slice_bytes_append(b, &bd, 1);
        break;
	case CODEC_VALUE_NEG_INT: 
        codec_binc_encode_uint((uint8_t)CODEC_BINC_DESC_NEGINT << 4, false, v->v.vUint64, b, err);
        break;
	case CODEC_VALUE_POS_INT:
        codec_binc_encode_uint((uint8_t)CODEC_BINC_DESC_POSINT << 4, true, v->v.vUint64, b, err);
        break;
	case CODEC_VALUE_FLOAT32:
	case CODEC_VALUE_FLOAT64:
        if(v->v.vFloat64 == 0.0) {
            bd = ((uint8_t)CODEC_BINC_DESC_SPECIAL << 4) | (uint8_t)CODEC_BINC_SPECIAL_ZEROFLOAT;
            slice_bytes_append(b, &bd, 1);
            break;
        }
        // pruning
        uint8_t i = 7;
        for(; i >= 0 && ((&v->v.vFloat64)[i] == 0); i--) {}
        i++;
        if(i <= 6) {
            bd = ((uint8_t)CODEC_BINC_DESC_FLOAT << 4) | (uint8_t)0x8 | (uint8_t)0x2;
            slice_bytes_append(b, &bd, 1);
            slice_bytes_append(b, &i, 1);
            slice_bytes_append(b, &v->v.vFloat64, i);
        } else {
            bd = ((uint8_t)CODEC_BINC_DESC_FLOAT << 4) | (uint8_t)0x2;
            slice_bytes_append(b, &bd, 1);
            slice_bytes_append(b, &v->v.vFloat64, 8);
        }       
        // bd = ((uint8_t)CODEC_BINC_DESC_FLOAT << 4) | 2;
        // slice_bytes_append(b, &bd, 1);
        // slice_bytes_append(b, &v->v.vFloat64, 8);
        break;
	case CODEC_VALUE_STRING:
        codec_binc_encode_len((uint8_t)CODEC_BINC_DESC_STRING << 4, v->v.vString.bytes.len, b, err);
        if(*err != NULL) return;
        if(v->v.vString.bytes.len > 0) slice_bytes_append(b, v->v.vString.bytes.v, v->v.vString.bytes.len);
        break;
	case CODEC_VALUE_BYTES:
        codec_binc_encode_len((uint8_t)CODEC_BINC_DESC_BYTEARRAY << 4, v->v.vBytes.bytes.len, b, err);
        if(*err != NULL) return;
        if(v->v.vBytes.bytes.len > 0) slice_bytes_append(b, v->v.vBytes.bytes.v, v->v.vBytes.bytes.len);
        break;
	case CODEC_VALUE_ARRAY:
        codec_binc_encode_len((uint8_t)CODEC_BINC_DESC_ARRAY << 4, v->v.vArray.len, b, err);
        if(*err != NULL) return;
        for(int i = 0; i < v->v.vArray.len; i++) {
            codec_binc_encode(&v->v.vArray.v[i], b, err);
            if(*err != NULL) return;
        }
        break;        
	case CODEC_VALUE_MAP:
        codec_binc_encode_len((uint8_t)CODEC_BINC_DESC_MAP << 4, v->v.vMap.len, b, err);
        if(*err != NULL) return;
        // fprintf(stderr, "map: len: %ld, b.pos: %ld\n", v->v.vMap.len, b->bytes.len);
        for(int i = 0; i < v->v.vMap.len; i++) {
            codec_binc_encode(&v->v.vMap.v[i].k, b, err);
            if(*err != NULL) return;
            codec_binc_encode(&v->v.vMap.v[i].v, b, err);
            if(*err != NULL) return;
        }
        break;
	case CODEC_VALUE_CUSTOMEXT:
        codec_binc_encode_len((uint8_t)CODEC_BINC_DESC_CUSTOMEXT << 4, v->v.vExt.v.bytes.len, b, err);
        if(*err != NULL) return;
        slice_bytes_append(b, &v->v.vExt.tag, 1);
        if(v->v.vExt.v.bytes.len > 0) slice_bytes_append(b, &v->v.vExt.v.bytes.v, v->v.vExt.v.bytes.len);
        break;   
    }
}

void codec_binc_decode_r(codec_decode_state* b, codec_value* v, char** err) {
    if(codec_decode_state_chk(b, 1, err)) return;
    uint8_t bd = b->b.bytes.v[b->pos++];
    uint8_t vd = bd >> 4;
    uint8_t vs = bd & 0x0f;
    // fprintf(stderr, "decode_r: bd: 0x%x, vd:0x%x, vs:0x%x\n", bd, vd, vs);
    uint64_t dd;
    
    switch(vd) {
    case CODEC_BINC_DESC_SPECIAL:
        switch(vs) {
        case CODEC_BINC_SPECIAL_NIL:
            v->v.vNil = true;
            v->type = CODEC_VALUE_NIL;
            break;
        case CODEC_BINC_SPECIAL_FALSE:
            v->v.vBool = false;
            v->type = CODEC_VALUE_BOOL;
            break;
        case CODEC_BINC_SPECIAL_TRUE:
            v->type = CODEC_VALUE_BOOL;
            v->v.vBool = true;
            break;
        case CODEC_BINC_SPECIAL_NAN:
            v->type = CODEC_VALUE_FLOAT64;
            dd = CODEC_uvnan;
            v->v.vFloat64 = *(double*)(&dd);
            break;
        case CODEC_BINC_SPECIAL_POSINF:
            v->type = CODEC_VALUE_FLOAT64;
            dd = CODEC_uvinf;
            v->v.vFloat64 = *(double*)(&dd);
            break;
        case CODEC_BINC_SPECIAL_NEGINF:
            v->type = CODEC_VALUE_FLOAT64;
            dd = CODEC_uvneginf;
            v->v.vFloat64 = *(double*)(&dd);
            break;
        case CODEC_BINC_SPECIAL_ZEROFLOAT:
            v->type = CODEC_VALUE_FLOAT64;
            v->v.vFloat64 = 0.0;
            break;
        case CODEC_BINC_SPECIAL_ZERO:
            v->type = CODEC_VALUE_POS_INT;
            v->v.vUint64 = 0;
            break;
        case CODEC_BINC_SPECIAL_NEGONE:
            v->type = CODEC_VALUE_NEG_INT;
            v->v.vUint64 = 1;
            break;
        }
        break;
    case CODEC_BINC_DESC_SMALLINT:
        v->type = CODEC_VALUE_POS_INT;
        v->v.vUint64 = (uint64_t)((int8_t)vs + 1);
        break;
    case CODEC_BINC_DESC_POSINT:
        v->type = CODEC_VALUE_POS_INT;
        v->v.vUint64 = codec_binc_decode_uint64(vs, b, err);
        break;
    case CODEC_BINC_DESC_NEGINT:
        v->type = CODEC_VALUE_NEG_INT;
        v->v.vUint64 = codec_binc_decode_uint64(vs, b, err);
        break;
    case CODEC_BINC_DESC_FLOAT:
        v->type = CODEC_VALUE_FLOAT64;
        v->v.vFloat64 = codec_binc_decode_float(vs, b, err);
        break;
    case CODEC_BINC_DESC_SYMBOL:
    case CODEC_BINC_DESC_STRING:
        v->type = CODEC_VALUE_STRING;
        v->v.vString.bytes.len = codec_binc_decode_len(vs, b, err);
        if(*err != NULL) return;
        v->v.vString.bytes.v = &b->b.bytes.v[b->pos];
        b->pos += v->v.vString.bytes.len;
        break;
    case CODEC_BINC_DESC_BYTEARRAY:
        v->type = CODEC_VALUE_BYTES;
        v->v.vBytes.bytes.len = codec_binc_decode_len(vs, b, err);
        if(*err != NULL) return;
        v->v.vBytes.bytes.v = &b->b.bytes.v[b->pos];
        b->pos += v->v.vBytes.bytes.len;
        break;
    case CODEC_BINC_DESC_TIMESTAMP:
        *err = malloc(64);
        snprintf(*err, 64, "TIMESTAMP NOT SUPPORTED");
        return;
    case CODEC_BINC_DESC_CUSTOMEXT:
        v->type = CODEC_VALUE_CUSTOMEXT;
        v->v.vExt.v.bytes.len = codec_binc_decode_len(vs, b, err);
        if(*err != NULL) return;
        v->v.vExt.tag = b->b.bytes.v[b->pos++];
        v->v.vExt.v.bytes.v = &b->b.bytes.v[b->pos];
        b->pos += v->v.vExt.v.bytes.len;
        break;
    case CODEC_BINC_DESC_ARRAY:
        v->type = CODEC_VALUE_ARRAY;
        v->v.vArray.len = codec_binc_decode_len(vs, b, err);
        if(*err != NULL) return;
        v->v.vArray.v = calloc(v->v.vArray.len, sizeof (codec_value));
        for(int i = 0; i < v->v.vArray.len; i++) {
            codec_binc_decode_r(b, &v->v.vArray.v[i], err);
            if(*err != NULL) return;
        }
        break;
    case CODEC_BINC_DESC_MAP:
        v->type = CODEC_VALUE_MAP;
        v->v.vMap.len = codec_binc_decode_len(vs, b, err);
        if(*err != NULL) return;
        v->v.vMap.v = calloc(v->v.vMap.len, sizeof (codec_value_kv));
        for(int i = 0; i < v->v.vMap.len; i++) {
            codec_binc_decode_r(b, &v->v.vMap.v[i].k, err);
            if(*err != NULL) return;
            codec_binc_decode_r(b, &v->v.vMap.v[i].v, err);
            if(*err != NULL) return;
        }
        break;
    }
}

size_t codec_binc_decode(slice_bytes b, codec_value* v, char** err) {
    codec_decode_state s;
    memset(&s, 0, sizeof (codec_decode_state));
    s.b = b;
    codec_binc_decode_r(&s, v, err);
    return s.pos;
}


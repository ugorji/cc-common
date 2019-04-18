#ifndef _incl_ugorji_codec_codec_
#define _incl_ugorji_codec_codec_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../util/bigendian.h"
#include "../util/slice.h"

#define    CODEC_MaxInt8   1<<7 - 1
#define    CODEC_MinInt8   -1 << 7
#define    CODEC_MaxInt16  1<<15 - 1
#define    CODEC_MinInt16  -1 << 15
#define    CODEC_MaxInt32  1<<31 - 1
#define    CODEC_MinInt32  -1 << 31
#define    CODEC_MaxInt64  1<<63 - 1
#define    CODEC_MinInt64  -1 << 63
#define    CODEC_MaxUint8  1<<8 - 1
#define    CODEC_MaxUint16 1<<16 - 1
#define    CODEC_MaxUint32 1<<32 - 1
#define    CODEC_MaxUint64 1<<64 - 1
#define    CODEC_uvnan     0x7FF8000000000001
#define    CODEC_uvinf     0x7FF0000000000000
#define    CODEC_uvneginf  0xFFF0000000000000

//char* CODEC_DECODE_ERROR = "CODEC_DECODE_ERROR";

typedef enum codec_value_type {
    CODEC_VALUE_NIL  = 1,
	CODEC_VALUE_BOOL,
	CODEC_VALUE_POS_INT,
	CODEC_VALUE_NEG_INT,
	CODEC_VALUE_FLOAT32,
	CODEC_VALUE_FLOAT64,

	CODEC_VALUE_STRING,
	CODEC_VALUE_BYTES,
	CODEC_VALUE_ARRAY,
	CODEC_VALUE_MAP,

	CODEC_VALUE_CUSTOMEXT
} codec_value_type;

struct codec_value;
struct codec_value_kv;

typedef struct codec_value_list {
    size_t len;
    struct codec_value* v;
} codec_value_list;

typedef struct codec_value_map {
    size_t len;
    struct codec_value_kv* v;
} codec_value_map;

typedef struct codec_value_ext {
    uint8_t tag;
    slice_bytes v;
} codec_value_ext;
    
typedef union codec_value_discrim {
    bool vNil;
    bool vBool;
    uint64_t vUint64;
    double vFloat64;
    float vFloat32;
    slice_bytes vString;
    slice_bytes vBytes;
    codec_value_list vArray;
    codec_value_map vMap;
    codec_value_ext vExt;
} codec_value_discrim;

typedef struct codec_value {
    codec_value_type type;
    codec_value_discrim v; 
} codec_value;

typedef struct codec_value_kv {
    codec_value k;
    codec_value v;
} codec_value_kv;

typedef struct codec_decode_state {
    // uint8_t bd;
    uint8_t x[8];
    slice_bytes b;
    size_t pos;
} codec_decode_state;

typedef void (*codec_encode)(codec_value* v, slice_bytes* b, char** err);
typedef size_t (*codec_decode)(slice_bytes b, codec_value* v, char** err);

extern void codec_big_endian_write_uint64(uint8_t* b, uint64_t v);
extern uint64_t codec_big_endian_read_uint64(uint8_t* b);

// Free the codec_value recursively, depending on value of alloc_.
extern void codec_value_free(codec_value v);
extern void codec_value_print(codec_value* v, slice_bytes* b);
extern bool codec_decode_state_chk(codec_decode_state* b, size_t len, char** err);
extern bool codec_decode_state_chk2(codec_decode_state* b, char** err);

#ifdef __cplusplus
}  // end extern "C" 
#endif
#endif //_incl_ugorji_codec_codec_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "codec.h"

typedef enum codec_simple_desc_type {
    CODEC_SIMPLE_DESC_NIL = 1,
	CODEC_SIMPLE_DESC_FALSE = 2,
	CODEC_SIMPLE_DESC_TRUE = 3,
	CODEC_SIMPLE_DESC_FLOAT32 = 4,
	CODEC_SIMPLE_DESC_FLOAT64 = 5,
	CODEC_SIMPLE_DESC_POS_INT = 8,
	CODEC_SIMPLE_DESC_NEG_INT = 12,
	
	CODEC_SIMPLE_DESC_STRING = 216,
	CODEC_SIMPLE_DESC_BYTES = 224,
	CODEC_SIMPLE_DESC_ARRAY = 232,
	CODEC_SIMPLE_DESC_MAP = 240,

	CODEC_SIMPLE_DESC_CUSTOMEXT = 248
} codec_simple_desc_type;


extern void codec_simple_encode(codec_value* v, slice_bytes* b, char** err);
extern size_t codec_simple_decode(slice_bytes b, codec_value* v, char** err);

#ifdef __cplusplus
}  // end extern "C" 
#endif


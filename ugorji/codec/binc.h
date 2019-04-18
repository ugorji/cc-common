#ifndef _incl_ugorji_codec_binc_
#define _incl_ugorji_codec_binc_

#ifdef __cplusplus
extern "C" {
#endif

#include "codec.h"

typedef enum codec_binc_desc_type {
	CODEC_BINC_DESC_SPECIAL = 0,
	CODEC_BINC_DESC_POSINT,
	CODEC_BINC_DESC_NEGINT,
	CODEC_BINC_DESC_FLOAT,

	CODEC_BINC_DESC_STRING,
	CODEC_BINC_DESC_BYTEARRAY,
	CODEC_BINC_DESC_ARRAY,
	CODEC_BINC_DESC_MAP,

	CODEC_BINC_DESC_TIMESTAMP,
	CODEC_BINC_DESC_SMALLINT,
	CODEC_BINC_DESC_UNICODEOTHER,
	CODEC_BINC_DESC_SYMBOL,

	CODEC_BINC_DESC_DECIMAL,
	CODEC_BINC_DESC_OPENSLOT1____,
	CODEC_BINC_DESC_OPENSLOT2____,
	CODEC_BINC_DESC_CUSTOMEXT = 0X0F
} codec_binc_desc_type;

typedef enum codec_binc_special_desc_type {
	CODEC_BINC_SPECIAL_NIL = 0,
	CODEC_BINC_SPECIAL_FALSE,
	CODEC_BINC_SPECIAL_TRUE,
	CODEC_BINC_SPECIAL_NAN,
	CODEC_BINC_SPECIAL_POSINF,
	CODEC_BINC_SPECIAL_NEGINF,
	CODEC_BINC_SPECIAL_ZEROFLOAT,
	CODEC_BINC_SPECIAL_ZERO,
	CODEC_BINC_SPECIAL_NEGONE
} codec_binc_special_desc_type;

extern void codec_binc_encode(codec_value* v, slice_bytes* b, char** err);
extern size_t codec_binc_decode(slice_bytes b, codec_value* v, char** err);

#ifdef __cplusplus
}  // end extern "C" 
#endif
#endif //_incl_ugorji_codec_binc_



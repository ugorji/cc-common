#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern void util_big_endian_write_uint16(uint8_t* b, uint16_t v);
extern void util_big_endian_write_uint32(uint8_t* b, uint32_t v);
extern void util_big_endian_write_uint64(uint8_t* b, uint64_t v);
extern uint16_t util_big_endian_read_uint16(uint8_t* b);
extern uint32_t util_big_endian_read_uint32(uint8_t* b);
extern uint64_t util_big_endian_read_uint64(uint8_t* b);

#ifdef __cplusplus
}  // end extern "C" 
#endif

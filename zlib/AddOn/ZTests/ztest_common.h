// ZLIB tests tools
// CM 2022/08/29
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "zlib.h"

#define PLOG(F, ...)  do { fprintf(stderr, F"\n", ##__VA_ARGS__); } while (0)
#define PFAIL(F, ...) do { PLOG("[ERR "__FILE__":%s:%d] "F, __FUNCTION__, __LINE__, ##__VA_ARGS__); exit(1); } while (0)

#pragma mark - BENCHMARKING

int kpc_cycles_init(void);
uint64_t kpc_get_cycles(void);

#pragma mark - BUFFER API

// Encode buffer using zlib. Return number of compressed bytes, 0 on failure.
size_t zlib_encode_buffer(uint8_t* dst_buffer, size_t dst_size,
                          uint8_t* src_buffer, size_t src_size, int level, int rfc1950, int fixed);

// Decode buffer using zlib. Return number of uncompressed bytes, 0 on failure.
// Supports truncated decodes.
size_t zlib_decode_buffer(uint8_t* dst_buffer, size_t dst_size,
                          uint8_t* src_buffer, size_t src_size, int rfc1950);

// Decode buffer using zlib using infback interface. Return number of uncompressed bytes, 0 on failure.
// Supports truncated decodes.
size_t zlib_decode_infback(uint8_t* dst_buffer, size_t dst_size,
                           uint8_t* src_buffer, size_t src_size);

// Decode buffer using zlib. Torture streaming API. Return number of uncompressed bytes, 0 on failure.
size_t zlib_decode_torture(uint8_t* dst_buffer, size_t dst_size,
                           uint8_t* src_buffer, size_t src_size, int rfc1950);

#pragma mark - CHECKSUMS

// Return Crc32 of DATA[LEN]. Naive implementation.
uint32_t simple_crc32(uint8_t* src_buffer, const size_t src_size);

// Return Adler32 of DATA[LEN]. Naive implementation.
uint32_t simple_adler32(const unsigned char* src, const size_t src_size);

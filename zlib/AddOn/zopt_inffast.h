#pragma once

#include "zopt_defs.h"

#if defined(INFFAST_OPT)

#if defined(__SSE2__)
#include <immintrin.h>
#endif

#if defined __arm64__
#include <arm_neon.h>
#endif

#define INFLATE_INLINE static inline __attribute__((__always_inline__)) __attribute__((__overloadable__))

#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_expect)
# define likely(x)    __builtin_expect((x),1)
# define unlikely(x)  __builtin_expect((x),0)
#else
# define likely(x)    (x)
# define unlikely(x)  (x)
#endif

#pragma mark - MATCH COPY

// Shuffle vector X using permutation PERM.
INFLATE_INLINE vector_uchar16 inflate_shuffle(vector_uchar16 x, vector_uchar16 perm) // OK
{
#if defined(__SSE2__)
  return _mm_shuffle_epi8(x, perm);
#elif defined(__arm64__)
  return vqtbl1q_u8(x, perm);
#endif
}

// Copy 1-15 bytes from SRC to DST.
INFLATE_INLINE void inflate_copy_tail(uint8_t* dst, const uint8_t* src, uint32_t len) // OK
{
  if (len & 1) { *dst++ = *src++; }
  if (len & 2) { *(packed_uint16_t*)dst = *(packed_uint16_t*)src; src += 2; dst += 2; }
  if (len & 4) { *(packed_uint32_t*)dst = *(packed_uint32_t*)src; src += 4; dst += 4; }
  if (len & 8) { *(packed_uint64_t*)dst = *(packed_uint64_t*)src; src += 8; dst += 8; }
}

// Copy LEN bytes from DST-DISTANCE to DST with overlap.
INFLATE_INLINE void inflate_copy_with_overlap(uint8_t* dst, uint32_t distance, uint32_t len) // OK
{
#if defined(__SSE2__) || defined(__arm64__)
  const static vector_uchar16 repeat_perm[16] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    {14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15},
    {13,14,15,13,14,15,13,14,15,13,14,15,13,14,15,13},
    {12,13,14,15,12,13,14,15,12,13,14,15,12,13,14,15},
    {11,12,13,14,15,11,12,13,14,15,11,12,13,14,15,11},
    {10,11,12,13,14,15,10,11,12,13,14,15,10,11,12,13},
    { 9,10,11,12,13,14,15, 9,10,11,12,13,14,15, 9,10},
    { 8, 9,10,11,12,13,14,15, 8, 9,10,11,12,13,14,15},
    { 7, 8, 9,10,11,12,13,14,15, 7, 8, 9,10,11,12,13},
    { 6, 7, 8, 9,10,11,12,13,14,15, 6, 7, 8, 9,10,11},
    { 5, 6, 7, 8, 9,10,11,12,13,14,15, 5, 6, 7, 8, 9},
    { 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 4, 5, 6, 7},
    { 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 3, 4, 5},
    { 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 2, 3},
    { 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 1}
    };
  const static uint8_t repeat_distance[16] = {0,1+16,2+16,3+15,4+16,5+15,6+12,7+14,8+16,9+9,10+10,11+11,12+12,13+13,14+14,15+15};

  // Small overlap?
  if (distance < 16)
  {
    const vector_uchar16 pattern = inflate_shuffle(*(packed_uchar16*)(dst - 16), repeat_perm[distance]);

    // Small length?
    if (likely(len < 16))
    {
      inflate_copy_tail(dst, (const uint8_t*)&pattern, len);
      return;
    }
    
    // Copy pattern and transition to distance >= 16
    *(packed_uchar16*)dst = pattern;
    len -= 16;
    dst += 16;
    distance = repeat_distance[distance];
  }
  
  // Copy vectors with distance >= 16
  for (; len >= 16; len -= 16)
  {
    *(packed_uchar16*)dst = *(packed_uchar16*)(dst - distance);
    dst += 16;
  }
  // Copy tail
  inflate_copy_tail(dst, dst - distance, len);
#else
  for (; len > 0; len--, dst++) *dst = *(dst - distance);
#endif
}

// Copy w/o overlap with <= 15 bytes left excess. Does NEVER write beyond DST+LEN.
INFLATE_INLINE void inflate_copy_without_overlap(uint8_t* dst, const uint8_t* src, uint32_t len) // OK
{
  const uint32_t left_excess = -len & 15;
  uint8_t* left = dst - 16;
  dst -= left_excess; // NEVER write beyond DST+LEN
  src -= left_excess;

  // Save context
  const vector_uchar16 save_left = *(packed_uchar16*)left;
  
  // Copy first 16 bytes
  *(packed_uchar16*)dst = *(packed_uchar16*)src;

  // Copy remaining vectors
  for (; len > 16; len -= 16)
  {
    dst += 16;
    src += 16;
    *(packed_uchar16*)dst = *(packed_uchar16*)src;
  }
  
  // Restore context
  *(packed_uchar16*)left = save_left;
}

#endif

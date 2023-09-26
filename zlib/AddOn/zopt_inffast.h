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

// Copy with overlap and distance < 16. Can overshoot by 47 bytes.
INFLATE_INLINE uint8_t* inflate_copy_with_overlap_small(uint8_t* dst, const size_t distance, const size_t len) // OK
{
  Assert((distance) && (distance < 16), "bad overlapping distance");
  uint8_t* dst_end = dst + len;
#if defined(__SSE2__) || defined(__arm64__)
  #define _ 0
  const static vector_uchar16 repeat_perm[15] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1,  0,  1,  0,  1,  0, 1},
    {0, 1, 2, 0, 1, 2, 0, 1, 2, 0,  1,  2,  0,  1,  2, _},
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1,  2,  3,  0,  1,  2, 3},
    {0, 1, 2, 3, 4, 0, 1, 2, 3, 4,  0,  1,  2,  3,  4, _},
    {0, 1, 2, 3, 4, 5, 0, 1, 2, 3,  4,  5,  _,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 0, 1, 2,  3,  4,  5,  6,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 0, 1,  2,  3,  4,  5,  6, 7},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, _,  _,  _,  _,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  _,  _,  _,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,  _,  _,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,  _,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,  _,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,  _, _},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, _},
  };
  #undef _
  const static uint8_t repeat_size[15] = {16, 16, 15, 16, 15, 12, 14, 16, 9, 10, 11, 12, 13, 14, 15};
  const vector_uchar16 pattern = inflate_shuffle(*(packed_uchar16*)(dst - distance), repeat_perm[distance - 1]);
  const size_t pattern_size = repeat_size[distance - 1];
  const uint8_t* src = dst - distance;

  *(packed_uchar16*)(dst                    ) = pattern;
  *(packed_uchar16*)(dst + pattern_size     ) = *(packed_uchar16*)(src     ); // distance + pattern_size >= 16
  *(packed_uchar16*)(dst + pattern_size + 16) = *(packed_uchar16*)(src + 16);
  src -= pattern_size;
  for (size_t i = pattern_size + 32; (i < len); i += 16)
  {
    *(packed_uchar16*)(dst + i) = *(packed_uchar16*)(src + i);
  }
#else
  // No shuffle available, copy byte by byte.
  do { *dst = *(dst - distance); } while (++dst < dst_end);
#endif
  return dst_end;
}

// Copy with overlap and distance >= 16. Can overshoot by 47 bytes.
INFLATE_INLINE uint8_t* inflate_copy_with_overlap_large(uint8_t* dst, const size_t distance, uint32_t len) // OK
{
  Assert(distance >= 16, "bad overlapping distance");
  const uint8_t* src = dst - distance;
  uint8_t* dst_end = dst + len;
  
  *(packed_uchar16*)(dst     ) = *(packed_uchar16*)(src     );
  *(packed_uchar16*)(dst + 16) = *(packed_uchar16*)(src + 16);
  *(packed_uchar16*)(dst + 32) = *(packed_uchar16*)(src + 32);
  for (size_t i = 48; (i < len); i += 16)
  {
    *(packed_uchar16*)(dst + i) = *(packed_uchar16*)(src + i);
  }
  return dst_end;
}

// Copy without overlap. Can overshoot 63 bytes.
INFLATE_INLINE uint8_t* inflate_copy_fast(uint8_t* dst, const uint8_t* restrict src, const uint32_t len) // OK
{
  uint8_t* dst_end = dst + len;
  
  *(packed_uchar16*)(dst +  0) = *(packed_uchar16*)(src +  0);
  *(packed_uchar16*)(dst + 16) = *(packed_uchar16*)(src + 16);
  *(packed_uchar16*)(dst + 32) = *(packed_uchar16*)(src + 32);
  *(packed_uchar16*)(dst + 48) = *(packed_uchar16*)(src + 48);
  for (size_t i = 64; (i < len); i += 16)
  {
    *(packed_uchar16*)(dst + i) = *(packed_uchar16*)(src + i);
  }
  return dst_end;
}

#endif

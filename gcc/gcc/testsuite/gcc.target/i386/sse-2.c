/* APPLE LOCAL begin radar 4043818 */
/* { dg-do compile { target i?86*-*-* } } */
/* { dg-options "-O3 -msse" } */
/* { dg-final { scan-assembler-times "movq" 4 } } */

typedef int __m64 __attribute__ ((__vector_size__ (8)));

static __inline __m64 __attribute__((__always_inline__))
_mm_add_si64 (__m64 __m1, __m64 __m2)
{
  return (__m64) __builtin_ia32_paddq ((long long)__m1, (long long)__m2);
}


__m64 unsigned_add3( const __m64 *a, const __m64 *b, __m64 *result, unsigned long count )
{
  __m64 sum, _a, _b;
  unsigned int i;

  _a = a[0];
  _b = b[0];

  sum = _mm_add_si64( _a, _b );
  for( i = 1; i < count; i++ )
  {
   result[i-1] = sum;
   _a = a[i];
   _b = b[i];
   sum = _mm_add_si64( _a, _b );
  }
  return sum;
}
/* APPLE LOCAL end radar 4043818 */

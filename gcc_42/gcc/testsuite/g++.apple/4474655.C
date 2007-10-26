/* APPLE LOCAL file 4474655 4448096 */
/* { dg-do compile { target i?86-apple-darwin* } } */
/* { dg-options "-O2 -fapple-kext -mfpmath=sse -msse2" } */
#include <stdlib.h>
double d = 42.0;
unsigned int i;
main ()
{
  i = d;
  return 0;
}
/* The SSE-only double -> uint32 conversion uses several vector
   instructions (including "maxsd").  For a brief period, -fapple-kext
   disabled SSE vector instructions; this test insures that these
   instructions are permitted and generated.  See also
   gcc.apple/4448096-1.c.  */
/* { dg-final { scan-assembler "(maxsd|cvttsd2si)" } } */

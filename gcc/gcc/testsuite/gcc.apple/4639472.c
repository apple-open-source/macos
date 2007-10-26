/* { dg-do compile { target i?86*-*-darwin* } } */
/* { dg-skip-if "" { i?86-*-* } { "-m64" } { "" } } */
/* { dg-options "-mfpmath=sse" } */
/* x86_32, SSE FP only.  */
float
farce (unsigned int ui)
{
  return ui;
}
/* { dg-final { scan-assembler "mulss" } } */

/* APPLE LOCAL file CW asm blocks */
/* { dg-do assemble { target i?86*-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-options { -fasm-blocks -msse3 } } */
/* Radar 4419735 */

void foo(short* pSrc) {
  short oldcw;
  asm {
    mov esi, pSrc
    fild [WORD PTR esi]
    fild [WORD PTR esi + eax*2]
    fstcw oldcw
    fnstcw oldcw
  }
}

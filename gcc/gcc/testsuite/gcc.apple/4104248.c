/* APPLE LOCAL file 4104248 */
/* { dg-do compile { target "i?86*-*-darwin*" } }
/* { dg-options "-Os" } */
register char foo asm("edi");
char x;
int bar() {
  foo = x;
}
/* { dg-final { scan-assembler "movzbl" } } */

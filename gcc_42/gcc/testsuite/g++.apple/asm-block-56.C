/* { dg-do compile { target i?86*-*-darwin* } } */
/* { dg-options { -fasm-blocks } } */
/* Radar 4653181 */

void foo() {
  asm mul edx
}

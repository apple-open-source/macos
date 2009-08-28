/* APPLE LOCAL file EH __TEXT __gcc_except_tab 5819051 */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-final { scan-assembler "section __TEXT,__gcc_except_tab" } } */
/* Radar 5819051 */

#include <stdio.h>

void foo() {
  try {
    throw 1;
  } catch (int i) {
    printf("Hi\n");
  }
}

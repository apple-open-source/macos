/* APPLE LOCAL file 4232445 */
/* Contributed by Devang Patel  <dpatel@apple.com>  */

/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-gstabs+ -fno-eliminate-unused-debug-symbols" } */

#include <Carbon/Carbon.h>

/* { dg-final { scan-assembler ".stabs.*xsICAHeader:," } } */


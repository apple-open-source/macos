/* APPLE LOCAL file debugging */
/* Ensure that the compiler does not emit redundant BINCLs
   or EINCLs for the main translation unit.  */
/* Author: Ziemowit Laski <zlaski@apple.com> */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-gfull" } */

int foo(void) {
  return 2;
}

#include "stabs-bincl-1.h"
#include "stabs-bincl-2.h"

/* { dg-final { scan-assembler-not ".stabs-bincl.c.,130,0,0,0" } } */
/* { dg-final { scan-assembler ".stabs-bincl-1.h.,130,0,0,0" } } */
/* { dg-final { scan-assembler ".stabs-bincl-2.h.,130,0,0,0" } } */



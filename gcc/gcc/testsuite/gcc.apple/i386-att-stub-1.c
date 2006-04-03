/* APPLE LOCAL file Radar 4164563 */
/* { dg-do compile { target i?86-*-darwin* } } */
/* { dg-options "-matt-stubs" } */
/* { dg-final { scan-assembler "hlt" } } */

#include <stdio.h>

int main()
{ 
  printf ("hello, world\n");
}

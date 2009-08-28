/* APPLE LOCAL file blocks stabs 6034272 */
/* { dg-do compile } */
/* { dg-options "-gstabs" } */

void foo() {
   void (^x)(void) = ^{ foo(); };
   void *y = (void *)x;
}

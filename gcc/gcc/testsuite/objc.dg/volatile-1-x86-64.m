/* APPLE LOCAL file radar 4894756 */
/* Test for proper handling of volatile parameters in ObjC methods.  */
/* { dg-do compile { target i?86-*-darwin* } } */
/* { dg-options "-mmacosx-version-min=10.5 -O2 -m64" } */
/* Contributed by Ziemowit Laski  <zlaski@apple.com>  */

@interface Test
-(void) test2: (volatile int) a;
@end

@implementation Test
-(void) test2: (volatile int) a
{
  /* The following assignment should NOT be optimized away.  */
  a = 1;
}
@end

/* { dg-final { scan-assembler "movl\t\\\$1, -4\\(%rbp\\)" { target i?86*-*-darwin* } } } */

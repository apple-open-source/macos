/* APPLE LOCAL file radar 4664707 */
/* Test sequence of assignment to setters. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -std=c99 -lobjc" } */
/* { dg-do run { target *-*-darwin* } } */

#include <objc/objc.h>
#include "../objc/execute/Object2.h"
extern void abort (void);

@interface Bar : Object
{
  int iVar;
  int iBar;
  float f;
}
@property (ivar = iVar) int prop1;
@property (ivar = iBar) int prop2;
@property (ivar = f) int fprop;
@end

@implementation Bar
@property (ivar = iVar, setter = MySetter:) int prop1;

- (void) MySetter : (int) value { iVar = value; }

@end

int main(int argc, char *argv[]) {
    Bar *f = [Bar new];
    if (f.prop2 = 1)
      f.prop2 = f.prop1 = -4;
    if (f.prop1 == -4)
      f.prop2 = f.prop1 = 5;

    if (f.prop1 != 5 || f.prop2 != 5)
      abort ();

    f.fprop = 3.14;
    f.prop1 = f.prop2 = f.fprop;
    if (f.prop1 != 3 || f.prop2 != 3)
      abort ();
    while (f.prop1)
      f.prop1 -= 1;
    return f.prop1;
}


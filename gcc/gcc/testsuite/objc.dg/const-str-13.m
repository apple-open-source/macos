/* APPLE LOCAL file 4080358 */
/* Test if ObjC strings play nice with -fwritable-strings.  */
/* Author: Ziemowit Laski  */

/* { dg-options "-fno-constant-cfstrings -fwritable-strings -fconstant-string-class=Foo" } */
/* { dg-do run { target *-*-darwin* } } */

#include <objc/Object.h>
#include <stdlib.h>
#include <memory.h>

@interface Foo: Object {
  char *cString;
  unsigned int len;
}
- (char *)c_string;
@end

struct objc_class _FooClassReference;

static Foo *foobar = @"Apple";

@implementation Foo
- (char *)c_string {
  return cString;
}
@end

int main(void) {
  char *c, *d;

  /* Initialize the metaclass.  */
  memcpy(&_FooClassReference, objc_getClass("Foo"), sizeof(_FooClassReference));
  c = [foobar c_string];
  d = [@"Hello" c_string];

  if (*c != 'A')
    abort ();

  if (*d != 'H')
    abort ();

  return 0;
}

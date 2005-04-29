/* APPLE LOCAL file Objective-C++ */
/* Test whether we can manipulate instance variables inside of instance methods,
   even if global symbols of the same name exist.  */
/* Author: Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do compile } */
/* { dg-options "" } */

enum { foo = 7, fooz = 9 };

struct bar {
  int a, b;
};

@interface myClass {
  struct bar foo;
  int fooz;
}
- (void) setFoo:(int)n;
@end

@implementation myClass
- (void) setFoo:(int)n
{
  fooz = n;
  foo.a = n;
  foo.b = n + 1;
}
@end


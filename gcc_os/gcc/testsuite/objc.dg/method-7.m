/* APPLE LOCAL file test suite */
/* Check if finding multiple signatures for a method is handled gracefully.  */
/* Author:  Ziemowit Laski <zlaski@apple.com>  */
/* { dg-do compile } */

#include <objc/Object.h>

@interface Class1
- (void)setWindow:(Object *)wdw;
@end

@interface Class2
- (void)setWindow:(Class1 *)window;
@end

id foo(void) {
  Object *obj = [[Object alloc] init];
  [obj setWindow:nil];  /* { dg-warning ".Object. may not respond to .\\-setWindow:." } */
	/* { dg-warning "multiple declarations for method .setWindow:." "" { target *-*-* } 18 } */
	/* { dg-warning "using .\\-\\(void\\)setWindow:\\(Object \\*\\)wdw." "" { target *-*-* } 9 } */
	/* { dg-warning "also found .\\-\\(void\\)setWindow:\\(Class1 \\*\\)window." "" { target *-*-* } 13 } */
  return obj;
}

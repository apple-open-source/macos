/* APPLE LOCAL file ObjC GC */
/* Do _not_ generate write barriers for global function pointers,
   even ones returning 'id'.  */
/* { dg-do compile } */
/* { dg-options "-fnext-runtime -fobjc-gc -Wassign-intercept" } */

#include <objc/Object.h>

@interface Base: Object {
  int a;
}
@end

static IMP globalIMP = 0;

void foo(void) {
   IMP myIMP = [Object methodFor:@selector(new)];
   globalIMP = myIMP;
}

void bar(void) {
  Object *obj = 0;
  obj = [Object new];
  (Base *)obj = [Base new];  
}

void baz(id *b1) {
  id a1[4];
  int i;
  for(i = 0; i < 4; ++i) {
    a1[i] = b1[i];
  }
}

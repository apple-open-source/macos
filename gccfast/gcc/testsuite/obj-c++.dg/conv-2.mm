/* APPLE LOCAL file Objective-C++ */
/* Test for implicit use of conversion operators for message receivers.  */
/* Author:  Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do compile } */

#include <objc/Object.h>

extern "C" void abort(void);
#define CHECK_IF(expr) if(!(expr)) abort()

@interface Obj: Object {
  int a;
}
- (id)init;
- (int)getValue;
@end

template <class T>
struct CSharedIDRef {
  T *_obj;
  CSharedIDRef(void): _obj([[T alloc] init]) { }
};

void foo(void) {
  CSharedIDRef<Obj> myObj;
  id obj2;

  obj2 = myObj;   /* { dg-error "cannot convert" } */
  [myObj getValue];    /* { dg-error "cannot convert" } */
  [myObj._obj getValue];
  [obj2 getValue];
}

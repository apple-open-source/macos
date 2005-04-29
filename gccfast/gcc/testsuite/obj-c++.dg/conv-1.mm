/* APPLE LOCAL file Objective-C++ */
/* Test for implicit use of conversion operators for message receivers.  */
/* Author:  Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do run } */

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
  operator id() const { return _obj; }
};

@implementation Obj
- (id)init { a = 3456; return self; }
- (int)getValue { return a * 2; }
@end

int main(void) {
  CSharedIDRef<Obj> myObj;
  id obj2;

  obj2 = myObj;
  CHECK_IF([myObj getValue] == 3456 * 2);
  CHECK_IF([myObj._obj getValue] == 3456 * 2);
  CHECK_IF([obj2 getValue] == 3456 * 2);

  return 0;
}

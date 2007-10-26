/* APPLE LOCAL file radar 4590191 */
/* Any exception usage should generate a warning when  -mmacosx-version-min < 10.3 
(since use of the feature depends on 10.3 specific API's) */
/* { dg-options "-mmacosx-version-min=10.2" } */
/* { dg-do compile } */

/* APPLE LOCAL begin radar 4894756 */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
#include "../objc/execute/Object2.h"
/* APPLE LOCAL end radar 4894756 */

@protocol Proto1
- (int)meth1;
@end

@protocol Proto2
- (int)meth2;
@end

@interface MyClass: Object <Proto2> {
  int a;
}
- (int)meth2;
- (Object *)parm1: (id)p1 parm2: (id<Proto1>)p2;
@end

MyClass *mc1, *mc2;

@implementation MyClass
- (int)meth2 {
  return a;
}
- (Object *)parm1: (id)p1 parm2: (id<Proto1>)p2 {
  @try {
  }			/* { dg-warning "Mac OS X version 10.3" } */
  @catch (id exc) {
    return exc;
  }
  mc1 = p1;  /* no warning here! */
  return self;
}
@end  

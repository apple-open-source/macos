/* APPLE LOCAL file mainline */
/* { dg-do compile } */
/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"

@interface Derived: Object
@end

extern Object* foo(void);
static Derived *test(void)
{
   Derived *m = foo();   /* { dg-warning "initialization from distinct Objective\\-C type" } */

   return m;
}


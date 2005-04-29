/* Contributed by Nicola Pero - Thu Mar  8 16:27:46 CET 2001 */
#include <objc/objc.h>

/* APPLE LOCAL objc test suite */
#include "next_mapping.h"

/* Test that instance methods of root classes are available as class 
   methods to other classes as well */

@interface RootClass
{
  Class isa;
}
- (id) self;
@end

@implementation RootClass
- (id) self
{
  return self;
}
/* APPLE LOCAL begin objc test suite */
#ifdef __NEXT_RUNTIME__                                   
+ initialize { return self; }
#endif
/* APPLE LOCAL end objc test suite */
@end

@interface NormalClass : RootClass
@end

@implementation NormalClass : RootClass
@end

int main (void)
{
  Class normal = objc_get_class ("NormalClass");

  if (normal == Nil)
    {
      abort ();
    }

  if ([NormalClass self] != normal)
    {
      abort ();
    }

  return 0;
}

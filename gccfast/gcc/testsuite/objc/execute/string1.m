/* Based on a test case contributed by Nicola Pero.  */

#include <string.h>
#include <stdlib.h>

/* APPLE LOCAL begin objc test suite */
#ifdef __NEXT_RUNTIME__
#import <Foundation/NSString.h>
#else
#include <objc/NXConstStr.h>
#endif
/* APPLE LOCAL end objc test suite */

int main(int argc, void **args)
{
  if (strcmp ([@"this is a string" cString], "this is a string"))
    abort ();
  return 0;
}

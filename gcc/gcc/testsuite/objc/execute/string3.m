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

#define STRING "this is a string"

int main (int argc, void **args)
{
  if (strcmp ([@STRING cString], STRING))
    abort ();
  return 0;
}

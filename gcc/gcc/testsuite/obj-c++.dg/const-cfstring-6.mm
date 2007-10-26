/* APPLE LOCAL file 4080358 */
/* Test if constant CFstrings play nice with -fwritable-strings.  */
/* Author: Ziemowit Laski  */

/* { dg-options "-fconstant-cfstrings -fwritable-strings -framework Foundation" } */
/* { dg-do run { target *-*-darwin* } } */

#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <memory.h>

typedef const struct __CFString * CFStringRef;

static CFStringRef foobar = (CFStringRef)@"Apple";

int main(void) {
  char *c, *d;

#   if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5 || __OBJC2__)
NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
  c = (char *)[(id)foobar UTF8String];
  d = (char *)[(id)@"Hello" UTF8String];
#else
  c = (char *)[(id)foobar cString];
  d = (char *)[(id)@"Hello" cString];
#endif

  if (*c != 'A')
    abort ();

  if (*d != 'H')
    abort ();

#   if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5 || __OBJC2__)
[pool release];
#endif

  return 0;
}

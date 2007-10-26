/* APPLE LOCAL file mainline */
/* Sanity check for GNU-runtime version of constant strings,
   regardless of runtime used on target system.  */

/* { dg-do compile } */
/* { dg-options "-fgnu-runtime" } */
/* APPLE LOCAL radar 4894756 */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>
#include <string.h>
#include <stdlib.h>

@interface NXConstantString: Object
{
  char *c_string;
  unsigned int len;                                                     
}
-(const char *) cString;
-(unsigned int) length;
@end

@implementation NXConstantString
-(const char *) cString { return c_string; }
-(unsigned int) length  { return len; }
@end

int main(int argc, void **args)
{
  if (strcmp ([@"this is a string" cString], "this is a string"))
    abort ();
  return 0;
}

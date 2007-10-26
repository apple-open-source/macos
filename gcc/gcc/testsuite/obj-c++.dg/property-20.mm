/* APPLE LOCAL file radar 4695274 */
/* Check for correct offset calculation of inserted 'ivar' into the interface class. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do run { target *-*-darwin* } } */
/* APPLE LOCAL radar 4492976 */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/objc.h>
#include <objc/Object.h>
#include <stdlib.h>

@interface BASE
{
	double ivarBASE;
}
@property (ivar) double pp;
@end

@implementation BASE
@end

@interface XXX : BASE
{
@public
	char *pch1;
}
@end

@implementation XXX
@end

int main ()
{
  if (offsetof (XXX, pch1) != 16)
    abort ();
  return 0;
}

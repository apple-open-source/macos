/* APPLE LOCAL file mainline */
/* APPLE LOCAL radar 4585769 */
/* Test that the correct version number (7) is set in the module descriptor
   when compiling for the NeXT runtime.  */
/* Author: Ziemowit Laski <zlaski@apple.com>  */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-fnext-runtime" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"

@interface FooBar: Object
- (void)boo;
@end

@implementation FooBar
- (void)boo { }
@end

/* { dg-final { scan-assembler "L_OBJC_MODULES:\n\[ \t\]*\.long\t7\n" } } */

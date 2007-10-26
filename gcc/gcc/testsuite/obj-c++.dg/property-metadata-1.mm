/* APPLE LOCAL file radar 4498373 */
/* Test for a Synthesized Property */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>

@interface Number : Object
@property (ivar) double value;
@end

@implementation Number
@property double value;
@end
/* { dg-final { scan-assembler ".long\t8\n\t.long\t1\n\t.long\t.*\n\t.long\t.*" } } */

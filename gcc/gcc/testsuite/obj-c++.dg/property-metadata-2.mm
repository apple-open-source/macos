/* APPLE LOCAL file radar 4498373 */
/* Test for a Dynamic Property */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>

@interface ManagedObject: Object
@end

@interface ManagedObject (Asset)
@property (dynamic) const char *partNumber;
@property (dynamic) const char *serialNumber;
@property (dynamic) float *cost;
@end

@implementation  ManagedObject (Asset)
// partNumber, serialNumber, and cost are dynamic properties.
@end
/* { dg-final { scan-assembler ".long\t8\n\t.long\t3\n\t.long\t.*\n\t.long\t.*\n\t.long\t.*\n\t.long\t.*\n\t.long\t.*\n\t.long\t.*" } } */

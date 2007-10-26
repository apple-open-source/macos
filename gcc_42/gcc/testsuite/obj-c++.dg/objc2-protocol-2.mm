/* APPLE LOCAL file 4695109 */
/* Check for generation of protocol meta-data */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-do compile { target *-*-darwin* } } */

@protocol Proto1
@end

@protocol Proto2
@end

@interface Super <Proto1, Proto2> { id isa; } @end
@implementation Super @end
/* { dg-final { scan-assembler "L_ZL23_OBJC_PROTOCOL_\\\$_Proto1:" } } */
/* { dg-final { scan-assembler "L_ZL23_OBJC_PROTOCOL_\\\$_Proto2:" } } */

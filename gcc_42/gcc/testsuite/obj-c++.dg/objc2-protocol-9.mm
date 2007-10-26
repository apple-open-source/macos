/* APPLE LOCAL file 4695109 */
/* Cechk that both protocols metadata generated for them. */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-do compile { target *-*-darwin* } } */


@protocol Proto1
@end

@protocol Proto2
@end

@interface Foo
@end

@interface Foo (Category) <Proto1, Proto2>
@end

@implementation Foo (Category)
@end
/* { dg-final { scan-assembler "L_ZL23_OBJC_PROTOCOL_\\\$_Proto1:" } } */
/* { dg-final { scan-assembler "L_ZL23_OBJC_PROTOCOL_\\\$_Proto2:" } } */

/* APPLE LOCAL file radar 4805321 */
/* Test that we call objc_assign_weak and objc_read_weak */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-fobjc-new-property -mmacosx-version-min=10.5 -fobjc-gc" } */

@interface INTF
{
  __weak id IVAR;
}
@property (assign) __weak id uses_inclass_weak;
@property  (assign) __weak id uses_default_weak;
@end

@implementation INTF
@synthesize uses_inclass_weak = IVAR, uses_default_weak = IVAR;
@end
/* { dg-final { scan-assembler "objc_assign_weak" } } */
/* { dg-final { scan-assembler "objc_read_weak" } } */

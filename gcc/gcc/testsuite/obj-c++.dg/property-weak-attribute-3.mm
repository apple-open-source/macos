/* APPLE LOCAL file radar 4621020 */
/* Test that we call objc_assign_weak and objc_read_weak */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-gc" } */

@interface INTF
{
  __weak id IVAR;
}
@property (weak, ivar=IVAR) id uses_inclass_weak;
@property (weak, ivar) id uses_default_weak;
@end

@implementation INTF
@property (weak, ivar=IVAR ) id uses_inclass_weak;
@property (weak, ivar) id uses_default_weak;
@end
/* { dg-final { scan-assembler "objc_assign_weak" } } */
/* { dg-final { scan-assembler "objc_read_weak" } } */

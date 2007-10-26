/* APPLE LOCAL file radar 5333233 */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-mmacosx-version-min=10.5 -m64" } */

@interface Super { id isa; } @end
@implementation Super @end

@interface SubNoIvars : Super 
@end

@implementation SubNoIvars @end

int main() { return 0; }
/* { dg-final { scan-assembler "L_ZL27_OBJC_CLASS_RO_\\\$_SubNoIvars:\n\t.long\t0\n\t.long\t8\n\t.long\t8" } } */

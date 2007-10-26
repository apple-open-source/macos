/* APPLE LOCAL file radar 4964338 */
/* { dg-options "-Os" } */
/* { dg-do compile } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

@interface Foo 
- (void) doit;
@end

@implementation Foo
- (void) doit { }
@end
/* { dg-final { scan-assembler ".section __OBJC, __image_info" } } */
/* { dg-final { scan-assembler "L_OBJC_IMAGE_INFO:" } } */

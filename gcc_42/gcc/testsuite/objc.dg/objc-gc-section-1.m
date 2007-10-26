/* APPLE LOCAL file radar 4810587 */
/* Check that default option also results in generation of objc section 
   with flag of 0 */

/* { dg-do compile } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

@interface INTF
@end
@implementation  INTF
@end
/* { dg-final { scan-assembler ".section __OBJC, __image_info" } } */
/* { dg-final { scan-assembler "L_OBJC_IMAGE_INFO:\n\t.space 8" } } */


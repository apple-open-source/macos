/* APPLE LOCAL file radar 4805321, 4947014 */
/* Test that setter/getter helpers are generated for 'retain' property. */
/* { dg-options "-fobjc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface NSPerson
{
  id ivar;
}
@property(retain) id firstName;
@end

@implementation NSPerson
@synthesize firstName=ivar;
@end
/* { dg-final { scan-assembler "objc_setProperty" } } */
/* { dg-final { scan-assembler "objc_getProperty" } } */

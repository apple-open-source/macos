/* APPLE LOCAL file radar 4625635 */
/* Test for a Synthesized Property to be a 'byref' property by default*/
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface NSPerson
@property(ivar) id firstName;
@end

@implementation NSPerson
@property(ivar) id firstName;
@end
/* { dg-final { scan-assembler "object_setProperty_byref" } } */
/* { dg-final { scan-assembler "object_getProperty_byref" } } */

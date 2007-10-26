/* APPLE LOCAL file radar 4436866 */
/* Check for proper declaration of @property. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Bar
{
  int iVar;
}
@property int FooBar /* { dg-warning "expected \\`@end\\' at end of input" } */

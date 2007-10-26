/* APPLE LOCAL file radar 4436866 */
/* Property name cannot match the ivar name. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Person 
{
  char *firstName;
}
@property (dynamic) char *firstName; /* { dg-error "property name \\'firstName\\' matches an ivar name in this class" } */
@end	

@implementation  Person
@end

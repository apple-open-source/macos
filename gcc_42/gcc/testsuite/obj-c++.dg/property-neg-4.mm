/* APPLE LOCAL file radar 4436866 */
/* Property cannot be accessed in class method. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Person 
{
}
@property const char *fullName;
+ (void) testClass;
@end	

@implementation  Person
@dynamic  fullName;
+ (void) testClass {
	self.fullName = "MyName"; /* { dg-error "accessing unknown" } */
}
@end


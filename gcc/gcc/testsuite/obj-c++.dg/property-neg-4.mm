/* APPLE LOCAL file radar 4436866 */
/* Property cannot be accessed in class method. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Person 
{
}
@property (ivar) char *fullName;
+ (void) testClass;
@end	

@implementation  Person
@property char *fullName;
+ (void) testClass {
	self.fullName = "MyName"; /* { dg-error "request for member \\'fullName\\' in \\'self\\'" } */
}
@end


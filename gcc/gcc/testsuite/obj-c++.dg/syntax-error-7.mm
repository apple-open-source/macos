/* APPLE LOCAL begin radar 4290840 */
/* { dg-do compile } */

@interface Foo
-(void) someMethod;
@end

@implementation Foo
-(void)
-(void) someMethod /* { dg-error "expected before .-." } */
{
}
@end
/* APPLE LOCAL end radar 4290840 */

/* APPLE LOCAL file radar 4816280 */
/* Test that gcc warns when property declared in class has no implementation declarative. */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Subclass 
{
    int ivar;
}
@property int ivar;
@end

@implementation Subclass

@end /* { dg-warning "property 'ivar' requires method '-ivar' to be defined" } */
     /* { dg-warning "property 'ivar' requires the method 'setIvar:' to be defined" "" { target *-*-* } 15 } */

int main (void) {
  return 0;
}

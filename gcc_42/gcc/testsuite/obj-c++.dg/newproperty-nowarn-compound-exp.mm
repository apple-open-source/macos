/* APPLE LOCAL file radar 4805321 */
/* Test that no bogus warning is issued in the synthesize compound-expression. */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property -Wall" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface test
{
  int i;
}
@property (assign) int foo;
@property (assign) int foo1;
@property (assign) int foo2;
@end
extern int one ();
extern int two ();

@implementation test
@synthesize foo=i,foo1=i,foo2=i;
- (void) pickWithWarning:(int)which { 
	   self.foo = (which ? 1 : 2); 
	   self.foo1 = self.foo2 = (which ? 1 : 2); 
	   self.foo = (which ? one() : two() ); 
	   self.foo1 = self.foo2 = (which ? one() : two ()); 
}
@end

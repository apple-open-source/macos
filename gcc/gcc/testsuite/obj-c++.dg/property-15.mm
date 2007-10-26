/* APPLE LOCAL file radar 4675792 */
/* Test that 'class' can be a valid getter name in a property. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile } */

@interface Base {
    Class _isa;
}
- (Class)class;
@property(readonly) Class isa;
@end

@implementation Base
@property(readonly, getter=class) Class isa;

- (Class)class {
    return self.isa;
}
@end

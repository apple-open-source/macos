/* APPLE LOCAL file radar 4805321 */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property" } */
/* { dg-do compile } */

@interface Bar 
@property (assign, setter = MySetter:) int FooBar;
- (void) MySetter : (int) value;
- (int) FooBar;
@property (assign, getter = MyGetter) int PropGetter;
- (int) MyGetter;
@property (assign) int noSetterGetterProp;
@end

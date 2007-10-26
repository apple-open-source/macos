/* APPLE LOCAL file radar 5168008 */
/* Test that a getter name in a property declaration does not conflict with 
   another class name.  Propgram should compile with no error/warning. */
/* { dg-options "-mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface WorksA {
};
@end
@interface WorksB {
  WorksA *_a;
};
- (WorksA*) WorksA;
@end


@interface A {
};
@end

@interface B {
  A *_a;
};
- (A*) A;

@property (assign, getter=A) A *_a;
@end

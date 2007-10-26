/* APPLE LOCAL file 4653319 */
/* Test for several property corner cases; 1) not having ivar in the @interface, but 
   specifying ivar=name in the @implementation is legal. 2) 'ivar' can be inherited from base.
   3) super.string is how you access an inherited property from within a property accessor method.
*/
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do run { target *-*-darwin* } } */

// define a protocol consisting of 3 properties, object, number, and string.
@protocol P
@property id object;
@property int number;
@property char *string;
@end


// define a base class that contains a fully synthesized implementation of the string property.
// this is inherited by all other implementations

@interface Base {
@protected
    Class       isa;
    id          _object;
    int         _number;
}
@property(readonly) Class MyClass;
@property(ivar) char *string;           // fully synthesized property string.
@end

@implementation Base
@property(readonly, ivar=isa) Class MyClass;
@end

// Test 1:  Implement protocol P using synthesized properties by wrapping inherited instance variables.

@interface A : Base <P>                 // <P> is satisfied by inheriting string from Base, and implementing object and number.
@property id object;                    // unspecified implementation of property object in interface.
@property(ivar=_number) int number;     // partially synthesized property number using inherited ivar _number.
@end

@implementation A
@property(ivar=_object) id object;      // partially synthesized property number using inherited ivar _object.
@end					/* { dg-warning "no synthesized or user getter" } */
					/* { dg-warning "no synthesized or user setter" "" { target *-*-* } 44 } */

@interface B : Base {
}
@end

// Test 2:  implement protocol P with a category.

@implementation B
@end

@interface B (Properties) <P>
@property id object;                    // unspecified implementation of property object in interface.
@property(ivar=_number) int number;     // partially synthesized property number using inherited ivar _number.
@end

@implementation B (Properties)
@property(ivar=_object) id object;      // partially synthesized property number using inherited ivar _object.
@end

// Test 3:  implement protocol P with explicit accessor methods.

@interface C : Base <P>
@property id object;
@property int number;
@end

@implementation C

- (id)_getObject { return _object; }
- (void)_setObject:(id)value { _object = value; }
- (int)gEt_NuMbEr { return _number; }
- (void)SeT_nUmBeR:(int)value { _number = value; }

- (char *)string { return super.string; };
- (void)setString:(char *)value { super.string = value; }

@property(getter=_getObject, setter=_setObject:) id object;
@property(getter=gEt_NuMbEr, setter=SeT_nUmBeR:) int number;

@end

// Test 4: Warn when no accessor is syntesized. 

@interface Foo {
    Class isa;
}
@property id bar;
@end

@implementation Foo
@end /* { dg-warning "no synthesized or user getter" } */
     /* { dg-warning "no synthesized or user setter" "" { target *-*-* } 96 } */

int main(int argc, char **argv) {
    return 0;
}

/* APPLE LOCAL file radar 5435299  - radar 6825962 */
/* A synthesize property cannot use an ivar in its super class. */
/* { dg-options "-mmacosx-version-min=10.5 -m64" { target powerpc*-*-darwin* i?86*-*-darwin* } } */
/* { dg-options "-fobjc-new-property" { target arm*-*-darwin* } } */
/* { dg-do compile { target *-*-darwin* } } */

#ifdef __OBJC2__
#import <objc/Object.h>

@interface Test6Super : Object
{
   int prop;
}
@end

@implementation Test6Super @end

@interface Test6 : Test6Super
@property int prop;
@end

@implementation Test6
@synthesize prop;  /* { dg-error "property \\'prop\\' attempting to use ivar \\'prop\\' declared in super class of \\'Test6\\'" } */
@end

#endif

@interface A {
    id _x;
}
@end

@implementation A
@end

@interface B : A
@property (retain) id x;
@end

@implementation B
@synthesize x=_x; /* { dg-error "property \\'x\\' attempting to use ivar \\'_x\\' declared in super class of \\'B\\'" } */
@end


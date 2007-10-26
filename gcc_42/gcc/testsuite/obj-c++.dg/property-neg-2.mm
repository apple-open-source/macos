/* APPLE LOCAL file radar 4436866 */
/* This program checks for proper declaration of property. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface Bar
@end

@implementation Bar
@synthesize foo; /* { dg-error "no declaration of property \\'foo\\' found in the interface" } */
@end

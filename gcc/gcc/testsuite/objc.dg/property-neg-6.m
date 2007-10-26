/* APPLE LOCAL file radar 4498373 */
/* Test for 'bycopy' attribute */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-skip-if "" { powerpc*-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>
@interface Person : Object
@property (readonly) const char *name;
@end

@implementation Person
@property(bycopy, readonly, getter=_name) const char *name; /* { dg-error "property \\'name\\' has conflicting \\'bycopy\\' attribute with its interface version" } */

- (const char*)_name {
    return "MyName";
}
@end

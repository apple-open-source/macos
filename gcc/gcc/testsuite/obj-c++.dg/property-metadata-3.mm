/* APPLE LOCAL file radar 4498373 */
/* Test for Parametrized Accessors */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>
@interface Person : Object
@property const char *name;
@end

@implementation Person
@property(getter=_name, setter=_setName:) const char *name;

- (const char*)_name {
    return "MyName";
}

- (void)_setName:(const char*)ThisName {
  self.name = ThisName;
}
@end
/* { dg-final { scan-assembler ".long\t8\n\t.long\t1\n\t.long\t.*\n\t.long\t.*" } } */
/* { dg-final { scan-assembler ".ascii \"name\\\\0\"" } } */

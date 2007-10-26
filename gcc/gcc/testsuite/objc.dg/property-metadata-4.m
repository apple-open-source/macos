/* APPLE LOCAL file radar 4498373 */
/* Test for 'bycopy' attribute */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */

#include <objc/Object.h>
@interface Person : Object
@property (bycopy, readonly) id name;
@end

@implementation Person
@property(bycopy, readonly, getter=_name) id name;

- (id)_name {
    return 0;
}
@end
/* { dg-final { scan-assembler ".long\t8\n\t.long\t1\n\t.long\t.*\n\t.long\t.*" } } */
/* { dg-final { scan-assembler ".ascii \"name\\\\0\"" } } */
/* { dg-final { scan-assembler ".ascii \"T\\@,R,C,G_name\\\\0\"" } } */

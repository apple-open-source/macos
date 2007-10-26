/* APPLE LOCAL file radar 4666559 */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -gdwarf-2 -dA" } */
/* { dg-final { scan-assembler "\"_prop\\\\0\".*DW_AT_name" } } */
@interface Foo 
{
  id isa;
}
@property (ivar) const char* prop;
@end

@implementation Foo 
@property(ivar) const char* prop;
@end

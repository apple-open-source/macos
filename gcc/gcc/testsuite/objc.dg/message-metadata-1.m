/* APPLE LOCAL file radar 4582204 */
/* Test that message_ref_t meta-data is generated for for objc and obj-c++ */
/* { dg-options "-fobjc-abi-version=2 -mmacosx-version-min=10.5" } */
/* { dg-do compile } */

@interface Foo 
+class; 
@end
int main() {
    [Foo class];
}
/* { dg-final { scan-assembler "OBJC_MESSAGE_REF.*:" } } */

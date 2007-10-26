/* APPLE LOCAL file radar 4695101 */
/* Test that @implementation <protoname> syntax generates metadata for properties 
   declared in @protocol, as well as those declared in the @interface. */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -fobjc-abi-version=2" } */

@protocol GCObject
@property(readonly) unsigned long int instanceSize;
@property(dynamic, readonly) long referenceCount;
@property(readonly) const char *description;
@end

@interface GCObject <GCObject> {
    Class       isa;
}
@property(dynamic, readonly) unsigned long int instanceSize;
@property(dynamic, readonly) const char *description;
@end

@implementation GCObject @end

/* { dg-final { scan-assembler "L_OBJC_\\\$_PROP_LIST_GCObject:" } } */
/* { dg-final { scan-assembler "L_OBJC_\\\$_PROP_PROTO_LIST_GCObject:" } } */

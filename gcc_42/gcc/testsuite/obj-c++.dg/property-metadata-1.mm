/* APPLE LOCAL file radar 4695101 */
/* Test that @implementation <protoname> syntax generates metadata for properties 
   declared in @protocol, as well as those declared in the @interface. */
/* { dg-do compile { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-mmacosx-version-min=10.5 -m64" } */

@protocol GCObject
@property(readonly) unsigned long int instanceSize;
@property(readonly) long referenceCount;
@property(readonly) const char *description;
@end

@interface GCObject <GCObject> {
    Class       isa;
}
@end

@implementation GCObject 
@dynamic instanceSize;
@dynamic description;
@dynamic referenceCount;
@end

/* { dg-final { scan-assembler "L_ZL26_OBJC_\\\$_PROP_LIST_GCObject:" } } */
/* { dg-final { scan-assembler "L_ZL32_OBJC_\\\$_PROP_PROTO_LIST_GCObject:" } } */

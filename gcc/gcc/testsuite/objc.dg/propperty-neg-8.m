/* APPLE LOCAL file radar 4558088 */
/* Test that property declared in interface and implementation have
   identical types. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

#include <objc/Object.h>

@interface GCObject {
}

@property(readonly) Class class;
@property(readonly) unsigned int instanceSize;
@property(readonly) long referenceCount;
@property(readonly) BOOL finalized;
@property(readonly) const char *description;

@end

@interface GCPerson : GCObject
@property (ivar) int* age;
@property(ivar, bycopy)  id name;
@end

@implementation GCPerson
@property int age;	/* { dg-error "property \\'age\\' has conflicting type" } */
@property(bycopy) id name;
@end


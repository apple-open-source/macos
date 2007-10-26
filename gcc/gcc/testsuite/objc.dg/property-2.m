/* APPLE LOCAL file radar 4436866, modified due to radar 4625635 */
/* This program tests use of properties . */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -framework Foundation -fobjc-exceptions" } */
/* { dg-do run { target *-*-darwin* } } */

#include <Foundation/Foundation.h>

static id
object_getProperty_byref (id self, SEL _cmd, unsigned int offset)
{
  id *slot = (id*) ((char*)self + offset);
  return *slot;
}

static void 
object_setProperty_byref (id self, SEL _cmd, id value, unsigned int offset)
{
  id *slot = (id*) ((char*)self + offset);
  id oldValue = *slot;
  if (oldValue != value)
    *slot = value;
}

@interface Person : NSObject
@property (ivar)NSString *firstName, *lastName;
@property(ivar, readonly) NSString *fullName;
@end

@interface Group : NSObject
@property (ivar) Person *techLead, *runtimeGuru, *propertiesMaven;
@end

@implementation Group
@property Person *techLead, *runtimeGuru, *propertiesMaven;
- init {
  self.techLead = [[Person alloc] init];
  self.runtimeGuru = [[Person alloc] init];
  self.propertiesMaven = [[Person alloc] init];
  return self;
}
@end

@implementation Person
@property (ivar) NSString *firstName, *lastName;
@property(readonly, ivar) NSString *fullName;
- (NSString*)fullName { // computed getter
    return [NSString stringWithFormat:@"%@ %@", self.firstName, self.lastName];
}
@end

NSString *playWithProperties()
{
  Group *g = [[Group alloc] init] ;

  g.techLead.firstName = @"Blaine";
  g.techLead.lastName = @"Garst";
  g.runtimeGuru.firstName = @"Greg";
  g.runtimeGuru.lastName = @"Parker";
  g.propertiesMaven.firstName = @"Patrick";
  g.propertiesMaven.lastName = @"Beard";

  return [NSString stringWithFormat:@"techlead %@ runtimeGuru %@ propertiesMaven %@",
                        g.techLead.fullName, g.runtimeGuru.fullName, g.propertiesMaven.fullName];
}

main()
{
    char buf [256];
    NSAutoreleasePool* pool  = [[NSAutoreleasePool alloc] init];
#   if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5 || __OBJC2__)
    sprintf(buf, "%s", [playWithProperties() UTF8String]);
#else
    sprintf(buf, "%s", [playWithProperties() cString]);
#endif
    [pool release];
    return strcmp (buf, "techlead Blaine Garst runtimeGuru Greg Parker propertiesMaven Patrick Beard");
}


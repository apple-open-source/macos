/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "RBClassUtils.h"
#import <Foundation/Foundation.h>

#import <objc/objc.h>
#import <objc/objc-class.h>
#import <objc/objc-runtime.h>
#import "objc_compat.h"

#import "RBObject.h"
#import "OverrideMixin.h"
#import "ocdata_conv.h"

// XXX: the NSMutableDictionary-based hashing methods should be rewritten
// to use st_table, which is 1) faster and 2) independent from ObjC (no need
// to create autorelease pools etc...).

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4

Class objc_class_alloc(const char* name, Class super_class)
{
  Class klass = objc_getClass(name);
  if (klass != NULL) {
    rb_warn("Cannot create Objective-C class for Ruby class `%s', because another class is already registered in Objective-C with the same name. Using the existing class instead for the Ruby class representation.", name);
    return klass;
  }
  return objc_allocateClassPair(super_class, name, 0);
}

#else

static void* alloc_from_default_zone(unsigned int size)
{
  return NSZoneMalloc(NSDefaultMallocZone(), size);
}

static struct objc_method_list** method_list_alloc(int cnt)
{
  int i;
  struct objc_method_list** mlp;
  mlp = alloc_from_default_zone(cnt * sizeof(void*));
  for (i = 0; i < (cnt-1); i++)
    mlp[i] = NULL;
  mlp[cnt-1] = (struct objc_method_list*)-1; // END_OF_METHODS_LIST
  return mlp;
}

Class objc_class_alloc(const char* name, Class super_class)
{
  Class c = alloc_from_default_zone(sizeof(struct objc_class));
  Class isa = alloc_from_default_zone(sizeof(struct objc_class));
  struct objc_method_list **mlp0, **mlp1;
  mlp0 = method_list_alloc(16);
  mlp1 = method_list_alloc(4);

  c->isa = isa;
  c->super_class = super_class;
  c->name = strdup(name);
  c->version = 0;
  c->info = CLS_CLASS + CLS_METHOD_ARRAY;
  c->instance_size = super_class->instance_size;
  c->ivars = NULL;
  c->methodLists = mlp0;
  c->cache = NULL;
  c->protocols = NULL;

  isa->isa = super_class->isa->isa;
  isa->super_class = super_class->isa;
  isa->name = c->name;
  isa->version = 5;
  isa->info = CLS_META + CLS_INITIALIZED + CLS_METHOD_ARRAY;
  isa->instance_size = super_class->isa->instance_size;
  isa->ivars = NULL;
  isa->methodLists = mlp1;
  isa->cache = NULL;
  isa->protocols = NULL;
  return c;
}
#endif

/**
 * Dictionary for Ruby class (key by name)
 **/
static NSMutableDictionary* class_dic_by_name()
{
  static NSMutableDictionary* dic = nil;
  if (!dic) dic = [[NSMutableDictionary alloc] init];
  return dic;
}

/**
 * Dictionary for Ruby class (key by value)
 **/
static NSMutableDictionary* class_dic_by_value()
{
  static NSMutableDictionary* dic = nil;
  if (!dic) dic = [[NSMutableDictionary alloc] init];
  return dic;
}

static NSMutableDictionary* derived_class_dic()
{
  static NSMutableDictionary* dic = nil;
  if (!dic) dic = [[NSMutableDictionary alloc] init];
  return dic;
}

@interface RBClassMapInfo : NSObject {
  NSString* kls_name;
  NSNumber* kls_value;
}
- (id)initWithName:(const char*)name value:(VALUE) kls;
- (NSString*) name;
- (NSNumber*) value;
@end

@implementation RBClassMapInfo
- (id)initWithName:(const char*)name value:(VALUE) kls {
  self = [self init];
  if (self) {
    kls_name = [[NSString alloc] initWithUTF8String: name];
    kls_value = [[NSNumber alloc] initWithUnsignedLong: kls];
  }
  return self;
}
- (void) dealloc {
  [kls_name release];
  [kls_value release];
  [super dealloc];
}
- (NSString*) name  { return kls_name;  }
- (NSNumber*) value { return kls_value; }
@end

/**
 * add class map entry to dictionaries.
 **/
static void class_map_dic_add (const char* name, VALUE kls)
{
  RBClassMapInfo* info =
    [[RBClassMapInfo alloc] initWithName:name value:kls];
  [class_dic_by_name()  setObject:info forKey: [info name]];
  [class_dic_by_value() setObject:info forKey: [info value]];
  [info release];
}

Class RBObjcClassFromRubyClass (VALUE kls)
{
  id pool;
  NSNumber* kls_value;
  RBClassMapInfo* info;
  Class result = nil;

  pool = [[NSAutoreleasePool alloc] init];

  kls_value = [NSNumber numberWithUnsignedLong: kls];
  info = [class_dic_by_value() objectForKey: kls_value];
  result = NSClassFromString ([info name]);
  [pool release];
  return result;
}

VALUE RBRubyClassFromObjcClass (Class cls)
{
  id pool;
  RBClassMapInfo* info;
  NSString* kls_name;
  VALUE result = Qnil;

  pool = [[NSAutoreleasePool alloc] init];

  kls_name = NSStringFromClass(cls);
  info = [class_dic_by_name() objectForKey: kls_name];
  result = [[info value] unsignedLongValue];
  [pool release];
  return result;
}

Class RBObjcClassNew(VALUE kls, const char* name, Class super_class)
{
  Class c;

  c = objc_class_alloc(name, super_class);
  objc_registerClassPair(c);
  class_map_dic_add (name, kls);
  return c;
}

BOOL is_objc_derived_class(VALUE kls)
{
  id pool;
  BOOL ok;
 
  pool = [[NSAutoreleasePool alloc] init];
  ok = [derived_class_dic() objectForKey:[NSNumber numberWithUnsignedLong:kls]] != nil;
  [pool release];
  return ok;
}

void derived_class_dic_add(VALUE kls)
{
  id pool;
 
  pool = [[NSAutoreleasePool alloc] init];
  [derived_class_dic() setObject:[NSNumber numberWithBool:YES] forKey:[NSNumber numberWithUnsignedLong:kls]];
  [pool release];
}

Class RBObjcDerivedClassNew(VALUE kls, const char* name, Class super_class)
{
  Class c;

  c = objc_class_alloc(name, super_class);

  // init instance variable
  install_ovmix_ivars(c);

  // init instance methods
  install_ovmix_methods(c);

  // init class methods
  install_ovmix_class_methods(c);
  
  // add class to runtime system
  objc_registerClassPair(c);
  class_map_dic_add(name, kls);
  derived_class_dic_add(kls);

  // init hooks
  install_ovmix_hooks(c);

  return c;
}


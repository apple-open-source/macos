#import <Foundation/Foundation.h>
#import "libivars.h"

/* The following stolen from gdb.obj/myclass.m, written by Adam Fedor */

@implementation MyClass
+ newWithArg: arg
{
  id obj = [self new];
  [obj takeArg: arg];
  return obj;
}

- takeArg: arg
{
  object = arg;
  [object retain];
  _object2 = arg;
  [_object2 retain];
  return self;
}

- randomFunc
{
  puts ("hi");  /* Whatever, just a place to break and examine SELF in gdb */
}

@end

id return_an_object (void) {
  return [[MyClass new] init];
}

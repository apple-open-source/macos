#import <Foundation/Foundation.h>
#import "class.h"

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
  return self;
}

- sayHello
{
  puts ("hi v4");  /* Whatever, just a place to break and examine SELF in gdb */
}

- showArg
{
  puts ("showArg v4 called");
  return object;
}

@end


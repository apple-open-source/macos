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
  puts ("hi v3");  /* Whatever, just a place to break and examine SELF in gdb */
  id str = [NSString stringWithCString:"hi there"]; /* A new class/selector ref*/
}

- showArg
{
  puts ("showArg v3 called");
  return object;
}

@end


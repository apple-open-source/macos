#include <objc/Object.h>

@interface MyClass: Object
{
  id object;
}
+ newWithArg: arg;
- doIt;
- takeArg: arg;
@end

@interface MyClass (Private)
- hiddenMethod;
@end

@implementation MyClass
+ newWithArg: arg
{
  id obj = [self new];
  [obj takeArg: arg];
  return obj;
}

- doIt
{
  return self;
}

- takeArg: arg
{
  object = arg;
  [self hiddenMethod];
  return self;
}
@end

@implementation MyClass (Private)
- hiddenMethod
{
  return self;
}
@end

int main (int argc, const char *argv[])
{
  id obj;
  obj = [MyClass new];
  [obj takeArg: obj];
  return 0;
}

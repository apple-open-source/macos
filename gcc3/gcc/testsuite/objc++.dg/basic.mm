// APPLE LOCAL file Objective-C++
// A basic sanity check for Objective-C++.
// { dg-do run }

#include <objc/objc.h>
#include <objc/Object.h>

#include <iostream>

@interface Greeter : Object
- (void) greet;
@end

@implementation Greeter
- (void) greet { printf ("Hello from Objective-C\n"); }
@end

int
main ()
{
  std::cout << "Hello from C++\n";
  Greeter *obj = [Greeter new];
  [obj greet];
}

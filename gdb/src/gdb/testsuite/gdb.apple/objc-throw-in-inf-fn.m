#import <Foundation/Foundation.h>
#include <stdio.h>

@interface throw_me : NSObject {
  int myValue;
}
- (id) initWithValue: (int) inValue;
- (int) report;
- (void) throwMe;
@end

@implementation throw_me
- (id) initWithValue: (int) value
{
  myValue = value;
  return self;
}

- (int) report
{
  printf ("My value is %d\n", myValue);
  return myValue;
}

- (void) throwMe
{
  @throw self;
}
@end

int main (int argc, const char * argv[]) {
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
  throw_me *myException;
  myException = [[throw_me alloc] initWithValue:5];

  @try
    {
      [myException throwMe]; /* Breakpoint after making myException */
    }
  @catch (throw_me *exc)
    {
      [exc report]; /* This is the catch clause */
    }
  
  [pool release];
  return 0;
}

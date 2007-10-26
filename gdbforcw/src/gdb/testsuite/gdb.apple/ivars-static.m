#import <Foundation/Foundation.h>

@interface MyClass: NSObject
{
  /* secret! */
}
+ newWithArg: arg;
- takeArg: arg;
- randomFunc;
@end


int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    id object = [MyClass newWithArg:@"hi there"];
    [object randomFunc];
    
    [pool release];
    return 0;
}

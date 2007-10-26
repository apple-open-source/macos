/* APPLE LOCAL file 4854605 */
/* Check that iterator of an empty or nil collection is set to nil
   after foreach statement. */
/* { dg-options "-mmacosx-version-min=10.5 -framework Foundation" } */
/* { dg-do run } */

#include <Foundation/Foundation.h>
extern void abort (void);

int main (int argc, char const* argv[]) {
    NSString * foo;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    /* empty collection */
    NSArray * arr = [NSArray arrayWithObjects:nil, nil];
    int count = 0;
    for (foo in arr) { 
      count++;
      NSLog(@"foo is %@", foo);
    }
    if (foo != nil || count)
      abort ();

    /* nil collection */
    arr = nil;
    count = 0;
    for (foo in arr) { 
      count++;
      NSLog(@"foo is %@", foo);
    }
    if (foo != nil || count)
      abort ();
    [pool release];
    return 0;
}

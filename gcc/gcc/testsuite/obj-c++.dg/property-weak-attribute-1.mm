/* APPLE LOCAL file radar 4621020 */
/* Test that we call objc_assign_weak and objc_read_weak */
/* { dg-do run { target *-*-darwin* } } */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5 -framework Foundation" } */

#include <Foundation/Foundation.h>

@interface Foo : NSObject 
@property (ivar, weak) id delegate;
@end

@implementation Foo
@end

main () {
  [NSAutoreleasePool new];

  Foo *foo = [Foo new];

  id obj = [NSObject new];

  if ([obj retainCount] != 1)
    abort();

  [foo setDelegate:obj];

  if ([obj retainCount] != 1)
    abort();

  return 0;
}


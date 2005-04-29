/* APPLE LOCAL file test suite */
/* Check that sending messages to variables of type 'Class' does not involve instance methods.  */
/* Author: Ziemowit Laski <zlaski@apple.com>  */
/* { dg-do compile } */

#include <objc/Protocol.h>

@interface Base
- (unsigned)port;
- (id)starboard;
@end

@interface Derived: Base
- (Object *)port;
+ (Protocol *)port;
@end

id foo(void) {
  Class receiver;
  id p = [receiver port];  /* there should be no warnings here! */
  p = [receiver starboard];  /* { dg-warning ".Class. may not respond to .\\+starboard." } */
  p = [Class port];  /* { dg-error ".Class. is not an Objective\\-C class name or alias" } */
  return p;
}

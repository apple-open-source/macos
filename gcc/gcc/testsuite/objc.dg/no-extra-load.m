/* APPLE LOCAL 3926484 */
/* { dg-do compile } */
#import <Foundation/Foundation.h>
main() { [NSObject new]; }
/* { dg-final { scan-assembler-not "L_objc_msgSend\\\$non_lazy_ptr" } } */

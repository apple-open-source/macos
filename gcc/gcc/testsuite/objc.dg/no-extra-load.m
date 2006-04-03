/* APPLE LOCAL file mainline */
/* { dg-do compile { target *-*-darwin* } } */

#import <Foundation/Foundation.h>
main() { [NSObject new]; }

/* { dg-final { scan-assembler-not "L_objc_msgSend\\\$non_lazy_ptr" } } */

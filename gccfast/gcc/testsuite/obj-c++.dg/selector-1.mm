/* APPLE LOCAL file Objective-C++ */
/* Test whether including C++ keywords such as 'and', 'or',
   'not', etc., is allowed inside ObjC selectors.  */
/* Author: Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do compile } */


@interface Int1 
+ (int)and_eq:(int)arg1 and:(int)arg2;
- (int)or_eq:(int)arg1 or:(int)arg3;
- (int)not:(int)arg1 xor:(int)arg2;
- (void)bitand:(char)c1 bitor:(char)c2;
- (void)compl:(float)f1 xor_eq:(double)d1;
- (void)not_eq;
@end

@implementation Int1
+ (int)and_eq:(int)arg1 and:(int)arg2 { return arg1 + arg2; }
- (int)or_eq:(int)arg1 or:(int)arg3 { return arg1 + arg3; }
- (int)not:(int)arg1 xor:(int)arg2 { return arg1 + arg2; }
- (void)bitand:(char)c1 bitor:(char)c2 { }
- (void)compl:(float)f1 xor_eq:(double)d1 { }
- (void)not_eq { }
@end

/* { dg-final { scan-assembler  "\\+\\\[Int1 and_eq:and:\\]" } } */
/* { dg-final { scan-assembler  "\\-\\\[Int1 or_eq:or:\\]" } } */
/* { dg-final { scan-assembler  "\\-\\\[Int1 not:xor:\\]" } } */
/* { dg-final { scan-assembler  "\\-\\\[Int1 bitand:bitor:\\]" } } */
/* { dg-final { scan-assembler  "\\-\\\[Int1 compl:xor_eq:\\]" } } */
/* { dg-final { scan-assembler  "\\-\\\[Int1 not_eq\\]" } } */


/* APPLE LOCAL file radar 4903391 */
/* Test that rules for 'ivar' assignment of property in a @synthesize declaration
   follows the same type conversion rule for a setter/getter. */
/* { dg-options "-mmacosx-version-min=10.5" } */
/* { dg-do compile } */

@class AClass;
@class ASubClass;

@interface NSArray
@end

@interface NSMutableArray : NSArray
@end

@interface foo  {
     NSMutableArray *_array;
     NSArray *_array1;
     ASubClass *_array3;
}
@property(readonly) NSArray *things;
@property(readonly) NSMutableArray *things1;
@property(readonly) NSMutableArray *things2;
@property(readonly) AClass *things3;
@end

@implementation foo

@synthesize things = _array;

@synthesize things1 = _array;

@synthesize things2 = _array1;  /* { dg-error "type of property \'things2\' does not match" } */

@synthesize things3 = _array3;  /* { dg-error "type of property \'things3\' does not match" } */

@end


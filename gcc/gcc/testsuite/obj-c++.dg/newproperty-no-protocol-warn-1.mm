/* APPLE LOCAL file radar 4963113 */
/* Normally, -Wno-protocol warns if methods declared in protocol not implemented
   in the class implementation. In this case, how ever, property is dynamic and
   no implementation of its setter/getter is required. */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property -Wno-protocol" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface NSObject @end

@protocol PropProt
@property(readwrite)	int		foo;
- (int) bar;
@end

@interface MyProtocolClass : NSObject <PropProt>
@end


@implementation MyProtocolClass

@dynamic foo;

- (int) bar		{ return -1; }

@end


@interface MyClass : NSObject
@property(readwrite)	int		foo;
- (int) bar;
@end


@implementation MyClass

@dynamic foo;

- (int) bar		{ return -1; }

@end


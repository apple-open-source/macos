/* APPLE LOCAL file Objective-C++ */
/* Further template tests; also, make sure that -fconstant-cfstrings does not interfere with
   ObjC message dispatch type-checking.  */
/* Author:  Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do run } */
/* { dg-options "-fconstant-cfstrings -framework Foundation -lstdc++" } */

#import <Foundation/Foundation.h>

extern void abort(void);
#define CHECK_IF(expr) if(!(expr)) abort()

template <class ARR, class TYPE> class TestT
{
public:
	TYPE k;
	int abc( ARR *array ) {
		return [array count] * k;
	}
	TestT(TYPE _k): k(_k) { }
};

template <class TYPE>
NSString *getDesc(void) {
	return [TYPE description];
}

template <class TYPE>
int abc( TYPE *xyz, NSArray *array ) {
	return [xyz count] + [array count];
}

int main(void)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	CHECK_IF(![@"NSArray" compare: getDesc<NSArray>()]);
	NSArray* a1 = [NSArray arrayWithObjects:@"One", @"Two", @"Three", nil];
	NSArray* a2 = [NSArray arrayWithObjects:@"Four", @"Five", nil];
	
	TestT<NSArray, int> t(7);
	CHECK_IF(t.abc(a1) + t.abc(a2) == 35);
        CHECK_IF(abc(a1, a2) * t.k == 35);
	[pool release];
	return 0;
}

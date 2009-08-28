/* APPLE LOCAL file radar 6212722 */
/* Test for use of array (dynamic or static) as copied in object in a block. */
/* { dg-do run { target *-*-darwin[1-2][0-9]* } } */
/* { dg-options "-mmacosx-version-min=10.6 -ObjC -framework Foundation" { target *-*-darwin* } } */
/* { dg-skip-if "" { powerpc*-*-darwin* } { "-m64" } { "" } } */

#import <Foundation/Foundation.h>
#import <Block.h>


int _getArrayCount() {return 5;}


int func ()
{
	NSAutoreleasePool *pool	= [[NSAutoreleasePool alloc] init];

	int array[5];
	
	int i;
	const int c = 5;
	for (i = 0; i < c; ++i)
	{
		array[i] = i+1;
	}
	
	void (^block)(void) = ^{
	
		int i;
		NSLog (@"c = %d", c);
		for (i = 0; i < c; ++i)
		{
			NSLog (@"array[%d] = %d", i, array[i]);
		}
	
	};
	
	block();

	[pool drain];
	return 0;
}

int main (int argc, const char *argv[])
{
        int res;
	NSAutoreleasePool *pool	= [[NSAutoreleasePool alloc] init];

	int array[_getArrayCount()];
	
	int i;
	const int c = _getArrayCount();
	for (i = 0; i < c; ++i)
	{
		array[i] = i+1;
	}
	
	void (^block)(void) = ^{
	
		int i;
		//const int c = _getArrayCount();
		NSLog (@"c = %d", c);
		for (i = 0; i < c; ++i)
		{
			NSLog (@"array[%d] = %d", i, array[i]);
		}
	
	};
	
	block();
	res = func();

	[pool drain];
	return 0 + res;
}

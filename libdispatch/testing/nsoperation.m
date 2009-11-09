#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#include "dispatch_test.h"

@interface MYOperation : NSOperation
{
}
@end

@implementation MYOperation

- (id) init
{
	self = [super init];
	return self;
}

- (void)main
{
	test_stop();
}

@end

int
main(void)
{
	test_start("NSOperation");

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSOperationQueue *queue = [[[NSOperationQueue alloc] init] autorelease];
	test_ptr_notnull("NSOperationQueue", queue);
	
	MYOperation *operation = [[MYOperation alloc] init];
	test_ptr_notnull("NSOperation", operation);
	
	[queue addOperation:operation];
	[operation release];
	
	[[NSRunLoop mainRunLoop] run];
	
	[pool release];
	
	return 0;
}

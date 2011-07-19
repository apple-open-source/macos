//
//  CCMemoryHandler.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/13/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "CCMemoryHandler.h"

@interface CCMemoryAllocation : NSObject
{
	void*		_allocation;
}

@property (readonly) void* allocation;

- (id)initWithMemSize:(size_t)size;

@end

@implementation CCMemoryAllocation

@synthesize  allocation = _allocation;

- (id)initWithMemSize:(size_t)size
{
	_allocation = NULL;
	if ((self = [super init]))
	{
		_allocation = malloc(size);
		memset(_allocation, 0, size);
	}
	return self;
}

- (void)dealloc
{
	if (_allocation != NULL)
	{
		free(_allocation);
		_allocation = NULL;
	}
	[super dealloc];
}

@end

void* CCMemoryHandler::malloc(size_t size)
{
	CCMemoryAllocation* allocObj = [[CCMemoryAllocation alloc] initWithMemSize:size];
	void* result = allocObj.allocation;
	[_memoryList addObject:allocObj];
	return result;
}
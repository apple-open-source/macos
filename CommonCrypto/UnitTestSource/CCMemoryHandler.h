//
//  CCMemoryHandler.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/13/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>

class CCMemoryHandler
{
protected:
	NSMutableArray*	_memoryList;
	
private:
	// disallow heap based instances of this class
	void* operator new (size_t size)
	{
		return ::operator new(size);
	}
	
public:
	CCMemoryHandler() :
	_memoryList(nil)
	{
		_memoryList	= [NSMutableArray new];
	}
	
	virtual ~CCMemoryHandler()
	{
		[_memoryList release];
		_memoryList = nil;
	}
	
	void* malloc(size_t size);	
};


//
//  RandomNumberService.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/12/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "RandomNumberService.h"


// The one singleton representation of the RandomNumberService
static CCRandomNumberService*	gDefaultRandomNumberService = nil;

@implementation CCRandomNumberService

/* --------------------------------------------------------------------------
	method: 		[class] defaultRandomNumberService
	returns: 		CCRandomNumberService*
	decription: 	This class method ensures a singleton instance for getting
					random data for testing
   -------------------------------------------------------------------------- */
+ (CCRandomNumberService *) defaultRandomNumberService
{
	if (nil == gDefaultRandomNumberService)
	{
		gDefaultRandomNumberService = [CCRandomNumberService new];
	}
	
	return gDefaultRandomNumberService;
}

/* --------------------------------------------------------------------------
	method: 		[class] relaseDefaultRandomNumberService
	returns: 		void
	decription: 	Releases the singleton object
   -------------------------------------------------------------------------- */
+ (void)relaseDefaultRandomNumberService
{
	[gDefaultRandomNumberService release];
	gDefaultRandomNumberService = nil;
}

/* --------------------------------------------------------------------------
	method: 		init
	returns: 		id
	decription: 	Initialize a new instance of the CCRandomNumberService 
					object.  It ensure a singleton object for this service
   -------------------------------------------------------------------------- */
- (id)init
{
	if (nil != gDefaultRandomNumberService)
	{
		// This needs to be a singleton 
		// The correct thing here would be to 
		// complain but for now 'do the right thing'
		[self release];
		self = gDefaultRandomNumberService;
	}
	else
	{
		// This is the normal path
		if ((self = [super init]))
		{
			_devRandomFileHandle = [[NSFileHandle fileHandleForReadingAtPath:@"/dev/random"] retain];
		}
	}
	return self;
}

/* --------------------------------------------------------------------------
	method: 		dealloc
	returns: 		void
	decription: 	Alway put away your toys when you are done playing with
					them.
   -------------------------------------------------------------------------- */
- (void)dealloc
{
	[_devRandomFileHandle closeFile];
	[_devRandomFileHandle release];
	[super dealloc];
}

/* --------------------------------------------------------------------------
	method: 		generateRandomNumberInRange:toMax:
	returns: 		unsigned int within the specified range
	parameters:
					min:
						The minimum value to be returned
						
					max:
						The maximum value to be returned
						
	decription: 	Returns a pesudo random number within a range.
   -------------------------------------------------------------------------- */
- (unsigned int)generateRandomNumberInRange:(unsigned int)min toMax:(unsigned int)max
{
	unsigned int result = 0L;
	if (min == max)
	{
		result = min;
	}
	else
	{
		NSAutoreleasePool *pool = [NSAutoreleasePool new];
		NSData* randomData = [_devRandomFileHandle readDataOfLength:sizeof(result)];
		unsigned int temp_i = *((unsigned int *) [randomData bytes]);
		result = (min + (temp_i % (max - min + 1)));
		[pool drain]; 
	}
	return result;
}

/* --------------------------------------------------------------------------
	method: 		generateRandomDataOfSize:toMax:
	returns: 		autorelased NSData of  pesudo random data of the specified
					size
	parameters:
					length:
						The size in bytes of the data to be created
												
	decription: 	Returns a NSData of random data
   -------------------------------------------------------------------------- */
- (NSData *)generateRandomDataOfSize:(size_t)length
{
	NSData* randomData = [_devRandomFileHandle readDataOfLength:length];
	return randomData;
}

@end

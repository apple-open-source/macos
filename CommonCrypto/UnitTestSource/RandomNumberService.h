//
//  RandomNumberService.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/12/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>


/* --------------------------------------------------------------------------
	Class: 			CCRandomNumberService
	Description: 	This class provides random number services for testing
	Note: 			This should be in another shared file so that other unit 
					test code could use this service.  For now it can remain 
					here for illustration
   -------------------------------------------------------------------------- */
@interface CCRandomNumberService : NSObject 
{
	NSFileHandle*	_devRandomFileHandle;	// file handle for reading from /dev/random
}

// Get the "default" Random number service
+ (CCRandomNumberService *)defaultRandomNumberService;

// Release the default Random number service.  NOTE: This is really 
// an unsafe method.  This should ONLY be called once.
+ (void)relaseDefaultRandomNumberService;

// generate a random integer within a set range
- (unsigned int)generateRandomNumberInRange:(unsigned int)min 
									  toMax:(unsigned int)max;

// generate a random set of bytes of an arbitrary length
- (NSData *)generateRandomDataOfSize:(size_t)length;

@end

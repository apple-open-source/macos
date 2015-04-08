//
//  DataConversion.h
//  FIPSCAVS
//
//  Created by James Murphy on 04/26/13.
//  Copyright 2013 Apple. All rights reserved.
//
// 	This is confidential and proprietary code of Apple Inc.  It may not be 
//	used, copied or modified in any way without Apple's expressed 
//	written permission in each case. No redistribution of this 
//	Apple Confidential code is allowed.
//

#import <Foundation/Foundation.h>

/*! =========================================================================
	@class NSData

	@abstract	Extend the NSData object to convert to a hex string
	========================================================================= */
@interface NSData (DataConversion)

/*! -------------------------------------------------------------------------
	@method 	toHexString

	@result		returns a NSString object with the hex characters that 
				represent the value of the NSData object.
	------------------------------------------------------------------------- */
- (NSString *)toHexString;

@end


/*! =========================================================================
	@class NSString

	@abstract	Extend the NSString object to convert hex string into a 
				binary value in a NSData object.
	========================================================================= */
@interface NSString (DataConversion)

/*! -------------------------------------------------------------------------
	@method 	hextStringToData

	@result		Convert a NSString that contains a set of hex characters 
				into a binary value.  If the conversion cannot be done then
				nil will be returned.
	------------------------------------------------------------------------- */
- (NSData *)hexStringToData;

@end



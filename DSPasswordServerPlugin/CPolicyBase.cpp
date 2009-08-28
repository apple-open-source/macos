/*
 *  CPolicyBase.cpp
 *  PasswordServerPlugin
 *
 *  Created by Administrator on Fri Nov 21 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "CPolicyBase.h"

/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <PasswordServer/CPolicyBase.h>

// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark Public Methods
#pragma mark -
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
//  CPolicyBase constructors
// ----------------------------------------------------------------------------------------

CPolicyBase::CPolicyBase()
{
	mPolicyDict = NULL;
}
CPolicyBase::CPolicyBase( CFDictionaryRef inPolicyDict )
{
	mPolicyDict = NULL;
}

CPolicyBase::CPolicyBase( const char *xmlDataStr )
{
	mPolicyDict = NULL;
}


// ----------------------------------------------------------------------------------------
//  CPolicyBase destructor
// ----------------------------------------------------------------------------------------

CPolicyBase::~CPolicyBase()
{
	if ( mPolicyDict != NULL )
	{
		CFRelease( mPolicyDict );
		mPolicyDict = NULL;
	}
}


// ----------------------------------------------------------------------------------------
//  GetPolicyAsXMLData
//
//  Returns: a malloc'd copy of the current policy in XML form. Caller must free.
// ----------------------------------------------------------------------------------------

char *
CPolicyBase::GetPolicyAsXMLData( void )
{
	CFDataRef xmlData = NULL;
	const UInt8 *sourcePtr;
	char *returnString = NULL;
	long length;
	
	if ( mPolicyDict == NULL )
		return NULL;
		
	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)mPolicyDict );
	if ( xmlData == NULL )
		return NULL;
	
	sourcePtr = CFDataGetBytePtr( xmlData );
	length = CFDataGetLength( xmlData );
	if ( sourcePtr != NULL && length > 0 )
	{
		returnString = (char *) malloc( length + 1 );
		if ( returnString != NULL )
		{
			memcpy( returnString, sourcePtr, length );
			returnString[length] = '\0';
		}
	}
	
	CFRelease( xmlData );
	
	return returnString;
}


// ----------------------------------------------------------------------------------------
//	* ConvertCFDateToBSDTime
//
//  Utility function to convert between time storage schemes.
// ----------------------------------------------------------------------------------------

bool pwsf_ConvertCFDateToBSDTime( CFDateRef inDateRef, struct tm *outBSDDate )
{
	CFGregorianDate gregorianDate;
	CFAbsoluteTime theCFDate = 0;
	
	if ( outBSDDate == NULL || inDateRef == NULL )
		return false;
		
	theCFDate = CFDateGetAbsoluteTime( inDateRef );
	gregorianDate = CFAbsoluteTimeGetGregorianDate( theCFDate, NULL );
	
	outBSDDate->tm_sec = (int)gregorianDate.second;
	outBSDDate->tm_min = gregorianDate.minute;
	outBSDDate->tm_hour = gregorianDate.hour;
	outBSDDate->tm_mday = gregorianDate.day;
	outBSDDate->tm_mon = gregorianDate.month - 1;
	outBSDDate->tm_year = gregorianDate.year - 1900;
	outBSDDate->tm_wday = 0;
	outBSDDate->tm_yday = 0;
	outBSDDate->tm_isdst = 0;
	outBSDDate->tm_gmtoff = 0;
	outBSDDate->tm_zone = NULL;
	
	return true;
}

bool pwsf_ConvertCFDateToBSDTimeStructCopy( CFDateRef inDateRef, BSDTimeStructCopy *outBSDDate )
{
    struct tm bsdTimeStruct;

    if ( !pwsf_ConvertCFDateToBSDTime( inDateRef, &bsdTimeStruct ) )
        return false;

    StructTM2BSDTimeStructCopy( &bsdTimeStruct, outBSDDate );
    return true;
}


bool
CPolicyBase::ConvertCFDateToBSDTime( CFDateRef inDateRef, struct tm *outBSDDate )
{
	return pwsf_ConvertCFDateToBSDTime( inDateRef, outBSDDate );
}

bool
CPolicyBase::ConvertCFDateToBSDTime( CFDateRef inDateRef, BSDTimeStructCopy *outBSDDate )
{
	return pwsf_ConvertCFDateToBSDTimeStructCopy( inDateRef, outBSDDate );
}


// ----------------------------------------------------------------------------------------
//	* ConvertBSDTimeToCFDate
//
//  Utility function to convert between time storage schemes.
// ----------------------------------------------------------------------------------------

bool pwsf_ConvertBSDTimeToCFDate( struct tm *inBSDDate, CFDateRef *outDateRef )
{
	CFGregorianDate gregorianDate;
	
	if ( inBSDDate == NULL || outDateRef == NULL )
		return false;
	
	gregorianDate.second = inBSDDate->tm_sec;
	gregorianDate.minute = inBSDDate->tm_min;
	gregorianDate.hour = inBSDDate->tm_hour;
	gregorianDate.day = inBSDDate->tm_mday;
	gregorianDate.month = inBSDDate->tm_mon + 1;
	gregorianDate.year = inBSDDate->tm_year + 1900;
	
	*outDateRef = CFDateCreate( kCFAllocatorDefault, CFGregorianDateGetAbsoluteTime(gregorianDate, NULL) );
	
	return true;
}

bool pwsf_ConvertBSDTimeStructCopyToCFDate( BSDTimeStructCopy *inBSDDate, CFDateRef *outDateRef )
{
    struct tm bsdTimeStruct;
    
    if ( inBSDDate == NULL )
        return false;
    
    BSDTimeStructCopy2StructTM( inBSDDate, &bsdTimeStruct );
    return pwsf_ConvertBSDTimeToCFDate( &bsdTimeStruct, outDateRef );
}


bool
CPolicyBase::ConvertBSDTimeToCFDate( struct tm *inBSDDate, CFDateRef *outDateRef )
{
	return pwsf_ConvertBSDTimeToCFDate( inBSDDate, outDateRef );
}

bool
CPolicyBase::ConvertBSDTimeToCFDate( BSDTimeStructCopy *inBSDDate, CFDateRef *outDateRef )
{
	return pwsf_ConvertBSDTimeStructCopyToCFDate( inBSDDate, outDateRef );
}

// ----------------------------------------------------------------------------------------
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
// ----------------------------------------------------------------------------------------


// ----------------------------------------------------------------------------------------
//  GetBooleanForKey
//
//  Returns: TRUE if a boolean was retrieved successfully
// ----------------------------------------------------------------------------------------

bool
CPolicyBase::GetBooleanForKey( CFStringRef inKey, bool *outValue )
{
	CFTypeRef valueRef = NULL;
	bool result = false;
	int anIntValue = 0;
	
	if ( CFDictionaryGetValueIfPresent( mPolicyDict, inKey, (const void **)&valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFBooleanGetTypeID() )
		{
			*outValue = CFBooleanGetValue( (CFBooleanRef)valueRef );
			result = true;
		}
		else
		if ( CFGetTypeID(valueRef) == CFNumberGetTypeID() )
		{
			if ( CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberIntType, &anIntValue) )
			{
				*outValue = (anIntValue != 0);
				result = true;
			}
		}
	}
	
	return result;
}




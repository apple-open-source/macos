/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CAliases
 */

#include <stdlib.h>		// for malloc

#include "CAliases.h"
#include "DSUtils.h"

// --------------------------------------------------------------------------------
//	* CAliases ()
// --------------------------------------------------------------------------------

CAliases::CAliases ( void )
{
	fDataRef	= nil;
	fPlistRef	= nil;
	fDictRef	= nil;
} // CAliases


// --------------------------------------------------------------------------------
//	* ~CAliases ()
// --------------------------------------------------------------------------------

CAliases::~CAliases ( void )
{
} // ~CAliases


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CAliases::Initialize ( void *inXMLData, uInt32 inXMLDataLen )
{
	sInt32		siResult = errInvalidXMLData;

	fDataRef = ::CFDataCreate( nil, (const UInt8 *)inXMLData, inXMLDataLen );
	if ( fDataRef != nil )
	{
		fPlistRef = ::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, fDataRef, kCFPropertyListImmutable, nil );
		if ( fPlistRef != nil )
		{
			if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( fPlistRef ) )
			{
				fDictRef = (CFDictionaryRef)fPlistRef;

				siResult = errNoError;
			}
		}
	}

	return( siResult );

} // ~CAliases


// --------------------------------------------------------------------------------
//	* GetRecordID ()
// --------------------------------------------------------------------------------

sInt32 CAliases::GetRecordID ( char **outRecID )
{
	sInt32			siResult	= errItemNotFound;
	bool			bFound		= false;
	CFStringRef		cfStringRef	= nil;

	bFound = ::CFDictionaryContainsKey( fDictRef, CFSTR( kRecordID ) );
	if ( bFound == true )
	{
		siResult = errInvalidDataType;

		cfStringRef = (CFStringRef)::CFDictionaryGetValue( fDictRef, CFSTR( kRecordID ) );
		if ( cfStringRef != nil )
		{
			if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
			{
				siResult = errItemNotFound;

				*outRecID = (char *)::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( *outRecID != nil )
				{
					siResult = errNoError;
				}
			}
		}
	}

	return( siResult );

} // GetRecordID


// --------------------------------------------------------------------------------
//	* GetRecordName ()
// --------------------------------------------------------------------------------

sInt32 CAliases::GetRecordName ( tDataList *outDataList )
{
	sInt32			siResult	= errItemNotFound;
	sInt32			i			= 0;
	bool			bFound		= false;
	CFArrayRef		cfArrayRef	= nil;
	CFIndex			cfCount		= 0;
	CFStringRef		cfStringRef	= nil;
	const char	   *cpCStr		= nil;

	bFound = ::CFDictionaryContainsKey( fDictRef, CFSTR( kRecordName ) );
	if ( bFound == true )
	{
		siResult = errInvalidDataType;

		cfArrayRef = (CFArrayRef)::CFDictionaryGetValue( fDictRef, CFSTR( kRecordName ) );
		if ( ::CFGetTypeID( cfArrayRef ) == ::CFArrayGetTypeID() )
		{
			siResult = errEmptyArray;
			
			cfCount = ::CFArrayGetCount( cfArrayRef );
			if ( cfCount != 0 )
			{
				siResult = errInvalidDataType;

				for ( i = 0; i < cfCount; i++ )
				{
					cfStringRef = (CFStringRef)::CFArrayGetValueAtIndex( cfArrayRef, i );
					if ( cfStringRef != nil )
					{
						siResult = errItemNotFound;

						if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
						{
							cpCStr = ::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
							if ( cpCStr != nil )
							{
								::dsAppendStringToListPriv( outDataList, cpCStr );
								siResult = eDSNoErr;
							}
						}
					}
				}
			}
		}
	}

	return( siResult );

} // GetRecordName


// --------------------------------------------------------------------------------
//	* GetRecordType ()
// --------------------------------------------------------------------------------

sInt32 CAliases::GetRecordType ( char **outRecType )
{
	sInt32			siResult	= errItemNotFound;
	bool			bFound		= false;
	CFStringRef		cfStringRef	= nil;

	bFound = ::CFDictionaryContainsKey( fDictRef, CFSTR( kRecordType ) );
	if ( bFound == true )
	{
		siResult = errInvalidDataType;

		cfStringRef = (CFStringRef)::CFDictionaryGetValue( fDictRef, CFSTR( kRecordType ) );
		if ( cfStringRef != nil )
		{
			siResult = errItemNotFound;

			if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
			{
				*outRecType = (char *)::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( *outRecType != nil )
				{
					siResult = errNoError;
				}
			}
		}
	}

	return( siResult );

} // GetRecordType


// --------------------------------------------------------------------------------
//	* GetRecordLocation ()
// --------------------------------------------------------------------------------

sInt32 CAliases::GetRecordLocation ( tDataList *outDataList )
{
	sInt32			siResult	= errItemNotFound;
	sInt32			i			= 0;
	bool			bFound		= false;
	CFArrayRef		cfArrayRef	= nil;
	CFIndex			cfCount		= 0;
	CFStringRef		cfStringRef	= nil;
	const char	   *cpCStr		= nil;

	bFound = ::CFDictionaryContainsKey( fDictRef, CFSTR( kRecordLocation ) );
	if ( bFound == true )
	{
		siResult = errInvalidDataType;

		cfArrayRef = (CFArrayRef)::CFDictionaryGetValue( fDictRef, CFSTR( kRecordLocation ) );
		if ( ::CFGetTypeID( cfArrayRef ) == ::CFArrayGetTypeID() )
		{
			siResult = errEmptyArray;
			
			cfCount = ::CFArrayGetCount( cfArrayRef );
			if ( cfCount != 0 )
			{
				siResult = errInvalidDataType;

				for ( i = 0; i < cfCount; i++ )
				{
					cfStringRef = (CFStringRef)::CFArrayGetValueAtIndex( cfArrayRef, i );
					if ( cfStringRef != nil )
					{
						siResult = errItemNotFound;

						if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
						{
							cpCStr = ::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
							if ( cpCStr != nil )
							{
								::dsAppendStringToListPriv( outDataList, cpCStr );
								siResult = eDSNoErr;
							}
						}
					}
				}
			}
		}
	}

	return( siResult );

} // GetRecordLocation


// --------------------------------------------------------------------------------
//	* GetAliasVersion ()
// --------------------------------------------------------------------------------

sInt32 CAliases::GetAliasVersion ( char **outAliasVersion )
{
	sInt32			siResult	= errItemNotFound;
	bool			bFound		= false;
	CFStringRef		cfStringRef	= nil;

	bFound = ::CFDictionaryContainsKey( fDictRef, CFSTR( kAliasVersion ) );
	if ( bFound == true )
	{
		siResult = errInvalidDataType;

		cfStringRef = (CFStringRef)::CFDictionaryGetValue( fDictRef, CFSTR( kAliasVersion ) );
		if ( cfStringRef != nil )
		{
			siResult = errItemNotFound;

			if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
			{
				*outAliasVersion = (char *)::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( *outAliasVersion != nil )
				{
					siResult = errNoError;
				}
			}
		}
	}

	return( siResult );

} // GetAliasVersion



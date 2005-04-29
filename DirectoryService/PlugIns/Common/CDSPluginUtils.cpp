/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CDSPluginUtils
 */


#include "CDSPluginUtils.h"
#include "DSUtils.h"
#include <CoreFoundation/CoreFoundation.h>

//--------------------------------------------------------------------------------------------------
// * PWOpenDirNode ()
//
//--------------------------------------------------------------------------------------------------

sInt32 PWOpenDirNode( tDirNodeReference fDSRef, char *inNodeName, tDirNodeReference *outNodeRef )
{
	sInt32			error		= eDSNoErr;
	sInt32			error2		= eDSNoErr;
	tDataList	   *pDataList	= nil;

	pDataList = ::dsBuildFromPathPriv( inNodeName, "/" );
    if ( pDataList != nil )
    {
        error = ::dsOpenDirNode( fDSRef, pDataList, outNodeRef );
        error2 = ::dsDataListDeallocatePriv( pDataList );
        free( pDataList );
    }

    return( error );

} // PWOpenDirNode


// ---------------------------------------------------------------------------
//	* DoesThisMatch
// ---------------------------------------------------------------------------

bool DoesThisMatch (			const char		   *inString,
								const char		   *inPatt,
								tDirPatternMatch	inHow )
{
	bool		bOutResult	= false;
	CFMutableStringRef	strRef	= CFStringCreateMutable(NULL, 0);
	CFMutableStringRef	patRef	= CFStringCreateMutable(NULL, 0);
	CFRange		range;

	if ( (inString == nil) || (inPatt == nil) || (strRef == nil) || (patRef == nil) )
	{
		return( false );
	}

	CFStringAppendCString( strRef, inString, kCFStringEncodingUTF8 );
	CFStringAppendCString( patRef, inPatt, kCFStringEncodingUTF8 );	
	if ( (inHow >= eDSiExact) && (inHow <= eDSiRegularExpression) )
	{
		CFStringUppercase( strRef, NULL );
		CFStringUppercase( patRef, NULL );
	}

	switch ( inHow )
	{
		case eDSExact:
		case eDSiExact:
		{
			if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareEqualTo )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSStartsWith:
		case eDSiStartsWith:
		{
			if ( CFStringHasPrefix( strRef, patRef ) )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSEndsWith:
		case eDSiEndsWith:
		{
			if ( CFStringHasSuffix( strRef, patRef ) )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSContains:
		case eDSiContains:
		{
			range = CFStringFind( strRef, patRef, 0 );
			if ( range.location != kCFNotFound )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSLessThan:
		case eDSiLessThan:
		{
			if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareLessThan )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSGreaterThan:
		case eDSiGreaterThan:
		{
			if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareGreaterThan )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSLessEqual:
		case eDSiLessEqual:
		{
			if ( CFStringCompare( strRef, patRef, 0 ) != kCFCompareGreaterThan )
			{
				bOutResult = true;
			}
		}
		break;

		case eDSGreaterEqual:
		case eDSiGreaterEqual:
		{
			if ( CFStringCompare( strRef, patRef, 0 ) != kCFCompareLessThan )
			{
				bOutResult = true;
			}
		}
		break;

		default:
			break;
	}

	CFRelease( strRef );
	strRef = nil;
	CFRelease( patRef );
	patRef = nil;

	return( bOutResult );

} // DoesThisMatch




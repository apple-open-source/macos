/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * @header DSLDAPUtils
 */

#include <lber.h>
#include <ldap.h>
#include <ldap_private.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>

#include <Security/Authorization.h>

#include "PrivateTypes.h"
#include "DSLDAPUtils.h"
#include "GetMACAddress.h"

extern uint32_t gSystemGoingToSleep;

// ---------------------------------------------------------------------------
//	* GetXMLFromBuffer
// ---------------------------------------------------------------------------

CFMutableDictionaryRef GetXMLFromBuffer( tDataBufferPtr inBuffer )
{
	SInt32					iBufLen			= inBuffer->fBufferLength;
	CFMutableDictionaryRef  cfXMLDict		= NULL;
	SInt32					iXMLDataLength	= 0;
	CFDataRef				cfXMLData		= NULL;
	
	// we always get an XML blob, so let's parse the blob so we can do something with it
	iXMLDataLength = iBufLen - sizeof( AuthorizationExternalForm );
	if ( iXMLDataLength > 0 )
	{
		cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(inBuffer->fBufferData + sizeof(AuthorizationExternalForm)), iXMLDataLength );
		if ( cfXMLData != NULL )
		{
			// extract the config dictionary from the XML data.
			cfXMLDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
			
			CFRelease( cfXMLData ); // let's release it, we're done with it
			cfXMLData = NULL;
		}
	}
	
	return cfXMLDict;
} // GetXMLFromBuffer


// ---------------------------------------------------------------------------
//	* PutXMLInBuffer
// ---------------------------------------------------------------------------

SInt32 PutXMLInBuffer( CFDictionaryRef inXMLDict, tDataBufferPtr outBuffer )
{
	CFDataRef   cfReturnData	= (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, inXMLDict );
	SInt32		siResult		= eDSNoErr;
	
	if ( cfReturnData != NULL )
	{
		CFRange stRange = CFRangeMake( 0, CFDataGetLength(cfReturnData) );
		if ( outBuffer->fBufferSize < (unsigned int) stRange.length ) 
		{
			siResult = eDSBufferTooSmall;
		}
		else
		{
			CFDataGetBytes( cfReturnData, stRange, (UInt8*)(outBuffer->fBufferData) );
			outBuffer->fBufferLength = stRange.length;
		}
		CFRelease( cfReturnData );
		cfReturnData = 0;
	}
	return siResult;
} // PutXMLInBuffer

//------------------------------------------------------------------------------------
//	* BuildEscapedRDN
//------------------------------------------------------------------------------------

char *
BuildEscapedRDN( const char *inLDAPRDN )
{
	char	   *outLDAPRDN		= nil;
	UInt32		recNameLen		= 0;
	UInt32		originalIndex	= 0;
	UInt32		escapedIndex	= 0;

	if (inLDAPRDN != nil)
	{
		recNameLen = strlen(inLDAPRDN);
		outLDAPRDN = (char *)calloc(1, 2 * recNameLen + 1);
		// assume at most all characters will be escaped
		while (originalIndex < recNameLen)
		{
			switch (inLDAPRDN[originalIndex])
			{
				case '#':
				case ' ':
				case ',':
				case '+':
				case '"':
				case '\\':
				case '<':
				case '>':
				case ';':
					outLDAPRDN[escapedIndex] = '\\';
					++escapedIndex;
					//fall thru to complete these escaped cases
				default:
					outLDAPRDN[escapedIndex] = inLDAPRDN[originalIndex];
					++escapedIndex;
					break;
			}
			++originalIndex;
		}
	}
	return(outLDAPRDN);
}


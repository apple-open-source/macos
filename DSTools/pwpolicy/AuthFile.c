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
/*
 *  AuthFile.m
 */

#include "AuthFile.h"

#define kMaxPolicyStrLen		2048

// from DirServicesConst.h (our layer is below DS)
#define kDSValueAuthAuthorityShadowHash				";ShadowHash;"
#define kDSTagAuthAuthorityShadowHash				"ShadowHash"
#define kDSTagAuthAuthorityBetterHashOnly			"BetterHashOnly"
#define kHashNameListPrefix							"HASHLIST:"

static void pwsf_AppendUTF8StringToArray( const char *inUTF8Str, CFMutableArrayRef inArray )
{
    CFStringRef stringRef = CFStringCreateWithCString( kCFAllocatorDefault, inUTF8Str, kCFStringEncodingUTF8 );
    if ( stringRef != NULL ) {
        CFArrayAppendValue( inArray, stringRef );
        CFRelease( stringRef );
    }
}

// ----------------------------------------------------------------------------------------
//  pwsf_ShadowHashDataToArray
//
//	Returns: TRUE if an array is returned in <outHashTypeArray>.
// ----------------------------------------------------------------------------------------

int pwsf_ShadowHashDataToArray( const char *inAAData, CFMutableArrayRef *outHashTypeArray )
{
	CFMutableArrayRef hashTypeArray = NULL;
	char hashType[256];
	
	if ( inAAData == NULL || outHashTypeArray == NULL || *inAAData == '\0' )
		return 0;
	
	*outHashTypeArray = NULL;
	
	// get the existing list (if any)
	if ( strncmp( inAAData, kDSTagAuthAuthorityBetterHashOnly, sizeof(kDSTagAuthAuthorityBetterHashOnly)-1 ) == 0 )
	{	
		hashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( hashTypeArray == NULL )
			return 0;
		
		pwsf_AppendUTF8StringToArray( "SMB-NT", hashTypeArray );
		pwsf_AppendUTF8StringToArray( "SALTED-SHA1", hashTypeArray );
	}
	else
	if ( strncmp( inAAData, kHashNameListPrefix, sizeof(kHashNameListPrefix)-1 ) == 0 )
	{
		// comma delimited list
		const char *endPtr;
		const char *tptr = inAAData + sizeof(kHashNameListPrefix) - 1;
		
		hashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( hashTypeArray == NULL )
			return 0;
		
		if ( *tptr++ == '<' && strchr(tptr, '>') != NULL )
		{
			while ( (endPtr = strchr( tptr, ',' )) != NULL )
			{
				strlcpy( hashType, tptr, (endPtr - tptr) + 1 );
				pwsf_AppendUTF8StringToArray( hashType, hashTypeArray );
				
				tptr += (endPtr - tptr) + 1;
			}
			
			endPtr = strchr( tptr, '>' );
			if ( endPtr != NULL )
			{
				strlcpy( hashType, tptr, (endPtr - tptr) + 1 );
				pwsf_AppendUTF8StringToArray( hashType, hashTypeArray );
			}
		}
	}
	
	*outHashTypeArray = hashTypeArray;
	
	return 1;
}


// ----------------------------------------------------------------------------------------
//  pwsf_ShadowHashArrayToData
// ----------------------------------------------------------------------------------------

char *pwsf_ShadowHashArrayToData( CFArrayRef inHashTypeArray, long *outResultLen )
{
	char *aaNewData = NULL;
	char *newDataCStr = NULL;
	long len = 0;
	CFMutableStringRef newDataString = NULL;
	CFStringRef stringRef;
	CFIndex stringLen;
	
	// build the new string
	CFIndex typeCount = CFArrayGetCount( inHashTypeArray );
	if ( typeCount > 0 )
	{
		newDataString = CFStringCreateMutable( kCFAllocatorDefault, 0 );
		if ( newDataString == NULL )
			return NULL;
		
		CFIndex index;
		for ( index = 0; index < typeCount; index++ )
		{
			stringRef = (CFStringRef) CFArrayGetValueAtIndex( inHashTypeArray, index );
			if ( stringRef != NULL )
			{
				if ( CFStringGetLength(newDataString) > 0 )
					CFStringAppend( newDataString, CFSTR(",") );
				CFStringAppend( newDataString, stringRef );
			}
		}
	}
	
	// build the auth-authority
	stringLen = CFStringGetLength( newDataString );
	newDataCStr = (char *) calloc( 1, stringLen + 1 );
	CFStringGetCString( newDataString, newDataCStr, stringLen + 1, kCFStringEncodingUTF8 );
	aaNewData = (char *) calloc( 1, sizeof(kDSValueAuthAuthorityShadowHash) + sizeof(kHashNameListPrefix) + stringLen + 2 );
	
	// build string
	if ( newDataCStr != NULL && aaNewData != NULL )
		len = sprintf( aaNewData, "%s<%s>", kHashNameListPrefix, newDataCStr );
	
	// clean up
	if ( newDataString != NULL )
		CFRelease( newDataString );
	if ( newDataCStr != NULL )
		free( newDataCStr );
	
	// return string length (if requested)
	if ( outResultLen != NULL )
		*outResultLen = len;
	
	return aaNewData;
}

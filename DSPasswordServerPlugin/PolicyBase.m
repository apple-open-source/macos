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

#import "PolicyBase.h"

@implementation PolicyBase

-(id)initWithUTF8String:(const char *)xmlDataStr
{
	CFDataRef xmlData;
	CFStringRef errorString;
	CFMutableDictionaryRef policyDict;
	
	if ( [self conformsToProtocol:@protocol(PolicyAbstract)] )
	{
		self = [super init];
		
		if ( xmlDataStr != NULL )
		{
			xmlData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)xmlDataStr, strlen(xmlDataStr) );
			if ( xmlData != NULL )
			{
				policyDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString );
				if ( policyDict != NULL ) {
					[(id)self convertDictToStruct:policyDict];
					CFRelease( policyDict );
				}
				CFRelease( xmlData );
			}
		}
		
		return self;
	}
	
	return nil;
}

-(id)initWithDictionary:(CFDictionaryRef)policyDict
{
	if ( [self conformsToProtocol:@protocol(PolicyAbstract)] )
	{
		self = [super init];
		[(id)self convertDictToStruct:policyDict];
		return self;
	}
	
	return nil;
}

-(CFStringRef)policyAsSpaceDelimitedDataCopy
{
	CFDictionaryRef policyDict = NULL;
	CFIndex index, keyCount;
	CFStringRef *key = NULL;
	CFTypeRef *value = NULL;
	CFMutableStringRef resultString = NULL;
	struct tm bsdTimeStruct;
	time_t bsdTime;
	char timeBuff[256];
	BOOL addSpaceChar = NO;
	
	if ( [self conformsToProtocol:@protocol(PolicyAbstract)] )
	{
		do
		{
			policyDict = [(id)self convertStructToDict];
			if ( policyDict == NULL )
				break;
			
			resultString = CFStringCreateMutable( kCFAllocatorDefault, 0 );
			if ( resultString == NULL )
				break;
			
			keyCount = CFDictionaryGetCount( policyDict );
			if ( keyCount == 0 )
				break;
			
			key = (CFStringRef *) calloc( sizeof(void *), keyCount );
			if ( key == NULL )
				break;
			
			value = (CFTypeRef *) calloc( sizeof(void *), keyCount );
			if ( value == NULL )
				break;
			
			CFDictionaryGetKeysAndValues( policyDict, (const void **)key, value );
			for ( index = 0; index < keyCount; index++ )
			{
				if ( key[index] == NULL || value[index] == NULL )
					break;
				
				if ( addSpaceChar )
					CFStringAppend( resultString, CFSTR(" ") );
				
				CFStringAppend( resultString, key[index] );
				CFStringAppend( resultString, CFSTR("=") );
				
				if ( CFGetTypeID(value[index]) == CFDateGetTypeID() )
				{
					pwsf_ConvertCFDateToBSDTime( (CFDateRef)value[index], &bsdTimeStruct );
					bsdTime = timegm( &bsdTimeStruct );
					snprintf(timeBuff, sizeof(timeBuff), "%lu", bsdTime);
					CFStringAppendCString( resultString, timeBuff, kCFStringEncodingUTF8 );
				}
				else
				if ( CFGetTypeID(value[index]) == CFStringGetTypeID() )
				{
					CFStringAppend( resultString, (CFStringRef)value[index] );
				}
				else
				{
					CFStringAppendFormat( resultString, NULL, CFSTR("%@"), value[index] );
				}
				
				addSpaceChar = YES;
			}
		}
		while (0);
		
		if ( policyDict != NULL )
			CFRelease( policyDict );
		if ( key != NULL )
			free( key );
		if ( value != NULL )
			free( value );
	}
	
	return resultString;
}

-(char *)policyAsXMLDataCopy
{
	return [self policyAsXMLDataCopy:NO];
}

-(char *)policyAsXMLDataCopy:(BOOL)withStateInfo
{
	CFDictionaryRef policyDict = NULL;
	CFDataRef xmlData = NULL;
	const UInt8 *sourcePtr;
	char *returnString = NULL;
	long length;
	
	if ( [self conformsToProtocol:@protocol(PolicyAbstract)] )
	{
		policyDict = withStateInfo ? [(id)self convertStructToDictWithState] : [(id)self convertStructToDict];
		if ( policyDict == NULL )
			return NULL;
		
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)policyDict );
		if ( xmlData == NULL )
			return NULL;
		
		CFRelease( policyDict );
		policyDict = NULL;
		
		sourcePtr = CFDataGetBytePtr( xmlData );
		length = CFDataGetLength( xmlData );
		if ( sourcePtr != NULL && length > 0 )
		{
			returnString = (char *) malloc( length + 1 );
			if ( returnString != NULL )
				strlcpy( returnString, (char *)sourcePtr, length );
		}
		
		CFRelease( xmlData );
		
		return returnString;
	}

	return NULL;
}


-(int)intValueForKey:(CFStringRef)key inDictionary:(CFDictionaryRef)dict
{
	CFTypeRef valueRef = NULL;
	int result = 0;
	
	if ( CFDictionaryGetValueIfPresent( dict, key, (const void **)&valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFBooleanGetTypeID() )
			result = CFBooleanGetValue( (CFBooleanRef)valueRef );
		else if ( CFGetTypeID(valueRef) == CFNumberGetTypeID() )
			CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberIntType, &result );
	}
	
	return result;
}

-(CFDateRef)dateValueForKey:(CFStringRef)key inDictionary:(CFDictionaryRef)dict
{
	CFDateRef result = NULL;
	
	if ( CFDictionaryGetValueIfPresent(dict, key, (const void **)&result) )
		if ( CFGetTypeID(result) == CFDateGetTypeID() )
			return result;
	
	return NULL;
}

@end

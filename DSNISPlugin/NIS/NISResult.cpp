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
 *  @header NISResult
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 */
 
#include <CoreFoundation/CoreFoundation.h>

#include "NISHeaders.h"

NISResult::NISResult()
{
	mSelfPtr = this;
    mAttributes = NULL;
}

NISResult::NISResult( CFMutableDictionaryRef initialResults )
{
	mSelfPtr = this;
    mAttributes = initialResults;
    
    if ( mAttributes )
        ::CFRetain( mAttributes );
}

NISResult::~NISResult()
{
	mSelfPtr = NULL;
    if ( mAttributes )
        ::CFRelease( mAttributes );
}


void FreeDictItems(const void *inKey, const void *inValue, void *inContext)
{
    if ( inKey )
        ::CFRelease(inKey);
    
    if ( inValue )
        ::CFRelease(inValue);
}


#pragma mark -

void NISResult::AddAttribute( const char* key, const char* value )
{
    if ( !mAttributes )
    {
        mAttributes = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    }
    
    if ( mAttributes && key && value )
    {
        CFStringRef		keyRef, valueRef;
        
        keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, key, kCFStringEncodingUTF8 );
        valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, value, kCFStringEncodingUTF8 );
        
        if ( !valueRef )
            DBGLOG( "NISResult::AddAttribute, couldn't create valueRef! (%s)\n", value );
        else    
            AddAttribute( keyRef, valueRef );
    
		if ( keyRef )	
			::CFRelease( keyRef );
			
		if ( valueRef )
			::CFRelease( valueRef );
	}
}

void NISResult::AddAttribute( CFStringRef keyRef, CFStringRef valueRef )
{
    if ( !mAttributes )
    {
        mAttributes = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    }
    
    if ( mAttributes && keyRef && valueRef )
    {
        ::CFDictionaryAddValue( mAttributes, keyRef, valueRef );
        
        if ( getenv("NSLDEBUG") )
            CFShow( mAttributes );
    }
    else
        DBGLOG( "NISResult::AddAttribute ignoring attribute\n" );
}

#pragma mark -

CFStringRef NISResult::GetAttributeRef( CFStringRef keyRef ) const
{
    CFStringRef		result = NULL;
    
	if ( mSelfPtr != this )
	{
		fprintf( stderr, "NISResult::GetAttributeRef called on a bad NISResult object!\n" );
		return NULL;
	}
	
    if ( mAttributes && keyRef && ::CFDictionaryGetCount( mAttributes ) > 0 && ::CFDictionaryContainsKey( mAttributes, keyRef ) )
        result = (CFStringRef)::CFDictionaryGetValue( mAttributes, keyRef );
    else if ( mAttributes && getenv( "NSLDEBUG" ) )
    {
        DBGLOG( "NISResult::GetAttributeRef not found in dictionary!\n" );
        ::CFShow( mAttributes );
    }
        
    return result;
}

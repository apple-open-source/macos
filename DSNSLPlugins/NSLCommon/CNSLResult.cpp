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
 *  @header CNSLResult
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 */
 
#include <CoreFoundation/CoreFoundation.h>

#include "CNSLHeaders.h"

CFStringRef SLPRAdminNotifierCopyDesctriptionCallback ( const void *item )
{
     CNSLResult*		itemResult = (CNSLResult*)item;
     
     return itemResult->GetURLRef();
}

Boolean SLPRAdminNotifierEqualCallback ( const void *item1, const void *item2 )
{
    CNSLResult*		item1Result = (CNSLResult*)item1;
    CNSLResult*		item2Result = (CNSLResult*)item2;
    
    if ( item1 && item2 )
        return ( ::CFStringCompare( item1Result->GetURLRef(), item2Result->GetURLRef(), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
    else
        return false;
}

CFStringRef CNSLResult::GetURLRef( void )
{
	return GetAttributeRef( kDSNAttrURLSAFE_CFSTR );
}

CFStringRef CNSLResult::GetServiceTypeRef( void )
{
	return GetAttributeRef( kDS1AttrServiceTypeSAFE_CFSTR );
}

CNSLResult::CNSLResult()
{
	mSelfPtr = this;
    mAttributes = NULL;
}

CNSLResult::CNSLResult( CFMutableDictionaryRef initialResults )
{
	mSelfPtr = this;
    mAttributes = initialResults;
    
    if ( mAttributes )
        ::CFRetain( mAttributes );
}

CNSLResult::~CNSLResult()
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

void CNSLResult::SetURL( const char* urlPtr )
{
    CFStringRef		urlForService = NULL;
    
    if ( urlPtr )
        urlForService = ::CFStringCreateWithCString( kCFAllocatorDefault, urlPtr, kCFStringEncodingUTF8 );
        
    if ( urlForService )
    {
        AddAttribute( kDSNAttrURLSAFE_CFSTR, urlForService );
        ::CFRelease( urlForService );
    }
}

void CNSLResult::SetURL( CFStringRef urlForService )
{
    if ( urlForService )
    {
        AddAttribute( kDSNAttrURLSAFE_CFSTR, urlForService );
    }
}

void CNSLResult::SetServiceType( const char* serviceType )
{
    CFStringRef		serviceTypeRef = NULL;
    
    if ( serviceType )
        serviceTypeRef = ::CFStringCreateWithCString( kCFAllocatorDefault, serviceType, kCFStringEncodingUTF8 );
        
    if ( serviceTypeRef )
    {
        AddAttribute( kDS1AttrServiceTypeSAFE_CFSTR, serviceTypeRef );
        AddAttribute( kDSNAttrRecordTypeSAFE_CFSTR, serviceTypeRef );		// these are the same
        ::CFRelease( serviceTypeRef );
    }
}

void CNSLResult::SetServiceType( CFStringRef serviceTypeRef )
{
    if ( serviceTypeRef )
    {
        AddAttribute( kDS1AttrServiceTypeSAFE_CFSTR, serviceTypeRef );
        AddAttribute( kDSNAttrRecordTypeSAFE_CFSTR, serviceTypeRef );		// these are the same
    }
}

#pragma mark -

void CNSLResult::AddAttribute( const char* key, const char* value )
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
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, value, ::NSLGetSystemEncoding() );
            
        if ( !valueRef )
            DBGLOG( "CNSLResult::AddAttribute, couldn't create valueRef! (%s)\n", value );
        else    
            AddAttribute( keyRef, valueRef );
    
		if ( keyRef )	
			::CFRelease( keyRef );
			
		if ( valueRef )
			::CFRelease( valueRef );
	}
}

void CNSLResult::AddAttribute( CFStringRef keyRef, CFStringRef valueRef )
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
        DBGLOG( "CNSLResult::AddAttribute ignoring attribute\n" );
}

#pragma mark -

CFStringRef CNSLResult::GetAttributeRef( CFStringRef keyRef ) const
{
    CFStringRef		result = NULL;
    
	if ( mSelfPtr != this )
	{
		fprintf( stderr, "CNSLResult::GetAttributeRef called on a bad CNSLResult object!\n" );
		return NULL;
	}
	
    if ( mAttributes && keyRef && ::CFDictionaryGetCount( mAttributes ) > 0 && ::CFDictionaryContainsKey( mAttributes, keyRef ) )
        result = (CFStringRef)::CFDictionaryGetValue( mAttributes, keyRef );
    else if ( mAttributes && getenv( "NSLDEBUG" ) )
    {
        DBGLOG( "CNSLResult::GetAttributeRef not found in dictionary!\n" );
        ::CFShow( mAttributes );
    }
        
    return result;
}

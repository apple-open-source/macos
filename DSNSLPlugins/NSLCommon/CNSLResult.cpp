/*
 *  CNSLResult.cpp
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 *
 *  Created by imlucid on Mon Aug 20 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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
        AddAttribute( CFSTR(kDSNAttrURL), urlForService );
        ::CFRelease( urlForService );
    }
}

void CNSLResult::SetURL( CFStringRef urlForService )
{
    if ( urlForService )
    {
        AddAttribute( CFSTR(kDSNAttrURL), urlForService );
    }
}

void CNSLResult::SetServiceType( const char* serviceType )
{
    CFStringRef		serviceTypeRef = NULL;
    
    if ( serviceType )
        serviceTypeRef = ::CFStringCreateWithCString( kCFAllocatorDefault, serviceType, kCFStringEncodingUTF8 );
        
    if ( serviceTypeRef )
    {
        AddAttribute( CFSTR(kDS1AttrServiceType), serviceTypeRef );
        AddAttribute( CFSTR(kNSLAttrRecordType), serviceTypeRef );		// these are the same
        ::CFRelease( serviceTypeRef );
    }
}

void CNSLResult::SetServiceType( CFStringRef serviceTypeRef )
{
    if ( serviceTypeRef )
    {
        AddAttribute( CFSTR(kDS1AttrServiceType), serviceTypeRef );
        AddAttribute( CFSTR(kNSLAttrRecordType), serviceTypeRef );		// these are the same
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
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, value, ::CFStringGetSystemEncoding() );
            
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









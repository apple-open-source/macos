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
 *  @header NISDirNodeRep
 */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/ServerModuleLib.h>

#include "NISHeaders.h"

CFStringRef NISResultNotifierCopyDesctriptionCallback( const void *item )
{
    const NISResult*	result = (NISResult*)item;
    
    return result->GetAttributeRef(CFSTR(kDSNAttrRecordName));
}

Boolean NISResultNotifierEqualCallback( const void *item1, const void *item2 )
{
    const NISResult*	result1 = (NISResult*)item1;
    const NISResult*	result2 = (NISResult*)item2;

    return ( ::CFStringCompare( result1->GetAttributeRef(CFSTR(kDSNAttrRecordName)), result2->GetAttributeRef(CFSTR(kDSNAttrRecordName)), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

CFStringRef CNSLSearchThreadCopyDesctriptionCallback( const void *item )
{
    return CFSTR( "CNSLSearchThread" );
}

Boolean CNSLSearchThreadEqualCallback( const void *item1, const void *item2 )
{
    return (item1 == item2);
}

NISDirNodeRep::NISDirNodeRep( NISPlugin* parent, const void* ref )
{
    mInitialized = false;
	mSelfPtr = this;
	mDeleteSelfWhenDone = false;
    mLookupStarted = false;
    mResultList = NULL;
    mSearchList = NULL;
    mCurrentIndex = 0;
    mNodeName = NULL;
    mParentPlugin = parent;
    mRef = ref;
    mDelCounter = 0;
}

NISDirNodeRep::~NISDirNodeRep( void )
{
	if ( mSelfPtr == this )
	{
		DBGLOG( "NISDirNodeRep::~NISDirNodeRep for ref %lx called %ld time(s)\n", (UInt32)mRef, mDelCounter );
		mSelfPtr = NULL;
		
		mDelCounter++;

		if ( mInitialized )
		{
			mInitialized = false;
			
			if ( mSearchList )
			{
				NISRecordLookupThread* searchThread = NULL;
		
				this->SearchListQueueLock();

				while ( mSearchList && ::CFArrayGetCount( mSearchList ) > 0 )
				{
					searchThread = (NISRecordLookupThread*)::CFArrayGetValueAtIndex( mSearchList, 0 );
					::CFArrayRemoveValueAtIndex( mSearchList, 0 );
					searchThread->Cancel();
				}
				
				if ( mSearchList )
					CFRelease( mSearchList );
				mSearchList = NULL;
				this->SearchListQueueUnlock();
			}
			
			if ( mResultList )
			{
				NISResult* result = NULL;
		
				this->ResultListQueueLock();
				
				while ( ::CFArrayGetCount( mResultList ) > 0 )
				{
					result = (NISResult*)::CFArrayGetValueAtIndex( mResultList, 0 );
					::CFArrayRemoveValueAtIndex( mResultList, 0 );
					delete result;
				}
				
				::CFRelease( mResultList );
				mResultList = NULL;
				
				this->ResultListQueueUnlock();
			}
			
			if ( mNodeName )
				::CFRelease( mNodeName );
			mNodeName = NULL;
		}
		else
			DBGLOG( "NISDirNodeRep::~NISDirNodeRep called but we aren't initialized! (ref: %lx)\n", (UInt32)mRef );
			
	}
	else
		DBGLOG( "0x%x NISDirNodeRep::~NISDirNodeRep called with a bad mSelfPtr!\n", (int)this );
}

sInt32 NISDirNodeRep::Initialize( const char* nodeNamePtr, uid_t uid )
{
    sInt32	siResult = eDSNoErr;
	
	if ( nodeNamePtr )
    {
        CFStringRef		nodeNameRef = ::CFStringCreateWithCString(kCFAllocatorDefault, nodeNamePtr, kCFStringEncodingUTF8);
        
		if ( nodeNameRef )
		{
			Initialize( nodeNameRef, uid );
			::CFRelease( nodeNameRef );
		}
		else
			siResult = eParameterError;
    }
	else
		siResult = eDSNullParameter;
	
	return siResult;
}

void NISDirNodeRep::Initialize( CFStringRef nodeNameRef, uid_t uid )
{
	DBGLOG( "NISDirNodeRep::Initialize called: 0x%x\n", (int)this );
    
    pthread_mutex_init( &mResultListQueueLock, NULL );
    pthread_mutex_init( &mSearchListQueueLock, NULL );
    
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = NISResultNotifierCopyDesctriptionCallback;
    callBack.equal = NISResultNotifierEqualCallback;

    mResultList = ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &callBack);

    callBack.copyDescription = CNSLSearchThreadCopyDesctriptionCallback;
    callBack.equal = CNSLSearchThreadEqualCallback;
    mSearchList = ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &callBack);
    
    mNodeName = nodeNameRef;
    ::CFRetain( mNodeName );
    
    mUID = uid;
    mInitialized = true;
}

Boolean NISDirNodeRep::IsTopLevelNode( void )
{
	CFComparisonResult	result = kCFCompareEqualTo;
		
	if ( mNodeName )
	{
		CFStringRef			blah = CFStringCreateWithCString( NULL, mParentPlugin->GetProtocolPrefixString(), kCFStringEncodingUTF8 );
		
		if ( blah )
		{
			result = CFStringCompare( blah, mNodeName, 0 );
			CFRelease( blah );
		}
	}
	else
		result = kCFCompareLessThan;
		
	return result == kCFCompareEqualTo;
}

Boolean NISDirNodeRep::HaveResults( void )
{
    Boolean		weHaveData = false;
    
    this->ResultListQueueLock();
    if ( mInitialized && mResultList && ::CFArrayGetCount( mResultList ) && (mCurrentIndex < mRecSearchLimit || mRecSearchLimit == 0) )
        weHaveData = ::CFArrayGetCount( mResultList ) > mCurrentIndex;
    
    this->ResultListQueueUnlock();
    
    return weHaveData;
}

Boolean NISDirNodeRep::LookupComplete( void )
{
    Boolean		lookupComplete;
    
    this->SearchListQueueLock();
    lookupComplete = (!mInitialized || ( mSearchList && ::CFArrayGetCount( mSearchList ) == 0 ));
    this->SearchListQueueUnlock();
    
    return lookupComplete;
}

const NISResult* NISDirNodeRep::GetResult( CFIndex index )
{
    const NISResult*	result = NULL;
    
    this->ResultListQueueLock();
	DBGLOG( "NISDirNodeRep::GetResult #%ld for ref %lx\n", index, (UInt32)mRef );

    if ( mInitialized && index < ::CFArrayGetCount( mResultList ) )
        result = (NISResult*)::CFArrayGetValueAtIndex( mResultList, index );

    this->ResultListQueueUnlock();
    
    return result;
}

#pragma mark -
void NISDirNodeRep::AddService( NISResult* newResult )
{
    this->ResultListQueueLock();
	DBGLOG( "NISDirNodeRep::AddService for ref %lx\n", (UInt32)mRef );
		
    if ( mInitialized )
        ::CFArrayAppendValue( mResultList, newResult );
    else
        DBGLOG( "NISDirNodeRep::AddService called but mInitialized is false! (ref: %lx)\n", (UInt32)mRef );
    this->ResultListQueueUnlock();
}

void NISDirNodeRep::StartingNewLookup( NISRecordLookupThread* searchThread )
{
	if ( mSelfPtr == this )
	{    
		if ( mInitialized )
		{
			this->SearchListQueueLock();
			::CFArrayAppendValue( mSearchList, searchThread );
			this->SearchListQueueUnlock();
		}
		else
			DBGLOG( "NISDirNodeRep::StartingNewLookup called but not initialized! (ref: %lx)\n", (UInt32)mRef );
			
		mLookupStarted = true; 
	}
	else
		DBGLOG( "0x%x NISDirNodeRep::StartingNewLookup called with a bad mSelfPtr!\n", (int)this );
}

void NISDirNodeRep::ServiceLookupComplete( NISRecordLookupThread* searchThread )
{    
	if ( mSelfPtr == this )
	{    
		this->SearchListQueueLock();
		for ( CFIndex i=0; mSearchList && i < ::CFArrayGetCount( mSearchList ); i++ )
		{
			NISRecordLookupThread* curThread = (NISRecordLookupThread*)::CFArrayGetValueAtIndex( mSearchList, i );
			
			if ( searchThread == curThread )
			{
				::CFArrayRemoveValueAtIndex( mSearchList, i );

				DBGLOG( "NISDirNodeRep::ServiceLookupComplete (ref: %lx), found thread and removing from list\n", (UInt32)mRef );
				break;
			}
		}
		
		if ( mSearchList && ::CFArrayGetCount( mSearchList ) == 0 && mDeleteSelfWhenDone )
		{
			this->SearchListQueueUnlock();
			delete this;
		}
		else
			this->SearchListQueueUnlock();
	}
	else
		DBGLOG( "0x%x NISDirNodeRep::ServiceLookupComplete called with a bad mSelfPtr!\n", (int)this );
} // ServiceLookupComplete

void NISDirNodeRep::DeleteSelf( void )
{
    this->SearchListQueueLock();
	if ( mSearchList && ::CFArrayGetCount( mSearchList ) == 0 && !mDeleteSelfWhenDone )
	{
		this->SearchListQueueUnlock();
		delete this;		// only go away if all our searches are gone.
	}
	else
	{
		CFIndex						index = 0;
		NISRecordLookupThread*	searchThread = NULL;
		
		while ( mSearchList && index < CFArrayGetCount( mSearchList ) && (searchThread = (NISRecordLookupThread*)::CFArrayGetValueAtIndex( mSearchList, index++ )) )
		{			
			searchThread->Cancel();
		}

		mDeleteSelfWhenDone = true;
	    this->SearchListQueueUnlock();
	}
}

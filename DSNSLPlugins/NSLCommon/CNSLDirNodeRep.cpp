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
 *  @header CNSLDirNodeRep
 */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/ServerModuleLib.h>

#include "CNSLHeaders.h"

const CFStringRef	kCNSLSearchThreadStr = CFSTR( "CNSLSearchThread" );

CFStringRef CNSLResultNotifierCopyDesctriptionCallback( const void *item )
{
    CNSLResult*	result = (CNSLResult*)item;
    
    return result->GetURLRef();
}

Boolean CNSLResultNotifierEqualCallback( const void *item1, const void *item2 )
{
    CNSLResult*	result1 = (CNSLResult*)item1;
    CNSLResult*	result2 = (CNSLResult*)item2;

    return ( ::CFStringCompare( result1->GetURLRef(), result2->GetURLRef(), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

CFStringRef CNSLSearchThreadCopyDesctriptionCallback( const void *item )
{
    return kCNSLSearchThreadStr;
}

Boolean CNSLSearchThreadEqualCallback( const void *item1, const void *item2 )
{
    return (item1 == item2);
}

CNSLDirNodeRep::CNSLDirNodeRep( CNSLPlugin* parent, const void* ref )
{
    mInitialized = false;
	mSelfPtr = this;
    mLookupStarted = false;
    mResultList = NULL;
    mSearchList = NULL;
    mCurrentIndex = 0;
    mNodeName = NULL;
    mParentPlugin = parent;
    mRef = ref;
    mDelCounter = 0;
	mRefCounter = 0;
	mRecSearchLimit = 0;
}

CNSLDirNodeRep::~CNSLDirNodeRep( void )
{
	if ( mSelfPtr == this )
	{
		DBGLOG( "CNSLDirNodeRep::~CNSLDirNodeRep for ref %lx called %ld time(s)\n", (UInt32)mRef, mDelCounter );

		mSelfPtr = NULL;
		
		mDelCounter++;

		if ( mInitialized )
		{
			mInitialized = false;
			
			if ( mSearchList )
			{
				CNSLServiceLookupThread* searchThread = NULL;
		
				this->SearchListQueueLock();
				CFIndex		numSearches = ::CFArrayGetCount( mSearchList );
				
				for ( CFIndex i=numSearches-1; i>=0; i-- )
				{
					searchThread = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchList, i );
					::CFArrayRemoveValueAtIndex( mSearchList, i );
					SearchCompleted();
					searchThread->Cancel();
				}
				
				CFRelease( mSearchList );
				mSearchList = NULL;
				this->SearchListQueueUnlock();
			}
			
			if ( mResultList )
			{
				CNSLResult* result = NULL;
		
				this->ResultListQueueLock();
				CFIndex		numSearches = ::CFArrayGetCount( mResultList );

				for ( CFIndex i=numSearches-1; i>=0; i-- )
				{
					result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, i );
					::CFArrayRemoveValueAtIndex( mResultList, i );
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
			DBGLOG( "CNSLDirNodeRep::~CNSLDirNodeRep called but we aren't initialized! (ref: %lx)\n", (UInt32)mRef );
	}
	else
		DBGLOG( "0x%x CNSLDirNodeRep::~CNSLDirNodeRep called with a bad mSelfPtr!\n", this );
}

void CNSLDirNodeRep::Initialize( const char* nodeNamePtr, uid_t uid, Boolean isTopLevelNode )
{
    if ( nodeNamePtr )
    {
        CFStringRef		nodeNameRef = ::CFStringCreateWithCString(kCFAllocatorDefault, nodeNamePtr, kCFStringEncodingUTF8);
        
        Initialize( nodeNameRef, uid, isTopLevelNode );
        ::CFRelease( nodeNameRef );
    }
}

void CNSLDirNodeRep::Initialize( CFStringRef nodeNameRef, uid_t uid, Boolean isTopLevelNode )
{
	DBGLOG( "CNSLDirNodeRep::Initialize called: 0x%x, uid: %d, isTopLevelNode: %d\n", (void*)this, uid, isTopLevelNode );
    
    pthread_mutex_init( &mResultListQueueLock, NULL );
    pthread_mutex_init( &mSearchListQueueLock, NULL );
    
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = CNSLResultNotifierCopyDesctriptionCallback;
    callBack.equal = CNSLResultNotifierEqualCallback;

    mResultList = ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &callBack);

    callBack.copyDescription = CNSLSearchThreadCopyDesctriptionCallback;
    callBack.equal = CNSLSearchThreadEqualCallback;
    mSearchList = ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &callBack);
    
    mNodeName = nodeNameRef;
    ::CFRetain( mNodeName );
    
    mUID = uid;
    mInitialized = true;
    mIsTopLevelNode = isTopLevelNode;
}

Boolean CNSLDirNodeRep::HaveResults( void )
{
    Boolean		weHaveData = false;
    
    this->ResultListQueueLock();
    if ( mInitialized && mResultList && ::CFArrayGetCount( mResultList ) && (mCurrentIndex < mRecSearchLimit || mRecSearchLimit == 0) )
        weHaveData = ::CFArrayGetCount( mResultList ) > mCurrentIndex;
    
    this->ResultListQueueUnlock();
    
    return weHaveData;
}

Boolean CNSLDirNodeRep::LookupComplete( void )
{
    Boolean		lookupComplete;
    
    this->SearchListQueueLock();
    lookupComplete = (!mInitialized || ::CFArrayGetCount( mSearchList ) == 0);
    this->SearchListQueueUnlock();
    
    return lookupComplete;
}

const CNSLResult* CNSLDirNodeRep::GetNextResult( void )
{
    const CNSLResult*	result = NULL;
    
    this->ResultListQueueLock();
    
    result = GetResult( mCurrentIndex++ );
    
    this->ResultListQueueUnlock();
    
    return result;
}

const CNSLResult* CNSLDirNodeRep::GetResult( CFIndex index )
{
    const CNSLResult*	result = NULL;
    
	DBGLOG( "CNSLDirNodeRep::GetResult for ref %lx\n", (UInt32)mRef );

    if ( mInitialized && index < ::CFArrayGetCount( mResultList ) )
        result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, index );

    return result;
}

#pragma mark -
void CNSLDirNodeRep::AddService( CNSLResult* newResult )
{
    this->ResultListQueueLock();
	DBGLOG( "CNSLDirNodeRep::AddService for ref %lx\n", (UInt32)mRef );
		
    if ( mInitialized )
        ::CFArrayAppendValue( mResultList, newResult );
    else
        DBGLOG( "CNSLDirNodeRep::AddService called but mInitialized is false! (ref: %lx)\n", (UInt32)mRef );
    this->ResultListQueueUnlock();
}

void CNSLDirNodeRep::StartingNewLookup( CNSLServiceLookupThread* searchThread )
{
	if ( mSelfPtr == this )
	{    
		SearchStarted();
		
		if ( mInitialized )
		{
			this->SearchListQueueLock();
			::CFArrayAppendValue( mSearchList, searchThread );
			this->SearchListQueueUnlock();
		}
		else
			DBGLOG( "CNSLDirNodeRep::StartingNewLookup called but not initialized! (ref: %lx)\n", (UInt32)mRef );
			
		mLookupStarted = true; 
	}
	else
		DBGLOG( "0x%x CNSLDirNodeRep::StartingNewLookup called with a bad mSelfPtr!\n", this );
}

void CNSLDirNodeRep::ServiceLookupComplete( CNSLServiceLookupThread* searchThread )
{    
	if ( mSelfPtr == this )
	{    
		this->SearchListQueueLock();
		CFIndex		origNumItems = ::CFArrayGetCount( mSearchList );
		
		for ( CFIndex i=0; mSearchList && i < origNumItems; i++ )
		{
			CNSLServiceLookupThread* curThread = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchList, i );
			
			if ( searchThread == curThread )
			{
				::CFArrayRemoveValueAtIndex( mSearchList, i );

				SearchCompleted();
	
				DBGLOG( "CNSLDirNodeRep::ServiceLookupComplete (ref: %lx), found thread and removing from list\n", (UInt32)mRef );
				break;
			}
		}
		
		this->SearchListQueueUnlock();
	}
	else
		DBGLOG( "0x%x CNSLDirNodeRep::ServiceLookupComplete called with a bad mSelfPtr!\n", this );
} // ServiceLookupComplete

void CNSLDirNodeRep::DeleteSelf( void )
{
	delete this;
}

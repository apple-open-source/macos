/*
 *  CNSLDirNodeRep.cpp
 *
 *	This is a class that is used to representing a client session on a dir node.
 *	All searches are controlled by this class
 *
 *  Created by imlucid on Tue Aug 20 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/ServerModuleLib.h>
//#include <DirectoryServiceCore/UException.h>
/*
#include "DSUtils.h"
#include "ServerModuleLib.h"
#include "UException.h"
*/
#include "CNSLHeaders.h"

CFStringRef CNSLResultNotifierCopyDesctriptionCallback( const void *item )
{
    const CNSLResult*	result = (CNSLResult*)item;
    
    return result->GetURLRef();
}

Boolean CNSLResultNotifierEqualCallback( const void *item1, const void *item2 )
{
    const CNSLResult*	result1 = (CNSLResult*)item1;
    const CNSLResult*	result2 = (CNSLResult*)item2;

    return ( ::CFStringCompare( result1->GetURLRef(), result2->GetURLRef(), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

CFStringRef CNSLSearchThreadCopyDesctriptionCallback( const void *item )
{
    return CFSTR( "CNSLSearchThread" );
}

Boolean CNSLSearchThreadEqualCallback( const void *item1, const void *item2 )
{
    return (item1 == item2);
}


///pthread_mutex_t	CNSLDirNodeRep::mQueueLock;

CNSLDirNodeRep::CNSLDirNodeRep( CNSLPlugin* parent, const void* ref )
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

CNSLDirNodeRep::~CNSLDirNodeRep( void )
{
	if ( mSelfPtr == this )
	{
		DBGLOG( "CNSLDirNodeRep::~CNSLDirNodeRep for ref %lx called %ld time(s)\n", (UInt32)mRef, mDelCounter );
		mParentPlugin->LockPlugin();
		mSelfPtr = NULL;
		
		mDelCounter++;

		if ( mInitialized )
		{
			mInitialized = false;
			
			if ( mSearchList )
			{
				CNSLServiceLookupThread* searchThread = NULL;
		
				this->SearchListQueueLock();

				while ( mSearchList && ::CFArrayGetCount( mSearchList ) > 0 )
				{
					searchThread = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchList, 0 );
					::CFArrayRemoveValueAtIndex( mSearchList, 0 );
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
				
				while ( ::CFArrayGetCount( mResultList ) > 0 )
				{
					result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, 0 );
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
			DBGLOG( "CNSLDirNodeRep::~CNSLDirNodeRep called but we aren't initialized! (ref: %lx)\n", (UInt32)mRef );
			
		mParentPlugin->UnlockPlugin();
	}
	else
		DBGLOG( "0x%x CNSLDirNodeRep::~CNSLDirNodeRep called with a bad mSelfPtr!\n", this );
}

void CNSLDirNodeRep::Initialize( const char* nodeNamePtr, uid_t uid )
{
    if ( nodeNamePtr )
    {
        CFStringRef		nodeNameRef = ::CFStringCreateWithCString(kCFAllocatorDefault, nodeNamePtr, kCFStringEncodingUTF8);
        
        Initialize( nodeNameRef, uid );
        ::CFRelease( nodeNameRef );
    }
}

void CNSLDirNodeRep::Initialize( CFStringRef nodeNameRef, uid_t uid )
{
    mParentPlugin->LockPlugin();

	DBGLOG( "CNSLDirNodeRep::Initialize called: 0x%x\n", (void*)this );
    
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
    
    mParentPlugin->UnlockPlugin();
}

Boolean CNSLDirNodeRep::HaveResults( void )
{
    Boolean		weHaveData = false;
    
//    mParentPlugin->LockPlugin();
    
    this->ResultListQueueLock();
    if ( mInitialized && mResultList && ::CFArrayGetCount( mResultList ) && (mCurrentIndex < mRecSearchLimit || mRecSearchLimit == 0) )
        weHaveData = ::CFArrayGetCount( mResultList ) > mCurrentIndex;
    
    this->ResultListQueueUnlock();
//    mParentPlugin->UnlockPlugin();
    
    return weHaveData;
}

Boolean CNSLDirNodeRep::LookupComplete( void )
{
    Boolean		lookupComplete;
    
//    mParentPlugin->LockPlugin();
    
    this->SearchListQueueLock();
    lookupComplete = (!mInitialized || ::CFArrayGetCount( mSearchList ) == 0);
    this->SearchListQueueUnlock();
    
//    mParentPlugin->UnlockPlugin();
    
    return lookupComplete;
}

const CNSLResult* CNSLDirNodeRep::GetNextResult( void )
{
    const CNSLResult*	result = NULL;
    
    mParentPlugin->LockPlugin();
    
    result = GetResult( mCurrentIndex++ );
    
    mParentPlugin->UnlockPlugin();
    
    return result;
}

const CNSLResult* CNSLDirNodeRep::GetResult( CFIndex index )
{
    const CNSLResult*	result = NULL;
    
    this->ResultListQueueLock();
	DBGLOG( "CNSLDirNodeRep::GetResult for ref %lx\n", (UInt32)mRef );

    if ( mInitialized && index < ::CFArrayGetCount( mResultList ) )
        result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, index );

    this->ResultListQueueUnlock();
    
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
//    Boolean			listQueueUnlocked = false;
	if ( mSelfPtr == this )
	{    
		this->SearchListQueueLock();
		for ( CFIndex i=0; mSearchList && i < ::CFArrayGetCount( mSearchList ); i++ )
		{
			CNSLServiceLookupThread* curThread = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchList, i );
			
			if ( searchThread == curThread )
			{
				::CFArrayRemoveValueAtIndex( mSearchList, i );
	//            this->SearchListQueueUnlock();
	//            listQueueUnlocked = true;
				SearchCompleted();
	
				DBGLOG( "CNSLDirNodeRep::ServiceLookupComplete (ref: %lx), found thread and removing from list\n", (UInt32)mRef );
				break;
			}
		}
		
	//    if ( !listQueueUnlocked )
	//        this->SearchListQueueUnlock();
			
		if ( mSearchList && ::CFArrayGetCount( mSearchList ) == 0 && mDeleteSelfWhenDone )
		{
			this->SearchListQueueUnlock();
			delete this;
		}
		else
			this->SearchListQueueUnlock();
	}
	else
		DBGLOG( "0x%x CNSLDirNodeRep::ServiceLookupComplete called with a bad mSelfPtr!\n", this );
} // ServiceLookupComplete

void CNSLDirNodeRep::DeleteSelf( void )
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
		CNSLServiceLookupThread*	searchThread = NULL;
		
		while ( mSearchList && index < CFArrayGetCount( mSearchList ) && (searchThread = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchList, index++ )) )
		{			
			searchThread->Cancel();
		}

		mDeleteSelfWhenDone = true;
	    this->SearchListQueueUnlock();
	}
}













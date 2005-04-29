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
 *  @header BSDDirNodeRep
 */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/ServerModuleLib.h>

#include "BSDHeaders.h"

CFStringRef NISResultNotifierCopyDesctriptionCallback( const void *item )
{
    const BSDResult*	result = (BSDResult*)item;
    
    return result->GetAttributeRef(CFSTR(kDSNAttrRecordName));
}

Boolean NISResultNotifierEqualCallback( const void *item1, const void *item2 )
{
    const BSDResult*	result1 = (BSDResult*)item1;
    const BSDResult*	result2 = (BSDResult*)item2;

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

BSDDirNodeRep::BSDDirNodeRep( BSDPlugin* parent, const void* ref )
{
    mInitialized = false;
	mIsFFNode = true;
	mSelfPtr = this;
	mDeleteSelfWhenDone = false;
    mLookupStarted = false;
    mResultList = NULL;
    mCurrentIndex = 0;
    mNodeName = NULL;
    mParentPlugin = parent;
    mRef = ref;
    mDelCounter = 0;
	mRefCounter = 0;
}

BSDDirNodeRep::~BSDDirNodeRep( void )
{
	if ( mSelfPtr == this )
	{
		DBGLOG( "BSDDirNodeRep::~BSDDirNodeRep for ref %lx called %ld time(s)\n", (UInt32)mRef, mDelCounter );
		mSelfPtr = NULL;
		
		mDelCounter++;

		if ( mInitialized )
		{
			mInitialized = false;
						
			if ( mResultList )
			{
				BSDResult* result = NULL;
		
				this->ResultListQueueLock();
				
				while ( ::CFArrayGetCount( mResultList ) > 0 )
				{
					result = (BSDResult*)::CFArrayGetValueAtIndex( mResultList, 0 );
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
			DBGLOG( "BSDDirNodeRep::~BSDDirNodeRep called but we aren't initialized! (ref: %lx)\n", (UInt32)mRef );
			
	}
	else
		DBGLOG( "0x%x BSDDirNodeRep::~BSDDirNodeRep called with a bad mSelfPtr!\n", (int)this );
}

sInt32 BSDDirNodeRep::Initialize( const char* nodeNamePtr, uid_t uid )
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

void BSDDirNodeRep::Initialize( CFStringRef nodeNameRef, uid_t uid )
{
	DBGLOG( "BSDDirNodeRep::Initialize called: 0x%x\n", (int)this );
    
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
    
    mNodeName = nodeNameRef;
    ::CFRetain( mNodeName );
	
#ifdef BUILDING_COMBO_PLUGIN
	if ( CFStringHasSuffix( mNodeName, CFSTR(kFFNodeName) ) )
		mIsFFNode = true;
	else
#endif
		mIsFFNode = false;
    
    mUID = uid;
    mInitialized = true;
}

Boolean BSDDirNodeRep::IsTopLevelNode( void )
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

Boolean BSDDirNodeRep::HaveResults( void )
{
    Boolean		weHaveData = false;
    
    this->ResultListQueueLock();
    if ( mInitialized && mResultList && ::CFArrayGetCount( mResultList ) && (mCurrentIndex < mRecSearchLimit || mRecSearchLimit == 0) )
        weHaveData = ::CFArrayGetCount( mResultList ) > mCurrentIndex;
    
    this->ResultListQueueUnlock();
    
    return weHaveData;
}

const BSDResult* BSDDirNodeRep::GetResult( CFIndex index )
{
    const BSDResult*	result = NULL;
    
    this->ResultListQueueLock();
	DBGLOG( "BSDDirNodeRep::GetResult #%ld for ref %lx\n", index, (UInt32)mRef );

    if ( mInitialized && index < ::CFArrayGetCount( mResultList ) )
        result = (BSDResult*)::CFArrayGetValueAtIndex( mResultList, index );

    this->ResultListQueueUnlock();
    
    return result;
}

#pragma mark -
void BSDDirNodeRep::AddService( BSDResult* newResult )
{
    this->ResultListQueueLock();
	DBGLOG( "BSDDirNodeRep::AddService for ref %lx\n", (UInt32)mRef );
		
    if ( mInitialized )
        ::CFArrayAppendValue( mResultList, newResult );
    else
        DBGLOG( "BSDDirNodeRep::AddService called but mInitialized is false! (ref: %lx)\n", (UInt32)mRef );
    this->ResultListQueueUnlock();
}

void BSDDirNodeRep::DeleteSelf( void )
{
	delete this;
}

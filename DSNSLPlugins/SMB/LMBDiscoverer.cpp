/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  @class LMBDiscoverer
 */

#include "LMBDiscoverer.h"
#include "CommandLineUtilities.h"
#include "CNSLTimingUtils.h"
#include "CSMBPlugin.h"

#ifdef TOOL_LOGGING
#include <mach/mach_time.h>	// for dsTimeStamp
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

const CFStringRef	kGetPrimarySAFE_CFSTR = CFSTR("getPrimary");

extern void AddNode( CFStringRef nodeNameRef );	// needs to be defined by code including this
extern CFStringEncoding NSLGetSystemEncoding( void );

Boolean IsSafeToConnectToAddress( char* address );

LMBDiscoverer::LMBDiscoverer( void )
{
	mNodeListIsCurrent = false;
	mNodeSearchInProgress = false;
    mLocalNodeString = NULL;		
	mWINSServer = NULL;
	mBroadcastAddr = NULL;
	mAllKnownLMBs = NULL;
	mListOfLMBsInProgress = NULL;
	mListOfBadLMBs = NULL;
	mInitialSearch = true;
	mNeedFreshLookup = true;
	mCurrentSearchCanceled = false;
	mThreadsRunning = 0;
	mLastTimeLMBsSearched = 0;
}

LMBDiscoverer::~LMBDiscoverer( void )
{
	ClearBadLMBList();
}

sInt32 LMBDiscoverer::Initialize( void )
{
    sInt32				siResult	= eDSNoErr;
    
    pthread_mutex_init( &mAllKnownLMBsLock, NULL );
	pthread_mutex_init( &mListOfLMBsInProgressLock, NULL );
	pthread_mutex_init( &mOurBroadcastAddressLock, NULL );
	pthread_mutex_init( &mLockBadLMBListLock, NULL );
	DBGLOG( "LMBDiscoverer::InitPlugin\n" );
	
    return siResult;
}

#pragma mark -
void LMBDiscoverer::DiscoverCurrentWorkgroups( void )
{
	DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups called\n" );
	mLastTimeLMBsSearched = CFAbsoluteTimeGetCurrent();

	CFArrayRef	ourLMBs = CreateListOfLMBs();
	
	if ( ourLMBs )
	{
		CFIndex		lmbCount = CFArrayGetCount(ourLMBs);
		DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups getting info from %d LMBs we know about\n", (int)CFArrayGetCount(ourLMBs) );
		
		for ( CFIndex i=0; i<lmbCount && !mCurrentSearchCanceled; i++ )
		{
			CFStringRef		lmbRef = (CFStringRef)CFArrayGetValueAtIndex( ourLMBs, i );
			
			// instead of firing off threads for each LMB, just query one at a time until we get a result (should be first one)
			DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups get LMB Info from %@\n", lmbRef );
			CFArrayRef lmbResults = GetLMBInfoFromLMB( NULL, lmbRef );

			if ( lmbResults )
			{
				Boolean		gotResults = CFArrayGetCount(lmbResults) > 0;
				
				for ( CFIndex j=CFArrayGetCount(lmbResults)-2; j>=0; j-=2 )
				{
					CFStringRef		workgroupRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, j);
					CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, j+1);
					
					if ( workgroupRef && CFStringGetLength( workgroupRef ) > 0 && lmbNameRef && CFStringGetLength( lmbNameRef ) > 0 && !IsLMBKnown( workgroupRef, lmbNameRef ) )		// do we already know about this workgroup?
						AddToCachedResults( workgroupRef, lmbNameRef );		// if this is valid, add it to our list
				}

				CFRelease( lmbResults );
				
				if ( gotResults )
				{
					DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups got info from %@, ignore the rest\n", lmbRef );
					break;		// just need one set of results
				}

				DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups info from %@ was useless, try another\n", lmbRef );
			}
		}

		if ( GetWinsServer() )
		{
			DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups getting info from WINS server %s\n", GetWinsServer() );
			CFStringRef		winsRef = CFStringCreateWithCString( NULL, GetWinsServer(), kCFStringEncodingUTF8 );
			
			if ( winsRef )
			{
				CFArrayRef winsResults = GetLMBInfoFromLMB( NULL, winsRef );
	
				if ( winsResults )
				{
					for ( CFIndex j=CFArrayGetCount(winsResults)-2; j>=0; j-=2 )
					{
						CFStringRef		workgroupRef = (CFStringRef)CFArrayGetValueAtIndex(winsResults, j);
						CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex(winsResults, j+1);
						
						if ( workgroupRef && CFStringGetLength( workgroupRef ) > 0 && lmbNameRef && CFStringGetLength( lmbNameRef ) > 0 && !IsLMBKnown( workgroupRef, lmbNameRef ) )		// do we already know about this workgroup?
							AddToCachedResults( workgroupRef, lmbNameRef );		// if this is valid, add it to our list
					}
	
					CFRelease( winsResults );
				}
			}
		}
		
		if ( mCurrentSearchCanceled )
		{
			DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups canceled as mCurrentSearchCanceled is set to true\n" );
		}
		
		CFRelease( ourLMBs );
		
		while ( mThreadsRunning > 0 )
			SmartSleep(1*USEC_PER_SEC);
			
		if ( !mCurrentSearchCanceled )
			UpdateCachedLMBResults();
		else
		{
			LockLMBsInProgress();
			DBGLOG( "LMBDiscoverer::DiscoverCurrentWorkgroups was canceled, clearing LMBs found\n" );
			
			if ( mListOfLMBsInProgress )
				CFRelease( mListOfLMBsInProgress );
			
			mListOfLMBsInProgress = NULL;
			
			UnLockLMBsInProgress();
		}
	}
}

#pragma mark -
void LMBDiscoverer::ResetOurBroadcastAddress( void )
{
	LockOurBroadcastAddress();
	if ( mBroadcastAddr )
	{
		free( mBroadcastAddr );
		mBroadcastAddr = NULL;
	}
	
	UnLockOurBroadcastAddress();
}

char* LMBDiscoverer::CopyBroadcastAddress( void )
{
	char*		newBroadcastAddress = NULL;
	
	LockOurBroadcastAddress();
	
	if ( !mBroadcastAddr )
	{
		char*		address = NULL;
		sInt32		status = GetPrimaryInterfaceBroadcastAdrs( &address );
		
		if ( status )
			DBGLOG( "LMBDiscoverer::GetPrimaryInterfaceBroadcastAdrs returned error: %ld\n", status );
		else if ( address )
		{
			if ( IsSafeToConnectToAddress( address ) )
			{
				DBGLOG( "LMBDiscoverer::CopyBroadcastAddress found address reachable w/o dialup required\n" );
				mBroadcastAddr = address;
			}
			else
			{
				DBGLOG( "LMBDiscoverer::CopyBroadcastAddress found address not reachable w/o dialup being initiated, ignoreing\n" );
				free( address );
			}
		}
	}
	
	if ( mBroadcastAddr )
		newBroadcastAddress = strdup( mBroadcastAddr );
		
	UnLockOurBroadcastAddress();
	
	return newBroadcastAddress;
}

/************************************
 * GetPrimaryInterfaceBroadcastAdrs *
*************************************

	Return the IP addr of the primary interface broadcast address.
*/

sInt32 LMBDiscoverer::GetPrimaryInterfaceBroadcastAdrs( char** broadcastAddr )
{
	CFArrayRef			subnetMasks = NULL;
	CFDictionaryRef		globalDict = NULL;
	CFStringRef			key = NULL;
	CFStringRef			primaryService = NULL, router = NULL;
	CFDictionaryRef		serviceDict = NULL;
	SCDynamicStoreRef	store = NULL;
	sInt32				status = 0;
	CFStringRef			subnetMask = NULL;
	CFArrayRef			addressPieces = NULL, subnetMaskPieces = NULL;
	
	
	do {
		store = SCDynamicStoreCreate(NULL, kGetPrimarySAFE_CFSTR, NULL, NULL);
		if (!store) {
			DBGLOG("SCDynamicStoreCreate() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCEntNetIPv4);
		
		globalDict = (CFDictionaryRef)SCDynamicStoreCopyValue(store, key);
		CFRelease( key );
		
		if (!globalDict) {
			DBGLOG("SCDynamicStoreCopyValue() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		primaryService = (CFStringRef)CFDictionaryGetValue(globalDict,
							kSCDynamicStorePropNetPrimaryService);
		if (!primaryService) {
			DBGLOG("no primary service: %s\n", SCErrorString(SCError()) );

			status = -1;
			break;
		}
	
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								kSCDynamicStoreDomainState,
								primaryService,
								kSCEntNetIPv4);
		serviceDict = (CFDictionaryRef)SCDynamicStoreCopyValue(store, key);
		
		CFRelease(key);
		if (!serviceDict) {
			DBGLOG("SCDynamicStoreCopyValue() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		CFArrayRef	addressList = (CFArrayRef)CFDictionaryGetValue(serviceDict, kSCPropNetIPv4Addresses);
		
		router = (CFStringRef)CFDictionaryGetValue(serviceDict,
						kSCPropNetIPv4Router);
		if (!router) {
			
			if ( addressList && CFArrayGetCount(addressList) > 0 )
			{
				// no router, just use our address instead.
				router = (CFStringRef)CFArrayGetValueAtIndex( addressList, 0 );
			}	
			else
			{
				DBGLOG("no router\n" );
				status = -1;
				break;

			}
		}
		
		subnetMasks = (CFArrayRef)CFDictionaryGetValue(serviceDict,
						kSCPropNetIPv4SubnetMasks);
		if (!subnetMasks) {
			DBGLOG("no subnetMasks\n" );
			status = -1;
			break;
		}
	
		addressPieces = CFStringCreateArrayBySeparatingStrings( NULL, router, kDotSAFE_CFSTR );
	
		if ( subnetMasks )
		{
			subnetMask = (CFStringRef)CFArrayGetValueAtIndex(subnetMasks, 0);
	
			subnetMaskPieces = CFStringCreateArrayBySeparatingStrings( NULL, subnetMask, kDotSAFE_CFSTR );
		}
		
		char	bcastAddr[256] = {0};
		CFIndex	addressPiecesCount = CFArrayGetCount(addressPieces);
		
		for (int j=0; j<addressPiecesCount; j++)
		{
			int	addr = CFStringGetIntValue((CFStringRef)CFArrayGetValueAtIndex(addressPieces, j));
			int mask = CFStringGetIntValue((CFStringRef)CFArrayGetValueAtIndex(subnetMaskPieces, j));
			int invMask = (~mask & 255);
			int bcast = invMask | addr;
			
			char	bcastPiece[5];
			snprintf( bcastPiece, sizeof(bcastPiece), "%d.", bcast );
			strcat( bcastAddr, bcastPiece );
		}
		
		bcastAddr[strlen(bcastAddr)-1] = '\0';
		
		*broadcastAddr = (char*)malloc(strlen(bcastAddr)+1);
		strcpy( *broadcastAddr, bcastAddr );
	} while (false);
	
	if ( serviceDict )
		CFRelease( serviceDict );

	if ( globalDict )
		CFRelease( globalDict );

	if ( store )
		CFRelease( store );
		
	if ( addressPieces )
		CFRelease( addressPieces );
	
	if ( subnetMaskPieces )
		CFRelease( subnetMaskPieces );
		
	return status;

}

void LMBDiscoverer::ClearLMBCache( void )
{
	LockAllKnownLMBs();
	DBGLOG( "LMBDiscoverer::ClearLMBCache called\n" );
	
	if ( mAllKnownLMBs )
		CFRelease( mAllKnownLMBs );
	
	mAllKnownLMBs = NULL;
		
	UnLockAllKnownLMBs();
}

void LMBDiscoverer::UpdateCachedLMBResults( void )
{
	LockAllKnownLMBs();
	LockLMBsInProgress();
	DBGLOG( "LMBDiscoverer::UpdateCachedLMBResults called\n" );
	
	if ( mAllKnownLMBs )
		CFRelease( mAllKnownLMBs );
	
	mAllKnownLMBs = mListOfLMBsInProgress;
	mListOfLMBsInProgress = NULL;
	
	UnLockLMBsInProgress();
	UnLockAllKnownLMBs();
}

Boolean LMBDiscoverer::ClearLMBForWorkgroup( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	CFMutableArrayRef		lmbsForWorkgroup = NULL;
	Boolean					lastLMBForWorkgroup = false;
	
	LockAllKnownLMBs();
	DBGLOG( "LMBDiscoverer::ClearLMBForWorkgroup called\n" );

	if ( mAllKnownLMBs )
		lmbsForWorkgroup = (CFMutableArrayRef)CFDictionaryGetValue( mAllKnownLMBs, workgroupRef );
	
	if ( lmbsForWorkgroup && lmbNameRef && CFArrayGetCount( lmbsForWorkgroup ) > 0 )
	{
		CFIndex		index = kCFNotFound;
		
		for ( CFIndex i=CFArrayGetCount(lmbsForWorkgroup)-1; i>=0; i-- )
		{
			if ( CFStringCompare( lmbNameRef, (CFStringRef)CFArrayGetValueAtIndex( lmbsForWorkgroup, i ), 0 ) == kCFCompareEqualTo )
			{
				index = i;
				break;
			}
		}
		
		if ( index != kCFNotFound )
			CFArrayRemoveValueAtIndex( lmbsForWorkgroup, index );
			
		if ( CFArrayGetCount( lmbsForWorkgroup ) == 0 )
		{
			CFDictionaryRemoveValue( mAllKnownLMBs, workgroupRef );
			lmbsForWorkgroup = NULL;
		}
	}
	
	if ( !lmbsForWorkgroup )
		lastLMBForWorkgroup = true;
	
	UnLockAllKnownLMBs();
	
	return lastLMBForWorkgroup;
}

#pragma mark -
void LMBDiscoverer::ClearBadLMBList( void )
{
	LockBadLMBList();
	
	if ( mListOfBadLMBs )
		CFRelease( mListOfBadLMBs );
	mListOfBadLMBs = NULL;
	
	UnLockBadLMBList();
}

Boolean	LMBDiscoverer::IsLMBOnBadList( CFStringRef lmbNameRef )
{
	Boolean		isBadLMB = false;
	
	LockBadLMBList();
	
	if ( mListOfBadLMBs && CFDictionaryContainsKey( mListOfBadLMBs, lmbNameRef ) )
	{
		char			timeStamp[32];
		CFAbsoluteTime	currentTime = CFAbsoluteTimeGetCurrent();
		
		sprintf( timeStamp, "%f", currentTime );
		CFStringRef 	currentTimeRef = CFStringCreateWithCString(NULL, timeStamp, kCFStringEncodingUTF8);

		isBadLMB = (CFStringCompare( currentTimeRef, (CFStringRef)CFDictionaryGetValue(mListOfBadLMBs, lmbNameRef), kCFCompareNumerically ) == kCFCompareLessThan);

		if ( !isBadLMB )
		{
			char		lmbNameStr[32] = {0,};
			
			CFStringGetCString( lmbNameRef, lmbNameStr, sizeof(lmbNameStr), kCFStringEncodingUTF8 );
			syslog( LOG_INFO, "LMBDiscoverer::IsLMBOnBadList is giving lmb (%s) another chance\n", lmbNameStr );
			
			CFDictionaryRemoveValue( mListOfBadLMBs, lmbNameRef );
		}
		
		CFRelease( currentTimeRef );
	}
	
	UnLockBadLMBList();
	
	return isBadLMB;
}

void LMBDiscoverer::MarkLMBAsBad( CFStringRef lmbNameRef )
{
	LockBadLMBList();
	
	if ( !mListOfBadLMBs )
		mListOfBadLMBs = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	if ( !CFDictionaryContainsKey( mListOfBadLMBs, lmbNameRef ) )
	{
		char			timeStamp[32];
		CFAbsoluteTime	timeToNextCheckLMB = CFAbsoluteTimeGetCurrent() + kMinTimeToRecheckLMB;
		
		sprintf( timeStamp, "%f", timeToNextCheckLMB );
		CFStringRef 	timeToNextCheckLMBRef = CFStringCreateWithCString(NULL, timeStamp, kCFStringEncodingUTF8);

		CFDictionaryAddValue( mListOfBadLMBs, lmbNameRef, timeToNextCheckLMBRef );
		
		CFRelease( timeToNextCheckLMBRef );
	}
	
	UnLockBadLMBList();
}

#pragma mark -

Boolean LMBDiscoverer::IsLMBKnown( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	DBGLOG( "LMBDiscoverer::IsLMBKnown called\n" );
	Boolean		isLMBKnown = false;
	
	if ( lmbNameRef && !IsLMBOnBadList( lmbNameRef ) )
	{
		CFArrayRef	listOfLMBs = CreateCopyOfLMBsInProgress( workgroupRef, true );		// passing in true so that we don't start any searches
		

		if ( listOfLMBs && IsStringInArray( lmbNameRef, listOfLMBs ) )
			isLMBKnown = true;

		if ( listOfLMBs )
			CFRelease( listOfLMBs );
	}
	
	return isLMBKnown;
}

CFStringRef LMBDiscoverer::CopyOfCachedLMBForWorkgroup( CFStringRef workgroupRef )
{
	LockAllKnownLMBs();
	DBGLOG( "LMBDiscoverer::CopyOfCachedLMBForWorkgroup called\n" );
	CFArrayRef		lmbsForWorkgroup = NULL;
	CFStringRef		lmbCopy = NULL;

	if ( mAllKnownLMBs && workgroupRef )
	{
		lmbsForWorkgroup = (CFArrayRef)CFDictionaryGetValue( mAllKnownLMBs, workgroupRef );

		if ( lmbsForWorkgroup && CFArrayGetCount( lmbsForWorkgroup ) > 0 )
		{
			lmbCopy = (CFStringRef)CFArrayGetValueAtIndex( lmbsForWorkgroup, 0 );
			if ( lmbCopy )
				CFRetain( lmbCopy );
		}
	}
	
	
	if ( !lmbCopy && workgroupRef )
	{
		DBGLOG( "LMBDiscoverer::CopyOfCachedLMBForWorkgroup, no known lmbs cached, go with the currently found LMBs in progress\n" );
		lmbsForWorkgroup = CreateCopyOfLMBsInProgress( workgroupRef, false );		// ok to go ahead and hit the network
		
		if ( lmbsForWorkgroup && CFArrayGetCount( lmbsForWorkgroup ) > 0 )
		{
			lmbCopy = (CFStringRef)CFArrayGetValueAtIndex( lmbsForWorkgroup, 0 );
			if ( lmbCopy )
				CFRetain( lmbCopy );
		}
		
		if ( lmbsForWorkgroup )
			CFRelease( lmbsForWorkgroup );
	}
	
	UnLockAllKnownLMBs();
	
	return lmbCopy;
}

CFArrayRef LMBDiscoverer::CopyBroadcastResultsForLMB( CFStringRef workgroupRef )
{
	// ok, if we have this cached (mOurLMBs) we just return that unless the forceLookup param is set
	// in that case we will get the LMB and save it.
	CFMutableArrayRef	ourLMBsCopy = NULL;
	
	DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB called\n" );

	if ( workgroupRef )
	{
		char*		resultPtr = NULL;
		char*		curPtr = NULL;
		char*		curResult = NULL;
		char*		broadcastAddress = CopyBroadcastAddress();
		const char*	argv[6] = {0};
		char		workgroup[256] = {0,};
		Boolean		canceled = false;
		
		if ( broadcastAddress )
		{
			CFStringGetCString( workgroupRef, workgroup, sizeof(workgroup), GetWindowsSystemEncodingEquivalent() );

			DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB, using Broadcast address: %s\n", broadcastAddress );
			argv[0] = "/usr/bin/nmblookup";
			argv[1] = "-M";
			argv[2] = workgroup;
			argv[3] = "-B";
			argv[4] = broadcastAddress;
		
			if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
			{
				DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB nmblookup -M %s -B %s failed\n", workgroup, broadcastAddress );
			}
			else if ( resultPtr )
			{
				DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB, resultPtr = 0x%lx\n", (UInt32)resultPtr );
				DBGLOG( "%s\n", resultPtr );
				
	//			if ( !ExceptionInResult(resultPtr) )
				{        
					curPtr = resultPtr;
					curResult = curPtr;
					
					while( (curResult = GetNextMasterBrowser(&curPtr)) != NULL )	// GetNextMasterBrowser creates memory
					{
						if ( !ourLMBsCopy )
							ourLMBsCopy = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
						
						CFStringRef		curLMBRef = CFStringCreateWithCString( NULL, curResult, GetWindowsSystemEncodingEquivalent() );
//						CFStringRef		curLMBRef = CFStringCreateWithCString( NULL, curResult, kCFStringEncodingUTF8 );
						
						if ( curLMBRef && !IsStringInArray( curLMBRef, ourLMBsCopy ) ) 
						{
							DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB, adding %s to our array of LMBs\n", curResult );
							CFArrayAppendValue( ourLMBsCopy, curLMBRef );
							AddToCachedResults( workgroupRef, curLMBRef );	// adding workgroup as key, lmb as value
						}

						if ( curLMBRef )
							CFRelease( curLMBRef );
							
						free( curResult );
					}

					DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB finished reading\n" );
				}
				
				free( resultPtr );
				resultPtr = NULL;
			}
			
			free( broadcastAddress );
		}
	}
	else
	{
		DBGLOG( "LMBDiscoverer::CopyBroadcastResultsForLMB, no broadcast address skipping lookup\n" );
	}
	
	return ourLMBsCopy;
}
CFArrayRef LMBDiscoverer::CreateCopyOfLMBsInProgress( CFStringRef workgroupRef, Boolean onlyCheckLMBsInProgress )
{
	LockLMBsInProgress();
	DBGLOG( "LMBDiscoverer::CreateCopyOfLMBsInProgress called\n" );
	CFArrayRef		lmbsForWorkgroup = NULL;
	CFArrayRef		lmbsCopy = NULL;

	if ( mListOfLMBsInProgress && workgroupRef )
	{
		lmbsForWorkgroup = (CFArrayRef)CFDictionaryGetValue( mListOfLMBsInProgress, workgroupRef );

		if ( lmbsForWorkgroup )
			lmbsCopy = CFArrayCreateCopy( NULL, lmbsForWorkgroup );		// incase this changes		
	}
	
	UnLockLMBsInProgress();

	if ( !lmbsCopy && !onlyCheckLMBsInProgress )
	{
		// ok, lets do a broadcast and see who is supposed to be the LMB for this workgroup.
		lmbsCopy = CopyBroadcastResultsForLMB( workgroupRef );
	}
	
	return lmbsCopy;
}

void LMBDiscoverer::AddToCachedResults( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	LockLMBsInProgress();
	DBGLOG( "LMBDiscoverer::AddToCachedResults called\n" );
	
	if ( !mListOfLMBsInProgress )
	{
		mListOfLMBsInProgress = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
	
	if ( mListOfLMBsInProgress )
	{
		DBGLOG( "LMBDiscoverer::AddToCachedResults we are doing aggressive LMB discover (timely)\n" );
		// if we are doing aggressive discovery, we'll keep track of all LMB's per workgroup.  So every entry in our mListOfLMBsInProgress dictionary is a CFArray
		// of LMBs
		CFMutableArrayRef		lmbsForWorkgroup = (CFMutableArrayRef)CFDictionaryGetValue( mListOfLMBsInProgress, workgroupRef );
		
		if ( !lmbsForWorkgroup )
		{
			lmbsForWorkgroup = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			
			if ( lmbsForWorkgroup )
			{
				CFDictionaryAddValue( mListOfLMBsInProgress, workgroupRef, lmbsForWorkgroup );
				CFRelease( lmbsForWorkgroup );		// since Dictionary has retained this
			}
		}

		if ( lmbsForWorkgroup && !IsStringInArray( lmbNameRef, lmbsForWorkgroup ) )
		{
			CFArrayRemoveAllValues( lmbsForWorkgroup );			// we are only going to keep the latest
			CFArrayAppendValue( lmbsForWorkgroup, lmbNameRef );
		}

		AddNode( workgroupRef );
	}
	
	UnLockLMBsInProgress();
}

CFArrayRef LMBDiscoverer::CreateListOfLMBs( void )
{
	// ok, if we have this cached (mOurLMBs) we just return that unless the forceLookup param is set
	// in that case we will get the LMB and save it.
	CFMutableArrayRef	ourLMBsCopy = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	
	DBGLOG( "LMBDiscoverer::CreateListOfLMBs called\n" );

	char*		resultPtr = NULL;
	char*		curPtr = NULL;
	char*		curResult = NULL;
	char*		broadcastAddress = CopyBroadcastAddress();
	const char*	argv[6] = {0};
	Boolean		canceled = false;
	
	argv[0] = "/usr/bin/nmblookup";
	argv[1] = "-M";

	if ( broadcastAddress )
	{
		DBGLOG( "LMBDiscoverer::CreateListOfLMBs, using Broadcast address: %s\n", broadcastAddress );
		argv[2] = "-B";
		argv[3] = broadcastAddress;
		argv[4] = "--";
		argv[5] = "-.";
	
		if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
		{
			DBGLOG( "LMBDiscoverer::CreateListOfLMBs nmblookup -M - failed\n" );
		}
		else if ( resultPtr )
		{
			DBGLOG( "LMBDiscoverer::CreateListOfLMBs, resultPtr = 0x%lx\n", (UInt32)resultPtr );
			DBGLOG( "%s\n", resultPtr );
			
//			if ( !ExceptionInResult(resultPtr) )
			{        
				curPtr = resultPtr;
				curResult = curPtr;
				
				while( (curResult = GetNextMasterBrowser(&curPtr)) != NULL )	// GetNextMasterBrowser creates memory
				{
						CFStringRef		curLMBRef = CFStringCreateWithCString( NULL, curResult, GetWindowsSystemEncodingEquivalent() );
//					CFStringRef		curLMBRef = CFStringCreateWithCString( NULL, curResult, kCFStringEncodingUTF8 );
					
					if ( curLMBRef && !IsStringInArray( curLMBRef, ourLMBsCopy ) ) 
					{
						DBGLOG( "LMBDiscoverer::CreateListOfLMBs, adding %s to our array of LMBs\n", curResult );
						CFArrayAppendValue( ourLMBsCopy, curLMBRef );
						CFRelease( curLMBRef );
					}
					
					free( curResult );
				}
	
				DBGLOG( "LMBDiscoverer::CreateListOfLMBs finished reading\n" );
			}
			
			free( resultPtr );
			resultPtr = NULL;
		}

		free( broadcastAddress );
	}
	else
	{
		DBGLOG( "LMBDiscoverer::CreateListOfLMBs, no broadcast address skipping lookup\n" );
	}
		
	return ourLMBsCopy;
}

char* LMBDiscoverer::GetNextMasterBrowser( char** buffer )
{
	if ( !buffer || !(*buffer) )
		return NULL;
		
	long	addrOfMasterBrowser;
	char*	nextLine = strstr( *buffer, "\n" );
	char*	masterBrowser = NULL;
	char	testString[1024];
	char*	curPtr = *buffer;
	
	DBGLOG( "LMBDiscoverer::GetNextMasterBrowser, parsing %s\n", *buffer );
	while ( !masterBrowser )
	{
		if ( nextLine )
		{
			*nextLine = '\0';
			nextLine++;
		}
		
		if ( sscanf( curPtr, "%s", testString ) )
		{
			DBGLOG( "LMBDiscoverer::GetNextMasterBrowser, testing \"%s\" to see if its an IPAddress\n", testString );
			if ( IsIPAddress(testString, &addrOfMasterBrowser) )
			{
				DBGLOG( "LMBDiscoverer::GetNextMasterBrowser, testing \"%s\" to see if its an IPAddress\n", testString );
				masterBrowser = (char*)malloc(strlen(testString)+1);
				sprintf( masterBrowser, testString );
				
				*buffer = nextLine;
				if ( *buffer )
					*buffer++;
				break;
			}
		}
		
		curPtr = nextLine;
		if ( curPtr )
		{
			nextLine = strstr( curPtr, "\n" );	// look for next line
			DBGLOG( "LMBDiscoverer::GetNextMasterBrowser, try next line: %s\n", curPtr );
		}
		else
			break;
	}

	return masterBrowser;
}

#pragma mark -
LMBInterrogationThread::LMBInterrogationThread()
{
	mLMBToInterrogate = NULL;
	mDiscoverer = NULL;
}

LMBInterrogationThread::~LMBInterrogationThread()
{
	if ( mLMBToInterrogate )
		CFRelease( mLMBToInterrogate );
	
	mLMBToInterrogate = NULL;

	if ( mWorkgroup )
		CFRelease( mWorkgroup );

	mWorkgroup = NULL;

	if ( mDiscoverer )
		mDiscoverer->ThreadFinished();
}

void LMBInterrogationThread::Initialize( LMBDiscoverer* discoverer, CFStringRef lmbToInterrogate, CFStringRef workgroup )
{
	mLMBToInterrogate = lmbToInterrogate;
	
	if ( mLMBToInterrogate )
		CFRetain( mLMBToInterrogate );
	
	mWorkgroup = workgroup;
	
	if ( mWorkgroup )
		CFRetain( mWorkgroup );
	
	mDiscoverer = discoverer;
}

void* LMBInterrogationThread::Run( void )
{
	char	lmb[256];
	CFStringGetCString( mLMBToInterrogate, lmb, sizeof(lmb), kCFStringEncodingUTF8 );
	
#ifdef TOOL_LOGGING
	double	inTime		= 0;
	double	outTime		= 0;
	inTime = dsTimestamp();

	printf( "Starting query on LMB %s\n", lmb );
#endif
	if ( mLMBToInterrogate && mDiscoverer )
	{
		mDiscoverer->ThreadStarted();
		CFArrayRef lmbResults = GetLMBInfoFromLMB( mWorkgroup, mLMBToInterrogate );
	
		if ( lmbResults )
		{
			if ( mWorkgroup )
			{
				mDiscoverer->AddToCachedResults( mWorkgroup, mLMBToInterrogate );	// adding workgroup as key, lmb as value
	
				if ( !mDiscoverer->IsInitialSearch() )
				{
					// now recursively search all LMBs that this LMB knows as well!
					for ( CFIndex i=CFArrayGetCount(lmbResults)-2; i>=0; i-=2 )
					{
						CFStringRef		workgroupRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, i);
						CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, i+1);
						
						if ( workgroupRef && CFStringGetLength( workgroupRef ) > 0 && lmbNameRef && CFStringGetLength( lmbNameRef ) > 0 && !mDiscoverer->IsLMBKnown( workgroupRef, lmbNameRef ) )		// do we already know about this workgroup?
							InterrogateLMB( workgroupRef, lmbNameRef );		// recursively check this guy out
					}
				}
			}
			else
			{
				for ( CFIndex j=CFArrayGetCount(lmbResults)-2; j>=0; j-=2 )
				{
					CFStringRef		workgroupRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, j);
					CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, j+1);
					
					if ( workgroupRef && CFStringGetLength( workgroupRef ) > 0 && lmbNameRef && CFStringGetLength( lmbNameRef ) > 0 && !mDiscoverer->IsLMBKnown( workgroupRef, lmbNameRef ) )		// do we already know about this workgroup?
						InterrogateLMB( workgroupRef, lmbNameRef );		// if this is valid, add it to our list
				}
			}

			CFRelease( lmbResults );
		}
	}
	
#ifdef TOOL_LOGGING
	outTime = dsTimestamp();
	printf( "Finished query on LMB %s, Duration: %.2f sec\n", lmb, (outTime - inTime) );
#endif
	return NULL;
}

void LMBInterrogationThread::InterrogateLMB( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	LMBInterrogationThread*		interrogator = new LMBInterrogationThread();
	interrogator->Initialize( mDiscoverer, lmbNameRef, workgroupRef );
	
	interrogator->Resume();
}

CFArrayRef GetLMBInfoFromLMB( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
    char*		resultPtr = NULL;
	const char*	argv[9] = {0};
	char		lmbName[256] = {0,};
    Boolean		canceled = false;
	CFArrayRef	lmbResults = NULL;
	
	DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB called\n" );
	
	if ( lmbNameRef )
	{
		char		workgroup[256] = {0,};

		CFStringGetCString( lmbNameRef, lmbName, sizeof(lmbName), kCFStringEncodingUTF8 );
			
		if ( workgroupRef )
		{
			CFStringGetCString( workgroupRef, workgroup, sizeof(workgroup), GetWindowsSystemEncodingEquivalent() );
			
			argv[0] = "/usr/bin/smbclient";
			argv[1] = "-W";
			argv[2] = workgroup;
			argv[3] = "-NL";
			argv[4] = lmbName;
			argv[5] = "-U%";
			argv[6] = "-s";
			argv[7] = kBrowsingConfFilePath;

			DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB calling smbclient -W %s -NL %s -U\n", workgroup, lmbName );
		}
		else
		{
			argv[0] = "/usr/bin/smbclient";
			argv[1] = "-NL";
			argv[2] = lmbName;
			argv[3] = "-U%";
			argv[4] = "-s";
			argv[5] = kBrowsingConfFilePath;

			DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB calling smbclient -NL %s -U\n", lmbName );
		}
		
		if ( myexecutecommandas( NULL, "/usr/bin/smbclient", argv, false, kLMBGoodTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
		{
			DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB smbclient -W %s -NL %s -U failed\n", workgroup, lmbName );
		}
		else if ( resultPtr )
		{
			DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB, resultPtr = 0x%lx, length = %ld\n", (UInt32)resultPtr, strlen(resultPtr) );
			DBGLOG( "%s\n", resultPtr );
			
			if ( !ExceptionInResult(resultPtr) )
			{
				lmbResults = ParseOutStringsFromSMBClientResult( resultPtr, "Workgroup", "Master" );
			}
			else
			{
				DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB found an Exception in the result for smbclient -W %s -NL %s -U\n", workgroup, lmbName );
			}
			
			free( resultPtr );
			resultPtr = NULL;
		}
		else
			DBGLOG( "LMBDiscoverer::GetLMBInfoFromLMB resultPtr is NULL!\n" );
		
	}
	
	return lmbResults;
}

#pragma mark -
CFArrayRef ParseOutStringsFromSMBClientResult( char* smbClientResult, char* primaryKey, char* secondaryKey, char** outPtr )
{
	if ( !smbClientResult || !primaryKey || !secondaryKey )
		return NULL;
		
	char*				curPtr = NULL;
	char*				primaryKeyFoundPtr = NULL;
	char*				secondaryKeyFoundPtr = NULL;
	char*				eoResult = smbClientResult + strlen(smbClientResult);
	CFMutableArrayRef	arrayOfResults = NULL;
	
	DBGLOG( "ParseOutStringsFromSMBClientResult called\n" );

	primaryKeyFoundPtr = strstr( smbClientResult, primaryKey );
	
	if ( outPtr )
		*outPtr = smbClientResult;
		
	if ( primaryKeyFoundPtr )
		secondaryKeyFoundPtr = strstr( primaryKeyFoundPtr, secondaryKey );

	// we are looking for text matching the primaryKey and it should be on the same line as the secondaryKey
	// if not, then we should find the next match for the primary etc.
	curPtr = primaryKeyFoundPtr;

	while ( primaryKeyFoundPtr && secondaryKeyFoundPtr )
	{
		// check to make sure there are no new lines between these two
		while ( curPtr < secondaryKeyFoundPtr )
		{
			if ( *curPtr == '\n' )
			{
				primaryKeyFoundPtr = strstr( (primaryKeyFoundPtr + strlen(primaryKey)), primaryKey );	// look for next instance
	
				if ( primaryKeyFoundPtr )
					secondaryKeyFoundPtr = strstr( primaryKeyFoundPtr, secondaryKey );
				else
					return NULL;
					
				curPtr = primaryKeyFoundPtr;

				break;	// break out to outer loop to try again
			}
			else
				curPtr++;
		}
		
		if ( curPtr != primaryKeyFoundPtr )
			break;		//ok to break out
	}

	if ( primaryKeyFoundPtr && secondaryKeyFoundPtr )
	{
		// ok, so we should be pointing at the primary and secondary keys that are on the same line
		// with the next line being a bunch of underlines
		// i.e. "Workgroup            Master"
		//      "---------            -------"
		
		// we want to grab the tupled values following this until we reach a double line feed "\n\n"
		// but we want to discard the first set with the underlines
		int		numCharsToPrimary=0, numCharsToSecondary=0;
		char*	eoLine = NULL;
		char*	curPrimaryResult = NULL;
		char*	curSecondaryResult = NULL;
		
		curPtr = primaryKeyFoundPtr;
		while ( *curPtr != '\n' && curPtr > smbClientResult )
			curPtr--;
		
		numCharsToPrimary = primaryKeyFoundPtr - curPtr;
		numCharsToSecondary = secondaryKeyFoundPtr - curPtr;
		
		curPtr = secondaryKeyFoundPtr;
		while ( *curPtr != '\n' && curPtr < eoResult )
			curPtr++;
		
		if ( *curPtr == '\n' )
			curPtr++;
		// now we know what the offset is for each line.
		while ( curPtr && curPtr <= eoResult && *curPtr != '\n' && *curPtr != '\0' )
		{
			curPrimaryResult = curPtr + numCharsToPrimary -1;
			curSecondaryResult = curPtr + numCharsToSecondary -1;
			
			while ( *curSecondaryResult == '\n' )	// If there is no secondary field, this can be shorter. Back up
				curSecondaryResult--;
				
			eoLine = curPtr;
			while ( *eoLine != '\n' && *eoLine != '\0' )
				eoLine++;
			
			*eoLine	= '\0';
			
			// we should have a pointer to our primary string, our secondary string and the end of line
			if ( eoLine > curPrimaryResult && eoLine >= curSecondaryResult )	// sanity check
			{
				curSecondaryResult[-1] = '\0';	// help with parsing
				DBGLOG( "ParseOutStringsFromSMBClientResult( %s, %s ) looking at (%s) and (%s)\n", primaryKey, secondaryKey, curPrimaryResult, curSecondaryResult );
				
				char*	trimPtr = curSecondaryResult-2;
				while ( isspace( *trimPtr ) )
					*trimPtr-- = '\0';

				DBGLOG( "ParseOutStringsFromSMBClientResult( %s, %s ) looking at (%s) and (%s) after trim\n", primaryKey, secondaryKey, curPrimaryResult, curSecondaryResult );

				if ( curPrimaryResult && curSecondaryResult && strncmp( curPrimaryResult, "-----", 5 ) != 0 )
				{

					if ( !arrayOfResults )
						arrayOfResults = ::CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );

					CFStringRef		primaryRef = CFStringCreateWithCString( NULL, curPrimaryResult, GetWindowsSystemEncodingEquivalent() );
//					CFStringRef		primaryRef = CFStringCreateWithCString( NULL, curPrimaryResult, kCFStringEncodingUTF8 );
					CFStringRef		secondaryRef = CFStringCreateWithCString( NULL, curSecondaryResult, GetWindowsSystemEncodingEquivalent() );
//					CFStringRef		secondaryRef = CFStringCreateWithCString( NULL, curSecondaryResult, kCFStringEncodingUTF8 );
					
					if ( primaryRef && secondaryRef )
					{
						CFArrayAppendValue( arrayOfResults, primaryRef );						
						CFArrayAppendValue( arrayOfResults, secondaryRef );
					}
					else
						syslog( LOG_ALERT, "ParseOutStringsFromSMBClientResult, couldn't create CFStrings!  Encoding:%d, primary:%s, secondary:%s", GetWindowsSystemEncodingEquivalent(), curPrimaryResult, curSecondaryResult );
//						DBGLOG( "ParseOutStringsFromSMBClientResult, couldn't create CFStrings!  Encoding:%d, primary:%s, secondary:%s", GetWindowsSystemEncodingEquivalent(), curPrimaryResult, curSecondaryResult );
						
					if ( primaryRef )
						CFRelease( primaryRef );
						
					if ( secondaryRef )
						CFRelease( secondaryRef );
				}				
			}
			else
			{
				DBGLOG( "ParseOutStringsFromSMBClientResult( %s, %s ) failed its check parsing the line:\n%s\n", primaryKey, secondaryKey, curPtr );
				break;
			}
			
			curPtr = eoLine+1;
		}

		if ( outPtr )
			*outPtr = curPtr;
	}
	else
		DBGLOG( "ParseOutStringsFromSMBClientResult couldn't find the primary (%s) and secondary (%s) keys, returning no results\n", primaryKey, secondaryKey );
	
	return arrayOfResults;
}

Boolean ExceptionInResult( const char* resultPtr )
{
	Boolean		exceptionInResult = true; 		// assume its bad

	if ( strstr(resultPtr, "Anonymous login successful") )
		exceptionInResult = false;
	else if ( strstr(resultPtr, "Got a positive name query response") )
		exceptionInResult = false;
	else if ( strstr(resultPtr, "Workgroup") && strstr(resultPtr, "Master") )
		exceptionInResult = false;
	else if ( strstr(resultPtr, "Server") && strstr(resultPtr, "Comment") )
		exceptionInResult = false;
	else if ( strstr(resultPtr, "NT_STATUS_ACCESS_DENIED") )
		exceptionInResult = false;

	return exceptionInResult;
}

/***************
 * IsIPAddress *
 ***************
 
 Verifies a CString is a legal dotted-quad format. If it fails, it returns the 
 partial IP address that was collected.
 
*/

int IsIPAddress(const char* adrsStr, long *ipAdrs)
{
	short	i,accum,numOctets,lastDotPos;
	long	tempAdrs;
	register char	c;
	char	localCopy[20];					// local copy of the adrsStr
	
	strncpy(localCopy, adrsStr,sizeof(localCopy)-1);
	*ipAdrs = tempAdrs = 0;
	numOctets = 1;
	accum = 0;
	lastDotPos = -1;
	for (i = 0; localCopy[i] != 0; i++)	{	// loop 'til it hits the NUL
		c = localCopy[i];					// pulled this out of the comparison part of the for so that it is more obvious	// KA - 5/29/97
		if (c == '.')	{
			if (i - lastDotPos <= 1)	return 0;	// no digits
			if (accum > 255) 			return 0;	// only 8 bits, guys
			*ipAdrs = tempAdrs = (tempAdrs<<8) + accum; // copy back result so far
			accum = 0; 
			lastDotPos = i;							
			numOctets++;								// bump octet counter
		}
		else if ((c >= '0') && (c <= '9'))	{
			accum = accum * 10 + (c - '0');				// [0-9] is OK
		}
		else return 0;								// bogus character
	}
	
	if (accum > 255) return 0;						// if not too big...
	tempAdrs = (tempAdrs<<8) + accum;					// add in the last byte
	*ipAdrs = tempAdrs;									// return real IP adrs

	if (numOctets != 4)									// if wrong count
		return 0;									// 	return FALSE;
	else if (i-lastDotPos <= 1)							// if no last byte
		return 0;									//  return FALSE
	else	{											// if four bytes
		return 1;									// say it worked
	}
}

/*************
 * IsDNSName *
 *************
 
 Verify a CString is a valid dot-format DNS name. Check that:
 
	the name only contains letters, digits, hyphens, and dots; 
	no label begins or ends with a "-";
	the name doesn't begin with ".";
	labels are between 1 and 63 characters long;
	the entire length isn't > 255 characters;
	allow "." as a legal name
	will NOT allow an all-numeric name (ie, 1.2.3.4 will fail)
 .	must have at least ONE dot
*/

Boolean IsDNSName(char* theName)
{
	short	i;
	short	len;
	short	lastDotPos;
	short	lastDashPos;
	register char 	c;
	Boolean seenAlphaChar;
	
	if ( !strstr(theName, ".") )
		return FALSE;
		
	if ((strlen(theName) == 1) && (theName[0] == '.')) return TRUE;	// "." is legal...
	len = 0; 
	lastDotPos = -1;					// just "before" the start of string
	lastDashPos = -1;				
	seenAlphaChar = FALSE;
	for (i = 0; c = theName[i]; i++, len++)	{
	
		if (len > 255)	return FALSE;	// whole name is too long
		
		if (c == '-')	{
			if (lastDotPos == i-1) return FALSE;	// no leading "-" in labels 
			lastDashPos = i;
		}
		else if (c == '.')	{			// check label lengths
			if (lastDashPos == i-1)	 	 return FALSE; // trailing "-" in label
			if (i - lastDotPos - 1 > 63) return FALSE; // label too long
			if (i - lastDotPos <= 1) 	 return FALSE; // zero length label
			lastDotPos = i;
		}		
		else if (isdigit(c))	{ 		// any numeric chars are OK
			// nothing
		}
		else if (isalpha(c))	{ 		// lower or upper case, too.
			seenAlphaChar = TRUE;
		}
		else return FALSE;				// but nothing else
	}
	return seenAlphaChar;
}

Boolean IsStringInArray( CFStringRef theString, CFArrayRef theArray )
{
	Boolean		isInArray = false;
	
	if ( theArray && theString && CFArrayGetCount( theArray ) > 0 )
	{
		for ( CFIndex i=CFArrayGetCount(theArray)-1; i>=0; i-- )
		{
			if ( CFStringCompare( theString, (CFStringRef)CFArrayGetValueAtIndex( theArray, i ), 0 ) == kCFCompareEqualTo )
			{
				isInArray = true;
				break;
			}
		}
	}
	
	return isInArray;
}

const char* GetCodePageStringForCurrentSystem( void )
{
	// So we want to try and map our Mac System encoding to what the equivalent windows code page
	CFStringEncoding		encoding = NSLGetSystemEncoding();		// use our version due to bug where CFGetSystemEncoding doesn't work for boot processes
	const char*				codePageStr = "CP437";					// United States
	
	switch ( encoding )
	{
		case kCFStringEncodingMacCentralEurRoman:
		case kCFStringEncodingMacCroatian:
		case kCFStringEncodingMacRomanian:
			codePageStr = "CP852";
		break;
		
		case kCFStringEncodingMacJapanese:
			codePageStr = "CP932";
		break;
		
		case kCFStringEncodingMacChineseSimp:
			codePageStr = "CP936";
		break;
		
		case kCFStringEncodingMacChineseTrad:
			codePageStr = "CP950";
		break;
		
		case kCFStringEncodingMacKorean:
			codePageStr = "CP949";
		break;
		
		case kCFStringEncodingMacGreek:
			codePageStr = "CP737";
		break;
		
		case kCFStringEncodingMacCyrillic:
		case kCFStringEncodingMacUkrainian:
			codePageStr = "CP855";
		break;
		
		case kCFStringEncodingMacIcelandic:
			codePageStr = "CP861";
		break;

		case kCFStringEncodingMacArabic:
		case kCFStringEncodingMacFarsi:
			codePageStr = "CP864";
		break;
		
		case kCFStringEncodingMacHebrew:
			codePageStr = "CP862";
		break;
		
		case kCFStringEncodingMacThai:
			codePageStr = "CP874";
		break;
		
		case kCFStringEncodingMacTurkish:
			codePageStr = "CP857";
		break;
	}
	
	return codePageStr;
}

Boolean IsSafeToConnectToAddress( char* address )
{
	Boolean						safeToConnect = false;
	SCNetworkConnectionFlags	connectionFlags;
	struct sockaddr_in			addr;
	
	DBGLOG( "SafeToConnectToAddress checking Address: %s\n", address );

	::memset( &addr, 0, sizeof(addr) );
	addr.sin_family			= AF_INET;
	addr.sin_addr.s_addr	= htonl( inet_addr(address) );
	
	SCNetworkCheckReachabilityByAddress( (struct sockaddr*)&addr, sizeof(addr), &connectionFlags );
	
	if (connectionFlags & kSCNetworkFlagsReachable)
		DBGLOG( "SafeToConnectToAddress, flag: kSCNetworkFlagsReachable\n" );
	else
		DBGLOG( "SafeToConnectToAddress, flag: !kSCNetworkFlagsReachable\n" );
	
	if (connectionFlags & kSCNetworkFlagsConnectionRequired)
		DBGLOG( "SafeToConnectToAddress, flag: kSCNetworkFlagsConnectionRequired\n" );
	else
		DBGLOG( "SafeToConnectToAddress, flag: !kSCNetworkFlagsConnectionRequired\n" );
	
	if (connectionFlags & kSCNetworkFlagsTransientConnection)
		DBGLOG( "SafeToConnectToAddress, flag: kSCNetworkFlagsTransientConnection\n" );
	else
		DBGLOG( "SafeToConnectToAddress, flag: !kSCNetworkFlagsTransientConnection\n" );
	
	if ( (connectionFlags & kSCNetworkFlagsReachable) && !(connectionFlags & kSCNetworkFlagsConnectionRequired) && !(connectionFlags & kSCNetworkFlagsTransientConnection) )
	{
		DBGLOG( "SafeToConnectToAddress found address reachable w/o dialup required\n" );
		safeToConnect = true;
	}
	else
	{
		DBGLOG( "SafeToConnectToAddress found address not reachable w/o dialup being initiated, ignore\n" );
	}
	
	return safeToConnect;
}

#ifdef TOOL_LOGGING
double dsTimestamp(void)
{
	static uint32_t	num		= 0;
	static uint32_t	denom	= 0;
	uint64_t		now;
	
	if (denom == 0) 
	{
		struct mach_timebase_info tbi;
		kern_return_t r;
		r = mach_timebase_info(&tbi);
		if (r != KERN_SUCCESS) 
		{
			syslog( LOG_ALERT, "Warning: mach_timebase_info FAILED! - error = %u\n", r);
			return 0;
		}
		else
		{
			num		= tbi.numer;
			denom	= tbi.denom;
		}
	}
	now = mach_absolute_time();
	
	return (double)(now * (double)num / denom / NSEC_PER_SEC);	// return seconds
//	return (double)(now * (double)num / denom / NSEC_PER_USEC);	// return microsecs
//	return (double)(now * (double)num / denom);	// return nanoseconds
}
#endif

CFStringEncoding GetWindowsSystemEncodingEquivalent( void )
{
	// So we want to try and map our Mac System encoding to what the equivalent windows encoding would be on the
	// same network  (i.e. kCFStringEncodingMacJapanese to kCFStringEncodingShiftJIS)

// mappings sent to me from Nathan (from Aki)
/*
kCFStringEncodingMacRoman			kCFStringEncodingDOSLatinUS or kCFStringEncodingDOSLatin1
kCFStringEncodingMacJapanese			kCFStringEncodingDOSJapanese
kCFStringEncodingMacChineseTrad		kCFStringEncodingDOSChineseTrad
kCFStringEncodingMacKorean			kCFStringEncodingDOSKorean
kCFStringEncodingMacArabic				kCFStringEncodingDOSArabic
kCFStringEncodingMacHebrew			kCFStringEncodingDOSHebrew
kCFStringEncodingMacGreek				kCFStringEncodingDOSGreek
kCFStringEncodingMacCyrillic				kCFStringEncodingDOSCyrillic
kCFStringEncodingMacThai				kCFStringEncodingDOSThai
kCFStringEncodingMacChineseSimp		kCFStringEncodingDOSChineseSimplif
kCFStringEncodingMacCentralEurRoman	kCFStringEncodingDOSLatin2
kCFStringEncodingMacTurkish			kCFStringEncodingDOSTurkish
kCFStringEncodingMacCroatian			kCFStringEncodingDOSLatin2
kCFStringEncodingMacIcelandic			kCFStringEncodingDOSIcelandic
kCFStringEncodingMacRomanian			kCFStringEncodingDOSLatin2
kCFStringEncodingMacFarsi				kCFStringEncodingDOSArabic
kCFStringEncodingMacUkrainian			kCFStringEncodingDOSCyrillic
*/
	CFStringEncoding	encoding = NSLGetSystemEncoding();		// use our version due to bug where CFGetSystemEncoding doesn't work for boot processes
	
	switch ( encoding )
	{
		case	kCFStringEncodingMacRoman:
			encoding = kCFStringEncodingISOLatin1;
		break;
		
		case kCFStringEncodingMacJapanese:
// according to testing the ShiftJIS was working better...
//			encoding = kCFStringEncodingDOSJapanese;
			encoding = kCFStringEncodingShiftJIS;
		break;
		
		case kCFStringEncodingMacChineseSimp:
			encoding = kCFStringEncodingDOSChineseSimplif;
//			encoding = kCFStringEncodingHZ_GB_2312;
		break;
		
		case kCFStringEncodingMacChineseTrad:
//			encoding = kCFStringEncodingDOSChineseTrad;
			encoding = kCFStringEncodingBig5_HKSCS_1999;
		break;
		
		case kCFStringEncodingMacKorean:
			encoding = kCFStringEncodingDOSKorean;
//			encoding = kCFStringEncodingKSC_5601_92_Johab;
		break;
		
		case kCFStringEncodingMacArabic:
			encoding = kCFStringEncodingDOSArabic;
//			encoding = kCFStringEncodingWindowsArabic;
		break;
		
		case kCFStringEncodingMacHebrew:
			encoding = kCFStringEncodingDOSHebrew;
//			encoding = kCFStringEncodingWindowsHebrew;
		break;
		
		case kCFStringEncodingMacGreek:
			encoding = kCFStringEncodingDOSGreek;
//			encoding = kCFStringEncodingWindowsGreek;
		break;
		
		case kCFStringEncodingMacCyrillic:
			encoding = kCFStringEncodingDOSCyrillic;
//			encoding = kCFStringEncodingWindowsCyrillic;
		break;
		
		case kCFStringEncodingMacThai:
			encoding = kCFStringEncodingDOSThai;
		break;
		
		case kCFStringEncodingMacCentralEurRoman:
			encoding = kCFStringEncodingDOSLatin2;
		break;
		
		case kCFStringEncodingMacTurkish:
			encoding = kCFStringEncodingDOSTurkish;
		break;
		
		case kCFStringEncodingMacCroatian:
			encoding = kCFStringEncodingDOSLatin2;
		break;
		
		case kCFStringEncodingMacIcelandic:
			encoding = kCFStringEncodingDOSIcelandic;
		break;
		
		case kCFStringEncodingMacRomanian:
			encoding = kCFStringEncodingDOSLatin2;
		break;
		
		case kCFStringEncodingMacFarsi:
			encoding = kCFStringEncodingDOSArabic;
		break;
		
		case kCFStringEncodingMacUkrainian:
			encoding = kCFStringEncodingDOSCyrillic;
		break;
	}
	
	return encoding;
}

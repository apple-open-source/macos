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
 *  @header LMBDiscoverer
 */

#ifndef _LMBDiscoverer_
#define _LMBDiscoverer_
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>

#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>

#include <stdio.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "NSLDebugLog.h"

#ifdef TOOL_LOGGING
double dsTimestamp(void);
#endif

CFStringEncoding GetWindowsSystemEncodingEquivalent( void );
Boolean ExceptionInResult( const char* resultPtr );
int IsIPAddress(const char* adrsStr, long *ipAdrs);
Boolean IsDNSName(char* theName);

#define	kLMBGoodTimeOutVal			10		// seconds
#define kMinTimeBetweenRetries		15*60	// fifteen minutes
#define kMinTimeToRecheckLMB		15*60	// fifteen minutes?

class LMBInterrogationThread;

class LMBDiscoverer
{
friend class LMBInterrogationThread;

public:
                                LMBDiscoverer				( void );
								~LMBDiscoverer				( void );
    
			sInt32				Initialize				( void );

			void				SetWinsServer			( const char* server ) { mWINSServer = server; }
			const char*			GetWinsServer			( void ) { return mWINSServer; }
			
			void				ResetOurBroadcastAddress( void );
			char*				CopyBroadcastAddress	( void );
			
			void				DisableSearches			( void ) { mCurrentSearchCanceled = true; }
			void				EnableSearches			( void ) { mCurrentSearchCanceled = false; }
			void				ClearLMBCache			( void );
			void				UpdateCachedLMBResults	( void );
			Boolean				ClearLMBForWorkgroup	( CFStringRef workgroupRef, CFStringRef lmbNameRef );
			void				AddToCachedResults		( CFStringRef workgroupRef, CFStringRef lmbNameRef );

			Boolean				IsOKToLookupLMBsAgain		( void ) { return CFAbsoluteTimeGetCurrent() + kMinTimeBetweenRetries > mLastTimeLMBsSearched; }
			void				DiscoverCurrentWorkgroups	( void );
			
			Boolean				IsInitialSearch					( void ) { return mInitialSearch; }
			CFStringRef			CopyOfCachedLMBForWorkgroup		( CFStringRef workgroupRef );
			CFArrayRef			CopyBroadcastResultsForLMB		( CFStringRef workgroupRef );
			CFArrayRef			CreateCopyOfLMBsInProgress		( CFStringRef workgroupRef, Boolean onlyCheckLMBsInProgress = false );
			
			Boolean				IsLMBKnown						( CFStringRef workgroupRef, CFStringRef lmbNameRef );

			void				ClearBadLMBList					( void );
			Boolean				IsLMBOnBadList					( CFStringRef lmbNameRef );
			void				MarkLMBAsBad					( CFStringRef lmbNameRef );
			
			CFDictionaryRef		GetAllKnownLMBs					( void ) { return mAllKnownLMBs; }
			
			void				ThreadStarted					( void ) { mThreadsRunning++; }
			void				ThreadFinished					( void ) { mThreadsRunning--; }
protected: 
			
			sInt32				GetPrimaryInterfaceBroadcastAdrs( char** broadcastAddr );
	
			CFArrayRef			CreateListOfLMBs		( void );

			char*				GetNextMasterBrowser	( char** buffer );

private:
            void					LockNodeState				( void ) { pthread_mutex_lock( &mNodeStateLock ); }
            void					UnLockNodeState				( void ) { pthread_mutex_unlock( &mNodeStateLock ); }
            pthread_mutex_t			mNodeStateLock;
			
			void					LockAllKnownLMBs			( void ) { pthread_mutex_lock( &mAllKnownLMBsLock ); }
            void					UnLockAllKnownLMBs			( void ) { pthread_mutex_unlock( &mAllKnownLMBsLock ); }
            pthread_mutex_t			mAllKnownLMBsLock;

            void					LockLMBsInProgress			( void ) { pthread_mutex_lock( &mListOfLMBsInProgressLock ); }
            void					UnLockLMBsInProgress		( void ) { pthread_mutex_unlock( &mListOfLMBsInProgressLock ); }
            pthread_mutex_t			mListOfLMBsInProgressLock;

            void					LockBadLMBList				( void ) { pthread_mutex_lock( &mLockBadLMBListLock ); }
            void					UnLockBadLMBList			( void ) { pthread_mutex_unlock( &mLockBadLMBListLock ); }
            pthread_mutex_t			mLockBadLMBListLock;

            void					LockOurBroadcastAddress	( void ) { pthread_mutex_lock( &mOurBroadcastAddressLock ); }
            void					UnLockOurBroadcastAddress( void ) { pthread_mutex_unlock( &mOurBroadcastAddressLock ); }
            pthread_mutex_t			mOurBroadcastAddressLock;

			Boolean					mNodeListIsCurrent;
			Boolean					mNodeSearchInProgress;
            char*					mLocalNodeString;		
	const	char*					mWINSServer;
			char*					mBroadcastAddr;
			CFMutableDictionaryRef	mAllKnownLMBs;
			CFMutableDictionaryRef	mListOfLMBsInProgress;
			CFMutableDictionaryRef	mListOfBadLMBs;
			Boolean					mInitialSearch;
			Boolean					mNeedFreshLookup;
			Boolean					mCurrentSearchCanceled;
			int						mThreadsRunning;
			CFAbsoluteTime			mLastTimeLMBsSearched;
};

#include <DirectoryServiceCore/DSLThread.h>

#include "CNSLPlugin.h"

class LMBInterrogationThread : public DSLThread
{
public:
                LMBInterrogationThread			();
    virtual		~LMBInterrogationThread			();
	
			void			Initialize			( LMBDiscoverer* discoverer, CFStringRef lmbToInterrogate, CFStringRef workgroup = NULL );
    
	virtual void*			Run					( void );

			void			InterrogateLMB		( CFStringRef workgroupRef, CFStringRef lmbNameRef );

protected:

private:
		CFStringRef				mLMBToInterrogate;
		CFStringRef				mWorkgroup;
        LMBDiscoverer*			mDiscoverer;
};

CFArrayRef GetLMBInfoFromLMB	( CFStringRef workgroupRef, CFStringRef lmbNameRef );
CFArrayRef ParseOutStringsFromSMBClientResult( char* smbClientResult, char* primaryKey, char* secondaryKey, char** outPtr  = NULL );
Boolean IsStringInArray( CFStringRef theString, CFArrayRef theArray );


#endif

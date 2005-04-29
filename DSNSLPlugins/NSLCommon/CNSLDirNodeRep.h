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
 *  @header CNSLDirNode
 */


#ifndef _CNSLDirNodeRep_
#define _CNSLDirNodeRep_	1

#include <stdio.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CDataBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/CDSServerModule.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>

class CNSLPlugin;
class CNSLServiceLookupThread;
class CNSLResult;

class CNSLDirNodeRep
{
public:
                                CNSLDirNodeRep				( CNSLPlugin* parent, const void* ref );
                                ~CNSLDirNodeRep				( void );
            
			void				DeleteSelf					( void );
            void				Initialize					( CFStringRef nodeNameRef, uid_t uid, Boolean isTopLevelNode, CFStringRef nodePathRef );
            void				Initialize					( const char* nodeNamePtr, uid_t uid, Boolean isTopLevelNode, const char* nodePathPtr = NULL );
            Boolean				IsInitialized				( void ) { return mInitialized; }	// returns whether this object has been initialized
            Boolean				IsLookupStarted				( void ) { return mLookupStarted; }
            void				LookupHasStarted			( void ) { mLookupStarted = true; }
            void				ResetLookupHasStarted		( void ) { mLookupStarted = false; }
            
            CFStringRef			GetNodeName					( void ) { return mNodeName; }
            CFStringRef			GetNodePath					( void ) { return mNodePath; }
			Boolean				IsTopLevelNode				( void ) { return mIsTopLevelNode; }
            uid_t				GetUID						( void ) { return mUID; }
            void				LimitRecSearch				( unsigned long searchLimit ) { mRecSearchLimit = searchLimit; }
            Boolean				HaveResults					( void );
            Boolean				LookupComplete				( void );
            
    const 	CNSLResult*   		GetNextResult				( void );
            void				NeedToRedoLastResult		( void )	{ mCurrentIndex--; }
    const 	CNSLResult*			GetResult					( CFIndex index );		// 0 based

            void				AddService					( CNSLResult* newResult );
			void				AddServices					( CFArrayRef newResults );
            
            void				StartingNewLookup			( CNSLServiceLookupThread* searchThread );
            // service lookup thread calls this when it finishes
            void				ServiceLookupComplete		( CNSLServiceLookupThread* searchThread );		
            
            void				ResultListQueueLock			( void ) { pthread_mutex_lock( &mResultListQueueLock ); }
            void				ResultListQueueUnlock		( void ) { pthread_mutex_unlock( &mResultListQueueLock ); }
            void				SearchListQueueLock			( void ) { pthread_mutex_lock( &mSearchListQueueLock ); }
            void				SearchListQueueUnlock		( void ) { pthread_mutex_unlock( &mSearchListQueueLock ); }
			void				Retain						( void ) { mRefCounter++; }
			void				Release						( void ) { if ( --mRefCounter == 0 ) DeleteSelf(); }
protected:
            
private:
            pthread_mutex_t		mResultListQueueLock;
            pthread_mutex_t		mSearchListQueueLock;
            CFStringRef			mNodeName;
            CFStringRef			mNodePath;
			Boolean				mIsTopLevelNode;
            uid_t				mUID;
            CNSLPlugin*			mParentPlugin;
    const	void*				mRef;						// this corresponds to the key mapping to this lookup
            CFIndex				mRecSearchLimit;
            CFMutableArrayRef	mResultList;	
            CFMutableArrayRef	mSearchList;	
            CFIndex				mCurrentIndex;
            Boolean				mInitialized;
            Boolean				mLookupStarted;
            UInt32				mDelCounter;
			UInt32				mRefCounter;
			CNSLDirNodeRep*		mSelfPtr;
};


#endif // #ifndef

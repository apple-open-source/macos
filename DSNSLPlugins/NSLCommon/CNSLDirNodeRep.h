/*
 *  CNSLDirNode.h
 *  DSSLPPlugIn
 *
 *  Created by imlucid on Mon Aug 20 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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

//#include "CRecNameList.h"
//#include "CRecTypeList.h"

class CNSLPlugin;
class CNSLServiceLookupThread;
class CNSLResult;

class CNSLDirNodeRep
{
public:
                                CNSLDirNodeRep				( CNSLPlugin* parent, const void* ref );
                                ~CNSLDirNodeRep				( void );
            
			void				DeleteSelf					( void );
            void				Initialize					( CFStringRef nodeNameRef, uid_t uid );
            void				Initialize					( const char* nodeNamePtr, uid_t uid );
            Boolean				IsInitialized				( void ) { return mInitialized; }	// returnes whether this object has been initialized
            Boolean				IsLookupStarted				( void ) { return mLookupStarted; }
            void				LookupHasStarted			( void ) { mLookupStarted = true; }
            void				ResetLookupHasStarted		( void ) { mLookupStarted = false; }
            
            CFStringRef			GetNodeName					( void ) { return mNodeName; }
            uid_t				GetUID						( void ) { return mUID; }
            void				LimitRecSearch				( unsigned long searchLimit ) { mRecSearchLimit = searchLimit; }
            Boolean				HaveResults					( void );
            Boolean				LookupComplete				( void );
            
    const 	CNSLResult*   		GetNextResult				( void );
            void				NeedToRedoLastResult		( void )	{ mCurrentIndex--; }
    const 	CNSLResult*			GetResult					( CFIndex index );		// 0 based

            void				AddService					( CNSLResult* newResult );
            
            void				StartingNewLookup			( CNSLServiceLookupThread* searchThread );
            // service lookup thread calls this when it finishes
            void				ServiceLookupComplete		( CNSLServiceLookupThread* searchThread );		
            
            void				ResultListQueueLock			( void ) { pthread_mutex_lock( &mResultListQueueLock ); }
            void				ResultListQueueUnlock		( void ) { pthread_mutex_unlock( &mResultListQueueLock ); }
            void				SearchListQueueLock			( void ) { pthread_mutex_lock( &mSearchListQueueLock ); }
            void				SearchListQueueUnlock		( void ) { pthread_mutex_unlock( &mSearchListQueueLock ); }
protected:
            
private:
            pthread_mutex_t		mResultListQueueLock;
            pthread_mutex_t		mSearchListQueueLock;
            CFStringRef			mNodeName;
            uid_t				mUID;
            CNSLPlugin*			mParentPlugin;
    const	void*				mRef;						// this corresponds to the key mapping to this lookup
            CFIndex				mRecSearchLimit;
            CFMutableArrayRef	mResultList;	
            CFMutableArrayRef	mSearchList;	
            CFIndex				mCurrentIndex;
            Boolean				mInitialized;
			Boolean				mDeleteSelfWhenDone;
            Boolean				mLookupStarted;
            UInt32				mDelCounter;
			CNSLDirNodeRep*		mSelfPtr;
};



#endif // #ifndef












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
 *  @header CNSLPlugin
 */

#ifndef _CNSLPlugin_
#define _CNSLPlugin_	1

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/CDSServerModule.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>
#include <DirectoryService/DirServicesConst.h>

#include <stdio.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "NSLDebugLog.h"
#include "CNSLResult.h"

#define kTaskInterval	2
#define	kNodeTimerIntervalImmediate	1			// Fire ASAP

#define	kOncePerDay					1*60*60*24
#define kKeepTimerAroundAfterFiring	kOncePerDay	// 1 day interval on timers
#define kMaxTimeBetweenNodeLookups	30*60		// 30 minutes

#define kHTTPServiceType			"http"
#define kAFPServiceType				"afp"
#define kNFSServiceType				"nfs"
#define kFTPServiceType				"ftp"
#define kSMBServiceType				"smb"
#define kRAdminServiceType			"radminx"
#define kLaserWriterServiceType		"LaserWriter"

#define	kDSNAttrLocation			 "dsAttrTypeStandard:Location"

const CFStringRef	kDSNSLQueuedSearchSAFE_CFSTR = CFSTR("DS NSL Queued Search");
const CFStringRef	kNetworkChangeNSLCopyStringCallbackSAFE_CFSTR = CFSTR("NetworkChangeNSLCopyStringCallback");

const CFStringRef	kDSNAttrRecordNameSAFE_CFSTR = CFSTR(kDSNAttrRecordName);
const CFStringRef	kDS1AttrLocationSAFE_CFSTR = CFSTR(kDS1AttrLocation);
const CFStringRef	kDS1AttrCommentSAFE_CFSTR = CFSTR(kDS1AttrComment);
const CFStringRef	kDS1AttrServiceTypeSAFE_CFSTR = CFSTR(kDS1AttrServiceType);
const CFStringRef	kDSNAttrRecordTypeSAFE_CFSTR = CFSTR(kDSNAttrRecordType);
const CFStringRef	kDSStdRecordTypePrefixSAFE_CFSTR = CFSTR(kDSStdRecordTypePrefix);
const CFStringRef	kDSNativeRecordTypePrefixSAFE_CFSTR = CFSTR(kDSNativeRecordTypePrefix);

const CFStringRef	kDSNAttrURLSAFE_CFSTR = CFSTR(kDSNAttrURL);
const CFStringRef	kDSNAttrLocationSAFE_CFSTR = CFSTR(kDSNAttrLocation);
const CFStringRef	kDNSTextRecordSAFE_CFSTR = CFSTR("dsAttrTypeStandard:DNSTextRecord");
const CFStringRef	kDS1AttrPtrSAFE_CFSTR = CFSTR(kDS1AttrPort);

const CFStringRef	kDSStdRecordTypeAFPServerSAFE_CFSTR = CFSTR(kDSStdRecordTypeAFPServer);
const CFStringRef	kDSStdRecordTypeSMBServerSAFE_CFSTR = CFSTR(kDSStdRecordTypeSMBServer);
const CFStringRef	kDSStdRecordTypeNFSSAFE_CFSTR = CFSTR(kDSStdRecordTypeNFS);
const CFStringRef	kDSStdRecordTypeFTPServerSAFE_CFSTR = CFSTR(kDSStdRecordTypeFTPServer);
const CFStringRef	kDSStdRecordTypeWebServerSAFE_CFSTR = CFSTR(kDSStdRecordTypeWebServer);

const CFStringRef	kDotSAFE_CFSTR = CFSTR(".");
const CFStringRef	kEmptySAFE_CFSTR = CFSTR("");
const CFStringRef	kDotUnderscoreTCPSAFE_CFSTR = CFSTR("._tcp.");
const CFStringRef	kUnderscoreSAFE_CFSTR = CFSTR("_");

#define kSecondsBeforeStartingNewNodeLookup	1			// time to wait before starting new network traffic
enum eBuffType {
    kRecordListType			= 'RecL',
    kClientSideParsingBuff	= 'StdB'
};

typedef struct NodeData {
    CFStringRef				fNodeName;			// this is what our plugin is going to find
    tDataList*				fDSName;			// this is the converted DS construct
    UInt32					fTimeStamp;			// this is when the node was last "found"
    bool					fIsADefaultNode;	// is this node one that is considered "local" by plugin?
    uInt32					fSignature;			// signature of plugin registering this
    CFMutableDictionaryRef	fServicesRefTable;
} NodeData;

typedef enum {
    kClearOutStaleNodes,					// check each node's timestamp and compare to last time we did a lookup
    kClearOutAllNodes						// just get rid of them.
} NodeDataMessageType;

typedef struct NSLNodeHandlerContext {
    CFMutableDictionaryRef		fDictionary;
    NodeDataMessageType			fMessage;
    void*						fDataPtr;
	CFMutableArrayRef			fNodesToRemove;
} NSLNodeHandlerContext;

#define kMaxNumOutstandingSearches	6

void SearchStarted( void );
void SearchCompleted( void );

class CNSLServiceLookupThread;
class CNSLDirNodeRep;
class TCFResources;
class CPlugInRef;
class CDataBuff;

class CNSLPlugin : public CDSServerModule
{
friend class CNSLServiceLookupThread;
public:
                                CNSLPlugin				( void );
    virtual                     ~CNSLPlugin				( void );
    virtual sInt32				InitPlugin				( void ) = 0;
            
			void				WaitForInit				( void );
	virtual sInt32				Validate				( const char *inVersionStr, const uInt32 inSignature );
	
			Boolean				IsActive				( void );
	virtual sInt32				ProcessRequest			( void *inData );
	virtual sInt32				SetServerIdleRunLoopRef	( CFRunLoopRef idleRunLoopRef );
	virtual sInt32				SetPluginState			( const uInt32 inState );
	virtual	void				ActivateSelf			( void );
	virtual	void				DeActivateSelf			( void );

#pragma mark
	virtual sInt32				NSLSearchTickler		( void );
	virtual	void				CancelCurrentlyQueuedSearches	( void );

            void				AddNode					( CFStringRef nodeNameRef, Boolean isLocalNode = false );
            void				AddNode					( const char* nodeName, Boolean isLocalNode = false );
            void				RemoveNode				( CFStringRef nodeNameRef );
	virtual	void				NodeLookupComplete		( void );		// node lookup thread calls this when it finishes

            void				LockPlugin				( void ) { pthread_mutex_lock( &mPluginLock ); }
            void				UnlockPlugin			( void ) { pthread_mutex_unlock( &mPluginLock ); }
            pthread_mutex_t		mPluginLock;

            void				LockOpenRefTable		( void ) { pthread_mutex_lock( &mOpenRefTableLock ); }
            void				UnlockOpenRefTable		( void ) { pthread_mutex_unlock( &mOpenRefTableLock ); }
            pthread_mutex_t		mOpenRefTableLock;

            void				LockSearchQueue			( void )  { pthread_mutex_lock( &mSearchQueueLock ); }
            void				UnlockSearchQueue		( void )  { pthread_mutex_unlock( &mSearchQueueLock ); }
            pthread_mutex_t		mSearchQueueLock;
            
            void				LockPublishedNodes		( void ) { pthread_mutex_lock( &mQueueLock ); }
            void				UnlockPublishedNodes	( void ) { pthread_mutex_unlock( &mQueueLock ); }
            pthread_mutex_t		mQueueLock;

            void				LockSearchLookupTimer	( void ) { pthread_mutex_lock( &mSearchLookupTimerLock ); }
            void				UnlockSearchLookupTimer	( void ) { pthread_mutex_unlock( &mSearchLookupTimerLock ); }
            pthread_mutex_t		mSearchLookupTimerLock;

            void				LockNodeLookupTimer		( void ) { pthread_mutex_lock( &mNodeLookupTimerLock ); }
            void				UnlockNodeLookupTimer	( void ) { pthread_mutex_unlock( &mNodeLookupTimerLock ); }
            pthread_mutex_t		mNodeLookupTimerLock;

            void				StartNodeLookup			( void );			// this should fire off some threads in the subclass
            void				ClearOutAllNodes		( void );		

    virtual const char*			GetProtocolPrefixString	( void ) = 0;	// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString		( void );		// the node that should get mapped to the "Local" node
    virtual Boolean 			IsLocalNode				( const char *inNode );
    virtual Boolean 			IsADefaultOnlyNode		( const char *inNode );
    virtual	Boolean				OKToStartNewSearch		( void );
    virtual void				StartNextQueuedSearch	( void );
            sInt16				NumOutstandingSearches	( void );
    
    static	CNSLPlugin*			TheNSLPlugin			( void ) { return gsTheNSLPlugin; }
    static	CNSLPlugin*			gsTheNSLPlugin;

			CFRunLoopRef		GetRunLoopRef			( void ) { return mRunLoopRef; }

			void				HandleNetworkTransitionIfTime( void );
			void				PeriodicNodeLookupTask		( void );
			
    virtual char*				CreateNSLTypeFromRecType( char *inRecType );
    virtual char*				CreateRecTypeFromURL	( char *inNSLType );      
	virtual CFStringRef			CreateRecTypeFromNativeType ( char *inNativeType );
	virtual CFStringRef			CreateRecTypeFromNativeType ( CFStringRef inNativeType );
    
protected:
#pragma mark
    virtual	CFStringRef			GetBundleIdentifier		( void ) = 0;
    virtual void				NewNodeLookup			( void ) = 0;
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName ) = 0;
			Boolean				PluginHasBeenNSLActivated( void ) { return mActivatedByNSL; }

    virtual void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep ) = 0;
    
            void				QueueNewSearch			( CNSLServiceLookupThread* newLookup );
            void				StartSubNodeLookup		( char* parentNodeName );
            void				StartServicesLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    
            sInt32				HandleRequest 			( void *inData );

	virtual	sInt32				DoPlugInCustomCall		( sDoPlugInCustomCall *inData );

	virtual sInt32				GetDirNodeInfo			( sGetDirNodeInfo *inData );
            sInt32				OpenDirNode 			( sOpenDirNode *inData );
            sInt32				CloseDirNode 			( sCloseDirNode *inData );
            sInt32				GetRecordList			( sGetRecordList *inData );
            
    virtual	sInt32				OpenRecord				( sOpenRecord *inData );
    virtual	sInt32				CloseRecord				( sCloseRecord *inData );
    virtual	sInt32				CreateRecord			( sCreateRecord *inData );
    virtual	sInt32				DeleteRecord			( sDeleteRecord *inData );
    virtual	sInt32				FlushRecord				( sFlushRecord *inData );
    virtual	sInt32				AddAttributeValue		( sAddAttributeValue *inData );
    virtual	sInt32				RemoveAttribute			( sRemoveAttribute *inData );
    virtual	sInt32				RemoveAttributeValue	( sRemoveAttributeValue *inData );
    virtual	sInt32				SetAttributeValue		( sSetAttributeValue *inData );
    virtual	sInt32				HandleNetworkTransition	( sHeader *inData );
    
    virtual	Boolean				ReadOnlyPlugin			( void ) { return true; }
    virtual	Boolean				IsClientAuthorizedToCreateRecords ( sCreateRecord *inData );
	
	virtual Boolean				ResultMatchesRequestCriteria( const CNSLResult* result, sGetRecordList* request );
    
    virtual	sInt32				RegisterService			( tRecordReference recordRef, CFDictionaryRef service );
    virtual	sInt32				DeregisterService		( tRecordReference recordRef, CFDictionaryRef service );

            sInt32				RetrieveResults			( sGetRecordList* inData, CNSLDirNodeRep* nodeRep );

            // when node lookup is complete, any nodes that are stale should get unpublished
    virtual void				ClearOutStaleNodes		( void );		
            
            void				ZeroLastNodeLookupStartTime	( void ) { mLastNodeLookupStartTime = 0; }
            UInt32				GetLastNodeLookupStartTime	( void ) { return mLastNodeLookupStartTime; }

	virtual	UInt32				GetTimeBetweenNodeLookups	( void ) { return kOncePerDay; }	// plugins can override if they need to
			void				InstallNodeLookupTimer		( void );
			void				UnInstallNodeLookupTimer	( void );
			void				ResetNodeLookupTimer		( UInt32 timeTillNewLookup );
			
        CFMutableDictionaryRef	mOpenRefTable;
        CFMutableDictionaryRef	mPublishedNodes;		// we keep track of published nodes here
        Boolean					mActivatedByNSL;
        
        uInt32					mState;
		time_t					mTransitionCheckTime;
private:
    virtual sInt32				Initialize				( void );
			void				InstallSearchTickler	( void );
			void				UnInstallSearchTickler	( void );
        
		CFRunLoopRef			mRunLoopRef;
		CFRunLoopTimerRef 		mTimerRef;
		CFRunLoopTimerRef 		mNodeLookupTimerRef;
		Boolean					mSearchTicklerInstalled;
		Boolean					mNodeLookupTimerInstalled;
        CFMutableArrayRef		mSearchQueue;
        UInt32					mLastNodeLookupStartTime;
        char*					mDSLocalNodeLabel;
        char*					mDSNetworkNodeLabel;
        char*					mDSTopLevelNodeLabel;
        uInt32			mSignature;
};

NodeData * AllocateNodeData();
void DeallocateNodeData( NodeData *nodeData );


UInt32 GetCurrentTime( void );		// returns current time in seconds
CFStringEncoding NSLGetSystemEncoding( void );	// read the contents of /var/root/.CFUserTextEncoding
double dsTimestamp(void);

#endif		// #ifndef

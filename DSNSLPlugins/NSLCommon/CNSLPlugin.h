/*
 *  CNSLPlugin.h
 *
 *	This is a wrapper base class that is to be used by migrating plugins from NSL
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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

#include <stdio.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "NSLDebugLog.h"
#include "CNSLResult.h"

#define kMinTimeBetweenNodeLookups	5*60		// 5 minutes is the least amount of time to wait between lookups
#define kMaxTimeBetweenNodeLookups	12*60*60	// 12 hours

#define kHTTPServiceType			"http"
#define kAFPServiceType				"afp"
#define kNFSServiceType				"nfs"
#define kFTPServiceType				"ftp"
#define kSMBServiceType				"smb"
#define kRAdminServiceType			"radminx"
#define kLaserWriterServiceType		"LaserWriter"

#define	kDSNAttrLocation			 "dsAttrTypeStandard:Location"

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
            
	virtual sInt32				Validate				( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32				ProcessRequest			( void *inData );
	virtual sInt32				SetServerIdleRunLoopRef	( CFRunLoopRef idleRunLoopRef );
	virtual sInt32				SetPluginState			( const uInt32 inState );

#pragma mark
	virtual sInt32				NSLSearchTickler		( void );

            void				AddNode					( CFStringRef nodeNameRef, Boolean isLocalNode = false );
            void				AddNode					( const char* nodeName, Boolean isLocalNode = false );
            void				RemoveNode				( CFStringRef nodeNameRef );
	virtual	void				NodeLookupComplete		( void );		// node lookup thread calls this when it finishes

            void				LockPlugin				( void ) { pthread_mutex_lock( &mPluginLock ); }
            void				UnlockPlugin			( void ) { pthread_mutex_unlock( &mPluginLock ); }
            pthread_mutex_t		mPluginLock;

            void				LockSearchQueue			( void )  { pthread_mutex_lock( &mSearchQueueLock ); }
            void				UnlockSearchQueue		( void )  { pthread_mutex_unlock( &mSearchQueueLock ); }
            pthread_mutex_t		mSearchQueueLock;
            
            void				LockPublishedNodes		( void ) { pthread_mutex_lock( &mQueueLock ); }
            void				UnlockPublishedNodes	( void ) { pthread_mutex_unlock( &mQueueLock ); }
            pthread_mutex_t		mQueueLock;

            void				StartNodeLookup			( void );			// this should fire off some threads in the subclass
            void				ClearOutAllNodes		( void );		

    virtual const char*			GetProtocolPrefixString	( void ) = 0;	// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString		( void );		// the node that should get mapped to the "Local" node
    virtual Boolean 			IsLocalNode				( const char *inNode );
    virtual Boolean 			IsADefaultOnlyNode		( const char *inNode );
    virtual	Boolean				OKToStartNewSearch		( void );
    virtual void				StartNextQueuedSearch	( void );
            sInt16				NumOutstandingSearches	( void );
//            void				ServiceLookupComplete	( CNSLServiceLookupThread* searchThread, CNSLDirNodeRep* node );
    
    static	CNSLPlugin*			TheNSLPlugin			( void ) { return gsTheNSLPlugin; }
    static	CNSLPlugin*			gsTheNSLPlugin;

			CFRunLoopRef		GetRunLoopRef			( void ) { return mRunLoopRef; }

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
    
    virtual	Boolean				ReadOnlyPlugin			( void );
    virtual	Boolean				IsClientAuthorizedToCreateRecords ( sCreateRecord *inData );
	
	virtual Boolean				ResultMatchesRequestCriteria( const CNSLResult* result, sGetRecordList* request );
    
    virtual	sInt32				RegisterService			( tRecordReference recordRef, CFDictionaryRef service );
    virtual	sInt32				DeregisterService		( tRecordReference recordRef, CFDictionaryRef service );

            sInt32				RetrieveResults			( sGetRecordList* inData, CNSLDirNodeRep* nodeRep );

            TCFResources*		OurResources			( void ) { return mOurResources; }
            
            Boolean				IsTimeForNewNeighborhoodLookup( void );
            
            // when node lookup is complete, any nodes that are stale should get unpublished
            void				ClearOutStaleNodes		( void );		
            Boolean				IsTimeToClearOutStaleNodes( void ) { return mTimeToClearOutStaleNodes; }
            void				SetTimeToClearOutStaleNodes( Boolean isTime ) { mTimeToClearOutStaleNodes = isTime; }
            
            UInt32				GetLastNodeLookupStartTime( void ) { return mLastNodeLookupStartTime; }
        
        CFMutableDictionaryRef	mOpenRefTable;
        CFMutableDictionaryRef	mPublishedNodes;		// we keep track of published nodes here
        Boolean					mActivatedByNSL;
        
        uInt32					mState;
private:
    virtual sInt32				Initialize				( void );
	virtual sInt32				PeriodicTask			( void );
			void				InstallSearchTickler	( void );
			void				UnInstallSearchTickler	( void );
        
		CFRunLoopRef			mRunLoopRef;
		CFRunLoopTimerRef 		mTimerRef;
		Boolean					mSearchTicklerInstalled;
        CFMutableArrayRef		mSearchQueue;
        UInt32					mLastNodeLookupStartTime;
		UInt32					mMinTimeBetweenNodeLookups;
        Boolean					mTimeToClearOutStaleNodes;
        TCFResources*			mOurResources;
        char*					mDSLocalNodeLabel;
        char*					mDSNetworkNodeLabel;
        char*					mDSTopLevelNodeLabel;
        uInt32			mSignature;
};

NodeData * AllocateNodeData();
void DeallocateNodeData( NodeData *nodeData );


UInt32 GetCurrentTime( void );		// returns current time in seconds
CFStringEncoding NSLGetSystemEncoding( void );	// read the contents of /var/root/.CFUserTextEncoding

#endif		// #ifndef

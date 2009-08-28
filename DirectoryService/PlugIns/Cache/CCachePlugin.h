/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * @header CCachePlugin
 */

#ifndef __CCachePlugin_h__
#define __CCachePlugin_h__	1

#include <stdio.h>

#include <pwd.h>
#include <grp.h>
#include <dns_util.h>
#include <SystemConfiguration/SCDynamicStore.h>

#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "CDSServerModule.h"
#include "SharedConsts.h"
#include "PluginData.h"
#include "CServerPlugin.h"
#include "CCache.h"
#include "DSMutexSemaphore.h"
#include "DSEventSemaphore.h"
#include "DSSemaphore.h"
#include "DSUtils.h"
#include "CLog.h"
#include <set>
#include <libkern/OSAtomic.h>
#include <mach/clock_types.h>
#include <pthread.h>
#include "CCache.h"
#include <dispatch/dispatch.h>

#include <CoreFoundation/CoreFoundation.h>

#define kDSCacheNodeName "/Cache"
// ThreadStateExitRequested is in res_private.h - it should be moved to dns_private.h
#define ThreadStateExitRequested 4

typedef struct {
	tDirReference		fDirRef;				// DS session ref
	tDirNodeReference	fNodeRef;				// search node ref
	UInt32				fLimitRecSearch;		// limit on record count request
	tContextData		fContinue;				// continue data handle for search node
} sCacheContinueData;

typedef struct {
	char			   *fNodeName;				// cache node name string
	UInt32				fUID;					// client process UID
	UInt32				fEffectiveUID;			// client process EUID
	UInt32				offset;					// offset for data extraction out of the buffers
} sCacheContextData;

#ifdef HANDLE_DNS_LOOKUPS

#define kMaxDNSQueries          32

struct sDNSLookup
{
    volatile int32_t    fOutstanding;   // number of pending queries (expected)
    int                 fTotal;         // number of total actual queries initiated
    uint16_t            fQueryTypes[10];
    uint16_t            fQueryClasses[10];
    char                *fQueryStrings[10];
    dns_reply_t         *fAnswers[10];  // up to 10 active queries
    uint16_t            fAdditionalInfo[10];
    int32_t             fMinimumTTL[10];
    double              fAnswerTime[10];
    int32_t             fQueryFinished[10];
    pthread_t           fThreadID[10];
	pthread_t           fAllocatedByThread;
    dispatch_source_t   fTimerSource;
	pthread_mutex_t		fMutex;
    dispatch_semaphore_t    fSemaphore;

    sDNSLookup( const char *inName = NULL )
    {
        fOutstanding = 0;
        fTotal = 0;
        bzero( fQueryTypes, sizeof(fQueryTypes) );
        bzero( fQueryClasses, sizeof(fQueryClasses) );
        bzero( fAnswers, sizeof(fAnswers) );
        bzero( fQueryStrings, sizeof(fQueryStrings) );
        bzero( fAdditionalInfo, sizeof(fAdditionalInfo) );
        bzero( fMinimumTTL, sizeof(fMinimumTTL) );
        bzero( fAnswerTime, sizeof(fAnswerTime) );
        bzero( fQueryFinished, sizeof(fQueryFinished) );
        bzero( fThreadID, sizeof(fThreadID) );
        fTimerSource = NULL;
	    fAllocatedByThread = pthread_self();
        
        pthread_mutexattr_t attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
        pthread_mutex_init( &fMutex, &attr );
        pthread_mutexattr_destroy( &attr );
        
        fSemaphore = NULL;
    }
    
    ~sDNSLookup( void )
    {
		// if there are still outstanding queries and we're being deleted,
		// that's a bad thing.  Log and abort rather than risk using a freed
		// pointer.
		if ( fOutstanding > 0 )
		{
			DbgLog( kLogDebug,
					"CCachePlugin::~sDNSLookup - destructor called by thread %X with %d outstanding queries.  Allocated by thread %X.  Aborting",
					pthread_self(), fOutstanding, fAllocatedByThread );

			abort();
		}

        for( int ii = 0; ii < fTotal; ii++ )
        {
            if( fAnswers[ii] != NULL )
            {
                dns_free_reply( fAnswers[ii] );
                fAnswers[ii] = NULL;
            }
            
            DSFree( fQueryStrings[ii] );
        }
        
        if ( fTimerSource != NULL ) {
            dispatch_cancel( fTimerSource );
            dispatch_release( fTimerSource );
            fTimerSource = NULL;
        }
        if ( fSemaphore != NULL ) {
            dispatch_release( fSemaphore );
            fSemaphore = NULL;
        }
        
        pthread_mutex_destroy( &fMutex );
    }
    
    void AddQuery( uint16_t inQueryClass, uint16_t inQueryType, char *inQueryString, uint16_t inAdditionalInfo = 0 )
    {
        fQueryClasses[fTotal] = inQueryClass;
        fQueryTypes[fTotal] = inQueryType;
        fQueryStrings[fTotal] = strdup( inQueryString );
        fAdditionalInfo[fTotal] = inAdditionalInfo;
        fTotal++;
    }
};
#else
	typedef void *sDNSLookup;
#endif

typedef sCacheValidation* (*ProcessEntryCallback)( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, 
                                                   tDataBufferPtr inDataBuffer, tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, 
                                                   void *additionalInfo, CCache *inCache, const char **inKeys );

class CCachePlugin : public CServerPlugin
{

public:
						CCachePlugin			( FourCharCode inSig, const char *inName );
	virtual			   ~CCachePlugin			( void );

	virtual SInt32		Validate				( const char *inVersionStr, const UInt32 inSignature );
	virtual SInt32		Initialize				( void );
	virtual SInt32		SetPluginState			( const UInt32 inState );
	virtual SInt32		PeriodicTask			( void );
	virtual SInt32		ProcessRequest			( void *inData );

    static  void        WakeUpRequests          ( void );
	static	void		ContinueDeallocProc		( void *inContinueData );
	static	void		ContextDeallocProc		( void* inContextData );
	kvbuf_t*			ProcessLookupRequest	( int inProcNumber, char* inData, int inCount, pid_t inPID );

    static void         AddEntryToCacheWithKeys ( CCache *inCache,
                                                  sCacheValidation *inValidation, 
                                                  kvbuf_t *inEntry, 
                                                  uint32_t flags,
                                                  uint32_t ttl,
                                                  const char **inKeys );
    static void         AddEntryToCacheWithMultiKey
                                                ( CCache *inCache, 
                                                  sCacheValidation *inValidation, 
                                                  kvbuf_t *inEntry, 
                                                  uint32_t flags,
                                                  uint32_t ttl,
                                                  ... );
    static void         AddEntryToCacheWithKeylists
                                                ( CCache *inCache, 
                                                  sCacheValidation *inValidation, 
                                                  kvbuf_t *inEntry, 
                                                  uint32_t flags,
                                                  uint32_t ttl,
                                                  ... );
    static void         RemoveEntryWithMultiKey( CCache *inCache, ... );
                                                  
    void                EmptyCacheEntryType     ( uint32_t inEntryType );
    int                 UpdateNodeReachability  ( char *inNodeName, 
                                                  bool inReachable );
#ifdef HANDLE_DNS_LOOKUPS
    void                DNSConfigurationChanged ( void );
#endif

    kvbuf_t*            DSgetnetgrent           ( kvbuf_t *inBuffer );  // need public access

protected:
	SInt32			HandleRequest				(	void *inData );
	void			WaitForInit					(	void );
    static SInt32	CleanContextData			(	sCacheContextData *inContext );

private:
    friend struct UserGroup **Mbrd_FindItemsAndRetain( tDirNodeReference dirNode, tDataListPtr recType, int idType, const char *value, uint32_t flags, UInt32 *recCount );
	SInt32			OpenDirNode					( sOpenDirNode *inData );
	SInt32			CloseDirNode				( sCloseDirNode *inData );
	SInt32			GetDirNodeInfo				( sGetDirNodeInfo *inData );
	SInt32			GetRecordList				( sGetRecordList *inData );
	SInt32			AttributeValueSearch		( sDoAttrValueSearchWithData *inData );
	SInt32			MultipleAttributeValueSearch( sDoMultiAttrValueSearchWithData *inData );
	SInt32			GetRecordEntry				( sGetRecordEntry *inData );
	SInt32			GetAttributeEntry			( sGetAttributeEntry *inData );
	SInt32			GetAttributeValue			( sGetAttributeValue *inData );
	SInt32			CloseAttributeList			( sCloseAttributeList *inData );
	SInt32			CloseAttributeValueList		( sCloseAttributeValueList *inData );
	SInt32			ReleaseContinueData			( sReleaseContinueData *inData );
	SInt32			DoPlugInCustomCall			( sDoPlugInCustomCall *inData );
    
    static void     SearchPolicyChange          ( SCDynamicStoreRef	store,
                                                  CFArrayRef		changedKeys,
                                                  void              *info );
    void            CheckSearchPolicyForNIS     ( void );
	
	kvbuf_t*		FetchFromCache				( CCache *inCache, 
                                                  uint32_t reqFlags, 
                                                  int32_t *outTTL,
												  ... );
	kvbuf_t*		GetRecordListLibInfo		( tDirNodeReference inNodeRef,
												  const char* inSearchValue, 
												  const char* inRecordType, 
												  UInt32 inRecCount, 
												  tDataListPtr inAttributes, 
												  ProcessEntryCallback inCallback,
												  kvbuf_t* inBuffer,
												  void *additionalInfo,
                                                  CCache *inCache,
                                                  const char **inKeys,
                                                  sCacheValidation **outValidation = NULL );
	
	kvbuf_t*		ValueSearchLibInfo			( tDirNodeReference inNodeRef, 
												  const char* inSearchAttrib,
												  const char* inSearchValue,
												  const char* inRecordType,
												  UInt32 inRecCount,
												  tDataListPtr inAttributes,
												  ProcessEntryCallback inCallback,
												  kvbuf_t* inBuffer,
												  void *additionalInfo,
                                                  sCacheValidation **outValidation,
                                                  tDirPatternMatch inSearchType = eDSiExact );
	
	kvbuf_t*		DSgetpwnam					( kvbuf_t *inBuffer, pid_t inPID );
    void            DSgetpwnam_initext          ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetpwnam_ext              ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetpwnam_int              ( kvbuf_t *inBuffer, pid_t inPID, uint32_t reqFlags, const tDataListPtr attrTypes );
    kvbuf_t*        DSgetpwuuid                 ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetpwuid					( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetpwent					( void );

	kvbuf_t*		DSgetgrnam					( kvbuf_t *inBuffer, pid_t inPID );
    void            DSgetgrnam_initext          ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetgrnam_ext              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetgrnam_int              ( kvbuf_t *inBuffer, pid_t inPID, uint32_t reqFlags, const tDataListPtr attrTypes );
    kvbuf_t*        DSgetgruuid                 ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetgrgid					( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetgrent					( void );
    
    kvbuf_t*        DSgetfsbyname               ( kvbuf_t *inBuffer );
    kvbuf_t*        DSgetfsent                  ( pid_t inPID );
	
	kvbuf_t*		DSgetaliasbyname			( kvbuf_t *inBuffer );
	kvbuf_t*		DSgetaliasent				( void );
	
	kvbuf_t*        DSgetservbyname_int         ( const char *service, const char *proto, pid_t inPID );
	kvbuf_t*		DSgetservbyname				( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetservbyport				( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetservent				( void );
	
	kvbuf_t*		DSgetprotobyname			( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetprotobynumber			( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetprotoent				( void );
    
	kvbuf_t*		DSgetrpcbyname              ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetrpcbynumber            ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetrpcent                 ( void );
    
	kvbuf_t*		DSgetnetbyname              ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetnetbyaddr              ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetnetent                 ( void );
    
    kvbuf_t*        DSinnetgr                   ( kvbuf_t *inBuffer );

    kvbuf_t*        DSsetloginuser              ( kvbuf_t *inBuffer, pid_t inPID );
    
    kvbuf_t *       dnsreply_to_kvbuf           ( dns_reply_t *inReply, 
                                                  const char *inIPv4, 
                                                  const char *inIPv6, 
                                                  pid_t inPID, 
                                                  int32_t *outTTL );
    
#ifdef HANDLE_DNS_LOOKUPS
    int             SortPartitionAdditional     ( dns_resource_record_t **inRecords, 
                                                  int inFirst, 
                                                  int inLast );
    void            QuickSortAdditional         ( dns_resource_record_t **inRecords, 
                                                  int inFirst, 
                                                  int inLast );
    sDNSLookup *    CreateAdditionalDNSQueries  ( sDNSLookup *inDNSLookup, 
                                                  const char *inService, 
                                                  const char *inProtocol, 
                                                  const char *inName, 
                                                  const char *inSearchDomain,
                                                  uint16_t inAdditionalInfo );
    void            InitiateDNSQuery            ( sDNSLookup *inLookup, 
                                                  bool inParallel );
	void            IssueParallelDNSQueries     ( sDNSLookup      *inLookup );
	static void     AbandonDNSQueries           ( sDNSLookup      *inLookup );
#endif
    
    kvbuf_t*        DSgethostbyname             ( kvbuf_t *inBuffer, 
                                                  pid_t inPID, 
                                                  bool inParallelQuery = false,
                                                  sDNSLookup *inLookup = NULL );
    kvbuf_t*        DSgethostbyname_int         ( char *inName, 
                                                  const char *inIPv4, 
                                                  const char *inIPv6, 
                                                  int inPID, 
                                                  bool inParallelQuery, 
                                                  sDNSLookup *inLookup, 
                                                  int32_t *outTTL = NULL );
    kvbuf_t*        DSgethostbyname_service     ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgethostbyaddr             ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgethostent                ( kvbuf_t *inBuffer, pid_t inPID );
    
#ifdef HANDLE_DNS_LOOKUPS
    kvbuf_t*        DSgetnameinfo               ( kvbuf_t *inBuffer, pid_t inPID );
#endif

    kvbuf_t*        DSgetmacbyname              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgethostbymac              ( kvbuf_t *inBuffer, pid_t inPID );
#ifdef HANDLE_DNS_LOOKUPS
    kvbuf_t*        DSgetaddrinfo               ( kvbuf_t *inBuffer, pid_t inPID );
#endif
    kvbuf_t*        DSdns_proxy                 ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetbootpbyhw              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetbootpbyaddr            ( kvbuf_t *inBuffer, pid_t inPID );
    
    bool            IsLocalOnlyPID              ( pid_t inPID );
    
#ifdef HANDLE_DNS_LOOKUPS
    void            checkAAAAstatus             ( void );
#endif

	sCacheContextData*	MakeContextData			( void );
	
	tDirReference		fDirRef;				// DS session reference
	tDirNodeReference	fSearchNodeRef;			// search node reference
    tDirNodeReference   fFlatFileNodeRef;       // flat file node reference
    tDirNodeReference   fNISNodeRef;
    tDirNodeReference   fLocalNodeRef;
    dispatch_queue_t    fNISQueue;
    dispatch_queue_t    fCheckNISQueue;
	UInt32				fState;
	bool				fPluginInitialized;
	CCache				*fLibinfoCache;
    DSSemaphore         fPIDListLock;
    set<pid_t>          fLocalOnlyPIDs;
    
    DSMutexSemaphore    fStatsLock;
    int64_t             fCacheHits;
    int64_t             fCacheMisses;
    int32_t             fFlushCount;
    double              fTotalCallTime;
    int64_t             fTotalCalls;
    int64_t             fCallsByFunction[ 128 ];
    int64_t             fCacheHitsByFunction[ 128 ];
    int64_t             fCacheMissByFunction[ 128 ];
#ifdef HANDLE_DNS_LOOKUPS
    uint16_t            fTypeCNAME;
    uint16_t            fTypeA;
    uint16_t            fTypeAAAA;
    uint16_t            fTypeSRV;
    uint16_t            fTypePTR;
    uint16_t            fTypeMX;
    uint16_t            fClassIN;
    int                 fGetAddrStateEngine[10];
    bool                fUnqualifiedSRVAllowed;
    bool                fAlwaysDoAAAA;
#endif
};

#endif	// __CCachePlugin_H__

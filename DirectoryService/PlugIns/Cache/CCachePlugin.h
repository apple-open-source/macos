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

#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "CDSServerModule.h"
#include "SharedConsts.h"
#include "PluginData.h"
#include "CServerPlugin.h"
#include "CInternalDispatchThread.h"
#include "cache.h"
#include "DSMutexSemaphore.h"
#include "DSEventSemaphore.h"
#include "DSSemaphore.h"
#include "DSUtils.h"
#include "CLog.h"
#include <set>
#include <libkern/OSAtomic.h>
#include <mach/clock_types.h>

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

#define kMaxDNSQueries          32

struct sDNSLookup
{
    pthread_mutex_t     fMutex;
    pthread_cond_t      fCondition;
    int32_t             fOutstanding;   // number of pending queries (expected)
    int                 fTotal;         // number of total actual queries initiated
    bool                fDeleteEntry;   // request already should be deleted if fOutanding = 0
    uint16_t            fQueryTypes[10];
    uint16_t            fQueryClasses[10];
    char                *fQueryStrings[10];
    dns_reply_t         *fAnswers[10];  // up to 10 active queries
    uint16_t            fAdditionalInfo[10];
    int                 fMinimumTTL[10];
    double              fAnswerTime[10];
    int32_t             fQueryFinished[10];
    pthread_t           fThreadID[10];
    
    sDNSLookup( void )
    {
        fDeleteEntry = false;
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
        pthread_mutex_init( &fMutex, NULL );
        pthread_cond_init( &fCondition, NULL );
    }
    
    ~sDNSLookup( void )
    {
        for( int ii = 0; ii < fTotal; ii++ )
        {
            if( fAnswers[ii] != NULL )
            {
                dns_free_reply( fAnswers[ii] );
                fAnswers[ii] = NULL;
            }
            
            if( fQueryStrings[ii] != NULL )
            {
                free( fQueryStrings[ii] );
                fQueryStrings[ii] = NULL;
            }
        }
    }
    
    void AddQuery( uint16_t inQueryClass, uint16_t inQueryType, char *inQueryString, uint16_t inAdditionalInfo = 0 )
    {
        pthread_mutex_lock( &fMutex );
        fQueryClasses[fTotal] = inQueryClass;
        fQueryTypes[fTotal] = inQueryType;
        fQueryStrings[fTotal] = strdup( inQueryString );
        fAdditionalInfo[fTotal] = inAdditionalInfo;
        fTotal++;
        pthread_mutex_unlock( &fMutex );
    }
    
    void WaitUntilDone( int inIndexOfAReq )
    {
        int             milliSecs   = 0;
        double          maxTime     = 0.0;
        struct timeval	tvNow;
        struct timespec	tsTimeout   = { 0 };
        bool            bTimedOut   = false;
        
        pthread_mutex_lock( &fMutex );

        if ( inIndexOfAReq >= 0 )
            DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - Waiting (60 sec max) for first A response, Outstanding = %d, Total = %d", 
                    fOutstanding, fTotal );
        else
            DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - Waiting (60 sec max), Outstanding = %d, Total = %d", fOutstanding, 
                    fTotal );

        gettimeofday( &tvNow, NULL );
        TIMEVAL_TO_TIMESPEC ( &tvNow, &tsTimeout );
        tsTimeout.tv_sec += 60; // only wait 60 seconds
        
        while ( fOutstanding > 0 )
        {
            if ( pthread_cond_timedwait( &fCondition, &fMutex, &tsTimeout ) == ETIMEDOUT ) {
                // we'll only wait 60 seconds
                DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - Giving up after 60 sec - Outstanding = %d, Total = %d", fOutstanding, 
                        fTotal );
                bTimedOut = true;
                break;
            }
            
            // if our IPv4 query finished we'll break out of this 60 second wait
            if ( inIndexOfAReq >= 0 ) {
                if ( fQueryFinished[inIndexOfAReq] == true )
                    break;
            } else if ( fOutstanding != fTotal ) {
                // if any queries finished we'll break too
                break;
            }
        } 
        
        // if we didn't timeout from the 60 seconds and we still have queries left
        if ( bTimedOut == false && fOutstanding > 0 )
        {
            // if we are looking for an A request and got an answer, use that time as our calculation
            if ( inIndexOfAReq >= 0 && fAnswers[inIndexOfAReq] != NULL )
                maxTime = fAnswerTime[inIndexOfAReq]; // use the time of that request directly
            
            // if we still don't have a maxTime, see if we have answers and get the max time
            if ( maxTime == 0.0 ) {
                // find response with highest time and use that as our further delay
                for ( int ii = 0; ii < fTotal; ii++ ) 
                {
                    // if the query is finished and we got an answer
                    if ( fQueryFinished[ii] == true && fAnswers[ii] != NULL && fAnswerTime[ii] > maxTime )
                        maxTime = fAnswerTime[ii];
                }
            }
            
            // only if we have a maxTime do we actually set a timeout
            if ( maxTime > 0.0 ) {
                if ( fOutstanding > 0 ) {
                    milliSecs = (maxTime / 1000) * fTotal;
                    DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - total timeout will be %d ms", milliSecs );
                } else {
                    DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - queries complete" );
                    milliSecs = 0;
                }
            }
            
            if ( milliSecs > 0 )
            {
                struct timespec	tsWaitTime  = { 0 };
                
                DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - Waiting %d ms for remaining, Outstanding = %d, Total = %d", milliSecs, fOutstanding, 
                        fTotal );
                
                gettimeofday( &tvNow, NULL );
                TIMEVAL_TO_TIMESPEC ( &tvNow, &tsTimeout );
                tsWaitTime.tv_sec = (milliSecs / 1000);
                tsWaitTime.tv_nsec = ((milliSecs % 1000) * 1000000);
                ADD_MACH_TIMESPEC( &tsTimeout, &tsWaitTime );
                
                while ( fOutstanding > 0 )
                {
                    if ( pthread_cond_timedwait(&fCondition, &fMutex, &tsTimeout) == ETIMEDOUT )
                    {
                        DbgLog( kLogDebug, "CCachePlugin::WaitUntilDone - Condition timeout, Outstanding = %d, Total = %d", fOutstanding, 
                                fTotal );
                        break;
                    }
                }
            }
        }
        
        pthread_mutex_unlock( &fMutex );
    }
    
    void AbandonLookups( void );
    
    void SignalCondition( void )
    {
        pthread_mutex_lock( &fMutex );
        DbgLog(  kLogDebug, "CCachePlugin::WaitUntilDone - Signalling, Outstanding = %d, Total = %d", fOutstanding, 
                 fTotal );
        pthread_cond_signal( &fCondition );
        pthread_mutex_unlock( &fMutex );
    }
};

typedef sCacheValidation* (*ProcessEntryCallback)( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, 
                                                     tDataBufferPtr inDataBuffer, tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, 
                                                     void *additionalInfo, CCache *inCache, char **inKeys );

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
                                                  char **inKeys );
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
    void                UpdateNodeReachability  ( char *inNodeName, 
                                                  bool inReachable );
    void                DNSConfigurationChanged ( void );

    kvbuf_t*            DSgetnetgrent           ( kvbuf_t *inBuffer );  // need public access

protected:
	SInt32			HandleRequest				(	void *inData );
	void			WaitForInit					(	void );
    static SInt32	CleanContextData			(	sCacheContextData *inContext );

private:
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
	
	kvbuf_t*		FetchFromCache				( CCache *inCache, 
                                                  uint32_t *outTTL,
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
                                                  char **inKeys,
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
    kvbuf_t*        DSgetpwuuid                 ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetpwuid					( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetpwent					( void );

	kvbuf_t*		DSgetgrnam					( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetgruuid                 ( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetgrgid					( kvbuf_t *inBuffer, pid_t inPID );
	kvbuf_t*		DSgetgrent					( void );
    
    kvbuf_t*        DSgetfsbyname               ( kvbuf_t *inBuffer );
    kvbuf_t*        DSgetfsent                  ( pid_t inPID );
	
	kvbuf_t*		DSgetaliasbyname			( kvbuf_t *inBuffer );
	kvbuf_t*		DSgetaliasent				( void );
	
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
                                                  uint32_t *outTTL );
    int             SortPartitionAdditional     ( dns_resource_record_t **inRecords, 
                                                  int inFirst, 
                                                  int inLast );
    void            QuickSortAdditional         ( dns_resource_record_t **inRecords, 
                                                  int inFirst, 
                                                  int inLast );
    sDNSLookup *    CreateAdditionalDNSQueries  ( sDNSLookup *inDNSLookup, 
                                                  char *inService, 
                                                  char *inProtocol, 
                                                  char *inName, 
                                                  char *inSearchDomain,
                                                  uint16_t inAdditionalInfo );
    void            InitiateDNSQuery            ( sDNSLookup *inLookup, 
                                                  bool inParallel );
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
                                                  uint32_t *outTTL = NULL );
    kvbuf_t*        DSgethostbyaddr             ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgethostent                ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetnameinfo               ( kvbuf_t *inBuffer, pid_t inPID );

    kvbuf_t*        DSgetmacbyname              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgethostbymac              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetaddrinfo               ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSdns_proxy                 ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetbootpbyhw              ( kvbuf_t *inBuffer, pid_t inPID );
    kvbuf_t*        DSgetbootpbyaddr            ( kvbuf_t *inBuffer, pid_t inPID );
    
    bool            IsLocalOnlyPID              ( pid_t inPID );
    
    void            checkAAAAstatus             ( void );

	sCacheContextData*	MakeContextData			( void );
	
	tDirReference		fDirRef;				// DS session reference
	tDirNodeReference	fSearchNodeRef;			// search node reference
    tDirNodeReference   fFlatFileNodeRef;       // flat file node reference
    tDirNodeReference   fLocalNodeRef;
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
};

#endif	// __CCachePlugin_H__

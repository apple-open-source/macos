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
 * Implements the search policies.
 */

#include "CCachePlugin.h"

#include "CPlugInRef.h";
#include "CContinue.h";
#include "DSEventSemaphore.h";
#include "DirServices.h"
#include "DirServicesUtils.h"
#include "ServerModuleLib.h"
#include "DSUtils.h"
#include "COSUtils.h"
#include "PluginData.h"
#include "DSCThread.h"
#include "CLog.h"
#include "CAttributeList.h"
#include "CBuff.h"
#include "CDataBuff.h"
#include "ServerControl.h"
#include "CPlugInList.h"
#include "PrivateTypes.h"
#include "idna.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <dns_util.h>
#include <dns_private.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <notify.h>
#include <net/ethernet.h>
#include <syslog.h>
#include <sys/sysctl.h>
#include <DirectoryServiceCore/CSharedData.h>
#include <netdb_async.h>

// Globals ---------------------------------------------------------------------------

static CPlugInRef		 	*gCacheNodeRef			= NULL;
static CContinue		 	*gCacheNodeContinue		= NULL;
static int32_t				aaaa_cutoff_enabled		= true;
static int					gActiveThreadCount		= 0;
static pthread_mutex_t		gActiveThreadMutex		= PTHREAD_MUTEX_INITIALIZER;

CCachePlugin				*gCacheNode				= NULL;
DSEventSemaphore            gKickCacheRequests;

extern	const char		*lookupProcedures[];
extern	CPlugInList		*gPlugins;
extern	dsBool			gDSInstallDaemonMode;
extern	bool			gServerOS;

const int kNegativeDNSCacheTime = 300;  // 5 minutes
const int kNegativeCacheTime    = 1800; // 30 minutes
const int kCacheTime            = 3600; // 1 hour
const int DNS_BUFFER_SIZE       = 8192;

static pthread_t       gActiveThreads[512];
static int             gNotifyTokens[512];
static void*		   gInterruptTokens[512];
static bool			   gThreadAbandon[512];

// private SPI
extern "C" {
	uint32_t notify_register_plain( const char *name, int *out_token );
	void res_interrupt_requests_enable(void);
	void* res_init_interrupt_token(void);
	void res_delete_interrupt_token( void* token );
	void res_interrupt_request( void* token );
}

#pragma mark -
#pragma mark Cache Plugin
#pragma mark -

// used to initiate a query
struct sDNSQuery
{
    sDNSLookup          *fLookupEntry;
    int                 fQueryIndex;
	bool				fIsParallel;
	pthread_mutex_t     *fMutex;
    pthread_cond_t      *fCondition;
};

// state engine information
enum eResolveStates
{
    kResolveStateCheckDomain = 1,
    kResolveStateBuildExtraQueries,
    kResolveStateDoGetHostname,
    kResolveStateDoExtraQuery,
    kResolveStateDone
};

void CancelDNSThreads( void );

void dsSetNodeCacheAvailability( char *inNodeName, int inAvailable )
{
	if ( gCacheNode != NULL )
		gCacheNode->UpdateNodeReachability( inNodeName, inAvailable );
}

void dsFlushLibinfoCache( void )
{
	if ( gCacheNode != NULL )
		gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_ALL );
}

static void DNSChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument)
{
	gCacheNode->DNSConfigurationChanged();
}

// --------------------------------------------------------------------------------
//	* CCachePlugin ()
// --------------------------------------------------------------------------------

CCachePlugin::CCachePlugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName), fStatsLock("CCachePlugin::fStatsLock")
{
    fDirRef					= 0;
    fSearchNodeRef			= 0;
    fState					= kUnknownState;
    
    if ( gCacheNodeRef == NULL )
    {
        if (gServerOS)
        {
            gCacheNodeRef = new CPlugInRef( CCachePlugin::ContextDeallocProc, 1024 );
        }
        else
        {
            gCacheNodeRef = new CPlugInRef( CCachePlugin::ContextDeallocProc, 256 );
        }
    }
    
    if ( gCacheNodeContinue == NULL )
    {
        if (gServerOS)
        {
            gCacheNodeContinue = new CContinue( CCachePlugin::ContinueDeallocProc, 256 );
        }
        else
        {
            gCacheNodeContinue = new CContinue( CCachePlugin::ContinueDeallocProc, 64 );
        }
    }
    
    // we base our cache size on our physical memory, 3 sizes (small, medium, large) (<= 512 MB, <=2 GB, > 2GB)
    //    buckets = (512, 2048, 8192)
    //    cacheMax = (256, 512, 2048)
    //
    // cache doesn't really have a max, but it is used as a hint on how often to sweep the cache to attempt to
    // reduce expired entries, etc.
    
    int         mib[2]      = { CTL_HW, HW_MEMSIZE };
    uint64_t    memsize     = 0;
    size_t      len         = sizeof(memsize);
    uint32_t    maxSize     = 256;
    uint32_t    maxBuckets  = 512;
    
    // if we couldn't determine we'll just default to the lower values because memsize will be 0
    sysctl(mib, 2, &memsize, &len, NULL, 0);
    
    if ( memsize <= 512*1024*1024ULL )
    {
        maxSize = 256;
        maxBuckets = 512;
    }
    else if ( memsize <= 2*1024*1024*1024ULL )
    {
        maxSize = 512;
        maxBuckets = 2048;
    }
    else
    {
        maxSize = 2048;
        maxBuckets = 8192;
    }
    
    // don't replace on collision, will make names conflict
    fLibinfoCache = new CCache( maxSize, maxBuckets, 10, kCacheTime, 0 );
    
    fCacheHits = 0;
    fCacheMisses = 0;
    fTotalCalls = 0;
    fTotalCallTime = 0.0;
    fFlushCount = 0;
    fUnqualifiedSRVAllowed = true;
    fAlwaysDoAAAA = false;
    bzero( &fCallsByFunction, sizeof(fCallsByFunction) );
    bzero( &fCacheHitsByFunction, sizeof(fCacheHitsByFunction) );
    bzero( &fCacheMissByFunction, sizeof(fCacheMissByFunction) );

    dns_type_number( "CNAME", &fTypeCNAME );
    dns_type_number( "A", &fTypeA );
    dns_type_number( "AAAA", &fTypeAAAA );
    dns_type_number( "SRV", &fTypeSRV );
    dns_type_number( "PTR", &fTypePTR );
    dns_type_number( "MX", &fTypeMX );
    dns_class_number( "IN", &fClassIN );
	
	bzero( gActiveThreads, sizeof(gActiveThreads) );
	bzero( gNotifyTokens, sizeof(gNotifyTokens) );
	
	// let's enable the ability interrupt requests
	res_interrupt_requests_enable();
    
    // state engine initialization
    int iDoUnqualifiedSRV[] = { kResolveStateBuildExtraQueries,
                                kResolveStateDoGetHostname,
                                kResolveStateCheckDomain,
                                kResolveStateDone };
    int iNoUnqualifiedSRV[] = { kResolveStateBuildExtraQueries,
                                kResolveStateDoGetHostname,
                                kResolveStateCheckDomain,
                                kResolveStateBuildExtraQueries,
                                kResolveStateDoExtraQuery,
                                kResolveStateDone };
    
    if( fUnqualifiedSRVAllowed )
    {
        bcopy( iDoUnqualifiedSRV, fGetAddrStateEngine, sizeof(iDoUnqualifiedSRV) );
    }
    else
    {
        bcopy( iNoUnqualifiedSRV, fGetAddrStateEngine, sizeof(iNoUnqualifiedSRV) );
    }
    
    fLocalOnlyPIDs.insert( getpid() ); // add our own pid to the list

    fPluginInitialized = false;
} // CCachePlugin


// --------------------------------------------------------------------------------
//	* ~CCachePlugin ()
// --------------------------------------------------------------------------------

CCachePlugin::~CCachePlugin ( void )
{
    if (fDirRef != 0)
    {
        if (fSearchNodeRef != 0)
        {
            dsCloseDirNode(fSearchNodeRef);
        }
        dsCloseDirService( fDirRef );
    }
} // ~CCachePlugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

SInt32 CCachePlugin::Validate ( const char *inVersionStr, const UInt32 inSignature )
{
    fPlugInSignature = inSignature;
    
    fPluginInitialized = true;
    
    return( eDSNoErr );
} // Validate


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

SInt32 CCachePlugin::PeriodicTask ( void )
{
    fFlushCount++;
    
    // PeriodicTask happens every 30 seconds
    // Want to sweep the cache at 1/5 (or 1/10 under pressure) of the default cache time
    // which means we'll flush any expired entries at maximum 120% of the entry's TTL
    //   kCacheTime / 30 = how many periodic tasks
    //   periodic tasks / (5 or 10)
    
    int iCheckRate = (fLibinfoCache->isCacheOverMax() ? ((kCacheTime / 30) / 10) : ((kCacheTime / 30) / 5));
    if( fFlushCount > iCheckRate )
    {
        int iCount = fLibinfoCache->Sweep( CACHE_ENTRY_TYPE_ALL, true );

        if( iCount > 0 )
            DbgLog( kLogPlugin, "CCachePlugin::PeriodicTask - Flushed %d cache entries", iCount );

        fFlushCount = 0;
    }
    
    return( eDSNoErr );
} // PeriodicTask

// --------------------------------------------------------------------------------
//	* EmptyCacheEntryType ()
// --------------------------------------------------------------------------------

void CCachePlugin::EmptyCacheEntryType ( uint32_t inEntryType )
{
    if (gDSInstallDaemonMode) return;

    // if we are getting a request to empty all it's the equivalent of flushing the cache
    if ( inEntryType == CACHE_ENTRY_TYPE_ALL )
    {
        fLibinfoCache->Flush();
        DbgLog( kLogPlugin, "CCachePlugin::EmptyCacheEntryType - Request to empty all types - Flushing the cache" );
    }
    else
    {
        int iCount = fLibinfoCache->Sweep( inEntryType, false );
        if( iCount > 0 )
            DbgLog( kLogPlugin, "CCachePlugin::EmptyCacheEntryType - Flushed %d cache entries of type %X", iCount, inEntryType );
    }
} // EmptyCacheEntryType

// --------------------------------------------------------------------------------
//	* UpdateNodeReachability ()
// --------------------------------------------------------------------------------

void CCachePlugin::UpdateNodeReachability( char *inNodeName, bool inState )
{
    if (gDSInstallDaemonMode) return;

    int iCount = fLibinfoCache->UpdateAvailability( inNodeName, inState );
    if( iCount > 0 )
	{
        DbgLog( kLogPlugin, "CCachePlugin::UpdateNodeReachability - Updated %d cache entries for <%s> to <%s>", iCount, inNodeName, 
			   (inState ? "Available" : "Unavailable") );
	}
} // UpdateNodeReachability


// --------------------------------------------------------------------------------
//	* DNSConfigurationChanged ()
// --------------------------------------------------------------------------------

void CCachePlugin::DNSConfigurationChanged( void )
{
    CancelDNSThreads();

    int iCount = fLibinfoCache->Sweep( CACHE_ENTRY_TYPE_HOST, false );
	
    DbgLog( kLogPlugin, "CCachePlugin::DNSConfigurationChanged - flushed %d DNS cache entries", iCount );
} // DNSConfigurationChanged

// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

SInt32 CCachePlugin::SetPluginState ( const UInt32 inState )
{
    if ( inState & kActive )
    {
        //tell everyone we are ready to go
        WakeUpRequests();
    }
    return( eDSNoErr );
} // SetPluginState


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

SInt32 CCachePlugin::Initialize ( void )
{
    SInt32					siResult				= eDSNoErr;
    tDataBufferPtr			dataBuff				= NULL;
    UInt32					nodeCount				= 0;
    tContextData			context					= NULL;
    tDataListPtr			nodeName				= NULL;
    
    try
    {
        
        //TODO do we need to retry this if it fails - how can it possibly fail though?
        
        //verify the dirRef here and only open a new one if required
        //can't believe we ever need a new one since we are direct dispatch inside the daemon
        siResult = dsVerifyDirRefNum(fDirRef);
        if (siResult != eDSNoErr)
        {
            //this is to be used to service the Lookup calls
            //TODO do we need multithread capable search node here on single reference?
            
            // Get a directory services reference as a member variable
            siResult = dsOpenDirService( &fDirRef );
            if ( siResult != eDSNoErr ) throw( siResult );
            
            //get the search node reference as a member variable
            dataBuff = dsDataBufferAllocate( fDirRef, 256 );
            if ( dataBuff == NULL ) throw( (SInt32)eMemoryAllocError );
            
            siResult = dsFindDirNodes( fDirRef, dataBuff, NULL, eDSAuthenticationSearchNodeName, &nodeCount, &context );
            if ( siResult != eDSNoErr ) throw( siResult );
            if ( nodeCount < 1 ) throw( eDSNodeNotFound );
            
            siResult = dsGetDirNodeName( fDirRef, dataBuff, 1, &nodeName );
            if ( siResult != eDSNoErr ) throw( siResult );
            
            siResult = dsOpenDirNode( fDirRef, nodeName, &fSearchNodeRef );
            if ( nodeName != NULL )
            {
                dsDataListDeallocate( fDirRef, nodeName );
                DSFree( nodeName );
            }
            
            if ( siResult != eDSNoErr ) throw( siResult );

            // need to open the BSD local node specifically to bypass normal search policy when dispatching to ourself
            nodeName = dsBuildFromPath( fDirRef, "/BSD/local", "/" );
            siResult = dsOpenDirNode( fDirRef, nodeName, &fFlatFileNodeRef );
            if ( nodeName != NULL )
            {
                dsDataListDeallocate( fDirRef, nodeName );
                DSFree( nodeName );
            }
            
            // need to open the local node specifically to bypass normal search policy when dispatching to ourself
            siResult = dsFindDirNodes( fDirRef, dataBuff, NULL, eDSLocalNodeNames, &nodeCount, &context );
            if ( siResult != eDSNoErr ) throw( siResult );
            if ( nodeCount < 1 ) throw( eDSNodeNotFound );
            
            siResult = dsGetDirNodeName( fDirRef, dataBuff, 1, &nodeName );
            if ( siResult != eDSNoErr ) throw( siResult );
            
            siResult = dsOpenDirNode( fDirRef, nodeName, &fLocalNodeRef );
            if ( nodeName != NULL )
            {
                dsDataListDeallocate( fDirRef, nodeName );
                DSFree( nodeName );
            }
            
            if ( siResult != eDSNoErr ) throw( siResult );
            
            // set the cache node global after we've been initialized
            OSAtomicCompareAndSwapPtr( NULL, this, (void **) &gCacheNode );
        }
        
        //no impact if we re-register this node again
        nodeName = ::dsBuildFromPathPriv( kDSCacheNodeName , "/" );
        if( nodeName == NULL ) throw( eDSAllocationFailed );
        
        // TODO: register node once we are ready, for now, leave it unregistered
        CServerPlugin::_RegisterNode( fPlugInSignature, nodeName, kCacheNodeType );
//        CServerPlugin::_RegisterNode( fPlugInSignature, nodeName, kDirNodeType );
        
        dsDataListDeallocate( fDirRef, nodeName );
        DSFree( nodeName );
        
        checkAAAAstatus();
        
        // make cache node active
        fState = kUnknownState;
        fState += kInitialized;
        fState += kActive;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
        fState = kUnknownState;
        fState += kFailedToInit;
    }
    
    if ( dataBuff != NULL )
    {
        dsDataBufferDeAllocate( fDirRef, dataBuff );
        dataBuff = NULL;
    }
    if ( nodeName != NULL )
    {
        dsDataListDeallocate( fDirRef, nodeName );
        DSFree( nodeName );
    }
    
    return( siResult );
    
} // Initialize


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CCachePlugin::WakeUpRequests ( void )
{
	gKickCacheRequests.PostEvent();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CCachePlugin::WaitForInit ( void )
{
    // Now wait for 2 minutes until we are told that there is work to do or
    //	we wake up on our own and we will look for ourselves
    gKickCacheRequests.WaitForEvent( (UInt32)(2 * 60 * kMilliSecsPerSec) );
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

SInt32 CCachePlugin::ProcessRequest ( void *inData )
{
    SInt32		siResult	= eDSNoErr;
    char	   *pathStr		= NULL;
    
    try
    {
        if ( inData == NULL )
        {
            throw( (SInt32)ePlugInDataError );
        }
        
        if (((sHeader *)inData)->fType == kOpenDirNode)
        {
            if (((sOpenDirNode *)inData)->fInDirNodeName != NULL) //redundant check
            {
                pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
                if ( (pathStr != NULL) && (strncmp(pathStr, kDSCacheNodeName, 7) != 0) )
                {
                    throw( (SInt32)eDSOpenNodeFailed);
                }
            }
        }
        
        if ( ((sHeader *)inData)->fType == kServerRunLoop )
        {
            if ( (((sHeader *)inData)->fContextData) != NULL )
            {
                // now let's register for DNS change events cause we care about them
                CFStringRef             dnsKey				= NULL;	//DNS changes key
                CFMutableArrayRef       keys                = NULL;
                SCDynamicStoreRef       store				= NULL;
                CFRunLoopSourceRef      rls					= NULL;
                
                DbgLog( kLogApplication, "CCachePlugin::ProcessRequest registering for DNS Change Notifications" );
                
                keys	= CFArrayCreateMutable(	kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                
                //DNS changes
                dnsKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetDNS);
                
                CFArrayAppendValue(keys, dnsKey);
                CFRelease(dnsKey);
                
                store = SCDynamicStoreCreate(NULL, CFSTR("DirectoryService:CCachePlugin"), DNSChangeCallBack, NULL);
                if (store != NULL)
                {
                    SCDynamicStoreSetNotificationKeys(store, keys, NULL);
                    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
                    if (rls != NULL)
                    {
                        // we schedule notifications in the main loop, never on the plugin loop
                        CFRunLoopAddSource(CFRunLoopGetMain(), rls, kCFRunLoopDefaultMode);
                        CFRelease(rls);
                        rls = NULL;
                    }
                    else
                    {
                        syslog(LOG_ALERT, "CCachePlugin::ProcessRequest failed to create runloop source for DNS Notifications");
                    }
                }
                else
                {
                    syslog(LOG_ALERT, "CCachePlugin::ProcessRequest failed to register for DNS Notifications");
                }
                
                DSCFRelease(keys);
                DSCFRelease(store);
                
                return (siResult);
            }
        }
		else if( ((sHeader *)inData)->fType == kKerberosMutex )
		{
			// we don't care about these, just return
			return eDSNoErr;
		}
        
        WaitForInit();
        
        if (fState == kUnknownState)
        {
            throw( (SInt32)ePlugInCallTimedOut );
        }
        
        if ( (fState & kFailedToInit) || !(fState & kInitialized) )
        {
            throw( (SInt32)ePlugInFailedToInitialize );
        }
        
        if ( (fState & kInactive) || !(fState & kActive) )
        {
            throw( (SInt32)ePlugInNotActive );
        }
        
        if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
        {
            // explicitly empty negative entries, we'll pay the penalty of looking up again??
            EmptyCacheEntryType( CACHE_ENTRY_TYPE_NEGATIVE );
            checkAAAAstatus();
            siResult = eDSNoErr;
        }
        else if (((sHeader *)inData)->fType == kHandleSystemWillPowerOn)
        {
            siResult = eDSNoErr; //TODO we do nothing now on kHandleSystemWillPowerOn
        }
        else
        {
            siResult = HandleRequest( inData );
        }
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    DSFree( pathStr );
    
    return( siResult );
    
} // ProcessRequest


// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

SInt32 CCachePlugin::HandleRequest ( void *inData )
{
    SInt32				siResult	= eDSNoErr;
    sHeader			   *pMsgHdr		= NULL;
    
    if ( !fPluginInitialized )
    {
        DbgLog( kLogPlugin, "CCachePlugin::HandleRequest called when CCachePlugin hasn't finished being initialized" );
        return ePlugInInitError;
    }
    
    try
    {
        pMsgHdr = (sHeader *)inData;
        
        switch ( pMsgHdr->fType )
        {
            case kOpenDirNode:
                siResult = OpenDirNode( (sOpenDirNode *)inData );
                break;
                
            case kCloseDirNode:
                siResult = CloseDirNode( (sCloseDirNode *)inData );
                break;
                
            case kGetDirNodeInfo:
                siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
                break;
                
            case kGetRecordList:
                siResult = GetRecordList( (sGetRecordList *)inData );
                break;
                
            case kReleaseContinueData:
                siResult = ReleaseContinueData( (sReleaseContinueData *)inData );
                break;
                
            case kGetRecordEntry:
                siResult = GetRecordEntry( (sGetRecordEntry *)inData );
                break;
                
            case kGetAttributeEntry:
                siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
                break;
                
            case kGetAttributeValue:
                siResult = GetAttributeValue( (sGetAttributeValue *)inData );
                break;
                
            case kDoAttributeValueSearch:
            case kDoAttributeValueSearchWithData:
                siResult = AttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
                break;
                
            case kDoMultipleAttributeValueSearch:
            case kDoMultipleAttributeValueSearchWithData:
                siResult = MultipleAttributeValueSearch( (sDoMultiAttrValueSearchWithData *)inData );
                break;
                
            case kCloseAttributeList:
                siResult = CloseAttributeList( (sCloseAttributeList *)inData );
                break;
                
            case kCloseAttributeValueList:
                siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
                break;
                
            case kDoPlugInCustomCall:
                siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
                break;
                
            case kServerRunLoop:
                siResult = eDSNoErr; //handled above already
                break;
                
            default:
                siResult = eNotHandledByThisNode;
                break;
        }
        
        pMsgHdr->fResult = siResult;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    catch ( ... )
    {
        siResult = ePlugInError;
    }
    
    return( siResult );
    
} // HandleRequest


// ---------------------------------------------------------------------------
//	* ProcessLookupRequest
//
// ---------------------------------------------------------------------------

kvbuf_t* CCachePlugin::ProcessLookupRequest ( int inProcNumber, char* inData, int inCount, pid_t inPID )
{
    kvbuf_t         *outData    = NULL;
    kvbuf_t         *buffer     = (inCount != 0 ? kvbuf_init(inData, inCount) : NULL);
    double          startTime   = dsTimestamp();
    
    switch ( inProcNumber )
    {
        case kDSLUgetpwnam:
            outData = DSgetpwnam( buffer, inPID );
            break;
            
        case kDSLUgetpwuuid:
            outData = DSgetpwuuid( buffer, inPID );
            break;
            
        case kDSLUgetpwuid:
            outData = DSgetpwuid( buffer, inPID );
            break;
            
        case kDSLUgetpwent:
            outData = DSgetpwent();
            break;
            
        case kDSLUgetgrnam:
            outData = DSgetgrnam( buffer, inPID );
            break;
            
        case kDSLUgetgrgid:
            outData = DSgetgrgid( buffer, inPID );
            break;
            
        case kDSLUgetgrent:
            outData = DSgetgrent();
            break;
            
        case kDSLUgetfsbyname:
            outData = DSgetfsbyname( buffer );
            break;
            
        case kDSLUgetfsent:
            outData = DSgetfsent( inPID );
            break;
            
        case kDSLUalias_getbyname: // Aliases from directory
            outData = DSgetaliasbyname( buffer );
            break;
            
        case kDSLUalias_getent: // Aliases from directory
            outData = DSgetaliasent();
            break;
            
        case kDSLUgetservbyname: // /etc/services??
            outData = DSgetservbyname( buffer, inPID );
            break;
            
        case kDSLUgetservbyport: // /etc/services??
            outData = DSgetservbyport( buffer, inPID );
            break;
            
        case kDSLUgetservent: // /etc/services??
            outData = DSgetservent();
            break;
            
        case kDSLUgetprotobyname: // /etc/protocols
            outData = DSgetprotobyname( buffer, inPID );
            break;
            
        case kDSLUgetprotobynumber: // /etc/protocols
            outData = DSgetprotobynumber( buffer, inPID );
            break;
            
        case kDSLUgetprotoent: // /etc/protocols
            outData = DSgetprotoent();
            break;
            
        case kDSLUgetrpcbyname: // /etc/rpc
            outData = DSgetrpcbyname( buffer, inPID );
            break;
            
        case kDSLUgetrpcbynumber: // /etc/rpc
            outData = DSgetrpcbynumber( buffer, inPID );
            break;
            
        case kDSLUgetrpcent: // /etc/rpc
            outData = DSgetrpcent();
            break;
            
        case kDSLUgetnetent:
            outData = DSgetnetent();
            break;
            
        case kDSLUgetnetbyname:
            outData = DSgetnetbyname( buffer, inPID );
            break;
            
        case kDSLUgetnetbyaddr:
            outData = DSgetnetbyaddr( buffer, inPID );
            break;
            
        case kDSLUinnetgr:
            outData = DSinnetgr( buffer );
            break;
            
        case kDSLUgetnetgrent:
            outData = DSgetnetgrent( buffer );
            break;
            
        case kDSLUflushcache:
            DbgLog( kLogPlugin, "CCachePlugin::flushcache request" );
            fLibinfoCache->Flush();
            break;
            
        case kDSLUflushentry:
            // TODO:  need design for flushing an entry
            break;
            
        case kDSLUgetaddrinfo:
            outData = DSgetaddrinfo( buffer, inPID );
            break;
                
        case kDSLUgetnameinfo:
            outData = DSgetnameinfo( buffer, inPID );
            break;
            
        case kDSLUgethostbyaddr:
            outData = DSgethostbyaddr( buffer, inPID );
            break;
            
        case kDSLUgethostbyname:
            outData = DSgethostbyname( buffer, inPID );
            break;
            
        case kDSLUgethostent:
            outData = DSgethostent( buffer, inPID );
            break;
            
        case kDSLUgetmacbyname:
            outData = DSgetmacbyname( buffer, inPID );
            break;
            
        case kDSLUgethostbymac:
            outData = DSgethostbymac( buffer, inPID );
            break;
        
        case kDSLUdns_proxy:
            outData = DSdns_proxy( buffer, inPID );
            break;
        
        case kDSLUgetbootpbyhw:
            outData = DSgetbootpbyhw( buffer, inPID );
            break;
        
        case kDSLUgetbootpbyaddr:
            outData = DSgetbootpbyaddr( buffer, inPID );
            break;
            
        default:
            break;
    }
    
    if( buffer != NULL )
    {
        kvbuf_free( buffer );
        buffer = NULL;
    }
    
    // if this is an empty response, let's ensure we only return it for address lookups
    if ( outData != NULL && outData->datalen == sizeof(uint32_t) )
    {
        switch ( inProcNumber )
        {
            // these 4 types return empty responses, otherwise we free it
            case kDSLUgetaddrinfo:
            case kDSLUgetnameinfo:
            case kDSLUgethostbyaddr:
            case kDSLUgethostbyname:
                break;
                
            default:
                kvbuf_free( outData );
                outData = NULL;
                break;
        }
    }
    
    fStatsLock.WaitLock();
    
    fTotalCallTime += dsTimestamp() - startTime;
    fTotalCalls++;
    fCallsByFunction[inProcNumber] += 1;
    
    fStatsLock.SignalLock();
    
    return( outData );
    
} // ProcessLookupRequest


#pragma mark -
#pragma mark DS API Service Routines - Clients using Cache Node directly
#pragma mark -

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::OpenDirNode ( sOpenDirNode *inData )
{
    SInt32					siResult	= eDSOpenNodeFailed;
    char				   *pathStr		= NULL;
    sCacheContextData	   *pContext	= NULL;
    
    if ( inData != NULL )
    {
        pathStr = ::dsGetPathFromListPriv( inData->fInDirNodeName, "/" );
        if ( ( pathStr != NULL ) && ( ::strcmp( pathStr, kDSCacheNodeName ) == 0 ) )
        {
            pContext = MakeContextData();
            if (pContext != NULL)
            {
                pContext->fNodeName		= pathStr;
                pContext->fUID			= inData->fInUID;
                pContext->fEffectiveUID	= inData->fInEffectiveUID;
                gCacheNodeRef->AddItem( inData->fOutNodeRef, pContext );
                siResult = eDSNoErr;
            }
        }
    }
    
    return( siResult );
    
} // OpenDirNode


//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::CloseDirNode ( sCloseDirNode *inData )
{
    SInt32					siResult		= eDSNoErr;
    sCacheContextData	   *pContext		= NULL;
    
    try
    {
        pContext = (sCacheContextData *) gCacheNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32)eDSInvalidNodeRef );
        
        gCacheNodeRef->RemoveItem( inData->fInNodeRef );
        gCacheNodeContinue->RemoveItems( inData->fInNodeRef );
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // CloseDirNode

static CFDateRef ConvertBSDTimeToCFDate( time_t inTime )
{
	CFGregorianDate gregorianDate;
    
    struct tm *bsdDate = gmtime( &inTime );
	
	gregorianDate.second = bsdDate->tm_sec;
	gregorianDate.minute = bsdDate->tm_min;
	gregorianDate.hour = bsdDate->tm_hour;
	gregorianDate.day = bsdDate->tm_mday;
	gregorianDate.month = bsdDate->tm_mon + 1;
	gregorianDate.year = bsdDate->tm_year + 1900;
	
	return CFDateCreate( kCFAllocatorDefault, CFGregorianDateGetAbsoluteTime(gregorianDate, NULL) );
}

//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
    SInt32				siResult		= eDSNoErr;
    UInt32				uiOffset		= 0;
    UInt32				uiCntr			= 1;
    UInt32				uiAttrCnt		= 0;
    CAttributeList	   *inAttrList		= NULL;
    char			   *pAttrName		= NULL;
    char			   *pData			= NULL;
    sCacheContextData *pAttrContext	= NULL;
    CBuff				outBuff;
    sCacheContextData *pContext		= NULL;
    CDataBuff	 	   *aRecData		= NULL;
    CDataBuff	 	   *aAttrData		= NULL;
    CDataBuff	 	   *aTmpData		= NULL;
    UInt32				cacheNodeNameBufLen = 0;
    SInt32				buffResult		= eDSNoErr;
    
    //can ask for any of the following
    // kDSAttributesAll
    // kDS1AttrReadOnlyNode
    
    try
    {
        
        if ( inData  == NULL ) throw( (SInt32)eMemoryError );
        
        pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32)eDSInvalidNodeRef );
        
        inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
        if ( inAttrList == NULL ) throw( (SInt32)eDSNullNodeInfoTypeList );
        if (inAttrList->GetCount() == 0) throw( (SInt32)eDSEmptyNodeInfoTypeList );
        
        siResult = outBuff.Initialize( inData->fOutDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );
        
        // Set the real buffer type
        siResult = outBuff.SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );
        
        aRecData = new CDataBuff();
        if ( aRecData  == nil ) throw( (SInt32) eMemoryError );
        aAttrData = new CDataBuff();
        if ( aAttrData  == nil ) throw( (SInt32) eMemoryError );
        aTmpData = new CDataBuff();
        if ( aTmpData  == nil ) throw( (SInt32) eMemoryError );
        
        // Set the record name and type
        aRecData->AppendShort( ::strlen( "dsRecTypeStandard:CacheNodeInfo" ) );
        aRecData->AppendString( "dsRecTypeStandard:CacheNodeInfo" );
        if (pContext->fNodeName != NULL)
        {
            cacheNodeNameBufLen = strlen( pContext->fNodeName );
            aRecData->AppendShort( cacheNodeNameBufLen );
            cacheNodeNameBufLen += 2;
            aRecData->AppendString( pContext->fNodeName );
        }
        else
        {
            aRecData->AppendShort( ::strlen( "CacheNodeInfo" ) );
            aRecData->AppendString( "CacheNodeInfo" );
            cacheNodeNameBufLen = 15; //2 + 13 = 15
        }
        
        while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
        {
            //package up all the dir node attributes dependant upon what was asked for
            
            if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
                 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
            {
                uiAttrCnt++;
                
                //possible for a node to be ReadOnly, ReadWrite, WriteOnly
                //note that ReadWrite does not imply fully readable or writable
                buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrReadOnlyNode, "ReadOnly" );
            }
            
            if ( strcmp( pAttrName, "dsAttrTypeNative:LibinfoCacheOverview" ) == 0 || strcmp( pAttrName, "dsAttrTypeNative:LibinfoCacheDetails" ) == 0 )
            {
                // we build a plist with the statistics
                CFMutableDictionaryRef  cfInformation = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                                   &kCFTypeDictionaryValueCallBacks );
                
                CFMutableArrayRef  cfEntries = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
                
                // let's extract some information, we need to lock the cache
                fLibinfoCache->fCacheLock.WaitLock();
                
                // first let's get cache-level info
                CFStringRef bucket_count = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), fLibinfoCache->fBucketCount );
                CFDictionarySetValue( cfInformation, CFSTR("Hash Slots"), bucket_count );
                CFRelease( bucket_count );
                
                CFStringRef ttl = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), fLibinfoCache->fCacheTTL );
                CFDictionarySetValue( cfInformation, CFSTR("Default TTL"), ttl );
                CFRelease( ttl );

                CFStringRef cur_size = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), fLibinfoCache->GetCount() );
                CFDictionarySetValue( cfInformation, CFSTR("Cache Size"), cur_size );
                CFRelease( cur_size );
                
                CFStringRef max_size = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), fLibinfoCache->fMaxSize );
                CFDictionarySetValue( cfInformation, CFSTR("Cache Cap"), max_size );
                CFRelease( max_size );
                
                CFStringRef policy_flags = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), fLibinfoCache->fPolicyFlags );
                CFDictionarySetValue( cfInformation, CFSTR("Policy Flags"), policy_flags );
                CFRelease( policy_flags );

                CFDictionarySetValue( cfInformation, CFSTR("AAAA Queries"), 
									  (aaaa_cutoff_enabled ? CFSTR("Disabled (link-local IPv6 addresses)") : CFSTR("Enabled")) );
				
                CFMutableArrayRef cfSlotInfo = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
                CFDictionarySetValue( cfInformation, CFSTR("Bucket Info"), cfSlotInfo );
                CFRelease( cfSlotInfo );

                CFIndex iSlotsUsed = 0;
                CFIndex iCount = (CFIndex) fLibinfoCache->fBucketCount;
                CFIndex iMaxSlotDepth = 0;
                for (CFIndex ii = 0; ii < iCount; ii++)
                {
                    if (fLibinfoCache->fBuckets[ii] != NULL)
                    {
                        iSlotsUsed++;

                        CFIndex iDepth = fLibinfoCache->fBuckets[ii]->fCount;

                        CFNumberRef cfBucket = CFNumberCreate( kCFAllocatorDefault, kCFNumberCFIndexType, &ii );
                        CFNumberRef cfDepth = CFNumberCreate( kCFAllocatorDefault, kCFNumberCFIndexType, &iDepth );
                        
                        CFTypeRef keys[] = { CFSTR("Bucket"), CFSTR("Depth") };
                        CFTypeRef values[] = { cfBucket, cfDepth };
                        
                        CFDictionaryRef cfDict = CFDictionaryCreate( kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, 
                                                                     &kCFTypeDictionaryValueCallBacks );

                        CFArrayAppendValue( cfSlotInfo, cfDict );

                        if (iDepth > iMaxSlotDepth) iMaxSlotDepth = iDepth;
                        
                        CFRelease( cfDict );
                        CFRelease( cfBucket );
                        CFRelease( cfDepth );
                    }
                }

                CFNumberRef cfMaxDepth = CFNumberCreate( kCFAllocatorDefault, kCFNumberCFIndexType, &iMaxSlotDepth );
                CFDictionarySetValue( cfInformation, CFSTR("Max Bucket Depth"), cfMaxDepth );
                CFRelease( cfMaxDepth );
                
                CFNumberRef cfSlotsUsed = CFNumberCreate( kCFAllocatorDefault, kCFNumberCFIndexType, &iSlotsUsed );
                CFDictionarySetValue( cfInformation, CFSTR("Buckets Used"), cfSlotsUsed );
                CFRelease( cfSlotsUsed );

                // build an overview of the cache info to minimize data sent to client request
                CFStringRef typesStr[]      = { CFSTR("User"), CFSTR("Group"), CFSTR("Host"), CFSTR("Service"), CFSTR("Computer"),
                                                CFSTR("Mount"), CFSTR("Alias"), CFSTR("Protocol"), CFSTR("RPC"), CFSTR("Network") };
                uint32_t    cacheTypes[]    = { CACHE_ENTRY_TYPE_USER, CACHE_ENTRY_TYPE_GROUP, CACHE_ENTRY_TYPE_HOST, CACHE_ENTRY_TYPE_SERVICE,
                                                CACHE_ENTRY_TYPE_COMPUTER, CACHE_ENTRY_TYPE_MOUNT, CACHE_ENTRY_TYPE_ALIAS, CACHE_ENTRY_TYPE_PROTOCOL,
                                                CACHE_ENTRY_TYPE_RPC, CACHE_ENTRY_TYPE_NETWORK };
                int         typeCnt         = sizeof(cacheTypes) / sizeof(uint32_t);
                CFIndex     counts[typeCnt];
                
                bzero( counts, sizeof(counts) );
                      
                sCacheEntry *entry          = fLibinfoCache->fHead;
                while (entry != NULL)
                {
                    // count up the types
                    int iType = entry->fFlags & 0x00000FFF;
                    switch( iType )
                    {
                        case CACHE_ENTRY_TYPE_USER:
                            counts[0] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_GROUP:
                            counts[1] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_HOST:
                            counts[2] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_SERVICE:
                            counts[3] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_COMPUTER:
                            counts[4] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_MOUNT:
                            counts[5] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_ALIAS:
                            counts[6] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_PROTOCOL:
                            counts[7] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_RPC:
                            counts[8] += 1;
                            break;
                        case CACHE_ENTRY_TYPE_NETWORK:
                            counts[9] += 1;
                            break;
                        default:
                            break;
                    }
                    entry = entry->fNext;
                }
                
                CFMutableDictionaryRef  cfCounts = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                              &kCFTypeDictionaryValueCallBacks );
                CFDictionarySetValue( cfInformation, CFSTR("Counts By Type"), cfCounts );
                CFRelease( cfCounts );

                for ( int ii = 0; ii < typeCnt; ii++ )
                {
                    if ( counts[ii] > 0 )
                    {
                        CFNumberRef cfNumber = CFNumberCreate( kCFAllocatorDefault, kCFNumberCFIndexType, &counts[ii] );
                        
                        CFDictionarySetValue( cfCounts, typesStr[ii], cfNumber );
                        
                        CFRelease( cfNumber );
                    }
                }
                
                if ( strcmp( pAttrName, "dsAttrTypeNative:LibinfoCacheDetails" ) == 0 )
                {
                    // now go through entries and dump them
                    entry = fLibinfoCache->fHead;
                    while (entry != NULL)
                    {
                        CFMutableDictionaryRef	cfEntry = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                                     &kCFTypeDictionaryValueCallBacks );
                        
                        if ( entry->fKeyList.fCount > 0 )
                        {
                            CFMutableArrayRef cfKeys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
                            
                            sKeyListItem *item = entry->fKeyList.fHead;
                            while (item != NULL)
                            {
                                // no need to copy the string, just use the stuff directly since we will be turning into XML anyway
                                CFStringRef cfKeyValue = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, item->fKey, kCFStringEncodingUTF8, 
                                                                                          kCFAllocatorNull );
                                
                                CFArrayAppendValue( cfKeys, cfKeyValue );
                                CFRelease( cfKeyValue );
                                
                                item = item->fNext;
                            }
                            
                            CFDictionarySetValue( cfEntry, CFSTR("Keys"), cfKeys );
                            CFRelease( cfKeys );
                        }
                        
                        sCacheValidation *valid_t = entry->fValidation;
                        if (valid_t != NULL)
                        {
                            CFMutableDictionaryRef validation = CFDictionaryCreateMutable( kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, 
                                                                                          &kCFTypeDictionaryValueCallBacks );
                            
                            if ( valid_t->fNode != NULL )
                            {
                                CFStringRef cfNode = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, valid_t->fNode, kCFStringEncodingUTF8, 
                                                                                      kCFAllocatorNull );
                                CFDictionarySetValue( validation, CFSTR("Name"), cfNode );
                                CFRelease( cfNode );
                            }
                            
                            CFStringRef cfToken = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), valid_t->fToken );
                            CFStringRef cfRefs = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), valid_t->fRefCount );
                            
                            CFDictionarySetValue( validation, CFSTR("Available"), (valid_t->fNodeAvailable ? kCFBooleanTrue : kCFBooleanFalse) );
                            CFDictionarySetValue( validation, CFSTR("Token"), cfToken );
                            CFDictionarySetValue( validation, CFSTR("Reference Count"), cfRefs );
                            
                            CFRelease( cfToken );
                            CFRelease( cfRefs );
                            
                            CFDictionarySetValue( cfEntry, CFSTR("Validation Information"), validation );
                            CFRelease( validation );
                        }
                        
                        if ( entry->fTTL )
                        {
                            CFStringRef ttl2 = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), entry->fTTL );
                            CFDictionarySetValue( cfEntry, CFSTR("TTL"), ttl2 );
                            CFRelease( ttl2 );
                        }
                        
                        CFDateRef cfBestBefore = ConvertBSDTimeToCFDate( entry->fBestBefore );
                        if (cfBestBefore)
                        {
                            CFDictionarySetValue( cfEntry, CFSTR("Best Before"), cfBestBefore );
                            CFRelease( cfBestBefore );
                        }

                        CFDateRef cfLastAccess = ConvertBSDTimeToCFDate( entry->fLastAccess );
                        if (cfLastAccess)
                        {
                            CFDictionarySetValue( cfEntry, CFSTR("Last Access"), cfLastAccess );
                            CFRelease( cfLastAccess );
                        }
                        
                        CFStringRef hits = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), entry->fHits );
                        CFDictionarySetValue( cfEntry, CFSTR("Hits"), hits );
                        CFRelease( hits );
                        
                        CFStringRef cfRefs = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), entry->fRefCount );
                        CFDictionarySetValue( cfEntry, CFSTR("Reference Count"), cfRefs );
                        CFRelease( cfRefs );
                        
                        int iType = entry->fFlags & 0x00000FFF;
                        switch( iType )
                        {
                            case CACHE_ENTRY_TYPE_USER:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("User") );
                                break;
                            case CACHE_ENTRY_TYPE_GROUP:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Group") );
                                break;
                            case CACHE_ENTRY_TYPE_HOST:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Host") );
                                break;
                            case CACHE_ENTRY_TYPE_SERVICE:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Service") );
                                break;
                            case CACHE_ENTRY_TYPE_COMPUTER:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Computer") );
                                break;
                            case CACHE_ENTRY_TYPE_MOUNT:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Mount") );
                                break;
                            case CACHE_ENTRY_TYPE_ALIAS:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Alias") );
                                break;
                            case CACHE_ENTRY_TYPE_PROTOCOL:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Protocol") );
                                break;
                            case CACHE_ENTRY_TYPE_RPC:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("RPC") );
                                break;
                            case CACHE_ENTRY_TYPE_NETWORK:
                                CFDictionarySetValue( cfEntry, CFSTR("Type"), CFSTR("Network") );
                                break;
                            default:
                                break;
                        }
                        
                        if (entry->fFlags & CACHE_ENTRY_TYPE_NEGATIVE)
                            CFDictionarySetValue( cfEntry, CFSTR("Negative Entry"), kCFBooleanTrue );
                        
                        CFArrayAppendValue( cfEntries, cfEntry );
                        CFRelease( cfEntry );
                        
                        entry = entry->fNext;
                    }

                    CFDictionarySetValue( cfInformation, CFSTR("Entries"), cfEntries );
                }

                fLibinfoCache->fCacheLock.SignalLock();

                CFDataRef cfData = CFPropertyListCreateXMLData( kCFAllocatorDefault, cfInformation );
                
				uiAttrCnt++;

                buffResult = dsCDataBuffFromAttrTypeAndData( aAttrData, aTmpData, inData->fInAttrInfoOnly, pAttrName, 
                                                            (const char *) CFDataGetBytePtr(cfData), (UInt32) CFDataGetLength(cfData) );
                
                DSCFRelease( cfEntries );
                DSCFRelease( cfInformation );
				DSCFRelease( cfData );
            }
            
            if ( strcmp( pAttrName, "dsAttrTypeNative:Statistics" ) == 0 )
            {
                uiAttrCnt++;
                
                // Format:
                // CFArray -> CFDictionaries
                //              Keys (columns)  - "Category" required
                //              subValues       - CFArray of CFDictionaries to match above keys
                
                CFMutableArrayRef       cfStats = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
                
                // we build a plist with the statistics
                CFMutableDictionaryRef  cfGlobalStats = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                                   &kCFTypeDictionaryValueCallBacks );
                
                fStatsLock.WaitLock();
                
                double averageTime = 0.0;
                
                if( fTotalCalls > 0 )
                {
                    averageTime = (fTotalCallTime / (double) fTotalCalls) / (double) USEC_PER_SEC;  //fTotalCallTime is in microseconds
                }
                
                CFStringRef cfCacheHits = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fCacheHits );
                CFStringRef cfCacheMisses = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fCacheMisses );
                CFStringRef cfTotalCalls = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fTotalCalls );
                CFStringRef cfAverageCall = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%f"), averageTime );

                CFDictionarySetValue( cfGlobalStats, CFSTR("Category"), CFSTR("Global Statistics") );
                CFDictionarySetValue( cfGlobalStats, CFSTR("Cache Hits"), cfCacheHits );
                CFDictionarySetValue( cfGlobalStats, CFSTR("Cache Misses"), cfCacheMisses );
                CFDictionarySetValue( cfGlobalStats, CFSTR("Total External Calls"), cfTotalCalls );
                CFDictionarySetValue( cfGlobalStats, CFSTR("Average Call Time"), cfAverageCall );
                
                DSCFRelease( cfCacheHits );
                DSCFRelease( cfCacheMisses );
                DSCFRelease( cfTotalCalls );
                DSCFRelease( cfAverageCall );
                
                CFArrayAppendValue( cfStats, cfGlobalStats );
                DSCFRelease( cfGlobalStats );
                
                CFMutableArrayRef       cfProcedureStats = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
                CFMutableDictionaryRef  cfTempDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                                &kCFTypeDictionaryValueCallBacks );
                
                CFDictionarySetValue( cfTempDict, CFSTR("subValues"), cfProcedureStats );
                CFDictionarySetValue( cfTempDict, CFSTR("Category"), CFSTR("Procedure Statistics") );
                
                CFArrayAppendValue( cfStats, cfTempDict );
                DSCFRelease( cfTempDict );

                for( int ii = 1; ii < kDSLUlastprocnum; ii++ )
                {
                    CFStringRef cfProcedure;
					
                    cfTempDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                                    &kCFTypeDictionaryValueCallBacks );
                    cfProcedure = CFStringCreateWithCString( kCFAllocatorDefault, lookupProcedures[ii], kCFStringEncodingUTF8 );
                    cfCacheHits = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fCacheHitsByFunction[ii] );
                    cfCacheMisses = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fCacheMissByFunction[ii] );
                    cfTotalCalls = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%lld"), fCallsByFunction[ii] );

                    CFDictionarySetValue( cfTempDict, CFSTR("Category"), cfProcedure );
                    CFDictionarySetValue( cfTempDict, CFSTR("Cache Hits"), cfCacheHits );
                    CFDictionarySetValue( cfTempDict, CFSTR("Cache Misses"), cfCacheMisses );
                    CFDictionarySetValue( cfTempDict, CFSTR("Total Calls"), cfTotalCalls );

                    CFArrayAppendValue( cfProcedureStats, cfTempDict );

                    DSCFRelease( cfCacheHits );
                    DSCFRelease( cfCacheMisses );
                    DSCFRelease( cfTotalCalls );
                    DSCFRelease( cfProcedure );
                    DSCFRelease( cfTempDict );
                }
                
                DSCFRelease( cfProcedureStats );
                    
                fStatsLock.SignalLock();
                
                CFDataRef cfStatData = CFPropertyListCreateXMLData( kCFAllocatorDefault, cfStats );
                
                buffResult = dsCDataBuffFromAttrTypeAndData( aAttrData, aTmpData, inData->fInAttrInfoOnly, "dsAttrTypeNative:Statistics", 
                                                             (const char *) CFDataGetBytePtr(cfStatData), (UInt32) CFDataGetLength(cfStatData) );
                
                DSCFRelease( cfStatData );
                DSCFRelease( cfStats );
            }
            
        } // while loop over the attributes requested
        
        aRecData->AppendShort( uiAttrCnt );
        if (uiAttrCnt > 0)
        {
            aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
        }
        
        outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
        inData->fOutAttrInfoCount = uiAttrCnt;
        
        pData = outBuff.GetDataBlock( 1, &uiOffset );
        if ( pData != NULL )
        {
            pAttrContext = MakeContextData();
            if ( pAttrContext  == nil ) throw( (SInt32) eMemoryAllocError );
            
            //add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
            //		record length = 4
            //		aRecData->AppendShort( ::strlen( "dsRecTypeStandard:CacheNodeInfo" ) ); = 2
            //		aRecData->AppendString( "dsRecTypeStandard:CacheNodeInfo" ); = 31
            //		aRecData->AppendShort( ::strlen( "CacheNodeInfo" ) ); = see above for distinct node
            //		aRecData->AppendString( "CacheNodeInfo" ); = see above for distinct node
            //		total adjustment = 4 + 2 + 31 + 2 + 13 = 36
            
            pAttrContext->offset = uiOffset + 36 + cacheNodeNameBufLen;
            
            gCacheNodeRef->AddItem( inData->fOutAttrListRef, pAttrContext );
        }
        else
        {
            siResult = eDSBufferTooSmall;
        }
        
        inData->fOutDataBuff->fBufferLength = inData->fOutDataBuff->fBufferSize;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    DSDelete( inAttrList );
    DSDelete( aRecData );
    DSDelete( aAttrData );
    DSDelete( aTmpData );
    
    return( siResult );
    
} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::GetRecordList ( sGetRecordList *inData )
{
    SInt32					siResult		= eDSNoErr;
    sCacheContinueData	   *pContinue		= NULL;
    sCacheContextData	   *pContext		= NULL;
    
    //TODO build the cache key from the inpout data request
    
    //TODO here before we call thru to the search node we consult the cache
    
    //Now use the search node if cache has no results
    try
    {
        pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32)eDSInvalidNodeRef );
        
        //TODO need to flush out for what we carry around the continue data here ie. pass thru likely only
        if ( inData->fIOContinueData == NULL )
        {
            pContinue = (sCacheContinueData *)::calloc( 1, sizeof( sCacheContinueData ) );
            if ( pContinue == NULL ) throw( (SInt32)eMemoryAllocError );
            
            gCacheNodeContinue->AddItem( pContinue, inData->fInNodeRef );
            
            //TODO figure out if we need to open separate refs
            pContinue->fDirRef = fDirRef;
            pContinue->fNodeRef= fSearchNodeRef;
            pContinue->fContinue = NULL;
            
            pContinue->fLimitRecSearch	= 0;
            //check if the client has requested a limit on the number of records to return
            //we only do this the first call into this context for pContinue
            if (inData->fOutRecEntryCount >= 0)
            {
                pContinue->fLimitRecSearch = inData->fOutRecEntryCount;
            }
        }
        else
        {
            pContinue = (sCacheContinueData *)inData->fIOContinueData;
            if ( gCacheNodeContinue->VerifyItem( pContinue ) == false )
            {
                throw( (SInt32)eDSInvalidContinueData );
            }
        }
        
        // Pass the out buffer on thru as well as the continue data
        
        siResult = dsGetRecordList( pContinue->fNodeRef,
                                    inData->fInDataBuff,
                                    inData->fInRecNameList,
                                    inData->fInPatternMatch,
                                    inData->fInRecTypeList,
                                    inData->fInAttribTypeList,
                                    inData->fInAttribInfoOnly,
                                    &inData->fOutRecEntryCount,
                                    &pContinue->fContinue );
        
        if (pContinue->fContinue != NULL)
        {
            inData->fIOContinueData = (tContextData *)pContinue;
        }
        else
        {
            inData->fIOContinueData = NULL;
            gCacheNodeContinue->RemoveItem( pContinue );
        }
        
        //TODO here we add to the cache of the results returned
        //need to be smart and determine if the results is too big to cache
        //also need to cache across continue data for the cache key
        //ie. retrieval takes multiple calls just like an original call
        
        
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // GetRecordList


//------------------------------------------------------------------------------------
//	* AttributeValueSearch
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::AttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
    SInt32					siResult		= eDSNoErr;
    sCacheContinueData	   *pContinue		= NULL;
    sCacheContextData	   *pContext		= NULL;
    
    //TODO build the cache key from the inpout data request
    
    //TODO here before we call thru to the search node we consult the cache
    
    //Now use the search node if cache has no results
    try
    {
        pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32)eDSInvalidNodeRef );
        
        //TODO need to flush out for what we carry around the continue data here ie. pass thru likely only
        if ( inData->fIOContinueData == NULL )
        {
            pContinue = (sCacheContinueData *)::calloc( 1, sizeof( sCacheContinueData ) );
            if ( pContinue == NULL ) throw( (SInt32)eMemoryAllocError );
            
            gCacheNodeContinue->AddItem( pContinue, inData->fInNodeRef );
            
            //TODO figure out if we need to open separate refs
            pContinue->fDirRef = fDirRef;
            pContinue->fNodeRef= fSearchNodeRef;
            pContinue->fContinue = NULL;
            
            pContinue->fLimitRecSearch	= 0;
            //check if the client has requested a limit on the number of records to return
            //we only do this the first call into this context for pContinue
            if (inData->fOutMatchRecordCount >= 0)
            {
                pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
            }
        }
        else
        {
            pContinue = (sCacheContinueData *)inData->fIOContinueData;
            if ( gCacheNodeContinue->VerifyItem( pContinue ) == false )
            {
                throw( (SInt32)eDSInvalidContinueData );
            }
        }
        
        // Pass the out buffer on thru as well as the continue data
        
        if ( inData->fType == kDoAttributeValueSearchWithData )
        {
            siResult = ::dsDoAttributeValueSearchWithData(
                                                          pContinue->fNodeRef,
                                                          inData->fOutDataBuff,
                                                          inData->fInRecTypeList,
                                                          inData->fInAttrType,
                                                          inData->fInPattMatchType,
                                                          inData->fInPatt2Match,
                                                          inData->fInAttrTypeRequestList,
                                                          inData->fInAttrInfoOnly,
                                                          &inData->fOutMatchRecordCount,
                                                          &pContinue->fContinue );
        }
        else
        {
            siResult = ::dsDoAttributeValueSearch(	pContinue->fNodeRef,
                                                    inData->fOutDataBuff,
                                                    inData->fInRecTypeList,
                                                    inData->fInAttrType,
                                                    inData->fInPattMatchType,
                                                    inData->fInPatt2Match,
                                                    &inData->fOutMatchRecordCount,
                                                    &pContinue->fContinue );
        }
        
        if (pContinue->fContinue != NULL)
        {
            inData->fIOContinueData = (tContextData *)pContinue;
        }
        else
        {
            inData->fIOContinueData = NULL;
            gCacheNodeContinue->RemoveItem( pContinue );
        }
        
        //TODO here we add to the cache of the results returned
        //need to be smart and determine if the results is too big to cache
        //also need to cache across continue data for the cache key
        //ie. retrieval takes multiple calls just like an original call
        
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // AttributeValueSearch


//------------------------------------------------------------------------------------
//	* MultipleAttributeValueSearch
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::MultipleAttributeValueSearch ( sDoMultiAttrValueSearchWithData *inData )
{
    SInt32					siResult		= eDSNoErr;
    sCacheContinueData	   *pContinue		= NULL;
    sCacheContextData	   *pContext		= NULL;
    
    //TODO build the cache key from the inpout data request
    
    //TODO here before we call thru to the search node we consult the cache
    
    //Now use the search node if cache has no results
    try
    {
        pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32)eDSInvalidNodeRef );
        
        //TODO need to flush out for what we carry around the continue data here ie. pass thru likely only
        if ( inData->fIOContinueData == NULL )
        {
            pContinue = (sCacheContinueData *)::calloc( 1, sizeof( sCacheContinueData ) );
            if ( pContinue == NULL ) throw( (SInt32)eMemoryAllocError );
            
            gCacheNodeContinue->AddItem( pContinue, inData->fInNodeRef );
            
            //TODO figure out if we need to open separate refs
            pContinue->fDirRef = fDirRef;
            pContinue->fNodeRef= fSearchNodeRef;
            pContinue->fContinue = NULL;
            
            pContinue->fLimitRecSearch	= 0;
            //check if the client has requested a limit on the number of records to return
            //we only do this the first call into this context for pContinue
            if (inData->fOutMatchRecordCount >= 0)
            {
                pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
            }
        }
        else
        {
            pContinue = (sCacheContinueData *)inData->fIOContinueData;
            if ( gCacheNodeContinue->VerifyItem( pContinue ) == false )
            {
                throw( (SInt32)eDSInvalidContinueData );
            }
        }
        
        // Pass the out buffer on thru as well as the continue data
        
        if ( inData->fType == kDoMultipleAttributeValueSearchWithData )
        {
            siResult = ::dsDoMultipleAttributeValueSearchWithData(
                                                                  pContinue->fNodeRef,
                                                                  inData->fOutDataBuff,
                                                                  inData->fInRecTypeList,
                                                                  inData->fInAttrType,
                                                                  inData->fInPattMatchType,
                                                                  inData->fInPatterns2MatchList,
                                                                  inData->fInAttrTypeRequestList,
                                                                  inData->fInAttrInfoOnly,
                                                                  &inData->fOutMatchRecordCount,
                                                                  &pContinue->fContinue );
        }
        else
        {
            siResult = ::dsDoMultipleAttributeValueSearch(	
                                                            pContinue->fNodeRef,
                                                            inData->fOutDataBuff,
                                                            inData->fInRecTypeList,
                                                            inData->fInAttrType,
                                                            inData->fInPattMatchType,
                                                            inData->fInPatterns2MatchList,
                                                            &inData->fOutMatchRecordCount,
                                                            &pContinue->fContinue );
        }
        
        if (pContinue->fContinue != NULL)
        {
            inData->fIOContinueData = (tContextData *)pContinue;
        }
        else
        {
            inData->fIOContinueData = NULL;
            gCacheNodeContinue->RemoveItem( pContinue );
        }
        
        //TODO here we add to the cache of the results returned
        //need to be smart and determine if the results is too big to cache
        //also need to cache across continue data for the cache key
        //ie. retrieval takes multiple calls just like an original call
        
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // MultipleAttributeValueSearch


//------------------------------------------------------------------------------------
//	* GetRecordEntry - should not be needed
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::GetRecordEntry ( sGetRecordEntry *inData )
{
    SInt32					siResult		= eDSNoErr;
    UInt32					uiIndex			= 0;
    UInt32					uiCount			= 0;
    UInt32					uiOffset		= 0;
    UInt32					uberOffset		= 0;
    char 				   *pData			= NULL;
    tRecordEntryPtr			pRecEntry		= NULL;
    sCacheContextData 	   *pContext		= NULL;
    CBuff					inBuff;
    UInt32					offset			= 0;
    UInt16					usTypeLen		= 0;
    char				   *pRecType		= NULL;
    UInt16					usNameLen		= 0;
    char				   *pRecName		= NULL;
    UInt16					usAttrCnt		= 0;
    UInt32					buffLen			= 0;
    
    try
    {
        if ( inData  == NULL ) throw( (SInt32)eMemoryError );
        if ( inData->fInOutDataBuff  == NULL ) throw( (SInt32)eDSEmptyBuffer );
        if (inData->fInOutDataBuff->fBufferSize == 0) throw( (SInt32)eDSEmptyBuffer );
        
        siResult = inBuff.Initialize( inData->fInOutDataBuff );
        if ( siResult != eDSNoErr ) throw( siResult );
        
        siResult = inBuff.GetDataBlockCount( &uiCount );
        if ( siResult != eDSNoErr ) throw( siResult );
        
        uiIndex = inData->fInRecEntryIndex;
        if ( uiIndex == 0 ) throw( (SInt32)eDSInvalidIndex );

        if ( uiIndex > uiCount ) throw( (SInt32)eDSIndexOutOfRange );
        
        pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
        if ( pData  == NULL ) throw( (SInt32)eDSCorruptBuffer );
        
        //assume that the length retrieved is valid
        buffLen = inBuff.GetDataBlockLength( uiIndex );
        
        // Skip past this same record length obtained from GetDataBlockLength
        pData	+= 4;
        offset	= 0; //buffLen does not include first four bytes
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the length for the record type
        ::memcpy( &usTypeLen, pData, 2 );
        
        pData	+= 2;
        offset	+= 2;
        
        pRecType = pData;
        
        pData	+= usTypeLen;
        offset	+= usTypeLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the length for the record name
        ::memcpy( &usNameLen, pData, 2 );
        
        pData	+= 2;
        offset	+= 2;
        
        pRecName = pData;
        
        pData	+= usNameLen;
        offset	+= usNameLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the attribute count
        ::memcpy( &usAttrCnt, pData, 2 );
        
        pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );
        
        pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
        pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;
        
        // Add the record name length
        ::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
        uiOffset += 2;
        
        // Add the record name
        ::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
        uiOffset += usNameLen;
        
        // Add the record type length
        ::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );
        
        // Add the record type
        uiOffset += 2;
        ::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );
        
        pRecEntry->fRecordAttributeCount = usAttrCnt;
        
        pContext = MakeContextData();
        if ( pContext  == NULL ) throw( (SInt32)eMemoryAllocError );
        
        pContext->offset = uberOffset + offset + 4;	// context used by next calls of GetAttributeEntry
                                                    // include the four bytes of the buffLen
        
        gCacheNodeRef->AddItem( inData->fOutAttrListRef, pContext );
        
        inData->fOutRecEntryPtr = pRecEntry;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetAttributeEntry - should not be needed
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
    SInt32					siResult			= eDSNoErr;
    UInt16					usAttrTypeLen		= 0;
    UInt16					usAttrCnt			= 0;
    UInt32					usAttrLen			= 0;
    UInt16					usValueCnt			= 0;
    UInt32					usValueLen			= 0;
    UInt32					i					= 0;
    UInt32					uiIndex				= 0;
    UInt32					uiAttrEntrySize		= 0;
    UInt32					uiOffset			= 0;
    UInt32					uiTotalValueSize	= 0;
    UInt32					offset				= 0;
    UInt32					buffSize			= 0;
    UInt32					buffLen				= 0;
    char				   *p			   		= NULL;
    char				   *pAttrType	   		= NULL;
    tDataBuffer			   *pDataBuff			= NULL;
    tAttributeValueListRef	attrValueListRef	= 0;
    tAttributeEntryPtr		pAttribInfo			= NULL;
    sCacheContextData 	   *pAttrContext		= NULL;
    sCacheContextData 	   *pValueContext		= NULL;
    
    try
    {
        if ( inData  == NULL ) throw( (SInt32)eMemoryError );
        
        pAttrContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInAttrListRef );
        if ( pAttrContext  == NULL ) throw( (SInt32)eDSBadContextData );
        
        uiIndex = inData->fInAttrInfoIndex;
        if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );
        
        pDataBuff = inData->fInOutDataBuff;
        if ( pDataBuff  == NULL ) throw( (SInt32)eDSNullDataBuff );
        
        buffSize	= pDataBuff->fBufferSize;
        //buffLen		= pDataBuff->fBufferLength;
        //here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
        //and the fBufferLength is the overall length of the data for all blocks at the end of the data block
        //the value ALSO includes the bookkeeping data at the start of the data block
        //so we need to read it here
        
        p		= pDataBuff->fBufferData + pAttrContext->offset;
        offset	= pAttrContext->offset;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the attribute count
        ::memcpy( &usAttrCnt, p, 2 );
        if (uiIndex > usAttrCnt) throw( (SInt32)eDSIndexOutOfRange );
        
        // Move 2 bytes
        p		+= 2;
        offset	+= 2;
        
        // Skip to the attribute that we want
        for ( i = 1; i < uiIndex; i++ )
        {
            // Do record check, verify that offset is not past end of buffer, etc.
            if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
            
            // Get the length for the attribute
            ::memcpy( &usAttrLen, p, 4 );
            
            // Move the offset past the length word and the length of the data
            p		+= 4 + usAttrLen;
            offset	+= 4 + usAttrLen;
        }
        
        // Get the attribute offset
        uiOffset = offset;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the length for the attribute block
        ::memcpy( &usAttrLen, p, 4 );
        
        // Skip past the attribute length
        p		+= 4;
        offset	+= 4;
        
        //set the bufLen to stricter range
        buffLen = offset + usAttrLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the length for the attribute type
        ::memcpy( &usAttrTypeLen, p, 2 );
        
        pAttrType = p + 2;
        p		+= 2 + usAttrTypeLen;
        offset	+= 2 + usAttrTypeLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get number of values for this attribute
        ::memcpy( &usValueCnt, p, 2 );
        
        p		+= 2;
        offset	+= 2;
        
        for ( i = 0; i < usValueCnt; i++ )
        {
            // Do record check, verify that offset is not past end of buffer, etc.
            if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
            
            // Get the length for the value
            ::memcpy( &usValueLen, p, 4 );
            
            p		+= 4 + usValueLen;
            offset	+= 4 + usValueLen;
            
            uiTotalValueSize += usValueLen;
        }
        
        uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
        pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );
        
        pAttribInfo->fAttributeValueCount				= usValueCnt;
        pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
        pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
        pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
        pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
        ::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );
        
        attrValueListRef = inData->fOutAttrValueListRef;
        
        pValueContext = MakeContextData();
        if ( pValueContext  == NULL ) throw( (SInt32)eMemoryAllocError );
        
        pValueContext->offset	= uiOffset;
        
        gCacheNodeRef->AddItem( inData->fOutAttrValueListRef, pValueContext );
        
        inData->fOutAttrInfoPtr = pAttribInfo;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue - should not be needed
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::GetAttributeValue ( sGetAttributeValue *inData )
{
    SInt32						siResult		= eDSNoErr;
    UInt16						usValueCnt		= 0;
    UInt32						usValueLen		= 0;
    UInt16						usAttrNameLen	= 0;
    UInt32						i				= 0;
    UInt32						uiIndex			= 0;
    UInt32						offset			= 0;
    char					   *p				= NULL;
    tDataBuffer				   *pDataBuff		= NULL;
    tAttributeValueEntry	   *pAttrValue		= NULL;
    sCacheContextData 		   *pValueContext	= NULL;
    UInt32						buffSize		= 0;
    UInt32						buffLen			= 0;
    UInt32						attrLen			= 0;
    
    try
    {
        pValueContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInAttrValueListRef );
        if ( pValueContext  == NULL ) throw( (SInt32)eDSBadContextData );
        
        uiIndex = inData->fInAttrValueIndex;
        if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );
        
        pDataBuff = inData->fInOutDataBuff;
        if ( pDataBuff  == NULL ) throw( (SInt32)eDSNullDataBuff );
        
        buffSize	= pDataBuff->fBufferSize;
        //buffLen		= pDataBuff->fBufferLength;
        //here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
        //and the fBufferLength is the overall length of the data for all blocks at the end of the data block
        //the value ALSO includes the bookkeeping data at the start of the data block
        //so we need to read it here
        
        p		= pDataBuff->fBufferData + pValueContext->offset;
        offset	= pValueContext->offset;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the buffer length
        ::memcpy( &attrLen, p, 4 );
        
        //now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
        //AND add the length of the buffer length var as stored ie. 4 bytes
        buffLen		= attrLen + pValueContext->offset + 4;
        if (buffLen > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Skip past the attribute length
        p		+= 4;
        offset	+= 4;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the attribute name length
        ::memcpy( &usAttrNameLen, p, 2 );
        
        p		+= 2 + usAttrNameLen;
        offset	+= 2 + usAttrNameLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        // Get the value count
        ::memcpy( &usValueCnt, p, 2 );
        
        p		+= 2;
        offset	+= 2;
        
        if (uiIndex > usValueCnt)  throw( (SInt32)eDSIndexOutOfRange );
        
        // Skip to the value that we want
        for ( i = 1; i < uiIndex; i++ )
        {
            // Do record check, verify that offset is not past end of buffer, etc.
            if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
            
            // Get the length for the value
            ::memcpy( &usValueLen, p, 4 );
            
            p		+= 4 + usValueLen;
            offset	+= 4 + usValueLen;
        }
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
        
        ::memcpy( &usValueLen, p, 4 );
        
        p		+= 4;
        offset	+= 4;
        
        //if (usValueLen == 0)  throw( (SInt32)eDSInvalidBuffFormat ); //if zero is it okay?
        
        pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );
        
        pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
        pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
        
        // Do record check, verify that offset is not past end of buffer, etc.
        if ( usValueLen + offset > buffLen ) throw( (SInt32)eDSInvalidBuffFormat );
        
        ::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );
        
        // Set the attribute value ID
        pAttrValue->fAttributeValueID = 0x00;
        
        inData->fOutAttrValue = pAttrValue;
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    return( siResult );
    
} // GetAttributeValue


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::CloseAttributeList ( sCloseAttributeList *inData )
{
    SInt32				siResult		= eDSNoErr;
    sCacheContextData *pContext		= NULL;
    
    pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInAttributeListRef );
    if ( pContext != NULL )
    {
        //only "offset" should have been used in the Context
        gCacheNodeRef->RemoveItem( inData->fInAttributeListRef );
    }
    else
    {
        siResult = eDSInvalidAttrListRef;
    }
    
    return( siResult );
    
} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
    SInt32				siResult		= eDSNoErr;
    sCacheContextData *pContext		= NULL;
    
    pContext = (sCacheContextData *)gCacheNodeRef->GetItemData( inData->fInAttributeValueListRef );
    if ( pContext != NULL )
    {
        //only "offset" should have been used in the Context
        gCacheNodeRef->RemoveItem( inData->fInAttributeValueListRef );
    }
    else
    {
        siResult = eDSInvalidAttrValueRef;
    }
    
    return( siResult );
    
} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

SInt32 CCachePlugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
    SInt32					siResult		= eDSNoErr;
    sCacheContinueData	   *pContinue		= NULL;
    
    //Now use the search node
    try
    {
        pContinue = (sCacheContinueData *)inData->fInContinueData;
        if ( gCacheNodeContinue->VerifyItem( pContinue ) == false )
        {
            throw( (SInt32)eDSInvalidContinueData );
        }
        
        siResult = dsReleaseContinueData( pContinue->fNodeRef, pContinue->fContinue );
    }
    
    catch( SInt32 err )
    {
        siResult = err;
    }
    
    // RemoveItem calls our ContinueDeallocProc to clean up
    if ( gCacheNodeContinue->RemoveItem( pContinue ) != eDSNoErr )
    {
        siResult = eDSInvalidContext;
    }
    
    return( siResult );
    
} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	  * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

SInt32 CCachePlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
    SInt32                      siResult        = eDSNoErr;
    sCacheContextData           *pContext       = NULL;
    AuthorizationRef            authRef			= 0;
    AuthorizationExternalForm   blankExtForm;
    bool                        verifyAuthRef	= true;
    char                        *buffer         = inData->fInRequestData->fBufferData;
    UInt32                      bufferLen       = inData->fInRequestData->fBufferLength;
   
    //Now use the search node if cache has no results
    pContext = (sCacheContextData *) gCacheNodeRef->GetItemData( inData->fInNodeRef );
    if ( pContext == NULL ) {
        siResult = eDSInvalidNodeRef;
        goto done;
    }
    
    if ( bufferLen < sizeof(AuthorizationExternalForm) ) {
        siResult = eDSInvalidBuffFormat;
        goto done;
    }
    
    if ( pContext->fEffectiveUID == 0 ) {
        bzero( &blankExtForm, sizeof(AuthorizationExternalForm) );
        if ( memcmp(inData->fInRequestData->fBufferData, &blankExtForm, sizeof(AuthorizationExternalForm)) == 0 ) {
            verifyAuthRef = false;
        }
    }
    
    if ( verifyAuthRef ) {
        int status = AuthorizationCreateFromExternalForm( (AuthorizationExternalForm *)inData->fInRequestData->fBufferData, &authRef );
        if ( status != errAuthorizationSuccess ) {
            DbgLog( kLogPlugin, "CCachePlugin: AuthorizationCreateFromExternalForm returned error %d", status );
            siResult = eDSPermissionError;
            goto done;
        }
        
        AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
        AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
        
        status = AuthorizationCopyRights( authRef, &rightSet, NULL, kAuthorizationFlagExtendRights, NULL );
        if ( status != errAuthorizationSuccess ) {
            DbgLog( kLogPlugin, "CCachePlugin: AuthorizationCopyRights returned error %d", siResult );
            siResult = eDSPermissionError;
            goto done;
        }
    }
    
    if ( inData->fInRequestCode == eDSCustomCallCacheRegisterLocalSearchPID || inData->fInRequestCode == eDSCustomCallCacheUnregisterLocalSearchPID ) {
        if ( bufferLen < sizeof(AuthorizationExternalForm) + sizeof(pid_t) ) {
            siResult = eDSInvalidBuffFormat;
            goto done;
        }
        
        pid_t thePID = *((pid_t *) (buffer + sizeof(AuthorizationExternalForm)));
        if ( thePID > 0 ) {
            
            fPIDListLock.WaitLock();
            if ( inData->fInRequestCode == eDSCustomCallCacheRegisterLocalSearchPID )
                fLocalOnlyPIDs.insert( thePID );
            else
                fLocalOnlyPIDs.erase( thePID );
            fPIDListLock.SignalLock();
        }
    }

done:
    
    return( siResult );
    
} // DoPlugInCustomCall


#pragma mark -
#pragma mark Libinfo result parsing routines
#pragma mark -

// these calls need to be on internal dispatch threads created from the MIG server that receives the requests
//TODO the dir ref and node refs should not be member variables

// kvbuf_t key/value pair dictionary
//           struct passwd {
//                   char    *pw_name;       /* user name -- kDSNAttrRecordName */
//                   char    *pw_passwd;     /* encrypted password -- kDS1AttrPassword*/
//                   uid_t   pw_uid;         /* user uid -- kDS1AttrUniqueID*/
//                   gid_t   pw_gid;         /* user gid -- kDS1AttrPrimaryGroupID*/
//                   time_t  pw_change;      /* password change time -- 0*/
//                   char    *pw_class;      /* user access class -- NULL*/
//                   char    *pw_gecos;      /* Honeywell login info -- NULL*/
//                   char    *pw_dir;        /* home directory -- kDS1AttrNFSHomeDirectory*/
//                   char    *pw_shell;      /* default shell -- kDS1AttrUserShell*/
//                   time_t  pw_expire;      /* account expiration -- 0*/
//                   int     pw_fields;      /* internal: fields filled in -- 0*/
//           };
sCacheValidation* ParsePasswdEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                    tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef            = 0;
    tAttributeEntry		   *pAttrEntry          = nil;
    tAttributeValueEntry   *pValueEntry         = nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer          = kvbuf_new();
    sCacheValidation       *valid_t             = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = ::dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        //need to have at least one value - get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrPassword ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_passwd" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrUniqueID ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_uid" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrPrimaryGroupID ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_gid" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrNFSHomeDirectory ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_dir" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrUserShell ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_shell" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrGeneratedUID ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_uuid" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrDistinguishedName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "pw_gecos" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        
        if( valueRef != 0 )
        {
            dsCloseAttributeValueList( valueRef );
            valueRef = 0;
        }
        
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry(inDirRef, pAttrEntry);
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_USER, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

// kvbuf_t key/value pair dictionary
//           struct group {
//                   char    *gr_name;       /* group name -- kDSNAttrRecordName */
//                   char    *gr_passwd;     /* group password -- kDS1AttrPassword */
//                   int     gr_gid;         /* group id -- kDS1AttrPrimaryGroupID */
//                   char    **gr_mem;       /* group members -- kDSNAttrGroupMembership */
//           };
sCacheValidation* ParseGroupEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                   tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "gr_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrPassword ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "gr_passwd" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrPrimaryGroupID ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "gr_gid" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrGroupMembership ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "gr_mem" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    siResult = dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_GROUP, 0, inKeys );
    else
        kvbuf_free( tempBuffer );

    return( valid_t );
}

//struct fstab {
//    char    *fs_spec;       /* block special device name */
//    char    *fs_file;       /* file system path prefix */
//    char    *fs_vfstype;    /* File system type, ufs, nfs */
//    char    *fs_mntops;     /* Mount options ala -o */
//    char    *fs_type;       /* FSTAB_* from fs_mntops */
//    int     fs_freq;        /* dump frequency, in days */
//    int     fs_passno;      /* pass number on parallel fsck */
//};
sCacheValidation* ParseMountEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                   tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    Boolean                 bFreqAdded      = false;
    Boolean                 bPassAdded      = false;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "fs_spec" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrVFSLinkDir ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "fs_file" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrVFSType ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "fs_vfstype" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrVFSOpts ) == 0 )
            {
                // this value is comma separated
                char	tempBuff[256]   = { 0, };
                char    *fs_type         = "rw";
                
                kvbuf_add_key( tempBuffer, "fs_mntops" );

                if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "ro") == 0 )
                    fs_type = "ro";
                else if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "sw") == 0 )
                    fs_type = "sw";
                else if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "xx") == 0 )
                    fs_type = "xx";
                
                strlcpy( tempBuff, pValueEntry->fAttributeValueData.fBufferData, sizeof(tempBuff) );
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }

                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );

                    if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "ro") == 0 )
                        fs_type = "ro";
                    else if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "sw") == 0 )
                        fs_type = "sw";
                    else if( strcmp(pValueEntry->fAttributeValueData.fBufferData, "xx") == 0 )
                        fs_type = "xx";

                    strlcat( tempBuff, ",", sizeof(tempBuff) );
                    strlcat( tempBuff, pValueEntry->fAttributeValueData.fBufferData, sizeof(tempBuff) );
                }
                
                kvbuf_add_val( tempBuffer, tempBuff );
                
                kvbuf_add_key( tempBuffer, "fs_type" );
                kvbuf_add_val( tempBuffer, fs_type );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrVFSDumpFreq ) == 0 )
            {
                bFreqAdded = true;
                kvbuf_add_key( tempBuffer, "fs_freq" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrVFSPassNo ) == 0 )
            {
                bPassAdded = true;
                kvbuf_add_key( tempBuffer, "fs_passno" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    if( bFreqAdded == false )
    {
        kvbuf_add_key( tempBuffer, "fs_freq" );
        kvbuf_add_val( tempBuffer, "0" );
    }
    
    if( bPassAdded == false )
    {
        kvbuf_add_key( tempBuffer, "fs_passno" );
        kvbuf_add_val( tempBuffer, "0" );
    }
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_MOUNT, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

//struct aliasent {
//    char *alias_name;              /* alias name */
//    size_t alias_members_len;           
//    char **alias_members;          /* alias name list */
//    int alias_local;
//};
sCacheValidation* ParseAliasEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                   tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "alias_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            // TODO: why don't we use kDSNAttrMember?
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMember ) == 0 ||
                      strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:members" ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "alias_members" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    //build the validation struct
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_ALIAS, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

//struct  servent {
//	char    *s_name;        /* official name of service */
//	char    **s_aliases;    /* alias list */
//	int     s_port;         /* port service resides at */
//	char    *s_proto;       /* protocol to use */
//};
sCacheValidation* ParseServiceEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                     tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t				   *tempBuffer		= kvbuf_new();
    Boolean					bFoundProtocol	= false;
    char                   *portAndProtocols[64];
    uint32_t                iProtocolCount  = 0;
    char                   *pProtocol       = NULL;
    char                   *pPort           = NULL;
    sCacheValidation       *valid_t         = NULL;
    
    if( additionalInfo != NULL )
    {
        pPort = ((char **) additionalInfo)[0];
        pProtocol = ((char **) additionalInfo)[1];
    }
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    // first let's see if we are looking for a specific protocol
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "s_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                // all other names are aliases
                if( pAttrEntry->fAttributeValueCount > 1 )
                    kvbuf_add_key( tempBuffer, "s_aliases" );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }                
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:PortAndProtocol" ) == 0 )
            {
                UInt32  idx = 1;
                
                do
                {
                    if( pProtocol != NULL )
                    {
                        //if we don't have a particular protocol, pick the first one and break;
                        if( ((char *)pProtocol)[0] == '\0' )
                        {
                            portAndProtocols[iProtocolCount] = strdup( pValueEntry->fAttributeValueData.fBufferData );
                            iProtocolCount++;
                            break;
                        }
                        else
                        {
                            char    *slash = strchr( pValueEntry->fAttributeValueData.fBufferData, '/' );
                            
                            if( slash != NULL )
                            {
                                // let's queue up the protocols for later, we'll decide what to do
                                if( strcmp(slash+1, (char *)pProtocol) == 0 )
                                {
                                    *slash = '\0'; // kill the slash so we can get the port

                                    kvbuf_add_key( tempBuffer, "s_port" );
                                        kvbuf_add_val( tempBuffer, (pPort != NULL ? pPort : pValueEntry->fAttributeValueData.fBufferData) );
                                    
                                    bFoundProtocol = true;
                                }
                            }
                        }
                    }
                    else
                    {
                        portAndProtocols[iProtocolCount] = strdup( pValueEntry->fAttributeValueData.fBufferData );
                        iProtocolCount++;
                    }
                    
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    if( (++idx) <= pAttrEntry->fAttributeValueCount )
                    {
                        dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );                    
                    }
                    
                } while( idx <= pAttrEntry->fAttributeValueCount && bFoundProtocol == false );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    // if protocol found, let's cache it and return it
    if( bFoundProtocol == true )
    {
        kvbuf_add_key( tempBuffer, "s_proto" );
        kvbuf_add_val( tempBuffer, (char *) pProtocol );
        
        kvbuf_append_kvbuf( inBuffer, tempBuffer );
    }
    else if( iProtocolCount > 0 )
    {
        for( uint32_t ii = 0; ii < iProtocolCount; ii++ )
        {
            kvbuf_t *tempBuffer2    = kvbuf_init( tempBuffer->databuf, tempBuffer->datalen );
            
            kvbuf_reset( tempBuffer2 );
            kvbuf_next_dict( tempBuffer2 );
            
            char    *slash = strchr( portAndProtocols[ii], '/' );
            if( slash != NULL )
            {
                // kill the slash to split the port and protocol
                *slash = '\0';
                    
                kvbuf_add_key( tempBuffer2, "s_port" );
                kvbuf_add_val( tempBuffer2, portAndProtocols[ii] );

                kvbuf_add_key( tempBuffer2, "s_proto" );
                kvbuf_add_val( tempBuffer2, slash+1 );
                
                kvbuf_append_kvbuf( inBuffer, tempBuffer2 );
                
                DSFree( portAndProtocols[ii] );
            }
            
            kvbuf_free( tempBuffer2 );
        }
    }

    kvbuf_free( tempBuffer );

    return( valid_t );
}

//struct  protoent {
//	char    *p_name;        /* official name of protocol */
//	char    **p_aliases;    /* alias list */
//	int     p_proto;        /* protocol number */
//};
sCacheValidation* ParseProtocolEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                      tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "p_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                // all other names are aliases
                if( pAttrEntry->fAttributeValueCount > 1 )
                    kvbuf_add_key( tempBuffer, "p_aliases" );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:number" ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "p_proto" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_PROTOCOL, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

//struct rpcent {
//    char    *r_name;        /* name of server for this rpc program */
//    char    **r_aliases;    /* alias list */
//    long    r_number;       /* rpc program number */
//};
sCacheValidation* ParseRPCEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                 tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
                valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "r_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                // all other names are aliases
                if( pAttrEntry->fAttributeValueCount > 1 )
                    kvbuf_add_key( tempBuffer, "r_aliases" );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:number" ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "r_number" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_RPC, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

//struct  netent {
//    char            *n_name;        /* official name of net */
//    char            **n_aliases;    /* alias list */
//    int             n_addrtype;     /* net number type */
//    unsigned long   n_net;          /* net number */
//};
sCacheValidation* ParseNetworkEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                     tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
				valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "n_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                // all other names are aliases
                if( pAttrEntry->fAttributeValueCount > 1 )
                    kvbuf_add_key( tempBuffer, "n_aliases" );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:address" ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "n_net" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_NETWORK, 0, inKeys );
    else
        kvbuf_free( tempBuffer );

    return( valid_t );
}

sCacheValidation* ParseNetGroupEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                      tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache,
                                      char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                 *tempBuffer     = kvbuf_new();
    char                    **pSearch       = (char **) additionalInfo;
    char                    name[256]       = { 0, };
    sCacheValidation       *valid_t         = NULL;
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
				valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                strlcpy( name, pValueEntry->fAttributeValueData.fBufferData, sizeof(name) );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrNetGroups ) == 0 )
            {
                UInt32 idx = 1;
                
                do
                {
                    kvbuf_t *nestedGroup = kvbuf_new();
                    
                    kvbuf_add_dict( nestedGroup );
                    kvbuf_add_key( nestedGroup, "netgroup" );
                    kvbuf_add_val( nestedGroup, pValueEntry->fAttributeValueData.fBufferData );
                    
                    DbgLog( kLogDebug, "CCachePlugin::ParseNetGroupEntry nested group lookup - %s", pValueEntry->fAttributeValueData.fBufferData );

                    kvbuf_t *nestedData = gCacheNode->DSgetnetgrent( nestedGroup );

                    kvbuf_append_kvbuf( tempBuffer, nestedData );
                        
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    if( (++idx) <= pAttrEntry->fAttributeValueCount )
                    {
                        dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    }
                    
                    kvbuf_free( nestedGroup );
                    kvbuf_free( nestedData );
                    
                } while( idx <= pAttrEntry->fAttributeValueCount );
                
                DbgLog( kLogDebug, "CCachePlugin::ParseNetGroupEntry nested group lookup complete" );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, "dsAttrTypeNative:triplet" ) == 0 )
            {
                UInt32  idx     = 1;
                
                do
                {
                    char    *current    = pValueEntry->fAttributeValueData.fBufferData;
                    
                    // now we need to parse it into 3 pieces without the commas
                    char    *host = strsep( &current, "," );
                    char    *user = strsep( &current, "," );
                    char    *domain = strsep( &current, "," );

                    // if we got any of the values we add to the return buffer
                    if( host != NULL || user != NULL || domain != NULL )
                    {
                        if( pSearch != NULL )
                        {
                            // if the key is doesn't match, we just continue..
                            if( pSearch[0][0] != '\0' && host != NULL && strcmp(pSearch[0], host) != 0 )
                            {
                                goto nextValue;
                            }

                            if( pSearch[1][0] != '\0' && user != NULL && strcmp(pSearch[1], user) != 0 )
                            {
                                goto nextValue;
                            }

                            if( pSearch[2][0] != '\0' && domain != NULL && strcmp(pSearch[2], domain) != 0 )
                            {
                                goto nextValue;
                            }
                        }
                        
                        kvbuf_add_dict( tempBuffer );
                        
                        if( host != NULL )
                        {
                            kvbuf_add_key( tempBuffer, "host" );
                            kvbuf_add_val( tempBuffer, host );
                        }
                        
                        if( user != NULL )
                        {
                            kvbuf_add_key( tempBuffer, "user" );
                            kvbuf_add_val( tempBuffer, user );
                        }
                        
                        if( domain != NULL )
                        {
                            kvbuf_add_key( tempBuffer, "domain" );
                            kvbuf_add_val( tempBuffer, domain );
                        }
                    }
                    
nextValue:
                    if( (++idx) <= pAttrEntry->fAttributeValueCount )
                    {
                        if ( pValueEntry != NULL )
                        {
                            dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                            pValueEntry = NULL;
                        }
                        
                        dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    }
                } while( idx <= pAttrEntry->fAttributeValueCount );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );

    if( inCache != NULL )
    {
        // if we are using a specific search we need to do a different cache entry
        if( pSearch != NULL )
            CCachePlugin::AddEntryToCacheWithMultiKey( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_GROUP, 0, "netgroup", name, 
                                                       "host", pSearch[0], "user", pSearch[1], "domain", pSearch[2], NULL );
        else
            CCachePlugin::AddEntryToCacheWithMultiKey( inCache, valid_t, tempBuffer, CACHE_ENTRY_TYPE_GROUP, 0, "netgroup", name, NULL );
    }    
    else
    {
        kvbuf_free( tempBuffer );
    }
    
    return valid_t;
}

//struct  hostent {
//    char    *h_name;        /* official name of host */
//    char    **h_aliases;    /* alias list */
//    int     h_addrtype;     /* host address type */
//    int     h_length;       /* length of address */
//    char    **h_addr_list;  /* list of addresses from name server */
//};
sCacheValidation* ParseHostEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                  tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, 
                                  char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                 *tempBuffer     = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
				valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "h_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                // all other names are aliases
                if( pAttrEntry->fAttributeValueCount > 1 )
                    kvbuf_add_key( tempBuffer, "h_aliases" );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrIPAddress ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "h_ipv4_addr_list" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );

                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrIPv6Address ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "h_ipv6_addr_list" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                
                for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                {
                    if ( pValueEntry != NULL )
                    {
                        dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                        pValueEntry = NULL;
                    }
                    
                    dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                }
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, NULL, tempBuffer, CACHE_ENTRY_TYPE_HOST, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return valid_t;
}

sCacheValidation* ParseEthernetEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                      tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, 
                                      char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer      = kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    
    kvbuf_add_dict( tempBuffer );
    
    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
				valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrENetAddress ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "mac" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, NULL, tempBuffer, CACHE_ENTRY_TYPE_HOST, 0, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

static struct ether_addr *myether_aton( const char *s, struct ether_addr *ep )
{
    unsigned int t[6];
    
    if (ep == NULL) return NULL;
    
    int i = sscanf(s, " %x:%x:%x:%x:%x:%x", &t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
    if (i != 6) return NULL;

    for (i = 0; i < 6; i++)
        ep->ether_addr_octet[i] = t[i];
    
    return ep;
}

sCacheValidation* ParseBootpEntry( tDirReference inDirRef, tDirNodeReference inNodeRef, kvbuf_t *inBuffer, tDataBufferPtr inDataBuffer, 
                                   tRecordEntryPtr inRecEntry, tAttributeListRef inAttrListRef, void *additionalInfo, CCache *inCache, 
                                   char **inKeys )
{
    tAttributeValueListRef	valueRef		= 0;
    tAttributeEntry		   *pAttrEntry		= nil;
    tAttributeValueEntry   *pValueEntry		= nil;
    tDirStatus				siResult;
    kvbuf_t                *tempBuffer		= kvbuf_new();
    sCacheValidation       *valid_t         = NULL;
    bool                    bUsedPair       = false;
    char                  **keys            = (char **) additionalInfo;
    
	kvbuf_add_dict( tempBuffer );
    
    //let's see if we have the attribute kDSNAttrIPAddressAndENetAddress in the return, if so, we need to use it instead
    if ( keys != NULL )
    {
        for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount && bUsedPair == false; i++)
        {
            siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
            if ( siResult == eDSNoErr && pAttrEntry->fAttributeValueCount > 0 )
            {
                if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrIPAddressAndENetAddress ) == 0 )
                {
                    for ( UInt32 ii = 1; ii <= pAttrEntry->fAttributeValueCount && bUsedPair == false; ii++ )
                    {
                        siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, ii, valueRef, &pValueEntry );
                        if ( siResult == eDSNoErr )
                        {
                            char *slash = strchr( pValueEntry->fAttributeValueData.fBufferData, '/' );
                            
                            if ( slash != NULL )
                            {
                                bool    bFoundMatch = false;
                                
                                (*slash) = '\0';
                                slash++;
                                
                                if ( keys[0] != NULL && strcmp(keys[0], pValueEntry->fAttributeValueData.fBufferData) == 0 )
                                    bFoundMatch = true;
                                
                                if ( bFoundMatch || keys[1] != NULL )
                                {
                                    struct ether_addr   etherStorage;
                                    struct ether_addr	*etherAddr	= myether_aton( slash, &etherStorage );
                                    char                buffer[32];
                                    
                                    if ( etherAddr != NULL )
                                    {
                                        snprintf( buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x", etherAddr->octet[0], etherAddr->octet[1], 
                                                  etherAddr->octet[2], etherAddr->octet[3], etherAddr->octet[4], etherAddr->octet[5] );
                                        
                                        if ( bFoundMatch || strcmp(keys[1], buffer) == 0 )
                                        {
                                            kvbuf_add_key( tempBuffer, "bp_hw" );
                                            kvbuf_add_val( tempBuffer, buffer );
                                            kvbuf_add_key( tempBuffer, "bp_addr" );
                                            kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                                            
                                            bUsedPair = true;
                                        }
                                    }
                                }
                            }
                            
                            dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                            pValueEntry = NULL;
                        }
                    }
                }
                
                dsDeallocAttributeEntry( inDirRef, pAttrEntry );
                pAttrEntry = nil;
            }
        }
    }

    //index starts at one - should have multiple entries
    for (unsigned int i = 1; i <= inRecEntry->fRecordAttributeCount; i++)
    {
        siResult = dsGetAttributeEntry( inNodeRef, inDataBuffer, inAttrListRef, i, &valueRef, &pAttrEntry );
        
        //need to have at least one value - usually get first only in each case
        if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
        {
            // Get the first attribute value
            siResult = ::dsGetAttributeValue( inNodeRef, inDataBuffer, 1, valueRef, &pValueEntry );
            
            // Is it what we expected
            if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
            {
				valid_t = new sCacheValidation( pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "bp_name" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrBootFile ) == 0 )
            {
                kvbuf_add_key( tempBuffer, "bp_bootfile" );
                kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
            }
            else if ( bUsedPair == false )
            {
                if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrENetAddress ) == 0 )
                {
                    UInt32 idx = 1;
                    
                    kvbuf_add_key( tempBuffer, "bp_hw" );

                    while( 1 )
                    {
                        struct ether_addr   etherStorage;
                        struct ether_addr	*etherAddr	= myether_aton( pValueEntry->fAttributeValueData.fBufferData, &etherStorage );
                        
                        if( etherAddr != NULL )
                        {
                            char	buffer[32];

                            snprintf( buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x", etherAddr->octet[0], etherAddr->octet[1], etherAddr->octet[2], etherAddr->octet[3], etherAddr->octet[4], etherAddr->octet[5] );
                            
                            // need to canonicalize these since we are doing these searches insensitive
                            kvbuf_add_val( tempBuffer, buffer );
                        }

                        if( (++idx) <= pAttrEntry->fAttributeValueCount )
                        {
                            if ( pValueEntry != NULL )
                            {
                                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                                pValueEntry = NULL;
                            }
                            
                            dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                            continue;
                        }
                        
                        break;
                    }
                }
                else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrIPAddress ) == 0 )
                {
                    kvbuf_add_key( tempBuffer, "bp_addr" );
                    kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                    
                    for (UInt32 idx=2; idx <= pAttrEntry->fAttributeValueCount; idx++)
                    {
                        if ( pValueEntry != NULL )
                        {
                            dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                            pValueEntry = NULL;
                        }
                        
                        dsGetAttributeValue( inNodeRef, inDataBuffer, idx, valueRef, &pValueEntry );
                        kvbuf_add_val( tempBuffer, pValueEntry->fAttributeValueData.fBufferData );
                    }
                }
            }
            
            if ( pValueEntry != NULL )
            {
                dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
                pValueEntry = NULL;
            }
        }
        dsCloseAttributeValueList(valueRef);
        if (pAttrEntry != nil)
        {
            dsDeallocAttributeEntry( inDirRef, pAttrEntry );
            pAttrEntry = nil;
        }
    } //loop over attrs requested
    
    kvbuf_append_kvbuf( inBuffer, tempBuffer );
    
    if( inCache != NULL && inKeys != NULL )
        CCachePlugin::AddEntryToCacheWithKeys( inCache, NULL, tempBuffer, CACHE_ENTRY_TYPE_COMPUTER, 60, inKeys );
    else
        kvbuf_free( tempBuffer );
    
    return( valid_t );
}

#pragma mark -
#pragma mark Libinfo cache and support routines
#pragma mark -

void CCachePlugin::RemoveEntryWithMultiKey( CCache *inCache, ... )
{
    char       *key         = NULL;
    va_list     args;
    
    if (gDSInstallDaemonMode)
        return;
    
    va_start( args, inCache );
    
    while( 1 )
    {
        char *tempKey = va_arg( args, char * );
        if( tempKey == NULL )
            break;
        
        char *tempValue = va_arg( args, char * );
        if( tempValue == NULL )
            tempValue = ""; // we accept NULLs as empty value
        
        int newLen = 0;
        if( key != NULL )
        {
            newLen = strlen(key) + sizeof(" ") + strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) reallocf( key, newLen );
            strlcat( key, " ", newLen );
        }
        else
        {
            newLen = strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) calloc( newLen, sizeof(char) );
        }
        
        strlcat( key, tempKey, newLen );
        strlcat( key, ":", newLen );
        strlcat( key, tempValue, newLen );
    }
    
    inCache->RemoveKey( key );
    DbgLog( kLogPlugin, "CCachePlugin::RemoveEntryWithMultiKey - key %s", key );
    
    DSFree( key );
}

// this routine calculates a key based on the value pairs, a terminating NULL is expected
// but caller can pass a key with a value of NULL.  A NULL entry will create a negative cache entry
void CCachePlugin::AddEntryToCacheWithMultiKey( CCache *inCache, sCacheValidation *inValidation, kvbuf_t *inEntry, uint32_t flags, 
                                                uint32_t inTTL, ... )
{
    char       *key         = NULL;
    va_list     args;
    
    if (gDSInstallDaemonMode)
    {
        kvbuf_free( inEntry );    
        return;
   }

    va_start( args, inTTL );
    
    while( 1 )
    {
        char *tempKey = va_arg( args, char * );
        if( tempKey == NULL )
            break;
        
        char *tempValue = va_arg( args, char * );
        if( tempValue == NULL )
            tempValue = ""; // we accept NULLs as empty value
        
        int newLen = 0;
        if( key != NULL )
        {
            newLen = strlen(key) + sizeof(" ") + strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) reallocf( key, newLen );
            strlcat( key, " ", newLen );
        }
        else
        {
            newLen = strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) calloc( newLen, sizeof(char) );
        }
        
        strlcat( key, tempKey, newLen );
        strlcat( key, ":", newLen );
        strlcat( key, tempValue, newLen );
    }
    
    if ( inEntry != NULL )
    {
        sCacheEntry *cacheEntry = inCache->AddEntry( inEntry, key, inTTL, flags );
        if( cacheEntry != NULL )
        {
            DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithMultiKey - Entry added for record %X with key %s - TTL %d", inEntry, key, 
                    inTTL );
            cacheEntry->AddValidation( inValidation );
        }
        else
        {
            // if not added, we need to free the entry because the caller is not expecting to free
            kvbuf_free( inEntry );
            DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithMultiKey - Entry NOT added for record %X with key %s - collision", inEntry, key );
        }
    }
    else
    {
        sCacheEntry *cacheEntry = inCache->AddEntry( NULL, key, inTTL, (flags | CACHE_ENTRY_TYPE_NEGATIVE) );
        if ( cacheEntry != NULL )
        {
            DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithMultiKey - Negative cache entry for key %s - TTL %d", key, inTTL );
        }
    }

    DSFree( key );
}

// this routine calculates a key based on the key lists as combined keys, a terminating NULL is expected.
// A NULL entry will create a negative cache entry
//   char []* are passed so that we can combine keys for things e.g. "s_proto:### s_port:####"
void CCachePlugin::AddEntryToCacheWithKeylists( CCache *inCache, sCacheValidation *inValidation, kvbuf_t *inEntry, uint32_t flags, 
                                                uint32_t inTTL, ... )
{
    va_list         args;
    sCacheEntry     *cacheEntry     = NULL;
    
    if (gDSInstallDaemonMode)
    {
        kvbuf_free( inEntry );
        return;
    }
    
    va_start( args, inTTL );
    
    kvarray_t   *array  = kvbuf_decode( inEntry );
    if( array == NULL )
    {
        kvbuf_free( inEntry );    
        return;
    }

    kvdict_t    *dict   = array->dict;
    uint32_t    kcount  = dict->kcount;

    while( 1 )
    {
        char        **keyList       = va_arg( args, char ** );
        uint32_t    indexKeysCnt    = 0;
        char        **indexKeys     = NULL;
        char        **tempKeyList;
        char        *tempKey;
        
        if( keyList == NULL )
            break;

        // let's loop through the keys passed and find them to keep the order correct
        for( tempKeyList = keyList, tempKey = (*tempKeyList); tempKey != NULL; tempKeyList++, tempKey = (*tempKeyList) )
        {
            if( indexKeys == NULL )
            {
                for( uint32_t k = 0; k < kcount && indexKeys == NULL; k++ )
                {
                    // found our key, now lets create our bucket
                    if( strcmp(dict->key[k], tempKey) == 0 )
                    {
                        indexKeysCnt = dict->vcount[k];
                        indexKeys = (char **) calloc( dict->vcount[k] + 1, sizeof(char *) );
                        
                        for( uint32_t v = 0; v < indexKeysCnt; v++ )
                        {
                            const char  *tempValue  = dict->val[k][v];
                            int         newLen      = strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
                            char        *indexKey   = (char *) calloc( newLen, sizeof(char) ); // key:value
                            
                            indexKeys[v] = indexKey;
                            
                            // let's concatentate the key name and the value to create the index value
                            strlcpy( indexKey, tempKey, newLen );
                            strlcat( indexKey, ":", newLen );
                            strlcat( indexKey, tempValue, newLen );
                        }
                        break;
                    }
                }
                
                // nothing to do since we didn't find our first key
                if( indexKeysCnt == 0 )
                    break;
            }
            else
            {
                for( uint32_t k = 0; k < kcount; k++ )
                {
                    // let's see if this key exists here
                    if( strcmp(dict->key[k], tempKey) == 0 )
                    {
                        if( dict->vcount[k] > 0 )
                        {
                            const char  *tempValue = dict->val[k][0];
                            int         iTempKeyLen = strlen( tempKey );
                            int         iTempValLen = strlen( tempValue );
                            
                            for( uint32_t ii = 0; ii < indexKeysCnt; ii++ )
                            {
                                int  newLen = strlen(indexKeys[ii]) + sizeof(" ") + iTempKeyLen + sizeof(":") + iTempValLen + 1;
                                char *indexKey = (char *) reallocf( indexKeys[ii], newLen );
                                indexKeys[ii] = indexKey;
                                
                                strlcat( indexKey, " ", newLen );
                                
                                // let's concatentate the key name and the value to create the index value
                                strlcat( indexKey, tempKey, newLen );
                                strlcat( indexKey, ":", newLen );
                                strlcat( indexKey, tempValue, newLen );
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        if( indexKeys != NULL )
        {
            for( uint32_t ii = 0; ii < indexKeysCnt; ii++ )
            {
                char *indexKey = indexKeys[ii];
                
                if( indexKey != NULL )
                {
                    if( cacheEntry != NULL )
                    {
                        inCache->AddKeyToEntry( cacheEntry, indexKey, TRUE );
                        DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeylists - Entry %X added key %s", cacheEntry, indexKey );
                    }
                    else
                    {
                        cacheEntry = inCache->AddEntry( inEntry, indexKey, inTTL, flags );
                        if( cacheEntry != NULL )
                        {
                            DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeylists - Entry %X added for record %X with key %s - TTL %d", 
                                    cacheEntry, inEntry, indexKey, inTTL );
                            cacheEntry->AddValidation( inValidation );
                        }
                        else
                        {
                            DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeylists - Entry NOT added for record %X with key %s - collision", 
                                    inEntry, indexKey );
                        }
                    }
                    
                    DSFree( indexKey );
                    indexKeys[ii] = NULL;
                }
            }
            
            DSFree( indexKeys );
        }
    }

    // if we cached the entry, then we need to NULL the pointer in the kvarray otherwise it will get freed
    if( cacheEntry != NULL )
        array->kv = NULL;

    kvarray_free( array );
    
}

// this routine adds an entry to the cache and indexing the keys requested
void CCachePlugin::AddEntryToCacheWithKeys( CCache *inCache, sCacheValidation *inValidation, kvbuf_t *inEntry, uint32_t flags, uint32_t inTTL, 
                                            char **inKeysToIndex )
{
    sCacheEntry     *cacheEntry	= NULL;
    const char      *key		= NULL;
    
    if (gDSInstallDaemonMode)
    {
        kvbuf_free( inEntry );    
        return;
   }

    // reset the entry since we don't know if we are at the beginning or not
    kvbuf_reset( inEntry );
    
    // let's loop through the keys and add a key for them
    uint32_t	kcount	= kvbuf_next_dict( inEntry );
    sKeyList    *keys   = new sKeyList;
    
    // loop over the keys in the dictionary to find any keys that were requested for index
    for (uint32_t k = 0; k < kcount; k++)
    {
        uint32_t	vcount	= 0;
        
        key	= kvbuf_next_key( inEntry, &vcount );
        if( key != NULL )
        {
            char		**keyList;
            char		*tempKey;
            
            // see if this key matches any of the inKeysToIndex so we can add it to the the index
            for( keyList = inKeysToIndex, tempKey = (*keyList); *keyList != NULL; keyList++, tempKey = (*keyList) )
            {
                if( strcmp(key, tempKey) == 0 )
                {
                    // loop through the values and build the key entry
                    for (uint32_t v = 0; v < vcount; v++)
                    {
                        char *val = kvbuf_next_val( inEntry );
                        
                        if( val != NULL )
                        {
                            // let's concatentate the key name and the value to create the index value
                            int  newLen = strlen(tempKey) + sizeof(":") + strlen(val) + 1;
                            char *indexedKey = (char *) malloc( newLen ); // key:value
                            strlcpy( indexedKey, tempKey, newLen );
                            strlcat( indexedKey, ":", newLen );
                            strlcat( indexedKey, val, newLen );
                            
                            // need to dupe the key so we can free it, since offsets might change
                            keys->AddKey( indexedKey );
                        }
                    }
                }
            }
        }
    }
    
    // how we can add the entries to they key list
    sKeyListItem *item = keys->fHead;
    while( item != NULL )
    {
		key = item->fKey;
		if (key != NULL)
		{
			// if we have an entry, then we've already added it to the cache, only need to add the key itself
			if( cacheEntry != NULL )
			{
                inCache->AddKeyToEntry( cacheEntry, key, TRUE );
				DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeys - Entry %X added key %s", cacheEntry, key );
			}
			else
			{
				cacheEntry = inCache->AddEntry( inEntry, key, inTTL, flags );
				
				if( cacheEntry != NULL )
				{
					DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeys - Entry %X added for record %X with key %s - TTL %d", cacheEntry, 
						    inEntry, key, inTTL );
                    cacheEntry->AddValidation( inValidation );
				}
				else
				{
					DbgLog( kLogPlugin, "CCachePlugin::AddEntryToCacheWithKeys - Entry NOT added for record %X with key %s - collision", 
						   inEntry, key );
				}
			}
		}
		item = item->fNext;
    }

    // if we didn't add it, free it because caller doesn't expect to free
    if( cacheEntry == NULL )
    {
        kvbuf_free( inEntry );
    }
    
    DSDelete( keys );
}

kvbuf_t* CCachePlugin::FetchFromCache( CCache *inCache, uint32_t *outTTL, ... )
{
    sKeyList    *query		= NULL;
    kvbuf_t		*outBuffer	= NULL;
    char		*key		= NULL;
    va_list		args;
    
    if (gDSInstallDaemonMode) return(NULL);

    va_start( args, outTTL );
    
    while( 1 )
    {
        char *tempKey = va_arg( args, char * );
        if( tempKey == NULL )
            break;
        
        char *tempValue = va_arg( args, char * );
        if( tempValue == NULL )
            tempValue = ""; // we accept NULLs as empty value
        
        int newLen = 0;
        if( key != NULL )
        {
            newLen = strlen(key) + sizeof(" ") + strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) reallocf( key, newLen );
            strlcat( key, " ", newLen );
        }
        else
        {
            newLen = strlen(tempKey) + sizeof(":") + strlen(tempValue) + 1;
            key = (char *) calloc( newLen, sizeof(char) );
        }
        
        strlcat( key, tempKey, newLen );
        strlcat( key, ":", newLen );
        strlcat( key, tempValue, newLen );
    }
    
    // first let's see if we have a match in the cache
    query = new sKeyList;
    
    query->AddKey( key ); // list takes ownership of the key
    
    DbgLog( kLogDebug, "CCachePlugin::FetchFromCache - Looking for entry with key %s", key );

    outBuffer = inCache->Fetch( query, false, outTTL );
    
    DSDelete( query );
    
    // update cache hits / misses
    fStatsLock.WaitLock();
    
    if( outBuffer != NULL )
        fCacheHits++;
    else
        fCacheMisses++;
    
    fStatsLock.SignalLock();

    // if we had a result, make a copy because we're gonna free it later TODO KW what is this?
    return outBuffer;
}

kvbuf_t* CCachePlugin::GetRecordListLibInfo( tDirNodeReference inNodeRef, const char* inSearchValue, const char* inRecordType, 
                                             UInt32 inRecCount, tDataListPtr inAttributes, ProcessEntryCallback inCallback, 
                                             kvbuf_t* inBuffer, void *additionalInfo, CCache *inCache, char **inKeys,
                                             sCacheValidation **outValidation)
{
    kvbuf_t				   *outBuffer		= inBuffer;
    tDataListPtr			recName			= NULL;
    tDataListPtr			recType			= NULL;
    tDataBufferPtr			dataBuff		= NULL;
    SInt32					siResult		= eDSNoErr;
    tContextData			context			= NULL;
    tRecordEntry		   *pRecEntry		= nil;
    tAttributeListRef		attrListRef		= 0;
    tDataListPtr			nodeName		= NULL;
    uint32_t                total           = 0;
    sCacheValidation       *valid_t         = NULL;
    
    recName = dsBuildListFromStrings( fDirRef, inSearchValue, NULL );
    recType = dsBuildListFromStrings( fDirRef, inRecordType, NULL );
    
    dataBuff = dsDataBufferAllocate( fDirRef, 8192 );
    do 
    {
        siResult = dsGetRecordList(	inNodeRef,
                                    dataBuff,
                                    recName,
                                    eDSiExact,
                                    recType,
                                    inAttributes,
                                    false,
                                    &inRecCount,
                                    &context);
        if (siResult == eDSBufferTooSmall)
        {
            UInt32 bufSize = dataBuff->fBufferSize;
            dsDataBufferDeallocatePriv( dataBuff );
            dataBuff = nil;
            dataBuff = dsDataBufferAllocate( fDirRef, bufSize * 2 );
        }
        
        if ( (siResult == eDSNoErr) && (inRecCount > 0) )
        {
            total += inRecCount;
            
            // if we got some results, let's make a buffer
            if ( outBuffer == NULL )
                outBuffer = kvbuf_new();
            
            for( unsigned int ii = 1; ii <= inRecCount; ii++ )
            {
                siResult = ::dsGetRecordEntry( inNodeRef, dataBuff, ii, &attrListRef, &pRecEntry );
                if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
                {
                    // call the callback to parse the data
                    valid_t = inCallback( fDirRef, inNodeRef, outBuffer, dataBuff, pRecEntry, attrListRef, additionalInfo, inCache, inKeys );
                    if ( outValidation != NULL )
                        (*outValidation) = valid_t;
                    else
                        valid_t->Release();
                }//found record entry(ies)
                
                if (attrListRef != 0)
                {
                    dsCloseAttributeList( attrListRef );
                    attrListRef = 0;
                }
                
                if (pRecEntry != NULL)
                {
                    dsDeallocRecordEntry( fDirRef, pRecEntry );
                    pRecEntry = nil;
                }
            }
        }// got records returned
		
        // if we got invalid context or continue data, reset the search
        if ( siResult == eDSInvalidContext || siResult == eDSInvalidContinueData || siResult == eDSBadContextData ) {
            kvbuf_free( outBuffer );
            outBuffer = NULL;
            context = 0;
            siResult = eDSBadContextData; // make it one error code to simplify check in do/while
        }
        
    } while ( siResult == eDSBufferTooSmall || (siResult == eDSNoErr && context != 0) || siResult == eDSBadContextData );
    
    if ( siResult == eDSNoErr )
    {
        DbgLog( kLogDebug, "CCachePlugin::GetRecordListLibInfo - Found %u total results", total );
    }
    else
    {
        DbgLog( kLogDebug, "CCachePlugin::GetRecordListLibInfo - Error on search <%d>", siResult);
    }

    if ( recName != NULL )
    {
        dsDataListDeallocate( fDirRef, recName );
        DSFree( recName );
    }
    if ( recType != NULL )
    {
        dsDataListDeallocate( fDirRef, recType );
        DSFree( recType );
    }
    if ( dataBuff != NULL )
    {
        dsDataBufferDeAllocate( fDirRef, dataBuff );
        dataBuff = NULL;
    }
    if ( nodeName != NULL )
    {
        dsDataListDeallocate( fDirRef, nodeName );
        DSFree( nodeName );
    }
    
    return( outBuffer );	
}

kvbuf_t* CCachePlugin::ValueSearchLibInfo( tDirNodeReference inNodeRef, const char* inSearchAttrib, const char* inSearchValue, 
                                           const char* inRecordType, UInt32 inRecCount, tDataListPtr inAttributes, 
                                           ProcessEntryCallback inCallback, kvbuf_t* inBuffer, void *additionalInfo, 
                                           sCacheValidation **outValidation, tDirPatternMatch inSearchType )
{
    kvbuf_t				   *outBuffer		= inBuffer;
    tDataNodePtr			pattMatch		= NULL;
    tDataListPtr			recType			= NULL;
    tDataNodePtr			attrMatchType	= NULL;
    tDataBufferPtr			dataBuff		= NULL;
    SInt32					siResult		= eDSNoErr;
    tContextData			context			= NULL;
    tRecordEntry		   *pRecEntry		= nil;
    tAttributeListRef		attrListRef		= 0;
    
    //TODO plan to resuse the actual DS calls? between the Lookup retrievals
    try
    {
        pattMatch = dsDataNodeAllocateString( fDirRef, inSearchValue );
        attrMatchType = dsDataNodeAllocateString( fDirRef, inSearchAttrib );
        recType = dsBuildListFromStrings( fDirRef, inRecordType, NULL );
        
        dataBuff = dsDataBufferAllocate( fDirRef, 8192 );
        do 
        {
            siResult = dsDoAttributeValueSearchWithData(	inNodeRef,
                                                            dataBuff,
                                                            recType, 
                                                            attrMatchType,
                                                            inSearchType,
                                                            pattMatch,
                                                            inAttributes,
                                                            false,
                                                            &inRecCount,
                                                            &context);
            if (siResult == eDSBufferTooSmall)
            {
                UInt32 bufSize = dataBuff->fBufferSize;
                dsDataBufferDeallocatePriv( dataBuff );
                dataBuff = nil;
                dataBuff = dsDataBufferAllocate( fDirRef, bufSize * 2 );
            }
            
            if ( (siResult == eDSNoErr) && (inRecCount > 0) )
            {
                // if we got some results, let's make a buffer
                if ( outBuffer == NULL )
                    outBuffer = kvbuf_new();
                
                for( unsigned int ii = 1; ii <= inRecCount; ii++ )
                {
                    siResult = ::dsGetRecordEntry( inNodeRef, dataBuff, ii, &attrListRef, &pRecEntry );
                    if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
                    {
                        sCacheValidation *tempValid_t = NULL;
                        tempValid_t = inCallback( fDirRef, inNodeRef, outBuffer, dataBuff, pRecEntry, attrListRef, additionalInfo, NULL, NULL );
                        if (outValidation != NULL && *outValidation == NULL)
                            *outValidation = tempValid_t;
                        else 
                            tempValid_t->Release();
                    }
                    
                    if( attrListRef != 0 )
                    {
                        dsCloseAttributeList(attrListRef);
                        attrListRef = 0;
                    }
                    if (pRecEntry != nil)
                    {
                        dsDeallocRecordEntry(fDirRef, pRecEntry);
                        pRecEntry = nil;
                    }
                }
            }// got records returned
			
            // if we got invalid context or continue data, reset the search
            if ( siResult == eDSInvalidContext || siResult == eDSInvalidContinueData || siResult == eDSBadContextData ) {
                kvbuf_free( outBuffer );
                outBuffer = NULL;
                context = 0;
                siResult = eDSBadContextData; // make it one error code to simplify check in do/while
            }
            
        } while ( siResult == eDSBufferTooSmall || (siResult == eDSNoErr && context != 0) || siResult == eDSBadContextData );
    }
    
    catch( SInt32 err )
    {
        //TODO do something with this error?
    }	
    
    if ( pattMatch != NULL )
    {
        dsDataBufferDeAllocate( fDirRef, pattMatch );
        pattMatch = NULL;
    }
    if ( recType != NULL )
    {
        dsDataListDeallocate( fDirRef, recType );
        DSFree( recType );
    }
    if ( attrMatchType != NULL )
    {
        dsDataBufferDeAllocate( fDirRef, attrMatchType );
        attrMatchType = NULL;
    }
    if ( dataBuff != NULL )
    {
        dsDataBufferDeAllocate( fDirRef, dataBuff );
        dataBuff = NULL;
    }
    
    return( outBuffer );
    
}

#pragma mark -
#pragma mark Libinfo Support routines
#pragma mark -

void CCachePlugin::checkAAAAstatus( void )
{
    struct ifaddrs *ifa, *ifap;
    sa_family_t family;
    struct sockaddr_in6 *sin6;
    bool isLinkLocal, isLoopback;
    
    if (getifaddrs(&ifa) != 0) return;
    
    OSAtomicCompareAndSwap32Barrier( false, true, &aaaa_cutoff_enabled );
    
    if (fAlwaysDoAAAA) return;
    
    for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next)
    {
        family = ifap->ifa_addr->sa_family;
        if (family == AF_INET6)
        {
            sin6 = (struct sockaddr_in6 *)(ifap->ifa_addr);
            isLinkLocal = (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) != 0);
            isLoopback = (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr) != 0);
            
            /*
             * Anything other than link-local or loopback is considered routable.
             * If we have routable IPv6 addresees, we want to do AAAA queries in DNS.
             */
            if ((!isLinkLocal) && (!isLoopback)) OSAtomicCompareAndSwap32Barrier( true, false, &aaaa_cutoff_enabled );
        }
    }
    
    freeifaddrs(ifa);    

    DbgLog( kLogDebug, "CCachePlugin::checkAAAAstatus - %s AAAA queries", (aaaa_cutoff_enabled ? "skipping" : "not skipping") );
}

int AddToDNSThreads( void )
{
    int slot = -1;
    int ii;
    
    int rc = pthread_mutex_lock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::AddToDNSThreads - pthread_mutex_lock failed %s (%d).  Aborting",
			   strerror(rc), rc);
		abort();
	}

    for( ii = 0; ii < gActiveThreadCount; ii++ )
    {
        if( gActiveThreads[ii] == NULL )
        {
            slot = ii;
            break;
        }
    }
    
    // if we passed our active threads, no open slot
    if( slot == -1 && gActiveThreadCount < 512 )
    {
		slot = gActiveThreadCount;
        gActiveThreadCount++;
    }
	
	if ( slot != -1 )
	{
		gActiveThreads[slot] = pthread_self();
		gInterruptTokens[slot] = res_init_interrupt_token();
		DbgLog( kLogDebug, "CCachePlugin::AddToDNSThreads called added thread %X to slot %d", (unsigned long) gActiveThreads[slot], slot );
	}
	else
	{
		DbgLog( kLogDebug, "CCachePlugin::AddToDNSThreads called no slots available to add thread %X ", (unsigned long) pthread_self() );
	}
	
    rc = pthread_mutex_unlock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::AddToDNSThreads - pthread_mutex_unlock failed %s (%d).  Aborting",
			   strerror(rc), rc);
		abort();
	}
    
    return slot;
}

bool RemoveFromDNSThreads( int inSlot )
{
	bool	bReturn = false;
	
	if( inSlot == -1 ) 
	{
        DbgLog( kLogDebug, "CCachePlugin::RemoveFromDNSThreads called for slot with -1" );
        return bReturn;
	}
	
    int rc = pthread_mutex_lock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::RemoveFromDNSThreads - pthread_mutex_lock failed %s (%d).  Aborting",
			   strerror(rc), rc);
		abort();
	}
	
    DbgLog( kLogDebug, "CCachePlugin::RemoveFromDNSThreads called for slot %d = %X", inSlot, (unsigned long) gActiveThreads[inSlot] );

    gActiveThreads[inSlot] = NULL;
    
    if( gNotifyTokens[inSlot] != 0 )
    {
		if ( gThreadAbandon[inSlot] == false ) // ensure we didn't abandon intentionally
			bReturn = true;
		else
			DbgLog( kLogDebug, "CCachePlugin::RemoveFromDNSThreads lookup abandoned for slot %d not retrying", inSlot );
		
        notify_cancel( gNotifyTokens[inSlot] );
        DbgLog( kLogDebug, "CCachePlugin::RemoveFromDNSThreads cancelling token %d", gNotifyTokens[inSlot] );
        gNotifyTokens[inSlot] = 0;
		gThreadAbandon[inSlot] = false;
    }
	
	if ( gInterruptTokens[inSlot] != NULL )
	{
        DbgLog( kLogDebug, "CCachePlugin::RemoveFromDNSThreads deleting interrupt token %d", gInterruptTokens[inSlot] );
		res_delete_interrupt_token( gInterruptTokens[inSlot] );
		gInterruptTokens[inSlot] = NULL;
	}

    rc = pthread_mutex_unlock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::RemoveFromDNSThreads - pthread_mutex_unlock failed %s (%d).  Aborting",
			   strerror(rc), rc);
		abort();
	}
	
	return bReturn;
}

void CancelDNSThreads( void )
{
    int rc = pthread_mutex_lock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::CancelDNSThreads - pthread_mutex_lock failed %s (%d).  Aborting",
			    strerror(rc), rc);
		abort();
	}
	
	int numThreadsCancelled = 0;
    for( int ii = 0; ii < gActiveThreadCount; ii++ )
    {
        if( gActiveThreads[ii] != NULL && gNotifyTokens[ii] == 0 )
        {
            char notify_name[128];
            int notify_token = 0;
            snprintf(notify_name, sizeof(notify_name), "self.thread.%lu", (unsigned long) gActiveThreads[ii]);

            int status = notify_register_plain(notify_name, &notify_token);
            if (status == NOTIFY_STATUS_OK)
            {
                notify_set_state(notify_token, ThreadStateExitRequested);
                gNotifyTokens[ii] = notify_token;
                DbgLog( kLogDebug, "CCachePlugin::CancelDNSThreads called for slot %d notification '%s'", ii, notify_name );
				
				// let's interrupt inflight requests so the token(s) can be checked
				// we need to do for each thread to break multiple selects it seems
				// Only do this if we got a notify token - libresolv could spin if we
				// interrupt w/o a token.
				res_interrupt_request( gInterruptTokens[ii] );
				++numThreadsCancelled;
            }
        }
    }
	
	DbgLog( kLogDebug, "CCachePlugin::CancelDNSThreads %d threads cancelled", numThreadsCancelled );

    rc = pthread_mutex_unlock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::CancelDNSThreads - pthread_mutex_unlock failed %s (%d).  Aborting",
			   strerror(rc), rc);
		abort();
	}
}

void *DoDNSQuery( void *inData )
{
    dns_handle_t	dns			= dns_open( NULL );
    dns_reply_t     *dnsReply   = NULL;
    sDNSQuery       *theQuery   = (sDNSQuery *) inData;
    sDNSLookup      *lookup     = theQuery->fLookupEntry;
    int             index       = theQuery->fQueryIndex;
	int				rc;
	double          endTime     = 0.0;
    
lookupAgain:
	
    if( dns )
    {
        if( dns->sdns != NULL && aaaa_cutoff_enabled ) 
        {
            dns->sdns->flags |= DNS_FLAG_OK_TO_SKIP_AAAA;
            DbgLog( kLogDebug, "CCachePlugin::DoDNSQuery - Index %d enabling DNS_FLAG_OK_TO_SKIP_AAAA", index );
        }

        dns_set_buffer_size( dns, 8192 );
		
		// Reset the TTL in case this is a redo.
		lookup->fMinimumTTL[index] = -1;

        int slot = AddToDNSThreads();
		double startTime = dsTimestamp();
        dnsReply = idna_dns_lookup( dns, lookup->fQueryStrings[index], lookup->fQueryClasses[index], lookup->fQueryTypes[index], 
								    &(lookup->fMinimumTTL[index]) );
		endTime = dsTimestamp() - startTime;
		
        dns_free( dns );
        
		// RemoveFromDNSThreads returns true if it was cancelled, so we need to re-issue the request
		if ( RemoveFromDNSThreads(slot) == true )
		{
			dns = dns_open( NULL );
			
			// free any existing reply we don't care what it was
			if ( dnsReply != NULL )
			{
				dns_free_reply( dnsReply );
				dnsReply = NULL;
			}
			
			goto lookupAgain;
		}

		DbgLog( kLogPlugin, "CCachePlugin::DoDNSQuery - Index %d got %d answer(s) for %s type %d minTTL %d - %d ms", index, 
			    (dnsReply ? dnsReply->header->ancount : 0), lookup->fQueryStrings[index], lookup->fQueryTypes[index], lookup->fMinimumTTL[index],
			    (int) (endTime / 1000.0) );
	}
    
	// only need to lock for multi-threaded queries
	if ( theQuery->fIsParallel )
	{
		rc = pthread_mutex_lock( theQuery->fMutex );
		if ( rc != 0 )
		{
			DbgLog( kLogCritical, "CCachePlugin::DoDNSQuery - pthread_mutex_lock failed, %s (%d).  Aborting",
					strerror(rc), rc);
			abort();
		}
	}

    lookup->fAnswers[index] = dnsReply;
	lookup->fAnswerTime[index] = endTime;
	lookup->fThreadID[index] = NULL; // since we are done, let's NULL out our thread ID
	OSAtomicCompareAndSwap32Barrier( false, true, &(lookup->fQueryFinished[index]) );
	OSAtomicDecrement32Barrier( &(lookup->fOutstanding) );
    
    DbgLog( kLogDebug, "CCachePlugin::DoDNSQuery - Index %d, Outstanding = %d, Total = %d", index, lookup->fOutstanding, lookup->fTotal );
    
	// only need to signal for multi-threaded queries
	if ( theQuery->fIsParallel )
	{
		rc = pthread_cond_signal( theQuery->fCondition );
		if ( rc != 0 )
		{
			DbgLog( kLogCritical, "CCachePlugin::DoDNSQuery - pthread_cond_signal failed, %s (%d).  Aborting",
					strerror(rc), rc);
			abort();
		}

		rc = pthread_mutex_unlock( theQuery->fMutex );
		if ( rc != 0 )
		{
			DbgLog( kLogCritical, "CCachePlugin::DoDNSQuery - pthread_mutex_unlock failed, %s (%d).  Aborting",
					strerror(rc), rc);
			abort();
		}
	}

    return NULL;
}

bool CCachePlugin::IsLocalOnlyPID( pid_t inPID )
{
    bool    bReturn = false;
    
    fPIDListLock.WaitLock();
    if ( fLocalOnlyPIDs.find(inPID) != fLocalOnlyPIDs.end() )
        bReturn = true;
    fPIDListLock.SignalLock();
    
    return bReturn;
}

#pragma mark -
#pragma mark Libinfo API implementation routines
#pragma mark -

//------------------------------------------------------------------------------------
//	  * DSgetpwnam
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetpwnam ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t         *outBuffer		= NULL;
    tDataListPtr    attrTypes		= NULL;
    uint32_t        dictCount       = kvbuf_reset( inBuffer );
    uint32_t        valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "login") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "pw_name", name, NULL );
    if ( outBuffer == NULL )
        outBuffer = FetchFromCache( fLibinfoCache, NULL, "pw_gecos", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrUniqueID,
                                            kDS1AttrGeneratedUID,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrNFSHomeDirectory,
                                            kDS1AttrUserShell,
                                            kDS1AttrDistinguishedName,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "pw_name", "pw_uid", "pw_uuid", "pw_gecos", NULL };
            
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, NULL, 
												  NULL, fLibinfoCache, keys );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, name, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, NULL, 
												  NULL, fLibinfoCache, keys );
				
				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, NULL, 
													  NULL, fLibinfoCache, keys );
				}
			}
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            if( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_USER, kNegativeCacheTime, "pw_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetpwnam] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetpwnam] += 1;
        fStatsLock.SignalLock();
        DbgLog( kLogPlugin, "CCachePlugin::getpwnam - Cache hit for %s", name );
    }
    
    return( outBuffer );
    
} // DSgetpwnam

//------------------------------------------------------------------------------------
//	  * DSgetpwuuid
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetpwuuid ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t         *outBuffer		= NULL;
    tDataListPtr    attrTypes		= NULL;
    uint32_t        dictCount       = kvbuf_reset( inBuffer );
    uint32_t        valCount        = 0;
    sCacheValidation  *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "uuid") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "pw_uuid", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrUniqueID,
                                            kDS1AttrGeneratedUID,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrNFSHomeDirectory,
                                            kDS1AttrUserShell,
                                            kDS1AttrDistinguishedName,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry,
												NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry,
												NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry,
													NULL, NULL, &theValidation );
				}
			}
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "pw_name", "pw_uid", "pw_uuid", NULL };
                
                if( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_USER, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            if( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_USER, kNegativeCacheTime, "pw_uuid", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }

        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetpwuuid] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetpwuuid] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getpwuuid - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
    
} // DSgetpwuuid

//------------------------------------------------------------------------------------
//	  * DSgetpwuid
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetpwuid ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t			*outBuffer		= NULL;
    tDataListPtr	attrTypes		= NULL;
    uint32_t        dictCount       = kvbuf_reset( inBuffer );
    uint32_t        valCount        = 0;
    sCacheValidation     *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "uid") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "pw_uid", number, NULL );
    
    // no entry in the cache, let's look in DS
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrUniqueID,
                                            kDS1AttrGeneratedUID,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrNFSHomeDirectory,
                                            kDS1AttrUserShell,
                                            kDS1AttrDistinguishedName,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "pw_name", "pw_uid", "pw_uuid", NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrUniqueID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, 
												NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDS1AttrUniqueID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, 
												NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrUniqueID, number, kDSStdRecordTypeUsers, 1, attrTypes, ParsePasswdEntry, 
													NULL, NULL, &theValidation );
				}
			}			
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_USER, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_USER, kNegativeCacheTime, "pw_uid", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetpwuid] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetpwuid] += 1;
        fStatsLock.SignalLock();

        DbgLog( kLogPlugin, "CCachePlugin::getpwuid - Cache hit for %s", number );
    }	
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
    
} // DSgetpwuid


//------------------------------------------------------------------------------------
//	  * DSgetpwent
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetpwent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    // there is no cache here, we just get all the entries since this is what libinfo expects
    attrTypes = dsBuildListFromStrings( fDirRef,
                                        kDSNAttrMetaNodeLocation,
                                        kDSNAttrRecordName,
                                        kDS1AttrPassword,
                                        kDS1AttrUniqueID,
                                        kDS1AttrGeneratedUID,
                                        kDS1AttrPrimaryGroupID,
                                        kDS1AttrNFSHomeDirectory,
                                        kDS1AttrUserShell,
                                        kDS1AttrDistinguishedName,
                                        NULL );
    
    if ( attrTypes != NULL )
    {
        outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeUsers, 0, attrTypes, ParsePasswdEntry, NULL, NULL, 
                                          NULL, NULL );
        
        dsDataListDeallocate( fDirRef, attrTypes );
        DSFree( attrTypes );
    }
    
    return( outBuffer );
    
} // DSgetpwent


//------------------------------------------------------------------------------------
//	  * DSgetgrnam
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetgrnam ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "gr_name", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrGeneratedUID,
                                            kDSNAttrGroupMembership,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "gr_name", "gr_gid", "gr_uuid", NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, NULL, NULL,
												  fLibinfoCache, keys );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, name, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, NULL, NULL,
												  fLibinfoCache, keys );
				
				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, NULL, NULL,
													  fLibinfoCache, keys );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_GROUP, kNegativeCacheTime, "gr_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }

        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetgrnam] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetgrnam] += 1;
        fStatsLock.SignalLock();

        DbgLog( kLogPlugin, "CCachePlugin::getgrnam - Cache hit for %s", name );
    }
    
    return( outBuffer );
    
} // DSgetgrnam

//------------------------------------------------------------------------------------
//	  * DSgetgruuid
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetgruuid ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation     *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "uuid") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "gr_uuid", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrGeneratedUID,
                                            kDSNAttrGroupMembership,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
												NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
												NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrGeneratedUID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
													NULL, NULL, &theValidation );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "gr_name", "gr_gid", "gr_uuid", NULL };
                
                if ( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_GROUP, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_GROUP, kNegativeCacheTime, "gr_uuid", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetgruuid] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetgruuid] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getgruuid - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
    
} // DSgetgruuid

//------------------------------------------------------------------------------------
//	  * DSgetgrgid
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetgrgid ( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation     *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "gid") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "gr_gid", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrGeneratedUID,
                                            kDSNAttrGroupMembership,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrPrimaryGroupID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
												NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDS1AttrPrimaryGroupID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
												NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrPrimaryGroupID, number, kDSStdRecordTypeGroups, 1, attrTypes, ParseGroupEntry, 
													NULL, NULL, &theValidation );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "gr_name", "gr_gid", "gr_uuid", NULL };
                
                if ( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_GROUP, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_GROUP, kNegativeCacheTime, "gr_gid", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetgrgid] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetgrgid] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getgrgid - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
    
} // DSgetgrgid


//------------------------------------------------------------------------------------
//	  * DSgetgrent
//------------------------------------------------------------------------------------ 

kvbuf_t* CCachePlugin::DSgetgrent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrPassword,
                                            kDS1AttrPrimaryGroupID,
                                            kDS1AttrGeneratedUID,
                                            kDSNAttrGroupMembership,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeGroups, 0, attrTypes, ParseGroupEntry, NULL, NULL,
                                              NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
    
} // DSgetgrent

kvbuf_t* CCachePlugin::DSgetfsbyname( kvbuf_t *inBuffer )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "fs_spec", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrVFSLinkDir,
                                            kDS1AttrVFSType,
                                            kDSNAttrVFSOpts,
                                            kDS1AttrVFSDumpFreq,
                                            kDS1AttrVFSPassNo,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "fs_spec", NULL };

            outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeMounts, 1, attrTypes, ParseMountEntry, NULL, NULL,
                                              fLibinfoCache, keys );
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if ( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_MOUNT, kNegativeCacheTime, "fs_spec", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetfsbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetfsbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getfsbyname - Cache hit for %s", name );
    }
    
    return( outBuffer );    
}

kvbuf_t* CCachePlugin::DSgetfsent( pid_t inPID )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrVFSLinkDir,
                                            kDS1AttrVFSType,
                                            kDSNAttrVFSOpts,
                                            kDS1AttrVFSDumpFreq,
                                            kDS1AttrVFSPassNo,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if ( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeMounts, 0, attrTypes, ParseMountEntry, NULL, NULL,
												  NULL, NULL );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, kDSRecordsAll, kDSStdRecordTypeMounts, 0, attrTypes, ParseMountEntry, outBuffer, NULL,
												  NULL, NULL );
				
				outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, kDSRecordsAll, kDSStdRecordTypeMounts, 0, attrTypes, ParseMountEntry, outBuffer, NULL,
												  NULL, NULL );
			}			
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
    
} // DSgetgrent

kvbuf_t* CCachePlugin::DSgetaliasbyname( kvbuf_t *inBuffer )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "alias_name", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDSNAttrMember,
                                            "dsAttrTypeNative:members",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char *keys[] = {"alias_name",NULL};
            
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeAliases, 1, attrTypes, ParseAliasEntry, 
                                            NULL, NULL, fLibinfoCache, keys );
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if ( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_ALIAS, kNegativeCacheTime, "alias_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUalias_getbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUalias_getbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getaliasbyname - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetaliasent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDSNAttrMember,
                                            "dsAttrTypeNative:members",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeAliases, 0, attrTypes, ParseAliasEntry,
                                              NULL, NULL, NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetservbyname( kvbuf_t *inBuffer, pid_t inPID )
{
    uint32_t		dictCount	= kvbuf_reset( inBuffer );
    char			*service	= NULL;
    char			*proto		= "";
    kvbuf_t			*outBuffer	= NULL;
    tDataListPtr	attrTypes	= NULL;
    char            *pValues[]  = { NULL, NULL }; // port, protocol
    sCacheValidation    *theValidation   = NULL;
    
    if( dictCount != 0 )
    {
        char		*key	= NULL;
        uint32_t	count;
        
        kvbuf_next_dict( inBuffer );
        while( (key = kvbuf_next_key(inBuffer, &count)) ) 
        {
            if( strcmp(key, "name") == 0 )
                service = kvbuf_next_val( inBuffer );
            else if( strcmp(key, "proto") == 0 )
                proto = kvbuf_next_val( inBuffer );
        }
    }
    
    if( service != NULL && proto != NULL )
    {
		// allowed to not pass a protocol, we find the first available
		if (proto[0] == '\0')
			outBuffer = FetchFromCache( fLibinfoCache, NULL, "s_name", service, NULL );
		else
			outBuffer = FetchFromCache( fLibinfoCache, NULL, "s_name", service, "s_proto", proto, NULL );

        
        if ( outBuffer == NULL )
        {
            attrTypes = dsBuildListFromStrings( fDirRef,
                                                kDSNAttrMetaNodeLocation,
                                                kDSNAttrRecordName,
                                                "dsAttrTypeNative:PortAndProtocol",
                                                NULL );
            
            if ( attrTypes != NULL )
            {
                pValues[1] = proto;
                
				bool localOnlyPID = IsLocalOnlyPID( inPID );
                if( localOnlyPID == false )
                {
                    if ( proto[0] == '\0' )
                    {
                        outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 1, attrTypes, ParseServiceEntry,
                                                        NULL, NULL, &theValidation );
                    }
                    else
                    {
                        outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 0, attrTypes, ParseServiceEntry,
                                                        NULL, (void *)pValues, &theValidation );
                    }
                }
                else
                {
					// check then Local node first
					// then FFPlugin 
                    if ( proto[0] == '\0' )
                    {
                        outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 1, attrTypes, ParseServiceEntry, 
                                                        NULL, NULL, &theValidation );
                    }
                    else
                    {
                        outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 0, attrTypes, ParseServiceEntry, 
                                                        NULL, (void *)pValues, &theValidation );
                    }
					
					if( outBuffer == NULL )
					{
                        if ( proto[0] == '\0' )
                        {
                            outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 1, attrTypes,
                                                            ParseServiceEntry, NULL, NULL, &theValidation );
                        }
                        else
                        {
                            outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDSNAttrRecordName, service, kDSStdRecordTypeServices, 0, attrTypes,
                                                           ParseServiceEntry, NULL, (void *)pValues, &theValidation );
                        }
					}
                }
                
                if( outBuffer != NULL )
                {
					char    *keyList1[] = { "s_name", "s_proto", NULL };
					char    *keyList2[] = { "s_port", "s_proto", NULL };
					char	*keyList3[] = { "s_name", NULL }; // we only pass this if we have no protocol
					kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
					
					AddEntryToCacheWithKeylists( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_SERVICE, 0, keyList1, keyList2, 
												 (proto[0] == '\0' ? keyList3 : NULL), NULL );
				}
                
                // need to check this specific so we can do a negative cache
                if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
                {
					// if no protocol was specified, only cache the name as negative with no port
					if (proto[0] == '\0')
						AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_SERVICE, kNegativeCacheTime,
													 "s_name", service, NULL );
					else
						AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_SERVICE, kNegativeCacheTime,
													 "s_name", service, "s_proto", proto, NULL );
                }
                
                dsDataListDeallocate( fDirRef, attrTypes );
                DSFree( attrTypes );
            }
            
            fStatsLock.WaitLock();
            fCacheMissByFunction[kDSLUgetservbyname] += 1;
            fStatsLock.SignalLock();
        }
        else
        {
            fStatsLock.WaitLock();
            fCacheHitsByFunction[kDSLUgetservbyname] += 1;
            fStatsLock.SignalLock();
 
            DbgLog( kLogPlugin, "CCachePlugin::getservbyname - Cache hit for %s:%s", service, proto );
        }
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return outBuffer;
}

kvbuf_t* CCachePlugin::DSgetservbyport( kvbuf_t *inBuffer, pid_t inPID )
{
    uint32_t		dictCount	= kvbuf_reset( inBuffer );
    char			*port		= NULL;
    char			*proto		= NULL;
    kvbuf_t			*outBuffer	= NULL;
    tDataListPtr	attrTypes	= NULL;
    sCacheValidation    *theValidation   = NULL;

    if( dictCount != 0 )
    {
        char		*key	= NULL;
        uint32_t	count;
        
        kvbuf_next_dict( inBuffer );
        while( (key = kvbuf_next_key(inBuffer, &count)) ) 
        {
            if( strcmp(key, "port") == 0 )
                port = kvbuf_next_val( inBuffer );
            else if( strcmp(key, "proto") == 0 )
                proto = kvbuf_next_val( inBuffer );
        }
    }
    
    if( port != NULL && proto != NULL )
    {
		if (proto[0] == '\0')
			outBuffer = FetchFromCache( fLibinfoCache, NULL, "s_port", port, NULL );
		else
			outBuffer = FetchFromCache( fLibinfoCache, NULL, "s_port", port, "s_proto", proto, NULL );
        
        if ( outBuffer == NULL )
        {
            attrTypes = dsBuildListFromStrings( fDirRef,
                                                kDSNAttrMetaNodeLocation,
                                                kDSNAttrRecordName,
                                                "dsAttrTypeNative:PortAndProtocol",
                                                NULL );
            
            if ( attrTypes != NULL )
            {
                char    specificSearch[64] = { 0, };
                
                if (proto[0] != '\0')
                {
                    snprintf( specificSearch, sizeof(specificSearch), "%s/%s", port, proto );
                }
                
				bool localOnlyPID = IsLocalOnlyPID( inPID );
                if( localOnlyPID == false )
                {
                    if (specificSearch[0] != '\0')
                    {
                        outBuffer = ValueSearchLibInfo( fSearchNodeRef, "dsAttrTypeNative:PortAndProtocol", specificSearch, kDSStdRecordTypeServices, 0, 
                                                        attrTypes, ParseServiceEntry, NULL, NULL, &theValidation );
                    }
                    else
                    {
                        outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrPort, port, kDSStdRecordTypeServices, 1, attrTypes, ParseServiceEntry, 
                                                        NULL, NULL, &theValidation );
                    }
                }
                else
                {
					// check then Local node first
					// then FFPlugin 
                    if (specificSearch[0] != '\0')
                    {
                        outBuffer = ValueSearchLibInfo( fLocalNodeRef, "dsAttrTypeNative:PortAndProtocol", specificSearch, kDSStdRecordTypeServices, 
                                                        0, attrTypes, ParseServiceEntry, outBuffer, NULL, &theValidation );
                    }
                    else
                    {
                        outBuffer = ValueSearchLibInfo( fLocalNodeRef, kDS1AttrPort, port, kDSStdRecordTypeServices, 
                                                        1, attrTypes, ParseServiceEntry, outBuffer, NULL, &theValidation );
                    }
					
					if( outBuffer == NULL )
					{
                        if (specificSearch[0] != '\0')
                        {
                            outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, "dsAttrTypeNative:PortAndProtocol", specificSearch, 
                                                           kDSStdRecordTypeServices, 0, attrTypes, ParseServiceEntry, outBuffer, NULL, &theValidation );
                        }
                        else
                        {
                            outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrPort, port, kDSStdRecordTypeServices, 
                                                            1, attrTypes, ParseServiceEntry, outBuffer, NULL, &theValidation );
                        }
					}
                }
                
                if( outBuffer != NULL )
                {
                    // add this entry to the cache
                    char    *keyList1[] = { "s_name", "s_proto", NULL };
                    char    *keyList2[] = { "s_port", "s_proto", NULL };
                    char    *keyList3[] = { "s_port", NULL };
                    kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeylists( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_SERVICE, 0, keyList1, keyList2, 
                                                 (proto[0] == '\0' ? keyList3 : NULL), NULL );
                }
                
                // need to check this specific so we can do a negative cache
                if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
                {
					if (proto[0] == '\0')
						AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_SERVICE, kNegativeCacheTime,
													 "s_port", port, NULL );
					else
						AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_SERVICE, kNegativeCacheTime, 
													 "s_port", port, "s_proto", proto, NULL );
                }
                
                dsDataListDeallocate( fDirRef, attrTypes );
                DSFree( attrTypes );
            }
            
            fStatsLock.WaitLock();
            fCacheMissByFunction[kDSLUgetservbyport] += 1;
            fStatsLock.SignalLock();
        }
        else
        {
            fStatsLock.WaitLock();
            fCacheHitsByFunction[kDSLUgetservbyport] += 1;
            fStatsLock.SignalLock();
            
            DbgLog( kLogPlugin, "CCachePlugin::getservbyport - Cache hit for %s:%s", port, proto );
        }
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return outBuffer;
}

kvbuf_t* CCachePlugin::DSgetservent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:PortAndProtocol",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeServices, 0, attrTypes, ParseServiceEntry, 
                                              NULL, NULL, NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetprotobyname( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "p_name", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "p_name", "p_proto", NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeProtocols, 1, attrTypes, ParseProtocolEntry, NULL, NULL,
												  fLibinfoCache, keys );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, name, kDSStdRecordTypeProtocols, 1, attrTypes, ParseProtocolEntry, NULL, NULL,
												  fLibinfoCache, keys );
				
				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeProtocols, 1, attrTypes, ParseProtocolEntry, NULL, NULL,
													  fLibinfoCache, keys );
				}
			}			
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_PROTOCOL, kNegativeCacheTime, "p_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetprotobyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetprotobyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getprotobyname - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetprotobynumber( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation     *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "number") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "p_proto", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeProtocols, 1, attrTypes, 
                                                ParseProtocolEntry, NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeProtocols, 1, attrTypes, 
                                                ParseProtocolEntry, NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeProtocols, 1, attrTypes, ParseProtocolEntry, 
					NULL, NULL, &theValidation );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "p_name", "p_proto", NULL };
                
                if( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_PROTOCOL, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_PROTOCOL, kNegativeCacheTime, "p_proto", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetprotobynumber] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetprotobynumber] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getprotobynumber - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetprotoent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeProtocols, 0, attrTypes, ParseProtocolEntry, 
                                              NULL, NULL, NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetrpcbyname( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "r_name", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "r_name", "r_number", NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, NULL, NULL,
												  fLibinfoCache, keys );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, name, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, NULL, NULL,
												  fLibinfoCache, keys );
				
				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, NULL, NULL,
													  fLibinfoCache, keys );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_RPC, kNegativeCacheTime, "r_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetrpcbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetrpcbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getrpcbyname - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetrpcbynumber( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation     *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "number") != 0 || valCount == 0 )
        return NULL;
    
    char *number = kvbuf_next_val( inBuffer );
    if( number == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "r_number", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, 
												NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, 
												NULL, NULL, &theValidation );
				
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, "dsAttrTypeNative:number", number, kDSStdRecordTypeRPC, 1, attrTypes, ParseRPCEntry, 
													NULL, NULL, &theValidation );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "r_name", "r_number", NULL };
                
                if( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_RPC, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_RPC, kNegativeCacheTime, "r_number", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetrpcbynumber] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetrpcbynumber] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getrpcbynumber - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetrpcent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:number",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeRPC, 0, attrTypes, ParseRPCEntry, NULL, NULL,
                                              NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetnetbyname( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "n_name", name, NULL );
    
    // check cache under aliases
    if ( outBuffer == NULL )
    {
        outBuffer = FetchFromCache( fLibinfoCache, NULL, "n_aliases", name, NULL );
    }
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:address",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "n_name", "n_aliases", "n_net", NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeNetworks, 1, attrTypes, ParseNetworkEntry, NULL, NULL,
												  fLibinfoCache, keys );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, name, kDSStdRecordTypeNetworks, 1, attrTypes, ParseNetworkEntry, NULL, NULL,
												  fLibinfoCache, keys );
				
				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeNetworks, 1, attrTypes, ParseNetworkEntry, NULL, NULL,
													  fLibinfoCache, keys );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_NETWORK, kNegativeCacheTime, "n_name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetnetbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetnetbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getnetbyname - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetnetbyaddr( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t            *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    char               *number          = NULL;
    sCacheValidation *theValidation   = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    if( dictCount != 0 )
    {
        char		*key	= NULL;
        uint32_t	count;
        
        kvbuf_next_dict( inBuffer );
        while( (key = kvbuf_next_key(inBuffer, &count)) ) 
        {
            if( strcmp(key, "net") == 0 )
                number = kvbuf_next_val( inBuffer );
//			else if( strcmp(key, "type") == 0 )
//				proto = kvbuf_next_val( inBuffer );
        }
    }
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "n_net", number, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:address",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
			{
				outBuffer = ValueSearchLibInfo( fSearchNodeRef, "dsAttrTypeNative:address", number, kDSStdRecordTypeNetworks, 1, attrTypes,
												ParseNetworkEntry, NULL, NULL, &theValidation );
			}
			else
			{
				// check then Local node first
				// then FFPlugin 
				outBuffer = ValueSearchLibInfo( fLocalNodeRef, "dsAttrTypeNative:address", number, kDSStdRecordTypeNetworks, 1, attrTypes,
												ParseNetworkEntry, NULL, NULL, &theValidation );
				if( NULL == outBuffer )
				{
					outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, "dsAttrTypeNative:address", number, kDSStdRecordTypeNetworks, 1, attrTypes,
													ParseNetworkEntry, NULL, NULL, &theValidation );
				}
			}			
			
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                char	*keys[] = { "n_name", "n_aliases", "n_net", NULL };
                
                if( dcount == 1 )
                {
                    kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_NETWORK, 0, keys );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_NETWORK, kNegativeCacheTime, "n_net", number, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetnetbyaddr] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetnetbyaddr] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getnetbyaddr - Cache hit for %s", number );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetnetent( void )
{
    kvbuf_t				   *outBuffer		= NULL;
    tDataListPtr			attrTypes		= NULL;
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:address",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeNetworks, 0, attrTypes, ParseNetworkEntry, 
                                              NULL, NULL, NULL, NULL );
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetnetgrent( kvbuf_t *inBuffer )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    char                *name           = NULL;
    char                *key            = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    // note this does a query for a netgroup name and called by setnetgrent, not actually by getnetgrent
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "netgroup") == 0 )
            name = kvbuf_next_val( inBuffer );
    }
    
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "netgroup", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDSNAttrNetGroups,
                                            "dsAttrTypeNative:triplet",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeNetGroups, 1, attrTypes, ParseNetGroupEntry, NULL, NULL,
                                              NULL, NULL );
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_GROUP, kNegativeCacheTime, "netgroup", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
    }
    else
    {
        DbgLog( kLogPlugin, "CCachePlugin::getnetgrent - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t *CCachePlugin::DSinnetgr( kvbuf_t *inBuffer )
{
    kvbuf_t             *tempBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    char                *key            = NULL;
    char                *name           = NULL;
    char                *triplet[3]     = { "", "", "" };
    
    if( dictCount == 0 )
        return NULL;
    
    // note this does a query for a netgroup name and called by setnetgrent, not actually by getnetgrent
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "netgroup") == 0 )
            name = kvbuf_next_val( inBuffer );
        else if( strcmp(key, "host") == 0 )
            triplet[0] = kvbuf_next_val( inBuffer );
        else if( strcmp(key, "user") == 0 )
            triplet[1] = kvbuf_next_val( inBuffer );
        else if( strcmp(key, "domain") == 0 )
            triplet[2] = kvbuf_next_val( inBuffer );
    }
    
    if( name == NULL )
        return NULL;
    
    tempBuffer = FetchFromCache( fLibinfoCache, NULL, "netgroup", name, "host", triplet[0], "user", triplet[1], "domain", triplet[2], NULL );
    
    if ( tempBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            "dsAttrTypeNative:triplet",
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            tempBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeNetGroups, 1, attrTypes, ParseNetGroupEntry, NULL, triplet,
                                              fLibinfoCache, NULL );
            
            // need to check this specific so we can do a negative cache
            if ( tempBuffer == NULL )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_GROUP, 0, "netgroup", name, "host", triplet[0], 
											 "user", triplet[1], "domain", triplet[2], NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUinnetgr] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUinnetgr] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::innetgr - Cache hit for %s - %s, %s, %s", name, (triplet[0] ? triplet[0] : ""), 
                 (triplet[1] ? triplet[1] : ""), (triplet[2] ? triplet[2] : ""));
    }
    
    kvbuf_t *outBuffer = kvbuf_new();
    kvbuf_add_dict( outBuffer );
    kvbuf_add_key( outBuffer, "result" );
    kvbuf_add_val( outBuffer, (kvbuf_reset(tempBuffer) > 0 ? "1" : "0") );

    kvbuf_free( tempBuffer );
    
    return outBuffer;
}

void CCachePlugin::InitiateDNSQuery( sDNSLookup *inLookup, bool inParallel )
{
    if ( inParallel && inLookup->fTotal > 1 )
    {
        DbgLog( kLogDebug, "CCachePlugin::InitiateDNSQuery - Called with Total = %d", inLookup->fTotal );

		IssueParallelDNSQueries( inLookup );
    }
    else
    {
		if ( inParallel )
            DbgLog( kLogDebug, "CCachePlugin::InitiateDNSQuery - AI_PARALLEL was requested, but only one query" );
		

        for (int index = 0;  index < inLookup->fTotal;  index++ )
        {
            sDNSQuery   query;
            
            query.fLookupEntry = inLookup;
            query.fQueryIndex = index;
			query.fIsParallel = false;
			query.fMutex = NULL;
			query.fCondition = NULL;
            
			OSAtomicCompareAndSwap32Barrier( inLookup->fOutstanding, 1, &inLookup->fOutstanding );

            DoDNSQuery( &query );
        }
    }
}

void CCachePlugin::IssueParallelDNSQueries( sDNSLookup *inLookup )
{
	int	iIndexOfAReq	= -1;

	pthread_mutexattr_t dnsMutexAttr;
	int rc = pthread_mutexattr_init( &dnsMutexAttr );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_mutexattr_init failed, %s (%d).  Aborting.",
				strerror(rc), rc);
		abort();
	}

	rc = pthread_mutexattr_settype( &dnsMutexAttr, PTHREAD_MUTEX_ERRORCHECK );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_mutexattr_settype failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}

	pthread_mutex_t dnsMutex;
	rc = pthread_mutex_init( &dnsMutex, &dnsMutexAttr );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_mutex_init failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}

	pthread_cond_t dnsCondition;
	rc = pthread_cond_init( &dnsCondition, NULL );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_mutex_init failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}

	bool reissueQueries;

	do
	{
		int iSlot = AddToDNSThreads();

		reissueQueries = false;  // assume the queries will finish.
		OSAtomicCompareAndSwap32Barrier( inLookup->fOutstanding, 0, &inLookup->fOutstanding );

		sDNSQuery *pQueries = new sDNSQuery[inLookup->fTotal];

		for (int index = 0;  index < inLookup->fTotal;  index++ )
		{
			sDNSQuery       *pQuery     = &pQueries[index];

			pQuery->fLookupEntry = inLookup;
			pQuery->fQueryIndex = index;
			pQuery->fIsParallel = true;
			pQuery->fMutex = &dnsMutex;
			pQuery->fCondition = &dnsCondition;

			if ( inLookup->fQueryTypes[index] == fTypeA )
				iIndexOfAReq = index;

			pthread_attr_t attrs;
			pthread_attr_init( &attrs );
			pthread_attr_setdetachstate( &attrs, PTHREAD_CREATE_DETACHED );

			while ( pthread_create( &(inLookup->fThreadID[index]), &attrs, DoDNSQuery, pQuery ) != 0 )
			{
				usleep( 1000 );
			}

			// increment here to ensure fOutstanding is non-zero before we go
			// into the wait.  it's possible that the threads won't even run
			// until this thread blocks.
			OSAtomicIncrement32Barrier( &inLookup->fOutstanding );

			DbgLog( kLogDebug, "CCachePlugin::IssueParallelDNSQueries - spawned thread #%d, thread ID = %X, Outstanding = %d, Total = %d",
				   index, inLookup->fThreadID[index], inLookup->fOutstanding, inLookup->fTotal );		
		}

		WaitForDNSQueries( iIndexOfAReq, inLookup, &dnsMutex, &dnsCondition );

		// if our lookup was cancelled, we need to re-issue all DNS lookups
		if ( RemoveFromDNSThreads(iSlot) == true )
		{
			DbgLog( kLogDebug, "CCachePlugin::IssueParallelDNSQueries - re-issuing queries due to DNS change, waiting for threads to finish" );

			// delete all answers and re-issue
			for ( int ii = 0; ii < inLookup->fTotal; ii++ )
			{
				if ( inLookup->fAnswers[ii] != NULL ) {
					dns_free_reply( inLookup->fAnswers[ii] );
					inLookup->fAnswers[ii] = NULL;
				}
				inLookup->fQueryFinished[ii] = false;
				inLookup->fAnswerTime[ii] = 0.0;
			}

			OSMemoryBarrier(); // ensure we are sync'd across processors at this point

			reissueQueries = true;
		}

		delete[] pQueries;
	} while ( reissueQueries );

	rc = pthread_cond_destroy( &dnsCondition );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_cond_destroy failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}

	rc = pthread_mutex_destroy( &dnsMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::IssueParallelDNSQueries - pthread_mutex_destroy failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}
}

void CCachePlugin::WaitForDNSQueries( int              inIndexOfAReq,
									  sDNSLookup      *inLookup,
									  pthread_mutex_t *dnsMutex,
									  pthread_cond_t  *dnsCondition )
{
	struct timeval	tvNow;
	struct timespec	tsTimeout   = { 0 };
	struct timespec	tsWaitTime  = { 60, 0 }; // only wait 60 seconds.

	if ( inIndexOfAReq >= 0 )
	{
		DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - Waiting (60 sec max) for first A response, Outstanding = %d, Total = %d", 
				inLookup->fOutstanding, inLookup->fTotal );
	}
	else
	{
		DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - Waiting (60 sec max), Outstanding = %d, Total = %d",
				inLookup->fOutstanding, inLookup->fTotal );
	}

	int rc = pthread_mutex_lock( dnsMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::WaitForDNSQueries - pthread_mutex_lock failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}

	bool bWaitingForCancelledThreads = false;

	while ( inLookup->fOutstanding > 0 )
	{
		gettimeofday( &tvNow, NULL );
		TIMEVAL_TO_TIMESPEC ( &tvNow, &tsTimeout );
		ADD_MACH_TIMESPEC( &tsTimeout, &tsWaitTime );
		
		rc = pthread_cond_timedwait( dnsCondition, dnsMutex, &tsTimeout );
		if ( rc == ETIMEDOUT )
		{
			DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - timed out after %d %s  - Outstanding = %d, Total = %d",
					tsWaitTime.tv_sec ? tsWaitTime.tv_sec : tsWaitTime.tv_nsec * 1000000,
					tsWaitTime.tv_sec ? "sec" : "ms", inLookup->fOutstanding, inLookup->fTotal );

			if ( !bWaitingForCancelledThreads )
			{
				DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - cancelling remaining threads - Outstanding = %d, Total = %d.  Will wait 30 seconds for threads to cancel.",
						inLookup->fOutstanding, inLookup->fTotal );

				// Timed out waiting for the queries to complete.  Cancel them
				// and wait for them to finish.
				AbandonDNSQueries( inLookup );
				bWaitingForCancelledThreads = true;
				tsWaitTime.tv_sec = 30;
				tsWaitTime.tv_nsec = 0;
			}
			else
			{
				// This is bad - the threads were cancelled but they haven't come back.
				// Those threads are sharing inLookup as well as pointers to dnsMutex
				// and dnsCondition.
				// If they haven't come back, they're likely hung in libresolv.
				syslog( LOG_ERR, "CCachePlugin::WaitForDNSQueries - aborting because cancelled threads didn't return after 30 seconds." );
				abort();
			}
		}
		else if ( rc != 0 )
		{
			DbgLog( kLogCritical, "CCachePlugin::WaitForDNSQueries - pthread_cond_timedwait failed, %s (%d).  Aborting",
					strerror(rc), rc);
			abort();
		}
		else // rc == 0, we were signalled.
		{
			if ( inLookup->fOutstanding == 0 )
			{
				DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - queries complete" );
			}
			else
			{
				// got at least one answer back, but waiting on others.  determine
				// how much longer we'll wait for the rest of the queries to finish.

				double maxTime = 0.0;  // in microseconds

				// if we are looking for an A request and got an answer, use that time as our calculation
				// if we are looking for an A request and didn't get an answer, wait 60 seconds.
				if ( inIndexOfAReq >= 0 )
				{
					if ( inLookup->fQueryFinished[inIndexOfAReq] )
					{
						maxTime = inLookup->fAnswerTime[inIndexOfAReq]; // use the time of that request directly
					}
					else
					{
						maxTime = 60000000.0; // 60 seconds in microseconds
					}
				}
				else
				{
					// find response with highest time and use that as our further delay
					for ( int ii = 0; ii < inLookup->fTotal; ii++ ) 
					{
						// if the query is finished and we got an answer
						if ( inLookup->fQueryFinished[ii] && inLookup->fAnswerTime[ii] > maxTime )
						{
							maxTime = inLookup->fAnswerTime[ii];
						}
					}
				}

				int milliSecs = (maxTime / 1000) * inLookup->fTotal;
				milliSecs = MAX( milliSecs, 100 );    // wait at least 100 ms ...
				milliSecs = MIN( milliSecs, 60000 );  // ... but no more than 60
				tsWaitTime.tv_sec = (milliSecs / 1000);
				tsWaitTime.tv_nsec = ((milliSecs % 1000) * 1000000);

				DbgLog( kLogDebug, "CCachePlugin::WaitForDNSQueries - got at least 1 answer, waiting %d ms for the rest", milliSecs );
			}
		}
	}

	rc = pthread_mutex_unlock( dnsMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::WaitForDNSQueries - dnsMutex pthread_mutex_unlock failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}
}

void CCachePlugin::AbandonDNSQueries( sDNSLookup *inLookup )
{
	int rc = pthread_mutex_lock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::AbandonDNSQueries - gActiveThreadMutex pthread_mutex_lock failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}
	
	int numThreadsCancelled = 0;
	for ( int ii = 0; ii < inLookup->fTotal; ii++ )
	{
		for ( int zz = 0; zz < gActiveThreadCount; zz++ )
		{
			// see if it matches the ID we are looking for so we get the slot
			if ( inLookup->fThreadID[ii] != NULL && gActiveThreads[zz] == inLookup->fThreadID[ii] )
			{
				if ( gNotifyTokens[zz] == 0 )
				{
					char notify_name[128];
					int notify_token = 0;
					
					snprintf( notify_name, sizeof(notify_name), "self.thread.%lu", (unsigned long) gActiveThreads[zz] );
					
					int status = notify_register_plain( notify_name, &notify_token );
					if (status == NOTIFY_STATUS_OK)
					{
						notify_set_state( notify_token, ThreadStateExitRequested );
						gNotifyTokens[zz] = notify_token;
						gThreadAbandon[zz] = true;
						DbgLog( kLogDebug, "CCachePlugin::AbandonDNSQueries called for slot %d notification '%s'", zz, notify_name );
						
						// let's interrupt inflight requests so the token can be checked
						// we need to do for each thread to break multiple selects it seems.
						// Only do this if we got a notify token - libresolv could spin if we
						// interrupt w/o a token.
						res_interrupt_request( gInterruptTokens[zz] );
						++numThreadsCancelled;
					}
				}
				else
				{
					DbgLog( kLogDebug, "CCachePlugin::AbandonDNSQueries slot %d already being cancelled, just setting abandon flag", zz );
					gThreadAbandon[zz] = true;
				}
				
				// we can break out of this loop and continue
				break;
			}
		}
	}

	DbgLog( kLogDebug, "CCachePlugin::AbandonDNSQueries %d threads cancelled", numThreadsCancelled );

	rc = pthread_mutex_unlock( &gActiveThreadMutex );
	if ( rc != 0 )
	{
		DbgLog( kLogCritical, "CCachePlugin::AbandonDNSQueries - dnsMutex pthread_mutex_unlock failed, %s (%d).  Aborting",
				strerror(rc), rc);
		abort();
	}
}

kvbuf_t* CCachePlugin::DSgethostbyname( kvbuf_t *inBuffer, pid_t inPID, bool inParallelQuery, sDNSLookup *inLookup )
{
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    char                *pIPv4          = "0";
    char                *pIPv6          = "0";
    char                *key;
    char                *name           = NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "name") == 0 )
        {
            name = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "ipv4") == 0 )
        {
            pIPv4 = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "ipv6") == 0 )
        {
            pIPv6 = kvbuf_next_val( inBuffer );
        }
    }
    
    return DSgethostbyname_int( name, pIPv4, pIPv6, inPID, inParallelQuery, inLookup );
}

kvbuf_t* CCachePlugin::DSgethostbyname_int( char *inName, const char *inIPv4, const char *inIPv6, int inPID, bool inParallelQuery, sDNSLookup *inLookup, 
                                            uint32_t *outTTL )
{
    uint32_t            ipv4            = 0;
    uint32_t            ipv6            = 0;
    kvbuf_t             *outBuffer      = NULL;
    tDataListPtr        attrTypes       = NULL;
    uint32_t            ttl             = 0;
    sCacheValidation    *theValidation	= NULL;

    if( NULL != inIPv4 )
    {
        ipv4 = strtol( inIPv4, NULL, 10 );
    }

    if( NULL != inIPv6 )
    {
        ipv6 = strtol( inIPv6, NULL, 10 );
    }

    if( NULL == inName || (0 == ipv4 && 0 == ipv6) )
        return NULL;
    
    // TODO:  should we lowercase the search string??? what's impact on International Domain support?

    // if ipv4 is set, then we have to check that flag first, otherwise we won't check order properly
    if( ipv4 != 0 )
    {
        outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_name", inName, "ipv4", inIPv4, (ipv6 ? "ipv6" : NULL), inIPv6, NULL );
        
        // now check the aliases
        if( outBuffer == NULL )
        {
            outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_aliases", inName, "ipv4", inIPv4, (ipv6 ? "ipv6" : NULL), inIPv6, NULL );
        }
    }
    else
    {
        outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_name", inName, "ipv6", inIPv6, NULL );
        
        // now check the aliases
        if( outBuffer == NULL )
        {
            outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_aliases", inName, "ipv6", inIPv6, NULL );
        }
    }
    
    if( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            (1 == ipv4 ? kDSNAttrIPAddress : (1 == ipv6 ? kDSNAttrIPv6Address : NULL)),
                                            ((1 == ipv4 && 1 == ipv6) ? kDSNAttrIPv6Address : NULL),
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = GetRecordListLibInfo( fSearchNodeRef, inName, kDSStdRecordTypeHosts, 1, attrTypes, ParseHostEntry, NULL, NULL,
                                                  NULL, NULL, &theValidation );
            }
            else
            {
				// check then Local node first
				// then FFPlugin 
				outBuffer = GetRecordListLibInfo( fLocalNodeRef, inName, kDSStdRecordTypeHosts, 1, attrTypes, ParseHostEntry, NULL, NULL,
												  NULL, NULL, &theValidation );

				if( NULL == outBuffer )
				{
					outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, inName, kDSStdRecordTypeHosts, 1, attrTypes, ParseHostEntry, NULL, NULL,
													  NULL, NULL, &theValidation );
				}
            }
            
            // now lookup in DNS
            if( NULL == outBuffer )
            {
                sDNSLookup  *pAnswer        = inLookup;
                char        *actualName     = NULL;
                char        *domainMatch    = NULL; // this holds the domain that was used from search policy
                uint16_t    aliasCount      = 0;
                bool        ipv4Found       = false;
                bool        ipv6Found       = false;
                char        *aliases[1024];
                
                // reset TTL to 0
                ttl = 0;
                
                if( pAnswer == NULL )
                {
                    pAnswer = new sDNSLookup;
                }                
                
                if( 0 != ipv4 )
                {
                    pAnswer->AddQuery( fClassIN, fTypeA, inName );
                }
                
                if( 0 != ipv6 )
                {
                    pAnswer->AddQuery( fClassIN, fTypeAAAA, inName );
                }
                
                InitiateDNSQuery( pAnswer, inParallelQuery );

                outBuffer = kvbuf_new();
                kvbuf_add_dict( outBuffer );
                
                // if we did a query that could potential result in conflicting answers, let's coalesce them here
                // we only care about the difference if we aren't a qualified question
                int nameLen     = strlen( inName );
                
                if( pAnswer->fTotal > 1 && inName[nameLen-1] != '.' )
                {
                    int     iMatch[pAnswer->fTotal];
                    int     iLowest                 = 255;
                    int     iDomainIndex            = 0;
                    char    *pDomain                = NULL;

                    // we are going to filter out any response that came from the search domains vs. fully qualified
                    // i.e., www vs. www.apple.com
                    for( int ii = 0; ii < pAnswer->fTotal; ii++ )
                    {
                        iMatch[ii] = 255;
                    }
                            
                    if( _res.options & RES_INIT == 0 )
                    {
                        res_init();
                    }

                    // now check each of the domains
                    while( (pDomain = _res.dnsrch[iDomainIndex]) != NULL )
                    {
                        // name + '.' + suffix + NULL
                        int     newLen      = nameLen + 1 + strlen(pDomain) + 1;
                        char    *pTemp      = (char *) calloc( newLen, sizeof(char) );
                        
                        snprintf( pTemp, newLen, "%s.%s", inName, pDomain );
                        DbgLog( kLogPlugin, "CCachePlugin::gethostbyname - checking name as FQDN %s", pTemp );

                        for( int zz = 0; zz < pAnswer->fTotal; zz++ )
                        {
                            dns_reply_t *reply = pAnswer->fAnswers[zz];
                            
                            if( NULL != reply )
                            {
                                if( (reply->header != NULL) && (reply->header->ancount > 0) )
                                {
                                    int index = 0;

                                    while( index < reply->header->ancount && iMatch[zz] != 255 )
                                    {
                                        if( NULL != reply->answer[index] )
                                        {
                                            uint16_t iType = reply->answer[index]->dnstype;
                                            
                                            if( iType == fTypeA || iType == fTypeAAAA || iType == fTypeCNAME )
                                            {
                                                if( strcmp(reply->answer[index]->name, pTemp) == 0 )
                                                {
                                                    iMatch[zz] = iDomainIndex;
                                                    if( iDomainIndex < iLowest )
                                                        iLowest = iDomainIndex;
                                                }
                                            }
                                        }
                                        index++;
                                    }
                                }
                            }
                        }
                        iDomainIndex++;
                        DSFree( pTemp );
                    }
                    
                    if( iDomainIndex == 0 )
                    {
                        DbgLog( kLogPlugin, "CCachePlugin::gethostbyname - search domains = %d", iDomainIndex );
                    }
                    
                    // ok throw out any answers who's domains are not match of the lowest match
                    if( iLowest < 255 )
                    {
                        for( int zz = 0; zz < pAnswer->fTotal; zz++ )
                        {
                            if( pAnswer->fAnswers[zz] != NULL )
                            {
                                if( iMatch[zz] > iLowest )
                                {
                                    dns_reply_t *reply = pAnswer->fAnswers[zz];
                                    
                                    if( reply->header->ancount > 0 && reply->answer[0] != NULL )
                                    {
                                        DbgLog( kLogPlugin, "CCachePlugin::gethostbyname - disposing of answer %s", reply->answer[0]->name );
                                    }
                                    dns_free_reply( pAnswer->fAnswers[zz] );
                                    pAnswer->fAnswers[zz] = NULL;
                                }
                                else if( domainMatch == NULL )
                                {
                                    domainMatch = strdup( _res.dnsrch[iDomainIndex] );
                                    DbgLog( kLogPlugin, "CCachePlugin::gethostbyname - answer is in search domain %s", _res.dnsrch[iDomainIndex] );
                                }
                            }
                        }
                    }
                }
                else
                {
                    DbgLog( kLogDebug, "CCachePlugin::gethostbyname - only one query so skipping domain check" );
                }

                for( int zz = 0; zz < pAnswer->fTotal; zz++ )
                {
                    dns_reply_t *reply = pAnswer->fAnswers[zz];
                    
                    if( NULL != reply )
                    {
                        if( (reply->header != NULL) && (reply->header->ancount > 0) )
                        {
                            int index = 0;
                            
                            if( fTypeA == pAnswer->fQueryTypes[zz] )
                            {
                                kvbuf_add_key( outBuffer, "h_ipv4_addr_list" );
                                ipv4Found = true;
                            }
                            else if( fTypeAAAA == pAnswer->fQueryTypes[zz] )
                            {
                                kvbuf_add_key( outBuffer, "h_ipv6_addr_list" );
                                ipv6Found = true;
                            }
                            else
                            {
                                // we don't parse any other type of query since this might be a combined query
                                continue;
                            }
							
                            int iface = 0;
                            if (reply->server->sa_family == AF_INET)
                            {
                                memcpy( &iface, (((struct sockaddr_in *)(reply->server))->sin_zero), 4 );
                            }
                            else if (reply->server->sa_family == AF_INET6)
                            {
                                iface = ((struct sockaddr_in6 *)(reply->server))->sin6_scope_id;
                            }
                            
                            // the answer is not a null terminated list, it has a count...
                            while( index < reply->header->ancount )
                            {
                                if( NULL != reply->answer[index] )
                                {
                                    // we use the lowest TTL so we refresh this if needed
                                    int tempTTL = reply->answer[index]->ttl;
                                    
                                    if( reply->answer[index]->dnstype == fTypeA )
                                    {
                                        dns_address_record_t	*address = reply->answer[index]->data.A;
                                        
                                        if( NULL == actualName )
                                        {
                                            actualName = reply->answer[index]->name;
                                        }
                                        
                                        if( NULL != address )
                                        {
                                            kvbuf_add_val( outBuffer, inet_ntoa(address->addr) );
                                        }
										
                                        // Only care about ttl for IPv4 & IPv6
                                        if( 0 >= ttl || tempTTL < (int) ttl )
										    ttl = tempTTL;
                                   }
                                    else if( reply->answer[index]->dnstype == fTypeAAAA )
                                    {
                                        dns_in6_address_record_t    *address = reply->answer[index]->data.AAAA;
                                        char                        buffer[ INET6_ADDRSTRLEN ];
                                        
                                        if( NULL == actualName )
                                        {
                                            actualName = reply->answer[index]->name;
                                        }                                    
                                        
                                        if( NULL != address )
                                        {
                                            char    tempBuffer[INET6_ADDRSTRLEN + 16];
                                            char    ifname[IF_NAMESIZE];
                                            
                                            inet_ntop( AF_INET6, &(address->addr), buffer, INET6_ADDRSTRLEN );
                                            
                                            if( if_indextoname(iface, ifname) != NULL )
                                            {
                                                snprintf( tempBuffer, sizeof(tempBuffer), "%s%%%s", buffer, ifname );
                                                kvbuf_add_val( outBuffer, tempBuffer );
                                            }
                                            else
                                            {
                                                kvbuf_add_val( outBuffer, buffer );
                                            }
                                        }

                                        // Only care about ttl for IPv4 & IPv6
                                        if( 0 >= ttl || tempTTL < (int) ttl )
                                            ttl = tempTTL;
                                    }                                    
                                    else if( reply->answer[index]->dnstype == fTypeCNAME )
                                    {
                                        char    *newAlias = reply->answer[index]->name;
                                        
                                        for( int ii = 0; ii < aliasCount; ii++ )
                                        {
                                            if( strcmp(aliases[ii], newAlias) == 0 )
                                            {
                                                newAlias = NULL;
                                                break;
                                            }
                                        }
                                        
                                        if( NULL != newAlias )
                                        {
                                            aliases[aliasCount] = newAlias;
                                            aliasCount++;
                                        }
                                    }
                                }
                                
                                index++;
                            }
                        }
                    }
                    else if ( pAnswer->fMinimumTTL[zz] > 0 && (ttl == 0 || pAnswer->fMinimumTTL[zz] < (int) ttl) )
                    {
                        ttl = pAnswer->fMinimumTTL[zz];
                    }
                }
                
                if( NULL != outBuffer )
                {
                    if( actualName != NULL )
                    {
                        kvbuf_add_key( outBuffer, "h_name" );
                        kvbuf_add_val( outBuffer, actualName );
                        
                        kvbuf_add_key( outBuffer, "ipv4" );
                        kvbuf_add_val( outBuffer, inIPv4 );

                        kvbuf_add_key( outBuffer, "ipv6" );
                        kvbuf_add_val( outBuffer, inIPv6 );
                        
                        if( domainMatch != NULL )
                        {
                            kvbuf_add_key( outBuffer, "searchDomainUsed" );
                            kvbuf_add_val( outBuffer, domainMatch );
                        }

                        // query might be unqualified, so we need to compare
                        char *pAliasName = (actualName != NULL && strcmp(inName, actualName) == 0 ? NULL : inName);
                        
                        if( aliasCount > 0 )
                        {
                            kvbuf_add_key( outBuffer, "h_aliases" );
                            
                            for( int ii = 0; ii < aliasCount; ii++ )
                            {
                                kvbuf_add_val( outBuffer, aliases[ii] );
                                
                                // if the name wasn't the actual, lets see if it is already in the alias list
                                if( NULL != pAliasName && strcmp(aliases[ii], pAliasName) == 0 )
                                {
                                    pAliasName = NULL;
                                }
                            }
                            
                            if( NULL != pAliasName )
                            {
                                kvbuf_add_val( outBuffer, pAliasName );
                            }
                        }
                        else if( NULL != pAliasName )
                        {
                            kvbuf_add_key( outBuffer, "h_aliases" );
                            kvbuf_add_val( outBuffer, pAliasName );
                        }
                    }
                    else
                    {
                        kvbuf_free( outBuffer );
                        outBuffer = NULL;
                    }
                }
                
                // if this was a internally generated query, we can clean up after 
                // we check the outstanding requests, if it is 0, we delete it, otherwise just
                // set the delete flag and unlock
                if( inLookup == NULL )
                {
					DSDelete( pAnswer );
                }
            }
            else
            {
                kvbuf_add_key( outBuffer, "ipv4" );
                kvbuf_add_val( outBuffer, inIPv4 );
                
                kvbuf_add_key( outBuffer, "ipv6" );
                kvbuf_add_val( outBuffer, inIPv6 );                                    
            }
            
            if( outBuffer != NULL )
            {
                kvbuf_t *copy = kvbuf_init( outBuffer->databuf, outBuffer->datalen );

                if( ipv4 == 1 && ipv6 == 1 )
                {
                    char    *keylist1[] = { "h_name", "ipv4", NULL };
                    char    *keylist2[] = { "h_name", "ipv6", NULL };
                    char    *keylist3[] = { "h_name", "ipv4", "ipv6", NULL };
                    char    *keylist4[] = { "h_aliases", "ipv4", NULL };
                    char    *keylist5[] = { "h_aliases", "ipv6", NULL };
                    char    *keylist6[] = { "h_aliases", "ipv4", "ipv6", NULL };
                    
                    // even though we replace, doesn't mean the h_name matches the name looked up it could be an alias, so we need to
                    // manually remove
					fLibinfoCache->fCacheLock.WaitLock();
                    RemoveEntryWithMultiKey( fLibinfoCache, "h_name", inName, "ipv4", "1", "ipv6", "1", NULL );
                    RemoveEntryWithMultiKey( fLibinfoCache, "h_name", inName, "ipv4", "1", NULL );
                    RemoveEntryWithMultiKey( fLibinfoCache, "h_name", inName, "ipv6", "1", NULL );
                                            
                    AddEntryToCacheWithKeylists( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_HOST | CACHE_ENTRY_TYPE_REPLACE, ttl,
                                                 keylist1, keylist2, keylist3, keylist4, keylist5, keylist6, NULL );
					fLibinfoCache->fCacheLock.SignalLock();
                }
                else if( ipv4 == 1 )
                {
                    char    *keylist1[] = { "h_name", "ipv4", NULL };
                    char    *keylist2[] = { "h_aliases", "ipv4", NULL };
                    
                    // even though we replace, doesn't mean the h_name matches the name looked up it could be an alias, so we need to
                    // manually remove
					fLibinfoCache->fCacheLock.WaitLock();
                    RemoveEntryWithMultiKey( fLibinfoCache, "h_name", inName, "ipv4", "1", NULL );

                    AddEntryToCacheWithKeylists( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_HOST | CACHE_ENTRY_TYPE_REPLACE, ttl, 
                                                 keylist1, keylist2, NULL );
					fLibinfoCache->fCacheLock.SignalLock();
                }
                else
                {
                    char    *keylist1[] = { "h_name", "ipv6", NULL };
                    char    *keylist2[] = { "h_aliases", "ipv6", NULL };

                    // even though we replace, doesn't mean the h_name matches the name looked up it could be an alias, so we need to
                    // manually remove
					fLibinfoCache->fCacheLock.WaitLock();
                    RemoveEntryWithMultiKey( fLibinfoCache, "h_name", inName, "ipv6", "1", NULL );

                    AddEntryToCacheWithKeylists( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_HOST | CACHE_ENTRY_TYPE_REPLACE, ttl, 
                                                 keylist1, keylist2, NULL );
					fLibinfoCache->fCacheLock.SignalLock();
                }
                
                if( outTTL != NULL )
                    (*outTTL) = ttl;
            }
            else if ( localOnlyPID == false )
            {
				if ( ttl > 0 )
				{
					// before we put a negative entry, let's look 1 more time while holding the lock before forcing a negative entry
					fLibinfoCache->fCacheLock.WaitLock();
					
					if ( ipv4 != 0 )
					{
						outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_name", inName, "ipv4", inIPv4, (ipv6 ? "ipv6" : NULL), inIPv6, NULL );
						if ( outBuffer == NULL )
							outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_aliases", inName, "ipv4", inIPv4, (ipv6 ? "ipv6" : NULL), inIPv6, NULL );
					}
					else
					{
						outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_name", inName, "ipv6", inIPv6, NULL );
						if ( outBuffer == NULL )
							outBuffer = FetchFromCache( fLibinfoCache, &ttl, "h_aliases", inName, "ipv6", inIPv6, NULL );
					}

					// if we still don't have an entry while holding the cache, it's safe to put a negative entry
					if ( outBuffer == NULL )
					{
						// if we got a TTL, then we'll do a NODATA equivalent
						outBuffer = kvbuf_new();
						
						// we negative cache based on the determined TTL which is based on RFC2308 behavior
						// when we have no TTL, we have no authority, so we don't cache
						if ( ipv4 == 1 && ipv6 == 1 )
						{
							AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, ttl, "h_name", inName, 
														"ipv4", "1", "ipv6", "1", NULL );
						}
						else if ( ipv4 == 1 )
						{
							AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, ttl, "h_name", inName, 
														"ipv4", "1", NULL );
						}
						else
						{
							AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, ttl, "h_name", inName, 
														"ipv6", "1", NULL );
						}
					}
					
					fLibinfoCache->fCacheLock.SignalLock();
				}
				else
				{
					DbgLog( kLogDebug, "CCachePlugin::gethostbyname - query for %s didn't return A/AAAA records - no cache entries created", inName );
				}
			}
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgethostbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgethostbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::gethostbyname - Cache hit for %s", inName );
        
        // if we hit our cache, we still have more responses to get if someone handed us an inLookup
        if( inLookup != NULL )
        {
            DbgLog( kLogDebug, "CCachePlugin::gethostbyname - have a query to initiate" );
            InitiateDNSQuery( inLookup, inParallelQuery );
        }
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgethostbyaddr( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    int                 ttl             = 0;
    uint32_t            valCount        = 0;
    char                *address        = NULL;
    uint32_t            family          = 0;
    sCacheValidation    *theValidation  = NULL;
    char                *key;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "address") == 0 )
        {
            address = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "family") == 0 )
        {
            char *pTemp = kvbuf_next_val( inBuffer );
            if( NULL != pTemp )
            {
                family = strtol( pTemp, NULL, 10 );
            }
        }
    }
    
    if( family == 0 || address == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, (family == AF_INET ? "h_ipv4_addr_list" : "h_ipv6_addr_list"), address, NULL );
    
    if( NULL == outBuffer )
    {
        const char *addressAttribute = (AF_INET == family ? kDSNAttrIPAddress : kDSNAttrIPv6Address);
        
        // TODO define kDSNAttrIPv6Address
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            addressAttribute,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );

            // TODO - need a way to specify an order of lookups, i.e., nsswitch
            // if we aren't dispatching to ourself, dispatch to the FF Plugin
            if( NULL == outBuffer )
            {
                if( localOnlyPID == false )
                {
                    outBuffer = ValueSearchLibInfo( fSearchNodeRef, addressAttribute, address, kDSStdRecordTypeHosts, 1, attrTypes, ParseHostEntry,
                                                    NULL, NULL, &theValidation );
                }
                else
                {
                    // check then Local node first
                    // then FFPlugin 
					outBuffer = ValueSearchLibInfo( fLocalNodeRef, addressAttribute, address, kDSStdRecordTypeHosts, 1, attrTypes, 
													ParseHostEntry, NULL, NULL, &theValidation );
					if( NULL == outBuffer )
					{
						outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, addressAttribute, address, kDSStdRecordTypeHosts, 1, attrTypes,
														ParseHostEntry, NULL, NULL, &theValidation );
                    }
                }
                
                // now lookup in DNS if no answer in local files
                if( NULL == outBuffer )
                {
                    dns_handle_t	dns			= dns_open( NULL );
                    dns_reply_t    *reply       = NULL;
                    
                    if( dns )
                    {
                        unsigned char    buffer[ 16 ];
                        
                        int slot = AddToDNSThreads();

                        // need to reverse address
                        if( inet_pton(family, address, buffer) == 1 )
                        {
                            char    query[256];
                            
                            if( AF_INET == family )
                            {
                                snprintf( query, sizeof(query), "%d.%d.%d.%d.in-addr.arpa", 
                                          (int) buffer[3], (int) buffer[2], (int) buffer[1], (int) buffer[0] );
                            }
                            else if( AF_INET6 == family )
                            {
                                snprintf( query, sizeof(query), 
                                          "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
                                          buffer[15]&0xf, buffer[15] >> 4, buffer[14]&0xf, buffer[14] >> 4,
                                          buffer[13]&0xf, buffer[13] >> 4, buffer[12]&0xf, buffer[12] >> 4,
                                          buffer[11]&0xf, buffer[11] >> 4, buffer[10]&0xf, buffer[10] >> 4,
                                          buffer[9]&0xf, buffer[9] >> 4, buffer[8]&0xf, buffer[8] >> 4,
                                          buffer[7]&0xf, buffer[7] >> 4, buffer[6]&0xf, buffer[6] >> 4,
                                          buffer[5]&0xf, buffer[5] >> 4, buffer[4]&0xf, buffer[4] >> 4,
                                          buffer[3]&0xf, buffer[3] >> 4, buffer[2]&0xf, buffer[2] >> 4,
                                          buffer[1]&0xf, buffer[1] >> 4, buffer[0]&0xf, buffer[0] >> 4 );
                            }
                            
                            reply = idna_dns_lookup( dns, query, fClassIN, fTypePTR, &ttl );
                        }
                        
                        dns_free( dns );
                        
                        RemoveFromDNSThreads( slot );
                    }
                    
                    if( NULL != reply )
                    {
                        if ( (reply->header != NULL) && (DNS_STATUS_OK == reply->status) )
                        {
							// There could be CNAME records too, so loop until a PTR record is found.
							for ( int ii = 0;  ii < reply->header->ancount;  ii++ )
							{
								if ( (NULL != reply->answer[ii]) && (ns_t_ptr == reply->answer[ii]->dnstype) )
								{
									dns_domain_name_record_t *PTR = reply->answer[ii]->data.PTR;
									
									int tempTTL = reply->answer[ii]->ttl;
									if ( ttl == 0 || tempTTL < ttl )
										ttl = tempTTL;
									
									if( NULL != PTR && PTR->name != NULL )
									{
										outBuffer = kvbuf_new();
										kvbuf_add_dict( outBuffer );

										kvbuf_add_key( outBuffer, "h_name" );
										kvbuf_add_val( outBuffer, PTR->name );
										kvbuf_add_key( outBuffer, (family == AF_INET ? "h_ipv4_addr_list" : "h_ipv6_addr_list") );
										kvbuf_add_val( outBuffer, address );
										
										// found one - no need to continue;
										break;
									}
								}
							}
                        }
                        else if ( reply->authority[0] != NULL )
                        {
                            // if we got an authority, then we will use the minimum time
                            int tempTTL = reply->authority[0]->data.SOA->minimum;
                            if ( ttl == 0 || tempTTL < ttl )
                                ttl = tempTTL;
                        }
                        
                        dns_free_reply( reply );
                    }
                }                    
                
                if( outBuffer != NULL )
                {
                    // add this entry to the cache
                    uint32_t dcount = kvbuf_reset( outBuffer );
                    
                    if( dcount == 1 )
                    {
                        kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                        
                        AddEntryToCacheWithMultiKey( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_HOST, ttl, 
                                                     (family == AF_INET ? "h_ipv4_addr_list" : "h_ipv6_addr_list"), address, NULL );
                    }
                    else
                    {
                        kvbuf_free( outBuffer );
                        outBuffer = NULL;
                    }
                }
            }
            
            // if we have a ttl, we return an empty buffer
            if ( outBuffer == NULL && ttl > 0 && localOnlyPID == false )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, theValidation, NULL, CACHE_ENTRY_TYPE_HOST, ttl, 
                                             (family == AF_INET ? "h_ipv4_addr_list" : "h_ipv6_addr_list"), address,  NULL );
                
                // we need to return an empty buffer to make libinfo happy
                outBuffer = kvbuf_new();
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgethostbyaddr] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgethostbyaddr] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::gethostbyaddr - Cache hit for %s", address );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
        
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgethostent( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    
    if( outBuffer == NULL )
    {
        tDataListPtr   attrTypes = dsBuildListFromStrings( fDirRef,
                                                            kDSNAttrMetaNodeLocation,
                                                            kDSNAttrRecordName,
                                                            kDSNAttrIPAddress,
                                                            kDSNAttrIPv6Address,
                                                            NULL );
        
        if( attrTypes != NULL )
        {
            // TODO - need a way to specify an order of lookups, i.e., nsswitch
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = GetRecordListLibInfo( fSearchNodeRef, kDSRecordsAll, kDSStdRecordTypeHosts, 0, attrTypes, ParseHostEntry, NULL, NULL,
                                                  NULL, NULL );
            }
            else
            {
				// check then Local node first
				// then FFPlugin 
                outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, kDSRecordsAll, kDSStdRecordTypeHosts, 0, attrTypes, ParseHostEntry, outBuffer, NULL,
                                                  NULL, NULL );

                outBuffer = GetRecordListLibInfo( fLocalNodeRef, kDSRecordsAll, kDSStdRecordTypeHosts, 0, attrTypes, ParseHostEntry, outBuffer, NULL,
                                                  NULL, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgethostent] += 1;
        fStatsLock.SignalLock();
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetnameinfo( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    kvbuf_t             *tempBuffer     = NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    int                 ttl             = 0;
    uint32_t            valCount        = 0;
    char                *reqAddress     = NULL;
    uint32_t            family          = 0;
    char                *pFamily        = NULL;
    uint16_t            port            = 0;
    char                *pPort          = "0";
    uint32_t            flags           = 0;
    char                pCacheFlags[16] = { 0, };
    char                *key;
    char                canonicalAddr[INET6_ADDRSTRLEN];
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "address") == 0 )
        {
            reqAddress = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "family") == 0 )
        {
            pFamily = kvbuf_next_val( inBuffer );
            if( NULL != pFamily )
            {
                family = strtol( pFamily, NULL, 10 );
            }
        }
        else if( strcmp(key, "port") == 0 )
        {
            pPort = kvbuf_next_val( inBuffer );
            if( NULL != pPort )
            {
                port = strtol( pPort, NULL, 10 );
            }
            else
            {
                pPort = "0";
            }
        }
        else if( strcmp(key, "flags") == 0 )
        {
            char *pTemp = kvbuf_next_val( inBuffer );
            if( NULL != pTemp )
            {
                flags = strtoul( pTemp, NULL, 10 );
            }
        }
    }
    
    // create the new flags
    uint32_t    workingFlags = flags ^ (flags & NI_NOFQDN); // some flags are not relevant when looking in the cache
    
    snprintf( pCacheFlags, sizeof(pCacheFlags), "%u", workingFlags );
    
    if ( reqAddress != NULL )
    {
        // canonicalize the address so we find it in the cache and DS correctly
        uint32_t    tempFamily  = AF_UNSPEC;
        char        canonicalBuffer[16];		// IPv6 is 128 bit so max 16 bytes
        
        if( inet_pton(AF_INET6, reqAddress, canonicalBuffer) == 1 )
            tempFamily = AF_INET6;
        else if( inet_pton(AF_INET, reqAddress, canonicalBuffer) == 1 )
            tempFamily = AF_INET;
        
        if ( tempFamily == AF_UNSPEC )
            return NULL;
        
        // We use inet_pton to convert to a canonical form
        if ( inet_ntop(tempFamily, canonicalBuffer, canonicalAddr, sizeof(canonicalAddr)) == NULL )
            return NULL;
    }
    
    tempBuffer = FetchFromCache( fLibinfoCache, NULL, "gni_address", canonicalAddr, "gni_family", pFamily, "gni_port", pPort, "flags", pCacheFlags, NULL );

    if( NULL == tempBuffer )
    {
        const char *addressAttribute = (AF_INET == family ? kDSNAttrIPAddress : kDSNAttrIPv6Address);
        
        // TODO define kDSNAttrIPv6Address
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            addressAttribute,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );

            // TODO - need a way to specify an order of lookups, i.e., nsswitch
            // if we aren't dispatching to ourself, dispatch to the FF Plugin
           if( NULL == tempBuffer )
            {
                if( NULL != reqAddress && (0 == (flags & NI_NUMERICHOST) || NI_NAMEREQD == (flags & NI_NAMEREQD)) &&
                    (AF_INET == family || AF_INET6 == family) )
                {
                    kvbuf_t *searchBuffer = NULL;
                    
                    if( localOnlyPID == false )
                    {
                        searchBuffer = ValueSearchLibInfo( fSearchNodeRef, addressAttribute, canonicalAddr, kDSStdRecordTypeHosts, 1, attrTypes, 
                                                         ParseHostEntry, NULL, NULL, NULL );
                    }
                    else
                    {
						// check then Local node first
						// then FFPlugin 
                        searchBuffer = ValueSearchLibInfo( fLocalNodeRef, addressAttribute, canonicalAddr, kDSStdRecordTypeHosts, 1, attrTypes, 
                                                                    ParseHostEntry, NULL, NULL, NULL );
                        if( NULL == searchBuffer )
                        {
                            searchBuffer = ValueSearchLibInfo( fFlatFileNodeRef, addressAttribute, canonicalAddr, kDSStdRecordTypeHosts, 1, attrTypes, 
															   ParseHostEntry, NULL, NULL, NULL );
                        }
                    }
                        
                        if( kvbuf_reset(searchBuffer) > 0 )
                        {
                            tempBuffer = kvbuf_new();
                            kvbuf_add_dict( tempBuffer );
                            
                            kvbuf_next_dict( searchBuffer );
                            
                            while( (key = kvbuf_next_key(searchBuffer, &valCount)) ) 
                            {
                                if( strcmp(key, "h_name") == 0 )
                                {
                                    char *pHostName = kvbuf_next_val( searchBuffer );
                                    if( NULL != pHostName )
                                    {
                                        kvbuf_add_key( tempBuffer, "gni_name" );
                                        kvbuf_add_val( tempBuffer, pHostName );
                                    }
                                    break;
                                }
                            }
                        }
                        
                        kvbuf_free( searchBuffer );
                    
                    // now lookup in DNS if no answer in local files
                   if( NULL == tempBuffer )
                    {
                        dns_handle_t	dns			= dns_open( NULL );
                        dns_reply_t    *reply       = NULL;
                        
                        if( dns )
                        {
                            unsigned char    buffer[ 16 ];
                            
                            int slot = AddToDNSThreads();

                            // need to reverse address
                            if( inet_pton(family, canonicalAddr, buffer) == 1 )
                            {
                                char    query[256];
                                
                                if( AF_INET == family )
                                {
                                    snprintf( query, sizeof(query), "%d.%d.%d.%d.in-addr.arpa", 
                                              (int) buffer[3], (int) buffer[2], (int) buffer[1], (int) buffer[0] );
                                }
                                else if( AF_INET6 == family )
                                {
                                    snprintf( query, sizeof(query), 
                                              "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
                                              buffer[15]&0xf, buffer[15] >> 4, buffer[14]&0xf, buffer[14] >> 4,
                                              buffer[13]&0xf, buffer[13] >> 4, buffer[12]&0xf, buffer[12] >> 4,
                                              buffer[11]&0xf, buffer[11] >> 4, buffer[10]&0xf, buffer[10] >> 4,
                                              buffer[9]&0xf, buffer[9] >> 4, buffer[8]&0xf, buffer[8] >> 4,
                                              buffer[7]&0xf, buffer[7] >> 4, buffer[6]&0xf, buffer[6] >> 4,
                                              buffer[5]&0xf, buffer[5] >> 4, buffer[4]&0xf, buffer[4] >> 4,
                                              buffer[3]&0xf, buffer[3] >> 4, buffer[2]&0xf, buffer[2] >> 4,
                                              buffer[1]&0xf, buffer[1] >> 4, buffer[0]&0xf, buffer[0] >> 4 );
                                }

                                reply = idna_dns_lookup( dns, query, fClassIN, fTypePTR, &ttl );
                            }
                            
                            dns_free( dns );
                            
                            RemoveFromDNSThreads( slot );
                        }
                        
                        if ( (NULL != reply) && (DNS_STATUS_OK == reply->status) && (reply->header != NULL) )
                        {
							// There could be CNAME records too, so loop until a PTR record is found.
                            for ( int ii = 0;  ii < reply->header->ancount;  ii++ )
                            {
                                if ( (NULL != reply->answer[ii]) && (ns_t_ptr == reply->answer[ii]->dnstype) )
                                {
                                    dns_domain_name_record_t *PTR = reply->answer[ii]->data.PTR;
                                    
                                    int tempTTL = reply->answer[ii]->ttl;
                                    if ( ttl == 0 || tempTTL < ttl )
                                        ttl = tempTTL;
                                    
                                    if ( NULL != PTR && PTR->name != NULL )
                                    {
                                        tempBuffer = kvbuf_new();
                                        kvbuf_add_dict( tempBuffer );

                                        kvbuf_add_key( tempBuffer, "gni_name" );
                                        kvbuf_add_val( tempBuffer, PTR->name );
										
										// found one - no need to continue;
										break;
                                    }
                                }
                            }
                            
                            dns_free_reply( reply );
                        }
                    }                    
                }
                
                // let's go query with our getservbyport call if they don't want a numeric service
                if( NULL != pPort && (0 == (flags & NI_NAMEREQD) || tempBuffer != NULL) )
                {
                    if( 0 == (flags & NI_NUMERICSERV) )
                    {
                        kvbuf_t *request = kvbuf_new();
                        
                        kvbuf_add_dict( request );
                        kvbuf_add_key( request, "port" );
                        kvbuf_add_val( request, pPort );
                        
                        if( (flags & NI_DGRAM) == NI_DGRAM )
                        {
                            kvbuf_add_key( request, "proto" );
                            kvbuf_add_val( request, "udp" );
                        }
                        else
                        {
                            kvbuf_add_key( request, "proto" );
                            kvbuf_add_val( request, "tcp" );
                        }
                        
                        kvbuf_t *answer = DSgetservbyport( request, inPID );
                        if( kvbuf_reset(answer) > 0 )
                        {
                            if( NULL == tempBuffer )
                            {
                                tempBuffer = kvbuf_new();
                                kvbuf_add_dict( tempBuffer );
                            }
                            
                            kvbuf_next_dict( answer );
                            
                            while( (key = kvbuf_next_key(answer, &valCount)) ) 
                            {
                                if( strcmp(key, "s_name") == 0 )
                                {
                                    char *pService = kvbuf_next_val( answer );
                                    if( NULL != pService )
                                    {
                                        kvbuf_add_key( tempBuffer, "gni_service" );
                                        kvbuf_add_val( tempBuffer, pService );
                                    }
                                }
                            }
                        }
                        else
                        {
                            kvbuf_add_key( tempBuffer, "gni_service" );
                            kvbuf_add_val( tempBuffer, pPort );
                        }
                        
                        kvbuf_free( answer );
                        kvbuf_free( request );
                    }
                }
                
                if( tempBuffer != NULL )
                {
                    // add this entry to the cache
                    uint32_t dcount = kvbuf_reset( tempBuffer );
                    
                    if( dcount == 1 )
                    {
                        kvbuf_t *copy       = kvbuf_init( tempBuffer->databuf, tempBuffer->datalen );
                        
                        AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, copy, CACHE_ENTRY_TYPE_HOST, ttl, "gni_address", canonicalAddr, 
													 "gni_family", pFamily, "gni_port", pPort, "flags", pCacheFlags, NULL );
                        
                    }
                    else
                    {
                        kvbuf_free( tempBuffer );
                        tempBuffer = NULL;
                    }
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( tempBuffer == NULL && localOnlyPID == false && ttl > 0 )
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, ttl, "gni_address", canonicalAddr, 
											 "gni_family", pFamily, "gni_port", pPort, "flags", pCacheFlags, NULL );
                
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetnameinfo] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetnameinfo] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getnameinfo - Cache hit for %s:%s %s:%s", (reqAddress ? canonicalAddr : ""), (pFamily ? pFamily : ""), 
                 (pPort ? pPort : ""), ((flags & NI_DGRAM) == NI_DGRAM ? "udp" : "tcp") );
    }
    
    // we need to re-write the response to what the caller wants now since they may want just the numeric forms, or stripped DNS names
    if( NULL != tempBuffer )
    {
        bool        bNameFound  = false;

        kvbuf_reset( tempBuffer );
        kvbuf_next_dict( tempBuffer );
        
        outBuffer = kvbuf_new();
        kvbuf_add_dict( outBuffer );
        
        while( (key = kvbuf_next_key(tempBuffer, &valCount)) ) 
        {
            if( strcmp(key, "gni_name") == 0 )
            {
                bNameFound = true;

                if( 0 == (flags & NI_NUMERICHOST) )
                {
                    char *pTemp = kvbuf_next_val( tempBuffer );

                    if( pTemp != NULL )
                    {
                        kvbuf_add_key( outBuffer, "gni_name" );
                        
                        if ( 0 == (flags & NI_NOFQDN) ) {
                            kvbuf_add_val( outBuffer, pTemp );
                        }
                        else {
                            char *dot = strchr( pTemp, '.' );
                            if ( dot != NULL )
                                *dot = '\0';

                            kvbuf_add_val( outBuffer, pTemp );
                        }
                    }
                }
            }
            else if( 0 == (flags & NI_NUMERICSERV) && strcmp(key, "gni_service") == 0 )
            {
                char* pTemp = kvbuf_next_val(tempBuffer);
                if( NULL != pTemp )
                {
                    kvbuf_add_key( outBuffer, "gni_service" );
                    kvbuf_add_val( outBuffer,  pTemp);
                }
            }
        }
        
        if( (NULL != reqAddress) && ((false == bNameFound && 0 == (flags & NI_NAMEREQD)) || (NI_NUMERICHOST == (flags & NI_NUMERICHOST))) )
        {
            kvbuf_add_key( outBuffer, "gni_name" );
            kvbuf_add_val( outBuffer, reqAddress );
        }
        
        if( NI_NUMERICSERV == (flags & NI_NUMERICSERV) )
        {
            kvbuf_add_key( outBuffer, "gni_service" );
            kvbuf_add_val( outBuffer, pPort );
        }
        
        kvbuf_free( tempBuffer );
        tempBuffer = NULL;
    }

    // if we have no response and the name is not required, we can just return the values as is
    if( NULL == outBuffer && 0 == (flags & NI_NAMEREQD) )
    {
        outBuffer = kvbuf_new();
        kvbuf_add_dict( outBuffer );
        if( NULL != reqAddress )
        {
            kvbuf_add_key( outBuffer, "gni_name" );
            kvbuf_add_val( outBuffer, reqAddress );
        }
        kvbuf_add_key( outBuffer, "gni_service" );
        kvbuf_add_val( outBuffer, pPort );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetmacbyname( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "name") != 0 || valCount == 0 )
        return NULL;
    
    char *name = kvbuf_next_val( inBuffer );
    if( name == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "name", name, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrENetAddress,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char	*keys[] = { "mac", "name", NULL };
            
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = GetRecordListLibInfo( fSearchNodeRef, name, kDSStdRecordTypeEthernets, 1, attrTypes, ParseEthernetEntry, NULL, NULL,
                                                  fLibinfoCache, keys );
            }
            else
            {
                outBuffer = GetRecordListLibInfo( fFlatFileNodeRef, name, kDSStdRecordTypeEthernets, 1, attrTypes, ParseEthernetEntry, NULL, NULL,
                                                  fLibinfoCache, keys );
            }
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount != 1 )
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, kNegativeCacheTime, "name", name, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetmacbyname] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetmacbyname] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getmacbyname - Cache hit for %s", name );
    }
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgethostbymac( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "mac") != 0 || valCount == 0 )
        return NULL;
    
    char *mac = kvbuf_next_val( inBuffer );
    if( mac == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "mac", mac, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrENetAddress,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrENetAddress, mac, kDSStdRecordTypeEthernets, 1, attrTypes, 
                                                ParseEthernetEntry, NULL, NULL, NULL );
            }
            else
            {
                outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrENetAddress, mac, kDSStdRecordTypeEthernets, 1, attrTypes,
                                                ParseEthernetEntry, NULL, NULL, NULL );
            }
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount == 1 )
                {
                    char    *keylist[]  = { "mac", "name", NULL };
                    kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
                    AddEntryToCacheWithKeylists( fLibinfoCache, NULL, copy, CACHE_ENTRY_TYPE_HOST, 0, keylist, NULL );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_HOST, kNegativeCacheTime, "mac", mac, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgethostbymac] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgethostbymac] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::gethostbymac - Cache hit for %s", mac );
    }
    
    return( outBuffer );
}

// this routine only works for SRV and MX replies
kvbuf_t *CCachePlugin::dnsreply_to_kvbuf( dns_reply_t *inReply, const char *inIPv4, const char *inIPv6, pid_t inPID, uint32_t *outTTL )
{
    kvbuf_t *outBuffer = kvbuf_new();
    
    // if we have some answers, something to do
    if( inReply != NULL && inReply->header->ancount > 0 )
    {
        uint32_t    count   = inReply->header->ancount;
        char        buffer[16];
        char        dnsName[512];
        uint32_t    index   = 0;
        
        while( index < count )
        {
            dns_resource_record_t *reply = inReply->answer[index];
            
            char    *pTarget    = NULL;
            kvbuf_t *hostInfo   = NULL;
            
            if( outTTL != NULL && (*outTTL == 0 || reply->ttl < *outTTL) )
            {
                *outTTL = reply->ttl;
            }
            
            // these are qualified hosts, so lets ensure they have a dot to prevent unwanted delays/search domains
            if( reply->dnstype == fTypeSRV )
            {
                pTarget = reply->data.SRV->target;
            }
            else if( reply->dnstype == fTypeMX )
            {
                pTarget = reply->data.MX->name;
            }
            
            if( pTarget != NULL )
            {
                int length = strlen( pTarget );
                
                // see if there is already a trailing dot, if not add one
                if( pTarget[length-1] != '.' )
                {
                    strlcpy( dnsName, pTarget, sizeof(dnsName) );
                    strlcat( dnsName, ".", sizeof(dnsName ));
                    pTarget = dnsName;
                }
                
                hostInfo = DSgethostbyname_int( pTarget, inIPv4, inIPv6, inPID, false, NULL );
            }
            
            // this could be optimized, but if someone is calling getaddrinfo, they probably call other calls requiring the
            // same info, so first call will be most painful
            int iDictCount = kvbuf_reset( hostInfo );
            for( int ii = 0; ii < iDictCount; ii++ )
            {
                kvbuf_add_dict( outBuffer );
                
                if( reply->dnstype == fTypeSRV )
                {
                    kvbuf_add_key( outBuffer, "port" );
                    
                    snprintf( buffer, sizeof(buffer), "%u", (unsigned int) reply->data.SRV->port );
                    kvbuf_add_val( outBuffer, buffer );
                }
                
                // here we just merged the dictionaries
                uint32_t keyCount = kvbuf_next_dict( hostInfo );
                
                for( uint32_t yy = 0; yy < keyCount; yy++ )
                {
                    uint32_t    valCount;
                    
                    char *key = kvbuf_next_key( hostInfo, &valCount );
                    
                    kvbuf_add_key( outBuffer, key );
                    for( uint32_t zz = 0; zz < valCount; zz++ )
                    {
                        kvbuf_add_val( outBuffer, kvbuf_next_val(hostInfo) );
                    }
                }
            }
            
            index++;
        }
    }
    
    return outBuffer;
}

sDNSLookup *CCachePlugin::CreateAdditionalDNSQueries( sDNSLookup *inDNSLookup, char *inService, char *inProtocol, char *inName, char *inSearchDomain,
                                                      uint16_t inAdditionalInfo )
{
    char        pQuery[512];
    
    if( inDNSLookup == NULL )
    {
        inDNSLookup = new sDNSLookup;
    }

    // this is a specialized one, if service == MX and no protocol, we want an MX query
    if( fUnqualifiedSRVAllowed == true && strcmp(inService, "MX") == 0 && inProtocol == NULL  )
    {
        if( inSearchDomain != NULL )
        {
            snprintf( pQuery, sizeof(pQuery), "%s.%s.", inName, inSearchDomain );
        }
        else
        {
            strlcpy( pQuery, inName, sizeof(pQuery) );
        }

        DbgLog( kLogDebug, "CCachePlugin::CreateAdditionalDNSQueries - Adding MX Record Lookup = %s", pQuery );
        
        inDNSLookup->AddQuery( fClassIN, fTypeMX, pQuery, inAdditionalInfo );
    }
    else
    {
        if( inSearchDomain != NULL )
        {
            snprintf( pQuery, sizeof(pQuery), "_%s._%s.%s.%s.", inService, inProtocol, inName, inSearchDomain );
            inDNSLookup->AddQuery( fClassIN, fTypeSRV, pQuery, inAdditionalInfo );
            DbgLog( kLogDebug, "CCachePlugin::CreateAdditionalDNSQueries - Adding Service Record Lookup = %s", pQuery );
        }
        else if( fUnqualifiedSRVAllowed == true )
        {
            snprintf( pQuery, sizeof(pQuery), "_%s._%s.%s", inService, inProtocol, inName );
            inDNSLookup->AddQuery( fClassIN, fTypeSRV, pQuery, inAdditionalInfo );
            DbgLog( kLogDebug, "CCachePlugin::CreateAdditionalDNSQueries - Adding Service Record Lookup = %s", pQuery );
        }
    }
        
    return inDNSLookup;
}

int CCachePlugin::SortPartitionAdditional( dns_resource_record_t **inRecords, int inFirst, int inLast )
{
    int                     up      = inFirst;
    int                     down    = inLast;
    dns_resource_record_t   *temp   = NULL;
    dns_resource_record_t   *pivot  = inRecords[inFirst];
    bool                    isSRV   = (pivot->dnstype == fTypeSRV);
    
    goto partLower;
    
    do
    {
        temp = inRecords[up];
        inRecords[up] = inRecords[down];
        inRecords[down] = temp;
        
partLower:
        if( isSRV )
        {
            uint16_t pivotPriority = pivot->data.SRV->priority;
            while ( up < inLast )
            {
                uint16_t priority = inRecords[up]->data.SRV->priority;
                
                if( priority > pivotPriority )
                {
                    break;
                }
                else if( priority == pivotPriority )
                {   
                    if( inRecords[up]->data.SRV->weight < pivot->data.SRV->weight )
                        break;
                }
                up++;
            }
            
            while( down > inFirst )
            {   
                uint16_t priority = inRecords[down]->data.SRV->priority;
                
                if( priority < pivotPriority )
                {   
                    break;
                }
                else if( priority == pivotPriority )
                {   
                    if( inRecords[up]->data.SRV->weight > pivot->data.SRV->weight )
                        break;
                }
                down--;
            }
        }
        else
        {
            uint16_t pivotPreference = pivot->data.MX->preference;
            while (inRecords[up]->data.MX->preference >= pivotPreference && up < inLast)
            {
                up++;
            }
            
            while (inRecords[down]->data.MX->preference < pivotPreference  && down > inFirst )
            {
                down--;
            }
        }
    } while( down > up );
    
    inRecords[inFirst] = inRecords[down];
    inRecords[down] = pivot;
    return down;
}

void CCachePlugin::QuickSortAdditional( dns_resource_record_t **inRecords, int inFirst, int inLast )
{
    int pivotIndex = 0;
    
    if( inFirst < inLast )
    {
        pivotIndex = SortPartitionAdditional( inRecords, inFirst, inLast );
        QuickSortAdditional( inRecords, inFirst, (pivotIndex-1) );
        QuickSortAdditional( inRecords, (pivotIndex+1), inLast );
    }
}

kvbuf_t* CCachePlugin::DSgetaddrinfo( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t     *outBuffer          = NULL;
    uint32_t    dictCount           = kvbuf_reset( inBuffer );
    uint32_t    valCount            = 0;
    char        *pName              = NULL;
    char        *pService           = NULL;
    char        *pProtocol          = NULL;
    char        *pFamily            = NULL;
    uint32_t    protocol            = 0;
    char        *pFlags             = NULL;
    uint32_t    flags               = 0;
    char        *pSockType          = NULL;
    uint32_t    socktype            = 0;
    uint32_t    family              = 0;
    uint32_t    parallel            = 0;
    bool        bResolveName        = false;
    bool        bHaveServiceName    = false;
    bool        bHaveName           = false;
    char        *key                = NULL;

    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "name") == 0 )
        {
            pName = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "service") == 0 )
        {
            pService = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "protocol") == 0 )
        {
            pProtocol = kvbuf_next_val( inBuffer );
            if( pProtocol != NULL )
            {
                protocol = strtoul( pProtocol, NULL, 10 );
            }
        }
        else if( strcmp(key, "socktype") == 0 )
        {
            pSockType = kvbuf_next_val( inBuffer );
            if( pSockType != NULL )
            {
                socktype = strtoul( pSockType, NULL, 10 );
            }            
        }
        else if( strcmp(key, "family") == 0 )
        {
            pFamily = kvbuf_next_val( inBuffer );
            if( pFamily != NULL )
            {
                family = strtoul( pFamily, NULL, 10 );
            }            
        }
        else if( strcmp(key, "ai_flags") == 0 )
        {
            pFlags = kvbuf_next_val( inBuffer );
            if( pFlags != NULL )
            {
                flags = strtoul( pFlags, NULL, 10 );
            }
        }
    }
	
    bHaveServiceName = (pService != NULL && pService[0] != '\0');
    bHaveName = (pName != NULL && pName[0] != '\0');
	
	if ( (flags & AI_PARALLEL) != 0 )
	{
		parallel = true;
		DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - AI_PARALLEL was requested" );
	}
	
    // check for invalid combinations
    if( (bHaveServiceName == false && bHaveName == false) ||
        (IPPROTO_UDP == protocol && SOCK_STREAM == socktype) || (IPPROTO_TCP == protocol && SOCK_DGRAM == socktype) )
    {
        return NULL;
    }
    
    // if we don't have a hostname or it is numeric, we do some extra work
    if( bHaveName == true )
    {
        // first check to see if it is a dotted-IP
        char        buffer[16];
        uint32_t    tempFamily  = AF_UNSPEC;
        
        if( inet_pton(AF_INET6, pName, buffer) == 1 )
            tempFamily = AF_INET6;
        if( tempFamily == AF_UNSPEC && inet_pton(AF_INET, pName, buffer) == 1 )
            tempFamily = AF_INET;
        
        if( AF_UNSPEC == tempFamily )
        {
            if( AI_NUMERICHOST == (flags & AI_NUMERICHOST) )
            {
                // this is an error condition, if we couldn't parse the IP and the user said it was dotted..
                return NULL; 
            }
            else
            {
                bResolveName = true; // if we are unspec, must be a non-dotted IP address
            }
        }
        else 
        {
            // if the family wasn't specified or the family discovered matched what was specified
            if( family == AF_UNSPEC || family == tempFamily )
            {
                family = tempFamily;
            }
            else
            {
                // this is an error condition... if the family stated didn't match, it's an error
                return NULL;
            }
        }
    }
    
    // we only use an array of 3 since we can only have 2 types of SRV queries and an MX query outstanding
    char        *socktypeList[]         = { NULL, NULL, NULL }; // this is used for the response
    char        *protocolList[]         = { NULL, NULL, NULL }; // this is used for the response
    char        *protocolListStr[]      = { NULL, NULL, NULL }; // used for searching SRV entries and lookup
    char        *servicePortList[]      = { NULL, NULL, NULL };
    char        *serviceNameList[]      = { NULL, NULL, NULL };
    kvbuf_t     *pOtherAnswers[]        = { NULL, NULL, NULL };
    uint32_t    serviceCount            = 0;
    sDNSLookup  *pDNSLookup             = NULL;
    kvbuf_t     *pTempBuffer            = NULL;
        
    // we search UDP, then TCP, but it's done in reverse, so we can minimize the roundtrips if a service has 
    // more than one protcol when the protocol is unspecified
    
    // we special case smtp and TCP combo and insert a specialized query
    if( bHaveServiceName == true && bResolveName && (IPPROTO_TCP == protocol || 0 == protocol) && (SOCK_STREAM == socktype || 0 == socktype) && 
        (strcmp(pService, "smtp") == 0 || strcmp(pService, "25") == 0) )
    {
        protocolListStr[serviceCount] = NULL;
        protocolList[serviceCount] = "6";
        socktypeList[serviceCount] = "1";
        serviceNameList[serviceCount] = strdup( "MX" );
        serviceCount++;
    }
    
    if( (IPPROTO_TCP == protocol || 0 == protocol) && (SOCK_STREAM == socktype || 0 == socktype) )
    {
        protocolListStr[serviceCount] = "tcp";
        protocolList[serviceCount] = "6";
        socktypeList[serviceCount] = "1";
        serviceCount++;
    }
    
    if( (IPPROTO_UDP == protocol || 0 == protocol) && (SOCK_DGRAM == socktype || 0 == socktype) )
    {
        protocolListStr[serviceCount] = "udp";
        protocolList[serviceCount] = "17";
        socktypeList[serviceCount] = "2";
        serviceCount++;
    }

    // if requested SOCK_RAW, but specified a port or service, then this is invalid request
    if ( socktype == SOCK_RAW && pService != NULL && pService[0] != '\0' )
        return NULL;
    
    // let's resolve service information first so we know what we are looking for next
    if( bHaveServiceName )
    {
        char *endptr = NULL;
        
        int32_t iPort = strtol( pService, &endptr, 10 );

        // if iPort is something other than 0, but pointer is non-NULL and is not pointing to a NULL char
        // then this was an invalid string, so return NULL, otherwise we do a lookup
        if( endptr != NULL && (*endptr) != '\0' && iPort != 0 )
            return NULL;
        
        int index = serviceCount;
        
        while( index > 0 )
        {
            index--;

            if( protocolListStr[index] != NULL ) // this is a special case query, no protocol signifies a MX query
            {
                kvbuf_t *request = kvbuf_new();
                if( request == NULL)
                {
                    break;
                }
                
                kvbuf_add_dict( request );
                
                kvbuf_add_key( request, (iPort == 0 ? "name" : "port") );
                kvbuf_add_val( request, pService );
                kvbuf_add_key( request, "proto" );
                kvbuf_add_val( request, protocolListStr[index] );
                
                kvbuf_t *answer = (iPort == 0 ? DSgetservbyname(request, inPID) : DSgetservbyport(request, inPID) );
                
                // if we have a port, no need to do anything special, just fill it
                if( iPort != 0 )
                {
                    servicePortList[index] = strdup( pService );
                }
                    
                if( kvbuf_reset(answer) > 0 )
                {
                    kvbuf_next_dict( answer );
                    
                    while( (key = kvbuf_next_key(answer, &valCount)) ) 
                    {
                        if( servicePortList[index] == NULL && strcmp(key, "s_port") == 0 )
                        {
                            char *pTemp = kvbuf_next_val( answer );
                            if( pTemp != NULL )
                            {
                                servicePortList[index] = strdup( pTemp );
                            }
                        }
                        else if( strcmp(key, "s_name") == 0 )
                        {
                            char *pTemp = kvbuf_next_val( answer );
                            if( pTemp != NULL )
                            {
                                serviceNameList[index] = strdup( pTemp );
                            }
                        }
                    }
                }
                kvbuf_free( answer );
                kvbuf_free( request );
            }
        }
    }
    
    // now let's resolve the name if needed
    if( bResolveName == true )
    {
        const char              *pIPv6              = ((AF_UNSPEC == family || AF_INET6 == family) ? "1" : "0");
        const char              *pIPv4              = ((AF_UNSPEC == family || AF_INET == family) ? "1" : "0");
        char                    *searchDomainUsed   = NULL;
        uint32_t                ttl                 = 0;
        
        // if we aren't parallel and we don't allow unqualified SRV queries, we start with step #2
        int                     iStateIndex         = (parallel == 0 && fUnqualifiedSRVAllowed == false ? 1 : 0); 
        
        // now let's look and see if the Additional query information is in the cache
        // look for answers for this question
        int                     iProtListIndex      = serviceCount;
        
        while( iProtListIndex > 0 )
        {
            iProtListIndex--;
            
            if( serviceNameList[iProtListIndex] != NULL )
            {
                if( protocolListStr[iProtListIndex] != NULL )
                {
                    pOtherAnswers[iProtListIndex] = FetchFromCache( fLibinfoCache, NULL, "sname", serviceNameList[iProtListIndex], 
                                                                    "pname", protocolListStr[iProtListIndex], "dnsname", pName, NULL );
                }
                else
                {
                    pOtherAnswers[iProtListIndex] = FetchFromCache( fLibinfoCache, NULL, "mxname", pName, NULL );
                }
            }
        }

        do
        {
            switch( fGetAddrStateEngine[iStateIndex] )
            {
                case kResolveStateCheckDomain:
                    // first lets see what name we used, either the actual, or a searchDomain
                    if( pTempBuffer != NULL && kvbuf_reset(pTempBuffer) > 0 )
                    {                        
                        kvbuf_next_dict( pTempBuffer );
                        while( (key = kvbuf_next_key( pTempBuffer, &valCount)) )
                        {
                            if( strcmp(key, "searchDomainUsed") == 0 )
                            {
                                searchDomainUsed = kvbuf_next_val( pTempBuffer );
                                break;
                            }
                        }
                        
                        // if no searchDomain, just stop now, means our original query should be good
                        if( searchDomainUsed == NULL )
                        {
                            DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Stopping search because no search domain used" );
                            fGetAddrStateEngine[iStateIndex+1] = kResolveStateDone;
                        }
                        else if( pDNSLookup != NULL )
                        {
                            DbgLog( kLogPlugin, "CCachePlugin::DSgetaddrinfo - Mismatch - re-issue new Additional Queries" );
                            DSDelete( pDNSLookup ); // now delete the structure since it was a bad query and redo
                            pDNSLookup = NULL;
                        }
                    }
                    break;
                case kResolveStateBuildExtraQueries:
                    if( bHaveServiceName == true && serviceCount != 0 )
                    {
                        iProtListIndex  = serviceCount;

                        DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Building Additional Queries" );
                        while( iProtListIndex > 0 )
                        {
                            iProtListIndex--;
                            
                            // if we have something to lookup and we got nothing from the cache
                            if( serviceNameList[iProtListIndex] != NULL && pOtherAnswers[iProtListIndex] == NULL )
                            {
                                pDNSLookup = CreateAdditionalDNSQueries( pDNSLookup, serviceNameList[iProtListIndex], protocolListStr[iProtListIndex],
                                                                         pName, searchDomainUsed, iProtListIndex );
                            }
                        }
                        DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Done Building Additional Queries" );
                    }
                    else
                    {
                        DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - No Additional Queries, no service supplied" );
                    }
                    break;
                case kResolveStateDoGetHostname: // this is where we do the actual get host name
                    DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Calling gethostbyname" );
                    pTempBuffer = DSgethostbyname_int( pName, pIPv4, pIPv6, inPID, parallel, pDNSLookup, &ttl );

                    // if no answer, stop here, nothing to do since it will wait for pDNSLookup as well
                    if( pTempBuffer == NULL )
                    {
                        fGetAddrStateEngine[iStateIndex+1] = kResolveStateDone;
                    }
                    break;
                case kResolveStateDoExtraQuery:
                    // here we just do the SRV query directly
                    if( pDNSLookup != NULL )
                    {
                        DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Initiating Additional Queries" );
                        InitiateDNSQuery( pDNSLookup, false ); // no need to parallel this query
                    }
                    break;
                default:
                    break;
            }
            
            iStateIndex++;
        } while( fGetAddrStateEngine[iStateIndex] != kResolveStateDone );
        
        DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - DNS Queries complete" );
        
        // we got a DNSQuery results directly, which means we did an SRV lookup, process them first
        if( NULL != pDNSLookup )
        {
            // we need to parse out any SRV lookups in this request, ignoring 
            for( int ii = 0; ii < pDNSLookup->fTotal; ii++ )
            {
                // if we encounter an A or AAAA record, we are finished with our SRV/MX entries
                // since they are listed first
                if( pDNSLookup->fQueryTypes[ii] == fTypeA || pDNSLookup->fQueryTypes[ii] == fTypeAAAA )
                    break;
                
                dns_reply_t *reply = pDNSLookup->fAnswers[ii];
                DbgLog( kLogDebug, "CCachePlugin::DSgetaddrinfo - Additional query %d answers = %d", ii,
                        (reply != NULL && reply->header != NULL ? reply->header->ancount : 0) );

                int tempIndex = pDNSLookup->fAdditionalInfo[ii];

                if( (NULL != reply) && (DNS_STATUS_OK == reply->status) )
                {
                    // grab any TTL based on the minimum returned
                    if ( pDNSLookup->fMinimumTTL[ii] > 0 && (ttl == 0 || pDNSLookup->fMinimumTTL[ii] < (int) ttl) )
                        ttl = pDNSLookup->fMinimumTTL[ii];

                    // if we have more than 1 answer, lets sort them
                    if( (NULL != reply->header) && (reply->header->ancount > 1) )
                        QuickSortAdditional( reply->answer, 0, (reply->header->ancount - 1) );
                    
                    // now that it's sorted, let's build a kvbuf_t for this response
                    pOtherAnswers[tempIndex] = dnsreply_to_kvbuf( reply, pIPv4, pIPv6, inPID, &ttl );
                }
                
                // if we ended up with some kind of TTL, let's cache
                if( ttl > 0 )
                {
                    kvbuf_t *copy   = NULL;
                    int     tempTTL = ttl;
                    
                    if( pOtherAnswers[tempIndex] != NULL )
                    {
                        copy = kvbuf_init( pOtherAnswers[tempIndex]->databuf, pOtherAnswers[tempIndex]->datalen );
                    }
                    else
                    {
                        tempTTL = pDNSLookup->fMinimumTTL[ii];
                    }

                    if( protocolListStr[tempIndex] != NULL )
                    {
                        AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, copy, CACHE_ENTRY_TYPE_HOST, tempTTL, 
													 "sname", serviceNameList[tempIndex], "pname", protocolListStr[tempIndex],
													 "dnsname", pName, NULL );
                    }
                    else
                    {
                        AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, copy, CACHE_ENTRY_TYPE_HOST, tempTTL, "mxname", pName, NULL );
                    }
                }                
            }
        }
    }
    
    // ok time to build a response
    // we base our forming of the answer on whether or not we had a name
    outBuffer = kvbuf_new();
    
    // special case the no name or no resolution case because we can optimize the loop...
    if( bHaveName == false || bResolveName == false )
    {
        char    pIPv6[INET6_ADDRSTRLEN] = { 0, };
        char    pIPv4[INET6_ADDRSTRLEN] = { 0, };
        char    *addresses[2]           = { NULL, NULL };
        char    *families[2]            = { NULL, NULL };
        int     addressCnt              = 0;
        
        // No name, see if AI_PASSIVE is set, if so just set the structure to INADDR_ANY or IN6ADDR_ANY_INIT
        // if it is not set then we use INADDR_LOOPBACK or IN6ADDR_LOOPBACK_INIT
        
        // we go in reverse so we do IPv4 first so it is listed last
        if( family == AF_INET || family == AF_UNSPEC )
        {
            if( bHaveName == false )
            {
                in_addr_t   any     = htonl( INADDR_ANY );
                in_addr_t   loop    = htonl( INADDR_LOOPBACK );
                
                inet_ntop( AF_INET, ((flags & AI_PASSIVE) == AI_PASSIVE ? &any : &loop), pIPv4, INET6_ADDRSTRLEN );
                addresses[addressCnt] = pIPv4;
            }
            else
            {
                addresses[addressCnt] = pName;
            }
            
            families[addressCnt] = "2";
            addressCnt++;
        }
        
        if( family == AF_INET6 || family == AF_UNSPEC )
        {
            if( bHaveName == false )
            {
                in6_addr    any     = IN6ADDR_ANY_INIT;
                in6_addr    loop    = IN6ADDR_LOOPBACK_INIT;
                
                inet_ntop( AF_INET6, ((flags & AI_PASSIVE) == AI_PASSIVE ? &any : &loop), pIPv6, INET6_ADDRSTRLEN );
                addresses[addressCnt] = pIPv6;
            }
            else
            {
                addresses[addressCnt] = pName;
            }
            families[addressCnt] = "30";
            addressCnt++;
        }
        
        while( addressCnt > 0 )
        {
            addressCnt--;

            int index = serviceCount;
            while( index > 0 )
            {
                index--;
                
                kvbuf_add_dict( outBuffer );
                
                kvbuf_add_key( outBuffer, "gai_flags");
                kvbuf_add_val( outBuffer, pFlags );
                
                kvbuf_add_key( outBuffer, "gai_socktype");
                kvbuf_add_val( outBuffer, socktypeList[index] );
                
                kvbuf_add_key( outBuffer, "gai_protocol");
                kvbuf_add_val( outBuffer, protocolList[index] );
                
                kvbuf_add_key( outBuffer, "gai_port");
                kvbuf_add_val( outBuffer, (servicePortList[index] != NULL ? servicePortList[index] : "0") );
                    
                kvbuf_add_key( outBuffer, "gai_address");
                kvbuf_add_val( outBuffer, addresses[addressCnt] );

                kvbuf_add_key( outBuffer, "gai_family");
                kvbuf_add_val( outBuffer, families[addressCnt] );
                
                char    *p = strrchr( addresses[addressCnt], '%' );
                if (p != NULL)
                {
                    char    scope[16]   = { 0, };
                    
                    unsigned scopeid = if_nametoindex( p + 1 );
                    
                    snprintf( scope, sizeof(scope), "%u", scopeid );
                    
                    kvbuf_add_key( outBuffer, "gai_scopeid" );
                    kvbuf_add_val( outBuffer, scope );
                }
                
                // if we have a null name and they want a canonnical name and AI_PASSIVE isn't set, then it is loopback
                if( bHaveName == false && (AI_CANONNAME & flags) == AI_CANONNAME && (AI_PASSIVE & flags) == 0 )
                {
                    kvbuf_add_key( outBuffer, "gai_canonname");
                    kvbuf_add_val( outBuffer, "localhost" );
                }                
            }                
        }
    }
    else
    {
        enum
        {
            kProcessAdditional  = 0,
            kProcessAddress,
            kAddValuesToDict,
            kDone
        };
        
        int         iState              = kProcessAdditional;
        int         iNextState          = kProcessAddress;
        char        **ipv6List          = NULL;
        char        **ipv4List          = NULL;
        char        **addressLists[]    = { NULL, NULL };
        char        *portList[]         = { NULL, NULL };
        int         listCount           = 0;
        char        *actualName         = NULL;
        bool        bAddedAddresses     = false;
        uint32_t    iCount              = 0;
        int         otherOffset         = 0;
        
        while( iState != kDone )
        {
            switch( iState )
            {
                case kProcessAdditional:
                    // here we look at the other answers before we decide to use gethostbyname answer
                    while( listCount == 0 && otherOffset < 3 )
                    {
                        kvbuf_t     *tempBuffer = pOtherAnswers[otherOffset];
                        
                        iCount = kvbuf_reset( tempBuffer );
                        if( iCount > 0 )
                        {
                            char        *port   = NULL;
                            uint32_t    vCount;

                            // if this is an MX lookup and we haven't added any values, we will use otherwise skip
                            // MX is always last in the lookups, so we should be ok..
                            if( protocolListStr[otherOffset] == NULL && bAddedAddresses == true )
                            {
                                otherOffset++;
                                continue;
                            }
                            
                            kvbuf_next_dict( tempBuffer );
                            
                            // let's get the lists we need...
                            while( (key = kvbuf_next_key(tempBuffer, &vCount)) )
                            {
                                if( vCount != 0 )
                                {
                                    char    **pWorkingList  = NULL;
                                    
                                    if( (family == AF_UNSPEC || family == AF_INET6) && strcmp(key, "h_ipv6_addr_list") == 0 )
                                    {
                                        ipv6List = (char **) calloc( vCount+1, sizeof(char *) );
                                        pWorkingList = ipv6List;
                                    }
                                    else if( (family == AF_UNSPEC || family == AF_INET) && strcmp(key, "h_ipv4_addr_list") == 0 )
                                    {
                                        ipv4List = (char **) calloc( vCount+1, sizeof(char *) );
                                        pWorkingList = ipv4List;
                                    }
                                    else if( strcmp(key, "h_name") == 0 )
                                    {
                                        actualName = kvbuf_next_val( tempBuffer );
                                    }
                                    else if( strcmp(key, "port") == 0 )
                                    {
                                        port = kvbuf_next_val( tempBuffer );
                                    }
                                    
                                    if( pWorkingList != NULL )
                                    {
                                        for( uint32_t ii = 0; ii < vCount; ii++ )
                                        {
                                            pWorkingList[ii] = kvbuf_next_val( tempBuffer );
                                        }
                                    }
                                }
                            }
                            
                            if( (family == AF_UNSPEC || family == AF_INET) && ipv4List != NULL )
                            {
                                addressLists[listCount] = ipv4List;
                                portList[listCount] = port;
                                listCount++;
                            }
                            
                            if( (family == AF_UNSPEC || family == AF_INET6) && ipv6List != NULL )
                            {
                                addressLists[listCount] = ipv6List;
                                portList[listCount] = port;
                                listCount++;
                            }
                        }
                        otherOffset++; // increment to the next one in case we come back
                    }
                    
                    if( listCount > 0 )
                    {
                        iNextState = kProcessAdditional; // continue here until we have nothing to do
                        iState = kAddValuesToDict;
                        continue;
                    }
                    else if( bAddedAddresses == false ) // skip straight to process addresses
                    {
                        iState = kProcessAddress;
                        continue;
                    }
                    else // no lists but we added addresses already, stop
                    {
                        iState = kDone;
                        continue;
                    }
                    break;
                case kProcessAddress:
                    // this is purely merging of gethostbyname info
                    iCount = kvbuf_reset( pTempBuffer );
                    if( iCount > 0 )
                    {
                        uint32_t    vCount;
                        
                        kvbuf_next_dict( pTempBuffer );
                        
                        // let's get the lists we need...
                        while( (key = kvbuf_next_key(pTempBuffer, &vCount)) )
                        {
                            if( vCount != 0 )
                            {
                                char    **pWorkingList  = NULL;
                                
                                if( (family == AF_UNSPEC || family == AF_INET6) && strcmp(key, "h_ipv6_addr_list") == 0 )
                                {
                                    ipv6List = (char **) calloc( vCount+1, sizeof(char *) );
                                    pWorkingList = ipv6List;
                                }
                                else if( (family == AF_UNSPEC || family == AF_INET) && strcmp(key, "h_ipv4_addr_list") == 0 )
                                {
                                    ipv4List = (char **) calloc( vCount+1, sizeof(char *) );
                                    pWorkingList = ipv4List;
                                }
                                else if( strcmp(key, "h_name") == 0 )
                                {
                                    actualName = kvbuf_next_val( pTempBuffer );
                                }
                                
                                if( pWorkingList != NULL )
                                {
                                    for( uint32_t ii = 0; ii < vCount; ii++ )
                                    {
                                        pWorkingList[ii] = kvbuf_next_val( pTempBuffer );
                                    }
                                }
                            }
                        }
                        
                        if( (family == AF_UNSPEC || family == AF_INET) && ipv4List != NULL )
                        {
                            addressLists[listCount] = ipv4List;
                            listCount++;
                        }

                        if( (family == AF_UNSPEC || family == AF_INET6) && ipv6List != NULL )
                        {
                            addressLists[listCount] = ipv6List;
                            listCount++;
                        }
                    }
                        
                    iNextState = kDone;
                    // just fall through here...
                    
                case kAddValuesToDict:
                    while( listCount > 0 )
                    {
                        listCount--;
                        
                        char    **tempList      = addressLists[listCount];
                        int     addrIndex = 0;
                        char    *address;
                        
                        while( (address = tempList[addrIndex]) != NULL )
                        {
                            // now let's step through each of our responses and see which we want to use
                            int     index       = serviceCount;

                            do
                            {
                                index--;
                                
                                kvbuf_add_dict( outBuffer );
                                
                                kvbuf_add_key( outBuffer, "gai_flags");
                                kvbuf_add_val( outBuffer, pFlags );
                                
                                // if we are still processing additional things, then use the otherOffset instead
                                kvbuf_add_key( outBuffer, "gai_socktype");
                                kvbuf_add_val( outBuffer, socktypeList[(iNextState == kProcessAdditional ? (otherOffset - 1) : index)] );
                                
                                kvbuf_add_key( outBuffer, "gai_protocol");
                                kvbuf_add_val( outBuffer, protocolList[(iNextState == kProcessAdditional ? (otherOffset - 1) : index)] );
                                
                                kvbuf_add_key( outBuffer, "gai_port");
                                if( portList[listCount] != NULL )
                                {
                                    kvbuf_add_val( outBuffer, portList[listCount] );
                                }
                                else
                                {
                                    kvbuf_add_val( outBuffer, (servicePortList[index] != NULL ? servicePortList[index] : "0") );
                                }
                                
                                if( tempList == ipv6List )
                                {
                                    char    *pAddress = (*tempList);
                                    char    *p = strrchr( pAddress, '%' );
                                    
                                    if (p != NULL)
                                    {
                                        char    scope[16]   = { 0, };
                                        
                                        unsigned scopeid = if_nametoindex( p + 1 );
                                        
                                        snprintf( scope, sizeof(scope), "%u", scopeid );
                                        
                                        kvbuf_add_key( outBuffer, "gai_scopeid" );
                                        kvbuf_add_val( outBuffer, scope );
                                    }
                                    
                                    kvbuf_add_key( outBuffer, "gai_address");
                                    kvbuf_add_val( outBuffer, pAddress );
                                    
                                    kvbuf_add_key( outBuffer, "gai_family");
                                    kvbuf_add_val( outBuffer, "30" );
                                }
                                else
                                {
                                    kvbuf_add_key( outBuffer, "gai_address");
                                    kvbuf_add_val( outBuffer, address );
                                    
                                    kvbuf_add_key( outBuffer, "gai_family");
                                    kvbuf_add_val( outBuffer, "2" );
                                }
                                
                                if( (AI_CANONNAME & flags) == AI_CANONNAME && actualName != NULL )
                                {
                                    kvbuf_add_key( outBuffer, "gai_canonname");
                                    kvbuf_add_val( outBuffer, actualName );
                                }   
                            } while( index > 0 && iNextState != kProcessAdditional );
                            
                            addrIndex++;
                        }
                        addressLists[listCount] = NULL;
                        portList[listCount] = NULL;
                        actualName = NULL;
                        bAddedAddresses = true;
                    }
                    
                    DSFree( ipv4List );
                    DSFree( ipv6List );
                    break;
                default:
                    break;
            }
            
            iState = iNextState;
        };
        
        DSFree( ipv4List );
        DSFree( ipv6List );
    }
    
    while( serviceCount > 0 )
    {
        serviceCount--;
        
        DSFree( servicePortList[serviceCount] );
        DSFree( serviceNameList[serviceCount] );
    }
    
    int otherOffset = 0;
    while( otherOffset < 3 )
    {
        kvbuf_free( pOtherAnswers[otherOffset] );
        pOtherAnswers[otherOffset] = NULL;
        otherOffset++;
    }
                
    if( pDNSLookup != NULL )
    {
		DSDelete( pDNSLookup );
    }
    
    // if the pTempBuffer == NULL or is empty, then we got no lookup from DNS, so if the outBuffer is also empty, let's release it and return NULL instead
    if ( (pTempBuffer == NULL || pTempBuffer->datalen == sizeof(int32_t)) && outBuffer != NULL && outBuffer->datalen == sizeof(int32_t) )
    {
        // if DNS lookup failed and we had no other answers free the answer
        kvbuf_free( outBuffer );
        outBuffer = NULL;
    }
    
    kvbuf_free( pTempBuffer );

    return outBuffer;
}

kvbuf_t* CCachePlugin::DSdns_proxy( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    char                *domain         = NULL;
    char                *key            = NULL;
    uint16_t            dnsclass        = 0xffff;
    uint16_t            dnstype         = 0xffff;
    uint32_t            searchType      = 0;
    uint32_t            valCount        = 0;

    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    while( (key = kvbuf_next_key(inBuffer, &valCount)) ) 
    {
        if( strcmp(key, "domain") == 0 )
        {
            domain = kvbuf_next_val( inBuffer );
        }
        else if( strcmp(key, "class") == 0 )
        {
            char *pTemp = kvbuf_next_val( inBuffer );
            if( NULL != pTemp )
            {
                dnsclass = strtol( pTemp, NULL, 10 );
            }
        }
        else if( strcmp(key, "type") == 0 )
        {
            char *pTemp = kvbuf_next_val( inBuffer );
            if( NULL != pTemp )
            {
                dnstype = strtol( pTemp, NULL, 10 );
            }
        }
        else if( strcmp(key, "search") == 0 )
        {
            char *pTemp = kvbuf_next_val( inBuffer );
            if( NULL != pTemp )
            {
                searchType = strtol( pTemp, NULL, 10 );
            }
        }
    }
    
    if( 0xffff == dnsclass || 0xffff == dnstype || NULL == domain )
    {
        return NULL;
    }
    
    dns_handle_t            dns                     = dns_open( NULL );
    int32_t                 bufferlen               = 0;
    uint32_t                fromlen                 = sizeof(struct sockaddr_storage);
    struct sockaddr         *from;
    char                    buffer[DNS_BUFFER_SIZE];
    
    if( dns )
    {
        int slot = AddToDNSThreads();

        from = (struct sockaddr *) calloc( 1, sizeof(sockaddr_storage) );
        
        if( searchType == 0 )
        {
            bufferlen = dns_query( dns, domain, dnsclass, dnstype, buffer, DNS_BUFFER_SIZE, from, &fromlen );
        }
        else
        {
            bufferlen = dns_search( dns, domain, dnsclass, dnstype, buffer, DNS_BUFFER_SIZE, from, &fromlen );
        }
        
        RemoveFromDNSThreads( slot );
        
        if( bufferlen > 0 )
        {
            outBuffer = kvbuf_new();
            kvbuf_add_dict( outBuffer );
            kvbuf_add_key( outBuffer, "data" );
            kvbuf_add_val_len( outBuffer, buffer, bufferlen );
            
            // reuse the buffer to encode the name
            inet_ntop( from->sa_family, (char *)(from) + (from->sa_family == AF_INET6 ? 8 : 4), buffer, DNS_BUFFER_SIZE );

            kvbuf_add_key( outBuffer, "server" );
            kvbuf_add_val( outBuffer, buffer );
        }
        
        DSFree( from );
        dns_free( dns );
    }
    
    return outBuffer;
}

kvbuf_t* CCachePlugin::DSgetbootpbyhw( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation   *theValidation	= NULL;
    char                buffer[32];
   
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "hw") != 0 || valCount == 0 )
        return NULL;
    
    char *hw = kvbuf_next_val( inBuffer );
    if( hw == NULL )
        return NULL;
	
	// need to canonicalize lowercase the mac address in order for it to be found in the cache
    struct ether_addr   etherStorage;
	struct ether_addr	*etherAddr	= myether_aton( hw, &etherStorage );
	if( etherAddr != NULL )
	{
		snprintf( buffer, sizeof(buffer), "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x", etherAddr->octet[0], etherAddr->octet[1], etherAddr->octet[2], etherAddr->octet[3], etherAddr->octet[4], etherAddr->octet[5] );
		hw = buffer;
	}
	else
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "bp_hw", hw, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
                                            kDSNAttrMetaNodeLocation,
                                            kDSNAttrRecordName,
                                            kDS1AttrENetAddress,
											kDSNAttrIPAddress,
                                            kDSNAttrIPAddressAndENetAddress,
											kDS1AttrBootFile,
                                            NULL );
        
        if ( attrTypes != NULL )
        {
            char    *keys[2] = { NULL, hw };
            
			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDS1AttrENetAddress, hw, kDSStdRecordTypeComputers, 1, attrTypes, 
                                                ParseBootpEntry, NULL, keys, &theValidation, eDSiExact );
            }
            else
            {
                outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDS1AttrENetAddress, hw, kDSStdRecordTypeComputers, 1, attrTypes,
                                                ParseBootpEntry, NULL, keys, &theValidation, eDSiExact );
            }
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount == 1 )
                {
                    char    *keys2[]	= { "bp_hw", "bp_addr", NULL };
                    kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
					AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_COMPUTER, 60, keys2 );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
				// we are only caching these for 60 seconds
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_COMPUTER, 60, "bp_hw", hw, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetbootpbyhw] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetbootpbyhw] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getbootpbyhw - Cache hit for %s", hw );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

kvbuf_t* CCachePlugin::DSgetbootpbyaddr( kvbuf_t *inBuffer, pid_t inPID )
{
    kvbuf_t             *outBuffer		= NULL;
    tDataListPtr        attrTypes		= NULL;
    uint32_t            dictCount       = kvbuf_reset( inBuffer );
    uint32_t            valCount        = 0;
    sCacheValidation    *theValidation	= NULL;
    
    if( dictCount == 0 )
        return NULL;
    
    kvbuf_next_dict( inBuffer );
    
    char *key = kvbuf_next_key( inBuffer, &valCount );
    if( strcmp(key, "addr") != 0 || valCount == 0 )
        return NULL;
    
    char *addr = kvbuf_next_val( inBuffer );
    if( addr == NULL )
        return NULL;
    
    outBuffer = FetchFromCache( fLibinfoCache, NULL, "bp_addr", addr, NULL );
    
    if ( outBuffer == NULL )
    {
        attrTypes = dsBuildListFromStrings( fDirRef,
											kDSNAttrMetaNodeLocation,
											kDSNAttrRecordName,
											kDS1AttrENetAddress,
											kDSNAttrIPAddress,
                                            kDSNAttrIPAddressAndENetAddress,
											kDS1AttrBootFile,
											NULL );
        
        if ( attrTypes != NULL )
        {
            char    *keys[2] = { addr, NULL };

			bool localOnlyPID = IsLocalOnlyPID( inPID );
			if( localOnlyPID == false )
            {
                outBuffer = ValueSearchLibInfo( fSearchNodeRef, kDSNAttrIPAddress, addr, kDSStdRecordTypeComputers, 1, attrTypes, 
												ParseBootpEntry, NULL, keys, NULL );
            }
            else
            {
                outBuffer = ValueSearchLibInfo( fFlatFileNodeRef, kDSNAttrIPAddress, addr, kDSStdRecordTypeComputers, 1, attrTypes,
												ParseBootpEntry, NULL, keys, NULL );
            }
            
            if( outBuffer != NULL )
            {
                // add this entry to the cache
                uint32_t dcount = kvbuf_reset( outBuffer );
                
                if( dcount == 1 )
                {
                    char    *keys2[]	= { "bp_hw", "bp_addr", NULL };
                    kvbuf_t *copy       = kvbuf_init( outBuffer->databuf, outBuffer->datalen );
                    
					AddEntryToCacheWithKeys( fLibinfoCache, theValidation, copy, CACHE_ENTRY_TYPE_COMPUTER, 60, keys2 );
                }
                else
                {
                    kvbuf_free( outBuffer );
                    outBuffer = NULL;
                }
            }
            
            // need to check this specific so we can do a negative cache
            if ( outBuffer == NULL && localOnlyPID == false ) // don't cache if it was a local only lookup
            {
				// we are only caching these for 60 seconds
                AddEntryToCacheWithMultiKey( fLibinfoCache, NULL, NULL, CACHE_ENTRY_TYPE_COMPUTER, 60, "bp_addr", addr, NULL );
            }
            
            dsDataListDeallocate( fDirRef, attrTypes );
            DSFree( attrTypes );
        }
        
        fStatsLock.WaitLock();
        fCacheMissByFunction[kDSLUgetbootpbyaddr] += 1;
        fStatsLock.SignalLock();
    }
    else
    {
        fStatsLock.WaitLock();
        fCacheHitsByFunction[kDSLUgetbootpbyaddr] += 1;
        fStatsLock.SignalLock();
        
        DbgLog( kLogPlugin, "CCachePlugin::getbootpbyaddr - Cache hit for %s", addr );
    }
    
	// validation data has an internal spinlock to protect release/retain
    theValidation->Release();
    
    return( outBuffer );
}

#pragma mark -
#pragma mark Support Routines
#pragma mark -

// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sCacheContextData* CCachePlugin::MakeContextData ( void )
{
    sCacheContextData	*pOut	= NULL;
    
    pOut = (sCacheContextData *) ::calloc( 1, sizeof(sCacheContextData) );
    if ( pOut != NULL )
    {
        pOut->fNodeName			= NULL;
        //use 99" user as init for UIDs ie. can't use 0
        pOut->fUID				= 99;
        pOut->fEffectiveUID		= 99;
}

return( pOut );

} // MakeContextData


// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

SInt32 CCachePlugin::CleanContextData ( sCacheContextData *inContext )
{
    SInt32				siResult 	= eDSNoErr;
    
    if (( inContext == NULL ) || ( gCacheNode == NULL ))
    {
        siResult = eDSBadContextData;
    }
    else
    {
        DSFreeString(inContext->fNodeName);
        DSFree( inContext );
    }
    
    return( siResult );
    
} // CleanContextData


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CCachePlugin::ContinueDeallocProc ( void* inContinueData )
{
    sCacheContinueData *pContinue = (sCacheContinueData *)inContinueData;
    
    //TODO do nothing with references now since they are now class members
    DSFree( pContinue );
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CCachePlugin::ContextDeallocProc ( void* inContextData )
{
    sCacheContextData *pContext = (sCacheContextData *) inContextData;
    
    if ( pContext != NULL )
    {
        CleanContextData( pContext );
    }
} // ContextDeallocProc


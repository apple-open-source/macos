/*

Copyright (c) 2003 Apple Computer, Inc. All rights reserved.

License for apache_mod_rendezvous_apple module:
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this list of 
conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list 
of conditions and the following disclaimer in the documentation and/or other materials 
provided with the distribution.
3. The end-user documentation included with the redistribution, if any, must include 
the following acknowledgment:
	"This product includes software developed by Apple Computer, Inc."
Alternately, this acknowledgment may appear in the software itself, if and 
wherever such third-party acknowledgments normally appear.
4. The names "Apache", "Apache Software Foundation", "Apple" and "Apple Computer, Inc." 
must not be used to endorse or promote products derived from this software without 
prior written permission. For written permission regarding the "Apache" and 
"Apache Software Foundation" names, please contact apache@apache.org.
5. Products derived from this software may not be called "Apache" or "Apple", 
nor may "Apache" or "Apple" appear in their name, without prior written 
permission of the Apache Software Foundation, or Apple Computer, Inc., respectively.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE COMPUTER, INC., 
THE APACHE SOFTWARE FOUNDATION OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGE.

Except as expressly set forth above, nothing in this License shall be construed 
as granting licensee, expressly or by implication, estoppel or otherwise, any 
rights or license under any trade secrets, know-how, patents, registrations, 
copyrights or other intellectual property rights of Apple, including without 
limitation, application programming interfaces ("APIs") referenced by the code, 
the functionality implemented by the APIs and the functionality invoked by calling 
the APIs.

*/

/* 
 * mod_rendezvous_apple Apache module
 *
 
 * This does not process requests; it just processes the config file to 
 * determine what if any names need to be registered with Rendezvous, and registers them.
 * 
 * Apache 1 is normally single-threaded; this module spawns a pthread to handle 
 * registration callbacks. These can come at any time after registration, so the thread needs to persist.
 * The run loop for that thread has a source for each registered service.
 * An array of pointers to replyRec structures is kept, one for each registered service, so that
 * the old registrations can be cleaned up on a graceful restart.
 *
 * Because Apache has a bootstrap phase where the config file gets processed, it's necessary to
 * use a global ctx so that the code can tell whether it's in the bootstrap phase or the real
 * processing phase. 
 *
 */

#define CORE_PRIVATE                    1
#define MSG_PREFIX                      "mod_rendezvous_apple:"

#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <DNSServiceDiscovery/DNSServiceDiscovery.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_conf_globals.h"
 
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <pthread.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#define getModConfig()    (module_cfg_rec *)ap_ctx_get( ap_global_ctx, "rendezvous_apple_module" )
#define TITLE_MAX 127
#define TXT_MAX 511
#define REGNAME_MAX 255
#define PORT_MAX 65535
#define MAX_NAME_FORMAT 64
#define ALL_USERS "all-users"
#define CUSTOMIZED_USERS "customized-users"
#define DEFAULT_NAME_FORMAT "%l"

typedef struct replyRec {	/* Needs to store info required to retry registration */
    char        name[REGNAME_MAX+1];	/* and to clean up upon deregistration */
    char        txt[TXT_MAX+1];
    int         port;
    dns_service_discovery_ref serviceRef;
    server_rec*		serverData;
    CFRunLoopSourceRef	runLoopSource;
    Boolean		shouldFreeInfo;
    CFMachPortRef	cfMachPort;
    mach_port_t 	machPort;
} replyRec;

typedef struct module_cfg_rec {
    pool *pPool;
    int initCount;              // work around multiple apache calls to init
    array_header *replyRecPtrArray;
    pthread_mutex_t threadMutex;
	pthread_cond_t cond;
	replyRec* initialRegReplyData;
    CFRunLoopRef replyRunLoop;
    int globalErr;
} module_cfg_rec;

typedef struct server_cfg_rec { // holdover from mod_example
    int cmode;                  /* Environment to which record applies (directory,
                                 * server, or combination).
                                 */
#define CONFIG_MODE_SERVER 1
#define CONFIG_MODE_DIRECTORY 2
#define CONFIG_MODE_COMBO 3
    int local;                  /* Boolean: "Example" directive declared here? */
    int congenital;             /* Boolean: did we inherit an "Example"? */
    char *loc;                  /* Location to which this record applies. */
} server_cfg_rec;


module MODULE_VAR_EXPORT rendezvous_apple_module;

static int 		allUsersDone = 0;
static void*	templateIndexMM = 0;
static int		templateIndexFD = 0;
static struct stat 	templateIndexFinfo;

static void     awaitRendezvousCallbacks( server_rec* serverData );
static void		unregisterRefs();
static int 		createCallbackThread( server_rec *serverData );
static void 	registerService( char* inName, int inPort, char* inTxt, server_rec* serverData );
static void     handleMachMessage( CFMachPortRef port, void *msg, CFIndex size, void *info );
static void     regReply( DNSServiceRegistrationReplyErrorType inErrorCode, void *replyDataVoid );
static tDirStatus getDefaultLocalNode ( tDirReference inDirRef, tDirNodeReference *outLocalNodeRef );
static tDirStatus processUsers( tDirReference dirRef, tDirNodeReference defaultLocalNodeRef,
                    char* whichUsers, char* regNameFormat, int port, cmd_parms *cmd );
static int 		getUserSitePath( char *inUserName, char *outSitePath, cmd_parms* cmd );
static char* 	extractHTMLTitle( char* fileName, cmd_parms* cmd );
static Boolean	userHasValidCustomizedSite( char* inUserName, cmd_parms* cmd );
static void     registerUser( char* inUserName, char* inRegNameFormat, int inPort, cmd_parms *cmd );
static void 	getTitle( char* inSiteFolder, char* outTitle, cmd_parms* cmd );
static void     registerUserTitle( char* inUserName, int inPort, cmd_parms *cmd );
static void     registerUsers( char* whichUsers, char* regNameFormat, int port, cmd_parms *cmd );
static const char *processRegDefaultSite( cmd_parms *cmd, void *dummy, const char *arg );
static const char *processRegUserSite( cmd_parms *cmd, void *dummy, char *inName, char *inPort, char *inHost );
static const char *processRegResource( cmd_parms *cmd, void *dummy, char *inName, char *inPath, char* inPort );
static void     registerComputerName( server_rec *serverData, int inPort );
static void 	rendezvousChildInit( server_rec *serverData, pool *p );
static void 	rendezvousModuleInit( server_rec *serverData, pool *p );
static void 	*rendezvousModuleCreateServerConfig( pool *p, char *dirspec );

/* 
* Block on CFRunloop until a callback arrives
*/
static void awaitRendezvousCallbacks( server_rec* serverData ) {
    int status = 0;
    module_cfg_rec *module_cfg = getModConfig();
	CFRunLoopRef runLoop = CFRunLoopGetCurrent();
	pthread_mutex_lock(&module_cfg->threadMutex);
	// Serialize access to this; read by main thread
	module_cfg->replyRunLoop = runLoop;
	pthread_mutex_unlock(&module_cfg->threadMutex);
	pthread_cond_signal(&module_cfg->cond);

    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, serverData,
            "%s In awaitRendezvousCallbacks.",
            MSG_PREFIX );
	CFRunLoopAddSource( CFRunLoopGetCurrent(), module_cfg->initialRegReplyData->runLoopSource, 
		kCFRunLoopCommonModes );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, serverData,
		"%s Now awaiting callbacks for initial registration name '%s'.",
		MSG_PREFIX, module_cfg->initialRegReplyData->name );

    CFRunLoopRun();

	module_cfg->replyRunLoop = NULL;
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, serverData,
            "%s No more registrations.",
            MSG_PREFIX );
    return;
}

/* Unregister all the saved refs, clean up, and clear the array
*/
static void unregisterRefs() {
    int i;
    module_cfg_rec *module_cfg = getModConfig();
    replyRec** replyDataPtrs = NULL;
    replyDataPtrs = (replyRec**)module_cfg->replyRecPtrArray->elts;
	
    for (i = 0; i < module_cfg->replyRecPtrArray->nelts; i++) {
        if (!replyDataPtrs[i])
            continue;
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, replyDataPtrs[i]->serverData,
            "%s DeRegistering '%s' cfMachPort %d from pid %d",
            MSG_PREFIX, replyDataPtrs[i]->name, (int) replyDataPtrs[i]->cfMachPort, getpid() );

        if (replyDataPtrs[i]->serviceRef) {
            DNSServiceDiscoveryDeallocate( replyDataPtrs[i]->serviceRef );
			replyDataPtrs[i]->serviceRef = 0;
        }
    }
    for (i = 0; i < module_cfg->replyRecPtrArray->nelts; i++) {
        if (!replyDataPtrs[i])
            continue;
        if (replyDataPtrs[i]->runLoopSource) {
            CFRunLoopSourceInvalidate( replyDataPtrs[i]->runLoopSource );
            CFRunLoopRemoveSource( module_cfg->replyRunLoop, replyDataPtrs[i]->runLoopSource, kCFRunLoopDefaultMode );
            CFRelease( replyDataPtrs[i]->runLoopSource );
        }
        if (replyDataPtrs[i]->cfMachPort) {
            CFMachPortInvalidate( replyDataPtrs[i]->cfMachPort );
            CFRelease( replyDataPtrs[i]->cfMachPort );
        }
        if (replyDataPtrs[i]->machPort && replyDataPtrs[i]->shouldFreeInfo)
            mach_port_deallocate( mach_task_self(), replyDataPtrs[i]->machPort );
        free( replyDataPtrs[i] );
    }
    module_cfg->replyRecPtrArray->nelts = 0;
    if (templateIndexMM > 0) {
        close( templateIndexFD );
        munmap( templateIndexMM, templateIndexFinfo.st_size );
        templateIndexMM = 0;
    }
}

/*
* Create a thread to handle registration callbacks
* Return 0 if successful, 1 if error.
*/
static int createCallbackThread( server_rec *serverData ) {

    pthread_attr_t      theThreadAttrs;
    pthread_t           thread;
    int			status;

    module_cfg_rec *module_cfg = getModConfig();
    
    status = pthread_attr_init( &theThreadAttrs );
    if (status) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Error allocating thread attribute.",
            MSG_PREFIX );
        return status;
    }
    
    status = pthread_attr_setdetachstate( &theThreadAttrs, PTHREAD_CREATE_DETACHED );
    if (status) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Error setting thread attribute; cannot confirm registration.",
            MSG_PREFIX );
        return 1;
    }
    
    status = pthread_create( &thread, &theThreadAttrs, (void*) awaitRendezvousCallbacks,
        (void*) serverData );
    if (status) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Cannot create registration callback thread; cannot confirm any registrations.",
            MSG_PREFIX );
        return 1;                
    }
    
    status = pthread_attr_destroy( &theThreadAttrs );
    if (status) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, serverData,
            "%s Cannot destroy registration callback thread attributes.",
            MSG_PREFIX );
        return 0;                
    }
	return 0;
}

/*
* Register the service, add source to the reply runloop.
* The port is assumed to NOT already be in network byte order.
*/
static void registerService( char* inName, int inPort, char* inTxt, server_rec* serverData ) {
    
    int			status;
    module_cfg_rec *module_cfg = getModConfig();
    
    // Allocate a reply structure; this will be passed to regReply callback.
    replyRec* replyData = (replyRec*) malloc( sizeof(replyRec) );
    if (!replyData) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Error allocating memory, cannot register '%s'.",
            MSG_PREFIX, inName );
        return;
    }
    
    if (inName)
        strncpy( replyData->name, inName, sizeof(replyData->name) );
    if (inTxt)
        strncpy( replyData->txt, inTxt, sizeof(replyData->txt) );
    replyData->port = inPort;
    replyData->serverData = serverData;
        
    // Need to give context to the regReply callback so it can try a new name if advised of a conflict.

    replyData->serviceRef = DNSServiceRegistrationCreate( inName, "_http._tcp", "", 
        htons( inPort ), inTxt, regReply, (void*)replyData );
            
    if (!replyData->serviceRef) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Error trying to register '%s' from pid=%d.",
            MSG_PREFIX, inName, getpid() );
        free( replyData );
        return;
    }
    
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, serverData,
        "%s Registered name='%s' txt='%s' port=%d from pid=%d; awaiting confirmation.",
        MSG_PREFIX, inName, inTxt, inPort, getpid() );

	// From here on, an error means we won't be able to get callback,
	// but don't free replyData.
	
    replyData->machPort = DNSServiceDiscoveryMachPort( replyData->serviceRef );
    if (!replyData->machPort) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Error retrieving mach port; will not receive callbacks for name='%s'.",
            MSG_PREFIX, inName );
        return;
    }

    CFMachPortContext  context = { 0, 0, NULL, NULL, NULL };
    replyData->cfMachPort = CFMachPortCreateWithPort( kCFAllocatorDefault, replyData->machPort, 
            handleMachMessage, &context, &(replyData->shouldFreeInfo) );
    if (!replyData->cfMachPort) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Could not create cfMachPort; will not receive callbacks for name='%s'.",
            MSG_PREFIX, inName );
		return;
	}

    replyData->runLoopSource = CFMachPortCreateRunLoopSource( NULL, replyData->cfMachPort, 0 );
    if (!replyData->runLoopSource) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, serverData,
            "%s Could not create runloop source from mach port; will not receive callbacks for name='%s'.",
            MSG_PREFIX, inName );
		return;
	}
	module_cfg->initialRegReplyData = replyData;
				
	if (!module_cfg->replyRunLoop) {
		struct timeval now;
		struct timespec waitTime = { 0, 0 };
		if (createCallbackThread( serverData )) {
			module_cfg->globalErr = 1;
			return;
		}
		/* Wait up to 10 seconds for thread to start up */
		gettimeofday(&now, NULL);
		waitTime.tv_sec = now.tv_sec + 10;
		pthread_mutex_lock(&module_cfg->threadMutex);
		while (!module_cfg->replyRunLoop) {
			int ret = pthread_cond_timedwait(&module_cfg->cond, &module_cfg->threadMutex, &waitTime);
			if (ETIMEDOUT == ret) {
				/* Thread failed to start */
				pthread_mutex_unlock(&module_cfg->threadMutex);
				module_cfg->globalErr = 1;
				ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, serverData,
				"%s timeout waiting for thread to start.",
				MSG_PREFIX );
				return;
			}
		}
		pthread_mutex_unlock(&module_cfg->threadMutex);
	}

	else {
		CFRunLoopAddSource( module_cfg->replyRunLoop, replyData->runLoopSource, kCFRunLoopCommonModes );
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, serverData,
		"%s Now awaiting callbacks for name='%s'.",
		MSG_PREFIX, inName );
	}
}

/*
* This is passed to CFMachPortCreateWithPort; it gets called when a message is received.
* It just relays the message to the Rendezvous function that calls regReply().
*/
static void handleMachMessage( CFMachPortRef port, void *msg, CFIndex size, void *info ) {
    DNSServiceDiscovery_handleReply( msg );
}

/*
* This gets called asynchronously.
*/
static void regReply( DNSServiceRegistrationReplyErrorType inErrorCode, void *replyDataVoid ) {

    char        newName[REGNAME_MAX+1];
    void*       unusedValue = NULL;
    replyRec* 	replyData = (replyRec*)replyDataVoid;
    module_cfg_rec *module_cfg = getModConfig();
    int 		status = 0;
    Boolean 	threadSafe = false;
    replyRec** 	saveReplyRec;
    int 		i;
    replyRec** 	replyDataPtrs = NULL;
    replyDataPtrs = (replyRec**)module_cfg->replyRecPtrArray->elts;
    Boolean 	found;
    
    status = pthread_mutex_lock( &(module_cfg->threadMutex) );
    threadSafe = (status == 0);

    switch (inErrorCode) {
    
        case kDNSServiceDiscoveryNoError:
        
            if (threadSafe) {
                // Save pointer to reply rec so we can free it when deregistering
                found = FALSE;
                for (i = 0; i < module_cfg->replyRecPtrArray->nelts; i++) {
                    if (!strcmp( replyDataPtrs[i]->name, replyData->name )) {
                        found = TRUE;
                        break;
                    }
                }
                if (found)
                    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, replyData->serverData,
                        "%s Re-registration confirmed for name='%s', port=%d.",
                        MSG_PREFIX, replyData->name, replyData->port );
                else {
                    saveReplyRec = (replyRec **)ap_push_array( module_cfg->replyRecPtrArray );
                    *saveReplyRec = replyData;
                    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, replyData->serverData,
                        "%s Registration confirmed for name='%s', port=%d.",
                        MSG_PREFIX, replyData->name, replyData->port );
                }
            }
            break;
            
        case kDNSServiceDiscoveryNameConflict: 
            if (threadSafe) {
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, replyData->serverData,
                    "%s Name %s in use; trying again with a new name", MSG_PREFIX, replyData->name );
                if (replyData->serverData->server_hostname)
                    snprintf( newName, sizeof(newName), "%s - %s", replyData->name, replyData->serverData->server_hostname );
                else
                    snprintf( newName, sizeof(newName), "%s - %ld", replyData->name, random() % 100 );
                // Note that we're in a callback function, and we call this. Review carefully.
                registerService( newName, replyData->port, replyData->txt, replyData->serverData );
            }
            break;
            
        default:
            if (threadSafe) {
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, replyData->serverData,
                    "%s Registration callback received unexpected error %d for name '%s'.",
                    MSG_PREFIX, inErrorCode, replyData->name );
            }
    }
    
    if (threadSafe) {
        status = pthread_mutex_unlock( &(module_cfg->threadMutex) );
	if (status)
            ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, replyData->serverData,
                "%s Registration callback cannot release mutex lock for name '%s'.",
             	MSG_PREFIX, replyData->name );
    }
}

/* Find the default local directory node 
*/
static tDirStatus getDefaultLocalNode( tDirReference inDirRef, tDirNodeReference *outLocalNodeRef )
{
    tDirStatus dsStatus 	= eMemoryAllocError;
    unsigned long nodeCount 	= 0;
    tDataBuffer *pTDataBuff     = NULL;
    tDataList *pDataList        = NULL;

    pTDataBuff = dsDataBufferAllocate( inDirRef, 8*1024 );
    if ( pTDataBuff != NULL ) {
        dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, eDSLocalNodeNames, &nodeCount, NULL );
        if ( dsStatus == eDSNoErr ) {
            dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
            if ( dsStatus == eDSNoErr )
                dsStatus = dsOpenDirNode( inDirRef, pDataList, outLocalNodeRef );

            if ( pDataList != NULL ) {
                (void)dsDataListDeallocate( inDirRef, pDataList );
                free( pDataList );
                pDataList = NULL;
            }
        }
        (void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
        pTDataBuff = NULL;
    }
    return dsStatus;
}

/* Call registerUser for each user in node
*/
static tDirStatus processUsers( tDirReference inDirRef, tDirNodeReference inNodeRef, char* whichUsers, char* regNameFormat, int inPort, cmd_parms *cmd ) {

    tDataBufferPtr      nameDataBufferPtr;
    tDataList           recTypes;
    tDataList           recNames;
    tDataList           attributeList;
    tRecordEntryPtr     recordEntryPtr;
    tAttributeListRef   attrListRef = 0;
    tDirStatus          dsStatus;
    unsigned long       recordCount = 0;
    unsigned long       i;
    tContextData        contextDataRecList = NULL;
    tAttributeValueListRef  attrValueListRef;
    tAttributeEntryPtr	attrEntryPtr;
    tAttributeValueEntryPtr attrValueEntryPtr;
    char*               recordName;

    if( !(nameDataBufferPtr = dsDataBufferAllocate( inDirRef, 8*1024 )) ) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
            "%s Failure of dsDataBufferAllocate",
            MSG_PREFIX );
        return -1;
    }
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &recTypes, kDSStdRecordTypeUsers, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
            "%s Failure of dsDataBufferDeAllocate: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
        
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &recNames, kDSRecordsAll, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
        dsDataListDeallocate( inDirRef, &recTypes );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
            "%s Failure of dsBuildListFromStringsAlloc: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
 
    dsStatus = dsBuildListFromStringsAlloc( inDirRef, &attributeList, kDSNAttrRecordName,
        kDS1AttrUniqueID, NULL );
    if (dsStatus != eDSNoErr) {
	dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
        dsDataListDeallocate( inDirRef, &recTypes );
        dsDataListDeallocate( inDirRef, &recNames );
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
            "%s Failure of dsBuildListFromStringsAlloc: %d",
            MSG_PREFIX, dsStatus );
        return dsStatus;
    }
    do { 
        /* 
            While there are more buffers 
        */

        dsStatus = dsGetRecordList( inNodeRef, nameDataBufferPtr, &recNames, eDSiExact, &recTypes, 
            &attributeList, 0, &recordCount, &contextDataRecList );
        if (dsStatus != eDSNoErr) {
            dsDataListDeallocate( inDirRef, &attributeList );
            dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
            dsDataListDeallocate( inDirRef, &recTypes );
            dsDataListDeallocate( inDirRef, &recNames );
            ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                "%s Failure of dsGetRecordList: %d",
                MSG_PREFIX, dsStatus );
            return dsStatus;
        }

        for (i = 1; i <= recordCount; i++) {
            /* 
                For one buffer load of records
            */
            
            dsStatus = dsGetRecordEntry( inNodeRef, nameDataBufferPtr, i, &attrListRef, 
                &recordEntryPtr );
            if (dsStatus != eDSNoErr) { 
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                    "%s Failure of dsGetRecordEntry: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
                return dsStatus;
            }
    
            // Turn the AttrListRef into an AttrValueListRef
            dsStatus = dsGetAttributeEntry( inNodeRef, nameDataBufferPtr, attrListRef, 1, &attrValueListRef,
                &attrEntryPtr );
            if (dsStatus != eDSNoErr) {
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                dsCloseAttributeList( attrListRef );
                dsDeallocRecordEntry( inDirRef, recordEntryPtr );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                    "%s Failure of dsGetAttributeEntry: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
                return dsStatus;
            }
    
            // Finally get the name attribute
            dsStatus = dsGetAttributeValue( inNodeRef, nameDataBufferPtr, 1, attrValueListRef,
                &attrValueEntryPtr);
            if (dsStatus  != eDSNoErr) {
                dsDataListDeallocate( inDirRef, &attributeList );
                dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
                dsDataListDeallocate( inDirRef, &recTypes );
                dsDataListDeallocate( inDirRef, &recNames );
                dsDeallocAttributeEntry( inDirRef, attrEntryPtr );
                dsCloseAttributeValueList( attrValueListRef );
                dsCloseAttributeList( attrListRef );
                dsDeallocRecordEntry( inDirRef, recordEntryPtr );
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                    "%s Failure of dsGetAttributeValue: %d, i=%lu",
                    MSG_PREFIX, dsStatus, i );
                return dsStatus;
            }
            
            recordName = attrValueEntryPtr->fAttributeValueData.fBufferData;
            if (!strcmp( whichUsers, ALL_USERS ))
                registerUser( recordName, regNameFormat, inPort, cmd );
            else if (!strcmp( whichUsers, CUSTOMIZED_USERS )) {
                if (userHasValidCustomizedSite( recordName, cmd ))
                    registerUser( recordName, regNameFormat, inPort, cmd );
                else
                    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
                        "%s Skipping non-customized user %s",
                        MSG_PREFIX, recordName );
            }
            else
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                        "%s Unexpected 'RegisterUserSite %s', not registering.",
                        MSG_PREFIX, whichUsers );

            dsDeallocAttributeValueEntry( inDirRef, attrValueEntryPtr );
            dsDeallocAttributeEntry( inDirRef, attrEntryPtr );
            dsCloseAttributeValueList( attrValueListRef );
            dsCloseAttributeList( attrListRef );
            dsDeallocRecordEntry( inDirRef, recordEntryPtr );
        }
    } while (contextDataRecList != NULL);
    
    dsDataBufferDeAllocate( inDirRef, nameDataBufferPtr );
    dsDataListDeallocate( inDirRef, &attributeList );
    dsDataListDeallocate( inDirRef, &recTypes );
    dsDataListDeallocate( inDirRef, &recNames );
    return eDSNoErr;
}

/*
* Read mod_userdir's config structure to determine userdir site path.
* This function includes code from translate_userdir() in mod_userdir from
* apache 1.3.27.
* Returns 0 on success and sets outSitePath to the translated path to the site
* (i.e., it sets outSitePath to the local file path that 
* "http://www.example.com/~username/" translates to).
* Returns 1 if there is a problem or if mod_userdir config does not allow access to 
* user's dir.
*/
static int getUserSitePath( char *inUserName, char *outSitePath, cmd_parms* cmd ) {

    typedef struct userdir_config {
        int globally_disabled;
        char *userdir;
        table *enabled_users;
        table *disabled_users;
    } userdir_config;

    userdir_config *s_cfg;
    
    module* userdir_mod = ap_find_linked_module( "mod_userdir.c" );
    if (!userdir_mod) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Mod_userdir not loaded", MSG_PREFIX);
        return 1;
    }

    s_cfg = (userdir_config *) ap_get_module_config( cmd->server->module_config, userdir_mod );
    if (!s_cfg) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Mod_userdir config not present", MSG_PREFIX );
        return 1;
    }
    
    if (!s_cfg->userdir) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Null userdir", MSG_PREFIX );
        return 1;
    }

    const char *userdirs = s_cfg->userdir;
    struct stat statbuf;

    /*
     * Skip username if it's in the disabled list.
     */
    if (ap_table_get( s_cfg->disabled_users, inUserName ) != NULL) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
        "%s User '%s' is in disabled users list; not registering", MSG_PREFIX, inUserName );
        return 1;
    }
    /*
     * If there's a global interdiction on UserDirs, check to see if this
     * name is one of the Blessed.
     */
    if (s_cfg->globally_disabled
        && (ap_table_get( s_cfg->enabled_users, inUserName ) == NULL)) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Userdir globally disabled, and user '%s' is not an exception; not registering",
            MSG_PREFIX, inUserName );
        return 1;
    }

    /*
     * Special cases all checked, onward to normal substitution processing.
     */

    while (*userdirs) {
        const char *userdir = ap_getword_conf( cmd->pool, &userdirs );
        char *filename = NULL;
        int is_absolute = ap_os_is_path_absolute( userdir );

        if (strchr( userdir, '*' )) {
            /* token '*' embedded:
             */
            char *x = ap_getword( cmd->pool, &userdir, '*' );
            if (is_absolute) {
                /* token '*' within absolute path
                 * serves [UserDir arg-pre*][user][UserDir arg-post*]
                 * /somepath/ * /somedir + /~smith -> /somepath/smith/somedir
                 */
                filename = ap_pstrcat( cmd->pool, x, inUserName, userdir, NULL );
            }
            else if (strchr( x, ':' )) {
                /* token '*' within a redirect path
                 * serves [UserDir arg-pre*][user][UserDir arg-post*]
                 * http://server/user/ * + /~smith/foo ->
                 *   http://server/user/smith/foo
                 */
                break;
            }
            else {
                /* Not a redirect, not an absolute path, '*' token:
                 * serves [homedir]/[UserDir arg]
                 * something/ * /public_html
                 * Shouldn't happen, we trap for this in set_user_dir
                 */
                break;
            }
        }
        else if (is_absolute) {
            /* An absolute path, no * token:
             * serves [UserDir arg]/[user]
             * /home + /~smith -> /home/smith
             */
            if (userdir[strlen( userdir ) - 1] == '/')
                filename = ap_pstrcat( cmd->pool, userdir, inUserName, NULL );
            else
                filename = ap_pstrcat( cmd->pool, userdir, "/", inUserName, NULL );
        }
        else if (strchr( userdir, ':' )) {
            /* A redirect, not an absolute path, no * token:
             * serves [UserDir arg]/[user][dname]
             * http://server/ + /~smith/foo -> http://server/smith/foo
             */
            break;
        }
        else {
            /* Not a redirect, not an absolute path, no * token:
             * serves [homedir]/[UserDir arg]
             * e.g. /~smith -> /home/smith/public_html
             */
            struct passwd *pw;
            if ((pw = getpwnam( inUserName ))) {
                filename = ap_pstrcat( cmd->pool, pw->pw_dir, "/", userdir, NULL );
            }
        }

        /*
         * Now see if it exists.
         */
        if (filename && (stat( filename, &statbuf ) != -1)) {
            strncpy( outSitePath, filename, PATH_MAX );
            return 0;
        }
    }
    return 1;
}

/*
* Most of this code comes from the find_title() function in mod_autoindex in Apache 1.3.27
*/
static char *extractHTMLTitle( char* fileName, cmd_parms* cmd )
{
    char titlebuf[MAX_STRING_LEN], *find = "<TITLE>";
    FILE *thefile = NULL;
    int x, y, n, p;

    if (!(thefile = ap_pfopen(cmd->pool, fileName, "r"))) {
        return NULL;
    }
    n = fread(titlebuf, sizeof(char), MAX_STRING_LEN - 1, thefile);
    if (n <= 0) {
        ap_pfclose(cmd->pool, thefile);
        return NULL;
    }
    titlebuf[n] = '\0';
    for (x = 0, p = 0; titlebuf[x]; x++) {
        if (ap_toupper(titlebuf[x]) == find[p]) {
            if (!find[++p]) {
                if ((p = ap_ind(&titlebuf[++x], '<')) != -1) {
                    titlebuf[x + p] = '\0';
                }
                /* Scan for line breaks  */
                for (y = x; titlebuf[y]; y++) {
                    if ((titlebuf[y] == CR) || (titlebuf[y] == LF)) {
                        if (y == x) {
                            x++;
                        }
                        else {
                            titlebuf[y] = ' ';
                        }
                    }
                }
                ap_pfclose(cmd->pool, thefile);
                return ap_pstrdup(cmd->pool, &titlebuf[x]);
            }
        }
        else {
            p = 0;
        }
    }
    ap_pfclose(cmd->pool, thefile);
}

/*
* Figure out the index file at the given Site Folder path, as determined by mod_dir,
* and call a function to extract the HTML title.
*/
static void getTitle( char* inSiteFolder, char* outTitle, cmd_parms* cmd ) {

    typedef struct dir_config_struct {
        array_header *index_names;
    } dir_config_rec;

    int i;
    dir_config_rec *dir_cfg;
    char *dummy_ptr[1];
    char **dirIndexNames;
    int dirIndexNameCount;
    
    module* dir_mod = ap_find_linked_module( "mod_dir.c" );
    if (!dir_mod) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Mod_dir not loaded", MSG_PREFIX);
        *outTitle = NULL;
        return;
    }

    dir_cfg = (dir_config_rec *) ap_get_module_config( cmd->server->lookup_defaults, dir_mod );
    if (!dir_cfg) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Mod_dir config not present", MSG_PREFIX );
        *outTitle = NULL;
        return;
    }

    if (dir_cfg->index_names) {
	dirIndexNames = (char **)dir_cfg->index_names->elts;
	dirIndexNameCount = dir_cfg->index_names->nelts;
    }
    else {
	dummy_ptr[0] = DEFAULT_INDEX;
	dirIndexNames = dummy_ptr;
	dirIndexNameCount = 1;
    }

    for (; dirIndexNameCount; ++dirIndexNames, --dirIndexNameCount) {
        char *dirIndexName = *dirIndexNames;
        char fullSitePath[PATH_MAX+1];

        strncpy( fullSitePath, inSiteFolder, sizeof(fullSitePath) );
        strcat( fullSitePath, "/" );
        strncat( fullSitePath, dirIndexName, sizeof(fullSitePath) - strlen( fullSitePath ) );
        // Use the first index file of type text/html. Should fix this to use mime type
        if ((strlen( dirIndexName ) > 5 && !strcmp( &(dirIndexName[strlen( dirIndexName ) - 5]), ".html" ))
            || (strlen( dirIndexName ) > 4 && !strcmp( &(dirIndexName[strlen( dirIndexName ) - 4]), ".htm" ))) {
            char* titleStr = extractHTMLTitle( fullSitePath, cmd );
            if (!titleStr || !strcmp( titleStr, "" )) {
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, cmd->server,
                    "%s Index file %s has no title.",
                    MSG_PREFIX, fullSitePath );
                *outTitle = NULL;
                return;
            }
            // Title found by extractHTMLTitle could be way longer than what we want to use for Rendezvous; truncate.
            strncpy( outTitle, titleStr, TITLE_MAX );
            return;
        }
    }
    *outTitle = NULL;
}


/* 
* Compare user's index file with the user template.
* Returns true only if able to confirm that they differ.
*/
static Boolean	userHasValidCustomizedSite( char* inUserName, cmd_parms* cmd ) {
    char	site_path[PATH_MAX+1];
    int		filesDiffer;
    int 	userIndexFD;
    struct stat userIndexFinfo;
    void*	userIndexMM;
#define TEMPLATE_PATH "/System/Library/User Template/English.lproj/Sites/index.html"

	if (templateIndexMM == 0 || templateIndexMM == (void*) -1 ) {
        if (stat( TEMPLATE_PATH, &templateIndexFinfo ) == -1) {
            ap_log_error( APLOG_MARK, APLOG_WARNING, cmd->server,
                "%s Skipping user '%s' - cannot stat template index file '%s'.",
                MSG_PREFIX, inUserName, TEMPLATE_PATH );
            return FALSE;
        }
        templateIndexFD = open( TEMPLATE_PATH, O_RDONLY, 0 );
        templateIndexMM = mmap( NULL, templateIndexFinfo.st_size, PROT_READ, MAP_SHARED, templateIndexFD, 0 );
        if ( templateIndexMM == (void*) -1) {
            ap_log_error( APLOG_MARK, APLOG_WARNING, cmd->server,
                "%s Skipping user '%s' - cannot read template index file '%s'.",
                MSG_PREFIX, inUserName, TEMPLATE_PATH );
            close( userIndexFD );
            return FALSE;
        }
    }

    if (getUserSitePath( inUserName, &site_path, cmd )) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Skipping user '%s' - unable to confirm userdir site.",
            MSG_PREFIX, inUserName );
        return FALSE;
    }
    strcat( site_path, "/index.html" );
	if (stat( site_path, &userIndexFinfo ) == -1) {
        ap_log_error( APLOG_MARK,  - APLOG_NOERRNO|APLOG_WARNING, cmd->server,
            "%s Skipping user '%s' - cannot read index file '%s'.",
            MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }
    if ((userIndexFinfo.st_mode & S_IFMT) != S_IFREG) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, cmd->server,
            "%s Skipping user '%s' - index file isn't a regular file.",
            MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }
    if (userIndexFinfo.st_size != templateIndexFinfo.st_size) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server,
            "%s Concluding user '%s' has customized site - sizes differ %qd vs %qd.",
            MSG_PREFIX, inUserName, userIndexFinfo.st_size, templateIndexFinfo.st_size );
        return TRUE;
    }

    userIndexFD = open( site_path, O_RDONLY, 0 );
	if (userIndexFD == -1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, cmd->server,
            "%s Skipping user '%s' - cannot open index file '%s'.",
             MSG_PREFIX, inUserName, site_path );
        return FALSE;
    }

    userIndexMM = mmap( NULL, userIndexFinfo.st_size, PROT_READ, MAP_SHARED, userIndexFD, 0);
	if ( userIndexMM == (void*) -1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, cmd->server,
            "%s Skipping user '%s' - cannot read index file '%s'.",
             MSG_PREFIX, inUserName, site_path );
        close( userIndexFD );
        return FALSE;
    }
    filesDiffer = memcmp( templateIndexMM, userIndexMM, userIndexFinfo.st_size );

    munmap( userIndexMM, userIndexFinfo.st_size );
    close( userIndexFD );
    if (filesDiffer != 0) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server,
            "%s Concluding user '%s' has customized site - contents differ.",
            MSG_PREFIX, inUserName );
        return TRUE;
    }
    return FALSE;
}

/*
* Register the sites folder for specified user.
*/
static void registerUser( char* inUserName, char* inRegNameFormat, int inPort, cmd_parms* cmd ) {
    char		txt[TXT_MAX+1];
    char		site_path[PATH_MAX+1];
    char		title[TITLE_MAX+1];
    char		regName[REGNAME_MAX+1];
    struct passwd*	pw = getpwnam( inUserName );

    if (!pw) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
        "%s Skipping user '%s' - no pw entry.",
        MSG_PREFIX, inUserName );
        return;
    }
        
    if ((int)pw->pw_uid < 100) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Skipping user '%s' - uid '%d' indicates system user.",
            MSG_PREFIX, pw->pw_name, pw->pw_uid );
        return;
    }
    
    if (getUserSitePath( inUserName, &site_path, cmd )) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, cmd->server,
            "%s Skipping user '%s' - unable to confirm userdir site.",
            MSG_PREFIX, pw->pw_name );
        return;
    }
    
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server,
	"%s Confirmed user '%s' has existing userdir site directory '%s'.",
	MSG_PREFIX, pw->pw_name, site_path );
    
    snprintf( txt, sizeof(txt), "path=/~%s/", inUserName );
	if (!strcasecmp( inRegNameFormat, "longname" ))
        // On X, pw_gecos always contains the long name.
        strncpy( regName, pw->pw_gecos, sizeof(regName) );
	else if (!strcasecmp( inRegNameFormat, "title" )) {
        getTitle( site_path, &title, cmd );
        if (title && strcmp( title, ""  ))
            strncpy( regName, title, sizeof(regName) );
        else
            strncpy( regName, pw->pw_gecos, sizeof(regName) );
    }
	else if (!strcasecmp( inRegNameFormat, "longname-title" )) {
        getTitle( site_path, &title, cmd );
        if (title && strcmp( title, ""  ))
            snprintf( regName, sizeof(regName), "%s - %s", pw->pw_gecos, title );
        else
            strncpy( regName, pw->pw_gecos, sizeof(regName) );
    }
	else {
        // Format string
        int len = strlen(inRegNameFormat);
        int i;
        int j = 0;
        char uidStr[8];
        CFStringRef hostRef;
        CFStringEncoding enc;
        char hostStr[65];

        bzero( regName, sizeof(regName) );
        for (i = 0; i < len; i++) {
            if (inRegNameFormat[i] == '%') {
                switch (inRegNameFormat[i + 1]) {
                    case 't':	// %t - HTML title
                        getTitle( site_path, &title, cmd );
                        if (title && strcmp( title, "" )) {
                            strncat( regName, title, sizeof(regName) );
                            j = j + strlen( title );
                        }
                        i++;
                        break;
                    case 'l':	// %l - long name
                        strncat( regName, pw->pw_gecos, sizeof(regName) );
                        j = j + strlen( pw->pw_gecos );
                        i++;
                        break;
                    case 'n':	// %n - short name
                        strncat( regName, pw->pw_name, sizeof(regName) );
                        j = j + strlen( pw->pw_name );
                        i++;
                        break;
                    case 'u':	// %u - uid
                        sprintf( uidStr, "%d", pw->pw_uid );
                        strncat( regName, uidStr, sizeof(regName) );
                        j = j + strlen( uidStr );
                        i++;
                        break;
                    case 'c':	// %c - computer name
                        hostRef = SCDynamicStoreCopyComputerName( NULL, NULL );
                        CFStringGetCString( hostRef, hostStr, sizeof(hostStr), enc ); 
                        strncat( regName, hostStr, sizeof(regName) );
                        j = j + strlen( hostStr );
                        i++;
                        break;
                    default:
                        regName[j++] = inRegNameFormat[i];
                }
            }
            else
                regName[j++] = inRegNameFormat[i];
        }
    }

    registerService( regName, inPort, txt, cmd->server );
    return;
}

/* Find the local Directory node and call processUsers
*/
static void registerUsers( char* whichUsers, char* regNameFormat, int port, cmd_parms *cmd ) {

    tDirStatus dsStatus;
    tDirReference dirRef;
    tDirNodeReference defaultLocalNodeRef;

    dsStatus = dsOpenDirService( &dirRef );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
            "%s Unable to open Directory Services (error = %d).", MSG_PREFIX, dsStatus );
        return;
    }
    
    dsStatus = getDefaultLocalNode( dirRef, &defaultLocalNodeRef );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                "%s Unable to open Directory Services local node (error = %d).", MSG_PREFIX, dsStatus );
        (void)dsCloseDirService( dirRef );
        return;
    }
    
    dsStatus = processUsers( dirRef, defaultLocalNodeRef, whichUsers, regNameFormat, port, cmd );
    if (dsStatus != eDSNoErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
                "%s Unable to process all users (error = %d).", MSG_PREFIX, dsStatus );
        (void)dsCloseDirService( dirRef );
        return;
    }

    (void)dsCloseDirNode( defaultLocalNodeRef );
    (void)dsCloseDirService( dirRef );
}

/*
 * Process the RegisterDefaultSite directive.
 * RegisterDefaultSite [port | main]
 */
static const char *processRegDefaultSite( cmd_parms *cmd, void *dummy, const char *arg ) {

    module_cfg_rec *module_cfg = getModConfig();
    if (module_cfg->initCount < 1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server, 
            "%s Skipping bootstrap config for default site; count=%d",
            MSG_PREFIX, module_cfg->initCount );
        return NULL;
    }
    if (module_cfg->globalErr) {
		ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server, 
            "%s Global error, unable to process RegisterDefaultSite directive", 
            MSG_PREFIX );
        return NULL;
    }
    
    int port = DEFAULT_HTTP_PORT;
    int	err = 0;

    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
    
    char* portArg = ap_getword_conf( cmd->pool, &arg );

    if (portArg && strcmp( portArg, "" )) {
        if (strlen( portArg ) > 5)
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );

        if (!strcasecmp( portArg, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( portArg, "%d", &port );
            if (!err)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
            if (port < 0 || port > PORT_MAX)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument out of range", NULL );
        }
    }

    if (!ap_configtestonly)
    	registerComputerName( cmd->server, port );

    return NULL;
}

/*
 * Process the RegisterUserSite directive.
 * RegisterUserSite username | all-users | customized-users [ longname | title | longname-title [port | main ]]
 */
static const char *processRegUserSite( cmd_parms *cmd, void *dummy, char *inName, char *inRegNameFormat, char *inPort ) {
    int port = DEFAULT_HTTP_PORT;
    int err = 0;
    
    module_cfg_rec *module_cfg = getModConfig();
    if (module_cfg->initCount < 1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server, 
            "%s Skipping bootstrap config for user sites; count=%d",
            MSG_PREFIX, module_cfg->initCount );
        return NULL;
    }
    if (module_cfg->globalErr) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server, 
            "%s Global error, unable to process RegisterUserSite directive", 
            MSG_PREFIX );
        return NULL;
    }
    
    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
    
    if (!inName || !strcmp( inName, "" )) {
	return ap_pstrcat( cmd->pool, MSG_PREFIX, "Name argument missing", NULL );
    }
    if (strlen( inName ) > MAXLOGNAME) {
	return ap_pstrcat( cmd->pool, MSG_PREFIX, "Name argument too long", NULL );
    }
    if (inRegNameFormat && strlen( inRegNameFormat ) > MAX_NAME_FORMAT) {
	return ap_pstrcat( cmd->pool, MSG_PREFIX, "Name format argument too long", NULL );
    }
    if (inPort && strcmp( inPort, "" )) {
        if (strlen( inPort ) > 5)
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );
        
        if (!strcasecmp( inPort, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( inPort, "%d", &port );
            if (!err)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
            if (port < 0 || port > PORT_MAX)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument out of range", NULL );
        }
    }
        
    if (!strcasecmp( inName, ALL_USERS ) || !strcasecmp( inName, CUSTOMIZED_USERS ) ) {
        if (!ap_configtestonly) {
            if (allUsersDone) {
                ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, cmd->server, 
                "%s Skipping redundant request to register multiple users", MSG_PREFIX );
            }
            else {
                if (inRegNameFormat)
                    registerUsers( inName, inRegNameFormat, port, cmd );
                else
                    registerUsers( inName, DEFAULT_NAME_FORMAT, port, cmd );
                allUsersDone = 1;
            }
        }
    }

    else // it's an individual user
        if (!ap_configtestonly)
            registerUser( inName, inRegNameFormat, port, cmd );
    return NULL;
}

/*
 * Process the RegisterResource directive.
 * RegisterResource name path [port | main]
 */
static const char *processRegResource( cmd_parms *cmd, void *dummy, char *inName, char *inPath, char* inPort ) {

    char txt[TXT_MAX+1];  
    int port = DEFAULT_HTTP_PORT;
    int err = 0;
    
    module_cfg_rec *module_cfg = getModConfig();
    if (module_cfg->initCount < 1) {
        ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, cmd->server, 
            "%s Skipping bootstrap config for resource; count=%d", 
            MSG_PREFIX, module_cfg->initCount );

        return NULL;
    }
    if (module_cfg->globalErr) {
	ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server, 
            "%s Global error, unable to process RegisterResource directive", 
            MSG_PREFIX );
        return NULL;
    }
        
    const char *errString = ap_check_cmd_context( cmd, GLOBAL_ONLY );
    if (errString != NULL) {
        return errString;
    }
    
    if (inName && strcmp(inName, "" ))
        if (strlen( inName ) > REGNAME_MAX)
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Name argument too long", NULL );
            
    if (inPath && strcmp(inPath, "" )) {
        if (strlen( inPath ) > PATH_MAX)
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Path argument too long to be a path", NULL );
        if (strlen( inPath ) > TXT_MAX - 6) // allow space for "path="
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Path argument too long to use with Rendezvous", NULL );
    }
    
    if (inPort && strcmp( inPort, "" )) {
        if (strlen( inPort ) > 5)
            return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument too long", NULL );

        if (!strcasecmp( inPort, "main" ))	// use port of main server
            port = cmd->server->port;
        else {
            err = sscanf( inPort, "%d", &port );
            if (!err)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument not 'main' or numeric", NULL );
            if (port < 0 || port > PORT_MAX)
                return ap_pstrcat( cmd->pool, MSG_PREFIX, "Port argument out of range", NULL );
        }
    }

    snprintf( txt, sizeof(txt), "path=%s", inPath );
    
    if (!ap_configtestonly)
        registerService( inName, port, txt, cmd->server );

    return NULL;
}

static const command_rec rendezvousModuleCmds[] = {
    {
        "RegisterDefaultSite",          /* directive name */
        processRegDefaultSite,       	/* config action routine */
        NULL,                           /* argument to include in call */
        RSRC_CONF,                      /* where available */
        RAW_ARGS,                      	/* arguments */
        "Optionally, specify a port or keyword main; defaults to 80" 	/* directive description */
    },
    {
        "RegisterUserSite",             /* directive name */
        processRegUserSite,          	/* config action routine */
        NULL,                           /* argument to include in call */
        RSRC_CONF,                      /* where available */
        TAKE123,                          /* arguments */
        "Specify a user name or the keyword all-users or customized-users, optionally followed by keyword longname, title, or longname-title, optionally followed by a port or keyword main; default is 80" /* directive description */
    },
    {
        "RegisterResource",             /* directive name */
        processRegResource,           	/* config action routine */
        NULL,                           /* argument to include in call */
        RSRC_CONF,                      /* where available */
        TAKE23,                          /* arguments */
        "Specify a name under which to register, and a path, optionally followed by a port or keyword main; default is 80" /* directive description */
    },

    {NULL}
};

static void registerComputerName( server_rec *serverData, int inPort ) {

    registerService( "", inPort, "", serverData);
    return;
}

/*
 * Initialization handler.
 */
static void rendezvousChildInit( server_rec *serverData, pool *p ) {
    return;
}

static void rendezvousModuleInit( server_rec *serverData, pool *p ) {
    module_cfg_rec *module_cfg = getModConfig();
    module_cfg->initCount++;    // This field is used to tell if we're in the bootstrap phase or really working
    ap_log_error( APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, serverData,
            "%s Module init count=%d pid=%d.", MSG_PREFIX, module_cfg->initCount, getpid());
    // Cleanup function is in config pool
    ap_register_cleanup( p, (void *)NULL, unregisterRefs, ap_null_cleanup );
    return;
}

static void *rendezvousModuleCreateServerConfig( pool *p, char *dirspec ) {
    module_cfg_rec *module_cfg;
    server_cfg_rec *server_cfg;
    pool *pPool;
    int err;

    module_cfg = ap_ctx_get( ap_global_ctx, "rendezvous_apple_module" );
    if (!module_cfg) {
        /*
         * Allocate an own subpool which survives server restarts
         */
        pPool = ap_make_sub_pool( NULL ); 
        module_cfg = (module_cfg_rec *)ap_palloc( pPool, sizeof(module_cfg_rec) );
        /*
         * Initialize per-module configuration
         */
        module_cfg->globalErr = 0;
        module_cfg->pPool = pPool;
        module_cfg->initCount = 0;
        module_cfg->replyRecPtrArray = ap_make_array( pPool, 0, sizeof(replyRec*) );
        err = pthread_mutex_init( &(module_cfg->threadMutex), NULL );
        if (err)
            module_cfg->globalErr = 1;
		err = pthread_cond_init( &module_cfg->cond, NULL );
		if (err)
			module_cfg->globalErr = 1;
		module_cfg->replyRunLoop = NULL;
		module_cfg->initialRegReplyData = NULL;
        ap_ctx_set( ap_global_ctx, "rendezvous_apple_module", module_cfg );

    }

    /*
     * Some of this is left over from mod_example
     */
    server_cfg = (server_cfg_rec *) ap_pcalloc( p, sizeof(server_cfg_rec) );
    server_cfg->local = 0;
    server_cfg->congenital = 0;
    server_cfg->cmode = CONFIG_MODE_SERVER;

    return (void *) server_cfg;
}

/*
 * Module dispatch table.
 */
module MODULE_VAR_EXPORT rendezvous_apple_module = {
        STANDARD_MODULE_STUFF,
        rendezvousModuleInit,                        /* initializer */
        rendezvousModuleCreateServerConfig,        /* dir config creater */
        NULL,                                   /* dir merger --- default is to override */
        NULL,                                   /* server config */
        NULL,                                   /* merge server config */
        rendezvousModuleCmds,                        /* command table */
        NULL,                                   /* handlers */
        NULL,                                   /* filename translation */
        NULL,                                   /* check user_id */
        NULL,                                   /* check auth */
        NULL,                                   /* check access */
        NULL,                                   /* type_checker */
        NULL,                                   /* fixups */
        NULL,                                   /* logger */
        NULL,                                   /* header parser */
        rendezvousChildInit,                         /* child_init */
        NULL,                                   /* child_exit */
        NULL                                    /* post read-request */
};


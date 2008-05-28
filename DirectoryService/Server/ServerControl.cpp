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
 * @header ServerControl
 */

#if DEBUG_SRVR
# include <stdio.h>			// for stderr, fprintf(), et al
#endif

#include "ServerControl.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "CHandlers.h"
#include "DSTCPListener.h"
#include "CMsgQueue.h"
#include "CRefTable.h"
#include "DSMutexSemaphore.h"
#include "DSCThread.h"
#include "CServerPlugin.h"
#include "CPluginHandler.h"
#include "CNodeList.h"
#include "CLog.h"
#include "CPluginConfig.h"
#include "SharedConsts.h"
#include "CFile.h"
#include "CAuditUtils.h"
#include "DSMachEndian.h"
#include "WorkstationService.h"
#include "Mbrd_MembershipResolver.h"
#include "DSLDAPUtils.h"
#include "CDSPluginUtils.h"

#include <mach/mach.h>
#include <mach/notify.h>
#include <sys/stat.h>							//used for mkdir and stat
#include <IOKit/pwr_mgt/IOPMLib.h>				//required for power management handling
#include <syslog.h>								// for syslog()
#include <time.h>								// for time
#include <bsm/libbsm.h>
#include <uuid/uuid.h>

// This is for MIG
extern "C" {
	#include "DirectoryServiceMIGServer.h"
	#include "DSlibinfoMIGServer.h"
	#include "DSmemberdMIGServer.h"
}

extern void LoggingTimerCallBack( CFRunLoopTimerRef timer, void *info );

//power management
extern void dsPMNotificationHandler ( void *refContext, io_service_t service, natural_t messageType, void *notificationID );
extern io_object_t		gPMDeregisterNotifier;
extern io_connect_t		gPMKernelPort;

//network change
extern void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument);
extern CFRunLoopRef		gPluginRunLoop;

extern CFAbsoluteTime	gSunsetTime;
extern dsBool			gLogAPICalls;
extern dsBool			gDebugLogging;
extern dsBool			gDSFWCSBPDebugLogging;
extern dsBool			gIgnoreSunsetTime;

extern	bool			gServerOS;
extern dsBool			gDSDebugMode;
extern CCachePlugin	   *gCacheNode;
extern dsBool			gDSLocalOnlyMode;
extern dsBool			gDSInstallDaemonMode;
extern DSEventSemaphore gKickCacheRequests;
extern mach_port_t		gMachMIGSet;

// ---------------------------------------------------------------------------
//	* Globals
//
// ---------------------------------------------------------------------------

CFRunLoopTimerRef	ServerControl::fNSPCTimerRef = NULL; //Node Search Policy Changed
CFRunLoopTimerRef	ServerControl::fSPCNTimerRef = NULL; //Search Policy Changed Notify

UInt32					gAPICallCount		= 0;
UInt32					gLookupAPICallCount	= 0;
ServerControl		   *gSrvrCntl			= nil;
CRefTable			   *gRefTable			= nil;
CPlugInList			   *gPlugins			= nil;
CMsgQueue              *gLibinfoQueue       = nil;
CPluginConfig		   *gPluginConfig		= nil;
CNodeList			   *gNodeList			= nil;
CPluginHandler		   *gPluginHandler		= nil;
char				   *gDSLocalFilePath	= nil;
UInt32					gLocalSessionCount	= 0;

DSMutexSemaphore	   *gTCPHandlerLock			= new DSMutexSemaphore("::gTCPHandlerLock");	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gPerformanceLoggingLock = new DSMutexSemaphore("::gPerformanceLoggingLock");	//mutex on manipulating performance logging matrix
DSMutexSemaphore	   *gLazyPluginLoadingLock	= new DSMutexSemaphore("::gLazyPluginLoadingLock");	//mutex on loading plugins lazily
DSMutexSemaphore	   *gHashAuthFailedMapLock  = new DSMutexSemaphore("::gHashAuthFailedMapLock");	//mutex on failed shadow hash login table
DSMutexSemaphore	   *gHashAuthFailedLocalMapLock = gHashAuthFailedMapLock; //same mutex but different name
DSMutexSemaphore	   *gMachThreadLock			= new DSMutexSemaphore("::gMachThreadLock");	//mutex on count of mig handler threads
DSMutexSemaphore	   *gTimerMutex				= new DSMutexSemaphore("::gTimerMutex");		//mutex for creating/deleting timers
DSMutexSemaphore	   *gLibinfoQueueLock		= new DSMutexSemaphore("::gLibinfoQueueLock");	//mutex for creating Libinfo queues

UInt32					gDaemonPID;
UInt32					gDaemonIPAddress;
UInt32					gRefCountWarningLimit						= 500;
UInt32					gDelayFailedLocalAuthReturnsDeltaInSeconds  = 1;
UInt32					gMaxHandlerThreadCount						= kMaxHandlerThreads;
dsBool					gToggleDebugging							= false;
map<mach_port_t, pid_t>	gPIDMachMap;
char				   *gNIHierarchyTagString   = nil;
UInt32					gActiveMachThreads							= 0;
UInt32					gActiveLongRequests							= 0;
bool					gFirstNetworkUpAtBoot						= false;
bool					gNetInfoPluginIsLoaded						= false;

//eDSLookupProcedureNumber MUST be SYNC'ed with lookupProcedures
const char *lookupProcedures[] =
{
    "firstprocnum",
    
    // Users
    "getpwnam",
    "getpwuuid",
    "getpwuid",
    "getpwent",
    
    // Groups
    "getgrnam",
    "getgruuid",
    "getgrgid",
    "getgrent",
    
    // Services
    "getservbyname",
    "getservbyport",
    "getservent",
    
    // Protocols
    "getprotobyname",
    "getprotobynumber",
    "getprotoent",
    
    // RPCs
    "getrpcbyname",
    "getrpcbynumber",
    "getrpcent",
    
    // Mounts
    "getfsbyname",
    "getfsent",
    
    // Printers -- deprecated
//    "prdb_getbyname",
//    "prdb_get",
    
    // bootparams -- deprecated
//    "bootparams_getbyname",
//    "bootparams_getent",
    
    // Aliases
    "alias_getbyname",
    "alias_getent",
    
    // Networks
    "getnetent",
    "getnetbyname",
    "getnetbyaddr",
    
    // Bootp -- deprecated
//    "bootp_getbyip",
//    "bootp_getbyether",
    
    // NetGroups
    "innetgr",
    "getnetgrent",
    
    // DNS calls
    "getaddrinfo",
    "getnameinfo",
    "gethostbyname",
    "gethostbyaddr",
    "gethostent",
    "getmacbyname",
    "gethostbymac",
	
	"getbootpbyhw",
	"getbootpbyaddr",

	// other calls
    "dns_proxy",
    "_flushcache",
    "_flushentry",
	
    "lastprocnum",
    NULL				// safety in case we get out of sync
};

__BEGIN_DECLS
extern int ShouldRegisterWorkstation(void);
__END_DECLS

void DoPeriodicTask(CFRunLoopTimerRef timer, void *info);

static void NodeSearchPolicyChangeCallback(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->DoNodeSearchPolicyChange();
	}
}// DoSearchPolicyChange

static void SearchPolicyChangedNotifyCallback( CFRunLoopTimerRef timer, void *info )
{
	if ( info != nil )
	{
		((ServerControl *)info)->DoSearchPolicyChangedNotify();
	}
}

static CFStringRef SearchPolicyChangeCopyStringCallback( const void *item )
{
	return CFSTR("SearchPolicyChange");
}

static CFStringRef NotifySearchPolicyChangeCopyStringCallback( const void *item )
{
	return CFSTR("NotifySearchPolicyChange");
}

static CFStringRef PeriodicTaskCopyStringCallback( const void *item )
{
	return CFSTR("PeriodicTask");
}

#pragma mark -
#pragma mark MIG Support Routines - separate DS, Lookup, and memberd servers
#pragma mark -

void mig_spawnonceifnecessary( void )
{
	// need to lock while checking cause we could get a race condition
	gMachThreadLock->WaitLock();

	// see if we've reached our limit and if we don't have enought threads to handle requests
	bool bSpawnThread = ( gActiveMachThreads < gMaxHandlerThreadCount && gActiveLongRequests > gActiveMachThreads );

	gMachThreadLock->SignalLock();

	if( bSpawnThread )
	{
		CMigHandlerThread* aMigHandlerThread = new CMigHandlerThread(DSCThread::kTSMigHandlerThread, true);
		if (aMigHandlerThread != NULL)
			aMigHandlerThread->StartThread();
		//we don't keep a handle to the mig handler threads and don't check if they get created
	}
}

#pragma mark -
#pragma mark MIG Call Handler Routines - separate DS, Lookup, and memberd servers
#pragma mark -

kern_return_t dsmig_do_checkUsernameAndPassword( mach_port_t server,
												 sStringPtr username,
												 sStringPtr password,
												 int32_t *result,
												 audit_token_t atoken )
{
	CRequestHandler handler;
	char *debugDataTag = NULL;
	
	gMachThreadLock->WaitLock();
	gActiveLongRequests++;
	gMachThreadLock->SignalLock();

	// we should spawn the thread after we've incremented the number of requests active, otherwise thread will spawn and exit too soon
	mig_spawnonceifnecessary();

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		pid_t	aPID;
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "checkpw()", "Server" );
		DbgLog( kLogHandler, "%s : dsmig DAC : Username = %s", debugDataTag, username );
	}

#if USE_BSM_AUDIT
	uid_t			auidp;
	uid_t			euidp;
	gid_t			egidp;
	uid_t			ruidp;
	gid_t			rgidp;
	pid_t			pidp;
	au_asid_t		asidp;
	au_tid_t		tidp;
	audit_token_to_au32( atoken, &auidp, &euidp, &egidp, &ruidp, &rgidp, &pidp, &asidp, &tidp );
	char *textStr = nil;
	UInt32 bsmEventCode = AuditForThisEvent( kCheckUserNameAndPassword, username, &textStr );
#endif
		
	*result = handler.DoCheckUserNameAndPassword( username, password, eDSExact, NULL, NULL );
	
#if USE_BSM_AUDIT
	// BSM Audit
	if ( bsmEventCode > 0 )
	{
		token_t *tok;
		
		if ( *result == eDSNoErr )
		{
			tok = au_to_text( textStr );
			audit_write_success( bsmEventCode, tok,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
		else
		{
			audit_write_failure( bsmEventCode, textStr, (int)*result,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
	}
	DSFreeString( textStr );	// sets to NULL; required
#endif

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : dsmig DAR : Username %s : Result code = %d", debugDataTag, username, *result );
		free( debugDataTag );
	}

	gMachThreadLock->WaitLock();
	gActiveLongRequests--;
	gMachThreadLock->SignalLock();
	
	return KERN_SUCCESS;
}

kern_return_t dsmig_do_create_api_session( mach_port_t server, mach_port_t *newServer, audit_token_t atoken )
{
	mach_port_t		oldTargetOfNotification	= MACH_PORT_NULL;
	
	(void) mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, newServer );
	(void) mach_port_move_member( mach_task_self(), *newServer, gMachMIGSet );
	
	// Request no-senders notification so we can tell when server dies
	(void) mach_port_request_notification( mach_task_self(), *newServer, MACH_NOTIFY_NO_SENDERS, 1, *newServer, MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldTargetOfNotification );
	
	// let's get the audit data PID
	pid_t	aPID;
	audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

	gMachThreadLock->WaitLock();
	gPIDMachMap[*newServer] = aPID;
	gMachThreadLock->SignalLock();

	return KERN_SUCCESS;
}

kern_return_t dsmig_do_api_call( mach_port_t server,
								 mach_msg_type_name_t serverPoly,
								 sComDataPtr msg_data,
								 mach_msg_type_number_t msg_dataCnt,
								 vm_offset_t msg_data_ool,
								 mach_msg_type_number_t msg_data_oolCnt,
								 sComDataPtr reply_msg,
								 mach_msg_type_number_t *reply_msgCnt,
								 vm_offset_t *reply_msg_ool,
								 mach_msg_type_number_t *reply_msg_oolCnt,
								 audit_token_t atoken )
{
	kern_return_t	kr			= KERN_FAILURE;
	sComDataPtr		pComData	= NULL;
	UInt32			uiLength	= 0;
	UInt32			dataLength	= 0;
	UInt32			dataSize	= 0;
	
	if( msg_dataCnt )
	{
		pComData	= (sComDataPtr) msg_data;
		uiLength	= msg_dataCnt;
		dataLength	= pComData->fDataLength;
		dataSize	= pComData->fDataSize;
#ifdef __LITTLE_ENDIAN__
		if (pComData->type.msgt_translate == 1) //need to swap data length if mach msg came from BIG_ENDIAN translator
		{
			dataLength	= DSGetLong(&pComData->fDataLength, false);
			dataSize	= DSGetLong(&pComData->fDataSize, false);
		}
#endif
	}
	else
	{
		pComData	= (sComDataPtr) msg_data_ool;
		uiLength	= msg_data_oolCnt;
		dataLength	= pComData->fDataLength;
		dataSize	= pComData->fDataSize;
#ifdef __LITTLE_ENDIAN__
		if (pComData->type.msgt_translate == 1) //need to swap data length if mach msg came from BIG_ENDIAN translator
		{
			dataLength	= DSGetLong(&pComData->fDataLength, false);
			dataSize	= DSGetLong(&pComData->fDataSize, false);
		}
#endif
	}
	
	// lets see if the packet is big enough.. and see if the length matches the msg_data size minus 1
	if( uiLength >= (sizeof(sComData) - 1) )
	{
		if( dataLength == (uiLength - (sizeof(sComData) - 1)) )
		{
			// we need to copy because we will allocate/deallocate it in the handler
			//   but based on the size it thinks it is
			sComData *pRequest = (sComData *) calloc( sizeof(sComData) + dataSize, 1 );
			if ( pRequest == NULL )
				return KERN_MEMORY_ERROR;
			
			CRequestHandler handler;
			double reqStartTime = 0;
			double reqEndTime = 0;
			
			bcopy( (void *)pComData, pRequest, uiLength );
			
			gMachThreadLock->WaitLock();
			gActiveLongRequests ++;
			gMachThreadLock->SignalLock();
			
			// spawn a thread request
			mig_spawnonceifnecessary();

#ifdef __LITTLE_ENDIAN__
			if (pRequest->type.msgt_translate == 1) //need to swap since mach msg came from BIG_ENDIAN translator
			{
				DSMachEndian swapper(pRequest, kDSSwapToHost);
				swapper.SwapMessage();
			}
#endif

			// let's get the audit data and add it to the sComData
			audit_token_to_au32( atoken, NULL, (uid_t *)&pRequest->fEffectiveUID, NULL, (uid_t *)&pRequest->fUID, NULL, (pid_t *)&pRequest->fPID, NULL, NULL );
			
			if ( (gDebugLogging) || (gLogAPICalls) )
			{
				reqStartTime = dsTimestamp();
			}
			
			handler.HandleRequest( &pRequest );
			
			if ( (gDebugLogging) || (gLogAPICalls) )
			{
				pid_t	aPID;
				audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

				reqEndTime = dsTimestamp();

				// if the request took more than 2 seconds, log a message
				double totalTime = (reqEndTime - reqStartTime) / USEC_PER_SEC;
				if (totalTime > 2.0)
				{
					char *debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "API", "Server" );
					DbgLog( kLogHandler, "%s : dsmig DAR : Excessive request time %f seconds", debugDataTag, totalTime );
					free( debugDataTag );
				}
			}
			
			gMachThreadLock->WaitLock();
			gActiveLongRequests --;
			gMachThreadLock->SignalLock();
			
			// set the PID in the return to our PID for RefTable purposes
			pRequest->fPID = gDaemonPID;
			
			UInt32 dataLen = pRequest->fDataLength;
			
#ifdef __LITTLE_ENDIAN__
			if (pRequest->type.msgt_translate == 1) //need to swap since mach msg being sent back to BIG_ENDIAN translator
			{
				DSMachEndian swapper(pRequest, kDSSwapToBig);
				swapper.SwapMessage();
			}
#endif

			// if it will fit in the fixed buffer, use it otherwise use OOL
			if( sizeof(sComData) + dataLen <= *reply_msgCnt )
			{
				*reply_msgCnt = sizeof(sComData) + dataLen - 1;
				bcopy( pRequest, reply_msg, *reply_msgCnt );
				*reply_msg_oolCnt = 0;
			}
			else
			{
				*reply_msgCnt = 0; // ool, set the other to 0
				vm_read( mach_task_self(), (vm_address_t)pRequest, (sizeof(sComData) + dataLen - 1), reply_msg_ool, reply_msg_oolCnt );
			}

			// free our allocated request data...
			free( pRequest );
			pRequest = NULL;
			
			gAPICallCount++;
			
			if (gLogAPICalls)
			{
				if ( (gAPICallCount % 1023) == 1023 ) // every 1023 calls so we can do bit-wise check
				{
					syslog(LOG_CRIT,"API clients have called APIs %d times", gAPICallCount);
				}
			}

			kr = KERN_SUCCESS;
		}
		else
		{
			syslog( LOG_ALERT, "dsmig_do_api_call:  Bad message size %d, does not correlate with contents length %d + header %d", uiLength, dataLength, (sizeof(sComData) - 1) );
		}
	}
	else
	{
		syslog( LOG_ALERT, "dsmig_do_api_call message is too small to be valid message %d < %d", uiLength, sizeof(sComData) - 1 );
	}
	
	if( msg_data_oolCnt )
	{
		vm_deallocate( mach_task_self(), msg_data_ool, msg_data_oolCnt );
	}
	
	return kr;
}

kern_return_t libinfoDSmig_do_GetProcedureNumber
										(	mach_port_t server,
											char* indata,
											int *procnumber,
											audit_token_t atoken )
{
	kern_return_t	kr				= KERN_FAILURE;
	char			*debugDataTag	= NULL;

	// NOTE:  All MIG calls are Endian swapped automatically, nothing required on the client/server side
	
	// We won't spawn a thread, but we will increase our count while we do work so that existing thread won't exit
	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		pid_t			aPID;
		CRequestHandler handler;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
		DbgLog( kLogHandler, "%s : libinfomig DAC : Procedure Request = %s", debugDataTag, indata );
	}
	
	*procnumber = 0;
	if (indata != NULL)
	{
		for (int idx = 1; idx < (int)kDSLUlastprocnum && lookupProcedures[idx] != NULL; idx++)
		{
			if ( 0 == strcmp(indata, lookupProcedures[idx]) )
			{
				*procnumber = idx;
				kr = KERN_SUCCESS;
				break;
			}
		}
	}

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : libinfomig DAR : Procedure = %s (%d) : Result code = %d", debugDataTag, indata,
				*procnumber, kr );
		
		free( debugDataTag );
	}
	
	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
				
	return kr;
}

kern_return_t libinfoDSmig_do_Query(	mach_port_t server,
										int32_t procnumber,
										inline_data_t request,
										mach_msg_type_number_t requestCnt,
										inline_data_t reply,
										mach_msg_type_number_t *replyCnt,
										vm_offset_t *ooreply,
										mach_msg_type_number_t *ooreplyCnt,
										audit_token_t atoken )
{
	kern_return_t	kr				= KERN_FAILURE;
	kvbuf_t		   *returnedBuf		= NULL;
	Boolean			bValidProcedure	= ( (procnumber > 0) && (procnumber < (int)kDSLUlastprocnum) ? TRUE : FALSE);
	char			*debugDataTag	= NULL;
    pid_t			aPID;
	
	// Note: MIG deals with all byte swapping automatically no need to swap int32_t or any other standard integer type
	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	// spawn a thread request
	mig_spawnonceifnecessary();

    audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
    
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;

		if ( bValidProcedure )
		{
			debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
			DbgLog( kLogHandler, "%s : libinfomig DAC : Procedure = %s (%d)", debugDataTag, lookupProcedures[procnumber], procnumber );
            
            if( aPID == (SInt32)gDaemonPID )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAC : Dispatching from/to ourself", debugDataTag, lookupProcedures[procnumber],
                        procnumber );
            }
		}
		else
		{
			debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
			DbgLog( kLogHandler, "%s : libinfomig DAC : Invalid Procedure = %d", debugDataTag, procnumber );
		}
	}
    
    // if the cache node is NULL and (pid != DS or DNS lookup) wait for cache node
    if( gCacheNode == NULL && (aPID != (SInt32)gDaemonPID || (procnumber >= kDSLUgetaddrinfo && procnumber <= kDSLUdns_proxy )) )
    {
        gKickCacheRequests.WaitForEvent(); // wait until cache node is ready
    }
	
	//pass the data directly into the Cache plugin and only if the SearchNode is initialized
    // or the search node is not initialized and we aren't dispatching to ourselves
	if ( gCacheNode != NULL && bValidProcedure )
	{
        returnedBuf = gCacheNode->ProcessLookupRequest(procnumber, request, requestCnt, aPID);
        if ( (returnedBuf != NULL) && (returnedBuf->databuf != NULL) )
        {
            // if it will fit in the fixed buffer, use it otherwise use OOL
            if( returnedBuf->datalen <= *replyCnt )
            {
                *replyCnt = returnedBuf->datalen;
                bcopy( returnedBuf->databuf, reply, returnedBuf->datalen );
                *ooreplyCnt = 0; // inline set OOL to 0
            }
            else
            {
                vm_read( mach_task_self(), (vm_address_t)(returnedBuf->databuf), returnedBuf->datalen, ooreply, 
                         ooreplyCnt );
                *replyCnt = 0; // ool, set the other to 0
            }
        }
        else
        {
            *replyCnt = 0;
            *ooreplyCnt = 0;
        }
        
		kr = KERN_SUCCESS;
		
        kvbuf_free(returnedBuf);
	}

	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
	
	if ( debugDataTag )
	{
		if ( bValidProcedure )
		{
			DbgLog( kLogHandler, "%s : libinfomig DAR : Procedure = %s (%d) : Result code = %d", debugDataTag, 
					lookupProcedures[procnumber], procnumber, kr );
		}
		else
		{
			DbgLog( kLogHandler, "%s : libinfomig DAR : Invalid Procedure = %d", debugDataTag, procnumber );
		}

		free( debugDataTag );
	}

	return kr;
}

kern_return_t libinfoDSmig_do_Query_async(	mach_port_t server,
                                            mach_port_t replyToPort,
                                            int32_t procnumber,
                                            inline_data_t request,
                                            mach_msg_type_number_t requestCnt,
                                            mach_vm_address_t callbackAddr,
                                            audit_token_t atoken )
{
    Boolean			bValidProcedure	= ( (procnumber > 0) && (procnumber < (int)kDSLUlastprocnum) ? TRUE : FALSE);
    char			*debugDataTag	= NULL;
    sLibinfoRequest		*pLibinfoRequest = NULL;
    
    if ( (gDebugLogging) || (gLogAPICalls) )
    {
        pid_t			aPID;
        CRequestHandler handler;
        
        audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
        
        if ( bValidProcedure )
        {
            debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
            DbgLog( kLogHandler, "%s : libinfomig DAC : Async Procedure = %s (%d)", debugDataTag, lookupProcedures[procnumber], procnumber );
            
            if( aPID == (pid_t)gDaemonPID )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAC : Dispatching from/to ourself", debugDataTag, lookupProcedures[procnumber],
                        procnumber );
            }
        }
        else
        {
            debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
            DbgLog( kLogHandler, "%s : libinfomig DAC : Invalid Async Procedure = %d", debugDataTag, procnumber );
        }
    }
    
    if( bValidProcedure )
    {
        pLibinfoRequest = new sLibinfoRequest;
        
        pLibinfoRequest->fBuffer = (char *) calloc( requestCnt, sizeof(char) );
        if( pLibinfoRequest->fBuffer != NULL )
        {
            pLibinfoRequest->fReplyPort = replyToPort;
            pLibinfoRequest->fProcedure = procnumber;
            pLibinfoRequest->fToken = atoken;
            pLibinfoRequest->fCallbackAddr = callbackAddr;
            
            bcopy( request, pLibinfoRequest->fBuffer, requestCnt );
            pLibinfoRequest->fBufferLen = requestCnt;
            
            if( gLibinfoQueue->QueueMessage( pLibinfoRequest ) == false )
            {
                // use our lookup mutex here as well
                gLibinfoQueueLock->WaitLock();
                gSrvrCntl->StartAHandler( DSCThread::kTSLibinfoQueueThread );
                gLibinfoQueueLock->SignalLock();
            }
        }
        else
        {
            // need to dispose of rights if we aren't going to queue the message
            mach_port_mod_refs( mach_task_self(), replyToPort, MACH_PORT_RIGHT_SEND_ONCE, -1 );
        }
    }
    
    if ( debugDataTag )
    {
        if ( bValidProcedure )
        {
            if( pLibinfoRequest != NULL )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d) : Request %X queued", debugDataTag, 
                        lookupProcedures[procnumber], procnumber, pLibinfoRequest );
            }
            else
            {
                DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d) : Request not queued", debugDataTag, 
                        lookupProcedures[procnumber], procnumber );
            }
        }
        else
        {
            DbgLog( kLogHandler, "%s : libinfomig DAR : Invalid Procedure = %d", debugDataTag, procnumber );
        }
        
        free( debugDataTag );
    }
    
    return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_MembershipCall( mach_port_t server, kauth_identity_extlookup *request, audit_token_t *atoken )
{
	char			*debugDataTag	= NULL;
	
	// mig calls all use seqno as a byte order field, so check if we need to swap
	int needsSwap = (request->el_seqno != 1) && (request->el_seqno == ntohl(1));
	
	// Note: MIG deals with all byte swapping automatically no need to swap int32_t or any other standard integer type
	// if defined that way BUT request is uint_8 array
	if (needsSwap)
		Mbrd_SwapRequest(request);

	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	// spawn a thread request
	mig_spawnonceifnecessary();
    
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "MembershipCall", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC %s", debugDataTag, (needsSwap ? " : Via Rosetta" : "") );
		
		if( aPID == (SInt32)gDaemonPID )
		{
			DbgLog( kLogHandler, "%s : mbrmig DAC : Dispatching from/to ourself", debugDataTag );
		}
	}
	
	Mbrd_ProcessLookup( request );

	if (needsSwap)
		Mbrd_SwapRequest(request);

	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR", debugDataTag );
		free( debugDataTag );
	}

	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_GetStats(mach_port_t server, StatBlock *stats)
{
	Mbrd_ProcessGetStats( stats );
	
	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_ClearStats(mach_port_t server, audit_token_t atoken)
{
	char *debugDataTag = NULL;

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "ClearStats", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC", debugDataTag );
	}
	
	Mbrd_ProcessResetStats();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR", debugDataTag );
		free( debugDataTag );
	}
	
	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_MapName(mach_port_t server, uint8_t isUser, mstring name, guid_t *guid, audit_token_t *atoken)
{
	kern_return_t	result;
	char			*debugDataTag	= NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "MapName", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC : Name = %s : isUser = %d", debugDataTag, name, (int) isUser );
	}
	
	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	// spawn a thread request
	mig_spawnonceifnecessary();
    
	result = Mbrd_ProcessMapName(isUser, name, guid);

	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
	
	if ( debugDataTag )
	{
		if (result == KERN_SUCCESS)
		{
			char	uuid_string[37] = { 0, };
			
			uuid_unparse_upper( guid->g_guid, uuid_string );
			DbgLog( kLogHandler, "%s : mbrmig DAR : Name = %s : GUID = %s", debugDataTag, name, uuid_string );
		}
		else
		{
			DbgLog( kLogHandler, "%s : mbrmig DAR : Name = %s : Not found", debugDataTag, name );
		}
		
		free( debugDataTag );
	}	
	
	return result;
}

kern_return_t memberdDSmig_do_GetGroups(mach_port_t server, uint32_t uid, uint32_t* numGroups, GIDArray gids, audit_token_t *atoken)
{
	int		result;
	char	*debugDataTag = NULL;

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "GetGroups", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC : uid = %u", debugDataTag, uid );
	}

	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	// spawn a thread request
	mig_spawnonceifnecessary();
    
	result = Mbrd_ProcessGetGroups(uid, numGroups, gids);

	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR : Total groups = %u", debugDataTag, (*numGroups) );
		free( debugDataTag );
	}	
	
	return (kern_return_t)result;
}

kern_return_t memberdDSmig_do_GetAllGroups(mach_port_t server, uint32_t uid, uint32_t* numGroups, GIDList *gids, mach_msg_type_number_t *gidsCnt, 
										   audit_token_t *atoken)
{
	int		result;
	char	*debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "GetAllGroups", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC : uid = %u", debugDataTag, uid );
	}
	
	gMachThreadLock->WaitLock();
	gActiveLongRequests ++;
	gMachThreadLock->SignalLock();
	
	// spawn a thread request
	mig_spawnonceifnecessary();
	
	GIDList tempList = NULL;
	
	(*gids) = NULL;
	(*gidsCnt) = 0;
    
	result = Mbrd_ProcessGetAllGroups(uid, numGroups, &tempList);
	if ( result == KERN_SUCCESS && (*numGroups) > 0 && tempList != NULL )
	{
		result = vm_read( mach_task_self(), (vm_address_t) tempList, ((*numGroups) * sizeof(gid_t)), (vm_offset_t *) gids, gidsCnt );
		if ( result == KERN_SUCCESS )
		{
			// need to set the value to count not length of bytes
			(*gidsCnt) = (*numGroups);
		}
		
		DSFree( tempList );
	}
	
	gMachThreadLock->WaitLock();
	gActiveLongRequests --;
	gMachThreadLock->SignalLock();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR : Total groups = %u", debugDataTag, (*numGroups) );
		free( debugDataTag );
	}	
	
	return (kern_return_t)result;
}

kern_return_t memberdDSmig_do_ClearCache(mach_port_t server, audit_token_t atoken)
{
	char	*debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "ClearCache", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC", debugDataTag );
	}

	Mbrd_ProcessResetCache();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR", debugDataTag );
		free( debugDataTag );
	}		

	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_DumpState(mach_port_t server, audit_token_t atoken)
{
	char	*debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "DumpState", "Server" );
		DbgLog( kLogHandler, "%s : mbrmig DAC", debugDataTag );
	}
    
	Mbrd_ProcessDumpState();

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbrmig DAR", debugDataTag );
		free( debugDataTag );
	}		
	
	return KERN_SUCCESS;
}

#pragma mark -
#pragma mark ServerControl Routines
#pragma mark -

// ---------------------------------------------------------------------------
//	* ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::ServerControl ( void )
{
    gDaemonPID			= getpid();
	//we use a non valid IP Address of zero for ourselves
	gDaemonIPAddress	= 0;

	fTCPListener				= nil;
	fTCPHandlerThreadsCnt		= 0;
	fLibinfoHandlerThreadCnt    = 0;
	fSCDStore					= 0;	
	fPerformanceStatGatheringActive	= false; //default
	fMemberDaemonFlushCacheRequestCount	= 0;
	fTCPHandlers				= nil;
	
#ifdef BUILD_IN_PERFORMANCE
	fLastPluginCalled			= 0;
	fPerfTableNumPlugins		= 0;
	fPerfTable					= nil;
	
#if PERFORMANCE_STATS_ALWAYS_ON
	fPerformanceStatGatheringActive	= true;
#else
	fPerformanceStatGatheringActive	= false;
#endif
#endif

	if (gDSDebugMode)
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachDebugPortName, kCFStringEncodingUTF8 );
	}
	else if (gDSLocalOnlyMode)
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachLocalPortName, kCFStringEncodingUTF8 );
	}
	else
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachPortName, kCFStringEncodingUTF8 );
	}
	
	if (gDaemonPID > 100) //assumption here is that we crashed and restarted so say netowrk is already running ie. we are usually less than 50
	{
		gFirstNetworkUpAtBoot = true;
	}
    
} // ServerControl


// ---------------------------------------------------------------------------
//	* ~ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::~ServerControl ( void )
{
} // ~ServerControl


// ---------------------------------------------------------------------------
//	* StartUpServer ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::StartUpServer ( void )
{
	SInt32				result		= eDSNoErr;
	struct stat			statResult;
	try
	{
		if ( gNodeList == nil )
		{
			gNodeList = new CNodeList();
			if ( gNodeList == nil ) throw((SInt32)eMemoryAllocError);
		}

		if ( gRefTable == nil )
		{
			gRefTable = new CRefTable( CHandlerThread::RefDeallocProc );
			if ( gRefTable == nil ) throw( (SInt32)eMemoryAllocError );
		}
		
		// Let's initialize membership globals just in case we get called
		Mbrd_InitializeGlobals();

		if ( gPluginConfig == nil )
		{
			gPluginConfig = new CPluginConfig();
			if ( gPluginConfig == nil ) throw( (SInt32)eMemoryAllocError );
			gPluginConfig->Initialize();
		}
		
		//gMaxHandlerThreadCount may be discovered in the DS plist config file read by gPluginConfig above
		fTCPHandlers = (CHandlerThread **)calloc(gMaxHandlerThreadCount, sizeof(CHandlerThread *));
		fLibinfoHandlers = (CHandlerThread **)calloc(gMaxHandlerThreadCount, sizeof(CHandlerThread *));
		
		if ( gPlugins == nil )
		{
			gPlugins = new CPlugInList();
			if ( gPlugins == nil ) throw( (SInt32)eMemoryAllocError );
			gPlugins->ReadRecordTypeRestrictions();
		}

		if ( gLibinfoQueue == nil )
		{
			gLibinfoQueue = new CMsgQueue();
			if ( gLibinfoQueue == nil ) throw((SInt32)eMemoryAllocError);
		}
		
		CreateDebugPrefFileIfNecessary();

		if (::stat( "/Library/Preferences/DirectoryService/.DSLogAPIAtStart", &statResult ) == eDSNoErr)
		{
			gSunsetTime		= CFAbsoluteTimeGetCurrent() + 300;
			CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	kCFAllocatorDefault,
															gSunsetTime + 1,
															0,
															0,
															0,
															LoggingTimerCallBack,
															NULL );
			
			// this call this callback does not block the main runloop
			CFRunLoopAddTimer( CFRunLoopGetMain(), timer, kCFRunLoopDefaultMode );
			CFRelease( timer );
			timer = NULL;
			
			gLogAPICalls	= true;
			syslog(LOG_ALERT,"Logging of API Calls turned ON at Startup of DS Daemon.");
			if (!gDebugLogging)
			{
				gDebugLogging	= true;
				gSrvrCntl->ResetDebugging(); //ignore return status
				syslog(LOG_ALERT,"Debug Logging turned ON at Startup of DS Daemon.");
			}
		}

		// let's start the MIG listener
		CMigHandlerThread *migListener = new CMigHandlerThread( DSCThread::kTSMigHandlerThread, false );
		if ( migListener == nil ) throw((SInt32)eMemoryAllocError);
		migListener->StartThread();
		
		if (!gDSLocalOnlyMode && !gDSInstallDaemonMode)
		{
			// see if we need TCP too
			if ( 	(	(::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr) ||
						(gServerOS) ) &&
						(::stat( "/Library/Preferences/DirectoryService/.DSTCPNotListening", &statResult ) != eDSNoErr) )
			{
				// Start the TCP listener thread
				result = StartTCPListener(kDSDefaultListenPort);
				if ( result != eDSNoErr ) throw( result );
			}
		}

		if ( gPluginHandler == nil )
		{
			gPluginHandler = new CPluginHandler();
			if ( gPluginHandler == nil ) throw((SInt32)eMemoryAllocError);

			//this call could throw
			gPluginHandler->StartThread();
		}

		// now it's time to put up our kernel listener thread
		if (!gDSLocalOnlyMode)
		{
			kern_return_t kresult = syscall( SYS_identitysvc, KAUTH_EXTLOOKUP_REGISTER, 0 );
			if (kresult == 0)
			{
				// start the memberd kernel listener
				CMemberdKernelHandlerThread *aThread = new CMemberdKernelHandlerThread( DSCThread::kTSMemberdKernelHndlrThread );
				aThread->StartThread();
			}
			else
			{
				syslog(LOG_CRIT, "Got error %d trying to register with kernel\n", kresult);
			}
		}

		result = RegisterForSystemPower();
		if ( result != eDSNoErr ) throw( result );

		result = (SInt32)RegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );
		
		result = SetUpPeriodicTask();
		if ( result != eDSNoErr ) throw( result );
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // StartUpServer


// ---------------------------------------------------------------------------
//	* ShutDownServer ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::ShutDownServer ( void )
{
	SInt32				result		= eDSNoErr;
	UInt32				i			= 0;

	try
	{

		result = (SInt32)UnRegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );

//		result = UnRegisterForSystemPower();
//		if ( result != eDSNoErr ) throw( result );

		// MigListener is stopped by destroying the port set
		mach_port_destroy( mach_task_self(), gMachMIGSet );
		gMachMIGSet = MACH_PORT_NULL;
		
        gLibinfoQueueLock->WaitLock();
        for (i = 0; i < gMaxHandlerThreadCount; i++)
        {
            if (fLibinfoHandlers[ i ] != nil)
            {
                fLibinfoHandlers[ i ]->StopThread();
                fLibinfoHandlers[ i ] = nil;
            }
        }
        
        // this will broadcast to all, and since we stopped the threads, they will all close down
        gLibinfoQueue->ClearMsgQueue();

        gLibinfoQueueLock->SignalLock();

		//no need to delete the global objects as this process is going away and
		//we don't want to create a race condition on the threads dying that
		//could lead to a crash

		//no need to delete the global mutexes as this process is going away and
		//we don't want to create a reace condition on the threads dying that
		//could lead to a crash
		//delete(gTCPHandlerLock);
		//delete(gHandlerLock);
		//delete(gInternalHandlerLock);
		//delete(gCheckpwHandlerLock);
		//delete(gPerformanceLoggingLock);
		
		CLog::Deinitialize();
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // ShutDownServer


// ---------------------------------------------------------------------------
//	* StartTCPListener ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::StartTCPListener ( UInt32 inPort )
{
	SInt32	result	= eDSNoErr;

	try
	{
		fTCPListener = new DSTCPListener(inPort);
		if ( fTCPListener == nil ) throw((SInt32)eMemoryAllocError);

		//this call could throw
		fTCPListener->StartThread();
	}

	catch( SInt32 err )
	{
		result = err;
		DbgLog( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DbgLog( kLogApplication, "  Caught exception = %d.", err );
	}

	return( result );

} // StartTCPListener


// ---------------------------------------------------------------------------
//	* StopTCPListener ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::StopTCPListener ( void )
{
	SInt32	result	= eDSNoErr;

	try
	{
		if ( fTCPListener == nil ) throw((SInt32)eMemoryAllocError);

		//this call could throw
		fTCPListener->StopThread();
	}

	catch( SInt32 err )
	{
		result = err;
		DbgLog( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DbgLog( kLogApplication, "  Caught exception = %d.", err );
	}

	return( result );

} // StopTCPListener


// ---------------------------------------------------------------------------
//	* StartAHandler ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: StartAHandler ( const FourCharCode inThreadSignature )
{
	volatile	UInt32		iThread;
				SInt32		result	= eDSNoErr;

	try
	{
		// If we have less than the max handlers then we add one
		//decide from which set of handlers to start one

		if (inThreadSignature == DSCThread::kTSLibinfoQueueThread)
		{
			if ( (fLibinfoHandlerThreadCnt >= 0) && (fLibinfoHandlerThreadCnt < gMaxHandlerThreadCount) )
			{
				for (iThread = 0; iThread < gMaxHandlerThreadCount; iThread++)
				{
					if (fLibinfoHandlers[ iThread ] == nil)
					{
						// Start a handler thread
						fLibinfoHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSLibinfoQueueThread, iThread);
						if ( fLibinfoHandlers[ iThread ] == nil ) throw((SInt32)eMemoryAllocError);
                        
						fLibinfoHandlerThreadCnt++;
                        
						//this call could throw
						fLibinfoHandlers[ iThread ]->StartThread();
						break;
					}
				}
			}
		}
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // StartAHandler


//--------------------------------------------------------------------------------------------------
//	* WakeAHandler()
//
//--------------------------------------------------------------------------------------------------

void ServerControl:: WakeAHandler ( const FourCharCode inThreadSignature )
{
} // WakeAHandler


// ---------------------------------------------------------------------------
//	* StopAHandler ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: StopAHandler ( const FourCharCode inThreadSignature, UInt32 iThread, CHandlerThread *inThread )
{
	SInt32			result		= eDSNoErr;

	try
	{
//		DbgLog( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
//		DbgLog( kLogApplication, "StopAHandler: sig = %d and index = %d", inThreadSignature, iThread );
		if (inThreadSignature == DSCThread::kTSLibinfoQueueThread)
		{
			if ( (iThread >= 0) && (iThread < gMaxHandlerThreadCount) )
			{
				if (fLibinfoHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fLibinfoHandlers[ iThread ] = nil;
                    
					fLibinfoHandlerThreadCnt--;
				}
			}
		}        
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // StopAHandler


//--------------------------------------------------------------------------------------------------
//	* SleepAHandler(const FourCharCode inThreadSignature, UInt32 waitTime)
//
//--------------------------------------------------------------------------------------------------

void ServerControl:: SleepAHandler ( const FourCharCode inThreadSignature, UInt32 waitTime )
{
} // SleepAHandler


//--------------------------------------------------------------------------------------------------
//	* GetHandlerCount(const FourCharCode inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

UInt32 ServerControl::GetHandlerCount ( const FourCharCode inThreadSignature )
{
	if (inThreadSignature == DSCThread::kTSLibinfoQueueThread)
	{
		return fLibinfoHandlerThreadCnt;
	}

	return 0;
} // GetHandlerCount


// ---------------------------------------------------------------------------
//	* RegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: RegisterForNetworkChange ( void )
{
	SInt32				scdStatus			= eDSNoErr;
	CFStringRef			ipKey				= 0;	//ip changes key
	CFMutableArrayRef	notifyKeys			= 0;
	CFMutableArrayRef	notifyPatterns		= 0;
	SCDynamicStoreRef	store				= NULL;
    CFRunLoopSourceRef	rls					= NULL;
	
	DbgLog( kLogApplication, "RegisterForNetworkChange(): " );

	notifyKeys		= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
	notifyPatterns	= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
											
	// watch for IPv4 configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	CFArrayAppendValue(notifyKeys, ipKey);
	CFRelease(ipKey);
	
	// watch for IPv4 interface configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
	CFArrayAppendValue(notifyPatterns, ipKey);
	CFRelease(ipKey);

	// watch for IPv6 interface configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);
	CFArrayAppendValue(notifyPatterns, ipKey);
	CFRelease(ipKey);
	
	//not checking bool return
	store = SCDynamicStoreCreate(NULL, fServiceNameString, NetworkChangeCallBack, NULL);
	if (store != NULL && notifyKeys != NULL && notifyPatterns != NULL)
	{
		SCDynamicStoreSetNotificationKeys(store, notifyKeys, notifyPatterns);
		rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
		if (rls != NULL)
		{
			// we watch for network changes on our plugin loop so they can't block other notifications
			CFRunLoopAddSource(gPluginRunLoop, rls, kCFRunLoopDefaultMode);
			CFRelease(rls);
			rls = NULL;
		}
		else
		{
			syslog(LOG_ALERT,"Unable to add source to RunLoop for SystemConfiguration registration for Network Notification");
		}
	}
	else
	{
		syslog(LOG_ALERT,"Unable to create DirectoryService store for SystemConfiguration registration for Network Notification");
	}

	DSCFRelease(notifyKeys);
	DSCFRelease(notifyPatterns);
	DSCFRelease(store);
	
	return scdStatus;
	
} // RegisterForNetworkChange

// ---------------------------------------------------------------------------
//	* UnRegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: UnRegisterForNetworkChange ( void )
{
	SInt32		scdStatus = eDSNoErr;

	DbgLog( kLogApplication, "UnRegisterForNetworkChange(): " );

	return scdStatus;

} // UnRegisterForNetworkChange


// ---------------------------------------------------------------------------
//	* RegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::RegisterForSystemPower ( void )
{
	IONotificationPortRef	pmNotificationPortRef;
	CFRunLoopSourceRef		pmNotificationRunLoopSource;
	
	DbgLog( kLogApplication, "RegisterForSystemPower(): " );

	gPMKernelPort = IORegisterForSystemPower(this, &pmNotificationPortRef, dsPMNotificationHandler, &gPMDeregisterNotifier);
	if (gPMKernelPort == 0 || pmNotificationPortRef == nil)
	{
		ErrLog( kLogApplication, "RegisterForSystemPower(): IORegisterForSystemPower failed" );
	}
	else
	{
		pmNotificationRunLoopSource = IONotificationPortGetRunLoopSource(pmNotificationPortRef);
		
		if (pmNotificationRunLoopSource == nil)
		{
			ErrLog( kLogApplication, "RegisterForSystemPower(): IONotificationPortGetRunLoopSource failed" );
			gPMKernelPort = nil;
		}
		else
		{
			// TODO: should this be on Plugin loop due to potential blockage? or is that ok?
			CFRunLoopAddSource( CFRunLoopGetMain(), pmNotificationRunLoopSource, kCFRunLoopCommonModes );
		}
	}
	
	return (gPMKernelPort != 0) ? eDSNoErr : -1;
} // RegisterForSystemPower

// ---------------------------------------------------------------------------
//	* UnRegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::UnRegisterForSystemPower ( void )
{
	SInt32 ioResult = eDSNoErr;

	DbgLog( kLogApplication, "UnRegisterForSystemPower(): " );

	if (gPMKernelPort != 0) {
		gPMKernelPort = 0;
        ioResult = (SInt32)IODeregisterForSystemPower(&gPMDeregisterNotifier);
		if (ioResult != eDSNoErr)
		{
			DbgLog( kLogApplication, "UnRegisterForSystemPower(): IODeregisterForSystemPower failed, error= %d", ioResult );
		}
    }
   return ioResult;
} // UnRegisterForSystemPower


// ---------------------------------------------------------------------------
//	* HandleSystemWillSleep ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::HandleSystemWillSleep ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SrvrLog( kLogApplication, "Sleep Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillSleep;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					if (pPIInfo != nil)
					{
						ErrLog( kLogApplication, "SystemWillSleep Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
						ErrLog( kLogApplication, "SystemWillSleep Notification of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleSystemWillPowerOn ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::HandleSystemWillPowerOn ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SrvrLog( kLogApplication, "Will Power On (Wake) Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillPowerOn;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					if (pPIInfo != nil)
					{
						ErrLog( kLogApplication, "WillPowerOn Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
						ErrLog( kLogApplication, "WillPowerOn Notification of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleNetworkTransition ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::HandleNetworkTransition ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	CServerPlugin			   *searchPlugin	= nil;
	UInt32						searchIterator	= 0;
	
	aHeader.fType			= kHandleNetworkTransition;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	SrvrLog( kLogApplication, "Network transition occurred." );
	gFirstNetworkUpAtBoot = true;
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				if ( ::strcmp(pPIInfo->fName,"Search") != 0)
				{
					siResult = eDSNoErr;
					siResult = pPlugin->ProcessRequest( (void*)&aHeader );
					if (siResult != eDSNoErr)
					{
						if (pPIInfo != nil)
						{
							ErrLog( kLogApplication, "Network transition in %s plugin returned error %d", pPIInfo->fName, siResult );
						}
						else
						{
							ErrLog( kLogApplication, "Network transition of unnamed plugin returned error %d", siResult );
						}
					}
				}
				else
				{
					searchIterator	= iterator;
					searchPlugin	= pPlugin;
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	//handle the search plugin transition last to ensure at least Local and LDAPv3 have gone first
	if (searchPlugin != nil)
	{
		siResult = eDSNoErr;
		//now do the network transition itself
		aHeader.fType = kHandleNetworkTransition;
		siResult = searchPlugin->ProcessRequest( (void*)&aHeader );
		if (siResult != eDSNoErr)
		{
			ErrLog( kLogApplication, "Network transition in Search returned error %d", siResult );
		}
	}
	
	return siResult;
} // HandleNetworkTransition


// ---------------------------------------------------------------------------
//	* SetUpPeriodicTask ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::SetUpPeriodicTask ( void )
{
	SInt32		siResult	= eDSNoErr;
	void	   *ptInfo		= nil;
	
	CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, PeriodicTaskCopyStringCallback};
	
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
													CFAbsoluteTimeGetCurrent() + 30,
													30,
													0,
													0,
													DoPeriodicTask,
													(CFRunLoopTimerContext*)&c);
		
	CFRunLoopAddTimer(gPluginRunLoop, timer, kCFRunLoopDefaultMode);
	if (timer) CFRelease(timer);
					
	return siResult;
} // SetUpPeriodicTask

// this is used when only a notify is needed
void ServerControl::SearchPolicyChangedNotify( void )
{
	gTimerMutex->WaitLock();
	if (gPluginRunLoop != nil)
	{
		if( fSPCNTimerRef != NULL )
		{
			DbgLog( kLogPlugin, "ServerControl::SearchPolicyChangedNotify invalidating previous timer" );
			
			CFRunLoopTimerInvalidate( fSPCNTimerRef );
			CFRelease( fSPCNTimerRef );
			fSPCNTimerRef = NULL;
		}
		
		CFRunLoopTimerContext c = {0, (void*)this, NULL, NULL, NotifySearchPolicyChangeCopyStringCallback};
		
		fSPCNTimerRef = CFRunLoopTimerCreate(	NULL,
												CFAbsoluteTimeGetCurrent() + 2,
												0,
												0,
												0,
												SearchPolicyChangedNotifyCallback,
												(CFRunLoopTimerContext*)&c);
		
		CFRunLoopAddTimer(gPluginRunLoop, fSPCNTimerRef, kCFRunLoopDefaultMode);
	}
	gTimerMutex->SignalLock();
}

// this does more stuff because things get flushed, etc.
void ServerControl::NodeSearchPolicyChanged( void )
{
	gTimerMutex->WaitLock();
	if (gPluginRunLoop != nil)
	{
		if( fNSPCTimerRef != NULL )
		{
			DbgLog( kLogPlugin, "ServerControl::NodeSearchPolicyChanged invalidating previous timer" );

			CFRunLoopTimerInvalidate( fNSPCTimerRef );
			CFRelease( fNSPCTimerRef );
			fNSPCTimerRef = NULL;
		}
		
		CFRunLoopTimerContext c = {0, (void*)this, NULL, NULL, SearchPolicyChangeCopyStringCallback};
	
		fNSPCTimerRef = CFRunLoopTimerCreate(	NULL,
												CFAbsoluteTimeGetCurrent() + 2,
												0,
												0,
												0,
												NodeSearchPolicyChangeCallback,
												(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(gPluginRunLoop, fNSPCTimerRef, kCFRunLoopDefaultMode);
	}
	gTimerMutex->SignalLock();
}

void ServerControl::DoSearchPolicyChangedNotify( void )
{
	SCDynamicStoreRef	store		= NULL;
	
	DbgLog( kLogApplication, "DoNodeSearchPolicyChange" );
	
	store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
	if (store != NULL)
	{
		if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifySearchPolicyChanged), CFSTR("") ) )
		{
			ERRORLOG( kLogApplication, "Could not set the DirectoryService:SearchPolicyChangeToken in System Configuration" );
		}
		CFRelease(store);
		store = NULL;
	}
	else
	{
		ERRORLOG( kLogApplication, "ServerControl::DoNodeSearchPolicyChange SCDynamicStoreCreate not yet available from System Configuration" );
	}
	
	switch( ShouldRegisterWorkstation() )
	{
		case -1:
			WorkstationServiceUnregister();
			break;
		case 1:
			WorkstationServiceRegister();
			break;
	}
}

void ServerControl::DoNodeSearchPolicyChange( void )
{
	// flush memberd as well because privs could have changed
	// notify anyone listening
	// update kerberos configuration
	gSrvrCntl->FlushMemberDaemonCache();
	DoSearchPolicyChangedNotify();
	LaunchKerberosAutoConfigTool();
}

void ServerControl::NotifyDirNodeAdded( const char* newNode )
{
	SCDynamicStoreRef	store	= NULL;

	if ( newNode != nil )
	{
		CFStringRef		newNodeRef = CFStringCreateWithCString( NULL, newNode, kCFStringEncodingUTF8 );
		
		if ( newNodeRef == NULL )
		{
			ErrLog( kLogApplication, "Could not notify that dir node: (%s) was added due to an encoding problem", newNode );
		}
		else
		{
			store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
			if (store != NULL)
			{
				if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifyDirectoryNodeAdded), newNodeRef ) )
				{
					ErrLog( kLogApplication, "Could not set the DirectoryService:NotifyDirNodeAdded in System Configuration" );
				}
				CFRelease(store);
				store = NULL;
			}
			else
			{
				ErrLog( kLogApplication, "ServerControl::NotifyDirNodeAdded SCDynamicStoreCreate not yet available from System Configuration" );
			}
			CFRelease( newNodeRef );
			newNodeRef = NULL;
		}
	}
}

void ServerControl::NotifyDirNodeDeleted( char* oldNode )
{
	SCDynamicStoreRef	store	= NULL;

	if ( oldNode != nil )
	{
		CFStringRef		oldNodeRef = CFStringCreateWithCString( NULL, oldNode, kCFStringEncodingUTF8 );
		
		if ( oldNodeRef == NULL )
		{
			ErrLog( kLogApplication, "Could not notify that dir node: (%s) was deleted due to an encoding problem", oldNode );
		}
		else
		{
			store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
			if (store != NULL)
			{
				if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifyDirectoryNodeDeleted), oldNodeRef ) )
				{
					ErrLog( kLogApplication, "Could not set the DirectoryService:NotifyDirNodeDeleted in System Configuration" );
				}
				CFRelease(store);
				store = NULL;
			}
			else
			{
				ErrLog( kLogApplication, "ServerControl::NotifyDirNodeDeleted SCDynamicStoreCreate not yet available from System Configuration" );
			}
			CFRelease( oldNodeRef );
			oldNodeRef = NULL;
		}
	}
}

#ifdef BUILD_IN_PERFORMANCE
void ServerControl::DeletePerfStatTable( void )
{
	PluginPerformanceStats**	table = fPerfTable;
	UInt32						pluginCount = fPerfTableNumPlugins;

	fPerfTable = NULL;
	fPerfTableNumPlugins = 0;
	
	if ( table )
	{
		for ( UInt32 i=0; i<pluginCount+1; i++ )
		{
			if ( table[i] )
			{
				free( table[i] );
				table[i] = NULL;
			}
		}
		
		free( table );
	}
}

PluginPerformanceStats** ServerControl::CreatePerfStatTable( void )
{
DbgLog( kLogPerformanceStats, "ServerControl::CreatePerfStatTable called\n" );

	PluginPerformanceStats**	table = NULL;
	UInt32						pluginCount = gPlugins->GetPlugInCount();
	
	if ( fPerfTable )
		DeletePerfStatTable();
		
	// how many plugins?
	table = (PluginPerformanceStats**)calloc( sizeof(PluginPerformanceStats*), pluginCount+1 );	// create table for #plugins + 1 for server

	for ( UInt32 i=0; i<pluginCount; i++ )
	{
		table[i] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
		table[i]->pluginSignature = gPlugins->GetPlugInInfo(i)->fKey;
		table[i]->pluginName = gPlugins->GetPlugInInfo(i)->fName;
	}
	
	table[pluginCount] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
	table[pluginCount]->pluginSignature = 0;
	table[pluginCount]->pluginName = "Server";
	
	fPerfTableNumPlugins = pluginCount;
	fPerfTable = table;
	
	return table;
}

double gLastDump =0;
#define	kNumSecsBetweenDumps	60*2
void ServerControl::HandlePerformanceStats( UInt32 msgType, FourCharCode pluginSig, SInt32 siResult, SInt32 clientPID, double inTime, double outTime )
{
	// Since the number of plugins is so small, just doing an O(n)/2 search is probably fine...
	gPerformanceLoggingLock->WaitLock();
	PluginPerformanceStats*		curPluginStats = NULL;
	UInt32						pluginCount = gPlugins->GetPlugInCount();

	if ( !fPerfTable || fPerfTableNumPlugins != pluginCount )
	{
		// first api call, or number of plugins changed, (re)create the table
		fPerfTable = CreatePerfStatTable();
	}
	
	if ( !pluginSig )
		curPluginStats = fPerfTable[pluginCount];	// last entry in the table is reserved for the server
		
	if ( fPerfTable[fLastPluginCalled]->pluginSignature == pluginSig )
		curPluginStats = fPerfTable[fLastPluginCalled];
		
	for ( UInt32 i=0; !curPluginStats && i<pluginCount; i++ )
	{
		if ( pluginSig == fPerfTable[i]->pluginSignature )
		{
			curPluginStats = fPerfTable[i];
			fLastPluginCalled = i;
		}
	}
	
	if ( curPluginStats )
	{
		PluginPerformanceAPIStat*	curAPI = &(curPluginStats->apiStats[msgType]);
		double						duration = outTime-inTime;
		curAPI->msgCnt++;
		
		if ( siResult )
		{
			for( int i=kNumErrorsToTrack-1; i>0; i-- )
			{
				curAPI->lastNErrors[i].error = curAPI->lastNErrors[i-1].error;
				curAPI->lastNErrors[i].clientPID = curAPI->lastNErrors[i-1].clientPID;
			}
			
			curAPI->lastNErrors[0].error = siResult;
			curAPI->lastNErrors[0].clientPID = clientPID;
			curAPI->errCnt++;
		}
		
		if ( curAPI->minTime == 0 || curAPI->minTime > duration )
			curAPI->minTime = duration;
			
		if ( curAPI->maxTime == 0 || curAPI->maxTime < duration )
			curAPI->maxTime = duration;
		
		curAPI->totTime += duration;
	}
	
	gPerformanceLoggingLock->SignalLock();
}

#define USEC_PER_HOUR	(double)60*60*USEC_PER_SEC	/* microseconds per hour */
#define USEC_PER_DAY	(double)24*USEC_PER_HOUR	/* microseconds per day */

void ServerControl::LogStats( void )
{
	PluginPerformanceStats*		curPluginStats = NULL;
	UInt32						pluginCount = fPerfTableNumPlugins;
	char						logBuf[1024];
	char						totTimeStr[256];
	
	gPerformanceLoggingLock->WaitLock();

	syslog( LOG_CRIT, "**Usage Stats**\n");
	syslog( LOG_CRIT, "\tPlugin\tAPI\tMsgCnt\tErrCnt\tminTime (usec)\tmaxTime (usec)\taverageTime (usec)\ttotTime (usec|secs|hours|days)\tLast PID\tLast Error\tPrev PIDs/Errors\n" );
	
	for ( UInt32 i=0; i<pluginCount+1; i++ )	// server is at the end
	{
		if ( !fPerfTable[i] )
			continue;
			
		curPluginStats = fPerfTable[i];
				
		for ( UInt32 j=0; j<kDSPlugInCallsEnd; j++ )
		{
			if ( curPluginStats->apiStats[j].msgCnt > 0 )
			{
				if ( curPluginStats->apiStats[j].totTime < USEC_PER_SEC )
					sprintf( totTimeStr, "%0.f usecs", curPluginStats->apiStats[j].totTime );
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_HOUR )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_SEC;
					sprintf( totTimeStr, "%0.4f secs", time );
				}
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_DAY )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_HOUR;
					sprintf( totTimeStr, "%0.4f hours", time );
				}
				else
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_DAY;
					sprintf( totTimeStr, "%0.4f days", time );
				}
				
				sprintf( logBuf, "\t%s\t%s\t%ld\t%ld\t%.0f\t%0.f\t%0.f\t%s\t%ld/%ld\t%ld/%ld\t%ld/%ld\t%ld/%ld\t%ld/%ld\n",
								curPluginStats->pluginName,
								CRequestHandler::GetCallName(j),
								curPluginStats->apiStats[j].msgCnt,
								curPluginStats->apiStats[j].errCnt,
								curPluginStats->apiStats[j].minTime,
								curPluginStats->apiStats[j].maxTime,
								(curPluginStats->apiStats[j].totTime/curPluginStats->apiStats[j].msgCnt),
								totTimeStr,
								curPluginStats->apiStats[j].lastNErrors[0].clientPID,
								curPluginStats->apiStats[j].lastNErrors[0].error,
								curPluginStats->apiStats[j].lastNErrors[1].clientPID,
								curPluginStats->apiStats[j].lastNErrors[1].error,
								curPluginStats->apiStats[j].lastNErrors[2].clientPID,
								curPluginStats->apiStats[j].lastNErrors[2].error,
								curPluginStats->apiStats[j].lastNErrors[3].clientPID,
								curPluginStats->apiStats[j].lastNErrors[3].error,
								curPluginStats->apiStats[j].lastNErrors[4].clientPID,
								curPluginStats->apiStats[j].lastNErrors[4].error );
				
				syslog( LOG_CRIT, logBuf );
			}
		}
	}
	
	gPerformanceLoggingLock->SignalLock();
}
#endif

// ---------------------------------------------------------------------------
//	* DoPeriodicTask ()
//
// ---------------------------------------------------------------------------

void DoPeriodicTask(CFRunLoopTimerRef timer, void *info)
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	
	if ( gDSLocalOnlyMode && (gLocalSessionCount == 0) )
	{
		//no need to keep this daemon going anymore as it launches on demand
		SrvrLog( kLogApplication, "Shutting down DirectoryService..." );
		gSrvrCntl->ShutDownServer();
		exit(0);
	}
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only pulse the Active plugins
			{
				siResult = pPlugin->PeriodicTask();
				if (siResult != eDSNoErr)
				{
					if (pPIInfo != nil)
					{
							DbgLog( kLogApplication, "Periodic Task in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
							DbgLog( kLogApplication, "Periodic Task of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
					
	return;
} // DoPeriodicTask

// ---------------------------------------------------------------------------
//	* FlushMemberDaemonCache ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::FlushMemberDaemonCache ( void )
{
	SInt32 siResult = eDSNoErr;
	
	//routine created for potential other additions
	DbgLog( kLogApplication, "Flushing membership cache" );
	Mbrd_ProcessResetCache();
	
	return(siResult);
}// FlushMemberDaemonCache


void ServerControl::CreateDebugPrefFileIfNecessary( bool bForceCreate )
{
	UInt32					uiDataSize	= 0;
	CFile				   *pFile		= nil;
	struct stat				statbuf;
	CFDataRef				dataRef		= nil;

	if ( bForceCreate )
		unlink( kDSDebugConfigFilePath );
	
	if ( lstat(kDSDebugConfigFilePath, &statbuf) != 0 )
	{
		//write a default file and setup debugging
		SInt32 result = eDSNoErr;
		gDebugLogging = true;
		CLog::SetLoggingPriority(keDebugLog, 5);
		uiDataSize = ::strlen( kDefaultDebugConfig );
		dataRef = ::CFDataCreate( nil, (const UInt8 *)kDefaultDebugConfig, uiDataSize );
		if ( dataRef != nil )
		{
			result = dsCreatePrefsDirectory();
			
			CFIndex dataLen = CFDataGetLength( dataRef );
			const UInt8 *pUData = CFDataGetBytePtr( dataRef );
			if ( pUData != NULL && dataLen > 0 )
			{
				try
				{
					pFile = new CFile( kDSDebugConfigFilePath, true );
					if ( pFile != nil )
					{
						if ( pFile->is_open() )
						{
							pFile->seteof( 0 );
							pFile->write( pUData, dataLen );
							chmod( kDSDebugConfigFilePath, 0600 );
						}
						
						delete( pFile );
						pFile = nil;
					}
				}
				catch ( ... )
				{
				}
			}
			
			CFRelease( dataRef );
			dataRef = nil;
		}
	}
}

// ---------------------------------------------------------------------------
//	* ResetDebugging ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::ResetDebugging ( void )
{
	SInt32					siResult	= eDSNoErr;
	UInt32					uiDataSize	= 0;
	char				   *pData		= nil;
	CFile				   *pFile		= nil;
	struct stat				statbuf;
	CFDataRef				dataRef		= nil;
	CFBooleanRef			cfBool		= false;
	bool					bDebugging  = false;
	bool					bFileUsed   = false;
	

	if (gToggleDebugging)
	{
		gToggleDebugging = false;
		//here we turn everything off
		if (gDebugLogging)
		{
			CLog::StopDebugLog();
			gDebugLogging = false;
			syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
		}
		gDSFWCSBPDebugLogging = false;
	}
	else
	{
		//next time this is called we turn everything off
		gToggleDebugging = true;
		
		// Does the debug config file exist
		siResult = ::stat( kDSDebugConfigFilePath, &statbuf );
		if ( siResult == eDSNoErr )
		{
			// Attempt to get config info from file
			pFile = new CFile( kDSDebugConfigFilePath );
			if (pFile != nil) 
			{
				if ( (pFile->is_open()) && (pFile->FileSize() > 0) )
				{
					// Allocate space for the file data
					pData = (char *)::calloc( 1, pFile->FileSize() + 1 );
					if ( pData != nil )
					{
						// Read from the config file
						uiDataSize = pFile->ReadBlock( pData, pFile->FileSize() );
						dataRef = ::CFDataCreate( nil, (const UInt8 *)pData, uiDataSize );
						if ( dataRef != nil )
						{
							CFPropertyListRef   aPlistRef   = 0;
							CFDictionaryRef		aDictRef	= 0;
							// Is it valid XML data
							aPlistRef = ::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nil );
							if ( aPlistRef != nil )
							{
								// Is it a plist type
								if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( aPlistRef ) )
								{
								
									bFileUsed = true;
									
									aDictRef = (CFDictionaryRef)aPlistRef;
									
									//now set up the debugging according to the plist settings
									
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSIgnoreSunsetTimeKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSIgnoreSunsetTimeKey ) );
										if (cfBool != nil)
										{
											gIgnoreSunsetTime = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}

									//debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) );
										if (cfBool != nil)
										{
											bDebugging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
											if (gDebugLogging && !bDebugging)
											{
												CLog::StopDebugLog();
												gDebugLogging = false;
												syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
											}
											else if (bDebugging)
											{
												if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSDebugLoggingPriority ) ) )
												{
													CFNumberRef logPriority	= (CFNumberRef)CFDictionaryGetValue( aDictRef, CFSTR(kXMLDSDebugLoggingPriority) );
													UInt32 priorityValue = 0;
													CFNumberGetValue(logPriority, kCFNumberIntType, &priorityValue);
													if (priorityValue > 0 && priorityValue < 9) //1-5 is tiered and 6-8 are specific
													{
														CLog::SetLoggingPriority(keDebugLog, priorityValue);
														syslog(LOG_ALERT,"Debug Logging priority %u set.", priorityValue);
													}
												}
												else
												{
													CLog::SetLoggingPriority(keDebugLog, 5);
													syslog(LOG_ALERT,"Debug Logging default priority 5 set.");
												}

												CLog::StartDebugLog();
												if (!gDebugLogging)
												{
													syslog(LOG_ALERT,"Debug Logging turned ON after receiving USR1 signal.");
													gDebugLogging = true;
												}
											}
										}
									}
									else if (gDebugLogging)
									{
										CLog::StopDebugLog();
										gDebugLogging = false;
										syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
									}

									//FW CSBP debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) );
										if (cfBool != nil)
										{
											gDSFWCSBPDebugLogging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}
									else
									{
										gDSFWCSBPDebugLogging = false;
									}

									aDictRef = 0;
								}
								//free the propertylist
								CFRelease(aPlistRef);
							}
							CFRelease( dataRef );
							dataRef = nil;
						}
						free( pData );
						pData = nil;
					}
				}
				delete( pFile );
				pFile = nil;
			}
		}
		
		if (!bFileUsed)
			CreateDebugPrefFileIfNecessary( true );
	}
	
	return(siResult);
	
} // ResetDebugging



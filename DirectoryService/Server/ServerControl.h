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

#ifndef __ServerControl_h__
#define __ServerControl_h__	1

#include <mach/port.h>
#define USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>

#include "PrivateTypes.h"
#include "SharedConsts.h"
#include "CCachePlugin.h"

const UInt32 kMaxHandlerThreads			= 256; // this is used for both mach and TCP handler thread max

#define kDSDefaultListenPort 625				//TODO need final port number
#define kDSActOnThisNumberOfFlushRequests 25	//even if requests are close together send a flush after 25 requests

#define	kDSDebugConfigFilePath		"/Library/Preferences/DirectoryService/DirectoryServiceDebug.plist"
#define kXMLDSDebugLoggingKey		"Debug Logging"					//debug logging on/off
#define kXMLDSDebugLoggingPriority	"Debug Logging Priority Level"	//priority level 1 (low - everything) through 5 (high - critical things)
#define kXMLDSCSBPDebugLoggingKey   "CSBP FW Debug Logging"			//DS FW CSBP debug logging on/off
#define kXMLDSDebugTurnOffNISearch	"Turn Off NetInfo Search"		//turns off NetInfo use in the search policy
#define kXMLDSDebugLoadNIPlugin		"Load NetInfo Plugin"			//determines whether to load NetInfo plugin
#define kXMLDSIgnoreSunsetTimeKey   "Ignore syslog Sunset Time"		//ignore sunset time for API timing logging

#define kDefaultDebugConfig		\
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n\
<dict>\n\
	<key>Version</key>\n\
	<string>1.0</string>\n\
	<key>Debug Logging</key>\n\
	<true/>\n\
	<key>CSBP FW Debug Logging</key>\n\
	<false/>\n\
	<key>Turn Off NetInfo Search</key>\n\
	<true/>\n\
	<key>Load NetInfo Plugin</key>\n\
	<false/>\n\
	<key>Debug Logging Priority Level</key>\n\
	<integer>5</integer>\n\
	<key>Ignore syslog Sunset Time</key>\n\
	<false/>\n\
</dict>\n\
</plist>\n"
#define BUILD_IN_PERFORMANCE

#ifdef BUILD_IN_PERFORMANCE
#define			PERFORMANCE_STATS_ALWAYS_ON		0	

#define	kNumErrorsToTrack			5

//eDSLookupProcedureNumber MUST be SYNC'ed with lookupProcedures
typedef enum {
    kDSLUfirstprocnum = 0,
    
    kDSLUgetpwnam,
    kDSLUgetpwuuid,
    kDSLUgetpwuid,
    kDSLUgetpwent,
    kDSLUgetgrnam,
    kDSLUgetgruuid,
    kDSLUgetgrgid,
    kDSLUgetgrent,
    kDSLUgetservbyname,
    kDSLUgetservbyport,
    kDSLUgetservent,
    kDSLUgetprotobyname,
    kDSLUgetprotobynumber,
    kDSLUgetprotoent,
    kDSLUgetrpcbyname,
    kDSLUgetrpcbynumber,
    kDSLUgetrpcent,
    kDSLUgetfsbyname,
    kDSLUgetfsent,
//    kDSLUprdb_getbyname,
//    kDSLUprdb_get,
//    kDSLUbootparams_getbyname,
//    kDSLUbootparams_getent,
    kDSLUalias_getbyname,
    kDSLUalias_getent,
    kDSLUgetnetent,
    kDSLUgetnetbyname,
    kDSLUgetnetbyaddr,
//    kDSLUbootp_getbyip,
//    kDSLUbootp_getbyether,
    kDSLUinnetgr,
    kDSLUgetnetgrent,
    
    kDSLUgetaddrinfo,
    kDSLUgetnameinfo,
    kDSLUgethostbyname,
    kDSLUgethostbyaddr,
    kDSLUgethostent,
    kDSLUgetmacbyname,
    kDSLUgethostbymac,
	
	kDSLUgetbootpbyhw,
	kDSLUgetbootpbyaddr,

	// other types of calls
	kDSLUdns_proxy,

	kDSLUflushcache,
    kDSLUflushentry,

    kDSLUlastprocnum // this number will increment automatically
} eDSLookupProcedureNumber;

typedef struct {
	SInt32						clientPID;
	SInt32						error;
} ErrorByPID;

typedef struct {
	UInt32						msgCnt;
	UInt32						errCnt;
	double						minTime;
	double						maxTime;
	double						totTime;
	ErrorByPID					lastNErrors[kNumErrorsToTrack];
} PluginPerformanceAPIStat;

typedef struct {
	FourCharCode				pluginSignature;
	const char*					pluginName;
	PluginPerformanceAPIStat	apiStats[kDSPlugInCallsEnd];	// size is equal to the max number of API calls
} PluginPerformanceStats;
#endif

//-----------------------------------------------------------------------------
//	* Class definitions
//-----------------------------------------------------------------------------

class	DSTCPListener;
class	CHandlerThread;
class	CNodeList;
class	CPlugInList;

//-----------------------------------------------------------------------------
//	* externs
//-----------------------------------------------------------------------------

extern CNodeList	*gNodeList;
extern CPlugInList	*gPlugins;

//-----------------------------------------------------------------------------
//	* ServerControl
//-----------------------------------------------------------------------------

class ServerControl
{
public:
						ServerControl		 ( void );
	virtual			   ~ServerControl		 ( void );

	virtual SInt32		StartUpServer		( void );
	virtual SInt32		ShutDownServer		( void );
			SInt32		StartAHandler		( const FourCharCode inThreadSignature );
			SInt32		StopAHandler		( const FourCharCode inThreadSignature, UInt32 iThread, CHandlerThread *inThread );
			void		WakeAHandler		( const FourCharCode inThreadSignature );
			void		SleepAHandler		( const FourCharCode inThreadSignature, UInt32 waitTime );
			UInt32		GetHandlerCount		( const FourCharCode inThreadSignature );
			SInt32		StartTCPListener	( UInt32 inPort );
			SInt32		StopTCPListener		( void );

			SInt32		HandleNetworkTransition	( void );
			SInt32		HandleSystemWillSleep	( void );
			SInt32		HandleSystemWillPowerOn	( void );
			SInt32		FlushMemberDaemonCache	( void );
			SInt32		SetUpPeriodicTask	( void );
			SInt32		ResetDebugging		( void );

#ifdef BUILD_IN_PERFORMANCE
			void		ActivatePeformanceStatGathering( void ) { fPerformanceStatGatheringActive = true; }
			void		DeactivatePeformanceStatGathering( void ) { fPerformanceStatGatheringActive = false; LogStats(); DeletePerfStatTable(); }
			
			void		HandlePerformanceStats		( UInt32 msgType, FourCharCode pluginSig, SInt32 siResult, SInt32 clientPID, double inTime, double outTime );
			void		LogStats					( void );
#endif

			bool		IsPeformanceStatGatheringActive
													( void ) { return fPerformanceStatGatheringActive; }
			void		NotifyDirNodeAdded			( const char* newNode );
			void		NotifyDirNodeDeleted		( char* oldNode );
			
			void		SearchPolicyChangedNotify	( void );
			void		DoSearchPolicyChangedNotify	( void ); // should only be called by Timer
			void		NodeSearchPolicyChanged		( void );
			void		DoNodeSearchPolicyChange	( void ); // should only be called by Timer

protected:
#ifdef BUILD_IN_PERFORMANCE
			void		DeletePerfStatTable			( void );
			PluginPerformanceStats**	
						CreatePerfStatTable			( void );
#endif

			SInt32		RegisterForSystemPower		( void );
			SInt32		UnRegisterForSystemPower	( void );
			SInt32		RegisterForNetworkChange	( void );
			SInt32		UnRegisterForNetworkChange	( void );
			void		CreateDebugPrefFileIfNecessary	( bool bForceCreate = false );
private:

	UInt32				fTCPHandlerThreadsCnt;
	UInt32              fLibinfoHandlerThreadCnt;

#ifdef BUILD_IN_PERFORMANCE
	UInt32				fLastPluginCalled;
	PluginPerformanceStats	**fPerfTable;
	UInt32				fPerfTableNumPlugins;
#endif
	
	DSTCPListener	   *fTCPListener;
	CHandlerThread	  **fTCPHandlers;
	CHandlerThread    **fLibinfoHandlers;
	SCDynamicStoreRef	fSCDStore;
	bool				fPerformanceStatGatheringActive;
	UInt32				fMemberDaemonFlushCacheRequestCount;
	CFStringRef			fServiceNameString;
	static	CFRunLoopTimerRef	fNSPCTimerRef; //NSPC = Node Search Policy Change
	static	CFRunLoopTimerRef	fSPCNTimerRef; //SPCN = Search Policy Changed Notify
};

extern ServerControl	*gSrvrCntl;
#endif

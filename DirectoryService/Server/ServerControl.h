/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include "DSSemaphore.h"
#include "SharedConsts.h"

const uInt32 kMaxHandlerThreads			= 4; // this is used for both mach and TCP handler thread max
const uInt32 kMaxCheckpwHandlerThreads	= 1; // make sure this is one more than kMaxHandlerThreads
const uInt32 kMaxInternalHandlerThreads	= 10; // make sure this is one more than ALL external threads

#define kDSDefaultListenPort 625 //TODO need final port number

#define BUILD_IN_PERFORMANCE

#ifdef BUILD_IN_PERFORMANCE
#define			PERFORMANCE_STATS_ALWAYS_ON		0	

#define	kNumErrorsToTrack			5

typedef struct {
	sInt32						clientPID;
	sInt32						error;
} ErrorByPID;

typedef struct {
	uInt32						msgCnt;
	uInt32						errCnt;
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

class	CListener;
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

	virtual sInt32		StartUpServer		( void );
	virtual sInt32		ShutDownServer		( void );
			sInt32		StartAHandler		( const FourCharCode inThreadSignature );
			sInt32		StopAHandler		( const FourCharCode inThreadSignature, uInt32 iThread, CHandlerThread *inThread );
			void		WakeAHandler		( const FourCharCode inThreadSignature );
			void		SleepAHandler		( const FourCharCode inThreadSignature, uInt32 waitTime );
			uInt32		GetHandlerCount		( const FourCharCode inThreadSignature );
			sInt32		StartTCPListener	( uInt32 inPort );
			sInt32		StopTCPListener		( void );
			sInt32		HandleNetworkTransition
											( void );
			sInt32		NIAutoSwitchCheck	( void );
			sInt32		SetUpPeriodicTask	( void );

#ifdef BUILD_IN_PERFORMANCE
			void		ActivatePeformanceStatGathering( void ) { fPerformanceStatGatheringActive = true; }
			void		DeactivatePeformanceStatGathering( void ) { fPerformanceStatGatheringActive = false; LogStats(); DeletePerfStatTable(); }
			
			void		HandlePerformanceStats		( uInt32 msgType, FourCharCode pluginSig, sInt32 siResult, sInt32 clientPID, double inTime, double outTime );
			void		LogStats					( void );
#endif

			bool		IsPeformanceStatGatheringActive
													( void ) { return fPerformanceStatGatheringActive; }
			void		NotifyDirNodeAdded			( const char* newNode );
			void		NotifyDirNodeDeleted		( char* oldNode );
			
			void		NodeSearchPolicyChanged		( void );

protected:
#ifdef BUILD_IN_PERFORMANCE
			void		DeletePerfStatTable			( void );
			PluginPerformanceStats**	
						CreatePerfStatTable			( void );
#endif

			sInt32		StartListener				( void );
			sInt32		RegisterForSystemPower		( void );
			sInt32		UnRegisterForSystemPower	( void );
			sInt32		RegisterForNetworkChange	( void );
			sInt32		UnRegisterForNetworkChange	( void );
			sInt32		UnbindToNetInfo				( void );
			void		HandleMultipleNetworkTransitionsForNIAutoSwitch ( void );
			void		LaunchKerberosAutoConfigTool( void );
private:

	uInt32				fTCPHandlerThreadsCnt;
	uInt32				fHandlerThreadsCnt;
	uInt32				fInternalHandlerThreadsCnt;
	uInt32				fCheckpwHandlerThreadsCnt;

#ifdef BUILD_IN_PERFORMANCE
	uInt32				fLastPluginCalled;
	PluginPerformanceStats	**fPerfTable;
	uInt32				fPerfTableNumPlugins;
#endif
	
	CListener		   *fListener;
	DSTCPListener	   *fTCPListener;
	CHandlerThread		*fTCPHandlers[ kMaxHandlerThreads ];
	CHandlerThread	   *fHandlers[ kMaxHandlerThreads ];
	CHandlerThread	   *fInternalHandlers[ kMaxInternalHandlerThreads ];
	CHandlerThread	   *fCheckpwHandlers[ kMaxCheckpwHandlerThreads ];
	DSSemaphore		   *fTCPHandlerSemaphore;
	DSSemaphore		   *fHandlerSemaphore;
	DSSemaphore		   *fInternalHandlerSemaphore;
	DSSemaphore		   *fCheckpwHandlerSemaphore;
	SCDynamicStoreRef	fSCDStore;
	bool				fPerformanceStatGatheringActive;
	double				fTimeToCheckNIAutoSwitch;
	SCDynamicStoreRef	fHoldStore;
	CFStringRef			fServiceNameString;

};

extern ServerControl	*gSrvrCntl;
#endif

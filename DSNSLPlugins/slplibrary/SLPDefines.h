/*
	File:		SLPDefines.h

	Contains:	

	Written by:	Kevin Arnold 

	Copyright:	© 1997 - 1999 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):



*/
#ifndef _SLPDefines_
#define _SLPDefines_
#pragma once

#include <Carbon/Carbon.h>
//#include <syslog.h>
//#define kSLPdPath	"/System/Library/Frameworks/NSLManager.framework/Resources/NSLPlugins/NSL_SLP_SA_Plugin.bundle/Resources/slp_ipc"
#define kSLPdPath	"/tmp/slp_ipc"

#define CONFIG_DA_HEART_BEAT		10800	// (3 hours) DA Heartbeat,
											// so that SAs passively detect
											// new DAs.

#define v2_Default_Scope			"DEFAULT"
#define kSLPPluginNotInitialized	-1

enum
{
	kLogOptionRegDereg			= 0x01000000,
	kLogOptionExpirations		= 0x02000000,
	kLogOptionServiceRequests	= 0x04000000,
	kLogOptionDAInfoRequests	= 0x08000000,
	kLogOptionRejections		= 0x10000000,
	kLogOptionRAdminInteraction	= 0x20000000,
	kLogOptionErrors			= 0x00000080,
	kLogOptionDebuggingMessages	= 0x40000000,
    kLogOptionAllMessages		= 0x80000000
};
#if 0
//////////////////////////////////
// Now we will conform to SLP v2 spec definitions below
//////////////////////////////////
//
	#define v2_Port_Number				427
	#define v2_Multicast_Address_String	"239.255.255.253"	
	#define v2_Multicast_Address_Num	0xEFFFFFFD
	#define v2_Spec_Version				2
	#define v2_Default_Scope			"DEFAULT"
//
//////////////////////////////////

//////////////////////////////////
// These are our preferences stuff
//////////////////////////////////
	
	#define kIsDATag						"net.slp.isDA"
	#define kRegistrationLifetimeTag		"net.slp.RegistrationLifetime"
	#define kDADynamicAddresses				"net.slp.DADynamicAddresses"		// these are ones found via DAAdverts
	#define kDAAddresses					"net.slp.DAAddresses"				// for pre-configuring
	#define kLoggingLevelBitMap				"net.slp.LoggingLevel"
	#define kProxyForOtherDAs				"net.slp.ProxyForOtherDAs"
	#define kDAHeartBeatTag					"net.slp.DAHeartBeat"
	#define kUseScopesTag					"net.slp.useScopes"
	#define kTraceDATrafficTag				"net.slp.traceDATraffic"
	#define kTraceMsgTag					"net.slp.traceMsg"
	#define kTraceDropTag					"net.slp.traceDrop"
	#define kTraceRegTag					"net.slp.traceReg"
	#define kTraceErrorsTag					"net.slp.traceErrors"
	#define kTraceDebug						"net.slp.traceDebug"
	#define kSerializedRegURLTag			"net.slp.serializedRegURL"
	#define kIsBroadcastOnlyTag				"net.slp.isBroadcastOnly"
	#define kPassiveDADetectionTag			"net.slp.passiveDADetection"
	#define kMulticastTTLTag				"net.slp.multicastTTL"
	#define kDAActiveDiscoveryIntervalTag	"net.slp.DAActiveDiscoveryInterval"
	#define kMulticastMaximumWaitTag		"net.slp.multicastMaximumWait"
	#define kMulticastTimeoutsTag			"net.slp.multicastTimeouts"
	#define kDADiscoveryTimeoutsTag			"net.slp.DADiscoveryTimeouts"
	#define kRandomWaitBoundTag				"net.slp.randomWaitBound"
	#define kDatagramTimeouts				"net.slp.datagramTimeouts"
	#define kLocaleTag						"net.slp.locale"
	#define kSecurityEnabledTag				"net.slp.securityEnabled"
	#define kMTUTag							"net.slp.MTU"
	#define kTypeHintTag					"net.slp.typeHint"
	#define kInterfacesTag					"net.slp.interfaces"
	#define kDAAttributesTag				"net.slp.DAAttributes"
	#define kSAAttributesTag				"net.slp.SAAttributes"
	#define kMaxResultsTag					"net.slp.maxResults"
//
//////////////////////////////////

#define kSARequestServiceType		"service:service-agent"
#define kDARequestServiceType		"service:directory-agent"

enum
{
	kLogDATraffic			= 0x00010000,					//  A boolean controlling printing of messages about traffic 
															//	with DAs.

	kLogMsg					= 0x00020000,					//	A boolean controlling printing of details on SLP messages.
         													//	The fields in all incoming messages and outgoing replies are
        													//	printed.
        													
	kLogDrop				= 0x00040000,					//	A boolean controlling printing details when a SLP message is
         													//	dropped for any reason.
         													
//	kLogReg					= 0x00080000,					//	Just log registrations/deregistrations
																														
	kLogRegDebug 			= 0x00100000,					// 	debug logging for reg/dereg actions
	kLogReqDebug			= 0x00200000,					// 
//	kLogReserved4			= 0x00400000,					// 
//	kLogReserved5			= 0x00800000,					// 		
	kLogDebug				= 0xFFFFFFFF,					//	Log everything (old debug msgs too)

	
	// the kLogOption values are meant to map directly to the user set option levels for SLP DA Server
	kLogOptionRegDereg			= 0x01000000,
	kLogOptionExpirations		= 0x02000000,
	kLogOptionServiceRequests	= 0x04000000,
	kLogOptionDAInfoRequests	= 0x08000000,
	kLogOptionRejections		= 0x10000000,
	kLogOptionRAdminInteraction	= 0x20000000,
	kLogOptionErrors			= 0x00000080,
	kLogOptionDebuggingMessages	= 0x40000000
};

#define kStandardEventsSet		kLogOptionRegDereg + kLogOptionServiceRequests + kLogOptionDAInfoRequests + kLogOptionErrors
#define kErrorsOnlySet			kLogOptionErrors
#define kDebuggingSet			kLogOptionDebuggingMessages

#define kPluginInfoServiceType		"service:x-MacSLPInfo"
#define kTimeOfStartupServiceType	"service:x-MacSLPInfo:startuptime"
#define kNumSLPInstancesServiceType	"service:x-MacSLPInfo:numberofclients"
#define kSLPPreferencesServiceType	"service:x-MacSLPInfo:preferences"
#define kCurRegServicesServiceType	"service:x-MacSLPInfo:curregservices"

#define kAttributeDelimiter			";"
#define kVersionTag					"Version"
#define kOSVersionTag				"OSVersion"
#define kNumRegServicesTag			"NumRegServices"
#define kStartupTimeTag				"Time Of Startup"
#define kNumSLPInstancesTag			"Number of SLP Clients"
#define kSLPPreferencesTag			"Current Preferences"
#define kCurRegServicesTag			"Current Types of Registered Services"

//#define kSubnetTag	"subnet-"		// !!!! This need to be put into a resource and localized!

#define kLocalizedStrings				128
enum {
	kLocalServicesStringID			=	1
};

// header default values
const	UInt8	DEFAULT_VERSION			= 2;
//const	UInt8	DEFAULT_VERSION			= 1;
const	UInt8	DEFAULT_DIALECT			= 0;
const	UInt16	DEFAULT_LANGUAGE_CODE	= 0x656e;	// 'en'
const	UInt16	DEFAULT_CHAR_ENCODING	= 3;
const	UInt8	DEFAULT_BIT_MAP			= 0;

// Header Function Values
const	UInt8	SrvReq		= 1;
const	UInt8	SrvRply		= 2;
const	UInt8	SrvReg		= 3;
const	UInt8	SrvDereg	= 4;
const	UInt8	SrvAck		= 5;
const	UInt8	AttrRqst	= 6;
const	UInt8	AttrRply	= 7;
const	UInt8	DAAdvert	= 8;
const	UInt8	SrvTypeRqst	= 9;
const	UInt8	SrvTypeRply	= 10;
const	UInt8	SAAdvert	= 11;
const	UInt8	PluginInfoReq = 0xFF;
#define IANA	""
// the following Time Out Intervals are all in seconds
unsigned short RangedRdm( unsigned short min, unsigned short max );
#endif

#define CONFIG_INTERVAL_0			60		// Cache replies by XID.

#define CONFIG_INTERVAL_1			10800	// registration Lifetime,
											// (ie. 3 hours) 
											// after which ad expires
											
#define CONFIG_INTERVAL_2			1		// Retry multicast query 
											// until no new values arrive.
											
#define CONFIG_INTERVAL_3			30		// Max time to wait for a 
//#define CONFIG_INTERVAL_3			15		// Max time to wait for a 
											// complete multicast query
											// response (all values.)

#define CONFIG_INTERVAL_4			3		// Wait to rgister on reboot

#define CONFIG_INTERVAL_5			3		// Retransmit DA discovery,
											// retry it 3 times
											
#define CONFIG_INTERVAL_6			5		// Give up on requests sent to DA

#define CONFIG_INTERVAL_7			15		// Give up on DA discovery

#define CONFIG_INTERVAL_8			15		// Give up on requests sent to SAs

#define CONFIG_INTERVAL_9			10800	// (3 hours) DA Heartbeat,
											// so that SAs passively detect
											// new DAs.
											
#define CONFIG_INTERVAL_10			RangedRdm(1,3)	
											// (should be random 1-3)
											// wait to register services on
											// passive DA discovery
											
#define CONFIG_INTERVAL_11			RangedRdm(1,3)
											// (should be random 1-3)
											// wait to register services on
											// active DA discovery.
											
#define CONFIG_INTERVAL_12			300		// (ie. 5 seconds)
											// DAs and SAs close idle connections
#if 0
#define CONFIG_CLOSE_DOWN			60*60*5	// five minutes
#define CONFIG_DA_FIND				900		// seconds between active da discoveries
#define CONFIG_RETRY				2		// seconds between retransmitting unicast requests
#define CONFIG_RETRY_MAX			15		// seconds to give up on unicast transmission

enum {
	kReservedBit1		= 0x0100,
	kReservedBit2		= 0x0200,
	kReservedBit3		= 0x0400,
	kReservedBit4		= 0x0800,
	kReservedBit5		= 0x1000,
	kRequestMulticast	= 0x2000,
	kFreshRegistration	= 0x4000,
	kOverFlowBit		= 0x8000
};

// from here down we have our own defines
//#define kTimeBeforeTTLTimeOutToReregister	7200	// (2 hours)
//#define kSAAdvertisementRequestTimeout		10		// ten seconds
//#define kMCastRequestTimeout				20		// twenty seconds
//#define kMCastWriteInterval					3		// three seconds
#define kTimeToListenForReplyToRequest		2		// two seconds
#define kTimeToGiveUpOnDAResponse			2*CONFIG_INTERVAL_6	// let's double the suggested SLP Spec
#define kTimeToGiveUpOnDAConnection			15		// fifteen seconds

#define kLifeTimeValue						3600	// an hour?
#define kDelayWaitingForBindToPort			1000	// in ticks
#define kSLPListenerTimeoutValue			99999	// in ticks

#define kTimeBetweenTTLTimeChecks			1800	// in ticks	(30 seconds)
#define	kDiffBetweenMacOSSecsAndSLPSecs		

#define	kTimeToWaitBeforeUpdatingCacheAfterDeregistration	60	// in ticks (1 second)

enum AgentStatus {
	kUninitialized 		= -1,
	kQuitting			= 0,
	kRunning			= 1
};
#endif

#endif
/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __PasswordServerPrefsDefs__
#define __PasswordServerPrefsDefs__

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdbool.h>
#include <time.h>
#include "sasl.h"

#define kPWExternalToolPath			"/usr/sbin/authserver/tools"
#define kPWPrefsFile				"/Library/Preferences/com.apple.passwordserver.plist"
#define kMaxListenerPorts			10
#define kMaxSASLPlugins				30
#define kKerberosCacheScaleLimit	95000	// the number of principals that can be stored in memory for replication

#define kPWPrefsKey_PassiveReplicationOnly		"PassiveReplicationOnly"
#define kPWPrefsKey_ProvideReplicationOnly		"ProvideReplicationOnly"
#define kPWPrefsKey_BadTrialDelay				"BadTrialDelay"
#define kPWPrefsKey_TimeSkewMaxSeconds			"TimeSkewMaxSeconds"
#define kPWPrefsKey_SyncInterval				"SyncInterval"
#define kPWPrefsKey_ListenerPorts				"ListenerPorts"
#define kPWPrefsKey_ListenerInterfaces			"ListenerInterfaces"
#define kPWPrefsValue_ListenerEnet				"Ethernet"
#define kPWPrefsValue_ListenerLocal				"Local"
#define kPWPrefsKey_TestSpillBucket				"TestSpillBucket"
#define kPWPrefsKey_SASLRealm					"SASLRealm"
#define kPWPrefsKey_ExternalTool				"ExternalCommand"
#define kPWPrefsValue_ExternalToolNone			"Disabled"
#define kPWPrefsKey_KerberosCacheLimit			"KerberosCacheLimit"
#define kPWPrefsKey_SyncSASLPluginList			"SyncSASLPlugInList"
#define kPWPrefsKey_SASLPluginList				"SASLPluginStates"

typedef enum ListenerTypes {
	kPWPrefsNoListeners		= 0x00,
	kPWPrefsEnet			= 0x01,
	kPWPrefsLocal			= 0x02,
	kPWPrefsEnetAndLocal	= 0x03
} ListenerTypes;

typedef enum SASLPluginStatus {
	kSASLPluginStateUnlisted,
	kSASLPluginStateAllowed,
	kSASLPluginStateDisabled
} SASLPluginStatus;

typedef struct SASLPluginEntry {
	char name[SASL_MECHNAMEMAX + 1];
	SASLPluginStatus state;
} SASLPluginEntry;

typedef struct PasswordServerPrefs {
	bool passiveReplicationOnly;							// server waits for another server to sync with it
	bool provideReplicationOnly;							// server provides sync data, but trusts no one
	unsigned long badTrialDelay;							// for a failed login attempt, sleep the session this long
	unsigned long timeSkewMaxSeconds;						// maximum time clock difference between two replicas
	unsigned long syncInterval;
	unsigned short listenerPort[kMaxListenerPorts + 1];
	ListenerTypes listenerTypeFlags;
	bool externalToolSet;
	char externalToolPath[256];
	bool testSpillBucket;
	bool realmSet;
	char realm[256];
	unsigned long kerberosCacheLimit;
	bool syncSASLPluginList;
	SASLPluginEntry saslPluginState[kMaxSASLPlugins + 1];
	time_t deleteWait;
	time_t purgeWait;
} PasswordServerPrefs;

bool pwsf_SetSASLPluginState( const char *inMechName, bool enabled );

#ifdef __cplusplus
};
#endif

#endif

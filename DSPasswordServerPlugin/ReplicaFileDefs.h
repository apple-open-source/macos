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

#ifndef __REPLICAFILEDEFS__
#define __REPLICAFILEDEFS__

#include <arpa/nameser.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include "AuthFile.h"

#define kPWReplicaFile						"/var/db/authserver/authserverreplicas"
#define kPWReplicaFileTemp					"/var/db/authserver/authserverreplicas.temp"
#define kPWReplicaRemoteFilePrefix			"/var/db/authserver/authserverreplicas.remote."

#define kPWReplicaParentKey					"Parent"
#define kPWReplicaReplicaKey				"Replicas"
#define kPWReplicaDecommissionedListKey		"DecommissionedReplicas"
#define kPWReplicaIDKey						"ID"
#define kPWReplicaCurrentServerForLDAPKey	"CurrentServerForLDAP"

#define kPWReplicaIPKey						"IP"
#define kPWReplicaDNSKey					"DNS"
#define kPWReplicaNameKey					"ReplicaName"
#define kPWReplicaPolicyKey					"ReplicaPolicy"
#define kPWReplicaStatusKey					"ReplicaStatus"
#define kPWReplicaNameValuePrefix			"Replica"
#define kPWReplicaSyncDateKey				"LastSyncDate"
#define kPWReplicaSyncServerKey				"LastSyncServer"
#define kPWReplicaSyncTIDKey				"LastSyncTID"
#define kPWReplicaStatusAllow				"AllowReplication"
#define kPWReplicaStatusUseACL				"UseACL"
#define kPWReplicaIDRangeBeginKey			"IDRangeBegin"
#define kPWReplicaIDRangeEndKey				"IDRangeEnd"
#define kPWReplicaSyncAttemptKey			"LastSyncFailedAttempt"
#define kPWReplicaIncompletePullKey			"PullStatus"
#define kPWReplicaPullDeferred				"PullDeferred"
#define kPWReplicaEntryModDateKey			"EntryModDate"

// values
#define kPWReplicaPolicyDefaultKey			"SyncDefault"
#define kPWReplicaPolicyNeverKey			"SyncNever"
#define kPWReplicaPolicyOnlyIfDesperateKey	"SyncOnlyIfDesperate"
#define kPWReplicaPolicyOnScheduleKey		"SyncOnSchedule"
#define kPWReplicaPolicyOnDirtyKey			"SyncOnDirty"
#define kPWReplicaPolicyAnytimeKey			"SyncAnytime"

#define kPWReplicaStatusActiveValue			"Active"
#define kPWReplicaStatusPermDenyValue		"PermissionDenied"
#define kPWReplicaStatusNotFoundValue		"NotFound"

// other
#define kPWReplicaMaxIPCount				32

typedef UInt8 ReplicaPolicy;
enum {
	kReplicaNone,
	kReplicaAllowAll,
	kReplicaUseACL
};

typedef UInt8 ReplicaSyncPolicy;
enum {
	kReplicaSyncDefault,
	kReplicaSyncNever,
	kReplicaSyncOnlyIfDesperate,
	kReplicaSyncOnSchedule,
	kReplicaSyncOnDirty,
	kReplicaSyncAnytime,
	kReplicaSyncInvalid
};

typedef UInt8 ReplicaStatus;
enum {
	kReplicaActive,
	kReplicaPermissionDenied,
	kReplicaNotFound
};

typedef enum ReplicaRole {
	kReplicaRoleUnset,
	kReplicaRoleParent,
	kReplicaRoleRelay,
	kReplicaRoleTierOneReplica,
	kReplicaRoleTierTwoReplica
} ReplicaRole;

typedef enum ReplicaChangeStatus {
	kReplicaChangeNone			= 0x00,
	kReplicaChangeGeneral		= 0x01,
	kReplicaChangeInterface		= 0x02,
	kReplicaChangeAll			= (kReplicaChangeGeneral | kReplicaChangeInterface)
} ReplicaChangeStatus;

typedef struct PWSReplicaEntry {
	int ipCount;
	char ip[kPWReplicaMaxIPCount][INET6_ADDRSTRLEN];
	char dns[NS_MAXDNAME];
	char name[kPWFileMaxReplicaName];
	ReplicaPolicy policy;
	ReplicaSyncPolicy syncPolicy;
	ReplicaStatus status;
	bool allowReplication;
	UInt32 idRangeBegin;
	UInt32 idRangeEnd;
	SInt64 lastSyncTID;
	uint32_t lastSyncDate;
	uint32_t lastSyncFailedAttempt;
	uint32_t pullIncompleteDate;
	uint32_t pullDeferredDate;
	uint32_t entryModDate;
	bool peer;
} PWSReplicaEntry;

#ifdef __cplusplus
extern "C" {
#endif

bool ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr );
CFStringRef pwsf_GetReplicaStatusString( ReplicaStatus replicaStatus );
void pwsf_CalcServerUniqueID( const char *inRSAPublicKey, char *outHexHash );
char *pwsf_ReplicaFilePath( void );

#ifdef __cplusplus
};
#endif

#endif

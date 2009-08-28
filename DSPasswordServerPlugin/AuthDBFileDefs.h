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

#ifndef __AuthDBFileDefs__
#define __AuthDBFileDefs__

#define kNominalLockInterval				250				// 1/4 second
#define kOneSecondLockInterval				1000			// 1 second
#define kLongerLockInterval					15000			// 15 seconds

#define kKerberosRecordScaleLimit			128

#define kFixedDESChunk						8
#define kMaxWriteSuspendTime				2				// seconds
#define kPWUserIDSize						(4 * sizeof(uint32_t))

#define kPWMoreDataPrefix					"+MORE "
/* Version identification string for identity files. */
#define AUTHFILE_ID_STRING "SSH PRIVATE KEY FILE FORMAT 1.1\n"

#define PWRecIsZero(A)		(((A).time == 0) && ((A).rnd == 0) && ((A).sequenceNumber == 0) && ((A).slot == 0))


// Reposonse Codes (used numerically)
enum {
    kAuthOK = 0,
    kAuthFail = -1,
    kAuthUserDisabled = -2,
    kAuthNeedAdminPrivs = -3,
    kAuthUserNotSet = -4,
    kAuthUserNotAuthenticated = -5,
    kAuthPasswordExpired = -6,
    kAuthPasswordNeedsChange = -7,
    kAuthPasswordNotChangeable = -8,
    kAuthPasswordTooShort = -9,
    kAuthPasswordTooLong = -10,
    kAuthPasswordNeedsAlpha = -11,
    kAuthPasswordNeedsDecimal = -12,
    kAuthMethodTooWeak = -13,
	kAuthPasswordNeedsMixedCase = -14,
	kAuthPasswordHasGuessablePattern = -15,
	kAuthPasswordCannotBeUsername = -16,
	kAuthPasswordNeedsSymbol = -17
};

typedef enum SyncStatus {
	kSyncStatusNoErr					=  0,
	kSyncStatusFail						= -1,
	kSyncStatusIncompatibleDatabases	= -2,
	kSyncStatusServerDatabaseBusy		= -3,
	kSyncStatusKerberosLocked			= -4
} SyncStatus;

typedef enum OverflowAction {
	kOverflowActionRequireNewPassword,
	kOverflowActionDoNotRequireNewPassword,
	kOverflowActionGetFromName,
	kOverflowActionGetFromPrincipal,
	kOverflowActionDumpRecords,
	kOverflowActionPurgeDeadSlots,
	kOverflowActionKerberizeOrNewPassword
} OverflowAction;

typedef enum ReplicationRecordType {
	kDBTypeLastSyncTime,
	kDBTypeHeader,
	kDBTypeSlot,
	kDBTypeKerberosPrincipal,
	kDBTypeAddressChange
} ReplicationRecordType;
#define kDBTypeFirstRecord kDBTypeLastSyncTime
#define kDBTypeLastRecord  kDBTypeAddressChange
#define kDBTypeIsValid(t) ( ((t) >= kDBTypeFirstRecord) && ((t) <= kDBTypeLastRecord) )

typedef enum SyncPriority {
	kSyncPriorityNormal,
	kSyncPriorityDirty,
	kSyncPriorityForce
} SyncPriority;

#endif

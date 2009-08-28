/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef __AUTHFILEPRIV_H__
#define __AUTHFILEPRIV_H__

#include <libkern/OSTypes.h>
#include <TargetConditionals.h>

typedef struct PWFileHeaderCompressed {
    UInt32 signature;								// some value that means this is a real pw file
    UInt32 version;									// version for the file format
    UInt32 entrySize;								// sizeof(PWFileEntry)
    UInt32 sequenceNumber;							// incremented by one each time a pw is saved, never decremented.
    UInt32 numberOfSlotsCurrentlyInFile; 			// last slot for the current file size
    UInt32 deepestSlotUsed;							// if < numberOfSlotsCurrentlyInFile, issue to the end, then look at freelist

    PWGlobalAccessFeatures access;					// password policies that apply to all users unless overridden
    AuthMethName weakAuthMethods[kPWFileMaxWeakMethods];
                                                    // list of methods that are considered too weak for administration
    													
	char replicationName[kPWFileMaxReplicaName];	// the name used by replication to identify this server
	UInt32 deepestSlotUsedByThisServer;				// deepest slot used in this replica's allocated range
	UInt32 accessModDate;							// time of last change to access field in seconds from 1970
	PWGlobalMoreAccessFeatures extraAccess;			// new policy data
} PWFileHeaderCompressed;


typedef struct PWAccessFeaturesCompressed {
#if TARGET_RT_BIG_ENDIAN
    int isDisabled:1;						// TRUE == cannot log in
    int isAdminUser:1;						// TRUE == can modify other slots in the db
    int newPasswordRequired:1;				// TRUE == user is required to change the password at next login
    int usingHistory:1;						// TRUE == user has a password history file
    int canModifyPasswordforSelf:1;			// TRUE == user can modify their own password
    int usingExpirationDate:1;				// TRUE == look at expirationDateGMT
    int usingHardExpirationDate:1;			// TRUE == look at hardExpirationDateGMT
    int requiresAlpha:1;					// TRUE == password must have one char in [A-Z], [a-z]
    int requiresNumeric:1;					// TRUE == password must have one char in [0-9]
    
	// db 1.1
	int passwordIsHash:1;
	int passwordCannotBeName:1;
	unsigned int historyCount:4;
	int isSessionKeyAgent:1;				// the user can retrieve (MPPE) session keys
#else
    int requiresAlpha:1;					// TRUE == password must have one char in [A-Z], [a-z]
    int usingHardExpirationDate:1;			// TRUE == look at hardExpirationDateGMT
    int usingExpirationDate:1;				// TRUE == look at expirationDateGMT
    int canModifyPasswordforSelf:1;			// TRUE == user can modify their own password
    int usingHistory:1;						// TRUE == user has a password history file
    int newPasswordRequired:1;				// TRUE == user is required to change the password at next login
    int isAdminUser:1;						// TRUE == can modify other slots in the db
    int isDisabled:1;						// TRUE == cannot log in
	int isSessionKeyAgent:1;				// the user can retrieve (MPPE) session keys
	unsigned int historyCount:4;
	int passwordCannotBeName:1;
	int passwordIsHash:1;
    int requiresNumeric:1;					// TRUE == password must have one char in [0-9]
#endif
    
    int32_t expirationDateGMT;
	int32_t hardExpireDateGMT;
    
    UInt32 maxMinutesUntilChangePassword;	// if exceeded, users must change password. 0==not used
    UInt32 maxMinutesUntilDisabled;			// if exceeded, users are disabled. 0==not used
    UInt32 maxMinutesOfNonUse;				// if exceeded, user is disabled. 0==not used
    UInt16 maxFailedLoginAttempts;			// if exceeded, user is disabled. 0==not used
    
    UInt16 minChars;						// minimum characters in a valid password
    UInt16 maxChars;						// maximum characters in a valid password
    
} PWAccessFeaturesCompressed;

typedef struct PWFileEntryCompressed {
    UInt32 time;
    UInt32 rnd;
    UInt32 sequenceNumber;
    UInt32 slot;
	
    int32_t creationDate;
    int32_t modificationDate;
    int32_t modDateOfPassword;
    int32_t lastLogin;
    UInt16 failedLoginAttempts;
    
    PWAccessFeaturesCompressed access;
    
	uuid_t userGUID;
	char userdata[393];
	PWDisableReasonCode disableReason;
	PWMoreAccessFeatures extraAccess;

	// variable length data: passwordStr, digest, usernameStr, group list
	char data[1];
} PWFileEntryCompressed;

#endif

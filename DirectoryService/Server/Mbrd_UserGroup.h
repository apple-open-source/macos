/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __Mbrd_UserGroup_h__
#define	__Mbrd_UserGroup_h__		1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include "DSmemberdMIG_types.h"

struct HashTable;
struct UserGroup;
 
typedef struct UserGroup
{
	uint32_t			fExpiration;
	uint32_t			fLoginExpiration;
	guid_t				fGUID;
	ntsid_t*			fSID;
	uid_t				fID;
	gid_t				fPrimaryGroup;
	char*				fName;
	struct HashTable*   fGUIDMembershipHash;
	struct HashTable*   fSIDMembershipHash;
	struct HashTable*   fGIDMembershipHash;
	uint16_t			fRefCount;
	uint16_t			fCheckVal;
	struct UserGroup*   fLink;
	struct UserGroup*   fBackLink;
	char				fIsUser;
	char				fIsComputer;
	char				fIsLocalAccount;
	char				fNotFound;
} UserGroup;

extern int gLoginExpiration;

#define kUseLoginTimeOutMask 1

__BEGIN_DECLS

void Mbrd_InitializeUserGroup(int numToCache, int defaultExpiration, int defaultNegativeExpiration, int loginExp);

void OpenDirService(void);
void Mbrd_ResetCache(void);
void Mbrd_DumpState(void);

uint32_t GetElapsedSeconds(void);
uint64_t GetElapsedMicroSeconds(void);
void AddToAverage(uint32_t* average, uint32_t* numDataPoints, uint32_t newDataPoint);

void SetThreadFlags(int flags);

UserGroup* GetItemWithGUID(guid_t* guid);
UserGroup* GetItemWithSID(ntsid_t* sid);
UserGroup* GetUserWithUID(int uid);
UserGroup* GetGroupWithGID(int gid);
UserGroup* GetUserWithName(char* name);
UserGroup* GetGroupWithName(char* name);

int IsUserMemberOfGroupByGUID(UserGroup* user, guid_t* groupGUID);
int IsUserMemberOfGroupBySID(UserGroup* user, ntsid_t* groupSID);
int IsUserMemberOfGroupByGID(UserGroup* user, int gid);

int Get16Groups(UserGroup* user, gid_t* gidArray);
int GetAllGroups(UserGroup* user, gid_t** gidArray);

void ConvertGUIDToString(guid_t* data, char* string);
void ConvertSIDToString(char* string, ntsid_t* sid);

void TouchItem(UserGroup* item);

__END_DECLS

#endif

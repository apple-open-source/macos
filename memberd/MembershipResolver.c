/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#import "MembershipResolver.h"

#import "UserGroup.h"
#import <sys/syslog.h>
#import <libkern/OSByteOrder.h>

void SwapRequest(struct kauth_identity_extlookup* request)
{
	int i;

	// swap 32 bit values
	request->el_seqno = OSSwapInt32(request->el_seqno);
	request->el_result = OSSwapInt32(request->el_result);
	request->el_flags = OSSwapInt32(request->el_flags);
	request->el_uid = OSSwapInt32(request->el_uid);
	request->el_uguid_valid = OSSwapInt32(request->el_uguid_valid);
	request->el_usid_valid = OSSwapInt32(request->el_usid_valid);
	request->el_gid = OSSwapInt32(request->el_gid);
	request->el_gguid_valid = OSSwapInt32(request->el_gguid_valid);
	request->el_member_valid = OSSwapInt32(request->el_member_valid);

	// uuids don't need swapping, and only the sid_authorities part of the sids do
	for (i = 0; i < KAUTH_NTSID_MAX_AUTHORITIES; i++)
	{
		request->el_usid.sid_authorities[i] = OSSwapInt32(request->el_usid.sid_authorities[i]);
		request->el_gsid.sid_authorities[i] = OSSwapInt32(request->el_gsid.sid_authorities[i]);
	}
}

void ProcessLookup(struct kauth_identity_extlookup* request)
{
	uint32_t flags = request->el_flags;
	UserGroup* user = NULL;
	UserGroup* group = NULL;
	int isMember = -1;
	uint64_t microsec = GetElapsedMicroSeconds();
	
	if (flags & (1<<15))
		SetThreadFlags(kUseLoginTimeOutMask);
	
	if (flags & KAUTH_EXTLOOKUP_VALID_UGUID)
		user = GetItemWithGUID(&request->el_uguid);
	else if (flags & KAUTH_EXTLOOKUP_VALID_USID)
		user = GetItemWithSID(&request->el_usid);
	else if (flags & KAUTH_EXTLOOKUP_VALID_UID)
		user = GetUserWithUID(request->el_uid);
	
	if (user != NULL && !user->fIsUser)
		user = NULL;
				
	if (user != NULL)
	{
		user->fRefCount++;
//		printf("Found user %s (%d)\n", user->fName, user->fID);
	}
		
	if ((flags & KAUTH_EXTLOOKUP_WANT_MEMBERSHIP) && (user != NULL))
	{
		request->el_member_valid = gLoginExpiration;
		if (flags & KAUTH_EXTLOOKUP_VALID_GGUID)
		{
			isMember = IsUserMemberOfGroupByGUID(user, &request->el_gguid);
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GSID)
		{
			isMember = IsUserMemberOfGroupBySID(user,  &request->el_gsid);
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GID)
		{
			isMember = IsUserMemberOfGroupByGID(user, request->el_gid);
//			printf("Checked if user if member of group %d, returned %d\n", request->el_gid, isMember);
		}
			
		if (isMember != -1)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_MEMBERSHIP;
			if (isMember)
				request->el_flags |= KAUTH_EXTLOOKUP_ISMEMBER;
		}
	}
	
	if (user != NULL)
	{
		request->el_uguid_valid  = request->el_usid_valid = gLoginExpiration;
		if (flags & KAUTH_EXTLOOKUP_WANT_UID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_UID;
			request->el_uid = user->fID;
		}
		if (flags & KAUTH_EXTLOOKUP_WANT_UGUID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_UGUID;
			memcpy(&request->el_uguid, &user->fGUID, sizeof(guid_t));
		}
		if ((flags & KAUTH_EXTLOOKUP_WANT_USID) && (user->fSID != NULL))
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_USID;
			memcpy(&request->el_usid, user->fSID, sizeof(ntsid_t));
		}

		user->fRefCount--;
	}
	
	if (flags & (KAUTH_EXTLOOKUP_WANT_GID | KAUTH_EXTLOOKUP_WANT_GGUID | KAUTH_EXTLOOKUP_WANT_GSID))
	{
		if (flags & KAUTH_EXTLOOKUP_VALID_GGUID)
			group = GetItemWithGUID(&request->el_gguid);
		else if (flags & KAUTH_EXTLOOKUP_VALID_GSID)
			group = GetItemWithSID(&request->el_gsid);
		else if (flags & KAUTH_EXTLOOKUP_VALID_GID)
			group = GetGroupWithGID(request->el_gid);
	}
	
	if (group != NULL)
	{
		request->el_gguid_valid  = request->el_gsid_valid = gLoginExpiration;
		if ((flags & KAUTH_EXTLOOKUP_WANT_GID) && !group->fIsUser)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GID;
			request->el_gid = group->fID;
		}
		if (flags & KAUTH_EXTLOOKUP_WANT_GGUID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GGUID;
			memcpy(&request->el_gguid, &group->fGUID, sizeof(guid_t));
		}
		if ((flags & KAUTH_EXTLOOKUP_WANT_GSID) && (group->fSID != NULL))
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GSID;
			memcpy(&request->el_gsid, group->fSID, sizeof(ntsid_t));
		}
	}
	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);
	request->el_result = KAUTH_EXTLOOKUP_SUCCESS;
	SetThreadFlags(0);
}

int ProcessGetGroups(uint32_t uid, uint32_t* numGroups, GIDArray gids)
{
	uint64_t microsec = GetElapsedMicroSeconds();
	int result = KERN_SUCCESS;

	UserGroup* user = GetUserWithUID(uid);
	
	*numGroups = 0;

	if (user == NULL)
	{
		result = KERN_FAILURE;
		syslog(LOG_ERR, "GetGroups couldn't find uid %d", uid);
	}
	else
	{
		*numGroups = Get16Groups(user, gids);
	}
	
	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);
	return result;
}

int ProcessMapName(uint8_t isUser, char* name, guid_t* guid)
{
	uint64_t microsec = GetElapsedMicroSeconds();
	int result = KERN_SUCCESS;
	UserGroup* item;
	if (isUser)
		item = GetUserWithName(name);
	else
		item = GetGroupWithName(name);
		
	if (item == NULL)
		result = KERN_FAILURE;
	else
		memcpy(guid, &item->fGUID, sizeof(guid_t));
		
	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);

	return result;
}

void ProcessResetCache()
{
	ResetCache();
}

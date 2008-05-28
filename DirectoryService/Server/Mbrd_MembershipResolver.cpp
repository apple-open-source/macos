/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#include "Mbrd_MembershipResolver.h"
#include "Mbrd_UserGroup.h"
#include <sys/syslog.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_error.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>			// for fcntl() and O_* flags
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include <DirectoryServiceCore/CLog.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>

#define kMaxGUIDSInCacheClient 500
#define kMaxGUIDSInCacheServer 2500
#define kDefaultExpirationServer 1*60*60
#define kDefaultNegativeExpirationServer 30*60
#define kDefaultExpirationClient 4*60*60
#define kDefaultNegativeExpirationClient 2*60*60
#define kDefaultLoginExpiration 2*60
#define kMaxItemsInCacheStr "MaxItemsInCache"
#define kDefaultExpirationStr "DefaultExpirationInSecs"
#define kDefaultNegativeExpirationStr "DefaultFailureExpirationInSecs"
#define kDefaultLoginExpirationStr "DefaultLoginExpirationInSecs"

extern dsBool	gDSLocalOnlyMode;
extern dsBool	gDSInstallDaemonMode;
extern StatBlock *gStatBlock;
DSMutexSemaphore gMbrGlobalMutex("::gMbrGlobalMutex");

static const uint8_t _mbr_root_uuid[] = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};

void Mbrd_SwapRequest(struct kauth_identity_extlookup* request)
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

void Mbrd_InitializeGlobals( void )
{
	char* path = "/etc/memberd.conf";
	char buffer[1024];
	int fd;
	size_t len;
	int rewriteConfig = 0;
	struct stat sb;
	int maxCache = kMaxGUIDSInCacheClient;
	int defaultExpiration = kDefaultExpirationClient;
	int defaultNegExpiration = kDefaultNegativeExpirationClient;
	int loginExpiration = kDefaultLoginExpiration;
	
	if (stat("/System/Library/CoreServices/ServerVersion.plist", &sb) == 0)
	{
		maxCache = kMaxGUIDSInCacheServer;
		defaultExpiration = kDefaultExpirationServer;
		defaultNegExpiration = kDefaultNegativeExpirationServer;
	}
	
	// if this is the installer or local only mode, we ignore the .conf file because we are probably read-only
	if ( !gDSInstallDaemonMode && !gDSLocalOnlyMode )
	{
		int result = stat(path, &sb);
		
		if ((result != 0) || (sb.st_size > 1023))
			rewriteConfig = 1;
		else
		{
			fd = open(path, O_RDONLY, 0);
			if (fd < 0) goto initialize;
			len = read(fd, buffer, sb.st_size);
			close(fd);
			if (strncmp(buffer, "#1.1", 4) != 0)
				rewriteConfig = 1;
		}
		
		if (rewriteConfig)
		{
			fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0755);
			if (fd < 0) goto initialize;
			sprintf(buffer, "#1.1\n%s %d\n%s %d\n%s %d\n%s %d\n",	kMaxItemsInCacheStr, maxCache, 
					kDefaultExpirationStr, defaultExpiration, 
					kDefaultNegativeExpirationStr, defaultNegExpiration,
					kDefaultLoginExpirationStr, loginExpiration);
			len = write(fd, buffer, strlen(buffer));
			close(fd);
		}
		else
		{
			char* temp;
			size_t i;
			
			if (len != sb.st_size) goto initialize;
			buffer[len] = '\0';
			
			for (i = 0; i < len; i++)
				if (buffer[i] == '\n') buffer[i] = '\0';
			
			i = 0;
			while (i < len)
			{
				temp = buffer + i;
				if (strncmp(temp, kMaxItemsInCacheStr, strlen(kMaxItemsInCacheStr)) == 0)
				{
					temp += strlen(kMaxItemsInCacheStr);
					maxCache = strtol(temp, &temp, 10);
					if (maxCache < 250)
						maxCache = 250;
					else if (maxCache > 1000000)
						maxCache = 1000000;
				}
				else if (strncmp(temp, kDefaultExpirationStr, strlen(kDefaultExpirationStr)) == 0)
				{
					temp += strlen(kDefaultExpirationStr);
					defaultExpiration = strtol(temp, &temp, 10);
					if (defaultExpiration < 30)
						defaultExpiration = 30;
					else if (defaultExpiration > 24 * 60 * 60)
						defaultExpiration = 24 * 60 * 60;
				}
				else if (strncmp(temp, kDefaultNegativeExpirationStr, strlen(kDefaultNegativeExpirationStr)) == 0)
				{
					temp += strlen(kDefaultNegativeExpirationStr);
					defaultNegExpiration = strtol(temp, &temp, 10);
					if (defaultNegExpiration < 30)
						defaultNegExpiration = 30;
					else if (defaultNegExpiration > 24 * 60 * 60)
						defaultNegExpiration = 24 * 60 * 60;
				}
				else if (strncmp(temp, kDefaultLoginExpirationStr, strlen(kDefaultLoginExpirationStr)) == 0)
				{
					temp += strlen(kDefaultLoginExpirationStr);
					loginExpiration = strtol(temp, &temp, 10);
					if (loginExpiration < 30)
						loginExpiration = 30;
					else if (loginExpiration > 1 * 60 * 60)
						loginExpiration = 1 * 60 * 60;
				}
				
				i += strlen(temp) + 1;
			}
		}
	}
	
initialize:
	
	Mbrd_InitializeUserGroup( maxCache, defaultExpiration, defaultNegExpiration, loginExpiration );
}

void Mbrd_ProcessLookup(struct kauth_identity_extlookup* request)
{
	uint32_t flags = request->el_flags;
	UserGroup* user = NULL;
	UserGroup* group = NULL;
	int isMember = -1;
	uint64_t microsec = GetElapsedMicroSeconds();
	
	// let's see if this is something related to root that we can just answer
	//
	// ensure we don't have any other bits set, if we do, we have more work and cannot shortcut the process, 
	// but if this is just a lookup then we can use default for UUID FFFFEEEE-DDDD-CCCC-BBBB-AAAA00000000 and UID 0
	if ( 0 == (flags & ~(KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_UID | KAUTH_EXTLOOKUP_WANT_GID)) && 
		 (flags & (KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_WANT_UID)) == (KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_WANT_UID) &&
		 memcmp(&request->el_uguid, _mbr_root_uuid, sizeof(guid_t)) == 0 )
	{
		request->el_flags |= KAUTH_EXTLOOKUP_VALID_UID;
		request->el_uid = 0;
		request->el_result = KAUTH_EXTLOOKUP_SUCCESS;
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - UUID FFFFEEEE-DDDD-CCCC-BBBB-AAAA00000000 default answer 0 (root)" );
		return;
	}
	else if ( flags == (flags & (KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_WANT_UGUID)) && request->el_uid == 0 )
	{
		request->el_flags |= KAUTH_EXTLOOKUP_VALID_UGUID;
		memcpy( &request->el_uguid, _mbr_root_uuid, sizeof(guid_t) );
		request->el_result = KAUTH_EXTLOOKUP_SUCCESS;
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - UID 0 default answer FFFFEEEE-DDDD-CCCC-BBBB-AAAA00000000 (root)" );
		return;
	}
	
	if (flags & (1<<15))
		SetThreadFlags( kUseLoginTimeOutMask );
	
	gMbrGlobalMutex.WaitLock();

	if (flags & KAUTH_EXTLOOKUP_VALID_UGUID)
	{
		user = GetItemWithGUID(&request->el_uguid);
		
		if ( LoggingEnabled(kLogPlugin) )
		{
			char guidString[37] = { 0, };
			uuid_unparse( request->el_uguid.g_guid, guidString );
			DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - user/computer GUID %s - %s %s", guidString, (user != NULL ? "succeeded" : "failed"), 
				   (user != NULL ? user->fName : "") );
		}
	}
	else if (flags & KAUTH_EXTLOOKUP_VALID_USID)
	{
		user = GetItemWithSID(&request->el_usid);
		
		if ( LoggingEnabled(kLogPlugin) )
		{
			char sidString[256] = { 0, };
			ConvertSIDToString( sidString, &request->el_usid );
			DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - user/computer SID %s - %s %s", sidString, (user != NULL ? "succeeded" : "failed"), 
				   (user != NULL ? user->fName : "") );
		}
	}
	else if (flags & KAUTH_EXTLOOKUP_VALID_UID)
	{
		user = GetUserWithUID(request->el_uid);
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - user/computer ID %d - %s %s", request->el_uid, (user != NULL ? "succeeded" : "failed"), 
			   (user != NULL ? user->fName : "") );
	}
	
	if (user != NULL && !user->fIsUser && !user->fIsComputer)
	{
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - result rejected was not a user/computer" );
		user = NULL;
	}
				
	if (user != NULL)
		user->fRefCount++;
		
	if ((flags & KAUTH_EXTLOOKUP_WANT_MEMBERSHIP) && (user != NULL))
	{
		request->el_member_valid = gLoginExpiration;
		if (flags & KAUTH_EXTLOOKUP_VALID_GGUID)
		{
			isMember = IsUserMemberOfGroupByGUID(user, &request->el_gguid);
			
			if ( LoggingEnabled(kLogPlugin) )
			{
				char guidString[37] = { 0, };
				uuid_unparse( request->el_gguid.g_guid, guidString );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - Membership - is %s %s member of group GUID %s = %s", (user->fIsUser ? "user" : "computer"), 
					   user->fName, guidString, (isMember == 1 ? "true" : "false") );
			}
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GSID)
		{
			isMember = IsUserMemberOfGroupBySID(user,  &request->el_gsid);
			
			if ( LoggingEnabled(kLogPlugin) )
			{
				char sidString[256] = { 0, };
				ConvertSIDToString( sidString, &request->el_gsid );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - Membership - is %s %s member of group SID %s = %s", (user->fIsUser ? "user" : "computer"), 
					    user->fName, sidString, (isMember == 1 ? "true" : "false") );
			}
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GID)
		{
			isMember = IsUserMemberOfGroupByGID(user, request->el_gid);
			DbgLog( kLogPlugin, "mbrmig - Dispatch - Membership - is %s %s member of group GID %d = %s", (user->fIsUser ? "user" : "computer"), 
					user->fName, request->el_gid, (isMember == 1 ? "true" : "false") );
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
			DbgLog( kLogPlugin, "mbrmig - Dispatch - WantUID - found %d - %s", user->fID, user->fName );

		}
		if (flags & KAUTH_EXTLOOKUP_WANT_UGUID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_UGUID;
			memcpy(&request->el_uguid, &user->fGUID, sizeof(guid_t));

			if ( LoggingEnabled(kLogPlugin) )
			{
				char guidString[37] = { 0, };
				uuid_unparse( user->fGUID.g_guid, guidString );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - WantGUID - found %s - %s", guidString, user->fName );
			}
		}
		if ((flags & KAUTH_EXTLOOKUP_WANT_USID) && (user->fSID != NULL))
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_USID;
			memcpy(&request->el_usid, user->fSID, sizeof(ntsid_t));

			if ( LoggingEnabled(kLogPlugin) )
			{
				char sidString[256] = { 0, };
				ConvertSIDToString( sidString, user->fSID );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - WantSID - found %s - %s", sidString, user->fName );
			}
		}

		user->fRefCount--;
	}
	
	if (flags & (KAUTH_EXTLOOKUP_WANT_GID | KAUTH_EXTLOOKUP_WANT_GGUID | KAUTH_EXTLOOKUP_WANT_GSID))
	{
		if (flags & KAUTH_EXTLOOKUP_VALID_GGUID)
		{
			group = GetItemWithGUID(&request->el_gguid);
			
			if ( LoggingEnabled(kLogPlugin) )
			{
				char guidString[37] = { 0, };
				uuid_unparse( request->el_gguid.g_guid, guidString );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - group GUID %s - %s %s", guidString, (group != NULL ? "succeeded" : "failed"), 
					   (group != NULL ? group->fName : "") );
			}
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GSID)
		{
			group = GetItemWithSID(&request->el_gsid);

			if ( LoggingEnabled(kLogPlugin) )
			{
				char sidString[256] = { 0, };
				ConvertSIDToString( sidString, &request->el_gsid );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - group SID %s - %s %s", sidString, (group != NULL ? "succeeded" : "failed"), 
					   (group != NULL ? group->fName : "") );
			}
		}
		else if (flags & KAUTH_EXTLOOKUP_VALID_GID)
		{
			group = GetGroupWithGID(request->el_gid);
			
			DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - group GID %d - %s %s", request->el_gid, (group != NULL ? "succeeded" : "failed"), 
				   (group != NULL ? group->fName : "") );
		}
	}
	
	if ( group != NULL && (group->fIsUser || group->fIsComputer) )
	{
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Lookup - result rejected was not a group" );
		group = NULL;
	}
	
	if (group != NULL)
	{
		request->el_gguid_valid  = request->el_gsid_valid = gLoginExpiration;
		if (flags & KAUTH_EXTLOOKUP_WANT_GID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GID;
			request->el_gid = group->fID;
			
			DbgLog( kLogPlugin, "mbrmig - Dispatch - WantGID - found %d - %s", group->fID, group->fName );
		}
		if (flags & KAUTH_EXTLOOKUP_WANT_GGUID)
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GGUID;
			memcpy(&request->el_gguid, &group->fGUID, sizeof(guid_t));

			if ( LoggingEnabled(kLogPlugin) )
			{
				char guidString[37] = { 0, };
				uuid_unparse( group->fGUID.g_guid, guidString );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - WantGGUID - found %s - %s", guidString, group->fName );
			}
		}
		if ((flags & KAUTH_EXTLOOKUP_WANT_GSID) && (group->fSID != NULL))
		{
			request->el_flags |= KAUTH_EXTLOOKUP_VALID_GSID;
			memcpy(&request->el_gsid, group->fSID, sizeof(ntsid_t));

			if ( LoggingEnabled(kLogPlugin) )
			{
				char sidString[256] = { 0, };
				ConvertSIDToString( sidString, group->fSID );
				DbgLog( kLogPlugin, "mbrmig - Dispatch - WantGSID - found %s - %s", sidString, group->fName );
			}
		}
	}

	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);
	request->el_result = KAUTH_EXTLOOKUP_SUCCESS;

	gMbrGlobalMutex.SignalLock();

	SetThreadFlags( 0 );
}

int Mbrd_ProcessGetGroups(uint32_t uid, uint32_t* numGroups, GIDArray gids)
{
	uint64_t microsec = GetElapsedMicroSeconds();
	int result = KERN_SUCCESS;

	gMbrGlobalMutex.WaitLock();

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

	gMbrGlobalMutex.SignalLock();

	return result;
}

int Mbrd_ProcessGetAllGroups(uint32_t uid, uint32_t *numGroups, GIDList *gids )
{
	uint64_t microsec = GetElapsedMicroSeconds();
	int result = KERN_SUCCESS;
		
	gMbrGlobalMutex.WaitLock();
	
	UserGroup* user = GetUserWithUID(uid);
	
	*numGroups = 0;
	
	if (user == NULL)
	{
		result = KERN_FAILURE;
		syslog(LOG_ERR, "GetGroups couldn't find uid %d", uid);
	}
	else
	{
		*numGroups = GetAllGroups(user, gids);
	}
	
	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);
	
	gMbrGlobalMutex.SignalLock();
		
	return result;
}

int Mbrd_ProcessMapName(uint8_t isUser, char* name, guid_t* guid)
{
	uint64_t microsec = GetElapsedMicroSeconds();
	int result = KERN_SUCCESS;
	UserGroup* item = NULL;

	gMbrGlobalMutex.WaitLock();

	if ( isUser && strcmp(name, "root") == 0 )
	{
		DbgLog( kLogPlugin, "mbrmig - Dispatch - Map name using default UUID for root - FFFFEEEE-DDDD-CCCC-BBBB-AAAA00000000" );
		memcpy( guid, _mbr_root_uuid, sizeof(guid_t) );
	}
	else
	{
		if (isUser)
			item = GetUserWithName(name);
		else
			item = GetGroupWithName(name);
		
		if (item == NULL)
			result = KERN_FAILURE;
		else
			memcpy(guid, &item->fGUID, sizeof(guid_t));
	}
		
	microsec = GetElapsedMicroSeconds() - microsec;
	AddToAverage(&gStatBlock->fAverageuSecPerCall, &gStatBlock->fTotalCallsHandled, (uint32_t)microsec);

	gMbrGlobalMutex.SignalLock();

	return result;
}

void dsFlushMembershipCache(void)
{
	Mbrd_ProcessResetCache();
}

void Mbrd_ProcessResetCache(void)
{
	gMbrGlobalMutex.WaitLock();
	Mbrd_ResetCache();
	gMbrGlobalMutex.SignalLock();
}

void Mbrd_ProcessGetStats(StatBlock *stats)
{
	gMbrGlobalMutex.WaitLock();
	memcpy(stats, gStatBlock, sizeof(StatBlock));
	gMbrGlobalMutex.SignalLock();
	stats->fTotalUpTime = GetElapsedSeconds() - stats->fTotalUpTime;
}

void Mbrd_ProcessResetStats(void)
{
	gMbrGlobalMutex.WaitLock();
	memset(gStatBlock, 0, sizeof(StatBlock));
	gMbrGlobalMutex.SignalLock();
}

void Mbrd_ProcessDumpState(void)
{
	gMbrGlobalMutex.WaitLock();
	Mbrd_DumpState();
	gMbrGlobalMutex.SignalLock();
}

void Mbrd_Initialize( void )
{
	bool gMbrdInitComplete = false;
	
	gMbrGlobalMutex.WaitLock();
	if ( !gMbrdInitComplete )
	{
		OpenDirService();

		do {
			if (GetUserWithUID(0) != NULL)
			{
				break;
			}
			else
			{
				syslog(LOG_CRIT, "Couldn't find root user.  Sleeping and trying again.");
				sleep(1);
				Mbrd_ResetCache();
			}
		} while (1);
		gMbrdInitComplete = true;
	}
	gMbrGlobalMutex.SignalLock();
}

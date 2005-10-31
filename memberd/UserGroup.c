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

#import "UserGroup.h"
#import "HashTable.h"
#import "Listener.h"

#import <stdlib.h>
#import <string.h>
#import <stdio.h>
#import <sys/stat.h>
#import <sys/syslog.h>
#import <unistd.h>
#import <libkern/OSByteOrder.h>

#import <DirectoryService/DirServices.h>
#import <DirectoryService/DirServicesConst.h>
#import <DirectoryService/DirServicesUtils.h>

#ifndef kDSNAttrGroupMembers
#define kDSNAttrGroupMembers "dsAttrTypeNative:GroupMembers"
#endif

#ifndef kDSNAttrNestedGroups
#define kDSNAttrNestedGroups "dsAttrTypeNative:NestedGroups"
#endif

#ifndef kDS1AttrTimeToLive
#define kDS1AttrTimeToLive "dsAttrTypeNative:ttl"
#endif

#ifndef kDS1AttrSMBSID
#define kDS1AttrSMBSID "dsAttrTypeNative:sid"
#endif

tDirReference gDirRef = 0;
tDirNodeReference gSearchNode = 0;

#define TYPE_USER 0
#define TYPE_GROUP 1
#define TYPE_BOTH 2

int gMaxGUIDSInCache;
int gDefaultExpiration;
int gDefaultNegativeExpiration;
int gLoginExpiration;

UserGroup* gListHead;
UserGroup* gListTail;
long gNumItemsInCache;

HashTable* gGUIDHash;
HashTable* gSIDHash;
HashTable* gUIDHash;
HashTable* gGIDHash;

HashTable* gUserNameHash;
HashTable* gGroupNameHash;

guid_t gEveryoneGuid;
ntsid_t gEveryoneSID;

void ResetUserGroup(UserGroup* ug);
UserGroup* DoRecordSearch(int recordType, char* attribute, char* value, UserGroup* membershipRoot);
void AddToHeadOfList(UserGroup* ug);
void RemoveFromList(UserGroup* ug);
uid_t GetTempIDForGUID(guid_t* guid);
uid_t GetTempIDForSID(ntsid_t* sid);
bool FindTempID(uid_t id, guid_t** guid, ntsid_t** sid);
void ConvertGUIDToString(guid_t* data, char* string);
void ConvertGUIDFromString(char* string, guid_t* destination);
void ConvertSIDToString(char* string, ntsid_t* sid);
void ConvertSIDFromString(char* string, ntsid_t* sid);
void GenerateItemMembership(UserGroup* item);
UserGroup* GetNewUGStruct();

#define kUUIDBlock 1
#define kSmallSIDBlock 2
#define kLargeSIDBlock 3

typedef struct TempUIDCacheBlockBase
{
	struct TempUIDCacheBlockBase* fNext;
	uid_t fKind;
	int fNumIDs;
	uid_t fStartID;
} TempUIDCacheBlockBase;

typedef struct TempUIDCacheBlockSmall
{
	struct TempUIDCacheBlock* fNext;
	uid_t fKind;
	int fNumIDs;
	uid_t fStartID;
	guid_t fGUIDs[1024];
} TempUIDCacheBlockSmall;

typedef struct TempUIDCacheBlockLarge
{
	struct TempUIDCacheBlock* fNext;
	uid_t fKind;
	int fNumIDs;
	uid_t fStartID;
	ntsid_t fSIDs[1024];
} TempUIDCacheBlockLarge;

TempUIDCacheBlockBase* gUIDCache = NULL;

#define kSearchByID 0x1
#define kSearchByUUID 0x2
#define kSearchBySID 0x3
#define kSearchByName 0x4
#define kCacheReset 0x5

#define kFoundItem 0x0100
#define kItemInCache 0x0200
#define kMembershipCheck 0x0400

typedef struct LogEntry
{
	u_int16_t fEntryType;
	u_int16_t fCheckVal;
	uid_t fID;
	UserGroup* fItem;
	time_t fTimestamp;
} LogEntry;

LogEntry* gLog = NULL;
int gLogSize = 0;
int curLogPtr = 0;

void PrintTime(FILE* file, time_t time);
void AddLogEntry(int kind, UserGroup* item, guid_t* extraID);

void OpenDirService()
{
	tDataBufferPtr  nodeBuffer;
	tContextData localContext = NULL;
	tDataListPtr nodeName = NULL; //dsDataListAllocate( fDirRef );
	unsigned long returnCount;
	tDirStatus status = eDSNoErr;
	
	if (gDirRef != 0)
		dsCloseDirService(gDirRef);
	
	dsOpenDirService(&gDirRef);
	nodeBuffer = dsDataBufferAllocate( gDirRef, 4096 );
	status = dsFindDirNodes(gDirRef, nodeBuffer, NULL, eDSSearchNodeName, &returnCount, &localContext);
	if (status != 0 || returnCount == 0)
	{
		dsDataBufferDeAllocate(gDirRef, nodeBuffer);
		syslog(LOG_CRIT, "dsFindDirNodes returned %d, count = %d", status, returnCount);
		return;
	}
	status = dsGetDirNodeName(gDirRef, nodeBuffer, 1, &nodeName); //Currently we only look at the 1st node.
	if (status != 0)
	{
		dsDataBufferDeAllocate(gDirRef, nodeBuffer);
		syslog(LOG_CRIT, "dsGetDirNodeName returned %d", status);
		return;
	}
	status = dsOpenDirNode(gDirRef, nodeName, &gSearchNode);
	if (status != 0)
		syslog(LOG_CRIT, "dsOpenDirNode returned %d", status);
	dsDataBufferDeAllocate(gDirRef, nodeBuffer);
}

void InitializeUserGroup(int numToCache, int defaultExpiration, int defaultNegativeExpiration, int logSize, int loginExp)
{
	if (gDebug)
	{
		PrintTime(stderr, time(NULL));
		fprintf(stderr, "memberd started\n");
	}

	gMaxGUIDSInCache = numToCache;
	gDefaultExpiration = defaultExpiration;
	gDefaultNegativeExpiration = defaultNegativeExpiration;
	gLogSize = logSize;
	gLog = (LogEntry*)malloc(gLogSize * sizeof(LogEntry));
	memset(gLog, 0, gLogSize * sizeof(LogEntry));
	gLoginExpiration = loginExp;

	gListHead = NULL;
	gListTail = NULL;
	gNumItemsInCache = 0;
	
	gGUIDHash = CreateHash(__offsetof(UserGroup, fGUID), sizeof(guid_t), 0, 0);
	gSIDHash = CreateHash(__offsetof(UserGroup, fSID), sizeof(ntsid_t), 1, 0);
	gUIDHash = CreateHash(__offsetof(UserGroup, fID), sizeof(uid_t), 0, 0);
	gGIDHash = CreateHash(__offsetof(UserGroup, fID), sizeof(uid_t), 0, 0);
	gUserNameHash = CreateHash(__offsetof(UserGroup, fName), 0, 1, 0);
	gGroupNameHash = CreateHash(__offsetof(UserGroup, fName), 0, 1, 0);
	
	ConvertGUIDFromString("ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000000C", &gEveryoneGuid);
	ConvertSIDFromString("S-1-1-0", &gEveryoneSID);
	
	gStatBlock = (StatBlock*)malloc(sizeof(StatBlock));
	memset(gStatBlock, 0, sizeof(StatBlock));
	gStatBlock->fTotalUpTime = GetElapsedSeconds();

	do {
		OpenDirService();
		if (GetUserWithUID(0) != NULL)
			break;
		else
		{
			syslog(LOG_CRIT, "Couldn't find root user.  Sleeping and trying again.");
			sleep(5);
			ResetCache();
		}
	} while (1);
}

void ResetCache()
{
	UserGroup* temp = gListHead;
	while (temp != NULL)
	{
		temp->fExpiration = 0;
		temp->fLoginExpiration = 0;
		temp = temp->fLink;
	}
	AddLogEntry(kCacheReset, NULL, NULL);
}

void PrintTime(FILE* file, time_t time)
{
	struct tm *tmTime = localtime (&time);
	fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d %s\t",
				tmTime->tm_year + 1900, tmTime->tm_mon + 1, tmTime->tm_mday,
				tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec,
				tmTime->tm_zone);
}

void PrintLogEntry(FILE* file, LogEntry* entry, guid_t* extraID)
{
	UserGroup* item = entry->fItem;
	u_int16_t type = entry->fEntryType;
	char subtype = type & 0xff;
	char messageBuf[2000];
	char* cur = messageBuf;
				
	PrintTime(file, entry->fTimestamp);
	if (type == kCacheReset)
		sprintf(cur, "cache was reset");
	else if (item != NULL)
	{
		if (item->fCheckVal != entry->fCheckVal)
			sprintf(cur, "unknown item with id %d", entry->fID);
		else
		{
			if (item->fIsUser)
				sprintf(cur, "User");
			else
				sprintf(cur, "Group");
				
			cur += strlen(cur);
			if (item->fName != NULL && strlen(item->fName) < 512)
				sprintf(cur, " '%s'", item->fName);
				
		}
		cur += strlen(cur);
				
		if ((type & kMembershipCheck) == 0)
		{
			if ((type & kFoundItem) == 0)
				sprintf(cur, " not found");
			else
				sprintf(cur, " found");

			cur += strlen(cur);
			if (subtype == kSearchByID)
				sprintf(cur, " using id %d", item->fID);
			else if (subtype == kSearchByUUID)
			{
				char guidString[37];
				ConvertGUIDToString(&item->fGUID, guidString);
				sprintf(cur, " using uuid %s", guidString);
			}
			else if (subtype == kSearchBySID && item->fSID != NULL)
			{
				char sidString[256];
				ConvertSIDToString(sidString, item->fSID);
				sprintf(cur, " using sid %s", sidString);
			}
			else
				sprintf(cur, " by name");

			cur += strlen(cur);
			if ((type & kItemInCache) == 0)
				sprintf(cur, " (result added to cache)");
			else
				sprintf(cur, " (result is from cache)");
		}
		else
		{
			if ((type & kFoundItem) == 0)
				sprintf(cur, " not a member of item");
			else
				sprintf(cur, " is a member of item");

			cur += strlen(cur);
			if (subtype == kSearchByID)
				sprintf(cur, " with id %d", *(uid_t*)extraID);
			else if (subtype == kSearchByUUID)
			{
				char guidString[37];
				ConvertGUIDToString(extraID, guidString);
				sprintf(cur, " with uuid %s", guidString);
			}
			else if (subtype == kSearchBySID)
			{
				char sidString[256];
				ConvertSIDToString(sidString, (ntsid_t*)extraID);
				sprintf(cur, " with sid %s", sidString);
			}
		}
	}
	
	fprintf(file, "%s\n", messageBuf);
	if (file == stderr)
		syslog(LOG_CRIT, "%s", messageBuf);
}

void AddLogEntry(int kind, UserGroup* item, guid_t* extraID)
{
	LogEntry* entry = &gLog[curLogPtr % gLogSize];
	LogEntry* second = NULL;
	// sort of a tricky case.  membership check log entries take up two slots.
	// if we are overwriting the first half of one, zero out the second.
	if (entry->fEntryType & kMembershipCheck)
		gLog[(curLogPtr + 1) % gLogSize].fEntryType = 0;
	entry->fEntryType = kind;
	entry->fTimestamp = time(NULL);
	entry->fItem = item;
	if (item != NULL)
	{
		entry->fCheckVal = item->fCheckVal;
		entry->fID = item->fID;
	}
	
	if (entry->fEntryType & kMembershipCheck)
	{
		second = &gLog[(curLogPtr + 1) % gLogSize];
		if (second->fEntryType & kMembershipCheck)
			gLog[(curLogPtr + 2) % gLogSize].fEntryType = 0;
		memcpy(second, extraID, sizeof(guid_t));
		curLogPtr++;
	}
	curLogPtr++;
	
	if (gDebug)
		PrintLogEntry(stderr, entry, (guid_t*)second);
}

void DumpState(bool dumpLogOnly)
{
	struct stat sb;
	char* logName = "/Library/Logs/memberd_dump.log";
	int i;
	if (stat(logName, &sb) == 0)
	{
		char fileName[30];
		int i = 1;
		while (1)
		{
			sprintf(fileName, "/Library/Logs/memberd-%d.log", i);
			if (stat(fileName, &sb) != 0)
				break;
			i++;
		}
		rename(logName, fileName);
	}
	FILE* dumpFile = fopen(logName, "w");
	i = curLogPtr - gLogSize;
	if (i < 0) i=0;
	while (i < curLogPtr)
	{
		LogEntry* entry = &gLog[i % gLogSize];
		guid_t* extraID = NULL;
		if (entry->fEntryType & kMembershipCheck)
		{
			extraID = (guid_t*)&gLog[(i+1) % gLogSize];
			i++;
		}
		PrintLogEntry(dumpFile, entry, extraID);
		i++;
	}
	
	if (!dumpLogOnly)
	{
		UserGroup* temp = gListHead;

		fprintf(dumpFile, "\nCache dump:\n\n");

		while (temp != NULL)
		{
			char guidString[37];
			char sidString[256];
			char* name = "";
			char* type = temp->fIsUser?"User":"Group";
			
			ConvertGUIDToString(&temp->fGUID, guidString);
			if (temp->fSID != NULL)
				ConvertSIDToString(sidString, temp->fSID);
			else
				sidString[0] = 0;
			
			if (temp->fNotFound)
			{
				if (temp->fName == NULL)
					name = "(doesn't exist)";
				else
				{
					fprintf(dumpFile, "0x%X: %s %s (doesn't exist)\n", (unsigned int)temp, type, temp->fName);
					temp = temp->fLink;
					continue;
				}
			}
			else if (temp->fName != NULL)
				name = temp->fName;
				
			fprintf(dumpFile, "0x%X: %s %s id: %d uuid: %s sid: %s (ref = %d)\n", (unsigned int)temp, type, name, temp->fID, guidString, sidString, temp->fRefCount);
			
			if (temp->fGUIDMembershipHash != NULL)
			{
				UserGroup* groups[250];
				int numResults = GetHashEntries(temp->fGUIDMembershipHash, groups, 250);
				fprintf(dumpFile, "\tMembership:");
				for (i = 0; i < numResults; i++)
				{
					if (i != 0) fprintf(dumpFile, ", ");
					name = (groups[i]->fName != NULL)?groups[i]->fName:"";
					fprintf(dumpFile, "%s(0x%X)", name, (unsigned int)groups[i]);
				}
				fprintf(dumpFile, "\n");
			}
			temp = temp->fLink;
		}
	}
	
	fclose(dumpFile);
}

int ItemOutdated(UserGroup* item)
{
	if ((GetThreadFlags() & kUseLoginTimeOutMask) != 0)
		return (item->fLoginExpiration <= GetElapsedSeconds());
	return (item->fExpiration <= GetElapsedSeconds());
}

UserGroup* GetItem(int recordType, char* idType, void* guidData, ntsid_t* sid, int id, char* name)
{
	// First try and look up item in cache
	UserGroup* result = NULL;
	UserGroup* cacheResult = NULL;
	u_int16_t logType = 0;
	ntsid_t tempsid;
	
	if (sid != NULL)
	{
		// make sure unused portion of sid structure is all zeros so compares succeed.
		if (sid->sid_authcount > KAUTH_NTSID_MAX_AUTHORITIES)
			return NULL;
		
		memset(&tempsid, 0, sizeof(ntsid_t));
		memcpy(&tempsid, sid, KAUTH_NTSID_SIZE(sid));
		sid = &tempsid;
	}

	if (guidData != NULL)
	{
		cacheResult = HashLookup(gGUIDHash, guidData);
		logType = kSearchByUUID;
	}
	else if (sid != NULL)
	{
		cacheResult = HashLookup(gSIDHash, sid);
		logType = kSearchBySID;
	}
	else if (name != NULL)
	{
		if (recordType == TYPE_USER)
			cacheResult = HashLookup(gUserNameHash, (void*)name);
		else
			cacheResult = HashLookup(gGroupNameHash, (void*)name);
		logType = kSearchByName;
	}
	else
	{
		if (recordType == TYPE_USER)
			cacheResult = HashLookup(gUIDHash, (void*)&id);
		else
			cacheResult = HashLookup(gGIDHash, (void*)&id);
		logType = kSearchByID;
	}
	
	// if we didn't find it or cache entry is too old, search for record
	if (cacheResult == NULL || ItemOutdated(cacheResult))
	{
		uint64_t microsec = GetElapsedMicroSeconds();
		
		gStatBlock->fCacheMisses++;
		if (guidData != NULL)
		{
			char guidString[37];
			ConvertGUIDToString(guidData, guidString);
			result = DoRecordSearch(recordType, idType, guidString, NULL);
		}
		else if (sid != NULL)
		{
			char sidString[256];
			ConvertSIDToString(sidString, sid);
			result = DoRecordSearch(recordType, idType, sidString, NULL);
		}
		else if (name != NULL)
		{
			result = DoRecordSearch(recordType, idType, name, NULL);
		}
		else
		{
			char idStr[16];
			sprintf(idStr,"%d", id);
			result = DoRecordSearch(recordType, idType, idStr, NULL);
		}

		// generally the record search will find the old cache result and replace it.  However, in some
		// circumstances (such as the old result being a negative one) it will create a new cache entry.
		// In those cases we need to delete the old cache entry.  However, if the ref count > 1 we can't remove it.
		// I'm not quite sure if that case could ever be hit, but it should be fine leaving this around if it is.
		if ((cacheResult != NULL) && (result != cacheResult) && (cacheResult->fRefCount == 0))
		{
			ResetUserGroup(cacheResult);
			cacheResult = NULL;
		}
		
		microsec = GetElapsedMicroSeconds() - microsec;
		AddToAverage(&gStatBlock->fAverageuSecPerRecordLookup, &gStatBlock->fTotalRecordLookups, (uint32_t)microsec);
	}
	else
	{
		gStatBlock->fCacheHits++;
		result = cacheResult;
		logType |= kItemInCache;
	}
	
	if (result == NULL)
	{
		u_int16_t checkval;
		
		// didn't manage to find item, so add negative cache hit
		if (cacheResult == NULL)
			result = GetNewUGStruct();
		else
		{
			// we found an outdated postive result, but the item no longer exists.
			// reset the item and add it as a negative result.
			ResetUserGroup(cacheResult);
			result = cacheResult;
		}

		gStatBlock->fNumFailedRecordLookups++;
		// add new entry to cache
		checkval = result->fCheckVal;
		memset(result, 0, sizeof(UserGroup));
		result->fCheckVal = checkval + 1;
		result->fIsUser = (recordType == TYPE_USER);
		AddToHeadOfList(result);
		if (guidData != NULL)
		{
			memcpy(&result->fGUID, guidData, sizeof(guid_t));
			result->fID = GetTempIDForGUID(guidData);
			AddToHash(gGUIDHash, result);
			AddToHash(gGIDHash, result);
		}
		else if (sid != NULL)
		{
			result->fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
			memcpy(result->fSID, sid, sizeof(ntsid_t));
			result->fID = GetTempIDForSID(sid);
			long* temp = (long*)&result->fGUID;
			temp[0] = htonl(0xAAAABBBB);
			temp[1] = htonl(0xCCCCDDDD);
			temp[2] = htonl(0xEEEEFFFF);
			temp[3] = htonl(result->fID);
			AddToHash(gGUIDHash, result);
			AddToHash(gSIDHash, result);
			AddToHash(gGIDHash, result);
		}
		else if (name != NULL)
		{
			result->fName = (char*)malloc(strlen(name) + 1);
			strcpy(result->fName, name);
			if (recordType == TYPE_USER)
				AddToHash(gUserNameHash, result);
			else
				AddToHash(gGroupNameHash, result);
		}
		else
		{
			result->fID = id;
			if (recordType == TYPE_USER)
				AddToHash(gUIDHash, result);
			else
			{
				guid_t* tempguid;
				ntsid_t* tempsid;
				FindTempID(id, &tempguid, &tempsid);
				if (tempsid != NULL && tempsid->sid_authcount <= KAUTH_NTSID_MAX_AUTHORITIES)
				{
					result->fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
					memset(result->fSID, 0, sizeof(ntsid_t));
					memcpy(result->fSID, tempsid, KAUTH_NTSID_SIZE(tempsid));
					AddToHash(gSIDHash, result);
				}
				if (tempguid != NULL)
				{
					memcpy(&result->fGUID, tempguid, sizeof(guid_t));
				}
				else
				{
					long* temp = (long*)&result->fGUID;
					temp[0] = htonl(0xAAAABBBB);
					temp[1] = htonl(0xCCCCDDDD);
					temp[2] = htonl(0xEEEEFFFF);
					temp[3] = htonl(result->fID);
				} 
				AddToHash(gGUIDHash, result);
				
				AddToHash(gGIDHash, result);
			}
		}

		result->fNotFound = 1;
		result->fExpiration = GetElapsedSeconds() + gDefaultNegativeExpiration;
		result->fLoginExpiration = GetElapsedSeconds() + gLoginExpiration;
	}
	
	if (!result->fNotFound)
		logType |= kFoundItem;
	AddLogEntry(logType, result, NULL);
	
	if ((recordType == TYPE_USER) && result->fNotFound)
	{
		result = NULL;
	}
	
	return result;
}

int IsCompatibilityGUID(guid_t* guid, int* isUser, uid_t* id)
{
	long* temp = (long*)guid;
	int result = 0;
	if ((temp[0] == htonl(0xFFFFEEEE)) && (temp[1] == htonl(0xDDDDCCCC)) && (temp[2] == htonl(0xBBBBAAAA)))
	{
		*id = ntohl(temp[3]);
		*isUser = 1;
		result = 1;
	}
	else if ((temp[0] == htonl(0xAAAABBBB)) && (temp[1] == htonl(0xCCCCDDDD)) && (temp[2] == htonl(0xEEEEFFFF)))
	{
		*id = ntohl(temp[3]);
		*isUser = 0;
		result = 1;
	}
	
	return result;
}

UserGroup* GetItemWithGUID(guid_t* guid)
{
	int isUser;
	uid_t id;
	
	if (IsCompatibilityGUID(guid, &isUser, &id))
	{
		if (isUser)
			return GetUserWithUID(id);
		else
			return GetGroupWithGID(id);
	}
	
	return GetItem(TYPE_BOTH, kDS1AttrGeneratedUID, guid, NULL, 0, NULL);
}

UserGroup* GetItemWithSID(ntsid_t* sid)
{
	return GetItem(TYPE_BOTH, kDS1AttrSMBSID, NULL, sid, 0, NULL);
}

UserGroup* GetUserWithUID(int uid)
{
	return GetItem(TYPE_USER, kDS1AttrUniqueID, NULL, NULL, uid, NULL);
}

UserGroup* GetGroupWithGID(int gid)
{
	return GetItem(TYPE_GROUP, kDS1AttrPrimaryGroupID, NULL, NULL, gid, NULL);
}

UserGroup* GetUserWithName(char* name)
{
	return GetItem(TYPE_USER, kDSNAttrRecordName, NULL, NULL, 0, name);
}

UserGroup* GetGroupWithName(char* name)
{
	UserGroup* result = GetItem(TYPE_GROUP, kDSNAttrRecordName, NULL, NULL, 0, name);
	if (result->fNotFound)
		result = NULL;
		
	return result;
}

int IsUserMemberOfGroupByGUID(UserGroup* user, guid_t* groupGUID)
{
	UserGroup* result;
	u_int16_t logType = kMembershipCheck | kSearchByUUID;
	if (memcmp(&user->fGUID, groupGUID, sizeof(guid_t)) == 0)
		return 1;

	if (memcmp(&gEveryoneGuid, groupGUID, sizeof(guid_t)) == 0)
		return 1;

	GenerateItemMembership(user);
		
	result = HashLookup(user->fGUIDMembershipHash, groupGUID);
	if (result != NULL) logType |= kFoundItem;
	AddLogEntry(logType, user, groupGUID);
	return (result != NULL);
}

int IsUserMemberOfGroupBySID(UserGroup* user, ntsid_t* groupSID)
{
	ntsid_t tempSID;
	UserGroup* result;
	u_int16_t logType = kMembershipCheck | kSearchBySID;
	
	if (groupSID->sid_authcount > KAUTH_NTSID_MAX_AUTHORITIES)
		return 0;
		
	memset(&tempSID, 0, sizeof(ntsid_t));
	memcpy(&tempSID, groupSID, KAUTH_NTSID_SIZE(groupSID));
	
	if ((user->fSID != NULL) && memcmp(user->fSID, &tempSID, sizeof(ntsid_t)) == 0)
		return 1;

	if (memcmp(&gEveryoneSID, &tempSID, sizeof(ntsid_t)) == 0)
		return 1;

	GenerateItemMembership(user);
		
	result = HashLookup(user->fSIDMembershipHash, &tempSID);
	if (result != NULL) logType |= kFoundItem;
	AddLogEntry(logType, user, (guid_t*)groupSID);
	return (result != NULL);
}

int IsUserMemberOfGroupByGID(UserGroup* user, int gid)
{
	UserGroup* result;
	u_int16_t logType = kMembershipCheck | kSearchByID;
	guid_t temp;
	
	GenerateItemMembership(user);
		
	result = HashLookup(user->fGIDMembershipHash, (void*)&gid);
	if (result != NULL) logType |= kFoundItem;
	memset(&temp, 0, sizeof(guid_t));
	*(uid_t*)&temp = gid;
	AddLogEntry(logType, user, &temp);
	return (result != NULL);
}

int Get16Groups(UserGroup* user, gid_t* gidArray)
{
	UserGroup* groups[16];
	int numGroups;
	int numResults = 1;
	int i;
	
	gidArray[0] = user->fPrimaryGroup;
	
	GenerateItemMembership(user);
	
	numGroups = GetHashEntries(user->fGUIDMembershipHash, groups, 16);
	for (i=0; i<numGroups; i++)
	{
		if (groups[i]->fID != user->fPrimaryGroup)
		{
			gidArray[numResults++] = groups[i]->fID;
		}
		
		if (numResults == 15) break;
	}
		
	return numResults;
}

void ResetUserGroup(UserGroup* ug)
{
	int checkval;

	ReleaseHash(ug->fGUIDMembershipHash);
	ReleaseHash(ug->fSIDMembershipHash);
	ReleaseHash(ug->fGIDMembershipHash);
	RemoveFromHash(gGUIDHash, ug);
	if (ug->fName != NULL)
	{
		free(ug->fName);
		if (ug->fIsUser)
			RemoveFromHash(gUserNameHash, ug);
		else
			RemoveFromHash(gGroupNameHash, ug);
	}
	if (ug->fSID != NULL)
	{
		RemoveFromHash(gSIDHash, ug);
		free(ug->fSID);
	}
	if (ug->fIsUser)
		RemoveFromHash(gUIDHash, ug);
	else
		RemoveFromHash(gGIDHash, ug);
	
	RemoveFromList(ug);
	checkval = ug->fCheckVal;
	memset(ug, 0, sizeof(UserGroup));
	ug->fCheckVal = checkval;
}

UserGroup* GetNewUGStruct()
{
	UserGroup* result = NULL;

	while (result == NULL)
	{
		if (gNumItemsInCache < gMaxGUIDSInCache)
		{
			result = (UserGroup*)malloc(sizeof(UserGroup));
			memset(result, 0, sizeof(UserGroup));
			gNumItemsInCache++;
		}

		if (result == NULL)
		{
			UserGroup* current = gListTail;
			while (current != NULL)
			{
				if (current->fRefCount == 0)
					break;  // use the first item not in a membership hash
				current = current->fBackLink;
			}
			
			if (current != NULL)
			{
				ResetUserGroup(current);
				result = current;
			}
			else gMaxGUIDSInCache += 10;
		}
	}
	
	return result;
}

void UpdateIDOrNameHash(HashTable* hash, UserGroup* result, void* resultKey)
{
	UserGroup* idLookup = HashLookup(hash, resultKey);
		
	if ((idLookup != NULL) && (idLookup != result))
	{
		RemoveFromHash(hash, idLookup);
	}
	
	if ((idLookup == NULL) || (idLookup != result))
	{
		AddToHash(hash, result);
	}
}


UserGroup* FindOrAddUG(UserGroup* template, int hasGUID, int hasID, int foundByID, int foundByName)
{
	UserGroup* result;
	
	if (!hasID)
	{
		if (template->fName != NULL)
			free(template->fName);
		if (template->fSID != NULL)
			free(template->fSID);
		return NULL;
	}
	else
	{
		if (!hasGUID)
		{
			long* temp = (long*)&template->fGUID;
			if (template->fIsUser)
			{
				temp[0] = htonl(0xFFFFEEEE);
				temp[1] = htonl(0xDDDDCCCC);
				temp[2] = htonl(0xBBBBAAAA);
				temp[3] = htonl(template->fID);
			}
			else
			{
				temp[0] = htonl(0xAAAABBBB);
				temp[1] = htonl(0xCCCCDDDD);
				temp[2] = htonl(0xEEEEFFFF);
				temp[3] = htonl(template->fID);
			}
		}
	}

	result = HashLookup(gGUIDHash, &template->fGUID);
	
	if (result == NULL || ItemOutdated(result))
	{
		int checkval;
	
		if (result == NULL)
			result = GetNewUGStruct();
		else
		{
			// wipe out old info
			ResetUserGroup(result);
		}

		checkval = result->fCheckVal;
		memcpy(result, template, sizeof(UserGroup));
		result->fCheckVal = checkval + 1;
		
		AddToHeadOfList(result);
		AddToHash(gGUIDHash, result);
		if (result->fSID != NULL)
			AddToHash(gSIDHash, result);
	}
	else
	{
		if (template->fName != NULL)
			free(template->fName);
		if (template->fSID != NULL)
			free(template->fSID);
	}
	
	if (foundByID)
	{
		if (result->fIsUser)
			UpdateIDOrNameHash(gUIDHash, result, (void*)&result->fID);
		else
			UpdateIDOrNameHash(gGIDHash, result, (void*)&result->fID);
	}
	
	if (foundByName && (result->fName != 0))
	{
		if (result->fIsUser)
			UpdateIDOrNameHash(gUserNameHash, result, result->fName);
		else
			UpdateIDOrNameHash(gGroupNameHash, result, result->fName);
	}
	
	return result;
}

void ConvertBytesToHex(char** string, char** data, int numBytes)
{
	int i;
	
	for (i=0; i < numBytes; i++)
	{
		unsigned char hi = ((**data) >> 4) & 0xf;
		unsigned char low = (**data) & 0xf;
		if (hi < 10)
			**string = '0' + hi;
		else
			**string = 'A' + hi - 10;
			
		(*string)++;

		if (low < 10)
			**string = '0' + low;
		else
			**string = 'A' + low - 10;

		(*string)++;
		(*data)++;
	}
}

void ConvertSIDToString(char* string, ntsid_t* sid)
{
	char* current = string;
	long long temp = 0;
	int i;
	
	for (i = 0; i < 6; i++)
		temp = (temp << 8) | sid->sid_authority[i];
	
	sprintf(current,"S-%u-%llu", sid->sid_kind, temp);
	
	for(i=0; i < sid->sid_authcount; i++)
	{
		current = current + strlen(current);
		sprintf(current, "-%lu", sid->sid_authorities[i]);
	}
}

void ConvertSIDFromString(char* string, ntsid_t* sid)
{
	char* current = string+2;
	int count = 0;
	long long temp;

	memset(sid, 0, sizeof(ntsid_t));
	if (string[0] != 'S' || string[1] != '-') return;
	
	sid->sid_kind = strtoul(current, &current, 10);
	if (*current == '\0') return;
	current++;
	temp = strtoll(current, &current, 10);
	// convert to BigEndian before copying
	temp = OSSwapHostToBigInt64(temp);
	memcpy(sid->sid_authority, ((char*)&temp)+2, 6);
	while (*current != '\0')
	{
		current++;
		sid->sid_authorities[count] = strtoul(current, &current, 10);
		count++;
	}
	
	sid->sid_authcount = count;
}

void ConvertGUIDToString(guid_t* data, char* string)
{
	char* guid = (char*)data;
	char* strPtr = string;
	ConvertBytesToHex(&strPtr, &guid, 4);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 2);
	*strPtr = '-'; strPtr++;
	ConvertBytesToHex(&strPtr, &guid, 6);
	*strPtr = '\0';
}

void ConvertGUIDFromString(char* string, guid_t* destination)
{
	short dataIndex = 0;
	int isFirstNibble = 1;
	
	while (*string != '\0' && dataIndex < 16)
	{
		char nibble;
		
		if (*string >= '0' && *string <= '9')
			nibble = *string - '0';
		else if (*string >= 'A' && *string <= 'F')
			nibble = *string - 'A' + 10;
		else if (*string >= 'a' && *string <= 'f')
			nibble = *string - 'a' + 10;
		else
		{
			string++;
			continue;
		}
		
		if (isFirstNibble)
		{
			destination->g_guid[dataIndex] = nibble << 4;
			isFirstNibble = 0;
		}
		else
		{
			destination->g_guid[dataIndex] |= nibble;
			dataIndex++;
			isFirstNibble = 1;
		}
		
		string++;
	}
}

void GenerateItemMembership(UserGroup* item)
{
	char guidString[37];
	UserGroup* primary = NULL;
	
	if (item->fGUIDMembershipHash != NULL) return;
	
	item->fRefCount++;  // mark item in use
	
	item->fGUIDMembershipHash = CreateHash(__offsetof(UserGroup, fGUID), sizeof(guid_t), 0, 1);
	item->fSIDMembershipHash = CreateHash(__offsetof(UserGroup, fSID), sizeof(ntsid_t), 1, 1);
	item->fGIDMembershipHash = CreateHash(__offsetof(UserGroup, fID), sizeof(uid_t), 0, 1);
	
	// Add primary group
	if (item->fIsUser)
		primary = GetGroupWithGID(item->fPrimaryGroup);
	if (primary != NULL)
	{
		AddToHash(item->fGUIDMembershipHash, primary);
		if (primary->fSID != NULL)
			AddToHash(item->fSIDMembershipHash, primary);
		AddToHash(item->fGIDMembershipHash, primary);
	}
	
	ConvertGUIDToString(&item->fGUID, guidString);

	if (item->fIsUser)
	{
		uint64_t microsec = GetElapsedMicroSeconds();
		DoRecordSearch(TYPE_GROUP, kDSNAttrMember, item->fName, item);
		DoRecordSearch(TYPE_GROUP, kDSNAttrGroupMembers, guidString, item);
		microsec = GetElapsedMicroSeconds() - microsec;
		AddToAverage(&gStatBlock->fAverageuSecPerMembershipSearch, &gStatBlock->fTotalMembershipSearches, (uint32_t)microsec);
	}
	else
	{
		DoRecordSearch(TYPE_GROUP, kDSNAttrNestedGroups, guidString, item);
	}

	item->fRefCount--;
}

UserGroup* DoRecordSearch(int recordType, char* attribute, char* value, UserGroup* membershipRoot)
{
	unsigned long recCount = 1;
	tContextData localContext = NULL;
//	OSStatus status = eDSNoErr;
	tDataBufferPtr searchBuffer;
	tDataListPtr recType;
	UserGroup* result = NULL;
	tDirStatus status;
	unsigned long buffSize = 4096;
	int recordIndex, attrIndex;
	uint64_t microsec = GetElapsedMicroSeconds();
	uint64_t totalTime = 0;
	int foundByID, foundByName;
	
	pthread_mutex_unlock(&gProcessMutex);
	
	if (dsVerifyDirRefNum(gDirRef) != eDSNoErr)
	{
		OpenDirService();
		ResetCache();
	}
	
	searchBuffer = dsDataBufferAllocate( gDirRef, buffSize );
	if (recordType == TYPE_USER)
		recType = dsBuildListFromStrings(gDirRef, kDSStdRecordTypeUsers, NULL);
	else if (recordType == TYPE_GROUP)
		recType = dsBuildListFromStrings(gDirRef, kDSStdRecordTypeGroups, NULL);
	else
		recType = dsBuildListFromStrings(gDirRef, kDSStdRecordTypeUsers, kDSStdRecordTypeGroups, NULL);
	tDataListPtr attrsToGet = dsBuildListFromStrings(gDirRef, kDS1AttrUniqueID, kDS1AttrGeneratedUID, 
													kDSNAttrRecordName, kDS1AttrPrimaryGroupID, 
													kDS1AttrTimeToLive, kDS1AttrSMBSID, NULL);
	tDataNodePtr attrType = dsDataNodeAllocateString(gDirRef, attribute);
	tDataNodePtr lookUpPtr = dsDataNodeAllocateString(gDirRef, value);
	
	do {
		do {
			recCount = 0;
			status = dsDoAttributeValueSearchWithData(gSearchNode, searchBuffer, recType,
											  attrType, eDSExact, lookUpPtr, attrsToGet, 0,
											  &recCount, &localContext);
			if (status == eDSBufferTooSmall) {
				buffSize *= 2;
				
				// a safety for a runaway condition
				if ( buffSize > 1024 * 1024 )
					break;
				
				dsDataBufferDeAllocate( gDirRef, searchBuffer );
				searchBuffer = dsDataBufferAllocate( gDirRef, buffSize );
				if ( searchBuffer == NULL )
					status = eMemoryError;
			}
		} while (((status == eDSNoErr) && (recCount == 0) && (localContext != NULL)) || 
					(status == eDSBufferTooSmall));
		
		if ((status == eDSNoErr) && (recCount <= 0))
			break;
		if ( status == eDSInvalidContinueData )
		{
			localContext = NULL;
			continue;
		}
		else if (status != eDSNoErr)
			break;

		for (recordIndex = 1; recordIndex <= recCount; ++recordIndex)
		{
			tAttributeValueListRef 	attributeValueListRef = 0;
			tAttributeListRef 		attributeListRef = 0;
			tRecordEntryPtr 		recordEntryPtr = NULL;
			tAttributeEntryPtr 		attributeInfo = NULL;
			tAttributeValueEntryPtr attrValue = NULL;
			int hasGUID = 0;
			int hasID = 0;
			
			status = dsGetRecordEntry(gSearchNode, searchBuffer, recordIndex, &attributeListRef, &recordEntryPtr);
			if (status == eDSNoErr)
			{
				char* recTypeStr = NULL;
				UserGroup template;
				memset(&template, 0, sizeof(UserGroup));
				template.fExpiration = GetElapsedSeconds() + gDefaultExpiration;
				template.fLoginExpiration = GetElapsedSeconds() + gLoginExpiration;
				
				status = dsGetRecordTypeFromEntry( recordEntryPtr, &recTypeStr );
				if (status == eDSNoErr && (strcmp(recTypeStr, kDSStdRecordTypeUsers) == 0))
					template.fIsUser = 1;
				else if (status == eDSNoErr && (strcmp(recTypeStr, kDSStdRecordTypeGroups) == 0))
					template.fIsUser = 0;
				else
				{
					if (status == eDSNoErr) free(recTypeStr);
					continue;
				}
					
				free(recTypeStr);
				
				for (attrIndex = 1; attrIndex <= recordEntryPtr->fRecordAttributeCount; ++attrIndex)
				{
					status = dsGetAttributeEntry(gSearchNode, searchBuffer, attributeListRef, 
												 attrIndex, &attributeValueListRef, &attributeInfo);									 		
					if (status == eDSNoErr)
					{
						status = dsGetAttributeValue(gSearchNode, searchBuffer, 1, attributeValueListRef, &attrValue);	
						if (status == eDSNoErr)
						{
							char* attrName = (char*)attributeInfo->fAttributeSignature.fBufferData;
							if (strcmp(attrName, kDSNAttrRecordName) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								template.fName = malloc(strlen(temp) + 1);
								strcpy(template.fName, temp);
							}
							else if (strcmp(attrName, kDS1AttrGeneratedUID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								ConvertGUIDFromString(temp, &template.fGUID);
								hasGUID = 1;
							}
							else if (strcmp(attrName, kDS1AttrSMBSID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								template.fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
								ConvertSIDFromString(temp, template.fSID);
							}
							else if (strcmp(attrName, kDS1AttrUniqueID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								template.fID = strtol(temp, NULL, 10);
								hasID = 1;
							}
							else if (strcmp(attrName, kDS1AttrTimeToLive) == 0)
							{
								int multiplier = 1;
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								char* endPtr; 
								int num = strtol(temp, &endPtr, 10);
								switch (*endPtr)
								{
									case 's':
										multiplier = 1;
										break;
									case 'm':
										multiplier = 60;
										break;
									case 'h':
										multiplier = 60 * 60;
										break;
									case 'd':
										multiplier = 60 * 60 * 24;
										break;
								}
								
								template.fExpiration = GetElapsedSeconds() + num * multiplier;
							}
							else if (strcmp(attrName, kDS1AttrPrimaryGroupID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								int id = strtol(temp, NULL, 10);
								if (template.fIsUser)
								{
									template.fPrimaryGroup = id;
								}
								else
								{
									template.fID = id;
									hasID = 1;
								}
							}

							dsDeallocAttributeValueEntry(gDirRef, attrValue);	
						}

						dsDeallocAttributeEntry(gDirRef, attributeInfo);	
						dsCloseAttributeValueList(attributeValueListRef);
					}
				}

				dsDeallocRecordEntry(gDirRef, recordEntryPtr);
				dsCloseAttributeList(attributeListRef);
				
				pthread_mutex_lock(&gProcessMutex);
				
				foundByID = ((strcmp(attribute, kDS1AttrUniqueID) == 0) || (strcmp(attribute, kDS1AttrPrimaryGroupID) == 0));
				foundByName = (strcmp(attribute, kDSNAttrRecordName) == 0);
				
				result = FindOrAddUG(&template, hasGUID, hasID, foundByID, foundByName);
				if (membershipRoot)
				{
					AddToHash(membershipRoot->fGUIDMembershipHash, result);
					if (result->fSID != NULL)
						AddToHash(membershipRoot->fSIDMembershipHash, result);
					AddToHash(membershipRoot->fGIDMembershipHash, result);
//					printf("Found group membership for id %d\n", result->fID);
					if (result->fGUIDMembershipHash == NULL)
					{
						totalTime += GetElapsedMicroSeconds() - microsec;
						GenerateItemMembership(result);
						microsec = GetElapsedMicroSeconds();
					}
					MergeHashEntries(membershipRoot->fGUIDMembershipHash, result->fGUIDMembershipHash);
					MergeHashEntries(membershipRoot->fSIDMembershipHash, result->fSIDMembershipHash);
					MergeHashEntries(membershipRoot->fGIDMembershipHash, result->fGIDMembershipHash);
					if (result->fExpiration < membershipRoot->fExpiration)
						membershipRoot->fExpiration = result->fExpiration;
				}
				else
				{
					if (localContext != NULL)
						dsReleaseContinueData(gSearchNode, localContext);
					localContext = NULL;
					pthread_mutex_unlock(&gProcessMutex);
					break;  // we only need to find the first entry
				}
				pthread_mutex_unlock(&gProcessMutex);

			}
		}
	} while ( localContext != NULL );
	
	dsDataBufferDeAllocate(gDirRef, searchBuffer);
	dsDataListDeallocate(gDirRef, recType);
	free(recType);
	dsDataListDeallocate(gDirRef, attrsToGet);
	free(attrsToGet);
	dsDataNodeDeAllocate(gDirRef, attrType);
	dsDataNodeDeAllocate(gDirRef, lookUpPtr);
	
	pthread_mutex_lock(&gProcessMutex);

	totalTime += GetElapsedMicroSeconds() - microsec;
	if (strcmp(attribute, kDSNAttrGroupMembers) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerGUIDMemberSearch, &gStatBlock->fTotalGUIDMemberSearches, (uint32_t)totalTime);
	else if (strcmp(attribute, kDSNAttrNestedGroups) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerNestedMemberSearch, &gStatBlock->fTotalNestedMemberSearches, (uint32_t)totalTime);
	else if (strcmp(attribute, kDSNAttrMember) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerLegacySearch, &gStatBlock->fTotalLegacySearches, (uint32_t)totalTime);

	return result;
}

void RemoveFromList(UserGroup* ug)
{
	if (ug->fLink == NULL)
	{
		gListTail = ug->fBackLink;
	}
	else
	{
		ug->fLink->fBackLink = ug->fBackLink;
	}
	if (ug->fBackLink == NULL)
	{
		gListHead = ug->fLink;
	}
	else
	{
		ug->fBackLink->fLink = ug->fLink;
	}
}

void AddToHeadOfList(UserGroup* ug)
{
	if (gListHead == NULL)
	{
		gListHead = gListTail = ug;
		ug->fLink = NULL;
	}
	else
	{
		ug->fLink = gListHead;
		gListHead->fBackLink = ug;
		gListHead = ug;
	}
	ug->fBackLink = NULL;
}

void TouchItem(UserGroup* item)
{
	RemoveFromList(item);
	AddToHeadOfList(item);
}

void* UIDCacheIndexToPointer(TempUIDCacheBlockBase* block, int index)
{
	void* result;

	if (block->fKind == kUUIDBlock || block->fKind == kSmallSIDBlock)
	{
		TempUIDCacheBlockSmall* temp = (TempUIDCacheBlockSmall*)block;
		result = &temp->fGUIDs[index];
	}
	else
	{
		TempUIDCacheBlockLarge* temp = (TempUIDCacheBlockLarge*)block;
		result = &temp->fSIDs[index];
	}
	
	return result;
}

uid_t GetTempID(void* id, int blockKind, int idSize)
{
	TempUIDCacheBlockBase* block = gUIDCache;
	TempUIDCacheBlockBase* lasttypeblock = NULL;
	TempUIDCacheBlockBase* lastblock = NULL;
	while (block != NULL)
	{
		int i;
		
		if (block->fKind == blockKind)
		{
			lasttypeblock = block;
			for (i = 0; i < block->fNumIDs; i++)
			{
				if (memcmp(id, UIDCacheIndexToPointer(lasttypeblock, i), idSize) == 0)
					return block->fStartID + i;
			}
		}
		
		lastblock = block;
		block = block->fNext;
	}
	
	if ((lasttypeblock == NULL) || (lasttypeblock->fNumIDs == 1024))
	{
		if (idSize == sizeof(guid_t))
		{
			lasttypeblock = (TempUIDCacheBlockBase*)malloc(sizeof(TempUIDCacheBlockSmall));
			memset(lasttypeblock, 0, sizeof(TempUIDCacheBlockSmall));
		}
		else
		{
			lasttypeblock = (TempUIDCacheBlockBase*)malloc(sizeof(TempUIDCacheBlockLarge));
			memset(lasttypeblock, 0, sizeof(TempUIDCacheBlockLarge));
		}
		lasttypeblock->fKind = blockKind;
		if (lastblock != NULL)
		{
			lastblock->fNext = lasttypeblock;
			lasttypeblock->fStartID = lastblock->fStartID + 1024;
		}
		else
		{
			gUIDCache = lasttypeblock;
			lasttypeblock->fStartID = 0x82000000;
		}
	}
	
	memcpy(UIDCacheIndexToPointer(lasttypeblock, lasttypeblock->fNumIDs), id, idSize);
	lasttypeblock->fNumIDs++;
	
	return lasttypeblock->fStartID + lasttypeblock->fNumIDs - 1;
}

uid_t GetTempIDForGUID(guid_t* guid)
{
	return GetTempID(guid, kUUIDBlock, sizeof(guid_t));
}

uid_t GetTempIDForSID(ntsid_t* sid)
{
	if (sid->sid_authcount <= 2)
		return GetTempID(sid, kSmallSIDBlock, sizeof(guid_t));
	
	return GetTempID(sid, kLargeSIDBlock, sizeof(ntsid_t));
}

bool FindTempID(uid_t id, guid_t** guid, ntsid_t** sid)
{
	TempUIDCacheBlockBase* block = gUIDCache;
	*guid = NULL;
	*sid = NULL;
	
	if (id < 0x82000000) return false;
	
	while ((block != NULL) && (block->fStartID + 1024 <= id))
		block = block->fNext;
		
	if (block == NULL) return false;
	
	if (block->fStartID + block->fNumIDs < id) return false;
	
	if (block->fKind == kUUIDBlock)
		*guid = (guid_t*)UIDCacheIndexToPointer(block, id - block->fStartID);
	else
		*sid = (ntsid_t*)UIDCacheIndexToPointer(block, id - block->fStartID);
		
	return true;
}




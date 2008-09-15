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

#include "Mbrd_UserGroup.h"
#include "Mbrd_HashTable.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <fcntl.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>

extern DSMutexSemaphore gMbrGlobalMutex;

tDirReference gMbrdDirRef = 0;
tDirNodeReference gMbrdSearchNode = 0;

#define TYPE_USER 0
#define TYPE_GROUP 1
#define TYPE_BOTH 2
#define TYPE_COMPUTER 3

int gMaxGUIDSInCache = 500;
int gDefaultExpiration = 4*60*60;
int gDefaultNegativeExpiration = 2*60*60;
int gLoginExpiration = 2*60;

uint32_t gScaleNumerator = 0;
uint32_t gScaleDenominator = 0;

UserGroup* gListHead = NULL;
UserGroup* gListTail = NULL;
long gNumItemsInCache = 0;

HashTable* gGUIDHash = NULL;
HashTable* gSIDHash = NULL;
HashTable* gUIDHash = NULL;
HashTable* gGIDHash = NULL;

HashTable* gUserNameHash = NULL;
HashTable* gGroupNameHash = NULL;
HashTable* gComputerNameHash = NULL;

static pthread_key_t gThreadKey;

StatBlock *gStatBlock = NULL;

guid_t gEveryoneGuid;
ntsid_t gEveryoneSID;

guid_t gLocalAccountsGuid;
guid_t gNetAccountsGuid;

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
UserGroup* GetNewUGStruct(void);

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

// keep these around, we use them a lot
static tDataListPtr gUserType		= NULL;
static tDataListPtr gComputerType	= NULL;
static tDataListPtr gGroupType		= NULL;
static tDataListPtr gUnknownType	= NULL;
static tDataListPtr gAttrsToGet		= NULL;

void SetThreadFlags(int flags)
{
	pthread_setspecific(gThreadKey, (const void*) flags);
}

uint32_t GetElapsedSeconds(void)
{
	uint64_t elapsed = mach_absolute_time();
	if (gScaleNumerator == 0)
	{
		struct mach_timebase_info mti = {0};
		mach_timebase_info(&mti);
		gScaleNumerator = mti.numer;
		gScaleDenominator = mti.denom;
	}
	long double temp = (long double)(((long double)elapsed *
								(long double)gScaleNumerator)/(long double)gScaleDenominator);
	uint32_t elapsedSeconds = (uint32_t) (temp/(long double)NSEC_PER_SEC);
	return elapsedSeconds;
}

uint64_t GetElapsedMicroSeconds(void)
{
	uint64_t elapsed = mach_absolute_time();
	if (gScaleNumerator == 0)
	{
		struct mach_timebase_info mti = {0};
		mach_timebase_info(&mti);
		gScaleNumerator = mti.numer;
		gScaleDenominator = mti.denom;
	}
	long double temp = (long double)(((long double)elapsed *
								(long double)gScaleNumerator)/(long double)gScaleDenominator);
	uint64_t elapsedMicroSeconds = (uint64_t) (temp/(long double)NSEC_PER_USEC);
	return elapsedMicroSeconds;
}

void AddToAverage(uint32_t* average, uint32_t* numDataPoints, uint32_t newDataPoint)
{
	*average = (((*average) * (*numDataPoints)) + newDataPoint) / (*numDataPoints + 1);
	*numDataPoints = *numDataPoints + 1;
}

void OpenDirService(void)
{
	tDataBufferPtr  nodeBuffer		= NULL;
	tContextData	localContext	= 0;
	tDataListPtr	nodeName		= NULL;
	UInt32			returnCount		= 0;
	tDirStatus		status			= eDSNoErr;
	
	dsOpenDirService(&gMbrdDirRef);
	
	nodeBuffer = dsDataBufferAllocate(gMbrdDirRef, 4096);
	
	status = dsFindDirNodes(gMbrdDirRef, nodeBuffer, NULL, eDSAuthenticationSearchNodeName, &returnCount, &localContext);
	if (status != eDSNoErr || returnCount == 0)
		syslog(LOG_CRIT, "dsFindDirNodes returned %d, count = %d", status, returnCount);
	
	status = dsGetDirNodeName(gMbrdDirRef, nodeBuffer, 1, &nodeName); //Currently we only look at the 1st node.
	if (status != eDSNoErr)
		syslog(LOG_CRIT, "dsGetDirNodeName returned %d", status);
	
	status = dsOpenDirNode(gMbrdDirRef, nodeName, &gMbrdSearchNode);
	if (status != eDSNoErr)
		syslog(LOG_CRIT, "dsOpenDirNode returned %d", status);
	
	if (localContext != 0)
		dsReleaseContinueData(gMbrdDirRef, localContext);
	
	dsDataBufferDeAllocate(gMbrdDirRef, nodeBuffer);
	dsDataListDeallocate(gMbrdDirRef, nodeName);
	free(nodeName);
}

void Mbrd_InitializeUserGroup(int numToCache, int defaultExpiration, int defaultNegativeExpiration, int loginExp)
{
	gMaxGUIDSInCache = numToCache;
	gDefaultExpiration = defaultExpiration;
	gDefaultNegativeExpiration = defaultNegativeExpiration;
	gLoginExpiration = loginExp;

	// setup a key for our threads
	pthread_key_create(&gThreadKey, NULL);

	gListHead = NULL;
	gListTail = NULL;
	gNumItemsInCache = 0;
	
	gGUIDHash = CreateHash(__offsetof(UserGroup, fGUID), sizeof(guid_t), 0, 0);
	gSIDHash = CreateHash(__offsetof(UserGroup, fSID), sizeof(ntsid_t), 1, 0);
	gUIDHash = CreateHash(__offsetof(UserGroup, fID), sizeof(uid_t), 0, 0);
	gGIDHash = CreateHash(__offsetof(UserGroup, fID), sizeof(uid_t), 0, 0);
	gUserNameHash = CreateHash(__offsetof(UserGroup, fName), 0, 1, 0);
	gGroupNameHash = CreateHash(__offsetof(UserGroup, fName), 0, 1, 0);
	gComputerNameHash = CreateHash(__offsetof(UserGroup, fName), 0, 1, 0);
	
	ConvertGUIDFromString("ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000000C", &gEveryoneGuid);
	ConvertSIDFromString("S-1-1-0", &gEveryoneSID);
	
	ConvertGUIDFromString("ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000003D", &gLocalAccountsGuid);
	ConvertGUIDFromString("ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000003E", &gNetAccountsGuid);
	
	gStatBlock = (StatBlock*)malloc(sizeof(StatBlock));
	memset(gStatBlock, 0, sizeof(StatBlock));
	gStatBlock->fTotalUpTime = GetElapsedSeconds();
	
	gUserType = dsBuildListFromStrings(0, kDSStdRecordTypeUsers, NULL);
	gComputerType = dsBuildListFromStrings(0, kDSStdRecordTypeComputers, NULL);
	gGroupType = dsBuildListFromStrings(0, kDSStdRecordTypeGroups, kDSStdRecordTypeComputerGroups, NULL);
	gUnknownType = dsBuildListFromStrings(0, kDSStdRecordTypeUsers, kDSStdRecordTypeComputers, kDSStdRecordTypeGroups, kDSStdRecordTypeComputerGroups, NULL);
	gAttrsToGet = dsBuildListFromStrings(0, kDS1AttrUniqueID, kDS1AttrGeneratedUID, kDSNAttrRecordName, kDS1AttrPrimaryGroupID, kDS1AttrTimeToLive, kDS1AttrSMBSID, kDS1AttrENetAddress, kDSNAttrMetaNodeLocation, kDS1AttrCopyTimestamp, NULL);
}

void Mbrd_ResetCache( void )
{
	UserGroup* temp = gListHead;
	while (temp != NULL)
	{
		temp->fExpiration = 0;
		temp->fLoginExpiration = 0;
		temp = temp->fLink;
	}
}

void Mbrd_DumpState( void )
{
    struct stat sb;
	char		*logName = "/Library/Logs/membership_dump.log";
	int			ii;
	
	// roll logs starting from the end
	// only keep the last 9 logs around
	for ( ii = 8; ii >= 0; ii-- )
	{
		char fileName[128];
		char newName[128];

		snprintf( fileName, sizeof(fileName), "%s.%d", logName, ii );
		if ( lstat(fileName, &sb) == 0 )
		{
			snprintf( newName, sizeof(newName), "%s.%d", logName, ii+1 );
			
			// first unlink any existing file and then rename current to the new
			unlink( newName );
			rename( fileName, newName );
		}
	}
	
	// now rename any existing log to /Library/Logs/membership_dump.log.0
    if ( lstat(logName, &sb) == 0 )
    {
		char newName[128];

		snprintf( newName, sizeof(newName), "%s.0", logName );
		
		// unlink the new name and immediately rename
		unlink( newName );
		rename( logName, newName );
    }

	int fd = open( logName, O_EXCL | O_CREAT | O_RDWR, 0644 );
	if ( fd == -1 )
		return;
	
	FILE* dumpFile = fdopen( fd, "w" );
	if ( dumpFile == NULL ) {
		close( fd );
		return;
	}
	
	UserGroup* temp = gListHead;

	fprintf(dumpFile, "\nCache dump:\n\n");

	while (temp != NULL)
	{
		char guidString[37];
		char sidString[256];
		const char* name = "";
		char* type = NULL;
		if (temp->fIsUser)
			type = strdup("User");
		else if (temp->fIsComputer)
			type = strdup("Computer");
		else
			type = strdup("Group");
		
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
			int i;
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
		free(type);
	}
	
	fclose(dumpFile);
}

int ItemOutdated(UserGroup* item)
{
	int flags = (int) pthread_getspecific(gThreadKey);
	if ((flags & kUseLoginTimeOutMask) != 0)
		return (item->fLoginExpiration <= GetElapsedSeconds());
	return (item->fExpiration <= GetElapsedSeconds());
}

UserGroup* GetItem(int recordType, char* idType, void* guidData, ntsid_t* sid, int id, char* name)
{
	// First try and look up item in cache
	static DSMutexSemaphore searchMutex;
	UserGroup* result = NULL;
	UserGroup* cacheResult = NULL;
	ntsid_t tempsid;
	bool bSearchMutexHeld = false;
	
	if (sid != NULL)
	{
		// make sure unused portion of sid structure is all zeros so compares succeed.
		if (sid->sid_authcount > KAUTH_NTSID_MAX_AUTHORITIES)
			return NULL;
		
		memset(&tempsid, 0, sizeof(ntsid_t));
		memcpy(&tempsid, sid, KAUTH_NTSID_SIZE(sid));
		sid = &tempsid;
	}
	
lookagain:
	
	if (guidData != NULL)
	{
		cacheResult = HashLookup(gGUIDHash, guidData);
	}
	else if (sid != NULL)
	{
		cacheResult = HashLookup(gSIDHash, sid);
	}
	else if (name != NULL)
	{
		if (recordType == TYPE_USER)
			cacheResult = HashLookup(gUserNameHash, (void*)name);
		else if (recordType == TYPE_COMPUTER)
			cacheResult = HashLookup(gComputerNameHash, (void*)name);
		else
			cacheResult = HashLookup(gGroupNameHash, (void*)name);
	}
	else
	{
		if (recordType == TYPE_USER)
			cacheResult = HashLookup(gUIDHash, (void*)&id);
		else
			cacheResult = HashLookup(gGIDHash, (void*)&id);
	}
	
	// if we didn't find it or cache entry is too old, search for record
	if (cacheResult == NULL || ItemOutdated(cacheResult))
	{
		if ( bSearchMutexHeld == false )
		{
			gMbrGlobalMutex.SignalLock();
			searchMutex.WaitLock();
			gMbrGlobalMutex.WaitLock();
			bSearchMutexHeld = true;
			goto lookagain; // we check again while we hold the search mutex since a search could have been active
		}
		else
		{
			gMbrGlobalMutex.SignalLock(); // let's give up our Global mutex now
		}

		uint64_t microsec = GetElapsedMicroSeconds();
		
		gStatBlock->fCacheMisses++;
		if (guidData != NULL)
		{
			char guidString[37];
			ConvertGUIDToString((guid_t *)guidData, guidString);
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
		
		gMbrGlobalMutex.WaitLock(); // regrab our mutex
	}
	else
	{
		gStatBlock->fCacheHits++;
		result = cacheResult;
	}
	
	// ensure we've given up the search
	if ( bSearchMutexHeld )
	{
		searchMutex.SignalLock();
		bSearchMutexHeld = false;
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
		result->fIsComputer = (recordType == TYPE_COMPUTER);
		AddToHeadOfList(result);
		if (guidData != NULL)
		{
			memcpy(&result->fGUID, guidData, sizeof(guid_t));
			result->fID = GetTempIDForGUID((guid_t*)guidData);
			AddToHash(gGUIDHash, result);
			
			if ( recordType == TYPE_USER || recordType == TYPE_COMPUTER )
				AddToHash(gUIDHash, result);
			else
				AddToHash(gGIDHash, result);
		}
		else if (sid != NULL)
		{
			result->fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
			memcpy(result->fSID, sid, sizeof(ntsid_t));
			result->fID = GetTempIDForSID(sid);
			long* temp = (long*)&result->fGUID;
			
			if ( recordType == TYPE_USER || recordType == TYPE_COMPUTER )
			{
				temp[0] = htonl(0xFFFFEEEE);
				temp[1] = htonl(0xDDDDCCCC);
				temp[2] = htonl(0xBBBBAAAA);
				temp[3] = htonl(result->fID);
				
				AddToHash(gUIDHash, result);
			}
			else
			{
				temp[0] = htonl(0xAAAABBBB);
				temp[1] = htonl(0xCCCCDDDD);
				temp[2] = htonl(0xEEEEFFFF);
				temp[3] = htonl(result->fID);
				
				AddToHash(gGIDHash, result);
			}
			
			AddToHash(gGUIDHash, result);
			AddToHash(gSIDHash, result);
		}
		else if (name != NULL)
		{
			// note there is no such thing as temporary IDs for non-existent names
			result->fName = (char*)malloc(strlen(name) + 1);
			strcpy(result->fName, name);
			if (recordType == TYPE_USER)
				AddToHash(gUserNameHash, result);
			else if (recordType == TYPE_COMPUTER)
				AddToHash(gComputerNameHash, result);
			else
				AddToHash(gGroupNameHash, result);
		}
		else
		{
			ntsid_t* tempsidPtr = NULL;
			guid_t* tempguid = NULL;

			result->fID = id;
			FindTempID(id, &tempguid, &tempsidPtr);
			if (tempsidPtr != NULL && tempsidPtr->sid_authcount <= KAUTH_NTSID_MAX_AUTHORITIES)
			{
				result->fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
				memset(result->fSID, 0, sizeof(ntsid_t));
				memcpy(result->fSID, tempsidPtr, KAUTH_NTSID_SIZE(tempsidPtr));
				AddToHash(gSIDHash, result);
			}
			
			if (tempguid != NULL)
			{
				memcpy(&result->fGUID, tempguid, sizeof(guid_t));
			}
			else
			{
				long* temp = (long*)&result->fGUID;

				if ( recordType == TYPE_USER || recordType == TYPE_COMPUTER )
				{
					temp[0] = htonl(0xFFFFEEEE);
					temp[1] = htonl(0xDDDDCCCC);
					temp[2] = htonl(0xBBBBAAAA);
					temp[3] = htonl(result->fID);
				}
				else
				{
					temp[0] = htonl(0xAAAABBBB);
					temp[1] = htonl(0xCCCCDDDD);
					temp[2] = htonl(0xEEEEFFFF);
					temp[3] = htonl(result->fID);
				}
			} 
			
			AddToHash(gGUIDHash, result);
			
			if ( recordType == TYPE_USER || recordType == TYPE_COMPUTER )
				AddToHash(gUIDHash, result);
			else
				AddToHash(gGIDHash, result);
		}

		result->fNotFound = 1;
		result->fExpiration = GetElapsedSeconds() + gDefaultNegativeExpiration;
		result->fLoginExpiration = GetElapsedSeconds() + gLoginExpiration;
	}
	
	return result;
}

int IsCompatibilityGUID(guid_t* guid, int* isUser, uid_t* id)
{
	unsigned long* temp = (unsigned long*)guid;
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
	UserGroup* result = GetItem(TYPE_USER, kDSNAttrRecordName, NULL, NULL, 0, name);
	if (result->fNotFound)
		result = NULL;
	
	return result;
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
	if (memcmp(&user->fGUID, groupGUID, sizeof(guid_t)) == 0)
		return 1;

	if (memcmp(&gEveryoneGuid, groupGUID, sizeof(guid_t)) == 0)
		return 1;
	
	if (memcmp(&gNetAccountsGuid, groupGUID, sizeof(guid_t)) == 0)
		return ((user->fIsUser || user->fIsComputer) && user->fIsLocalAccount == false);
	
	if (memcmp(&gLocalAccountsGuid, groupGUID, sizeof(guid_t)) == 0)
		return ((user->fIsUser || user->fIsComputer) && user->fIsLocalAccount == true);
	
	GenerateItemMembership(user);
	
	result = HashLookup(user->fGUIDMembershipHash, groupGUID);

	// if no result here, let's see if everyone is a member of this group
	if ( result == NULL )
	{
		UserGroup *everyone = GetItemWithGUID( &gEveryoneGuid );
		if ( everyone != NULL ) // should never fail, but safety
		{
			GenerateItemMembership( everyone );
			
			result = HashLookup(everyone->fGUIDMembershipHash, groupGUID);
		}
	}
	
	// if the account is not local we need to check if NetAccounts is nested in the group
	if ( result == NULL && user->fIsLocalAccount == false )
	{
		UserGroup *netAccts = GetItemWithGUID( &gNetAccountsGuid );
		if ( netAccts != NULL ) // should never fail, but safety
		{
			GenerateItemMembership( netAccts );
			
			result = HashLookup(netAccts->fGUIDMembershipHash, groupGUID);
		}
	}

	// if the account is local we need to check if LocalAccounts is nested in the group
	if ( result == NULL && user->fIsLocalAccount == true )
	{
		UserGroup *localAccts = GetItemWithGUID( &gLocalAccountsGuid );
		if ( localAccts != NULL ) // should never fail, but safety
		{
			GenerateItemMembership( localAccts );
			
			result = HashLookup(localAccts->fGUIDMembershipHash, groupGUID);
		}
	}
	
	return (result != NULL);
}

int IsUserMemberOfGroupBySID(UserGroup* user, ntsid_t* groupSID)
{
	ntsid_t tempSID;
	UserGroup* result;
	
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

	// if no result here, let's see if everyone is a member of this group
	if ( result == NULL )
	{
		UserGroup *everyone = GetItemWithGUID( &gEveryoneGuid );
		if ( everyone != NULL ) // should never fail, but safety
		{
			GenerateItemMembership( everyone );
			
			result = HashLookup(everyone->fSIDMembershipHash, &tempSID);
		}
	}	
	
	return (result != NULL);
}

int IsUserMemberOfGroupByGID(UserGroup* user, int gid)
{
	UserGroup* result;
	guid_t temp;
	
	GenerateItemMembership(user);
		
	result = HashLookup(user->fGIDMembershipHash, (void*)&gid);
	memset(&temp, 0, sizeof(guid_t));
	*(uid_t*)&temp = gid;
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
		
		if (numResults == 16) break;
	}
		
	return numResults;
}

int GetAllGroups(UserGroup* user, gid_t** gidArray)
{
	int numResults = 0;
	
	GenerateItemMembership(user);
	
	(*gidArray) = NULL;
	
	HashTable* hash = user->fGUIDMembershipHash;
	if ( hash != NULL && hash->fTable != NULL )
	{
		gid_t	*itemArray = NULL;
		int		iTemp = 1; // start with 1 since we have a primary group always
		int		i;
		
		// first see how many we have
		for (i = 0; i < hash->fTableSize; i++)
		{
			if (hash->fTable[i] != NULL)
				iTemp++;
		}
		
		(*gidArray) = itemArray = (gid_t *) calloc( iTemp, sizeof(gid_t) );
		
		// add the primary group to the top of the list
		itemArray[numResults++] = user->fPrimaryGroup;

		for (i = 0; i < hash->fTableSize; i++)
		{
			if (hash->fTable[i] != NULL)
			{
				UserGroup* group = hash->fTable[i];
				
				// if this group isn't our primary group
				if (group->fID != user->fPrimaryGroup)
					itemArray[numResults++] = group->fID;
			}
		}
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
		if (ug->fIsUser)
			RemoveFromHash(gUserNameHash, ug);
		else if (ug->fIsComputer)
			RemoveFromHash(gComputerNameHash, ug);
		else
			RemoveFromHash(gGroupNameHash, ug);

		free(ug->fName);
		ug->fName = NULL;
	}
	if (ug->fSID != NULL)
	{
		RemoveFromHash(gSIDHash, ug);
		
		free(ug->fSID);
		ug->fSID = NULL;
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

UserGroup* GetNewUGStruct(void)
{
	UserGroup* result = NULL;
	uint32_t currentTime = GetElapsedSeconds();

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
				// we'll keep the entry the minimum of the login expiration time before recycling
				if (current->fRefCount == 0 && currentTime > current->fLoginExpiration)
				{
					break;
				}
				current = current->fBackLink;
			}
			
			if (current != NULL)
			{
				ResetUserGroup(current);
				result = current;
			}
			else
			{
				gMaxGUIDSInCache += 10;
			}
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


UserGroup* FindOrAddUG(UserGroup* templateUG, int hasGUID, int hasID, int foundByID, int foundByName)
{
	UserGroup* result = NULL;
	
	if (!hasID)
	{
		if (templateUG->fName != NULL)
		{
			free(templateUG->fName);
			templateUG->fName = NULL;
		}
		if (templateUG->fSID != NULL)
		{
			free(templateUG->fSID);
			templateUG->fSID = NULL;
		}
		return NULL;
	}
	else
	{
		if (!hasGUID)
		{
			long* temp = (long*)&templateUG->fGUID;
			if (templateUG->fIsUser)
			{
				temp[0] = htonl(0xFFFFEEEE);
				temp[1] = htonl(0xDDDDCCCC);
				temp[2] = htonl(0xBBBBAAAA);
				temp[3] = htonl(templateUG->fID);
			}
			else
			{
				temp[0] = htonl(0xAAAABBBB);
				temp[1] = htonl(0xCCCCDDDD);
				temp[2] = htonl(0xEEEEFFFF);
				temp[3] = htonl(templateUG->fID);
			}
			//no workaround for fIsComputer
		}
	}

	result = HashLookup(gGUIDHash, &templateUG->fGUID);
	
	if (result == NULL || ItemOutdated(result))
	{
		int checkval;
	
		if (result == NULL)
			result = GetNewUGStruct();
		else
		{
			// wipe out old info after saving refcount
			templateUG->fRefCount = result->fRefCount;
			ResetUserGroup(result);
		}

		checkval = result->fCheckVal;
		memcpy(result, templateUG, sizeof(UserGroup));
		result->fCheckVal = checkval + 1;
		
		AddToHeadOfList(result);
		AddToHash(gGUIDHash, result);
		if (result->fSID != NULL)
			AddToHash(gSIDHash, result);
	}
	else
	{
		if (templateUG->fName != NULL)
		{
			free(templateUG->fName);
			templateUG->fName = NULL;
		}
		if (templateUG->fSID != NULL)
		{
			free(templateUG->fSID);
			templateUG->fSID = NULL;
		}
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
		else if (result->fIsComputer)
			UpdateIDOrNameHash(gComputerNameHash, result, result->fName);
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
		sprintf(current, "-%u", sid->sid_authorities[i]);
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
	{
		primary = GetGroupWithGID(item->fPrimaryGroup);
		//we need to use the GUID value for this PrimaryGroupID below so we can check for nested groups for it
	}
	
	if (primary != NULL)
	{
		AddToHash(item->fGUIDMembershipHash, primary);
		if (primary->fSID != NULL)
			AddToHash(item->fSIDMembershipHash, primary);
		AddToHash(item->fGIDMembershipHash, primary);
	}
	
	ConvertGUIDToString(&item->fGUID, guidString);

	if ( (item->fIsUser) || (item->fIsComputer) )
	{
		uint64_t microsec = GetElapsedMicroSeconds();
		DoRecordSearch(TYPE_GROUP, kDSNAttrGroupMembership, item->fName, item);
		DoRecordSearch(TYPE_GROUP, kDSNAttrGroupMembers, guidString, item);

		if (primary != NULL)
		{
			char primaryguidString[37];
			ConvertGUIDToString(&primary->fGUID, primaryguidString);
			DoRecordSearch(TYPE_GROUP, kDSNAttrNestedGroups, primaryguidString, item);
		}
		
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
	UInt32 recCount = 1;
	tContextData localContext = 0;
	UInt32 buffSize = 4096;
	tDataBufferPtr searchBuffer = dsDataBufferAllocate( gMbrdDirRef, buffSize );
	tDataListPtr recType;
	UserGroup* result = NULL;
	tDirStatus status;
	unsigned int recordIndex;
	unsigned int attrIndex;
	uint64_t microsec = GetElapsedMicroSeconds();
	uint64_t totalTime = 0;
	int foundByID, foundByName;
	
	if (recordType == TYPE_USER)
		recType = gUserType;
	else if (recordType == TYPE_COMPUTER)
		recType = gComputerType;
	else if (recordType == TYPE_GROUP)
		recType = gGroupType;
	else
		recType = gUnknownType;

	tDataNodePtr attrType = dsDataNodeAllocateString(gMbrdDirRef, attribute);
	tDataNodePtr lookUpPtr = dsDataNodeAllocateString(gMbrdDirRef, value);
	
	do {
		do {
			recCount = 0;
			status = dsDoAttributeValueSearchWithData(gMbrdSearchNode, searchBuffer, recType,
											  attrType, eDSExact, lookUpPtr, gAttrsToGet, 0,
											  &recCount, &localContext);
			if (status == eDSBufferTooSmall) {
				buffSize *= 2;
				
				// a safety for a runaway condition
				if ( buffSize > 1024 * 1024 )
					break;
				
				dsDataBufferDeAllocate( gMbrdDirRef, searchBuffer );
				searchBuffer = dsDataBufferAllocate( gMbrdDirRef, buffSize );
				if ( searchBuffer == NULL )
					status = eMemoryError;
			}
		} while (((status == eDSNoErr) && (recCount == 0) && (localContext != 0)) || 
					(status == eDSBufferTooSmall));
		
		if ((status == eDSNoErr) && (recCount <= 0))
			break;
		
		if ( status == eDSInvalidContinueData )
		{
			localContext = 0;
			continue;
		}
		else if (status != eDSNoErr)
		{
			break;
		}

		for (recordIndex = 1; recordIndex <= recCount; ++recordIndex)
		{
			tAttributeValueListRef 	attributeValueListRef = 0;
			tAttributeListRef 		attributeListRef = 0;
			tRecordEntryPtr 		recordEntryPtr = NULL;
			tAttributeEntryPtr 		attributeInfo = NULL;
			tAttributeValueEntryPtr attrValue = NULL;
			int hasGUID = 0;
			int hasID = 0;
			
			status = dsGetRecordEntry(gMbrdSearchNode, searchBuffer, recordIndex, &attributeListRef, &recordEntryPtr);
			if (status == eDSNoErr)
			{
				char* recTypeStr = NULL;
				UserGroup templateUG;
				memset(&templateUG, 0, sizeof(UserGroup));
				templateUG.fExpiration = GetElapsedSeconds() + gDefaultExpiration;
				templateUG.fLoginExpiration = GetElapsedSeconds() + gLoginExpiration;
				
				status = dsGetRecordTypeFromEntry( recordEntryPtr, &recTypeStr );
				if (status == eDSNoErr && (strcmp(recTypeStr, kDSStdRecordTypeUsers) == 0) )
				{
					templateUG.fIsUser = 1;
					templateUG.fIsComputer = 0;
				}
				else if (status == eDSNoErr && (strcmp(recTypeStr, kDSStdRecordTypeComputers) == 0) )
				{
					templateUG.fIsUser = 0;
					templateUG.fIsComputer = 1;
				}
				else if (status == eDSNoErr && ( (strcmp(recTypeStr, kDSStdRecordTypeGroups) == 0) || (strcmp(recTypeStr, kDSStdRecordTypeComputerGroups) == 0) ) )
				{
					templateUG.fIsUser = 0;
					templateUG.fIsComputer = 0;
				}
				else
				{
					if (status == eDSNoErr) free(recTypeStr);
					continue;
				}
					
				free(recTypeStr);
				
				bool bWasSetByCopyTimestamp = false;
				for (attrIndex = 1; attrIndex <= recordEntryPtr->fRecordAttributeCount; ++attrIndex)
				{
					status = dsGetAttributeEntry(gMbrdSearchNode, searchBuffer, attributeListRef, 
												 attrIndex, &attributeValueListRef, &attributeInfo);									 		
					if (status == eDSNoErr)
					{
						status = dsGetAttributeValue(gMbrdSearchNode, searchBuffer, 1, attributeValueListRef, &attrValue);	
						if (status == eDSNoErr)
						{
							char* attrName = (char*)attributeInfo->fAttributeSignature.fBufferData;
							if (strcmp(attrName, kDSNAttrRecordName) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								templateUG.fName = (char *)malloc(strlen(temp) + 1);
								strcpy(templateUG.fName, temp);
							}
							else if (strcmp(attrName, kDS1AttrGeneratedUID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								ConvertGUIDFromString(temp, &templateUG.fGUID);
								hasGUID = 1;
							}
							else if (strcmp(attrName, kDS1AttrSMBSID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								templateUG.fSID = (ntsid_t*)malloc(sizeof(ntsid_t));
								ConvertSIDFromString(temp, templateUG.fSID);
							}
							else if (strcmp(attrName, kDS1AttrUniqueID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								templateUG.fID = strtol(temp, NULL, 10);
								hasID = 1;
							}
							else if ( (strcmp(attrName, kDS1AttrENetAddress) == 0) && (hasID != 1) ) //computer record found
							{
								char* ptr = (char*)(attrValue->fAttributeValueData.fBufferData);
								//convert Mac address 00:11:aa:bb:cc:22 into 32 bit number 0xaabbcc22
								//might NOT be leading zero format
								ptr = strstr(ptr, ":"); ptr++;
								ptr = strstr(ptr, ":"); ptr++;
								char *holder = (char *)calloc(1, 11);
								holder[0]='0';
								holder[1]='x';
								int cnt = 2;
								while ( (ptr != NULL) && (cnt < 10) )
								{
									if (ptr[0] != ':')
									{
										if ( ((ptr+1 == NULL) || (ptr[1] == ':')) && (cnt != 9) && ( (cnt % 2) == 0) )
										{
											holder[cnt] = '0'; //add in missing leading zero if required
											cnt++;
										}
										holder[cnt] = ptr[0];
										cnt++;
									}
									ptr++;
								}
								templateUG.fID = strtol(holder, NULL, 16);
								free(holder);
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
								
								templateUG.fExpiration = GetElapsedSeconds() + num * multiplier;
							}
							else if (strcmp(attrName, kDS1AttrPrimaryGroupID) == 0)
							{					 
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);
								int id = strtol(temp, NULL, 10);
								if (templateUG.fIsUser)
								{
									templateUG.fPrimaryGroup = id;
								}
								else
								{
									templateUG.fID = id;
									hasID = 1;
								}
							}
							else if ( bWasSetByCopyTimestamp == false && strcmp(attrName, kDSNAttrMetaNodeLocation) == 0 )
							{
								char* temp = (char*)(attrValue->fAttributeValueData.fBufferData);

								templateUG.fIsLocalAccount = false;
								if ( strcmp(temp, "/Local/Default") == 0 || strcmp(temp, "/BSD/local") == 0 )
									templateUG.fIsLocalAccount = true;
							}
							else if ( strcmp(attrName, kDS1AttrCopyTimestamp) == 0 && attrValue->fAttributeValueData.fBufferLength > 0 )
							{
								// if the account has a copyTimeStamp it is not local so we flag it as remote
								templateUG.fIsLocalAccount = false;
								bWasSetByCopyTimestamp = true; // save this because attr order is not guaranteed
							}

							dsDeallocAttributeValueEntry(gMbrdDirRef, attrValue);	
						}

						dsDeallocAttributeEntry(gMbrdDirRef, attributeInfo);	
						dsCloseAttributeValueList(attributeValueListRef);
					}
				}

				dsDeallocRecordEntry(gMbrdDirRef, recordEntryPtr);
				dsCloseAttributeList(attributeListRef);
				
				foundByID = ((strcmp(attribute, kDS1AttrUniqueID) == 0) || (strcmp(attribute, kDS1AttrPrimaryGroupID) == 0));
				foundByName = (strcmp(attribute, kDSNAttrRecordName) == 0);
				
				gMbrGlobalMutex.WaitLock();
				
				result = FindOrAddUG(&templateUG, hasGUID, hasID, foundByID, foundByName);
				if (result)
				{
					if (membershipRoot)
					{
						AddToHash(membershipRoot->fGUIDMembershipHash, result);
						if (result->fSID != NULL)
							AddToHash(membershipRoot->fSIDMembershipHash, result);
						AddToHash(membershipRoot->fGIDMembershipHash, result);

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
						gMbrGlobalMutex.SignalLock();
						if (localContext != 0)
						{
							dsReleaseContinueData(gMbrdSearchNode, localContext);
						}
						localContext = 0;
						break;  // we only need to find the first entry
					}
				}
				
				gMbrGlobalMutex.SignalLock();
			}
		}
	} while ( localContext != 0 );
	
	dsDataBufferDeAllocate(gMbrdDirRef, searchBuffer);
	dsDataNodeDeAllocate(gMbrdDirRef, attrType);
	dsDataNodeDeAllocate(gMbrdDirRef, lookUpPtr);
	
	totalTime += GetElapsedMicroSeconds() - microsec;
	if (strcmp(attribute, kDSNAttrGroupMembers) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerGUIDMemberSearch, &gStatBlock->fTotalGUIDMemberSearches, (uint32_t)totalTime);
	else if (strcmp(attribute, kDSNAttrNestedGroups) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerNestedMemberSearch, &gStatBlock->fTotalNestedMemberSearches, (uint32_t)totalTime);
	else if (strcmp(attribute, kDSNAttrGroupMembership) == 0)
		AddToAverage(&gStatBlock->fAverageuSecPerLegacySearch, &gStatBlock->fTotalLegacySearches, (uint32_t)totalTime);

	return result;
}

void RemoveFromList(UserGroup* ug)
{
	if (ug->fLink == NULL)
		gListTail = ug->fBackLink;
	else
		ug->fLink->fBackLink = ug->fBackLink;
	
	if (ug->fBackLink == NULL)
		gListHead = ug->fLink;
	else
		ug->fBackLink->fLink = ug->fLink;
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

void* UIDCacheIndexToPointer(TempUIDCacheBlockBase* block, int inIndex)
{
	void* result;

	if (block->fKind == kUUIDBlock || block->fKind == kSmallSIDBlock)
	{
		TempUIDCacheBlockSmall* temp = (TempUIDCacheBlockSmall*)block;
		result = &temp->fGUIDs[inIndex];
	}
	else
	{
		TempUIDCacheBlockLarge* temp = (TempUIDCacheBlockLarge*)block;
		result = &temp->fSIDs[inIndex];
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
		
		if (block->fKind == (uid_t) blockKind)
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

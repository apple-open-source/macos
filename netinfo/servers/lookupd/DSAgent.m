/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2001 Apple Computer, Inc. All Rights
 * Reserved. This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License'). You may not use this file
 * except in compliance with the License. Please obtain a copy of the
 * License at http:www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * DSAgent.m
 * Directory Services agent for lookupd
 */

#import "DSAgent.h"

#ifndef _ENABLE_DSAGENT_
@implementation DSAgent
- (DSAgent *)init
{
	[self release];
	return nil;
}

- (LUAgent *)initWithArg:(char *)arg
{
	[self release];
	return nil;
}
@end
#else

#import "LUGlobal.h"
#import <NetInfo/system_log.h>
#import <NetInfo/dsutil.h>
#import "Thread.h"

#import <string.h>
#import <stdlib.h>
#import <sys/stat.h>
#import <DirectoryService/DirServicesUtils.h>

/*
 * This table MUST be kept up to date with the NetInfo plug-in
 * in Directory Services
 */

#ifdef DSDEBUG
#define kAttrConsts 45
#else
#define kAttrConsts 41
#endif

static const char *sAttrMap[kAttrConsts][2] =
{
 /* 1 */	{ kDSNAttrRecordName, "name" },
 			{ kDS1AttrDistinguishedName, "realname" },
			{ kDS1AttrPasswordPlus, "passwd" },
			{ kDS1AttrPassword, "passwd" }, /* needed when retrieving all so that standard type received gets correctly mapped */
 /* 5 */	{ kDS1AttrUniqueID, "uid" },
			{ kDS1AttrPrimaryGroupID, "gid" },
			{ kDS1AttrUserShell, "shell" },
			{ kDS1AttrNFSHomeDirectory, "home" },
			{ kDSNAttrHomeDirectory, "home_loc" },
 /* 10 */ 	{ kDS1AttrInternetAlias, "InetAlias" },
			{ kDS1AttrMailAttribute, "applemail" },
			{ kDS1AttrAuthenticationHint, "hint" },
			{ kDS1AttrRARA, "RARA" },
			{ kDS1AttrGeneratedUID, "GeneratedUID" },
 /* 15 */	{ kDSNAttrGroupMembership, "users" },
			{ kDSNAttrEMailAddress, "mail" },
			{ kDSNAttrURL, "URL" },
			{ kDSNAttrURLForNSL, "URL" },
			{ kDSNAttrMIME, "mime" },
 /* 20 */	{ kDSNAttrHTML, "htmldata" },
			{ kDSNAttrNBPEntry, "NBPEntry" },
			{ kDSNAttrDNSName, "dnsname" },
			{ kDSNAttrIPAddress, "IP_Address" },
			{ kDS1AttrPrintServiceInfoText, "PrintServiceInfoText" },
 /* 25 */	{ kDS1AttrPrintServiceInfoXML, "PrintServiceInfoXML" },
			{ kDS1AttrVFSType, "vfstype" },
			{ kDS1AttrVFSPassNo, "passno" },
			{ kDS1AttrVFSDumpFreq, "dump_freq" },
			{ kDS1AttrVFSLinkDir, "dir" },
 /* 30 */	{ kDSNAttrVFSOpts, "opts" },
			{ kDS1AttrAliasData, "alias_data" },
			{ kDSNAttrPhoneNumber, "phonenumber" },
			{ kDS1AttrCapabilities, "capabilities" },
			{ kDSNAttrProtocols, "protocols" },
 /* 35 */	{ kDSNAttrMember, "users" },
#ifdef DSDEBUG
 /* 36 */	{ kDSNAttrAllNames, "dsAttrTypeStandard:AllNames" },
			{ kStandardTargetAlias, "dsAttrTypeStandard:AppleMetaAliasTarget" },
			{ kStandardSourceAlias, "dsAttrTypeStandard:AppleMetaAliasSource" },
			{ kDSNAttrMetaNodeLocation, "dsAttrTypeStandard:AppleMetaNodeLocation" },
#endif
 /* 40-36 */{ kDS1AttrComment, "comment" },
			{ kDS1AttrAdminStatus, "AdminStatus" },
			{ kDS1AttrPwdAgingPolicy, "PwdAgingPolicy" },
			{ kDS1AttrChange, "change" },
			{ kDS1AttrExpire, "expire" },
 /* 45-41 */{ kDSNAttrRecordAlias, "RecordAlias" }
};

/* Static globals */

/* Static consts */
/* 4096 + 2048 */
static const unsigned long kBuffSize = 6144; 

/* Globals */

/* Can DS do the work or are we a noop */
BOOL gDSRunState = NO;

/* Did we check the DS file indicator ie. we do this only once */
BOOL gCheckDSFileIndicator = YES;

/* How long before we check our run state */
time_t gSeconds = 0; 

/* DS reference and search node reference are globals since DS FW is single threaded */
tDirReference gDirRef = 0;
tDirNodeReference gNodeRef = 0;

static void
LogErrStr(int inLine, char *inObjC, char *inAPICall, tDirStatus inError)
{
#ifdef DSDEBUG
	system_log(LOG_DEBUG, "-- DS: %d: *** Error *** %s: %s() error = %d.", inLine , inObjC, inAPICall, inError);
#endif
}

static void
LogDbgStr(int inLine, char *inObjC, char *inDbgStr)
{
#ifdef DSDEBUG
	system_log(LOG_DEBUG, "-- DS: %d: %s: %s.", inLine, inObjC, inDbgStr);
#endif
}

@implementation DSAgent

- (BOOL)doWeUseDS
{
	struct stat statResult;
	
	/*
	 * ONLY if custom search policy is set then use DirectoryService
	 */
	if (stat("/Library/Preferences/DirectoryService/.DSRunningSP3", &statResult) == 0) return YES;

#ifdef NOTDEF
	/*
	 * check to see if DS is running and then we use it
	 */
	if (dsIsDirServiceRunning() == eDSNoErr) return YES;

	/*
	 * now check if we need to allow the DS framework to restart DS
	 * ie. the system has been rebooted and the search policy HAD been set to either
	 * "custom" or "local netinfo only" so in these two cases we re-start DS
	 * we make this check only once
	 * ie. we check for the presence of the files
	 * "/Library/Preferences/DirectoryService/.DSRunningSP2" or ".DSRunningSP3"
	 */
	if (gCheckDSFileIndicator)
	{
		gCheckDSFileIndicator = NO;
		if (stat("/Library/Preferences/DirectoryService/.DSRunningSP2", &statResult) == 0) return YES;
		if (stat("/Library/Preferences/DirectoryService/.DSRunningSP3", &statResult) == 0) return YES;
	}
#endif

	return NO;
}

- (BOOL)canWeWork:(tDirReference*)inDirRef withSearchNode:(tDirNodeReference*)inNodeRef
{
	tDirStatus status = eDSNoErr;
	tDataList *pDataList = NULL;
	tDataBuffer *pDataBuff = NULL;
	tDirReference dirRef = 0;
	tDirNodeReference nodeRef = 0;
	unsigned long count = 0;
	time_t timenow = 0;

	timenow = time(NULL);

	/* Is it time to check or continue with past state */
	if (gSeconds > timenow)
	{
		/* Continuing with current state */
		/* If DS was not running then return */
		if (!gDSRunState) return NO;
		/* Else DS is running so we continue below to see if we have proper references */
	}
	else
	{
		/* Time to check */
		/* Don't bother if DS isn't running or DS should NOT be re-started */
		if (![self doWeUseDS])
		{
			gDSRunState = NO;
	
			/* Set the time to the future */
			gSeconds = timenow + 30;

			return NO;
		}
		/* Else DS is running so we continue below to get the proper references */
	}
	
	gDSRunState = YES;

	/* Set the time to the future */
	gSeconds = timenow + 30;

	/* verify that we have a valid dir ref otherwise get a new one */
	if ((*inDirRef == 0) || (dsVerifyDirRefNum(*inDirRef) != eDSNoErr))
	{
		*inDirRef = 0;
		*inNodeRef = 0;

		/* Open DirectoryService */
		status = dsOpenDirService(&dirRef);
		if (status != eDSNoErr)
		{
#ifdef DSDEBUG
			LogErrStr(__LINE__, "cww", "dsOpenDirService failed", status);
#endif
			gDSRunState = NO;
			return NO;
		}
		else
		{
			*inDirRef = dirRef;
		}
	}

	/* verify that we have a valid search node ref otherwise we get a new one */
	if (*inNodeRef == 0)
	{
		/* Allocate the data buffer to be used here - 512 chars max for search node name */
		pDataBuff = dsDataBufferAllocate(*inDirRef, 512);
		if (pDataBuff == NULL)
		{
			dsCloseDirService(*inDirRef);
			*inDirRef = 0;
			gDSRunState = NO;
			return NO;
		}

		/* Get the search node */
		status = dsFindDirNodes(*inDirRef, pDataBuff, NULL, eDSSearchNodeName, &count, NULL);

		/* if error or expecting only one search node returned */
		if ((status != eDSNoErr) || (count != 1))
		{
#ifdef DSDEBUG
			LogErrStr(__LINE__, "cww", "dsFindDirNodes can't get search node", status);
#endif
			dsDataBufferDeAllocate(*inDirRef, pDataBuff);
			dsCloseDirService(*inDirRef);
			*inDirRef = 0;
			gDSRunState = NO;
			return NO;
		}

		/* Allocate the tDataList to retrieve the search node name */
		pDataList = dsDataListAllocate(*inDirRef);
		if (pDataList == NULL)
		{
			dsDataBufferDeAllocate(*inDirRef, pDataBuff);
			dsCloseDirService(*inDirRef);
			*inDirRef = 0;
			gDSRunState = NO;
			return NO;
		}

		/* Now get the search node name so we can open it - index is one based */
		status = dsGetDirNodeName(*inDirRef, pDataBuff, 1, &pDataList);
		if (status != eDSNoErr)
		{
			dsDataBufferDeAllocate(*inDirRef, pDataBuff);
			dsDataListDeallocate(*inDirRef, pDataList);
			free(pDataList);
			dsCloseDirService(*inDirRef);
			*inDirRef = 0;
			gDSRunState = NO;
			return NO;
		}

		/* Open the search node */
		status = dsOpenDirNode(*inDirRef, pDataList, &nodeRef);
		dsDataListDeallocate(*inDirRef, pDataList);
		free(pDataList);

		if (status == eDSNoErr)
		{
#ifdef DSDEBUG
			LogDbgStr(__LINE__, "cww", "Search node opened");
#endif
			*inNodeRef = nodeRef;
		}
		else
		{
			dsDataBufferDeAllocate(*inDirRef, pDataBuff);
			dsCloseDirService(*inDirRef);
			*inDirRef = 0;
			gDSRunState = NO;
			return NO;
		}

		/* Deallocate the temp buff */
		dsDataBufferDeAllocate(*inDirRef, pDataBuff);
	}

	/*
	 * if DS is running then we use it since in the case of NetInfo default DS can resolve aliases and
	 * lookupd will have consulted NetInfo directly already and not found anything if it makes it here
	 */
	gDSRunState = YES;
	return YES;
}

- (void)dealloc
{
#ifdef DSDEBUG
	char errStr[256];

	sprintf(errStr, "Deallocated DSAgent 0x%08x\n", (int)self);
	LogDbgStr(__LINE__, "alloc", errStr);
#endif

	[super dealloc];
}

- (DSAgent *)init
{
	return (DSAgent *)[self initWithArg:NULL];
}

- (LUAgent *)initWithArg:(char *)arg
{
#ifdef DSDEBUG
	char errStr[256];
#endif
		
	[super initWithArg:arg];

	if (gSeconds == 0) gSeconds = time(NULL) - 1;

#ifdef DSDEBUG
	sprintf(errStr, "Created new DSAgent: 0x%08x", (int)self);
	LogDbgStr(__LINE__, "alloc", errStr);
#endif

	return self;
}

- (const char *)shortName
{
	return("DS");
}

- (LUDictionary *)stamp:(LUDictionary *)inDict
{
	time_t now;
	char scratch[32];

	if (inDict == NULL) return nil;

	now = time(NULL);
	sprintf(scratch, "%lu", now);

	[inDict setAgent:self];
	[inDict setValue:"DirectoryServices" forKey:"_lookup_info_system"];
	[inDict setValue:scratch forKey:"_lookup_DS_timestamp"];

	return inDict;
}

- (void)stampAll:(LUArray *)inArray addToList:(LUArray *)inList
{
	int i;
	int len;
	time_t now;
	char scratch[32];
	LUDictionary *dictItem;

	if (inArray == NULL) return;

	now = time(NULL);
	sprintf(scratch, "%lu", now);

	len = [inArray count];
	for (i = 0; i < len; i++)
	{
		dictItem = [inArray objectAtIndex:i];
		[dictItem setAgent:self];
		[dictItem setValue:"DirectoryServices" forKey:"_lookup_info_system"];
		[dictItem setValue:scratch forKey:"_lookup_DS_timestamp"];

		[inList addObject:dictItem];
	}
}

- (BOOL)isValid:(LUDictionary *)inDict
{
	time_t now = 0;
	time_t goodUntil = 0;

	if (inDict == NULL) return NO;

	goodUntil = [inDict unsignedLongForKey:"_lookup_DS_timestamp"];

	/* might need to review this time for live values of the cache */
	goodUntil += 600;

	now = time(0);
	if (now > goodUntil) return NO;

	return YES;
}

- (char *)mapDSAttrToNetInfoType:(const char *)inAttrType
{
	int i = 0;
	char *outResult = NULL;
	unsigned long uiStdLen = strlen(kDSStdAttrTypePrefix); 

	if (inAttrType == NULL) return NULL;

	if (strncmp(inAttrType, kDSStdAttrTypePrefix, uiStdLen) == 0)
	{
		for (i = 0; i < kAttrConsts; i++)
		{
			if (strcmp(inAttrType, sAttrMap[i][0]) == 0)
			{
				outResult = (char *)malloc(strlen(sAttrMap[i][1]) + 1);
				strcpy(outResult, sAttrMap[i][1]);
				break;
			}
		}
	}

	return outResult;
}

- (char *)mapNetInfoAttrToDSType:(const char *)inAttrType
{
	char *outResult = NULL;
	int i = 0;

	if (inAttrType == NULL) return NULL;

	/* Look for a standard type */
	for (i = 0; i < kAttrConsts; i++)
	{
		if (strcmp(inAttrType, sAttrMap[i][1]) == 0)
		{
			outResult = (char *)malloc(strlen(sAttrMap[i][0]) + 1);
			strcpy(outResult, sAttrMap[i][0]);
			break;
		}
	}

	return outResult;
}


- (char *)mapNetInfoRecToDSType:(LUCategory)inCategory
{
#ifdef DSDEBUG
	char errStr[4096];
#endif
	
	switch (inCategory)
	{
		case LUCategoryUser: return(kDSStdRecordTypeUsers);
		case LUCategoryGroup: return(kDSStdRecordTypeGroups);
		case LUCategoryInitgroups: return(kDSStdRecordTypeGroups);
		case LUCategoryMount: return(kDSStdRecordTypeMounts);
		case LUCategoryHost:
		case LUCategoryNetwork:
		case LUCategoryProtocol:
		case LUCategoryRpc:
		case LUCategoryPrinter:
		case LUCategoryBootparam:
		case LUCategoryBootp:
		case LUCategoryAlias:
		case LUCategoryNetgroup:
		case LUCategoryHostServices:
		default:
#ifdef DSDEBUG
			sprintf(errStr, "*** Warning *** Unmapped category: %d", inCategory);
			LogDbgStr(__LINE__, "mapNetInfoRecToDSType", errStr);
#endif
			return NULL;
	}

	return NULL;
}

- (LUDictionary *)dictFromDS:(tDirNodeReference)node buffer:(tDataBuffer *)buf index:(int)which
{
	LUDictionary *item;
	tDirStatus status;
	tAttributeListRef attrListRef = 0;
	tRecordEntry *pRecEntry = NULL;
	tAttributeEntry *pAttrEntry = NULL;
	tAttributeValueListRef valueRef = 0;
	tAttributeValueEntry *pValueEntry = NULL;
	char *pNIKey = NULL;
	char **pValArray = NULL;
	int i, j, k;

	attrListRef = 0;
	pRecEntry = NULL;

	/* Using "which plus one" since DS indices for retrieval are one-based */
	status = dsGetRecordEntry(node, buf, which+1, &attrListRef, &pRecEntry);
	if (status != eDSNoErr) return nil;
	item = [[LUDictionary alloc] init];

	for (i = 0; i < pRecEntry->fRecordAttributeCount; i++)
	{
		pAttrEntry = NULL;
		valueRef = 0;

		status = dsGetAttributeEntry(node, buf, attrListRef, i+1, &valueRef, &pAttrEntry);
		/* Still try for the next attribute if this one fails */
		if (status != eDSNoErr) continue;
	
		pNIKey = [self mapDSAttrToNetInfoType:pAttrEntry->fAttributeSignature.fBufferData];
		if (pNIKey == NULL)
		{
			dsCloseAttributeValueList(valueRef);
			dsDeallocAttributeEntry(gDirRef, pAttrEntry);
			continue;
		}

		if (pAttrEntry->fAttributeValueCount == 0)
		{
			[item addValue:NULL forKey:pNIKey];
		}
		else if (pAttrEntry->fAttributeValueCount == 1)
		{
			pValueEntry = NULL;
			status = dsGetAttributeValue(node, buf, 1, valueRef, &pValueEntry);

			if (status != eDSNoErr)
			{
				dsCloseAttributeValueList(valueRef);
				dsDeallocAttributeEntry(gDirRef, pAttrEntry);
				continue;
			}

			[item addValue:pValueEntry->fAttributeValueData.fBufferData forKey:pNIKey];
			dsDeallocAttributeValueEntry(gDirRef, pValueEntry);
		}
		else if (pAttrEntry->fAttributeValueCount > 1)
		{
			pValArray = (char **)malloc(sizeof(char **) * (pAttrEntry->fAttributeValueCount + 1));

			for (k=0, j = 0; j < pAttrEntry->fAttributeValueCount; j++)
			{
				pValueEntry = NULL;
				status = dsGetAttributeValue(node, buf, j+1, valueRef, &pValueEntry);
				if (status != eDSNoErr) continue;

				/* keep track of the values actually retrieved */
				pValArray[k] = copyString(pValueEntry->fAttributeValueData.fBufferData);
				k++;

				dsDeallocAttributeValueEntry(gDirRef, pValueEntry);
			}

			pValArray[k] = NULL;

			[item addValues:pValArray forKey:pNIKey count:k];

			for (j = 0; pValArray[j] != NULL; j++) free(pValArray[j]);
			free(pValArray);
		}

		free(pNIKey);
		pNIKey = NULL;
		
		dsCloseAttributeValueList(valueRef);
		dsDeallocAttributeEntry(gDirRef, pAttrEntry);
	}

	dsCloseAttributeList(attrListRef);
	dsDeallocRecordEntry(gDirRef, pRecEntry);

	return item;
}

- (LUDictionary *)itemWithKey:(char *)inKey
	value:(char *)inValue
	category:(LUCategory)inCategory
{
	LUDictionary *item;
	tDirStatus status = eDSNoErr;
	char *pDSRecord = NULL;
	char *pDSKey = NULL;
	tDataList *pRecType = NULL;
	tDataNode *pAttrType = NULL;
	tDataNode *pValue = NULL;
	tDataBuffer *pDataBuffer = NULL;
	unsigned long count = 0;
	tContextData pContext = NULL;
	BOOL bFound = NO;
	Thread *pThread;
#ifdef DSDEBUG
	char errStr[4096];
#endif
		
	pDSRecord = [self mapNetInfoRecToDSType:inCategory];
	if (pDSRecord == NULL) return nil;

	pDSKey = [self mapNetInfoAttrToDSType:inKey];
	if (pDSKey == NULL) return nil;

	if ([self canWeWork:&gDirRef withSearchNode:&gNodeRef] == NO)
	{
		/* pDSRecord is a constant that need not be freed */
		if (pDSKey != NULL)
		{
			free(pDSKey);
			pDSKey = NULL;
		}
		return nil;
	}

#ifdef DSDEBUG
	sprintf(errStr, "key = %s, val = %s, cat = %d", inKey, inValue, inCategory);
	LogDbgStr(__LINE__, "iwk", errStr);
#endif

	/* We need this if we need to sit and spin */
	pThread = [Thread currentThread];
	[pThread setState:ThreadStateActive];

#ifdef DSDEBUG
	sprintf(errStr, "rec = %s, key = %s, val = %s", pDSRecord, pDSKey, inValue);
	LogDbgStr(__LINE__, "iwk", errStr);
#endif
		
	pRecType = dsBuildListFromStrings(gDirRef, pDSRecord, NULL);
	if (pDSRecord != NULL)
	{
		/* DON'T free pDSRecord since it is a constant returned from mapNetInfoRecToDSType */
		pDSRecord = NULL;
	}

	if (pRecType == NULL) return nil;

	pAttrType = dsDataNodeAllocateString(gDirRef, pDSKey);
	if (pDSKey != NULL)
	{
		free(pDSKey);
		pDSKey = NULL;
	}

	if (pAttrType == NULL)
	{
		dsDataListDeallocate(gDirRef, pRecType);
		free(pRecType);
		LogDbgStr(__LINE__, "iwk", "dsDoAttributeValueSearch() item _NOT_ found");
		return nil;
	}

	pValue = dsDataNodeAllocateString(gDirRef, inValue);
	if (pValue == NULL)
	{
		dsDataNodeDeAllocate(gDirRef, pAttrType);
		dsDataListDeallocate(gDirRef, pRecType);
		free(pRecType);
		LogDbgStr(__LINE__, "iwk", "dsDoAttributeValueSearch() item _NOT_ found");
		return nil;
	}

	if (status != eDSNoErr)
	{
		dsDataNodeDeAllocate(gDirRef, pAttrType);
		dsDataListDeallocate(gDirRef, pRecType);
		free(pRecType);
		LogDbgStr(__LINE__, "iwk", "dsDoAttributeValueSearch() item _NOT_ found");
		return nil;
	}

	pDataBuffer = dsDataBufferAllocate(gDirRef, kBuffSize);
	if (pDataBuffer == NULL)
	{
		dsDataNodeDeAllocate(gDirRef, pAttrType);
		dsDataListDeallocate(gDirRef, pRecType);
		free(pRecType);
		LogDbgStr(__LINE__, "iwk", "dsDoAttributeValueSearch() item _NOT_ found");
		return nil;
	}

	count = 0;
	do
	{
		status = dsDoAttributeValueSearch(gNodeRef, pDataBuffer, pRecType, pAttrType, eDSExact, pValue, &count, &pContext);

		/* If the node ref is invalid then reset it to zero */
		if (status == eDSInvalidNodeRef) gNodeRef = 0;

		if (status == eDSNoErr)
		{
			if (count != 0)
			{
#ifdef DSDEBUG
				sprintf(errStr, "dsDoAttributeValueSearch() returned - %lu - item(s)", count);
				LogDbgStr(__LINE__, "iwk", errStr);
#endif
				bFound = YES;
				break;
			}
		}
			
#ifdef DSDEBUG
		sprintf(errStr, "status = %u, pContext = %ld, count = %lu", status, (unsigned long)pContext, count);
		LogDbgStr(__LINE__, "iwk", errStr);
#endif
	} while ((status == eDSNoErr) && (pContext != NULL) && (count == 0));

	dsDataBufferDeAllocate(gDirRef, pValue);
	pValue = NULL;

	dsDataNodeDeAllocate(gDirRef, pAttrType);
	pAttrType = NULL;

	dsDataListDeallocate(gDirRef, pRecType);
	free(pRecType);
	pRecType = NULL;

	if (bFound == NO)
	{
		dsDataBufferDeAllocate(gDirRef, pDataBuffer);
		pDataBuffer = NULL;
		return nil;
	}

	item = [self dictFromDS:gNodeRef buffer:pDataBuffer index:0];
		
	dsDataBufferDeAllocate(gDirRef, pDataBuffer);
	pDataBuffer = NULL;

	return [self stamp:item];
}

- (LUArray *)allItemsWithCategory:(LUCategory)inCategory
{
	tDirStatus status;
	char *pDSRecType = NULL;
	int i;
	unsigned long ulRecCount = 0;
	tDataList *pRecName = NULL;
	tDataList *pRecType = NULL;
	tDataList *pAttrType = NULL;
	tContextData pContext = NULL;
	LUDictionary *item;
	LUDictionary *vstamp;
	LUArray *list = nil;
	BOOL bFirstFound = NO;
	tDataBuffer *pDataBuffer = NULL;

	pDSRecType = [self mapNetInfoRecToDSType:inCategory];
	if (pDSRecType == NULL) return nil;

	if ([self canWeWork:&gDirRef withSearchNode:&gNodeRef] == NO) return nil;

	/* Get all records */
	pRecName = dsBuildListFromStrings(gDirRef, kDSRecordsAll, NULL);
	if (pRecName == NULL) return nil;

	/* And all attributes */
	pAttrType = dsBuildListFromStrings(gDirRef, kDSAttributesStandardAll, NULL);
	if (pAttrType == NULL)
	{
		dsDataListDeallocate(gDirRef, pRecName);
		free(pRecName);
		return nil;
	}

	/* Of this type */
	pRecType = dsBuildListFromStrings(gDirRef, pDSRecType, NULL);
	if (pDSRecType != NULL)
	{
		/* DON'T free this since it is a constant returned from mapNetInfoRecToDSType */
		pDSRecType = NULL;
	}

	if (pRecType == NULL)
	{
		dsDataListDeallocate(gDirRef, pAttrType);
		free(pAttrType);

		dsDataListDeallocate(gDirRef, pRecName);
		free(pRecName);

		return nil;
	}

	pDataBuffer = dsDataBufferAllocate(gDirRef, kBuffSize);
	if (pDataBuffer == NULL)
	{
		dsDataListDeallocate(gDirRef, pAttrType);
		free(pAttrType);

		dsDataListDeallocate(gDirRef, pRecName);
		free(pRecName);

		dsDataListDeallocate(gDirRef, pRecType);
		free(pRecType);

		return nil;
	}

	do
	{
		status = dsGetRecordList(gNodeRef, pDataBuffer, pRecName, eDSExact, pRecType, pAttrType, NO, &ulRecCount, &pContext);

		/* If the node ref is invalid then reset it to zero */
		if (status == eDSInvalidNodeRef) gNodeRef = 0;			

		/* Check for error and at least one record found. */
		if ((status == eDSNoErr) && (ulRecCount != 0))
		{
			if (bFirstFound == NO)
			{
				list = [[LUArray alloc] init];

				vstamp = [[LUDictionary alloc] init];
				[self stamp:vstamp];
				[list addValidationStamp:vstamp];
				[vstamp release];
				bFirstFound = YES;
			}
				
			for (i = 0; i < ulRecCount; i++)
			{
				item = [self dictFromDS:gNodeRef buffer:pDataBuffer index:i];
				if (item != nil)
				{
					[list addObject:item];
					[item release];
				}
			}
		}
	} while ((status == eDSNoErr) && (pContext != NULL));

	dsDataListDeallocate(gDirRef, pRecType);
	free(pRecType);

	dsDataListDeallocate(gDirRef, pAttrType);
	free(pAttrType);

	dsDataListDeallocate(gDirRef, pRecName);
	free(pRecName);

	dsDataBufferDeAllocate(gDirRef, pDataBuffer);
	pDataBuffer = NULL;

	if ([list count] == 0)
	{
		[list release];
		return nil;
	}

	return list;

}

@end
#endif _ENABLE_DSAGENT_

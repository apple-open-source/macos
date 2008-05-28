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

#import <unistd.h>
#import <openssl/md5.h>
#import <sys/stat.h>
#import "ReplicaFile.h"
#import "PSUtilitiesDefs.h"

void pwsf_AppendReplicaStatus( ReplicaFile *inReplicaFile, CFDictionaryRef inDict, CFMutableDictionaryRef inOutDict );

void pwsf_CalcServerUniqueID( const char *inRSAPublicKey, char *outHexHash )
{
	MD5_CTX ctx;
	unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
	
	if ( inRSAPublicKey == NULL ) {
		if ( outHexHash != NULL )
			bzero( outHexHash, MD5_DIGEST_LENGTH );
		return;
	}
	
	MD5_Init( &ctx );
	MD5_Update( &ctx, (unsigned char *)inRSAPublicKey, strlen(inRSAPublicKey) );
	MD5_Final( pubKeyHash, &ctx );
	
	outHexHash[0] = 0;
	ConvertBinaryToHex( pubKeyHash, MD5_DIGEST_LENGTH, outHexHash );
}


//----------------------------------------------------------------------------------------------------
//	pwsf_ReplicaFilePath
//----------------------------------------------------------------------------------------------------

char *pwsf_ReplicaFilePath( void )
{
	const char *altPathPrefix = getenv("PWSAltPathPrefix");
	if ( altPathPrefix != NULL )
	{
		char path[PATH_MAX];
		sprintf( path, "%s/%s", altPathPrefix, kPWReplicaFile );
		return strdup( path );
	}
	else
	{
		return strdup( kPWReplicaFile );
	}
	
	return NULL;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_GetStatusForReplicas
//----------------------------------------------------------------------------------------------------

CFDictionaryRef pwsf_GetStatusForReplicas( void )
{
	ReplicaFile *replicaFile = [[ReplicaFile alloc] init];
	CFDictionaryRef repDict;
	CFMutableDictionaryRef outputDict;
	unsigned long repIndex, repCount;
	
	repCount = [replicaFile replicaCount];
	outputDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( outputDict == NULL )
		return NULL;
	
	// parent
	repDict = (CFMutableDictionaryRef)[replicaFile getParent];
	if ( repDict == NULL ) {
		CFRelease( outputDict );
		return NULL;
	}
	pwsf_AppendReplicaStatus( replicaFile, repDict, outputDict );
	
	// replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		repDict = [replicaFile getReplica:repIndex];
		if ( repDict == NULL ) {
			CFRelease( outputDict );
			return NULL;
		}
		
		pwsf_AppendReplicaStatus( replicaFile, repDict, outputDict );
	}
	
	return (CFDictionaryRef)outputDict;
}


//----------------------------------------------------------------------------------------------------
//	pwsf_AppendReplicaStatus
//----------------------------------------------------------------------------------------------------

void pwsf_AppendReplicaStatus( ReplicaFile *inReplicaFile, CFDictionaryRef inDict, CFMutableDictionaryRef inOutDict )
{
	CFArrayRef ipArray;
	CFStringRef nameString;
	CFStringRef valueString = NULL;
	ReplicaStatus replicaStatus;
	CFIndex index, count;
	
	ipArray = [inReplicaFile getIPAddressesFromDict:inDict];
	if ( ipArray == NULL )
		return;
	
	replicaStatus = [inReplicaFile getReplicaStatus:inDict];
	if ( replicaStatus == kReplicaActive )
	{
		// not so fast, make sure it's syncing too
		CFDateRef lastSyncDate = NULL;
		CFDateRef lastFailedDate = NULL;
		
		bool hasLastSyncDate = ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaSyncDateKey), (const void **)&lastSyncDate ) && CFGetTypeID(lastSyncDate) == CFDateGetTypeID() );
		bool hasFailedSyncDate = ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaSyncAttemptKey), (const void **)&lastFailedDate ) && CFGetTypeID(lastFailedDate) == CFDateGetTypeID() );
		if ( hasFailedSyncDate && hasLastSyncDate )
		{
			if ( CFDateCompare(lastFailedDate, lastSyncDate, NULL) == kCFCompareGreaterThan )
			{
				// last sync was not successful but previous sessions were successful
				valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: The most recent replication failed.", kCFStringEncodingUTF8 );
			}
		}
		else
		if ( hasFailedSyncDate && (!hasLastSyncDate) )
		{
			// has failed all attempts
			valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: Replication has failed all attempts.", kCFStringEncodingUTF8 );
		}
		else if ( (!hasFailedSyncDate) && (!hasLastSyncDate) )
		{
			// has not completed a session yet
			valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: Replication has not completed yet.", kCFStringEncodingUTF8 );
		}
	}
	
	// if no special string, report status.
	if ( valueString == NULL )
		valueString = pwsf_GetReplicaStatusString( replicaStatus );
	
	count = CFArrayGetCount( ipArray );
	for ( index = 0; index < count; index++ )
	{
		nameString = (CFStringRef) CFArrayGetValueAtIndex( ipArray, index );
		if ( nameString == NULL )
			continue;
		
		CFDictionaryAddValue( inOutDict, nameString, valueString );
	}
	
	// add DNS if we have it
	if ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaDNSKey), (const void **)&nameString ) )
		CFDictionaryAddValue( inOutDict, nameString, valueString );
	
	// clean up
	CFRelease( valueString );
	CFRelease( ipArray );
}


//----------------------------------------------------------------------------------------------------
//	Class: ReplicaFile
//----------------------------------------------------------------------------------------------------

@implementation ReplicaFile

-(id)init
{
	char *replicaFilePath = pwsf_ReplicaFilePath();
	const char *pathToUse = replicaFilePath ? replicaFilePath : kPWReplicaFile;
	
	if ( (self = [super init]) != nil ) {
		[self loadXMLData:pathToUse];
		if ( replicaFilePath != NULL )
			free( replicaFilePath );
		
		[self initCommon];
	}
	
	return self;
}

-(id)initWithPList:(CFDictionaryRef)xmlPList
{
	CFDataRef xmlData = NULL;
	CFStringRef errorString = NULL;
	
	if ( (self = [super init]) != nil )
	{
		mReplicaDict = NULL;
		
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)xmlPList );
		if ( xmlData != NULL )
		{
			mReplicaDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData,
												kCFPropertyListMutableContainersAndLeaves, &errorString );
			CFRelease( xmlData );
		}
		
		[self initCommon];
	}
	
	return self;
}

-(id)initWithContentsOfFile:(const char *)filePath
{
	if ( (self = [super init]) != nil ) {
		[self loadXMLData:filePath];
		[self initCommon];
	}
	
	return self;
}

-(id)initWithXMLStr:(const char *)xmlStr
{
	CFDataRef xmlData;
	CFStringRef errorString;

	if ( (self = [super init]) != nil ) {
		[self lock];

		if ( xmlStr != NULL ) {
			xmlData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)xmlStr, strlen(xmlStr) );
			if ( xmlData != NULL )  {
				mReplicaDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData,
								kCFPropertyListMutableContainersAndLeaves, &errorString );
				CFRelease( xmlData );
			}
		}
				
		[self initCommon];
		[self unlock];
	}
	
	return self;	
}


-(void)initCommon
{
	mRunningAsParent = YES;
	
	if ( mReplicaDict == NULL ) {
		// make a new replication dictionary
		mReplicaDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
						&kCFTypeDictionaryValueCallBacks );
	}
	
	[self lock];
	if ( [self isOldFormat] )
		[self updateFormat];
	[self unlock];
}


-free
{
	[self lock];
	if ( mReplicaDict != NULL )
		CFRelease( mReplicaDict );
	if ( mSelfName != NULL )
		CFRelease( mSelfName );
	if ( mFlatReplicaArray != NULL )
		CFRelease( mFlatReplicaArray );
	[self unlock];
	
	return [super free];
}


-(PWSReplicaEntry *)snapshotOfReplicasForServer:(CFDictionaryRef)serverDict;
{
	CFDictionaryRef parentDict = NULL;
	CFMutableDictionaryRef replicaDict = NULL;
	CFArrayRef serverDictReplicas = NULL;
	CFIndex repIndex = 0;
	CFIndex repCount = 0;
	CFIndex snapshotTotal = 0;
	PWSReplicaEntry *entArray = NULL;
	int index = 0;
	
	if ( serverDict == NULL )
		return NULL;
	
	[self lock];
	
	// include parent
	parentDict = [self getParentOfReplica:serverDict];
	if ( parentDict != NULL )
	{
		snapshotTotal++;
	
		// include peers
		serverDictReplicas = CFDictionaryGetValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
		if ( serverDictReplicas != NULL )
			snapshotTotal += CFArrayGetCount( serverDictReplicas );
	}
	
	// include children
	serverDictReplicas = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaReplicaKey) );
	if ( serverDictReplicas != NULL )
		snapshotTotal += CFArrayGetCount( serverDictReplicas );
	
	// allocate a flat array with a zero element on the end
	entArray = (PWSReplicaEntry *) calloc( sizeof(PWSReplicaEntry), snapshotTotal + 1 );
	if ( entArray == NULL )
		return NULL;
	
	// parent first
	if ( parentDict != NULL )
		[self serverStruct:&(entArray[index++]) forServerDict:parentDict];
	
	// children second
	// we want one-level, not subtree, so fetch the array instead
	// of calling replicaCount.
	serverDictReplicas = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaReplicaKey) );
	if ( serverDictReplicas != NULL )
	{
		repCount = CFArrayGetCount( serverDictReplicas );
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			replicaDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( serverDictReplicas, repIndex );
			if ( replicaDict != NULL )
				[self serverStruct:&(entArray[index++]) forServerDict:replicaDict];
		}
	}

	// peers third
	// we want one-level, not subtree, so fetch the array instead
	// of calling replicaCount.
	if ( parentDict != NULL )
	{
		serverDictReplicas = CFDictionaryGetValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
		if ( serverDictReplicas != NULL )
		{
			repCount = CFArrayGetCount( serverDictReplicas );
			for ( repIndex = 0; repIndex < repCount; repIndex++ )
			{
				replicaDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( serverDictReplicas, repIndex );
				if ( replicaDict != NULL && replicaDict != serverDict ) {
					[self serverStruct:&(entArray[index]) forServerDict:replicaDict];
					entArray[index++].peer = 1;
				}
			}
		}
	}
	
	[self unlock];
	
	return entArray;
}


-(void)serverStruct:(PWSReplicaEntry *)sEnt forServerDict:(CFDictionaryRef)serverDict
{
	CFMutableArrayRef ipArray = NULL;
	CFStringRef aString = NULL;
	CFDateRef aDateRef = NULL;
	CFNumberRef aNumberRef = NULL;
	SInt64 aNumber = 0;
	CFIndex ipIndex, ipCount;
	struct tm aTimeStruct;
	char aStr[128];
	
	// get ip address list
	ipArray = [self getIPAddressesFromDict:serverDict];
	if ( ipArray == NULL )
		return;
	
	ipCount = CFArrayGetCount( ipArray );
	for ( ipIndex = 0; ipIndex < ipCount; ipIndex++ )
	{
		aString = CFArrayGetValueAtIndex( ipArray, ipIndex );
		if ( aString != NULL &&
			 CFStringGetCString(aString, aStr, sizeof(aStr), kCFStringEncodingUTF8) )
		{
			strlcpy(sEnt->ip[sEnt->ipCount], aStr, sizeof(sEnt->ip[0]));
			(sEnt->ipCount)++;
		}
	}
	
	aString = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaDNSKey) );
	if ( aString != NULL &&
		 CFStringGetCString(aString, aStr, sizeof(aStr), kCFStringEncodingUTF8) )
	{
		strlcpy(sEnt->dns, aStr, sizeof(sEnt->dns));
	}
	
	aString = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaNameKey) );
	if ( aString != NULL &&
		 CFStringGetCString(aString, aStr, sizeof(aStr), kCFStringEncodingUTF8) )
	{
		strlcpy(sEnt->name, aStr, sizeof(sEnt->name));
	}
	
	sEnt->syncPolicy = [self getReplicaSyncPolicy:serverDict];
	sEnt->status = [self getReplicaStatus:serverDict];
	
	[self getIDRangeStart:&sEnt->idRangeBegin end:&sEnt->idRangeEnd forReplica:serverDict];
	
	aNumberRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaSyncTIDKey) );
	if ( aNumberRef != NULL && CFNumberGetValue(aNumberRef, kCFNumberSInt64Type, &aNumber) )
		sEnt->lastSyncTID = aNumber;
	
	aDateRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaSyncDateKey) );
	if ( pwsf_ConvertCFDateToBSDTime(aDateRef, &aTimeStruct) )
		sEnt->lastSyncDate = timegm(&aTimeStruct);
	
	aDateRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaSyncAttemptKey) );
	if ( pwsf_ConvertCFDateToBSDTime(aDateRef, &aTimeStruct) )
		sEnt->lastSyncFailedAttempt = timegm(&aTimeStruct);
	
	aDateRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaIncompletePullKey) );
	if ( pwsf_ConvertCFDateToBSDTime(aDateRef, &aTimeStruct) )
		sEnt->pullIncompleteDate = timegm(&aTimeStruct);
	
	aDateRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaPullDeferred) );
	if ( pwsf_ConvertCFDateToBSDTime(aDateRef, &aTimeStruct) )
		sEnt->pullDeferredDate = timegm(&aTimeStruct);
	
	aDateRef = CFDictionaryGetValue( serverDict, CFSTR(kPWReplicaEntryModDateKey) );
	if ( pwsf_ConvertCFDateToBSDTime(aDateRef, &aTimeStruct) )
		sEnt->entryModDate = timegm(&aTimeStruct);
		
	CFRelease( ipArray );
}

	
-(ReplicaChangeStatus)mergeReplicaList:(ReplicaFile *)inOtherList
{
	CFStringRef id1 = NULL;
	CFStringRef id2 = NULL;
	ReplicaChangeStatus changeStatus = kReplicaChangeNone;
	
	if ( mReplicaDict == nil || inOtherList == nil || [inOtherList replicaDict] == nil )
		return NO;
	
	[self lock];
	if ( CFDictionaryGetValueIfPresent(mReplicaDict, CFSTR(kPWReplicaIDKey), (const void **)&id1) &&
		 CFDictionaryGetValueIfPresent([inOtherList replicaDict], CFSTR(kPWReplicaIDKey), (const void **)&id2) &&
		 CFStringCompare(id1, id2, 0) != kCFCompareEqualTo )
	{
		[self unlock];
		return NO;
	}
	
	[self mergeReplicaListDecommissionedList:inOtherList changeStatus:&changeStatus];
	[self mergeReplicaListParentRecords:inOtherList changeStatus:&changeStatus];
	[self mergeReplicaListReplicas:inOtherList changeStatus:&changeStatus];
	[self mergeReplicaListLegacyTigerReplicaList:inOtherList changeStatus:&changeStatus];
	
	[self unlock];
	
	return changeStatus;
}


-(void)mergeReplicaListDecommissionedList:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus
{
	CFIndex index = 0;
	CFIndex repCount = 0;
	CFMutableArrayRef decomArray1 = NULL;
	CFStringRef replicaNameString = NULL;
	char replicaName[256] = {0,};
	
	// merge decommissioned lists
	decomArray1 = [inOtherList getArrayForKey:CFSTR(kPWReplicaDecommissionedListKey)];
	if ( decomArray1 != NULL )
	{
		repCount = CFArrayGetCount( decomArray1 );
		for ( index = 0; index < repCount; index++ )
		{
			replicaNameString = (CFStringRef) CFArrayGetValueAtIndex( decomArray1, index );
			if ( replicaNameString != NULL )
				if ( CFStringGetCString(replicaNameString, replicaName, sizeof(replicaName), kCFStringEncodingUTF8) )
					if ( [self decommissionReplica:replicaNameString] )
						*inOutChangeStatus |= kReplicaChangeGeneral;
		}
	}
}


-(void)mergeReplicaListParentRecords:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus
{
	CFMutableDictionaryRef otherRepDict = NULL;
	CFMutableDictionaryRef ourRepDict = NULL;
	
	// merge parent records
	otherRepDict = (CFMutableDictionaryRef)[inOtherList getParent];
	if ( otherRepDict != NULL )
	{
		ourRepDict = (CFMutableDictionaryRef)[self getParent];
		if ( ourRepDict == nil )
		{
			[self setParentWithDict:otherRepDict];
			*inOutChangeStatus |= kReplicaChangeGeneral;
		}
		else
		{
			if ( [self needsMergeFrom:otherRepDict to:ourRepDict] )
			{
				if ( [self mergeReplicaValuesFrom:otherRepDict to:ourRepDict parent:YES] )
					*inOutChangeStatus |= kReplicaChangeInterface;
				*inOutChangeStatus |= kReplicaChangeGeneral;
			}
		}
	}
}


-(void)mergeReplicaListReplicas:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus
{
	CFIndex index = 0;
	CFIndex repCount = 0;
	CFMutableDictionaryRef otherRepDict = NULL;
	CFMutableDictionaryRef ourRepDict = NULL;
	CFStringRef replicaNameString = NULL;

	repCount = [inOtherList replicaCount];
	for ( index = 0; index < repCount; index++ )
	{
		otherRepDict = (CFMutableDictionaryRef)[inOtherList getReplica:index];
		if ( otherRepDict == nil )
			continue;
		
		replicaNameString = [inOtherList getNameOfReplica:otherRepDict];
		ourRepDict = [self getReplicaByName:replicaNameString];
		if ( ourRepDict == NULL )
		{
			// add
			if ( [self replicaIsNotDecommissioned:replicaNameString] && (! [self replicaHasBeenPromotedToMaster:otherRepDict]) )
			{
				CFStringRef parentNameString = NULL;
				CFMutableDictionaryRef ourRelativeParentDict = NULL;
				CFMutableDictionaryRef otherParentDict = [inOtherList getParentOfReplica:otherRepDict];
				if ( otherParentDict != NULL ) {
					parentNameString = CFDictionaryGetValue( otherParentDict, CFSTR(kPWReplicaNameKey) );
					if ( parentNameString != NULL )
						ourRelativeParentDict = [self getReplicaByName:parentNameString];
				}
				if ( ourRelativeParentDict == NULL )
					ourRelativeParentDict = (CFMutableDictionaryRef)[self getParent];
				
				[self addReplica:otherRepDict withParent:ourRelativeParentDict];
				*inOutChangeStatus |= kReplicaChangeGeneral;
			}
		}
		else
		{
			// update
			if ( [self needsMergeFrom:otherRepDict to:ourRepDict] )
			{
				if ( [self mergeReplicaValuesFrom:otherRepDict to:ourRepDict parent:NO] )
					*inOutChangeStatus |= kReplicaChangeInterface;
				*inOutChangeStatus |= kReplicaChangeGeneral;
			}
		}
	}
}


-(void)mergeReplicaListLegacyTigerReplicaList:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus
{
	CFIndex index = 0;
	CFIndex index2 = 0;
	CFIndex repCount = 0;
	CFIndex repCount2 = 0;
	CFMutableArrayRef tigerReplicaArrayOurs = NULL;
	CFMutableArrayRef tigerReplicaArrayTheirs = NULL;
	CFMutableDictionaryRef otherRepDict = NULL;
	CFMutableDictionaryRef ourRepDict = NULL;
	CFStringRef replicaNameString = NULL;
	CFStringRef replicaNameString2 = NULL;

	// merge legacy Tiger replicas
	tigerReplicaArrayOurs = [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)];
	tigerReplicaArrayTheirs = [inOtherList getArrayForKey:CFSTR(kPWReplicaReplicaKey)];
	if ( tigerReplicaArrayOurs != NULL && tigerReplicaArrayTheirs != NULL )
	{
		repCount = CFArrayGetCount( tigerReplicaArrayTheirs );
		for ( index = 0; index < repCount; index++ )
		{
			otherRepDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( tigerReplicaArrayTheirs, index );
			replicaNameString = [inOtherList getNameOfReplica:otherRepDict];
			
			// need to get the Tiger copy
			repCount2 = CFArrayGetCount( tigerReplicaArrayOurs );
			for ( index2 = 0; index2 < repCount2; index2++ )
			{
				ourRepDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( tigerReplicaArrayOurs, index2 );
				if ( ourRepDict != NULL )
					replicaNameString2 = CFDictionaryGetValue( ourRepDict, CFSTR(kPWReplicaNameKey) );
				if ( replicaNameString2 != NULL && CFStringCompare(replicaNameString, replicaNameString2, 0) == kCFCompareEqualTo )
					break;
				
				ourRepDict = NULL;
			}
			
			if ( ourRepDict == NULL )
			{
				// add
				if ( [self replicaIsNotDecommissioned:replicaNameString] && (! [self replicaHasBeenPromotedToMaster:otherRepDict]) )
				{
					CFArrayAppendValue( tigerReplicaArrayOurs, otherRepDict );
					*inOutChangeStatus |= kReplicaChangeGeneral;
				}
			}
			else
			{
				// update
				if ( [self needsMergeFrom:otherRepDict to:ourRepDict] )
				{
					if ( [self mergeReplicaValuesFrom:otherRepDict to:ourRepDict parent:NO] )
						*inOutChangeStatus |= kReplicaChangeInterface;
					*inOutChangeStatus |= kReplicaChangeGeneral;
				}
			}
		}
	}
}


// Returns: YES if an interface change is made
-(BOOL)mergeReplicaValuesFrom:(CFMutableDictionaryRef)dict1 to:(CFMutableDictionaryRef)dict2 parent:(BOOL)isParent;
{
	CFStringRef idRangeBegin;
	CFStringRef idRangeEnd;
	CFTypeRef ip = NULL;
	CFStringRef dns = NULL;
	CFStringRef replicaPolicy = NULL;
	CFDateRef modDate = NULL;
	BOOL ipChange = NO;
	
	if ( dict1 == NULL || dict2 == NULL )
		return NO;
	
	// get 'em
	idRangeBegin = (CFStringRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaIDRangeBeginKey) );
	idRangeEnd = (CFStringRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaIDRangeEndKey) );
	if ( isParent )
		replicaPolicy = (CFStringRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaPolicyKey) );
	
	if ( !isParent || !mRunningAsParent )
	{
		ip = (CFTypeRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaIPKey) );
		dns = (CFStringRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaDNSKey) );
	}
	modDate = (CFDateRef) CFDictionaryGetValue( dict1, CFSTR(kPWReplicaEntryModDateKey) );
	
	// set 'em
	if ( idRangeBegin != NULL )
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaIDRangeBeginKey), idRangeBegin );
	if ( idRangeEnd != NULL )
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaIDRangeEndKey), idRangeEnd );
	if ( ip != NULL ) {
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaIPKey), ip );
		ipChange = YES;
	}
	if ( dns != NULL )
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaDNSKey), dns );
	if ( replicaPolicy != NULL )
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaPolicyKey), replicaPolicy );
	if ( modDate != NULL )
		CFDictionarySetValue( dict2, CFSTR(kPWReplicaEntryModDateKey), modDate );
	
	// note that we don't update the lastSyncDate key because we want to keep *our* last sync date
	
	return ipChange;
}


-(BOOL)needsMergeFrom:(CFMutableDictionaryRef)dict1 to:(CFMutableDictionaryRef)dict2
{
	CFDateRef sinceNeverReferenceDate = NULL;
	CFDateRef lastSyncDate = NULL;
	CFDateRef targetLastSyncDate = NULL;
	bool needsMerge = false;
	
	if ( dict1 == NULL || dict2 == NULL )
		return false;
	
	sinceNeverReferenceDate = CFDateCreate( kCFAllocatorDefault, 0 );
	
	// get the last sync dates
	if ( CFDictionaryGetValueIfPresent( dict1, CFSTR(kPWReplicaEntryModDateKey), (const void **)&lastSyncDate ) )
	{
		if ( CFGetTypeID(lastSyncDate) != CFDateGetTypeID() )
			lastSyncDate = sinceNeverReferenceDate;
	}
	else
	{
		lastSyncDate = sinceNeverReferenceDate;
	}
				
	if ( CFDictionaryGetValueIfPresent( dict2, CFSTR(kPWReplicaEntryModDateKey), (const void **)&targetLastSyncDate ) )
	{
		if ( CFGetTypeID(targetLastSyncDate) != CFDateGetTypeID() )
		{
			// this is the dict we're keeping so fix it
			CFDictionaryRemoveValue( dict2, CFSTR(kPWReplicaEntryModDateKey) );
			targetLastSyncDate = sinceNeverReferenceDate;
		}
	}
	else
	{
		targetLastSyncDate = sinceNeverReferenceDate;
	}
	
	needsMerge = ( targetLastSyncDate != NULL && lastSyncDate != NULL &&
			 CFDateCompare( targetLastSyncDate, lastSyncDate, NULL ) == kCFCompareLessThan );
	
	if ( sinceNeverReferenceDate != NULL )
		CFRelease( sinceNeverReferenceDate );
		
	return needsMerge;
}


-(CFMutableDictionaryRef)getParentOfReplica:(CFDictionaryRef)replicaDict
{
	CFArrayRef replicaArray = NULL;
	CFMutableDictionaryRef parentDict = (CFMutableDictionaryRef)[self getParent];
	CFMutableDictionaryRef curReplicaDict = NULL;
	CFStringRef targetNameString = NULL;
	CFIndex index, repCount;
	
	if ( replicaDict == NULL || replicaDict == parentDict )
		return NULL;
	
	targetNameString = CFDictionaryGetValue( replicaDict, CFSTR(kPWReplicaNameKey) );
	if ( targetNameString == NULL )
		return NULL;
	
	// check top-level parent
	replicaArray = CFDictionaryGetValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
	if ( [self array:replicaArray containsReplicaWithName:targetNameString] )
		return parentDict;
	
	// dive
	repCount = [self replicaCount];
	for ( index = 0; index < repCount; index++ )
	{
		curReplicaDict = (CFMutableDictionaryRef)[self getReplica:index];
		if ( curReplicaDict != NULL )
		{
			replicaArray = CFDictionaryGetValue( curReplicaDict, CFSTR(kPWReplicaReplicaKey) );
			if ( [self array:replicaArray containsReplicaWithName:targetNameString] )
				return curReplicaDict;
		}
	}
	
	return NULL;
}


-(BOOL)array:(CFArrayRef)replicaArray containsReplicaWithName:(CFStringRef)targetNameString
{
	CFMutableDictionaryRef arrayItem = NULL;
	CFStringRef replicaNameString = NULL;
	CFIndex index, repCount;
		
	if ( replicaArray != NULL )
	{
		repCount = CFArrayGetCount( replicaArray );
		for ( index = 0; index < repCount; index++ )
		{
			arrayItem = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( replicaArray, index );
			if ( arrayItem != NULL )
			{
				replicaNameString = CFDictionaryGetValue( arrayItem, CFSTR(kPWReplicaNameKey) );
				if ( CFStringCompare(replicaNameString, targetNameString, 0) == kCFCompareEqualTo )
				{
					// found it
					return YES;
				}
			}
		}
	}
	
	return NO;
}


// traps for overrides
-(void)lock
{
}

-(void)unlock
{
}


// top level
-(ReplicaPolicy)getReplicaPolicy
{
	ReplicaPolicy result = kReplicaNone;
	char statusStr[256];
	
	if ( mReplicaDict == NULL )
		return kReplicaNone;
	
	// key is "Status"
	if ( ![self getCStringFromDictionary:mReplicaDict forKey:CFSTR("Status") maxLen:sizeof(statusStr) result:statusStr] )
		return kReplicaNone;
	
	if ( strcasecmp( statusStr, kPWReplicaStatusAllow ) == 0 )
		result = kReplicaAllowAll;
	else
	if ( strcasecmp( statusStr, kPWReplicaStatusUseACL ) == 0 )
		result = kReplicaUseACL;
	
	return result;
}


-(void)setReplicaPolicy:(ReplicaPolicy)inPolicy
{
	char *valueStr = NULL;
	CFStringRef valString;
	
	if ( mReplicaDict == NULL )
		return;
	
	switch( inPolicy )
	{
		case kReplicaNone:
			valueStr = "None";
			break;
			
		case kReplicaAllowAll:
			valueStr = kPWReplicaStatusAllow;
			break;
			
		case kReplicaUseACL:
			valueStr = kPWReplicaStatusUseACL;
			break;
	}
	
	if ( valueStr != NULL )
	{
		valString = CFStringCreateWithCString( kCFAllocatorDefault, valueStr, kCFStringEncodingUTF8 );
		if ( valString != NULL )
		{
			CFDictionarySetValue( mReplicaDict, CFSTR("Status"), valString );
			CFRelease( valString );
		}
	}
}


-(void)emptyFlatReplicaArray
{
	[self lock];
	if ( mFlatReplicaArray != NULL )
		CFArrayRemoveAllValues( mFlatReplicaArray );
	else
		mFlatReplicaArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	[self unlock];
}


-(unsigned long)replicaCount
{
	[self emptyFlatReplicaArray];
	
	return [self replicaCount:[self getParent]];
}


-(unsigned long)replicaCount:(CFDictionaryRef)inReplicaDict
{
	CFArrayRef replicaArray = NULL;
	CFDictionaryRef arrayItem = NULL;
	CFIndex index, repCount;
	unsigned long result = 0;
	
	if ( inReplicaDict == NULL )
		return 0;
	
	replicaArray = CFDictionaryGetValue( inReplicaDict, CFSTR(kPWReplicaReplicaKey) );
	if ( replicaArray != NULL )
	{
		repCount = CFArrayGetCount( replicaArray );
		for ( index = 0; index < repCount; index++ )
		{
			arrayItem = CFArrayGetValueAtIndex( replicaArray, index );
			if ( arrayItem != NULL )
			{
				CFArrayAppendValue( mFlatReplicaArray, arrayItem );
				result++;
				
				// add the children
				result += [self replicaCount:arrayItem];
			}
		}
	}
	
	return result;
}


-(CFDictionaryRef)getReplica:(unsigned long)index
{
	CFDictionaryRef result = NULL;
	
	[self lock];
	if ( mFlatReplicaArray != NULL )
		result = (CFDictionaryRef)CFArrayGetValueAtIndex( mFlatReplicaArray, index );
	[self unlock];
	
	return result;
}


-(BOOL)isActive
{
	if ( CFDictionaryContainsKey(mReplicaDict, CFSTR("Decommission")) )
		return NO;
	
	return YES;
}


-(CFStringRef)getUniqueID
{
	CFStringRef idString = NULL;
	
	if ( mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR(kPWReplicaIDKey), (const void **)&idString ) )
		return NULL;
	
	if ( CFGetTypeID(idString) != CFStringGetTypeID() )
		return NULL;
	
	return CFRetain( idString );
}


-(CFStringRef)currentServerForLDAP
{
	CFStringRef serverString = NULL;
	
	if ( mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR(kPWReplicaCurrentServerForLDAPKey), (const void **)&serverString ) )
		return NULL;
	
	if ( CFGetTypeID(serverString) != CFStringGetTypeID() )
		return NULL;
	
	CFRetain( serverString );
	return serverString;
}


-(CFDictionaryRef)getParent
{
	CFDictionaryRef parentDict = NULL;

	[self lock];
	if ( mReplicaDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR(kPWReplicaParentKey), (const void **)&parentDict ) )
		{
			if ( CFGetTypeID(parentDict) != CFDictionaryGetTypeID() )
				parentDict = NULL;
		}
	}
	[self unlock];
	
	return parentDict;
}


-(CFStringRef)xmlString
{
	CFStringRef returnString = NULL;
	CFDataRef xmlData = NULL;
	const UInt8 *sourcePtr;
	long length;
	
	[self lock];
	if ( mReplicaDict != NULL )
	{
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)mReplicaDict );
		if ( xmlData != NULL )
		{
			sourcePtr = CFDataGetBytePtr( xmlData );
			length = CFDataGetLength( xmlData );
			if ( sourcePtr != NULL && length > 0 )
				returnString = CFStringCreateWithBytes( kCFAllocatorDefault, sourcePtr, length, kCFStringEncodingUTF8, NO );
			
			CFRelease( xmlData );
		}
	}
	[self unlock];
	
	return returnString;
}


-(BOOL)fileHasChanged
{
	BOOL refresh = NO;
	struct timespec modDate;
	char *replicaFilePath = pwsf_ReplicaFilePath();
	const char *pathToUse = replicaFilePath ? replicaFilePath : kPWReplicaFile;
	
	refresh = ( [self statReplicaFile:pathToUse andGetModDate:&modDate] != 0 );
	if ( replicaFilePath != NULL )
		free( replicaFilePath );
	
	if ( !refresh && modDate.tv_sec > mReplicaFileModDate.tv_sec )
		refresh = YES;
		
	if ( !refresh && modDate.tv_sec == mReplicaFileModDate.tv_sec && modDate.tv_nsec > mReplicaFileModDate.tv_nsec )
		refresh = YES;
	
	return refresh;
}


-(void)refreshIfNeeded
{
	BOOL refresh = NO;
	CFMutableDictionaryRef lastPropertyList = mReplicaDict;
	
	refresh = [self fileHasChanged];
	
	if ( refresh && mDirty )
	{
		// need to resolve conflict between file and memory copies
		// SaveXMLData handles merging
		[self saveXMLData];
	}
	else
	{
		if ( refresh )
		{
			char *replicaFilePath = pwsf_ReplicaFilePath();
			const char *pathToUse = replicaFilePath ? replicaFilePath : kPWReplicaFile;

			if ( [self loadXMLData:pathToUse] == 0 )
			{
				if ( lastPropertyList != NULL )
					CFRelease( lastPropertyList );
			}
			else
			{
				mReplicaDict = lastPropertyList;
			}
			
			if ( replicaFilePath != NULL )
				free( replicaFilePath );
		}
		else
		if ( mDirty )
			[self saveXMLData];
	}
}


-(CFStringRef)calcServerUniqueID:(const char *)inRSAPublicKey
{
	char pubKeyHashHex[MD5_DIGEST_LENGTH*2 + 1];
	
	pwsf_CalcServerUniqueID( inRSAPublicKey, pubKeyHashHex );	
	return CFStringCreateWithCString( kCFAllocatorDefault, pubKeyHashHex, kCFStringEncodingUTF8 );
}


-(void)addServerUniqueID:(const char *)inRSAPublicKey
{
	CFStringRef idString;
	
	if ( inRSAPublicKey == NULL || mReplicaDict == NULL )
		return;
	
	// the ID never changes, so return if present
	if ( CFDictionaryContainsKey( mReplicaDict, CFSTR(kPWReplicaIDKey) ) )
		return;
		
	idString = [self calcServerUniqueID:inRSAPublicKey];
	if ( idString != NULL )
	{
		CFDictionaryAddValue( mReplicaDict, CFSTR(kPWReplicaIDKey), idString );
		mDirty = YES;
	}
}


-(void)setParentWithIP:(const char *)inIPStr andDNS:(const char *)inDNSStr
{
	CFMutableDictionaryRef parentData;
	CFStringRef ipString;
	CFStringRef dnsString = NULL;
	
	if ( inIPStr == NULL )
		return;
	
	parentData = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( parentData != NULL )
	{
		ipString = CFStringCreateWithCString( kCFAllocatorDefault, inIPStr, kCFStringEncodingUTF8 );
		if ( ipString != NULL ) {
			CFDictionaryAddValue( parentData, CFSTR(kPWReplicaIPKey), ipString );
			CFRelease( ipString );
			ipString = NULL;
		}
		
		if ( inDNSStr != NULL )
		{
			dnsString = CFStringCreateWithCString( kCFAllocatorDefault, inDNSStr, kCFStringEncodingUTF8 );
			if ( dnsString != NULL ) {
				CFDictionaryAddValue( parentData, CFSTR(kPWReplicaDNSKey), dnsString );
				CFRelease( dnsString );
				dnsString = NULL;
			}
		}
		
		[self setParentWithDict:parentData];
	}
}


-(void)setParentWithDict:(CFDictionaryRef)inParentData
{
	if ( inParentData == NULL )
		return;
	
	CFDictionarySetValue( mReplicaDict, CFSTR(kPWReplicaParentKey), inParentData );
	
	// need to have the parent inserted before calling AllocateIDRange()
	[self allocateIDRangeOfSize:500 forReplica:CFSTR("") minID:0];
	mDirty = YES;
}


-(CFMutableDictionaryRef)addReplicaWithIP:(const char *)inIPStr
	andDNS:(const char *)inDNSStr
	withParent:(CFMutableDictionaryRef)inParentDict
{
	CFMutableDictionaryRef replicaData = NULL;
	CFStringRef dnsString = NULL;
	
	if ( inIPStr == NULL )
		return NULL;
	
	replicaData = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( replicaData != NULL )
	{
		[self addIPAddress:inIPStr toReplica:replicaData];
		
		if ( inDNSStr != NULL )
			dnsString = CFStringCreateWithCString( kCFAllocatorDefault, inDNSStr, kCFStringEncodingUTF8 );
		if ( dnsString != NULL ) {
			CFDictionaryAddValue( replicaData, CFSTR(kPWReplicaDNSKey), dnsString );
			CFRelease( dnsString );
			dnsString = NULL;
		}
		
		[self addReplica:replicaData withParent:inParentDict];
	}
	
	return replicaData;
}


-(CFMutableDictionaryRef)addReplica:(CFMutableDictionaryRef)inReplicaData
	withParent:(CFMutableDictionaryRef)inParentDict
{
	CFMutableArrayRef replicaArray = NULL;
	CFStringRef ipString = NULL;
	CFStringRef nameString = NULL;
	
	if ( inReplicaData == NULL || inParentDict == NULL || mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaIPKey), (const void **)&ipString ) )
		return NULL;
	if ( ! CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaNameKey), (const void **)&nameString ) )
		nameString = NULL;
	
	replicaArray = (CFMutableArrayRef) CFDictionaryGetValue( inParentDict, CFSTR(kPWReplicaReplicaKey) );
	if ( replicaArray != NULL && nameString != NULL )
	{
		// don't add duplicates
		
		CFIndex repIndex, repCount;
		CFDictionaryRef replicaRef;
		CFStringRef replicaNameString;
		
		repCount = CFArrayGetCount( replicaArray );
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			replicaRef = [self getReplica:repIndex];
			if ( replicaRef == NULL )
				continue;
			
			if ( CFDictionaryGetValueIfPresent( replicaRef, CFSTR(kPWReplicaNameKey), (const void **)&replicaNameString ) )
			{
				if ( CFStringCompare( nameString, replicaNameString, (CFOptionFlags)0 ) == kCFCompareEqualTo )
				{
					// remove the old copy and refresh
					CFArrayRemoveValueAtIndex( replicaArray, repIndex );
					break;
				}
			}
		}
	}
	
	// has this replica been named?
	if ( nameString == NULL )
	{
		// add the Replica Name
		nameString = [self getNextReplicaName];
		if ( nameString != NULL )
			CFDictionaryAddValue( inReplicaData, CFSTR(kPWReplicaNameKey), nameString );
	}
	
	// add to the list of replicas
	if ( replicaArray == NULL )
	{
		replicaArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( replicaArray == NULL )
			return NULL;
			
		CFDictionaryAddValue( inParentDict, CFSTR(kPWReplicaReplicaKey), replicaArray );
		CFRelease( replicaArray );
	}
	CFArrayAppendValue( replicaArray, inReplicaData );
	
	// add to the old list of replicas for Tiger clients
	[self addReplicaToLegacyTigerList:inReplicaData];
	
	mDirty = YES;
	
	// add an ID range
	[self allocateIDRangeOfSize:500 forReplica:nameString minID:0];
	
	return inReplicaData;
}

-(void)addReplicaToLegacyTigerList:(CFMutableDictionaryRef)inReplicaData
{
	CFMutableArrayRef tigerReplicaArray = NULL;
	CFMutableDictionaryRef replicaDataCopy = NULL;
	
	tigerReplicaArray = [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)];
	if ( tigerReplicaArray == NULL )
	{
		tigerReplicaArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( tigerReplicaArray != NULL ) {
			CFDictionaryAddValue( mReplicaDict, CFSTR(kPWReplicaReplicaKey), tigerReplicaArray );
			CFRelease( tigerReplicaArray );
		}
	}
	if ( tigerReplicaArray != NULL )
	{
		// make a copy and remove the "Replicas" key from the entry
		replicaDataCopy = (CFMutableDictionaryRef)CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, inReplicaData );
		if ( replicaDataCopy != NULL )
		{
			CFDictionaryRemoveValue( replicaDataCopy, CFSTR(kPWReplicaReplicaKey) );
			CFArrayAppendValue( tigerReplicaArray, replicaDataCopy );
			CFRelease( replicaDataCopy );
		}
	}
	
	mDirty = YES;
}


-(BOOL)addIPAddress:(const char *)inIPStr toReplica:(CFMutableDictionaryRef)inReplicaDict
{
	CFTypeRef valueRef;
	CFStringRef ipString;
	CFMutableArrayRef arrayRef;
	BOOL result = NO;
	
	if ( inReplicaDict == NULL || inIPStr == NULL )
		return result;
	
	ipString = CFStringCreateWithCString( kCFAllocatorDefault, inIPStr, kCFStringEncodingUTF8 );
	if ( ipString == NULL )
		return result;
	
	if ( CFDictionaryGetValueIfPresent( inReplicaDict, CFSTR(kPWReplicaIPKey), &valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
		{
			arrayRef = (CFMutableArrayRef) valueRef;
			// Note: header says range should be (0,N-1), but reality is (0,N).
			if ( ! CFArrayContainsValue( arrayRef, CFRangeMake(0, CFArrayGetCount(arrayRef)), ipString ) )
			{
				CFArrayAppendValue( arrayRef, ipString );
				result = YES;
			}
		}
		else
		if ( CFGetTypeID(valueRef) == CFStringGetTypeID() &&
			 CFStringCompare( (CFStringRef)valueRef, ipString, 0 ) != kCFCompareEqualTo )
		{
			arrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( arrayRef != NULL )
			{
				CFArrayAppendValue( arrayRef, valueRef );
				CFArrayAppendValue( arrayRef, ipString );
				CFDictionaryReplaceValue( inReplicaDict, CFSTR(kPWReplicaIPKey), arrayRef );
				CFRelease( arrayRef );
				result = YES;
			}
		}
	}
	else
	{
		CFDictionaryAddValue( inReplicaDict, CFSTR(kPWReplicaIPKey), ipString );
		result = YES;
	}
	
	CFRelease( ipString );
		
	if ( result )
		[self setEntryModDateForReplica:inReplicaDict];
			
	return result;
}


-(void)addIPAddress:(const char *)inNewIPStr orReplaceIP:(const char *)inOldIPStr inReplica:(CFMutableDictionaryRef)inReplicaDict
{
	CFTypeRef valueRef;
	CFStringRef oldIPString;
	CFStringRef newIPString;
	CFMutableArrayRef arrayRef;
	CFIndex firstIndex;
	
	if ( inReplicaDict == NULL || inOldIPStr == NULL || inNewIPStr == NULL )
		return;
	
	oldIPString = CFStringCreateWithCString( kCFAllocatorDefault, inOldIPStr, kCFStringEncodingUTF8 );
	if ( oldIPString == NULL )
		return;
	
	if ( CFDictionaryGetValueIfPresent(inReplicaDict, CFSTR(kPWReplicaIPKey), &valueRef) )
	{
		if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
		{
			arrayRef = (CFMutableArrayRef) valueRef;
			// Note: header says range should be (0,N-1), but reality is (0,N).
			
			firstIndex = CFArrayGetFirstIndexOfValue( arrayRef, CFRangeMake(0, CFArrayGetCount(arrayRef)), oldIPString );
			if ( firstIndex != kCFNotFound )
				CFArrayRemoveValueAtIndex( arrayRef, firstIndex );
			
			[self addIPAddress:inNewIPStr toReplica:inReplicaDict];
		}
		else
		if ( CFGetTypeID(valueRef) == CFStringGetTypeID() )
		{
			if ( CFStringCompare( (CFStringRef)valueRef, oldIPString, 0 ) == kCFCompareEqualTo )
			{
				newIPString = CFStringCreateWithCString( kCFAllocatorDefault, inNewIPStr, kCFStringEncodingUTF8 );
				if ( newIPString == NULL )
					return;
				
				CFDictionaryReplaceValue( inReplicaDict, CFSTR(kPWReplicaIPKey), newIPString );
				CFRelease( newIPString );
				[self setEntryModDateForReplica:inReplicaDict];
			}
			else
			{
				[self addIPAddress:inNewIPStr toReplica:inReplicaDict];
			}
		}
	}
	else
	{
		[self addIPAddress:inNewIPStr toReplica:inReplicaDict];
	}
	
	CFRelease( oldIPString );
}


-(CFMutableArrayRef)getIPAddressesFromDict:(CFDictionaryRef)inReplicaDict
{
	CFTypeRef valueRef;
	CFMutableArrayRef arrayRef;
	
	if ( inReplicaDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( inReplicaDict, CFSTR(kPWReplicaIPKey), &valueRef ) )
		{
			if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
			{
				CFRetain( (CFMutableArrayRef) valueRef );
				return (CFMutableArrayRef) valueRef;
			}
			else
			if ( CFGetTypeID(valueRef) == CFStringGetTypeID() )
			{
				arrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				if ( arrayRef != NULL )
				{
					CFArrayAppendValue( arrayRef, valueRef );
					return arrayRef;
				}
			}
		}
	}
	
	return NULL;
}


-(int)saveXMLData
{
	char *replicaFilePath = pwsf_ReplicaFilePath();
	const char *pathToUse = replicaFilePath ? replicaFilePath : kPWReplicaFile;
	
	[self lock];
	int result = [self saveXMLDataToFile:pathToUse];
	if ( result == 0 )
	{
		[self statReplicaFile:pathToUse andGetModDate:&mReplicaFileModDate];
		mDirty = NO;
	}
	
	if ( replicaFilePath != NULL )
		free( replicaFilePath );
	[self unlock];
	
	return result;
}


-(int)saveXMLDataToFile:(const char *)inSaveFile
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFWriteStreamRef myWriteStreamRef = NULL;
	CFPropertyListRef localDictRef = NULL;
	CFStringRef errorString;
	int returnValue = -1;
	struct stat sb;
	int err;
	char *saveDir;
	char *slash;
	
	if ( mReplicaDict == NULL )
		return 0;
	
	[self lock];

	// ensure the directory exists
	saveDir = strdup( inSaveFile );
	if ( saveDir != NULL ) {
		slash = rindex( saveDir, '/' );
		if ( slash != NULL ) {
			*slash = '\0';
			if ( lstat(saveDir, &sb) != 0 ) {
				err = pwsf_mkdir_p( saveDir, 0600 );
				if ( err != 0 ) {
					free( saveDir );
					[self unlock];
					return err;
				}
			}
		}
		free( saveDir );
	}
	
	if ( [self fileHasChanged] )
	{
		// merge updates on disk
		// get the disk version
		ReplicaFile *onDiskReplicaFile = [[ReplicaFile alloc] init];
		if ( onDiskReplicaFile != nil )
		{
			if ( [onDiskReplicaFile isHappy] )
				[self mergeReplicaList:onDiskReplicaFile];
			[onDiskReplicaFile free];
		}
	}
	
	localDictRef = CFRetain( mReplicaDict ); 
	
	do
	{
		myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaFileTemp, kCFStringEncodingUTF8 );
		if ( myReplicaDataFilePathRef == NULL )
			break;
		
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myReplicaDataFilePathRef );
		
		if ( myReplicaDataFileRef == NULL )
			break;
		
		myWriteStreamRef = CFWriteStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
		
		CFRelease( myReplicaDataFileRef );
		
		if ( myWriteStreamRef == NULL )
			break;
		
		if ( CFWriteStreamOpen( myWriteStreamRef ) )
		{		
			errorString = NULL;
			CFPropertyListWriteToStream( localDictRef, myWriteStreamRef, kCFPropertyListXMLFormat_v1_0, &errorString );
			CFWriteStreamClose( myWriteStreamRef );
			
			// paranoia check
			if ( lstat(kPWReplicaFileTemp, &sb) == 0 && sb.st_size > 0 )
			{
				unlink( inSaveFile );
				rename( kPWReplicaFileTemp, inSaveFile );
			}
			else
			{
				unlink( kPWReplicaFileTemp );
			}
			
			if ( errorString != NULL ) {
				CFRelease( errorString );
				break;
			}
			
			returnValue = 0;
		}
	}
	while (0);
	
	if ( myWriteStreamRef != NULL )
		CFRelease( myWriteStreamRef );
	if ( localDictRef != NULL )
		CFRelease( localDictRef );
	
	[self unlock];
	
	return returnValue;
}


-(void)stripSyncDates
{
	unsigned long repIndex;
	unsigned long repCount = [self replicaCount];
	CFMutableDictionaryRef curReplica;
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	
	// strip parent
	curReplica = (CFMutableDictionaryRef)[self getParent];
	if ( curReplica != NULL )
	{
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncTIDKey) );
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncDateKey) );
		CFDictionarySetValue( curReplica, CFSTR(kPWReplicaEntryModDateKey), nowDate );
	}
	
	// strip replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = (CFMutableDictionaryRef)[self getReplica:repIndex];
		if ( curReplica == NULL )
			continue;
		
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncTIDKey) );
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncDateKey) );
		CFDictionarySetValue( curReplica, CFSTR(kPWReplicaEntryModDateKey), nowDate );
	}
	
	if ( nowDate != NULL )
		CFRelease( nowDate );
	
	mDirty = YES;
}


//----------------------------------------------------------------------------------------------------
//	OldestSyncDate
//
//	Returns: date of the most distant sync session or NULL
//
//  If any valid replicas have not completed a sync session, the method returns NULL.
//	The caller should assume there is some data that has not propagated and act accordingly.
//----------------------------------------------------------------------------------------------------

-(CFDateRef)oldestSyncDate
{
	unsigned long repIndex;
	unsigned long repCount = [self replicaCount];
	CFMutableDictionaryRef curReplica = NULL;
	CFDateRef oldestSyncDate = NULL;
	CFDateRef replicaSyncDate = NULL;

	if ( ![self isHappy] )
		return NULL;
	
	// start with the parent
	curReplica = (CFMutableDictionaryRef)[self getParent];
	if ( curReplica != nil )
	{
		if ( [self getReplicaStatus:curReplica] != kReplicaPermissionDenied )
		{
			if ( CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncDateKey), (const void **)&replicaSyncDate) &&
				 CFGetTypeID(replicaSyncDate) == CFDateGetTypeID() )
				oldestSyncDate = replicaSyncDate;
			else
				return NULL;
		}
	}
	
	// check replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = (CFMutableDictionaryRef)[self getReplica:repIndex];
		if ( curReplica == nil )
			continue;
		
		if ( [self getReplicaStatus:curReplica] != kReplicaPermissionDenied )
		{
			replicaSyncDate = CFDictionaryGetValue( curReplica, CFSTR(kPWReplicaSyncDateKey) );
			
			if ( oldestSyncDate == NULL )
				oldestSyncDate = replicaSyncDate;
			else if ( replicaSyncDate != NULL && CFDateCompare(replicaSyncDate, oldestSyncDate, NULL) == kCFCompareLessThan )
				oldestSyncDate = replicaSyncDate;
		}
		else
		{
			return NULL;
		}
	}
	
	return oldestSyncDate;
}


//----------------------------------------------------------------------------------------------------
//	lowTIDForReplica:
//
//	Returns: ReplicaRole enum
//----------------------------------------------------------------------------------------------------

-(ReplicaRole)roleForReplica:(CFStringRef)replicaName
{
	ReplicaRole result = kReplicaRoleUnset;
	CFMutableDictionaryRef replicaDict = NULL;
	
	[self lock];
	
	// Check for top tier Parent
	if ( replicaName == NULL ||
		 CFStringGetLength(replicaName) == 0 ||
		 CFStringCompare(replicaName, CFSTR(kPWReplicaParentKey), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
	{
		result = kReplicaRoleParent;
	}
	else
	{
		// Check existence
		replicaDict = [self getReplicaByName:replicaName];
		if ( replicaDict == NULL )
		{
			result = kReplicaRoleUnset;
		}
		else
		{
			// Check Tier
			int tier = 1;
			CFMutableDictionaryRef parentOfRepDict = [self getParentOfReplica:replicaDict];
			if ( parentOfRepDict != [self getParent] )
				tier = 2;
			
			CFArrayRef childArray = (CFArrayRef) CFDictionaryGetValue( replicaDict, CFSTR(kPWReplicaReplicaKey) );
			CFRelease( replicaDict );
			replicaDict = NULL;
			
			if ( childArray == NULL || CFArrayGetCount(childArray) == 0 )
			{
				result = ((tier == 1) ? kReplicaRoleTierOneReplica : kReplicaRoleTierTwoReplica);
			}
			else
			{
				result = kReplicaRoleRelay;
			}
		}
	}
	
	if ( replicaDict != NULL )
		CFRelease( replicaDict );
	
	[self unlock];
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	lowTIDForReplica:
//
//	Returns: the lowest TID for any replica that syncs with the one provided
//
//	Master		Syncs with first tier replicas and relays
//	Relay		Syncs with the Master and its replica children
//	Replica		Syncs with its parent (the master or a relay)
//----------------------------------------------------------------------------------------------------

-(SInt64)lowTIDForReplica:(CFStringRef)replicaName
{
	unsigned long repIndex = 0;
	unsigned long repCount = 0;
	CFMutableDictionaryRef curReplica = NULL;
	CFMutableDictionaryRef replicaDict = NULL;
	CFMutableDictionaryRef parentDict = NULL;
	CFNumberRef tidNumber = NULL;
	SInt64 tid = 0;
	SInt64 tidLow = 0;
	BOOL tidLowSet = NO;
	
	[self lock];
	
	switch ( [self roleForReplica:replicaName] )
	{
		case kReplicaRoleUnset:
			// nothing to count
			[self unlock];
			return 0;
			break;
		
		case kReplicaRoleParent:
			// top-level, nothing above
			replicaDict = (CFMutableDictionaryRef)[self getParent];
			CFRetain( replicaDict );
			break;
		
		case kReplicaRoleRelay:
		case kReplicaRoleTierOneReplica:
		case kReplicaRoleTierTwoReplica:
			replicaDict = [self getReplicaByName:replicaName];
			if ( replicaDict != NULL )
				parentDict = (CFMutableDictionaryRef)[self getParentOfReplica:replicaDict];
			break;
	}
	
	// get the tid for the parent
	if ( parentDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent(parentDict, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
				CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
				CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
		{
			tidLow = tid;
			tidLowSet = YES;
		}
	}
	
	// children
	if ( replicaDict != NULL )
	{
		CFArrayRef childArray = (CFArrayRef) CFDictionaryGetValue( replicaDict, CFSTR(kPWReplicaReplicaKey) );
		if ( childArray != NULL )
		{
			repCount = CFArrayGetCount( childArray );
			for ( repIndex = 0; repIndex < repCount; repIndex++ )
			{
				curReplica = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( childArray, repIndex );
				if ( curReplica != NULL &&
					 CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
					 CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
					 CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
				{
					if ( tidLowSet == NO || tid < tidLow ) {
						tidLow = tid;
						tidLowSet = YES;
					}
				}
			}
		}
		
		CFRelease( replicaDict );
	}
	
	[self unlock];
	
	return tidLow;
}


//----------------------------------------------------------------------------------------------------
//	lowTID
//
//	Returns: the lowest TID for any replica
//----------------------------------------------------------------------------------------------------

-(SInt64)lowTID
{
	unsigned long repIndex = 0;
	unsigned long repCount = [self replicaCount];
	CFMutableDictionaryRef curReplica;
	CFNumberRef tidNumber = NULL;
	SInt64 tid = 0;
	SInt64 tidLow = 0;
	BOOL tidLowSet = NO;
	
	// parent
	curReplica = (CFMutableDictionaryRef)[self getParent];
	if ( curReplica != NULL )
	{
		if ( CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
				CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
				CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
		{
			tidLow = tid;
			tidLowSet = YES;
		}
	}
	
	// replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = (CFMutableDictionaryRef)[self getReplica:repIndex];
		if ( curReplica == NULL )
			continue;
		
		if ( CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
				CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
				CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
		{
			if ( tidLowSet == NO || tid < tidLow ) {
				tidLow = tid;
				tidLowSet = YES;
			}
		}
	}
	
	return tidLow;
}


//----------------------------------------------------------------------------------------------------
//	highTID
//
//	Returns: the highest TID for any replica
//----------------------------------------------------------------------------------------------------

-(SInt64)highTID
{
	unsigned long repIndex = 0;
	unsigned long repCount = [self replicaCount];
	CFMutableDictionaryRef curReplica;
	CFNumberRef tidNumber = NULL;
	SInt64 tid = 0;
	SInt64 tidHigh = 0;
	
	// parent
	curReplica = (CFMutableDictionaryRef)[self getParent];
	if ( curReplica != NULL )
	{
		if ( CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
				CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
				CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
		{
			if ( tid > tidHigh )
				tidHigh = tid;
		}
	}
	
	// replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = (CFMutableDictionaryRef)[self getReplica:repIndex];
		if ( curReplica == NULL )
			continue;
		
		if ( CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaSyncTIDKey), (const void **)&tidNumber) &&
				CFGetTypeID(tidNumber) == CFNumberGetTypeID() &&
				CFNumberGetValue(tidNumber, kCFNumberSInt64Type, &tid) )
		{
			if ( tid > tidHigh )
				tidHigh = tid;
		}
	}
	
	return tidHigh;
}


//----------------------------------------------------------------------------------------------------
//	DecommissionReplica
//
//	Returns: TRUE if the replica is added to the decommissioned list
//----------------------------------------------------------------------------------------------------

-(BOOL)decommissionReplica:(CFStringRef)replicaNameString
{
	BOOL result = NO;
	CFMutableArrayRef decomArray = NULL;
	CFMutableDictionaryRef repDict = NULL;
	CFMutableDictionaryRef parentOfRepDict = NULL;
	CFMutableArrayRef replicaArray = NULL;
	CFStringRef curReplicaNameString = NULL;
	CFIndex repIndex = 0;
	CFIndex repCount = 0;
	CFIndex subtreeRepCount = 0;
	
	if ( replicaNameString == NULL )
		return NO;
	
	[self lock];
	
	@try
	{
		decomArray = [self getArrayForKey:CFSTR(kPWReplicaDecommissionedListKey)];
		
		if ( mReplicaDict != NULL )
		{
			if ( decomArray == NULL )
			{
				decomArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				if ( decomArray != NULL )
				{
					CFDictionaryAddValue( mReplicaDict, CFSTR(kPWReplicaDecommissionedListKey), decomArray );
					CFRelease( decomArray );
				}
			}
			if ( decomArray != NULL )
			{
				if ( ! CFArrayContainsValue(decomArray, CFRangeMake(0, CFArrayGetCount(decomArray)), replicaNameString) )
				{
					CFArrayAppendValue( decomArray, replicaNameString );
					result = YES;
				}
				
				// remove the replica from the Leopard style list
				repDict = [self getReplicaByName:replicaNameString];
				if ( repDict != NULL )
				{
					parentOfRepDict = [self getParentOfReplica:repDict];
					if ( CFDictionaryGetValueIfPresent(parentOfRepDict, CFSTR(kPWReplicaReplicaKey), (const void **)&replicaArray) )
					{
						[self emptyFlatReplicaArray];
						subtreeRepCount = [self replicaCount:parentOfRepDict];
						repCount = CFArrayGetCount( replicaArray );
						
						for ( repIndex = 0; repIndex < repCount; repIndex++ )
						{
							repDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( replicaArray, repIndex );
							if ( repDict != nil )
							{
								if ( CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaNameKey), (const void **)&curReplicaNameString ) )
								{
									if ( CFStringCompare(replicaNameString, curReplicaNameString, 0) == kCFCompareEqualTo )
									{
										if ( subtreeRepCount == 1 )
											CFDictionaryRemoveValue( parentOfRepDict, CFSTR(kPWReplicaReplicaKey) );
										else
											CFArrayRemoveValueAtIndex( replicaArray, repIndex );
										break;
									}
								}
							}
						}
					}
				}
				
				// remove the replica from the Tiger style list
				replicaArray = [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)];
				if ( replicaArray != NULL )
				{
					repCount = CFArrayGetCount( replicaArray );
					for ( repIndex = 0; repIndex < repCount; repIndex++ )
					{
						repDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( replicaArray, repIndex );
						if ( repDict != nil )
						{
							if ( CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaNameKey), (const void **)&curReplicaNameString ) )
							{
								if ( CFStringCompare(replicaNameString, curReplicaNameString, 0) == kCFCompareEqualTo )
								{
									if ( repCount == 1 )
										CFDictionaryRemoveValue( mReplicaDict, CFSTR(kPWReplicaReplicaKey) );
									else
										CFArrayRemoveValueAtIndex( replicaArray, repIndex );
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	@catch(id exception)
	{
	}
	
	[self unlock];
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	replicaIsNotDecommissioned
//----------------------------------------------------------------------------------------------------

-(BOOL)replicaIsNotDecommissioned:(CFStringRef)replicaNameString
{
	BOOL result = YES;
	CFMutableArrayRef decomArray = NULL;

	[self lock];
	
	decomArray = [self getArrayForKey:CFSTR(kPWReplicaDecommissionedListKey)];
	if ( decomArray != NULL )
	{
		if ( CFArrayContainsValue(decomArray, CFRangeMake(0, CFArrayGetCount(decomArray)), replicaNameString) )
			result = NO;
	}
	
	[self unlock];
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	RecommisionReplica
//----------------------------------------------------------------------------------------------------

-(void)recommisionReplica:(const char *)replicaName
{
	[self lock];

	CFMutableArrayRef decomArray = [self getArrayForKey:CFSTR(kPWReplicaDecommissionedListKey)];
	if ( decomArray != NULL )
	{
		CFStringRef replicaNameString = CFStringCreateWithCString( kCFAllocatorDefault, replicaName ? replicaName : "Parent", kCFStringEncodingUTF8 );
		CFIndex index;
		CFIndex itemCount = CFArrayGetCount( decomArray );
		CFStringRef curReplicaString;
		
		for ( index = 0; index < itemCount; index++ )
		{
			curReplicaString = (CFStringRef) CFArrayGetValueAtIndex( decomArray, index );
			if ( curReplicaString != NULL )
			{
				if ( CFStringCompare(curReplicaString, replicaNameString, 0) == kCFCompareEqualTo )
				{
					// found it
					if ( itemCount == 1 )
						CFDictionaryRemoveValue( mReplicaDict, CFSTR(kPWReplicaDecommissionedListKey) );
					else
						CFArrayRemoveValueAtIndex( decomArray, index );
					break;
				}
			}
		}
		
		if ( replicaNameString != NULL )
			CFRelease( replicaNameString );
	}
	
	[self unlock];
}


//----------------------------------------------------------------------------------------------------
//	ReplicaHasBeenPromotedToMaster
//----------------------------------------------------------------------------------------------------

-(BOOL)replicaHasBeenPromotedToMaster:(CFMutableDictionaryRef)inRepDict
{
	BOOL result = NO;
	CFDictionaryRef parentDict = [self getParent];
	CFMutableArrayRef parentIPArray = NULL;
	CFMutableArrayRef ipArray = NULL;
	CFStringRef ipString = NULL;
	CFIndex index;
	CFIndex ipCount;
	CFRange parentIPArrayRange;
	
	if ( parentDict == nil )
		return NO;
	
	parentIPArray = [self getIPAddressesFromDict:(CFMutableDictionaryRef)parentDict];
	if ( parentIPArray == nil )
		return NO;

	ipArray = [self getIPAddressesFromDict:inRepDict];
	if ( ipArray == nil ) {
		CFRelease( parentIPArray );
		return NO;
	}
	
	parentIPArrayRange = CFRangeMake( 0, CFArrayGetCount(parentIPArray) );
	
	ipArray = [self getIPAddressesFromDict:inRepDict];
	if ( ipArray == NULL ) {
		CFRelease( parentIPArray );
		return false;
	}
	
	ipCount = CFArrayGetCount( ipArray );
	for ( index = 0; index < ipCount; index++ )
	{
		ipString = (CFStringRef) CFArrayGetValueAtIndex( ipArray, index );
		if ( ipString != NULL && CFArrayContainsValue(parentIPArray, parentIPArrayRange, ipString) )
		{
			result = true;
			break;
		}
	}
	
	CFRelease( parentIPArray );
	CFRelease( ipArray );
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//	stripDecommissionedArray
//----------------------------------------------------------------------------------------------------

-(void)stripDecommissionedArray
{
	[self lock];
	if ( mReplicaDict != NULL )
		CFDictionaryRemoveValue( mReplicaDict, CFSTR(kPWReplicaDecommissionedListKey) );
	[self unlock];
}


//----------------------------------------------------------------------------------------------------
//	divorceAllReplicas
//----------------------------------------------------------------------------------------------------

-(void)divorceAllReplicas
{
	if ( mReplicaDict == NULL )
		return;
	
	// remove all replicas from the master's entry
	CFMutableDictionaryRef parentDict = (CFMutableDictionaryRef)[self getParent];
	if ( parentDict != NULL )
		CFDictionaryRemoveValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
	
	// remove the Tiger legacy list
	CFDictionaryRemoveValue( mReplicaDict, CFSTR(kPWReplicaReplicaKey) );
	
	[self stripDecommissionedArray];
	
	mDirty = YES;
}


#pragma mark -
#pragma mark Per-Replica methods
#pragma mark -

//----------------------------------------------------------------------------------------------------
//	allocateIDRangeOfSize
//----------------------------------------------------------------------------------------------------

-(void)allocateIDRangeOfSize:(unsigned long)count forReplica:(CFStringRef)inReplicaName minID:(unsigned long)inMinID
{
	CFMutableDictionaryRef selfDict;
	CFStringRef rangeString;
	char firstID[35] = {0,};
	char lastID[35] = {0,};
	char myLastID[35] = {0,};
	char *myLastIDPtr = NULL;
	int err;
	PWFileEntry passRec;
	
	selfDict = [self getReplicaByName:inReplicaName];
	if ( selfDict == NULL )
		return;
	
	[self lock];
	
	if ( CFDictionaryGetValueIfPresent( selfDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) )
		if ( CFStringGetCString( rangeString, myLastID, sizeof(myLastID), kCFStringEncodingUTF8 ) )
			myLastIDPtr = myLastID;
	
	[self getIDRangeOfSize:count after:myLastIDPtr start:firstID end:lastID];
	if ( pwsf_stringToPasswordRecRef(firstID, &passRec) == 1 && passRec.slot < inMinID )
	{
		passRec.slot = inMinID;
		pwsf_passwordRecRefToString( &passRec, firstID );
		passRec.slot = inMinID + count;
		pwsf_passwordRecRefToString( &passRec, lastID );
	}
	err = [self setIDRangeStart:firstID end:lastID forReplica:selfDict];
	[self setEntryModDateForReplica:selfDict];
	
	CFRelease( selfDict );
	mDirty = YES;
	
	[self unlock];
}


-(void)getIDRangeForReplica:(CFStringRef)inReplicaName start:(unsigned long *)outStart end:(unsigned long *)outEnd
{
	CFMutableDictionaryRef replicaDict = [self getReplicaByName:inReplicaName];
		
	if ( replicaDict != NULL )
	{
		[self getIDRangeStart:outStart end:outEnd forReplica:replicaDict];
		CFRelease( replicaDict );
	}
}


-(void)getIDRangeStart:(unsigned long *)outStart end:(unsigned long *)outEnd forReplica:(CFDictionaryRef)inReplicaDict
{
	CFStringRef rangeString;
	PWFileEntry passRec;
	char rangeStr[256];
	
	*outStart = 0;
	*outEnd = 0;
	
	if ( inReplicaDict == NULL )
		return;
	
	if ( CFDictionaryGetValueIfPresent( inReplicaDict, CFSTR(kPWReplicaIDRangeBeginKey), (const void **)&rangeString ) &&
		 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
		 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) )
	{
		*outStart = passRec.slot;
	}
	
	if ( CFDictionaryGetValueIfPresent( inReplicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) &&
		 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
		 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) )
	{
		*outEnd = passRec.slot;
	}
}


-(void)setSyncDate:(CFDateRef)date forReplica:(CFStringRef)inReplicaName
{
	CFMutableDictionaryRef repDict;

	repDict = [self getReplicaByName:inReplicaName];
	if ( repDict != NULL )
	{
		CFDictionarySetValue( repDict, CFSTR(kPWReplicaSyncDateKey), date );
		CFRelease( repDict );
		mDirty = YES;
	}
}


-(void)setSyncDate:(CFDateRef)date andHighTID:(SInt64)tid forReplica:(CFStringRef)inReplicaName
{
	CFMutableDictionaryRef repDict = NULL;
	CFNumberRef aNumberRef = NULL;
	
	repDict = [self getReplicaByName:inReplicaName];
	if ( repDict != NULL )
	{
		CFDictionarySetValue( repDict, CFSTR(kPWReplicaSyncDateKey), date );
		
		aNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt64Type, &tid );
		if ( aNumberRef != NULL ) {
			CFDictionarySetValue( repDict, CFSTR(kPWReplicaSyncTIDKey), aNumberRef );
			CFRelease( aNumberRef );
		}
		
		CFRelease( repDict );
		mDirty = YES;
	}
}


-(void)setEntryModDateForReplica:(CFMutableDictionaryRef)inReplicaDict
{
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	if ( nowDate != NULL ) {
		[self setKey:CFSTR(kPWReplicaEntryModDateKey) withDate:nowDate forReplica:inReplicaDict];
		CFRelease( nowDate );
	}
}


-(void)setSyncAttemptDateForReplica:(CFStringRef)inReplicaName
{
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	
	[self lock];
	[self setKey:CFSTR(kPWReplicaSyncAttemptKey) withDate:nowDate forReplicaWithName:inReplicaName];
	[self unlock];
}


-(void)setKey:(CFStringRef)key withDate:(CFDateRef)date forReplicaWithName:(CFStringRef)inReplicaName
{
	CFMutableDictionaryRef repDict;
	
	repDict = [self getReplicaByName:inReplicaName];
	if ( repDict == NULL )
		return;
	
	[self setKey:key withDate:date forReplica:repDict];
	CFRelease( repDict );
}

-(void)setKey:(CFStringRef)key withDate:(CFDateRef)date forReplica:(CFMutableDictionaryRef)inReplicaDict;
{
	CFDictionarySetValue( inReplicaDict, key, date );
	mDirty = YES;
}

-(CFMutableDictionaryRef)getReplicaByName:(CFStringRef)inReplicaName
{
	CFMutableDictionaryRef theDict = NULL;
	
	if ( inReplicaName == NULL || CFStringGetLength(inReplicaName) == 0 ||
		 CFStringCompare(inReplicaName, CFSTR("Parent"), 0) == kCFCompareEqualTo )
	{
		theDict = (CFMutableDictionaryRef)[self getParent];
	}
	else
	{
		theDict = [self findMatchToKey:CFSTR(kPWReplicaNameKey) withValue:inReplicaName];
	}
	
	if ( theDict != NULL )
		CFRetain( theDict );
	
	return theDict;
}


-(CFStringRef)getNameOfReplica:(CFMutableDictionaryRef)inReplicaDict
{
	if ( inReplicaDict == NULL )
		return NULL;
	
	return CFDictionaryGetValue( inReplicaDict, CFSTR(kPWReplicaNameKey) );
}


-(CFStringRef)getNameFromIPAddress:(const char *)inIPAddress
{
	CFStringRef ipAddrString = NULL;
	CFMutableDictionaryRef theReplica = NULL;
	CFStringRef nameString = NULL;
	
	ipAddrString = CFStringCreateWithCString( kCFAllocatorDefault, inIPAddress, kCFStringEncodingUTF8 );
	if ( ipAddrString != NULL )
	{
		theReplica = [self findMatchToKey:CFSTR(kPWReplicaIPKey) withValue:ipAddrString];
		CFRelease( ipAddrString );
	}
	
	if ( theReplica != NULL )
	{
		if ( !CFDictionaryGetValueIfPresent( theReplica, CFSTR(kPWReplicaNameKey), (const void **)&nameString ) )
			nameString = CFSTR("");		// it's the parent
	}
	
	return nameString;
}


-(UInt8)getReplicaSyncPolicy:(CFDictionaryRef)inReplicaDict
{
	return [self getReplicaSyncPolicy:inReplicaDict defaultPolicy:kReplicaSyncAnytime];
}


-(UInt8)getReplicaSyncPolicy:(CFDictionaryRef)inReplicaDict defaultPolicy:(UInt8)inDefaultPolicy
{
	CFStringRef valueString = NULL;
	UInt8 returnValue = inDefaultPolicy;
	
	valueString = CFDictionaryGetValue( inReplicaDict, CFSTR(kPWReplicaPolicyKey) );
	if ( valueString != NULL )
	{
		UInt8 testValue = [self getReplicaSyncPolicyForString:valueString];
		if ( testValue != kReplicaSyncInvalid )
			returnValue = testValue;
	}
	
	return returnValue;
}


//----------------------------------------------------------------------------------------------------
//	setReplicaSyncPolicy:forReplica:
//
//  Returns: TRUE if set
//----------------------------------------------------------------------------------------------------

-(BOOL)setReplicaSyncPolicy:(UInt8)policy forReplica:(CFStringRef)inReplicaName
{
	CFMutableDictionaryRef repDict;
	CFStringRef policyString = NULL;
	bool result = false;
	
	repDict = [self getReplicaByName:inReplicaName];
	if ( repDict == NULL )
		return result;
	
	switch( policy )
	{
		case kReplicaSyncDefault:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyDefaultKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncNever:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyNeverKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnlyIfDesperate:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnlyIfDesperateKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnSchedule:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnScheduleKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnDirty:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnDirtyKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncAnytime:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyAnytimeKey, kCFStringEncodingUTF8 );
			break;
	}
	
	if ( policyString != NULL )
	{
		result = [self setReplicaSyncPolicyWithString:policyString forReplica:repDict];
		CFRelease( policyString );
	}
	
	CFRelease( repDict );
	
	return result;
}


-(BOOL)setReplicaSyncPolicyWithString:(CFStringRef)inPolicyString forReplica:(CFMutableDictionaryRef)inRepDict
{
	if ( inRepDict == NULL || inPolicyString == NULL )
		return NO;
	
	if ( [self getReplicaSyncPolicyForString:inPolicyString] == kReplicaSyncInvalid )
		return NO;
	
	CFDictionarySetValue( inRepDict, CFSTR(kPWReplicaPolicyKey), inPolicyString );
	[self setEntryModDateForReplica:inRepDict];
	mDirty = YES;
	
	return YES;
}


-(UInt8)getReplicaSyncPolicyForString:(CFStringRef)inPolicyString
{
	UInt8 returnValue = kReplicaSyncInvalid;
	
	if ( inPolicyString != NULL )
	{
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyDefaultKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncDefault;
		else
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyNeverKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncNever;
		else
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyOnlyIfDesperateKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncOnlyIfDesperate;
		else
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyOnScheduleKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncOnSchedule;
		else
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyOnDirtyKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncOnDirty;
		else
		if ( CFStringCompare(inPolicyString, CFSTR(kPWReplicaPolicyAnytimeKey), 0) == kCFCompareEqualTo )
			returnValue = kReplicaSyncAnytime;
	}
	
	return returnValue;
}


-(ReplicaStatus)getReplicaStatus:(CFDictionaryRef)inReplicaDict
{
	char valueStr[256];
	UInt8 returnValue = kReplicaActive;
	
	if ( [self getCStringFromDictionary:inReplicaDict forKey:CFSTR(kPWReplicaStatusKey) maxLen:sizeof(valueStr) result:valueStr] )
	{
		if ( strcmp( valueStr, kPWReplicaStatusActiveValue ) == 0 )
			returnValue = kReplicaActive;
		else
		if ( strcmp( valueStr, kPWReplicaStatusPermDenyValue ) == 0 )
			returnValue = kReplicaPermissionDenied;
		else
		if ( strcmp( valueStr, kPWReplicaStatusNotFoundValue ) == 0 )
			returnValue = kReplicaNotFound;
	}
	
	return returnValue;
}


-(void)setReplicaStatus:(ReplicaStatus)status forReplica:(CFMutableDictionaryRef)repDict
{
	CFStringRef statusString = NULL;
	
	if ( repDict == NULL )
		return;
	
	switch( status )
	{
		case kReplicaActive:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusActiveValue, kCFStringEncodingUTF8 );
			break;
		case kReplicaPermissionDenied:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusPermDenyValue, kCFStringEncodingUTF8 );
			break;
		case kReplicaNotFound:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusNotFoundValue, kCFStringEncodingUTF8 );
			break;
	}
	
	if ( statusString != NULL )
	{
		CFDictionarySetValue( repDict, CFSTR(kPWReplicaStatusKey), statusString );
		CFRelease( statusString );
		[self setEntryModDateForReplica:repDict];
		mDirty = YES;
	}
}


// utilities
-(CFStringRef)selfName
{
	if ( mSelfName != NULL )
	{
		[self lock];
		if ( mSelfName != NULL )
			CFRetain( mSelfName );
		[self unlock];
	}
	
	return mSelfName;
}


-(void)setSelfName:(CFStringRef)selfName
{
	[self lock];
	CFRetain( selfName );
	if ( mSelfName != NULL )
		CFRelease( mSelfName );
	
	mSelfName = selfName;
	[self unlock];
}


-(BOOL)getCStringFromDictionary:(CFDictionaryRef)inDict forKey:(CFStringRef)inKey maxLen:(long)inMaxLen result:(char *)outString
{
	BOOL result = NO;
	CFStringRef stringValue;
	
	if ( inDict != NULL )
	{
		stringValue = (CFStringRef)CFDictionaryGetValue( inDict, inKey );
		if ( stringValue != NULL && CFGetTypeID(stringValue) == CFStringGetTypeID() )
			result = CFStringGetCString( stringValue, outString, inMaxLen, kCFStringEncodingUTF8 );
	}
	
	return result;
}


-(BOOL)dirty
{
	return mDirty;
}


-(void)setDirty:(BOOL)dirty
{
	mDirty = dirty;
}


// replica array is at the top level
-(BOOL)isOldFormat
{
	CFDictionaryRef parentDict = [self getParent];
	
	// error, no format, old or new
	if ( parentDict == NULL )
		return NO;
	
	// if there are replica arrays inside of the parent server's entry, it's the Leopard format.
	// if the Tiger array is missing, call it an old format.
	CFArrayRef parentDictReplicas = (CFArrayRef)CFDictionaryGetValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
	if ( parentDictReplicas != NULL )
		return ( [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)] == NULL );
	
	// if there are replicas in the old place (top-level) but not in the new place,
	// the format is old.
	return ( [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)] != NULL );
}


// copy replica array inside the parent's dictionary, but also
// leave the original array for Tiger clients.
-(void)updateFormat
{
	CFMutableDictionaryRef parentDict = NULL;
	CFMutableArrayRef replicaArray = NULL;
	CFArrayRef parentDictReplicas = NULL;
	
	[self lock];
	replicaArray = [self getArrayForKey:CFSTR(kPWReplicaReplicaKey)];
	if ( replicaArray != NULL )
	{
		CFRetain( replicaArray );
		parentDict = (CFMutableDictionaryRef)[self getParent];
		if ( parentDict != NULL )
			CFDictionaryAddValue( parentDict, CFSTR(kPWReplicaReplicaKey), replicaArray );
		CFRelease( replicaArray );
	}
	else
	{
		// Check for 10.5.0 GM list with no legacy Tiger replica list
		parentDict = (CFMutableDictionaryRef)[self getParent];
		if ( parentDict != NULL )
		{
			parentDictReplicas = (CFArrayRef)CFDictionaryGetValue( parentDict, CFSTR(kPWReplicaReplicaKey) );
			if ( parentDictReplicas != NULL )
			{
				unsigned long repIndex;
				unsigned long repCount = [self replicaCount];
				CFMutableDictionaryRef curReplica = NULL;
	
				// create Tiger legacy list
				for ( repIndex = 0; repIndex < repCount; repIndex++ )
				{
					curReplica = (CFMutableDictionaryRef)[self getReplica:repIndex];
					if ( curReplica != NULL )
						[self addReplicaToLegacyTigerList:curReplica];
				}
			}
		}
	}
	
	mDirty = YES;
	[self unlock];
}


-(void)runningAsParent:(BOOL)parent
{
	mRunningAsParent = parent;
}


-(BOOL)isHappy
{
	return (mReplicaDict != NULL);
}


// other
-(CFStringRef)getNextReplicaName
{
	UInt32 repIndex = 0;
	UInt32 repCount = 0;
	CFDictionaryRef curReplica = NULL;
	CFStringRef curNameString = NULL;
	CFMutableArrayRef decomArray = NULL;
	const int replicaNameValuePrefixLen = sizeof(kPWReplicaNameValuePrefix) - 1;
	int tempReplicaNumber = 0;
	int nextReplicaNumber = 1;
	char replicaNameStr[256];
	char nextReplicaNameStr[256];
	char selfName[256];
	
	[self lock];

	// check active replicas
	repCount = [self replicaCount];
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = [self getReplica:repIndex];
		if ( curReplica != NULL &&
			 CFDictionaryGetValueIfPresent(curReplica, CFSTR(kPWReplicaNameKey), (const void **)&curNameString) &&
			 CFStringHasPrefix(curNameString, CFSTR(kPWReplicaNameValuePrefix)) &&
			 CFStringGetCString(curNameString, replicaNameStr, sizeof(replicaNameStr), kCFStringEncodingUTF8) )
		{
			sscanf( replicaNameStr + replicaNameValuePrefixLen, "%d", &tempReplicaNumber );
			if ( tempReplicaNumber >= nextReplicaNumber )
				nextReplicaNumber = tempReplicaNumber + 1;
		}
	}
	
	// check decommissioned list
	decomArray = [self getArrayForKey:CFSTR(kPWReplicaDecommissionedListKey)];
	if ( decomArray != NULL )
	{
		repCount = CFArrayGetCount( decomArray );
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			curNameString = CFArrayGetValueAtIndex( decomArray, repIndex );
			if ( curNameString != NULL &&
				 CFStringHasPrefix(curNameString, CFSTR(kPWReplicaNameValuePrefix)) &&
				 CFStringGetCString(curNameString, replicaNameStr, sizeof(replicaNameStr), kCFStringEncodingUTF8) )
			{
				sscanf( replicaNameStr + replicaNameValuePrefixLen, "%d", &tempReplicaNumber );
				if ( tempReplicaNumber >= nextReplicaNumber )
					nextReplicaNumber = tempReplicaNumber + 1;
			}
		}
	}
	
	[self unlock];
	
	// make name
	sprintf( nextReplicaNameStr, "%s%d", kPWReplicaNameValuePrefix, nextReplicaNumber );
	
	// if making a replica of a replica, add the other name to avoid collisions
	if ( mSelfName != NULL &&
		 CFStringHasPrefix(mSelfName, CFSTR(kPWReplicaNameValuePrefix)) &&
		 CFStringGetCString(mSelfName, selfName, sizeof(selfName), kCFStringEncodingUTF8) )
	{
		strcat( nextReplicaNameStr, "." );
		strcat( nextReplicaNameStr, selfName + replicaNameValuePrefixLen );
	}
	
	return CFStringCreateWithCString( kCFAllocatorDefault, nextReplicaNameStr, kCFStringEncodingUTF8 );
}


-(int)statReplicaFile:(const char *)inFilePath andGetModDate:(struct timespec *)outModDate
{
	struct stat sb;
	int result;
	
	if ( outModDate != NULL ) {
		outModDate->tv_sec = 0;
		outModDate->tv_nsec = 0;
	}
	
	result = lstat( inFilePath, &sb );
	if ( result == 0 && outModDate != NULL )
		*outModDate = sb.st_mtimespec;
	
	return result;
}


-(int)loadXMLData:(const char *)inFilePath
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef;
	CFStringRef errorString;
	CFPropertyListFormat myPLFormat;
	struct timespec modDate;
	int result = -1;
	
	[self lock];
	
	do
	{
		if ( [self statReplicaFile:inFilePath andGetModDate:&modDate] != 0 )
			break;
	
		myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inFilePath, kCFStringEncodingUTF8 );
		if ( myReplicaDataFilePathRef == NULL )
			break;
		
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myReplicaDataFilePathRef );
		
		if ( myReplicaDataFileRef == NULL )
			break;
		
		myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
		
		CFRelease( myReplicaDataFileRef );
		
		if ( myReadStreamRef == NULL )
			break;
		
		if ( ! CFReadStreamOpen( myReadStreamRef ) ) {
			CFRelease( myReadStreamRef );
			break;
		}
		
		errorString = NULL;
		myPLFormat = kCFPropertyListXMLFormat_v1_0;
		myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
		CFReadStreamClose( myReadStreamRef );
		CFRelease( myReadStreamRef );
		
		if ( errorString != NULL )
			CFRelease( errorString );
		
		if ( myPropertyListRef == NULL )
			break;
		
		if ( CFGetTypeID(myPropertyListRef) != CFDictionaryGetTypeID() ) {
			CFRelease( myPropertyListRef );
			break;
		}
		
		mReplicaDict = (CFMutableDictionaryRef)myPropertyListRef;
		mReplicaFileModDate = modDate;
		result = 0;
	}
	while (0);
	
	[self unlock];
	
	return result;
}


-(CFMutableArrayRef)getArrayForKey:(CFStringRef)key
{
	CFMutableArrayRef theArray = NULL;
	
	if ( mReplicaDict == NULL )
		return NULL;
	
	[self lock];
	theArray = (CFMutableArrayRef) CFDictionaryGetValue( mReplicaDict, key );
	[self unlock];
	if ( theArray == NULL || CFGetTypeID(theArray) != CFArrayGetTypeID() )
		return NULL;
	
	return theArray;
}


-(void)getIDRangeOfSize:(unsigned long)count after:(const char *)inMyLastID start:(char *)outFirstID end:(char *)outLastID
{
	PWFileEntry passRec, endPassRec;
	CFDictionaryRef replicaDict;
	CFStringRef rangeString;
	UInt32 repIndex, repCount = [self replicaCount];
	char rangeStr[35];
	
	bzero( &passRec, sizeof(PWFileEntry) );
	bzero( &endPassRec, sizeof(PWFileEntry) );
	
	endPassRec.slot = 0;
	
	if ( inMyLastID != NULL )
	{
		pwsf_stringToPasswordRecRef( inMyLastID, &passRec );
		endPassRec.slot = passRec.slot;
	}
	
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		replicaDict = [self getReplica:repIndex];
		if ( replicaDict == NULL )
			continue;
		
		if ( ! CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) )
			continue;
	
		if ( ! CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) )
			continue;
		
		if ( pwsf_stringToPasswordRecRef( rangeStr, &passRec ) == 0 )
			continue;
		
		if ( passRec.slot > endPassRec.slot )
			endPassRec.slot = passRec.slot;
	}
	
	replicaDict = [self getParent];
	if ( replicaDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) &&
			 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
			 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) == 1
		   )
		{
			if ( passRec.slot > endPassRec.slot )
				endPassRec.slot = passRec.slot;		
		}
	}
	
	endPassRec.slot += (endPassRec.slot > 0) ? 20 : 1;
	pwsf_passwordRecRefToString( &endPassRec, outFirstID );
	
	endPassRec.slot += count;
	pwsf_passwordRecRefToString( &endPassRec, outLastID );
}


-(int)setIDRangeStart:(const char *)inFirstID end:(const char *)inLastID forReplica:(CFMutableDictionaryRef)inServerDict
{
	CFStringRef rangeString;
	
	rangeString = CFStringCreateWithCString( kCFAllocatorDefault, inFirstID, kCFStringEncodingUTF8 );
	if ( rangeString != NULL ) {
		CFDictionarySetValue( inServerDict, CFSTR(kPWReplicaIDRangeBeginKey), rangeString );
		CFRelease( rangeString );
	}
	
	rangeString = CFStringCreateWithCString( kCFAllocatorDefault, inLastID, kCFStringEncodingUTF8 );
	if ( rangeString != NULL ) {
		CFDictionarySetValue( inServerDict, CFSTR(kPWReplicaIDRangeEndKey), rangeString );
		CFRelease( rangeString );
	}
	
	return 0;
}


-(CFMutableDictionaryRef)findMatchToKey:(CFStringRef)inKey withValue:(CFStringRef)inValue
{
	CFMutableDictionaryRef theDict = NULL;
	CFTypeRef evalCFType;
	UInt32 repIndex, repCount;
	BOOL found = NO;
		
	theDict = (CFMutableDictionaryRef)[self getParent];
	if ( theDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( theDict, inKey, (const void **)&evalCFType ) )
		{
			if ( CFGetTypeID(evalCFType) == CFStringGetTypeID() &&
				 CFStringCompare(inValue, (CFStringRef)evalCFType, 0) == kCFCompareEqualTo )
			{
				found = YES;
			}
			else
			if ( CFGetTypeID(evalCFType) == CFArrayGetTypeID() )
			{
				if ( CFArrayContainsValue((CFArrayRef)evalCFType, CFRangeMake(0, CFArrayGetCount((CFArrayRef)evalCFType) - 1), inValue) )
					found = YES;
			}
		}
	}
	
	if ( !found )
	{
		// clear the parent dictionary
		theDict = NULL;
		
		repCount = [self replicaCount];
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			theDict = (CFMutableDictionaryRef)[self getReplica:repIndex];
			if ( theDict == NULL )
				break;
			
			if ( ! CFDictionaryGetValueIfPresent( theDict, inKey, (const void **)&evalCFType ) )
				continue;
			
			if ( CFGetTypeID(evalCFType) == CFStringGetTypeID() &&
				 CFStringCompare(inValue, (CFStringRef)evalCFType, 0) == kCFCompareEqualTo )
			{
				break;
			}
			else
			if ( CFGetTypeID(evalCFType) == CFArrayGetTypeID() )
			{
				if ( CFArrayContainsValue((CFArrayRef)evalCFType, CFRangeMake(0, CFArrayGetCount((CFArrayRef)evalCFType) - 1), inValue) )
					break;
			}
			
			theDict = NULL;
		}
	}
		
	return theDict;
}


-(CFMutableDictionaryRef)replicaDict
{
	return mReplicaDict;
}

@end



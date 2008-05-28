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

#import <objc/Object.h>
#import <PasswordServer/ReplicaFileDefs.h>

/*
static void SetEntryModDate( CFMutableDictionaryRef inReplicaDict );
static void AddOrReplaceValueStatic( CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue );
static int SaveXMLData( CFPropertyListRef inListToWrite, const char *inSaveFile );
*/

@interface ReplicaFile : Object {
	CFMutableDictionaryRef mReplicaDict;
	CFMutableArrayRef mFlatReplicaArray;
	BOOL mDirty;
	struct timespec mReplicaFileModDate;
	CFStringRef mSelfName;
	BOOL mRunningAsParent;
}

-(id)init;
-(id)initWithPList:(CFDictionaryRef)xmlPList;
-(id)initWithContentsOfFile:(const char *)filePath;
-(id)initWithXMLStr:(const char *)xmlStr;
-(void)initCommon;
-free;

// traps for overrides
-(void)lock;
-(void)unlock;

// merging
-(ReplicaChangeStatus)mergeReplicaList:(ReplicaFile *)inOtherList;
-(void)mergeReplicaListDecommissionedList:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus;
-(void)mergeReplicaListParentRecords:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus;
-(void)mergeReplicaListReplicas:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus;
-(void)mergeReplicaListLegacyTigerReplicaList:(ReplicaFile *)inOtherList changeStatus:(ReplicaChangeStatus *)inOutChangeStatus;
-(BOOL)mergeReplicaValuesFrom:(CFMutableDictionaryRef)dict1 to:(CFMutableDictionaryRef)dict2 parent:(BOOL)isParent;
-(BOOL)needsMergeFrom:(CFMutableDictionaryRef)dict1 to:(CFMutableDictionaryRef)dict2;

// top level
-(PWSReplicaEntry *)snapshotOfReplicasForServer:(CFDictionaryRef)serverDict;
-(void)serverStruct:(PWSReplicaEntry *)sEnt forServerDict:(CFDictionaryRef)serverDict;
-(CFMutableDictionaryRef)getParentOfReplica:(CFDictionaryRef)replicaDict;
-(BOOL)array:(CFArrayRef)replicaArray containsReplicaWithName:(CFStringRef)targetNameString;

-(ReplicaPolicy)getReplicaPolicy;
-(void)setReplicaPolicy:(ReplicaPolicy)inPolicy;
-(void)emptyFlatReplicaArray;
-(unsigned long)replicaCount;
-(unsigned long)replicaCount:(CFDictionaryRef)inReplicaDict;
-(CFDictionaryRef)getReplica:(unsigned long)index;
-(BOOL)isActive;
-(CFStringRef)getUniqueID;
-(CFStringRef)currentServerForLDAP;
-(CFDictionaryRef)getParent;
-(CFStringRef)xmlString;
-(BOOL)fileHasChanged;
-(void)refreshIfNeeded;
-(CFStringRef)calcServerUniqueID:(const char *)inRSAPublicKey;
-(void)addServerUniqueID:(const char *)inRSAPublicKey;
-(void)setParentWithIP:(const char *)inIPStr andDNS:(const char *)inDNSStr;
-(void)setParentWithDict:(CFDictionaryRef)inParentData;
-(CFMutableDictionaryRef)addReplicaWithIP:(const char *)inIPStr andDNS:(const char *)inDNSStr withParent:(CFMutableDictionaryRef)inParentDict;
-(CFMutableDictionaryRef)addReplica:(CFMutableDictionaryRef)inReplicaData withParent:(CFMutableDictionaryRef)inParentDict;
-(void)addReplicaToLegacyTigerList:(CFMutableDictionaryRef)inReplicaData;
-(BOOL)addIPAddress:(const char *)inIPStr toReplica:(CFMutableDictionaryRef)inReplicaDict;
-(void)addIPAddress:(const char *)inNewIPStr orReplaceIP:(const char *)inOldIPStr inReplica:(CFMutableDictionaryRef)inReplicaDict;
-(CFMutableArrayRef)getIPAddressesFromDict:(CFDictionaryRef)inReplicaDict;

-(int)saveXMLData;
-(int)saveXMLDataToFile:(const char *)inSaveFile;
-(void)stripSyncDates;
-(CFDateRef)oldestSyncDate;
-(ReplicaRole)roleForReplica:(CFStringRef)replicaName;
-(SInt64)lowTIDForReplica:(CFStringRef)replicaName;
-(SInt64)lowTID;
-(SInt64)highTID;

-(BOOL)decommissionReplica:(CFStringRef)replicaName;
-(BOOL)replicaIsNotDecommissioned:(CFStringRef)replicaNameString;
-(void)recommisionReplica:(const char *)replicaName;
-(BOOL)replicaHasBeenPromotedToMaster:(CFMutableDictionaryRef)inRepDict;
-(void)stripDecommissionedArray;
-(void)divorceAllReplicas;

// per replica
-(void)allocateIDRangeOfSize:(unsigned long)count forReplica:(CFStringRef)inReplicaName minID:(unsigned long)inMinID;
-(void)getIDRangeForReplica:(CFStringRef)inReplicaName start:(unsigned long *)outStart end:(unsigned long *)outEnd;
-(void)getIDRangeStart:(unsigned long *)outStart end:(unsigned long *)outEnd forReplica:(CFDictionaryRef)inReplicaDict;
-(void)setSyncDate:(CFDateRef)date forReplica:(CFStringRef)inReplicaName;
-(void)setSyncDate:(CFDateRef)date andHighTID:(SInt64)tid forReplica:(CFStringRef)inReplicaName;
-(void)setEntryModDateForReplica:(CFMutableDictionaryRef)inReplicaDict;
-(void)setSyncAttemptDateForReplica:(CFStringRef)inReplicaName;
-(void)setKey:(CFStringRef)key withDate:(CFDateRef)date forReplicaWithName:(CFStringRef)inReplicaName;
-(void)setKey:(CFStringRef)key withDate:(CFDateRef)date forReplica:(CFMutableDictionaryRef)inReplicaDict;
-(CFMutableDictionaryRef)getReplicaByName:(CFStringRef)inReplicaName;
-(CFStringRef)getNameOfReplica:(CFMutableDictionaryRef)inReplicaDict;
-(CFStringRef)getNameFromIPAddress:(const char *)inIPAddress;
-(UInt8)getReplicaSyncPolicy:(CFDictionaryRef)inReplicaDict;
-(UInt8)getReplicaSyncPolicy:(CFDictionaryRef)inReplicaDict defaultPolicy:(UInt8)inDefaultPolicy;
-(BOOL)setReplicaSyncPolicy:(UInt8)policy forReplica:(CFStringRef)inReplicaName;
-(BOOL)setReplicaSyncPolicyWithString:(CFStringRef)inPolicyString forReplica:(CFMutableDictionaryRef)inRepDict;
-(UInt8)getReplicaSyncPolicyForString:(CFStringRef)inPolicyString;
-(ReplicaStatus)getReplicaStatus:(CFDictionaryRef)inReplicaDict;
-(void)setReplicaStatus:(ReplicaStatus)status forReplica:(CFMutableDictionaryRef)repDict;

// utilities
-(CFStringRef)selfName;
-(void)setSelfName:(CFStringRef)selfName;
-(BOOL)getCStringFromDictionary:(CFDictionaryRef)inDict forKey:(CFStringRef)inKey maxLen:(long)inMaxLen result:(char *)outString;
-(BOOL)dirty;
-(void)setDirty:(BOOL)dirty;
-(BOOL)isOldFormat;
-(void)updateFormat;
-(void)runningAsParent:(BOOL)parent;
-(BOOL)isHappy;

// other
-(CFStringRef)getNextReplicaName;
-(int)statReplicaFile:(const char *)inFilePath andGetModDate:(struct timespec *)outModDate;
-(int)loadXMLData:(const char *)inFilePath;
-(CFMutableArrayRef)getArrayForKey:(CFStringRef)key;
-(void)getIDRangeOfSize:(unsigned long)count after:(const char *)inMyLastID start:(char *)outFirstID end:(char *)outLastID;
-(int)setIDRangeStart:(const char *)inFirstID end:(const char *)inLastID forReplica:(CFMutableDictionaryRef)inServerDict;
-(CFMutableDictionaryRef)findMatchToKey:(CFStringRef)inKey withValue:(CFStringRef)inValue;
-(CFMutableDictionaryRef)replicaDict;

@end

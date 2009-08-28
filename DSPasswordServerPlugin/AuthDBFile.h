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

#import <Foundation/Foundation.h>
#import <unistd.h>
#import <PasswordServer/AuthDBFileDefs.h>
#import <PasswordServer/AuthFile.h>
#import <PasswordServer/AuthOverflowFile.h>
#import <PasswordServer/DES.h>
#import <PasswordServer/key.h>
#import <PasswordServer/CKerberosPrincipal.h>

@interface AuthDBFile : NSObject {
	FILE *mFreeListFile;
	size_t mPWFileLen;
	PWFileHeader mPWFileHeader;
	Boolean mPWFileValidated;
	char mPWFilePermission[10];
	RSA *rsaKey;
	bool mGotHeader;
	char *mFilePathStr;
	char *mDirPathStr;
	bool mDBFileLocked;
	bool mReadOnlyFileSystem;
	AuthOverflowFile *mOverflow;
	SyncPriority mSyncPriority;
	BOOL mTestSpillBucket;
	uint32_t mKerberosCacheLimit;
	char *mSearchBase;
	int mPWFileNO;
	int32_t mPWLockCount;
	pthread_mutex_t mLocksLock;
	time_t mPWHdrLastMod;
}

-(id)initWithFile:(const char *)inFilePath;
-(void)dealloc;
-free DEPRECATED_ATTRIBUTE;

-(void)createOverflowObject;
-(int)validateFiles;
-(int)validatePasswordFile;
-(int)validateFreeListFile;
-(int)createPasswordFile;
-(int)openPasswordFile:(const char *)mode;
-(void)closePasswordFile;
-(void)closeFreeListFile;
-(void)freeRSAKey;

-(void)resetPasswordFileState;
-(void)pwLock;
-(BOOL)pwLock:(unsigned long)inMillisecondsToWait;
-(void)pwUnlock;
-(BOOL)pwLockLock:(unsigned long)inMillisecondsToWait;
-(void)pwLockUnlock;
-(void)pwWait;
-(void)pwSignal;
-(void)rsaWait;
-(void)rsaSignal;

-(int)getHeader:(PWFileHeader *)outHeader;
-(int)getHeader:(PWFileHeader *)outHeader cachedCopyOK:(BOOL)inCanUseCachedCopy;	// default NO
-(int)setHeader:(const PWFileHeader *)inHeader;

-(int)getRSAPublicKey:(char *)outRSAKeyStr;
-(int)loadRSAKeys;
-(int)decryptRSA:(unsigned char *)inBlob length:(int)inBlobLen result:(unsigned char *)outBlob;
-(int)encryptRSA:(unsigned char *)inBlob length:(int)inBlobLen result:(unsigned char *)outBlob;

-(int)isWeakAuthMethod:(const char *)inMethod;
-(int)addWeakAuthMethod:(const char *)inMethod;
-(int)removeWeakAuthMethod:(const char *)inMethod;

-(int)expandDatabase:(uint32_t)inNumSlots nextAvailableSlot:(uint32_t *)outSlot;
-(BOOL)getBigNumber:(char **)outBigNumStr;
-(uint32_t)nextSlot;

-(int)addRSAKeys;
-(int)addRSAKeys:(unsigned int)inBitCount;
-(int)addRSAKeys:(unsigned char *)publicKey
	publicKeyLen:(uint32_t)publicKeyLen
	privateKey:(unsigned char *)privateKey
	privateKeyLen:(uint32_t)privateKeyLen;

-(int)addGenesisUser:(const char *)username password:(const char *)password pwsRec:(PWFileEntry *)outPWRec;
-(int)addPassword:(PWFileEntry *)passwordRec obfuscate:(BOOL)obfuscate;
-(int)addPassword:(PWFileEntry *)passwordRec atSlot:(uint32_t)slot obfuscate:(BOOL)obfuscate;
-(int)addPassword:(PWFileEntry *)passwordRec atSlot:(uint32_t)slot obfuscate:(BOOL)obfuscate setModDate:(BOOL)setModDate;
-(int)addPasswordFast:(PWFileEntry *)passwordRec atSlot:(uint32_t)slot;
-(int)initPasswordRecord:(PWFileEntry *)passwordRec obfuscate:(BOOL)obfuscate;	// default YES
-(int)newPasswordForUser:(const char *)inUser
		password:(const char *)inPassword
		slotStr:(char *)outPasswordRef
		slotRec:(PWFileEntry *)inOutUserRec;
-(int)getPasswordRec:(uint32_t)slot putItHere:(PWFileEntry *)passRec;
-(int)getPasswordRec:(uint32_t)slot putItHere:(PWFileEntry *)passRec unObfuscate:(BOOL)unObfuscate;
-(int)getValidPasswordRec:(PWFileEntry *)passwordRec;
-(int)getValidPasswordRec:(PWFileEntry *)passwordRec fromSpillBucket:(BOOL *)outFromSpillBucket			// default NULL
	unObfuscate:(BOOL)unObfuscate;	// default YES
-(int)getValidOrZeroPasswordRec:(PWFileEntry *)passwordRec fromSpillBucket:(BOOL *)outFromSpillBucket	// default NULL
	unObfuscate:(BOOL)unObfuscate;	// default YES
-(CFArrayRef)purgeDeadSlots:(CFDateRef)beforeDate deleteWait:(long)deleteWaitSeconds purgeWait:(long)purgeWaitSeconds;
-(int)freeSlot:(PWFileEntry *)passwordRec;
-(int)freeSlot:(PWFileEntry *)passwordRec deathCertificate:(BOOL)useDeathCertificate;

-(int)setPassword:(PWFileEntry *)passwordRec atSlot:(uint32_t)slot;
-(int)setPassword:(PWFileEntry *)passwordRec
	atSlot:(uint32_t)slot
	obfuscate:(BOOL)obfuscate
	setModDate:(BOOL)setModDate;
-(int)setPasswordFast:(PWFileEntry *)passwordRec atSlot:(uint32_t)slot;
-(void)addHashes:(const char *)inRealm addNT:(BOOL)inAddNT addLM:(BOOL)inAddLM pwsRec:(PWFileEntry *)inOutPasswordRec;

// scope of authority groups
-(BOOL)addGroup:(uuid_t)groupUUID toAdmin:(PWFileEntry *)inOutAdminRec;
-(BOOL)removeGroup:(uuid_t)groupUUID fromAdmin:(PWFileEntry *)inOutAdminRec;
-(BOOL)removeAllGroupsFromAdmin:(PWFileEntry *)inOutAdminRec;

// owners of a computer slot
-(BOOL)addOwners:(const char *)ownerList toEntry:(PWFileEntry *)inOutRec;
-(BOOL)removeAllOwnersFromEntry:(PWFileEntry *)inOutRec;
-(BOOL)isOwner:(const char *)user forEntry:(PWFileEntry *)inRec;

// additional data helpers
-(void)setFilePath:(char *)inOutPath maxPathSize:(size_t)inPathMax forEntry:(PWFileEntry *)inRec;
-(BOOL)removeKey:(CFStringRef)inKey fromAdditionalDataFile:(const char *)inFilePath;

-(int)getUserIDFromName:(const char *)inName anyUser:(BOOL)inAnyUser maxBuffSize:(long)inMaxBuffSize pwsID:(char *)outID;
-(int)getUserRecordFromPrincipal:(const char *)inPrincipal record:(PWFileEntry *)inOutUserRec;
-(char *)getLDAPSearchBase;
-(int)getAccountIDFromLDAP:(const char *)inUID slotID:(char *)outID;
-(int)shouldTryLDAP;
-(void)requireNewPasswordForAllAccounts:(BOOL)inRequireNew;
-(void)kerberizeOrRequireNewPasswordForAllAccounts;

-(int)addPasswordForUser:(const char *)inUser password:(const char *)inPassword pwsID:(char *)outPasswordRef;
-(int)newPasswordSlot:(const char *)inUser
	password:(const char *)inPassword
	pwsID:(char *)outPasswordRef
	pwsRec:(PWFileEntry *)inOutUserRec;

// replication
-(int)getSyncTime:(time_t *)outSyncTime fromSyncFile:(const char *)inSyncFile;
-(int)makeSyncFile:(const char *)inFileName
	afterDate:(time_t)inAfterDate
	timeSkew:(long)inTimeSkew
	numRecordsUpdated:(long *)outNumRecordsUpdated
	kerberosRecordLimit:(unsigned int)inKerberosRecordLimit
	kerberosOmitted:(BOOL *)outKerberosOmitted
	passwordServerOmitted:(BOOL *)outPasswordServerOmitted;
-(int)processSyncFile:(const char *)inSyncFile
	timeSkew:(long)inTimeSkew
	numAccepted:(long *)outNumAccepted
	numTrumped:(long *)outNumTrumped;
-(int)mergeHeader:(PWFileHeader *)inRemoteHeader timeSkew:(long)inTimeSkew;
-(SyncStatus)mergeSlot:(PWFileEntry *)inRemoteRecord
	timeSkew:(long)inTimeSkew
	accepted:(BOOL *)outAccepted;
-(SyncStatus)mergeKerberosRecord:(unsigned char *)inRemoteKerberosRecordData
	recordDataLen:(int)inRemoteKerberosRecordDataLen
	withList:(PWSFKerberosPrincipalList *)inKerbList
	modList:(PWSFKerberosPrincipalList *)inOutModKerbList
	timeSkew:(long)inTimeSkew
	accepted:(BOOL *)outAccepted;

// policy testers
-(int)disableStatus:(PWFileEntry *)inOutPasswordRec changed:(BOOL *)outChanged reason:(PWDisableReasonCode *)outReasonCode;
-(int)changePasswordStatus:(PWFileEntry *)inPasswordRec;
-(int)requiredCharacterStatus:(PWFileEntry *)inPasswordRec forPassword:(const char *)inPassword;
-(int)passwordHistoryStatus:(PWFileEntry *)inPasswordRec password:(const char *)inPassword;
-(int)reenableStatus:(PWFileEntry *)inPasswordRec enableMinutes:(unsigned long)inGlobalReenableMinutes;

// history accessors
-(int)checkHistory:(PWFileEntry *)inPasswordRec count:(int)inMaxHistory password:(const char *)inPassword;
-(int)putInHistory:(PWFileEntry *)inPasswordRec passwordHash:(const char *)inPasswordHash;
-(int)getHistory:(PWFileEntry *)inPasswordRec data:(char *)outHistoryData;
-(int)addHistory:(PWFileEntry *)inPasswordRec;
-(int)saveHistory:(PWFileEntry *)inPasswordRec data:(char *)inOutHistoryData;
-(off_t)historySlotToOffset:(unsigned long)inSlotNumber;

// misc
-(SyncPriority)syncPriority;
-(void)setShouldSyncNow:(BOOL)onOff;
-(void)setForceSync:(BOOL)onOff;
-(void)setTestSpillBucket:(BOOL)onOff;
-(uint32_t)kerberosCacheLimit;
-(void)setKerberosCacheLimit:(uint32_t)inLimit;

@end

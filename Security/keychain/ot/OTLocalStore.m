/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
#if OCTAGON

#import <Foundation/Foundation.h>
#import <utilities/debugging.h>
#import <Prequelite/Prequelite.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <utilities/SecFileLocations.h>
#import "keychain/ot/OTDefines.h"
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#include <MobileGestalt.h>
#else
#include <AppleSystemInfo/AppleSystemInfo.h>
#endif

#import "OTLocalStore.h"
#import "OTBottledPeerSigned.h"

static NSString* const contextSchema = @"create table if not exists context (contextIDAndDSID text primary key, contextID text, accountDSID text, contextName text, zoneCreated boolean, subscribedToChanges boolean, changeToken blob, egoPeerID text, egoPeerCreationDate date, recoverySigningSPKI text, recoveryEncryptionSPKI text);";

static NSString* const bottledPeerSchema = @"create table if not exists bp (bottledPeerRecordID text primary key, contextIDAndDSID text, escrowRecordID text, peerID text, spID text, bottle text, escrowSigningSPKI text, peerSigningSPKI text, signatureUsingEscrow text, signatureUsingPeerKey text, encodedRecord text, launched text);";

static const NSInteger user_version = 0;

/* Octagon Trust Local Context Record Constants  */
static NSString* OTCKRecordContextAndDSID = @"contextIDAndDSID";
static NSString* OTCKRecordContextID = @"contextID";
static NSString* OTCKRecordDSID = @"accountDSID";
static NSString* OTCKRecordContextName = @"contextName";
static NSString* OTCKRecordZoneCreated = @"zoneCreated";
static NSString* OTCKRecordSubscribedToChanges = @"subscribedToChanges";
static NSString* OTCKRecordChangeToken = @"changeToken";
static NSString* OTCKRecordEgoPeerID = @"egoPeerID";
static NSString* OTCKRecordEgoPeerCreationDate = @"egoPeerCreationDate";
static NSString* OTCKRecordRecoverySigningSPKI = @"recoverySigningSPKI";
static NSString* OTCKRecordRecoveryEncryptionSPKI = @"recoveryEncryptionSPKI";
static NSString* OTCKRecordBottledPeerTableEntry = @"bottledPeer";

/* Octagon Trust Local Peer Record  */
static NSString* OTCKRecordPeerID = @"peerID";
static NSString* OTCKRecordPermanentInfo = @"permanentInfo";
static NSString* OTCKRecordStableInfo = @"stableInfo";
static NSString* OTCKRecordDynamicInfo = @"dynamicInfo";
static NSString* OTCKRecordRecoveryVoucher = @"recoveryVoucher";
static NSString* OTCKRecordIsEgoPeer = @"isEgoPeer";

/* Octagon Trust BottledPeerSchema  */
static NSString* OTCKRecordEscrowRecordID = @"escrowRecordID";
static NSString* OTCKRecordRecordID = @"bottledPeerRecordID";
static NSString* OTCKRecordSPID = @"spID";
static NSString* OTCKRecordBottle = @"bottle";
static NSString* OTCKRecordEscrowSigningSPKI = @"escrowSigningSPKI";
static NSString* OTCKRecordPeerSigningSPKI = @"peerSigningSPKI";
static NSString* OTCKRecordSignatureFromEscrow = @"signatureUsingEscrow";
static NSString* OTCKRecordSignatureFromPeerKey = @"signatureUsingPeerKey";
static NSString* OTCKRecordEncodedRecord = @"encodedRecord";
static NSString* OTCKRecordLaunched = @"launched";

/* Octagon Table Names */
static NSString* const contextTable = @"context";
static NSString* const peerTable = @"peer";
static NSString* const bottledPeerTable = @"bp";

/* Octagon Trust Schemas */
static NSString* const octagonZoctagonErrorDomainoneName = @"OctagonTrustZone";

/* Octagon Cloud Kit defines */
static NSString* OTCKContainerName = @"com.apple.security.keychain";
static NSString* OTCKZoneName = @"OctagonTrust";
static NSString* OTCKRecordName = @"bp-";
static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";

static NSArray* _Nullable selectAll(PQLResultSet *rs, Class class)
{
    NSMutableArray *arr = [NSMutableArray array];
    for (id o in [rs enumerateObjectsOfClass:class]) {
        [arr addObject:o];
    }
    if (rs.error) {
        return nil;
    }
    return arr;
}
#define selectArrays(db, sql, ...) \
selectAll([db fetch:sql, ##__VA_ARGS__], [NSArray class])

#define selectDictionaries(db, sql, ...) \
selectAll([db fetch:sql, ##__VA_ARGS__], [NSDictionary class])


@interface NSDictionary (PQLResultSetInitializer) <PQLResultSetInitializer>
@end
@implementation NSDictionary (PQLResultSetInitializer)
- (instancetype)initFromPQLResultSet:(PQLResultSet *)rs
                               error:(NSError **)error
{
    NSUInteger cols = rs.columns;
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] initWithCapacity:cols];

    for (NSUInteger i = 0; i < cols; i++) {
        id obj = rs[i];
        if (obj) {
            dict[[rs columnNameAtIndex:(int)i]] = obj;
        }
    }

    return [self initWithDictionary:dict];
}
@end


@implementation OTLocalStore

-(instancetype) initWithContextID:(NSString*)contextID dsid:(NSString*)dsid path:(nullable NSString*)path error:(NSError**)error
{
    self = [super init];
    if(self){
        if (!path) {
            NSURL* urlPath = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"otdb.db");
            path = [urlPath path];
        }
        _dbPath = [path copy];
        _pDB = [[PQLConnection alloc] init];
        _contextID = [contextID copy];
        _dsid = [dsid copy];
        _serialQ = dispatch_queue_create("com.apple.security.ot.db", DISPATCH_QUEUE_SERIAL);

        NSError* localError = nil;
        if(![self openDBWithError:&localError])
        {
            secerror("octagon: could not open db: %@", localError);
            if(error){
                *error = localError;
            }
            return nil;
        }
    }
    return self;
}

- (BOOL) createDirectoryAtPath:(NSString*)path error:(NSError **)error
{
    BOOL success = YES;
    NSError *localError;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    if (![fileManager createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&localError]) {
        if (![localError.domain isEqualToString:NSCocoaErrorDomain] || localError.code != NSFileWriteFileExistsError) {
            success = NO;
            if(error){
                *error = localError;
            }
        }
    }
    
#if TARGET_OS_IPHONE
    if (success) {
        NSDictionary *attributes = [fileManager attributesOfItemAtPath:path error:&localError];
        if (![attributes[NSFileProtectionKey] isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication]) {
            [fileManager setAttributes:@{ NSFileProtectionKey: NSFileProtectionCompleteUntilFirstUserAuthentication }
                          ofItemAtPath:path error:&localError];
        }
    }
#endif
    if (!success) {
        if (error) *error = localError;
    }
    return success;
}

-(BOOL)openDBWithError:(NSError**)error
{
    BOOL result = NO;
    NSError *localError = nil;

    if(!(result = [_pDB openAtURL:[NSURL URLWithString:_dbPath] sharedCache:NO error:&localError])){
        secerror("octagon: could not open db: %@", localError);
        if(error){
            *error = localError;
        }
        return NO;
    }
    if(![_pDB execute:bottledPeerSchema]){
        secerror("octagon: could not create bottled peer schema");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEntropyCreationFailure userInfo:@{NSLocalizedDescriptionKey: @"could not create bottled peer schema"}];
        }
        result = NO;
    }
    if(![_pDB execute:contextSchema]){
        secerror("octagon: could not create contextschema");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"could not create context schema"}];
        }
        result = NO;
    }
    if(![_pDB setupPragmas]){
        secerror("octagon: could not set up db pragmas");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"could not set up db pragmas"}];
        }
        result = NO;
    }
    if(![_pDB setUserVersion:user_version]){
        secerror("octagon: could not set version");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"could not set version"}];
        }
        result = NO;
    }
    return result;
}

-(BOOL)closeDBWithError:(NSError**)error
{
    BOOL result = NO;
    NSError *localError = nil;

    if(!(result =[_pDB close:&localError])){
        secerror("octagon: could not close db: %@", localError);
        if(error){
            *error = localError;
        }
    }
    return result;
}

-(BOOL)isProposedColumnNameInTable:(NSString*)proposedColumnName tableName:(NSString*)tableName
{
    BOOL result = NO;
    
    if([tableName isEqualToString:contextTable])
    {
        if([proposedColumnName isEqualToString:OTCKRecordContextAndDSID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordContextID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordDSID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordContextName]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordZoneCreated]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordSubscribedToChanges]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordChangeToken]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordEgoPeerID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordEgoPeerCreationDate]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordRecoverySigningSPKI]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordRecoveryEncryptionSPKI]){
            result = YES;
        }
        else{
            secerror("octagon: column name unknown: %@", proposedColumnName);
        }
    }
    else if([tableName isEqualToString:peerTable]){ //not using yet!
        result = NO;
        secerror("octagon: not using this table yet!");
    }
    else if([tableName isEqualToString:bottledPeerTable])
    {
        if([proposedColumnName isEqualToString:OTCKRecordContextAndDSID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordRecordID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordEscrowRecordID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordSPID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordPeerID]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordBottle]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordSignatureFromEscrow]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordSignatureFromPeerKey]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordEncodedRecord]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordLaunched]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordPeerSigningSPKI]){
            result = YES;
        }
        else if([proposedColumnName isEqualToString:OTCKRecordEscrowSigningSPKI]){
            result = YES;
        }
        else{
            secerror("octagon: column name unknown: %@", proposedColumnName);
        }
    }
    else{
        secerror("octagon: table name unknown: %@", tableName);
    }
    return result;
}

/////
//      Local Context Record
/////
-(OTContextRecord* _Nullable)readLocalContextRecordForContextIDAndDSID:(NSString*)contextAndDSID error:(NSError**)error
{
    OTContextRecord* record = [[OTContextRecord alloc]init];
    NSDictionary* attributes = nil;
    NSArray *selectArray = nil;
    
    selectArray = selectDictionaries(_pDB, @"SELECT * from context WHERE contextIDAndDSID == %@;", PQLName(contextAndDSID));
    if(selectArray && [selectArray count] > 0){
        attributes = [selectArray objectAtIndex:0];
    }
    if(attributes && [attributes count] > 0){
        record.contextID = attributes[OTCKRecordContextID];
        record.dsid = attributes[OTCKRecordDSID];
        record.contextName = attributes[OTCKRecordContextName];
        record.zoneCreated = (BOOL)attributes[OTCKRecordZoneCreated];
        record.subscribedToChanges = (BOOL)attributes[OTCKRecordSubscribedToChanges];
        record.changeToken = attributes[OTCKRecordChangeToken];
        record.egoPeerID = attributes[OTCKRecordEgoPeerID];
        record.egoPeerCreationDate = attributes[OTCKRecordEgoPeerCreationDate];
        record.recoverySigningSPKI = dataFromBase64(attributes[OTCKRecordRecoverySigningSPKI]);
        record.recoveryEncryptionSPKI = dataFromBase64(attributes[OTCKRecordRecoveryEncryptionSPKI]);
    }
    else{
        secerror("octagon: no context attributes found");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"no context attributes found"}];
        }
    }

    return record;
}

-(BOOL)initializeContextTable:(NSString*)contextID dsid:(NSString*)dsid error:(NSError**)error
{
    BOOL result = NO;
    NSError* localError = nil;
    NSString* contextName = nil;
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
    contextName = (__bridge_transfer NSString *)MGCopyAnswer(kMGQUserAssignedDeviceName, NULL);
#else
    contextName = (__bridge_transfer NSString *)SCDynamicStoreCopyComputerName(NULL, NULL);
#endif

    NSDictionary *contextAttributes = @{
                                        OTCKRecordContextAndDSID : [NSString stringWithFormat:@"%@-%@", contextID, dsid],
                                        OTCKRecordContextID : contextID,
                                        OTCKRecordDSID  :   dsid,
                                        OTCKRecordContextName : contextName,
                                        OTCKRecordZoneCreated : @(NO),
                                        OTCKRecordSubscribedToChanges : @(NO),
                                        OTCKRecordChangeToken : [NSData data],
                                        OTCKRecordEgoPeerID : @"ego peer id",
                                        OTCKRecordEgoPeerCreationDate : [NSDate date],
                                        OTCKRecordRecoverySigningSPKI : [NSData data],
                                        OTCKRecordRecoveryEncryptionSPKI : [NSData data]};

    result = [self insertLocalContextRecord:contextAttributes error:&localError];
    if(!result || localError != nil){
        secerror("octagon: context table init failed: %@", localError);
        if(error){
            *error = localError;
        }
    }
    return result;
}

-(BOOL)insertLocalContextRecord:(NSDictionary*)attributes error:(NSError**)error
{
    BOOL result = NO;

    NSString* dsidAndContext = [NSString stringWithFormat:@"%@-%@", attributes[OTCKRecordContextID], attributes[OTCKRecordDSID]];
    result = [_pDB execute:@"insert into context (contextIDAndDSID, contextID, accountDSID, contextName, zoneCreated, subscribedToChanges, changeToken, egoPeerID, egoPeerCreationDate, recoverySigningSPKI, recoveryEncryptionSPKI) values (%@,%@,%@,%@,%@,%@,%@,%@,%@,%@,%@)",
              dsidAndContext, attributes[OTCKRecordContextID], attributes[OTCKRecordDSID], attributes[OTCKRecordContextName], attributes[OTCKRecordZoneCreated],
              attributes[OTCKRecordSubscribedToChanges], attributes[OTCKRecordChangeToken],
              attributes[OTCKRecordEgoPeerID], attributes[OTCKRecordEgoPeerCreationDate],
              [attributes[OTCKRecordRecoverySigningSPKI] base64EncodedStringWithOptions:0], [attributes[OTCKRecordRecoveryEncryptionSPKI] base64EncodedStringWithOptions:0]];
    
    
    if(_pDB.lastError){
        secerror("octagon: failed to insert local context: %@", _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

-(BOOL)updateLocalContextRecordRowWithContextID:(NSString*)contextIDAndDSID columnName:(NSString*)columnName newValue:(void*)newValue error:(NSError**)error
{
    BOOL result = NO;
    if([self isProposedColumnNameInTable:columnName tableName:contextTable]){
        result = [_pDB execute:@"update context set %@ = %@ where contextIDAndDSID == %@",
                  PQLName(columnName), newValue, PQLName(_contextID)];
        if(!result && error){
            secerror("octagon: error updating table: %@", _pDB.lastError);
            *error = _pDB.lastError;
        }
    }
    else{
        secerror("octagon: failed to update local context record: %@", _pDB.lastError);

        if(error != nil){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorNoColumn userInfo:nil];
        }
    }
    return result;
}

-(BOOL) deleteLocalContext:(NSString*)contextIDAndDSID error:(NSError**)error
{
    BOOL result = NO;
    secnotice("octagon", "deleting local context: %@", contextIDAndDSID);
    
    result = [_pDB execute:@"delete from context where contextIDAndDSID == %@",
             PQLName(contextIDAndDSID)];
    
    if(!result){
        secerror("octagon: error updating table: %@", _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

-(BOOL) deleteAllContexts:(NSError**)error
{
    BOOL result = NO;
    secnotice("octagon", "deleting all local context");
    
    result = [_pDB execute:@"delete from context"];
    
    if(!result){
        secerror("octagon: error updating table: %@", _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

/////
//      Local Bottled Peer Record
/////

- (BOOL) insertBottledPeerRecord:(OTBottledPeerRecord *)rec
                  escrowRecordID:(NSString *)escrowRecordID
                           error:(NSError**)error
{
    BOOL result;

    result = [_pDB execute:@"insert or replace into bp (bottledPeerRecordID, contextIDAndDSID, escrowRecordID, peerID, spID, bottle, escrowSigningSPKI, peerSigningSPKI, signatureUsingEscrow, signatureUsingPeerKey, encodedRecord, launched) values (%@,%@,%@,%@,%@,%@,%@,%@,%@,%@,%@,%@)",
              rec.recordName,
              [NSString stringWithFormat:@"%@-%@", self.contextID, self.dsid],
              escrowRecordID,
              rec.peerID,
              rec.spID,
              [rec.bottle base64EncodedStringWithOptions:0],
              [rec.escrowedSigningSPKI base64EncodedStringWithOptions:0],
              [rec.peerSigningSPKI base64EncodedStringWithOptions:0],
              [rec.signatureUsingEscrowKey base64EncodedStringWithOptions:0],
              [rec.signatureUsingPeerKey base64EncodedStringWithOptions:0],
              [rec.encodedRecord base64EncodedStringWithOptions:0],
              rec.launched];

    if (!result) {
        secerror("octagon: error inserting bottled peer record: %@", _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

-(BOOL) removeAllBottledPeerRecords:(NSError**)error
{
    BOOL result = NO;
    
    result = [_pDB execute:@"DELETE from bp WHERE contextIDAndDSID == %@;", [NSString stringWithFormat:@"%@-%@", self.contextID, self.dsid]];
    
    if (!result) {
        secerror("octagon: error removing bottled peer records: %@", _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

-(BOOL) deleteBottledPeer:(NSString*) recordID
                            error:(NSError**)error
{
    BOOL result = NO;
    
    result = [_pDB execute:@"DELETE from bp WHERE contextIDAndDSID == %@ AND bottledPeerRecordID == %@;", [NSString stringWithFormat:@"%@-%@", self.contextID, self.dsid], recordID];
    
    if (!result) {
        secerror("octagon: error removing bottled peer record:%@, error: %@", recordID, _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

-(BOOL) deleteBottledPeersForContextAndDSID:(NSString*) contextIDAndDSID
                    error:(NSError**)error
{
    BOOL result = NO;
    
    result = [_pDB execute:@"DELETE from bp WHERE contextIDAndDSID == %@;", contextIDAndDSID];
    
    if (!result) {
        secerror("octagon: error removing bottled peer record:%@, error: %@", contextIDAndDSID, _pDB.lastError);
        if(error){
            *error = _pDB.lastError;
        }
    }
    return result;
}

- (nullable OTBottledPeerRecord *)readLocalBottledPeerRecordWithRecordID:(NSString *)recordID
                                                                         error:(NSError**)error
{
    NSArray *selectArray;
    
    selectArray = selectDictionaries(_pDB, @"SELECT * from bp WHERE contextIDAndDSID == %@ AND bottledPeerRecordID == %@;", [NSString stringWithFormat:@"%@-%@", self.contextID, self.dsid], recordID);
    if (!selectArray) {
        if (error) {
            secerror("octagon: failed to read local store entry for %@", recordID);
            *error = self.pDB.lastError;
        }
        return nil;
    }
    if ([selectArray count] > 1) {
        secerror("octagon: error multiple records exist in local store for %@", recordID);
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"error multiple records exist in local store"}];
        }
        return nil;
    }
    else if([selectArray count] == 0){
        secerror("octagon: record does not exist: %@", recordID);
        return nil;
    }
    NSDictionary *attributes = [selectArray objectAtIndex:0];

    OTBottledPeerRecord *rec = [[OTBottledPeerRecord alloc] init];
    rec.escrowRecordID = attributes[OTCKRecordEscrowRecordID];
    rec.peerID = attributes[OTCKRecordPeerID];
    rec.spID = attributes[OTCKRecordSPID];
    rec.bottle = dataFromBase64(attributes[OTCKRecordBottle]);
    rec.escrowedSigningSPKI = dataFromBase64(attributes[OTCKRecordEscrowSigningSPKI]);
    rec.peerSigningSPKI = dataFromBase64(attributes[OTCKRecordPeerSigningSPKI]);
    rec.signatureUsingEscrowKey = dataFromBase64(attributes[OTCKRecordSignatureFromEscrow]);
    rec.signatureUsingPeerKey = dataFromBase64(attributes[OTCKRecordSignatureFromPeerKey]);
    rec.encodedRecord = dataFromBase64(attributes[OTCKRecordEncodedRecord]);
    rec.launched = attributes[OTCKRecordLaunched];
    return rec;
}

- (NSMutableArray<OTBottledPeerRecord *>*) convertResultsToBottles:(NSArray*) selectArray
{
    NSMutableArray *arrayOfBottleRecords = [NSMutableArray<OTBottledPeerRecord *> array];
    for(NSDictionary* bottle in selectArray){
        OTBottledPeerRecord *rec = [[OTBottledPeerRecord alloc] init];
        rec.escrowRecordID = bottle[OTCKRecordEscrowRecordID];
        rec.peerID = bottle[OTCKRecordPeerID];
        rec.spID = bottle[OTCKRecordSPID];
        rec.bottle = dataFromBase64(bottle[OTCKRecordBottle]);
        rec.escrowedSigningSPKI = dataFromBase64(bottle[OTCKRecordEscrowSigningSPKI]);
        rec.peerSigningSPKI = dataFromBase64(bottle[OTCKRecordPeerSigningSPKI]);
        rec.signatureUsingEscrowKey = dataFromBase64(bottle[OTCKRecordSignatureFromEscrow]);
        rec.signatureUsingPeerKey = dataFromBase64(bottle[OTCKRecordSignatureFromPeerKey]);
        rec.encodedRecord = dataFromBase64(bottle[OTCKRecordEncodedRecord]);
        rec.launched = bottle[OTCKRecordLaunched];
        
        [arrayOfBottleRecords addObject:rec];
    }
    return arrayOfBottleRecords;
}

- (nullable NSArray<OTBottledPeerRecord*>*) readAllLocalBottledPeerRecords:(NSError**)error
{
    NSArray *selectArray;
    
    selectArray = selectDictionaries(_pDB, @"SELECT * from bp where contextIDAndDSID == %@;", [NSString stringWithFormat:@"%@-%@", self.contextID, self.dsid]);
    if (!selectArray) {
        if (error) {
            secerror("octagon: failed to read local store entries");
            *error = self.pDB.lastError;
        }
        return nil;
    }
    if ([selectArray count] == 0) {
        secerror("octagon: there are no bottled peer entries in local store");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"there are no bottled peer entries in local store"}];
        }
        return nil;
    }
    
    return [self convertResultsToBottles:selectArray];
}

- (nullable NSArray<OTBottledPeerRecord *>*) readLocalBottledPeerRecordsWithMatchingPeerID:(NSString*)peerID error:(NSError**)error
{
    NSArray *selectArray;
    
    selectArray = selectDictionaries(_pDB, @"SELECT * from bp where spID == %@;", peerID);
    if (!selectArray) {
        if (error) {
            secerror("octagon: failed to read local store entries");
            *error = self.pDB.lastError;
        }
        return nil;
    }
    if ([selectArray count] == 0) {
        secerror("octagon: there are no bottled peer entries in local store");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTLocalStore userInfo:@{NSLocalizedDescriptionKey: @"there are no bottled peer entries in local store"}];
        }
        return nil;
    }
    
    return [self convertResultsToBottles:selectArray];
}

static NSData * _Nullable dataFromBase64(NSString * _Nullable base64)
{
    if (base64 && [base64 length] > 0) {
        return [[NSData alloc] initWithBase64EncodedString:base64 options:0];
    }
    return nil;
}

@end
#endif

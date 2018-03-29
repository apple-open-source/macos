/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import "CKKSManifest.h"
#import "CKKSManifestLeafRecord.h"
#import "CKKS.h"
#import "CKKSItem.h"
#import "CKKSCurrentItemPointer.h"
#import "utilities/der_plist.h"
#import <securityd/SOSCloudCircleServer.h>
#import <securityd/SecItemServer.h>
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <CloudKit/CloudKit.h>

NSString* const CKKSManifestZoneKey = @"zone";
NSString* const CKKSManifestSignerIDKey = @"signerID";
NSString* const CKKSManifestGenCountKey = @"gencount";

static NSString* const CKKSManifestDigestKey = @"CKKSManifestDigestKey";
static NSString* const CKKSManifestPeerManifestsKey = @"CKKSManifestPeerManifestsKey";
static NSString* const CKKSManifestCurrentItemsKey = @"CKKSManifestCurrentItemsKey";
static NSString* const CKKSManifestGenerationCountKey = @"CKKSManifestGenerationCountKey";
static NSString* const CKKSManifestSchemaVersionKey = @"CKKSManifestSchemaVersionKey";

static NSString* const CKKSManifestEC384SignatureKey = @"CKKSManifestEC384SignatureKey";

static NSString* const CKKSManifestErrorDomain = @"CKKSManifestErrorDomain";

#define NUM_MANIFEST_LEAF_RECORDS 72
#define BITS_PER_UUID_CHAR 36

static CKKSManifestInjectionPointHelper* __egoHelper = nil;
static NSMutableDictionary<NSString*, CKKSManifestInjectionPointHelper*>* __helpersDict = nil;
static BOOL __ignoreChanges = NO;

enum {
    CKKSManifestErrorInvalidDigest = 1,
    CKKSManifestErrorVerifyingKeyNotFound = 2,
    CKKSManifestErrorManifestGenerationFailed = 3,
    CKKSManifestErrorCurrentItemUUIDNotFound = 4
};

typedef NS_ENUM(NSInteger, CKKSManifestFieldType) {
    CKKSManifestFieldTypeStringRaw = 0,
    CKKSManifestFieldTypeStringBase64Encoded = 1,
    CKKSManifestFieldTypeDataAsBase64String = 2,
    CKKSManifestFieldTypeNumber = 3,
    CKKSManifestFieldTypeArrayRaw = 4,
    CKKSManifestFieldTypeArrayAsDERBase64String = 5,
    CKKSManifestFieldTypeDictionaryAsDERBase64String = 6
};

@interface CKKSAccountInfo : NSObject {
    SFECKeyPair* _signingKey;
    NSDictionary* _peerVerifyingKeys;
    NSString* _egoPeerID;
    NSError* _setupError;
}

@property SFECKeyPair* signingKey;
@property NSDictionary* peerVerifyingKeys;
@property NSString* egoPeerID;
@property NSError* setupError;
@end

static NSDictionary* __thisBuildsSchema = nil;
static CKKSAccountInfo* s_accountInfo = nil;

@interface CKKSManifest () {
@package
    NSData* _derData;
    NSData* _digestValue;
    NSUInteger _generationCount;
    NSString* _signerID;
    NSString* _zoneName;
    NSArray* _leafRecordIDs;
    NSArray* _peerManifestIDs;
    NSMutableDictionary* _currentItemsDict;
    NSDictionary* _futureData;
    NSDictionary* _signaturesDict;
    NSDictionary* _schema;
    CKKSManifestInjectionPointHelper* _helper;
}

@property (nonatomic, readonly) NSString* zoneName;
@property (nonatomic, readonly) NSArray<NSString*>* leafRecordIDs;
@property (nonatomic, readonly) NSArray<NSString*>* peerManifestIDs;
@property (nonatomic, readonly) NSDictionary* currentItems;
@property (nonatomic, readonly) NSDictionary* futureData;
@property (nonatomic, readonly) NSDictionary* signatures;
@property (nonatomic, readonly) NSDictionary* schema;

@property (nonatomic, readwrite) NSString* signerID;
@property (nonatomic) CKKSManifestInjectionPointHelper* helper;

+ (NSData*)digestValueForLeafRecords:(NSArray*)leafRecords;

- (void)clearDigest;

@end

@interface CKKSPendingManifest () {
    NSMutableArray* _committedLeafRecordIDs;
}

@property (nonatomic, readonly) NSArray<NSString*>* committedLeafRecordIDs;

@end

@interface CKKSEgoManifest () {
    NSArray* _leafRecords;
}

@property (class, readonly) CKKSManifestInjectionPointHelper* egoHelper;

@property (nonatomic, readwrite) NSDictionary* signatures;

@end

@interface CKKSManifestInjectionPointHelper ()

- (instancetype)initWithPeerID:(NSString*)peerID keyPair:(SFECKeyPair*)keyPair isEgoPeer:(BOOL)isEgoPeer;

- (void)performWithSigningKey:(void (^)(SFECKeyPair* _Nullable signingKey, NSError* _Nullable error))handler;
- (void)performWithEgoPeerID:(void (^)(NSString* _Nullable egoPeerID, NSError* _Nullable error))handler;
- (void)performWithPeerVerifyingKeys:(void (^)(NSDictionary<NSString*, SFECPublicKey*>* _Nullable peerKeys, NSError* _Nullable error))handler;

@end

static NSData* ManifestDERData(NSString* zone, NSData* digestValue, NSArray<NSString*>* peerManifestIDs, NSDictionary<NSString*, NSString*>* currentItems, NSUInteger generationCount, NSDictionary* futureFields, NSDictionary* schema, NSError** error)
{
    NSArray* sortedPeerManifestIDs = [peerManifestIDs sortedArrayUsingSelector:@selector(compare:)];

    NSMutableDictionary* manifestDict = [NSMutableDictionary dictionary];
    manifestDict[CKKSManifestDigestKey] = digestValue;
    manifestDict[CKKSManifestPeerManifestsKey] = sortedPeerManifestIDs;
    manifestDict[CKKSManifestCurrentItemsKey] = currentItems;
    manifestDict[CKKSManifestGenerationCountKey] = [NSNumber numberWithUnsignedInteger:generationCount];

    [futureFields enumerateKeysAndObjectsUsingBlock:^(NSString* futureKey, id futureValue, BOOL* stop) {
        CKKSManifestFieldType fieldType = [schema[futureKey] integerValue];
        if (fieldType == CKKSManifestFieldTypeStringRaw) {
            manifestDict[futureKey] = futureValue;
        }
        else if (fieldType == CKKSManifestFieldTypeStringBase64Encoded) {
            manifestDict[futureKey] = [[NSString alloc] initWithData:[[NSData alloc] initWithBase64EncodedString:futureValue options:0] encoding:NSUTF8StringEncoding];
        }
        else if (fieldType == CKKSManifestFieldTypeDataAsBase64String) {
            manifestDict[futureKey] = [[NSData alloc] initWithBase64EncodedString:futureValue options:0];
        }
        else if (fieldType == CKKSManifestFieldTypeNumber) {
            manifestDict[futureKey] = futureValue;
        }
        else if (fieldType == CKKSManifestFieldTypeArrayRaw) {
            manifestDict[futureKey] = futureValue;
        }
        else if (fieldType == CKKSManifestFieldTypeArrayAsDERBase64String) {
            manifestDict[futureKey] = (__bridge_transfer NSArray*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)[[NSData alloc] initWithBase64EncodedData:futureValue options:0], 0, NULL, NULL);
        }
        else if (fieldType == CKKSManifestFieldTypeDictionaryAsDERBase64String) {
            manifestDict[futureKey] = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)[[NSData alloc] initWithBase64EncodedData:futureValue options:0], 0, NULL, NULL);
        }
        else {
            ckkserrorwithzonename("ckksmanifest", zone, "unrecognized field type in future schema: %ld", (long)fieldType);
        }
    }];
    
    CFErrorRef cfError = NULL;
    NSData* derData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)manifestDict, &cfError);
    if (cfError) {
        ckkserrorwithzonename("ckksmanifest", zone, "error creating manifest der data: %@", cfError);
        if (error) {
            *error = (__bridge_transfer NSError*)cfError;
        }        
        return nil;
    }
    
    return derData;
}

static NSUInteger LeafBucketIndexForUUID(NSString* uuid)
{
    NSInteger prefixIntegerValue = 0;
    for (NSInteger characterIndex = 0; characterIndex * BITS_PER_UUID_CHAR < NUM_MANIFEST_LEAF_RECORDS; characterIndex++) {
        prefixIntegerValue += [uuid characterAtIndex:characterIndex];
    }
    
    return prefixIntegerValue % NUM_MANIFEST_LEAF_RECORDS;
}

@implementation CKKSManifest

@synthesize zoneName = _zoneName;
@synthesize leafRecordIDs = _leafRecordIDs;
@synthesize peerManifestIDs = _peerManifestIDs;
@synthesize currentItems = _currentItemsDict;
@synthesize futureData = _futureData;
@synthesize signatures = _signaturesDict;
@synthesize signerID = _signerID;
@synthesize schema = _schema;
@synthesize helper = _helper;

+ (void)initialize
{
    if (self == [CKKSManifest class]) {
        __thisBuildsSchema = @{ CKKSManifestSchemaVersionKey : @(1),
                                SecCKRecordManifestDigestValueKey : @(CKKSManifestFieldTypeDataAsBase64String),
                                SecCKRecordManifestGenerationCountKey : @(CKKSManifestFieldTypeNumber),
                                SecCKRecordManifestLeafRecordIDsKey : @(CKKSManifestFieldTypeArrayRaw),
                                SecCKRecordManifestPeerManifestRecordIDsKey : @(CKKSManifestFieldTypeArrayRaw),
                                SecCKRecordManifestCurrentItemsKey : @(CKKSManifestFieldTypeDictionaryAsDERBase64String),
                                SecCKRecordManifestSignaturesKey : @(CKKSManifestFieldTypeDictionaryAsDERBase64String),
                                SecCKRecordManifestSignerIDKey : @(CKKSManifestFieldTypeStringRaw),
                                SecCKRecordManifestSchemaKey : @(CKKSManifestFieldTypeDictionaryAsDERBase64String) };
    }
}

+ (void)loadDefaults
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary* systemDefaults = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle bundleWithPath:@"/System/Library/Frameworks/Security.framework"] pathForResource:@"CKKSLogging" ofType:@"plist"]];
        bool shouldSync = !![[systemDefaults valueForKey:@"SyncManifests"] boolValue];
        bool shouldEnforce = !![[systemDefaults valueForKey:@"EnforceManifests"] boolValue];

        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SecCKKSUserDefaultsSuite];
        bool userDefaultsShouldSync = !![defaults boolForKey:@"SyncManifests"];
        bool userDefaultsShouldEnforce = !![defaults boolForKey:@"EnforceManifests"];
        shouldSync |= userDefaultsShouldSync;
        shouldEnforce |= userDefaultsShouldEnforce;

        if(shouldSync) {
            SecCKKSEnableSyncManifests();
        }
        if(shouldEnforce) {
            SecCKKSEnableEnforceManifests();
        }
    });
}

+ (bool)shouldSyncManifests
{
    [self loadDefaults];
    return SecCKKSSyncManifests();
}

+ (bool)shouldEnforceManifests
{
    [self loadDefaults];
    return SecCKKSEnforceManifests();
}

+ (void)performWithAccountInfo:(void (^)(void))action
{
    CKKSAccountInfo* accountInfo = [[CKKSAccountInfo alloc] init];

    [[CKKSEgoManifest egoHelper] performWithSigningKey:^(SFECKeyPair* signingKey, NSError* error) {
        accountInfo.signingKey = signingKey;
        if(error) {
            secerror("ckksmanifest: cannot get signing key from account: %@", error);
            if(accountInfo.setupError == nil) {
                accountInfo.setupError = error;
            }
        }
    }];

    [[CKKSEgoManifest egoHelper] performWithEgoPeerID:^(NSString* egoPeerID, NSError* error) {
        accountInfo.egoPeerID = egoPeerID;

        if(error) {
            secerror("ckksmanifest: cannot get ego peer ID from account: %@", error);
            if(accountInfo.setupError == nil) {
                accountInfo.setupError = error;
            }
        }
    }];

    [[CKKSEgoManifest egoHelper] performWithPeerVerifyingKeys:^(NSDictionary<NSString*, SFECPublicKey*>* peerKeys, NSError* error) {
        accountInfo.peerVerifyingKeys = peerKeys;
        if(error) {
            secerror("ckksmanifest: cannot get peer keys from account: %@", error);
            if(accountInfo.setupError == nil) {
                accountInfo.setupError = error;
            }
        }
    }];

    s_accountInfo = accountInfo;

    action();

    s_accountInfo = nil;
}

+ (nullable instancetype)tryFromDatabaseWhere:(NSDictionary*)whereDict error:(NSError* __autoreleasing *)error
{
    CKKSManifest* manifest = [super tryFromDatabaseWhere:whereDict error:error];
    manifest.helper = __helpersDict[manifest.signerID];
    return manifest;
}

+ (nullable instancetype)manifestForZone:(NSString*)zone peerID:(NSString*)peerID error:(NSError**)error
{
    NSDictionary* databaseWhereClause = @{ @"ckzone" : zone, @"signerID" : peerID };
    return [self tryFromDatabaseWhere:databaseWhereClause error:error];
}

+ (nullable instancetype)manifestForRecordName:(NSString*)recordName error:(NSError**)error
{
    return [self tryFromDatabaseWhere:[self whereClauseForRecordName:recordName] error:error];
}

+ (nullable instancetype)latestTrustedManifestForZone:(NSString*)zone error:(NSError**)error
{
    NSDictionary* databaseWhereClause = @{ @"ckzone" : zone };
    NSArray* manifests = [[self allWhere:databaseWhereClause error:error] sortedArrayUsingComparator:^NSComparisonResult(CKKSManifest* _Nonnull firstManifest, CKKSManifest* _Nonnull secondManifest) {
        NSInteger firstGenerationCount = firstManifest.generationCount;
        NSInteger secondGenerationCount = secondManifest.generationCount;
        
        if (firstGenerationCount > secondGenerationCount) {
            return NSOrderedDescending;
        }
        else if (firstGenerationCount < secondGenerationCount) {
            return NSOrderedAscending;
        }
        else {
            return NSOrderedSame;
        }
    }];
    
    __block CKKSManifest* result = nil;
    [manifests enumerateObjectsWithOptions:NSEnumerationReverse usingBlock:^(CKKSManifest* _Nonnull manifest, NSUInteger index, BOOL* _Nonnull stop) {
        if ([manifest validateWithError:nil]) {
            result = manifest;
            *stop = YES;
        }
    }];
    
    // TODO: add error for when we didn't find anything
    return result;
}

+ (SFEC_X962SigningOperation*)signatureOperation
{
    SFECKeySpecifier* keySpecifier = [[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384];
    return [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:keySpecifier digestOperation:[[SFSHA384DigestOperation alloc] init]];
}

+ (NSData*)digestForData:(NSData*)data
{
    return [SFSHA384DigestOperation digest:data];
}

+ (NSData*)digestValueForLeafRecords:(NSArray*)leafRecords
{
    NSMutableData* concatenatedLeafNodeDigestData = [[NSMutableData alloc] init];
    for (CKKSManifestLeafRecord* leafRecord in leafRecords) {
        [concatenatedLeafNodeDigestData appendData:leafRecord.digestValue];
    }
    
    return [self digestForData:concatenatedLeafNodeDigestData];
}

+ (instancetype)manifestForPendingManifest:(CKKSPendingManifest*)pendingManifest
{
    return [[self alloc] initWithDigestValue:pendingManifest.digestValue zone:pendingManifest.zoneName generationCount:pendingManifest.generationCount leafRecordIDs:pendingManifest.committedLeafRecordIDs peerManifestIDs:pendingManifest.peerManifestIDs currentItems:pendingManifest.currentItems futureData:pendingManifest.futureData signatures:pendingManifest.signatures signerID:pendingManifest.signerID schema:pendingManifest.schema encodedRecord:pendingManifest.encodedCKRecord];
}

+ (instancetype)fromDatabaseRow:(NSDictionary*)row
{
    NSString* digestBase64String = row[@"digest"];
    NSData* digest = [digestBase64String isKindOfClass:[NSString class]] ? [[NSData alloc] initWithBase64EncodedString:digestBase64String options:0] : nil;
    
    NSString* zone = row[@"ckzone"];
    NSUInteger generationCount = [row[@"gencount"] integerValue];
    NSString* signerID = row[@"signerID"];
    
    NSString* encodedRecordBase64String = row[@"ckrecord"];
    NSData* encodedRecord = [encodedRecordBase64String isKindOfClass:[NSString class]] ? [[NSData alloc] initWithBase64EncodedString:encodedRecordBase64String options:0] : nil;
    
    NSData* leafRecordIDData = [[NSData alloc] initWithBase64EncodedString:row[@"leafIDs"] options:0];
    NSArray* leafRecordIDs = (__bridge_transfer NSArray*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)leafRecordIDData, 0, NULL, NULL);
    if (![leafRecordIDs isKindOfClass:[NSArray class]]) {
        leafRecordIDs = [NSArray array];
    }
    
    NSData* peerManifestIDData = [[NSData alloc] initWithBase64EncodedString:row[@"peerManifests"] options:0];
    NSArray* peerManifestIDs = (__bridge_transfer NSArray*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)peerManifestIDData, 0, NULL, NULL);
    if (![peerManifestIDs isKindOfClass:[NSArray class]]) {
        peerManifestIDs = [NSArray array];
    }

    NSData* currentItemsData = [[NSData alloc] initWithBase64EncodedString:row[@"currentItems"] options:0];
    NSDictionary* currentItemsDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)currentItemsData, 0, NULL, NULL);
    if (![currentItemsDict isKindOfClass:[NSDictionary class]]) {
        currentItemsDict = [NSDictionary dictionary];
    }

    NSData* futureData = [[NSData alloc] initWithBase64EncodedString:row[@"futureData"] options:0];
    NSDictionary* futureDataDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)futureData, 0, NULL, NULL);
    if (![futureDataDict isKindOfClass:[NSDictionary class]]) {
        futureDataDict = [NSDictionary dictionary];
    }
    
    NSData* signaturesData = [[NSData alloc] initWithBase64EncodedString:row[@"signatures"] options:0];
    NSDictionary* signatures = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)signaturesData, 0, NULL, NULL);
    if (![signatures isKindOfClass:[NSDictionary class]]) {
        signatures = [NSDictionary dictionary];
    }

    NSData* schemaData = [[NSData alloc] initWithBase64EncodedString:row[@"schema"] options:0];
    NSDictionary* schemaDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)schemaData, 0, NULL, NULL);
    if (![schemaDict isKindOfClass:[NSDictionary class]]) {
        schemaDict = __thisBuildsSchema;
    }
    
    return [[self alloc] initWithDigestValue:digest zone:zone generationCount:generationCount leafRecordIDs:leafRecordIDs peerManifestIDs:peerManifestIDs currentItems:currentItemsDict futureData:futureDataDict signatures:signatures signerID:signerID schema:schemaDict encodedRecord:encodedRecord];
}

+ (NSArray<NSString*>*)sqlColumns
{
    return @[@"ckzone", @"gencount", @"digest", @"signatures", @"signerID", @"leafIDs", @"peerManifests", @"currentItems", @"futureData", @"schema", @"ckrecord"];
}

+ (NSString*)sqlTable
{
    return @"ckmanifest";
}

+ (NSUInteger)greatestKnownGenerationCount
{
    __block NSUInteger result = 0;
    [self queryMaxValueForField:@"gencount" inTable:self.sqlTable where:nil columns:@[@"gencount"] processRow:^(NSDictionary* row) {
        result = [row[@"gencount"] integerValue];
    }];
    
    [CKKSPendingManifest queryMaxValueForField:@"gencount" inTable:[CKKSPendingManifest sqlTable] where:nil columns:@[@"gencount"] processRow:^(NSDictionary* row) {
        result = MAX(result, (NSUInteger)[row[@"gencount"] integerValue]);
    }];
    
    return result;
}

- (instancetype)initWithDigestValue:(NSData*)digestValue zone:(NSString*)zone generationCount:(NSUInteger)generationCount leafRecordIDs:(NSArray<NSString*>*)leafRecordIDs peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems futureData:(NSDictionary*)futureData signatures:(NSDictionary*)signatures signerID:(NSString*)signerID schema:(NSDictionary*)schema helper:(CKKSManifestInjectionPointHelper*)helper
{
    if (self = [super init]) {
        _digestValue = digestValue;
        _zoneName = zone;
        _generationCount = generationCount;
        _leafRecordIDs = [leafRecordIDs copy];
        _currentItemsDict = currentItems ? [currentItems mutableCopy] : [NSMutableDictionary dictionary];
        _futureData = futureData ? [futureData copy] : @{};
        _signaturesDict = [signatures copy];
        _signerID = signerID;
        _schema = schema ? schema.copy : __thisBuildsSchema;
        
        if ([peerManifestIDs.firstObject isEqualToString:signerID]) {
            _peerManifestIDs = peerManifestIDs;
        }
        else {
            NSMutableArray* tempPeerManifests = [[NSMutableArray alloc] initWithObjects:signerID, nil];
            if (peerManifestIDs) {
                [tempPeerManifests addObjectsFromArray:peerManifestIDs];
            }
            _peerManifestIDs = tempPeerManifests;
        }
        
        _helper = helper ?: [self defaultHelperForSignerID:signerID];
        if (!_helper) {
            _helper = [[CKKSManifestInjectionPointHelper alloc] init];
        }
    }
    
    return self;
}

- (instancetype)initWithDigestValue:(NSData*)digestValue zone:(NSString*)zone generationCount:(NSUInteger)generationCount leafRecordIDs:(NSArray<NSString*>*)leafRecordIDs peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems futureData:(NSDictionary*)futureData signatures:(NSDictionary*)signatures signerID:(NSString*)signerID schema:(NSDictionary*)schema
{
    return [self initWithDigestValue:digestValue zone:zone generationCount:generationCount leafRecordIDs:leafRecordIDs peerManifestIDs:peerManifestIDs currentItems:currentItems futureData:futureData signatures:signatures signerID:signerID schema:schema helper:nil];
}

- (instancetype)initWithDigestValue:(NSData*)digestValue zone:(NSString*)zone generationCount:(NSUInteger)generationCount leafRecordIDs:(NSArray<NSString*>*)leafRecordIDs peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems futureData:(NSDictionary*)futureData signatures:(NSDictionary*)signatures signerID:(NSString*)signerID schema:(NSDictionary*)schema encodedRecord:(NSData*)encodedRecord
{
    if (self = [self initWithDigestValue:digestValue zone:zone generationCount:generationCount leafRecordIDs:leafRecordIDs peerManifestIDs:peerManifestIDs currentItems:currentItems futureData:futureData signatures:signatures signerID:signerID schema:schema]) {
        self.encodedCKRecord = encodedRecord;
    }
    
    return self;
}

- (instancetype)initWithCKRecord:(CKRecord*)record
{
    NSError* error = nil;
    NSString* signatureBase64String = record[SecCKRecordManifestSignaturesKey];
    if (!signatureBase64String) {
        ckkserror("ckksmanifest", record.recordID.zoneID, "attempt to create manifest from CKRecord that does not have signatures attached: %@", record);
        return nil;
    }
    NSData* signatureDERData = [[NSData alloc] initWithBase64EncodedString:signatureBase64String options:0];
    NSDictionary* signaturesDict = [self signatureDictFromDERData:signatureDERData error:&error];
    if (error) {
        ckkserror("ckksmanifest", record.recordID.zoneID, "failed to initialize CKKSManifest from CKRecord because we could not form a signature dict from the record: %@", record);
        return nil;
    }

    NSDictionary* schemaDict = nil;
    NSString* schemaBase64String = record[SecCKRecordManifestSchemaKey];
    if (schemaBase64String) {
        NSData* schemaData = [[NSData alloc] initWithBase64EncodedString:schemaBase64String options:0];
        schemaDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)schemaData, 0, NULL, NULL);
    }
    if (![schemaDict isKindOfClass:[NSDictionary class]]) {
        schemaDict = __thisBuildsSchema;
    }
    
    NSString* digestBase64String = record[SecCKRecordManifestDigestValueKey];
    if (!digestBase64String) {
        ckkserror("ckksmanifest", record.recordID.zoneID, "attempt to create manifest from CKRecord that does not have a digest attached: %@", record);
        return nil;
    }
    NSData* digestData = [[NSData alloc] initWithBase64EncodedString:digestBase64String options:0];
    
    if (self = [self initWithDigestValue:digestData
                                    zone:record.recordID.zoneID.zoneName
                         generationCount:[record[SecCKRecordManifestGenerationCountKey] unsignedIntegerValue]
                           leafRecordIDs:record[SecCKRecordManifestLeafRecordIDsKey]
                         peerManifestIDs:record[SecCKRecordManifestPeerManifestRecordIDsKey]
                            currentItems:record[SecCKRecordManifestCurrentItemsKey]
                              futureData:[self futureDataDictFromRecord:record withSchema:schemaDict]
                              signatures:signaturesDict
                                signerID:record[SecCKRecordManifestSignerIDKey]
                                  schema:schemaDict]) {
        self.storedCKRecord = record;
    }

    return self;
}

- (CKKSManifestInjectionPointHelper*)defaultHelperForSignerID:(NSString*)signerID
{
    return __helpersDict[signerID];
}

- (NSDictionary*)signatureDictFromDERData:(NSData*)derData error:(NSError**)error
{
    CFErrorRef localError = NULL;
    NSDictionary* signaturesDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)derData, 0, NULL, &localError);
    if (![signaturesDict isKindOfClass:[NSDictionary class]]) {
        ckkserror("ckksmanifest", self, "failed to decode signatures der dict with error: %@", localError);
        if (error) {
            *error = (__bridge_transfer NSError*)localError;
        }
    }
    
    return signaturesDict;
}

- (NSData*)derDataFromSignatureDict:(NSDictionary*)signatureDict error:(NSError**)error
{
    CFErrorRef localError = NULL;
    NSData* derData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)signatureDict, &localError);
    if (!derData) {
        ckkserror("ckksmanifest", self, "failed to encode signatures dict to der with error: %@", localError);
        if (error) {
            *error = (__bridge_transfer NSError*)localError;
        }
    }
    
    return derData;
}

- (NSArray*)peerManifestsFromDERData:(NSData*)derData error:(NSError**)error
{
    CFErrorRef localError = NULL;
    NSArray* peerManifests = (__bridge_transfer NSArray*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)derData, 0, NULL, &localError);
    if (![peerManifests isKindOfClass:[NSArray class]]) {
        ckkserror("ckksmanifest", self, "failed to decode peer manifests der array with error: %@", localError);
        if (error) {
            *error = (__bridge_transfer NSError*)localError;
        }
    }
    
    return peerManifests;
}

- (NSData*)derDataFromPeerManifests:(NSArray*)peerManifests error:(NSError**)error
{
    CFErrorRef localError = NULL;
    NSData* derData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)peerManifests, &localError);
    if (!derData) {
        ckkserror("ckksmanifest", self, "failed to encode peer manifests to der with error: %@", localError);
        if (error) {
            *error = (__bridge_transfer NSError*)localError;
        }
    }
    
    return derData;
}

- (NSDictionary*)futureDataDictFromRecord:(CKRecord*)record withSchema:(NSDictionary*)cloudSchema
{
    NSMutableDictionary* futureData = [NSMutableDictionary dictionary];
    [cloudSchema enumerateKeysAndObjectsUsingBlock:^(NSString* key, NSData* obj, BOOL* stop) {
        if (![__thisBuildsSchema.allKeys containsObject:key]) {
            futureData[key] = record[key];
        }
    }];

    return futureData;
}

- (BOOL)updateWithRecord:(CKRecord*)record error:(NSError**)error
{
    if ([CKKSManifestInjectionPointHelper ignoreChanges]) {
        return YES; // don't set off any alarms here - just pretend we did it
    }
    
    NSData* signatureDERData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestSignaturesKey] options:0];
    NSDictionary* signaturesDict = [self signatureDictFromDERData:signatureDERData error:error];
    if (!signaturesDict) {
        return NO;
    }
    
    NSData* cloudSchemaData = record[SecCKRecordManifestSchemaKey];
    NSDictionary* cloudSchemaDict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)cloudSchemaData, 0, NULL, NULL);
    if (![cloudSchemaDict isKindOfClass:[NSDictionary class]]) {
        cloudSchemaDict = __thisBuildsSchema;
    }

    _digestValue = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestDigestValueKey] options:0];
    _generationCount = [record[SecCKRecordManifestGenerationCountKey] unsignedIntegerValue];
    _leafRecordIDs = record[SecCKRecordManifestLeafRecordIDsKey];
    _peerManifestIDs = record[SecCKRecordManifestPeerManifestRecordIDsKey];
    _currentItemsDict = [record[SecCKRecordManifestCurrentItemsKey] mutableCopy];
    if (!_currentItemsDict) {
        _currentItemsDict = [NSMutableDictionary dictionary];
    }
    _futureData = [[self futureDataDictFromRecord:record withSchema:cloudSchemaDict] copy];
    _signaturesDict = signaturesDict;
    _signerID = record[SecCKRecordManifestSignerIDKey];
    _schema = cloudSchemaDict;
    self.storedCKRecord = record;
    
    _derData = nil;
    return YES;
}

- (NSDictionary<NSString*, NSString*>*)sqlValues
{
    void (^addValueSafelyToDictionaryAndLogIfNil)(NSMutableDictionary*, NSString*, id) = ^(NSMutableDictionary* dictionary, NSString* key, id value) {
        if (!value) {
            value = [NSNull null];
            secerror("CKKSManifest: saving manifest to database but %@ is nil", key);
        }

        dictionary[key] = value;
    };

    NSMutableDictionary* sqlValues = [[NSMutableDictionary alloc] init];
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"ckzone", _zoneName);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"gencount", [NSNumber numberWithUnsignedInteger:_generationCount]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"digest", self.digestValue);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"signatures", [[self derDataFromSignatureDict:self.signatures error:nil] base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"signerID", _signerID);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"leafIDs", [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)_leafRecordIDs, NULL) base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"peerManifests", [[self derDataFromPeerManifests:_peerManifestIDs error:nil] base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"currentItems", [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)_currentItemsDict, NULL) base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"futureData", [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)_futureData, NULL) base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"schema", [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)_schema, NULL) base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"ckrecord", [self.encodedCKRecord base64EncodedStringWithOptions:0]);

    return sqlValues;
}

- (NSDictionary<NSString*, NSString*>*)whereClauseToFindSelf
{
    return @{ @"ckzone" : CKKSNilToNSNull(_zoneName),
              @"gencount" : [NSNumber numberWithUnsignedInteger:_generationCount],
              @"signerID" : CKKSNilToNSNull(_signerID) };
}

- (NSString*)CKRecordName
{
    return [NSString stringWithFormat:@"Manifest:-:%@:-:%@:-:%lu", _zoneName, _signerID, (unsigned long)_generationCount];
}

+ (NSDictionary*)whereClauseForRecordName:(NSString*)recordName
{
    NSArray* components = [recordName componentsSeparatedByString:@":-:"];
    if (components.count < 4) {
        secerror("CKKSManifest: could not parse components from record name: %@", recordName);
    }
    
    return @{ @"ckzone" : components[1],
              @"signerID" : components[2],
              @"gencount" : components[3] };
}

- (CKRecord*)updateCKRecord:(CKRecord*)record zoneID:(CKRecordZoneID*)zoneID
{
    if (![record.recordType isEqualToString:SecCKRecordManifestType]) {
        @throw [NSException exceptionWithName:@"WrongCKRecordTypeException" reason:[NSString stringWithFormat:@"CKRecorType (%@) was not %@", record.recordType, SecCKRecordManifestType] userInfo:nil];
    }
    
    NSData* signatureDERData = [self derDataFromSignatureDict:self.signatures error:nil];
    if (!signatureDERData) {
        return record;
    }
    
    record[SecCKRecordManifestDigestValueKey] = [self.digestValue base64EncodedStringWithOptions:0];
    record[SecCKRecordManifestGenerationCountKey] = [NSNumber numberWithUnsignedInteger:_generationCount];
    record[SecCKRecordManifestLeafRecordIDsKey] = _leafRecordIDs;
    record[SecCKRecordManifestPeerManifestRecordIDsKey] = _peerManifestIDs;
    record[SecCKRecordManifestCurrentItemsKey] = [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)_currentItemsDict, NULL) base64EncodedStringWithOptions:0];
    record[SecCKRecordManifestSignaturesKey] = [signatureDERData base64EncodedStringWithOptions:0];
    record[SecCKRecordManifestSignerIDKey] = _signerID;
    record[SecCKRecordManifestSchemaKey] = [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)_schema, NULL) base64EncodedStringWithOptions:0];

    [_futureData enumerateKeysAndObjectsUsingBlock:^(NSString* key, id futureField, BOOL* stop) {
        record[key] = futureField;
    }];
    
    return record;
}

- (bool)matchesCKRecord:(CKRecord*)record
{
    if (![record.recordType isEqualToString:SecCKRecordManifestType]) {
        return false;
    }
    
    NSError* error = nil;
    NSData* signatureDERData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestSignaturesKey] options:0];
    NSDictionary* signaturesDict = [self signatureDictFromDERData:signatureDERData error:&error];
    if (!signaturesDict || error) {
        return NO;
    }
    
    NSData* digestData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestDigestValueKey] options:0];
    return  [digestData isEqual:self.digestValue] &&
            [record[SecCKRecordManifestGenerationCountKey] unsignedIntegerValue] == _generationCount &&
            [record[SecCKRecordManifestPeerManifestRecordIDsKey] isEqual:_peerManifestIDs] &&
            [signaturesDict isEqual:self.signatures] &&
            [record[SecCKRecordManifestSignerIDKey] isEqual:_signerID];
}

- (void)setFromCKRecord:(CKRecord*)record
{
    NSError* error = nil;
    if (![self updateWithRecord:record error:&error]) {
        ckkserror("ckksmanifest", self, "failed to update manifest from CKRecord with error: %@", error);
    }
}

- (NSData*)derData
{
    if (!_derData) {
        NSError* error = nil;
        _derData = ManifestDERData(_zoneName, self.digestValue, _peerManifestIDs, _currentItemsDict, _generationCount, _futureData, _schema, &error);
        if (error) {
            ckkserror("ckksmanifest", self, "error encoding manifest into DER: %@", error);
            _derData = nil;
        }
    }
    
    return _derData;
}

- (BOOL)validateWithError:(NSError**)error
{
    __block BOOL verified = false;
    NSData* manifestDerData = self.derData;
    if (manifestDerData) {
        __block NSError* localError = nil;
        
        [_helper performWithPeerVerifyingKeys:^(NSDictionary<NSString*, SFECPublicKey*>* _Nullable peerKeys, NSError* _Nullable error) {
            if(error) {
                ckkserror("ckksmanifest", self, "Error fetching peer verifying keys: %@", error);
            }
            SFECPublicKey* verifyingKey = peerKeys[self->_signerID];
            if (verifyingKey) {
                SFEC_X962SigningOperation* signingOperation = [self.class signatureOperation];
                SFSignedData* signedData = [[SFSignedData alloc] initWithData:manifestDerData signature:self.signatures[CKKSManifestEC384SignatureKey]];
                verified = [signingOperation verify:signedData withKey:verifyingKey error:&localError] == NULL ? false : true;
                
            }
            else {
                localError = [NSError errorWithDomain:CKKSManifestErrorDomain
                                                 code:CKKSManifestErrorVerifyingKeyNotFound
                                             userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"could not find manifest public key for peer %@", self->_signerID],
                                                        NSUnderlyingErrorKey: CKKSNilToNSNull(error)}];
            }
        }];
        
        if (error) {
            *error = localError;
        }
    }

    return verified;
}

- (BOOL)validateItem:(CKKSItem*)item withError:(NSError**)error
{
    NSString* uuid = item.uuid;
    CKKSManifestLeafRecord* leafRecord = [self leafRecordForItemUUID:uuid];
    NSData* expectedItemDigest = leafRecord.recordDigestDict[uuid];
    if ([[self.class digestForData:item.encitem] isEqual:expectedItemDigest]) {
        return YES;
    }
    else if (error) {
        *error = [NSError errorWithDomain:CKKSManifestErrorDomain code:CKKSManifestErrorInvalidDigest userInfo:@{NSLocalizedDescriptionKey : @"could not validate item because the digest is invalid"}];
    }
    
    return NO;
}

- (BOOL)validateCurrentItem:(CKKSCurrentItemPointer*)currentItem withError:(NSError**)error
{
    BOOL result = [currentItem.currentItemUUID isEqualToString:[_currentItemsDict valueForKey:currentItem.identifier]];
    if (!result && error) {
        *error = [NSError errorWithDomain:CKKSManifestErrorDomain code:CKKSManifestErrorCurrentItemUUIDNotFound userInfo:@{NSLocalizedDescriptionKey :@"could not validate current item because the UUID does not match the manifest"}];
    }

    return result;
}

- (BOOL)itemUUIDExistsInManifest:(NSString*)uuid
{
    CKKSManifestLeafRecord* leafRecord = [self leafRecordForItemUUID:uuid];
    return leafRecord.recordDigestDict[uuid] != nil;
}

- (BOOL)contentsAreEqualToManifest:(CKKSManifest*)otherManifest
{
    return [_digestValue isEqual:otherManifest.digestValue];
}

- (CKKSManifestLeafRecord*)leafRecordForID:(NSString*)leafRecordID
{
    NSError* error = nil;
    CKKSManifestLeafRecord* leafRecord = [CKKSManifestLeafRecord leafRecordForID:leafRecordID error:&error];
    if (error || !leafRecord) {
        ckkserror("ckksmanifest", self, "failed to lookup manifest leaf record with id: %@ error: %@", leafRecordID, error);
    }
    
    return leafRecord;
}

- (CKKSManifestLeafRecord*)leafRecordForItemUUID:(NSString*)uuid
{
    NSInteger bucketIndex = LeafBucketIndexForUUID(uuid);
    NSString* leafRecordID = _leafRecordIDs[bucketIndex];
    return [self leafRecordForID:leafRecordID];
}

- (void)clearDigest
{
    _digestValue = nil;
    _derData = nil;
    _signaturesDict = nil;
}

- (NSData*)digestValue
{
    if (!_digestValue) {
        _digestValue = [self.class digestValueForLeafRecords:self.leafRecords];
    }
    
    return _digestValue;
}

- (NSArray<CKKSManifestLeafRecord*>*)leafRecords
{
    NSMutableArray* leafRecords = [[NSMutableArray alloc] initWithCapacity:_leafRecordIDs.count];
    for (NSString* recordID in _leafRecordIDs) {
        CKKSManifestLeafRecord* leafRecord = [self leafRecordForID:recordID];
        if(leafRecord) {
            [leafRecords addObject:leafRecord];
        } else {
            ckkserror("ckksmanifest", self, "failed to fetch leaf record from CKManifest for %@", recordID);
            // TODO: auto bug capture?
        }
    }
    
    return leafRecords;
}

- (NSString*)ckRecordType
{
    return SecCKRecordManifestType;
}

- (void)nilAllIvars
{
    _derData = nil;
    _digestValue = nil;
    _signerID = nil;
    _zoneName = nil;
    _leafRecordIDs = nil;
    _peerManifestIDs = nil;
    _currentItemsDict = nil;
    _futureData = nil;
    _signaturesDict = nil;
    _schema = nil;
}

@end

@implementation CKKSPendingManifest

@synthesize committedLeafRecordIDs = _committedLeafRecordIDs;

+ (NSString*)sqlTable
{
    return @"pending_manifest";
}

- (BOOL)isReadyToCommit
{
    for (NSString* leafRecordID in self.leafRecordIDs) {
        if ([CKKSManifestLeafRecord recordExistsForID:leafRecordID] || [CKKSManifestPendingLeafRecord recordExistsForID:leafRecordID]) {
            continue;
        }
        else {
            ckksinfo("ckksmanifest", self, "Not ready to commit manifest, yet - missing leaf record ID: %@", leafRecordID);
            return NO;
        }
    }
    
    return YES;
}

- (CKKSManifest*)commitToDatabaseWithError:(NSError**)error
{
    NSError* localError = nil;
    
    _committedLeafRecordIDs = [[NSMutableArray alloc] init];
    
    for (NSString* leafRecordID in self.leafRecordIDs) {
        CKKSManifestPendingLeafRecord* pendingLeaf = [CKKSManifestPendingLeafRecord leafRecordForID:leafRecordID error:&localError];
        if (pendingLeaf) {
            CKKSManifestLeafRecord* committedLeaf = [pendingLeaf commitToDatabaseWithError:error];
            if (committedLeaf) {
                [_committedLeafRecordIDs addObject:committedLeaf.CKRecordName];
            }
            else {
                return nil;
            }
        }
        else {
            CKKSManifestLeafRecord* existingLeaf = [CKKSManifestLeafRecord leafRecordForID:leafRecordID error:&localError];
            if (existingLeaf) {
                [_committedLeafRecordIDs addObject:existingLeaf.CKRecordName];
                continue;
            }
        }
        
        if (localError) {
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }
    
    CKKSManifest* manifest = [CKKSManifest manifestForPendingManifest:self];
    if ([manifest saveToDatabase:error]) {
        [self deleteFromDatabase:error];
        return manifest;
    }
    else {
        return nil;
    }
}

@end

@implementation CKKSEgoManifest

+ (CKKSManifestInjectionPointHelper*)egoHelper
{
    return __egoHelper ?: [[CKKSManifestInjectionPointHelper alloc] init];
}

+ (NSArray*)leafRecordsForItems:(NSArray*)items manifestName:(NSString*)manifestName zone:(NSString*)zone
{
    NSMutableArray* leafRecords = [[NSMutableArray alloc] init];
    for (NSInteger i = 0; i < NUM_MANIFEST_LEAF_RECORDS; i++) {
        [leafRecords addObject:[CKKSEgoManifestLeafRecord newLeafRecordInZone:zone]];
    }
    
    for (CKKSItem* item in items) {
        CKKSEgoManifestLeafRecord* leafRecord = leafRecords[LeafBucketIndexForUUID(item.uuid)];
        [leafRecord addOrUpdateRecordUUID:item.uuid withEncryptedItemData:item.encitem];
    }
    
    return leafRecords;
}

+ (nullable CKKSEgoManifest*)tryCurrentEgoManifestForZone:(NSString*)zone
{
    __block CKKSEgoManifest* manifest = nil;
    [self.egoHelper performWithEgoPeerID:^(NSString * _Nullable egoPeerID, NSError * _Nullable error) {
        if(error) {
            ckkserrorwithzonename("ckksmanifest", zone, "Error getting peer ID: %@", error);
            return;
        }
        if (!egoPeerID) {
            ckkserrorwithzonename("ckksmanifest", zone, "can't get ego peer ID right now - the device probably hasn't been unlocked yet");
            return;
        }
        
        NSDictionary* whereDict = @{ @"ckzone" : zone, @"signerID" : egoPeerID };        
        [self queryMaxValueForField:@"gencount" inTable:self.sqlTable where:whereDict columns:self.sqlColumns processRow:^(NSDictionary* row) {
            manifest = [self fromDatabaseRow:row];
        }];
    }];
    
    return manifest;
}

+ (nullable instancetype)newFakeManifestForZone:(NSString*)zone withItemRecords:(NSArray<CKRecord*>*)itemRecords currentItems:(NSDictionary*)currentItems signerID:(NSString*)signerID keyPair:(SFECKeyPair*)keyPair error:(NSError**)error
{
    CKKSManifestInjectionPointHelper* helper = [[CKKSManifestInjectionPointHelper alloc] initWithPeerID:signerID keyPair:keyPair isEgoPeer:NO];
    CKKSEgoManifest* manifest = [self newManifestForZone:zone withItems:@[] peerManifestIDs:@[] currentItems:currentItems error:error helper:helper];
    manifest.signerID = signerID;
    manifest.helper = helper;
    [manifest updateWithNewOrChangedRecords:itemRecords deletedRecordIDs:@[]];
    return manifest;
}

+ (nullable instancetype)newManifestForZone:(NSString*)zone withItems:(NSArray<CKKSItem*>*)items peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems error:(NSError**)error
{
    return [self newManifestForZone:zone withItems:items peerManifestIDs:peerManifestIDs currentItems:currentItems error:error helper:self.egoHelper];
}

+ (nullable instancetype)newManifestForZone:(NSString*)zone withItems:(NSArray<CKKSItem*>*)items peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems error:(NSError**)error helper:(CKKSManifestInjectionPointHelper*)helper
{
    __block NSError* localError = nil;
    NSArray* leafRecords = [self leafRecordsForItems:items manifestName:nil zone:zone];
    NSData* digestValue = [self digestValueForLeafRecords:leafRecords];
    
    NSInteger generationCount = [self greatestKnownGenerationCount] + 1;
    
    __block CKKSEgoManifest* result = nil;
    [helper performWithEgoPeerID:^(NSString* _Nullable egoPeerID, NSError* _Nullable err) {
        if (err) {
            localError = err;
        }
        else if (egoPeerID) {
            result = [[self alloc] initWithDigestValue:digestValue zone:zone generationCount:generationCount leafRecords:leafRecords peerManifestIDs:peerManifestIDs currentItems:currentItems futureData:[NSDictionary dictionary] signatures:nil signerID:egoPeerID schema:__thisBuildsSchema];
        }
        else {
            localError = [NSError errorWithDomain:CKKSManifestErrorDomain code:CKKSManifestErrorManifestGenerationFailed userInfo:@{NSLocalizedDescriptionKey : @"failed to generate ego manifest because egoPeerID is nil"}];
        }
    }];
    
    if (!result && !localError) {
        localError = [NSError errorWithDomain:CKKSManifestErrorDomain code:CKKSManifestErrorManifestGenerationFailed userInfo:@{NSLocalizedDescriptionKey : @"failed to generate ego manifest"}];
    }
    if (error) {
        *error = localError;
    }
    
    return result;
}

+ (instancetype)fromDatabaseWhere:(NSDictionary *)whereDict error:(NSError * __autoreleasing *)error {
    CKKSEgoManifest* manifest = [super fromDatabaseWhere:whereDict error:error];
    if(!manifest) {
        return nil;
    }

    // Try to load leaf records
    if(![manifest loadLeafRecords:manifest.zoneID.zoneName error:error]) {
        return nil;
    }

    return manifest;
}

+ (instancetype)tryFromDatabaseWhere:(NSDictionary *)whereDict error:(NSError * __autoreleasing *)error {
    CKKSEgoManifest* manifest = [super fromDatabaseWhere:whereDict error:error];
    if(!manifest) {
        return nil;
    }

    // Try to load leaf records
    // Failing to load leaf records on a manifest that exists is an error, even in tryFromDatabaseWhere.
    if(![manifest loadLeafRecords:manifest.zoneID.zoneName error:error]) {
        return nil;
    }

    return manifest;
}

- (bool)loadLeafRecords:(NSString*)ckzone error:(NSError * __autoreleasing *)error {
    NSMutableArray* leafRecords = [[NSMutableArray alloc] initWithCapacity:self.leafRecordIDs.count];
    for (NSString* leafID in self.leafRecordIDs) {
        CKKSEgoManifestLeafRecord* leafRecord = [CKKSEgoManifestLeafRecord fromDatabaseWhere:@{@"uuid" : [CKKSManifestLeafRecord leafUUIDForRecordID:leafID], @"ckzone" : ckzone} error:error];
        if (leafRecord) {
            [leafRecords addObject:leafRecord];
        } else {
            secerror("ckksmanifest: error loading leaf record from database: %@", error ? *error : nil);
            return false;
        }
    }
    
    self->_leafRecords = leafRecords;
    return true;
}

+ (NSDictionary*)generateSignaturesWithHelper:(CKKSManifestInjectionPointHelper*)helper derData:(NSData*)manifestDerData error:(NSError**)error
{
    __block NSData* signature = nil;
    __block NSError* localError = nil;
    [helper performWithSigningKey:^(SFECKeyPair* _Nullable signingKey, NSError* _Nullable err) {
        if (err) {
            localError = err;
            return;
        }
        
        if (signingKey) {
            SFEC_X962SigningOperation* signingOperation = [self signatureOperation];
            SFSignedData* signedData = [signingOperation sign:manifestDerData withKey:signingKey error:&localError];
            signature = signedData.signature;
        }
    }];

    if(error) {
        *error = localError;
    }
    
    return signature ? @{CKKSManifestEC384SignatureKey : signature} : nil;
}

- (instancetype)initWithDigestValue:(NSData*)digestValue zone:(NSString*)zone generationCount:(NSUInteger)generationCount leafRecords:(NSArray<CKKSManifestLeafRecord*>*)leafRecords peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs currentItems:(NSDictionary*)currentItems futureData:(NSDictionary*)futureData signatures:(NSDictionary*)signatures signerID:(NSString*)signerID schema:(NSDictionary*)schema
{
    NSMutableArray* leafRecordIDs = [[NSMutableArray alloc] initWithCapacity:leafRecords.count];
    for (CKKSManifestLeafRecord* leafRecord in leafRecords) {
        [leafRecordIDs addObject:leafRecord.CKRecordName];
    }
    
    if (self = [super initWithDigestValue:digestValue zone:zone generationCount:generationCount leafRecordIDs:leafRecordIDs peerManifestIDs:peerManifestIDs currentItems:currentItems futureData:futureData signatures:signatures signerID:signerID schema:schema helper:[CKKSEgoManifest egoHelper]]) {
        _leafRecords = leafRecords.copy;
    }
    
    return self;
}

- (void)updateWithNewOrChangedRecords:(NSArray<CKRecord*>*)newOrChangedRecords deletedRecordIDs:(NSArray<CKRecordID*>*)deletedRecordIDs
{
    if ([CKKSManifestInjectionPointHelper ignoreChanges]) {
        return;
    }
    
    for (CKRecordID* deletedRecord in deletedRecordIDs) {
        NSString* deletedUUID = deletedRecord.recordName;
        CKKSEgoManifestLeafRecord* leafRecord = [self leafRecordForItemUUID:deletedUUID];
        [leafRecord deleteItemWithUUID:deletedUUID];
    }
    
    for (CKRecord* record in newOrChangedRecords) {
        CKKSEgoManifestLeafRecord* leafRecord = (CKKSEgoManifestLeafRecord*)[self leafRecordForItemUUID:record.recordID.recordName];
        [leafRecord addOrUpdateRecord:record];
    }
    
    [self clearDigest];
    _generationCount = [self.class greatestKnownGenerationCount] + 1;
}

- (void)setCurrentItemUUID:(NSString*)newCurrentItemUUID forIdentifier:(NSString*)currentPointerIdentifier
{
    _currentItemsDict[currentPointerIdentifier] = newCurrentItemUUID;
    [self clearDigest];
    _generationCount = [self.class greatestKnownGenerationCount] + 1;
}

- (CKKSEgoManifestLeafRecord*)leafRecordForItemUUID:(NSString*)uuid
{
    NSUInteger leafBucket = LeafBucketIndexForUUID(uuid);
    if(_leafRecords.count > leafBucket) {
        return _leafRecords[leafBucket];
    } else {
        return nil;
    }
}

- (NSArray<CKKSManifestLeafRecord*>*)leafRecords
{
    return _leafRecords;
}

- (NSArray<CKRecord*>*)allCKRecordsWithZoneID:(CKRecordZoneID*)zoneID
{
    NSMutableArray* records = [[NSMutableArray alloc] initWithCapacity:_leafRecords.count + 1];
    [records addObject:[self CKRecordWithZoneID:zoneID]];
    
    for (CKKSManifestLeafRecord* leafRecord in _leafRecords) {
        [records addObject:[leafRecord CKRecordWithZoneID:zoneID]];
    }
    
    return records;
}

- (bool)saveToDatabase:(NSError**)error
{
    bool result = [super saveToDatabase:error];    
    if (result) {
        for (CKKSManifestLeafRecord* leafRecord in _leafRecords) {
            result &= [leafRecord saveToDatabase:error];
        }
    }
    
    return result;
}

- (NSDictionary*)signatures
{
    if (!_signaturesDict) {
        _signaturesDict = [self.class generateSignaturesWithHelper:self.helper derData:self.derData error:nil];
    }
    
    return _signaturesDict;
}

- (void)setSignatures:(NSDictionary*)signatures
{
    _signaturesDict = signatures;
}

- (CKKSManifestInjectionPointHelper*)defaultHelperForSignerID:(NSString*)signerID
{
    CKKSManifestInjectionPointHelper* helper = __helpersDict[signerID];
    return helper ?: __egoHelper;
}

@end

@implementation CKKSManifestInjectionPointHelper {
    NSString* _peerID;
    SFECKeyPair* _keyPair;
}

+ (void)registerHelper:(CKKSManifestInjectionPointHelper*)helper forPeer:(NSString*)peerID
{
    if (!__helpersDict) {
        __helpersDict = [[NSMutableDictionary alloc] init];
    }
    
    __helpersDict[peerID] = helper;
}

+ (void)registerEgoPeerID:(NSString*)egoPeerID keyPair:(SFECKeyPair*)keyPair
{
    __egoHelper = [[self alloc] initWithPeerID:egoPeerID keyPair:keyPair isEgoPeer:YES];
}

+ (BOOL)ignoreChanges
{
    return __ignoreChanges;
}

+ (void)setIgnoreChanges:(BOOL)ignoreChanges
{
    __ignoreChanges = ignoreChanges ? YES : NO;
}

- (instancetype)initWithPeerID:(NSString*)peerID keyPair:(SFECKeyPair*)keyPair isEgoPeer:(BOOL)isEgoPeer
{
    if (self = [super init]) {
        _peerID = peerID;
        _keyPair = keyPair;
        if (isEgoPeer) {
            __egoHelper = self;
        }
        else {
            [self.class registerHelper:self forPeer:peerID];
        }
    }
    
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@ peerID: (%@)", [super description], _peerID];
}

- (void)performWithSigningKey:(void (^)(SFECKeyPair* _Nullable signingKey, NSError* _Nullable error))handler
{
    if (s_accountInfo) {
        if(s_accountInfo.setupError) {
            handler(nil, s_accountInfo.setupError);
        } else {
            handler(s_accountInfo.signingKey, nil);
        }
    }
    else if (_keyPair) {
        handler(_keyPair, nil);
    }
    else {
        SOSCCPerformWithOctagonSigningKey(^(SecKeyRef signingSecKey, CFErrorRef err) {
            SFECKeyPair* key = nil;
            if (!err && signingSecKey) {
                key = [[SFECKeyPair alloc] initWithSecKey:signingSecKey];
            }
            
            handler(key, (__bridge NSError*)err);
        });
    }
}

- (void)performWithEgoPeerID:(void (^)(NSString* _Nullable egoPeerID, NSError* _Nullable error))handler
{
    if (s_accountInfo) {
        if(s_accountInfo.setupError) {
            handler(nil, s_accountInfo.setupError);
        } else {
            handler(s_accountInfo.egoPeerID, nil);
        }
    }
    else if (_peerID) {
        handler(_peerID, nil);
    }
    else {
        NSError* error = nil;
        SOSPeerInfoRef egoPeerInfo = SOSCCCopyMyPeerInfo(NULL);
        NSString* egoPeerID = egoPeerInfo ? (__bridge NSString*)SOSPeerInfoGetPeerID(egoPeerInfo) : nil;
        handler(egoPeerID, error);
        CFReleaseNull(egoPeerInfo);
    }
}

- (void)performWithPeerVerifyingKeys:(void (^)(NSDictionary<NSString*, SFECPublicKey*>* _Nullable peerKeys, NSError* _Nullable error))handler
{
    if (s_accountInfo) {
        if(s_accountInfo.setupError) {
            handler(nil, s_accountInfo.setupError);
        } else {
            handler(s_accountInfo.peerVerifyingKeys, nil);
        }
    }
    else if (__egoHelper || __helpersDict) {
        NSMutableDictionary* verifyingKeys = [[NSMutableDictionary alloc] init];
        [__helpersDict enumerateKeysAndObjectsUsingBlock:^(NSString* _Nonnull peer, CKKSManifestInjectionPointHelper* _Nonnull helper, BOOL* _Nonnull stop) {
            verifyingKeys[peer] = helper.keyPair.publicKey;
        }];
        if (__egoHelper.keyPair) {
            verifyingKeys[__egoHelper.peerID] = __egoHelper.keyPair.publicKey;
        }
        handler(verifyingKeys, nil);
    }
    else {
        CFErrorRef error = NULL;
        NSMutableDictionary* peerKeys = [NSMutableDictionary dictionary];
        CFArrayRef peerInfos = SOSCCCopyValidPeerPeerInfo(&error);
        if (!peerInfos || error) {
            handler(nil, (__bridge NSError*)error);
            CFReleaseNull(peerInfos);
            CFReleaseNull(error);
            return;
        }
        
        CFArrayForEach(peerInfos, ^(const void* peerInfoPtr) {
            SOSPeerInfoRef peerInfo = (SOSPeerInfoRef)peerInfoPtr;
            CFErrorRef blockError = NULL;
            SecKeyRef secPublicKey = SOSPeerInfoCopyOctagonSigningPublicKey(peerInfo, &blockError);
            if (!secPublicKey || blockError) {
                CFReleaseNull(secPublicKey);
                CFReleaseNull(blockError);
                return;
            }
            
            SFECPublicKey* publicKey = [[SFECPublicKey alloc] initWithSecKey:secPublicKey];
            CFReleaseNull(secPublicKey);
            NSString* peerID = (__bridge NSString*)SOSPeerInfoGetPeerID(peerInfo);
            peerKeys[peerID] = publicKey;
        });
        
        handler(peerKeys, nil);
        CFReleaseNull(peerInfos);
    }
}

- (SFECKeyPair*)keyPair
{
    return _keyPair;
}

- (NSString*)peerID
{
    return _peerID;
}

@end

@implementation CKKSAccountInfo

@synthesize signingKey = _signingKey;
@synthesize peerVerifyingKeys = _peerVerifyingKeys;
@synthesize egoPeerID = _egoPeerID;

@end

#endif

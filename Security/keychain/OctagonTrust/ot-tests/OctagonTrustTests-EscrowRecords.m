/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
#import <XCTest/XCTest.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"
#import <OCMock/OCMock.h>
#pragma clang diagnostic pop

#import <OctagonTrust/OctagonTrust.h>
#import "OctagonTrustTests-EscrowTestVectors.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTClique+Private.h"

#import "keychain/OctagonTrust/categories/OctagonTrustEscrowRecoverer.h"

#import <Foundation/NSPropertyList.h>

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSMockSOSPresentAdapter.h"
#import "utilities/SecCFError.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "OctagonTrustTests.h"

@implementation ProxyXPCConnection

- (instancetype)initWithInterface:(NSXPCInterface*)interface obj:(id)obj
{
    if(self = [super init]) {
        self.obj = obj;
        self.serverInterface = interface;
        self.listener = [NSXPCListener anonymousListener];

        self.listener.delegate = self;
        [self.listener resume];
    }

    return self;
}

- (NSXPCConnection*)connection
{
    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithListenerEndpoint:self.listener.endpoint];
    connection.remoteObjectInterface = self.serverInterface;
    [connection resume];
    return connection;
}

- (BOOL)listener:(NSXPCListener*)listener newConnection:(NSXPCConnection*)newConnection {
    newConnection.exportedInterface = self.serverInterface;
    newConnection.exportedObject = self.obj;
    [newConnection resume];
    return true;
  }

@end

@interface FakeOTControlEntitlementBearer : NSObject <OctagonEntitlementBearerProtocol>
@property NSMutableDictionary* entitlements;
-(instancetype)init;
- (nullable id)valueForEntitlement:(NSString *)entitlement;
@end

@implementation FakeOTControlEntitlementBearer

-(instancetype)init
{
    if(self = [super init]) {
        self.entitlements = [NSMutableDictionary dictionary];
        self.entitlements[kSecEntitlementPrivateOctagonEscrow] = @YES;
    }
    return self;
}
-(id)valueForEntitlement:(NSString*)entitlement
{
    return self.entitlements[entitlement];
}

@end

@interface OTMockSecureBackup : NSObject <OctagonEscrowRecovererPrococol>
@property (nonatomic) NSString * bottleID;
@property (nonatomic) NSData* entropy;

@end

@implementation OTMockSecureBackup

-(instancetype) initWithBottleID:(NSString*)b entropy:(NSData*)e {
    if(self = [super init]) {
        self.bottleID = b;
        self.entropy = e;
    }
    return self;
}

- (NSError *)disableWithInfo:(NSDictionary *)info {
    return nil;
}

- (NSError *)getAccountInfoWithInfo:(NSDictionary *)info results:(NSDictionary *__autoreleasing *)results {
    NSError* localError = nil;
    NSData *testVectorData = [accountInfoWithInfoSample dataUsingEncoding:kCFStringEncodingUTF8];
    NSPropertyListFormat format;
    NSDictionary *testVectorPlist = [NSPropertyListSerialization propertyListWithData:testVectorData options: NSPropertyListMutableContainersAndLeaves format:&format error:&localError];
    *results = testVectorPlist;

    return nil;
}

- (NSDictionary *)recoverSilentWithCDPContext:(OTICDPRecordContext *)cdpContext allRecords:(NSArray<OTEscrowRecord *> *)allRecords error:(NSError *__autoreleasing *)error {
    return nil;
}

- (NSDictionary *)recoverWithCDPContext:(OTICDPRecordContext *)cdpContext escrowRecord:(OTEscrowRecord *)escrowRecord error:(NSError *__autoreleasing *)error {
    return nil;
}

- (NSError *)recoverWithInfo:(NSDictionary *)info results:(NSDictionary *__autoreleasing *)results {
    return nil;
}

- (void)restoreKeychainAsyncWithPassword:(id)password keybagDigest:(NSData *)keybagDigest haveBottledPeer:(BOOL)haveBottledPeer viewsNotToBeRestored:(NSMutableSet<NSString *> *)viewsNotToBeRestored error:(NSError *__autoreleasing *)error {
}


@end

@implementation OctagonTrustTests

- (OTEscrowRecord*)createLegacyRecord
{
    OTEscrowRecord *nonViableRecord = [[OTEscrowRecord alloc] init];
    nonViableRecord.creationDate = [NSDate date].timeIntervalSince1970;
    nonViableRecord.label = [[NSUUID UUID] UUIDString];
    nonViableRecord.recordStatus = OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID;
    nonViableRecord.recordViability = OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY;
    nonViableRecord.serialNumber = [[NSUUID UUID] UUIDString];
    nonViableRecord.silentAttemptAllowed = 1;
    nonViableRecord.escrowInformationMetadata = [[OTEscrowRecordMetadata alloc] init];
    nonViableRecord.escrowInformationMetadata.bottleValidity = @"invalid";

    // It even has a bottleId! Not a useful one, though.
    nonViableRecord.escrowInformationMetadata.bottleId = [[NSUUID UUID] UUIDString];
    return nonViableRecord;
}

- (OTEscrowRecord*)createLegacyRecordWithSOSNotViable
{
    OTEscrowRecord *nonViableRecord = [[OTEscrowRecord alloc] init];
    nonViableRecord.creationDate = [NSDate date].timeIntervalSince1970;
    nonViableRecord.label = [[NSUUID UUID] UUIDString];
    nonViableRecord.recordStatus = OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID;
    nonViableRecord.recordViability = OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY;
    nonViableRecord.viabilityStatus = OTEscrowRecord_SOSViability_SOS_NOT_VIABLE;
    nonViableRecord.serialNumber = [[NSUUID UUID] UUIDString];
    nonViableRecord.silentAttemptAllowed = 1;
    nonViableRecord.escrowInformationMetadata = [[OTEscrowRecordMetadata alloc] init];
    nonViableRecord.escrowInformationMetadata.bottleValidity = @"invalid";

    // It even has a bottleId! Not a useful one, though.
    nonViableRecord.escrowInformationMetadata.bottleId = [[NSUUID UUID] UUIDString];
    return nonViableRecord;
}

- (OTEscrowRecord*)createPartialRecord
{
    OTEscrowRecord *partiallyViableRecord = [[OTEscrowRecord alloc] init];
    partiallyViableRecord.creationDate = [NSDate date].timeIntervalSince1970;
    partiallyViableRecord.label = [[NSUUID UUID] UUIDString];
    partiallyViableRecord.recordStatus = OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID;
    partiallyViableRecord.recordViability = OTEscrowRecord_RecordViability_RECORD_VIABILITY_PARTIALLY_VIABLE;
    partiallyViableRecord.escrowInformationMetadata = [[OTEscrowRecordMetadata alloc] init];
    partiallyViableRecord.escrowInformationMetadata.bottleValidity = @"valid";
    partiallyViableRecord.escrowInformationMetadata.bottleId = [[NSUUID UUID] UUIDString];

    partiallyViableRecord.serialNumber =[[NSUUID UUID] UUIDString];
    partiallyViableRecord.silentAttemptAllowed = 1;
    return partiallyViableRecord;
}

- (OTEscrowRecord*)createOctagonViableSOSViableRecord
{
    OTEscrowRecord *octagonAndSOSViableRecord = [[OTEscrowRecord alloc] init];
    octagonAndSOSViableRecord.creationDate = [NSDate date].timeIntervalSince1970;
    octagonAndSOSViableRecord.label = [[NSUUID UUID] UUIDString];
    octagonAndSOSViableRecord.recordStatus = OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID;
    octagonAndSOSViableRecord.recordViability = OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE;
    octagonAndSOSViableRecord.viabilityStatus = OTEscrowRecord_SOSViability_SOS_VIABLE;

    octagonAndSOSViableRecord.escrowInformationMetadata = [[OTEscrowRecordMetadata alloc] init];
    octagonAndSOSViableRecord.escrowInformationMetadata.bottleValidity = @"valid";
    octagonAndSOSViableRecord.escrowInformationMetadata.bottleId = [[NSUUID UUID] UUIDString];

    octagonAndSOSViableRecord.serialNumber =[[NSUUID UUID] UUIDString];
    octagonAndSOSViableRecord.silentAttemptAllowed = 1;
    return octagonAndSOSViableRecord;
}

- (OTEscrowRecord*)createOctagonNotViableSOSViableRecord
{
    OTEscrowRecord *octagonAndSOSViableRecord = [[OTEscrowRecord alloc] init];
    octagonAndSOSViableRecord.creationDate = [NSDate date].timeIntervalSince1970;
    octagonAndSOSViableRecord.label = [[NSUUID UUID] UUIDString];
    octagonAndSOSViableRecord.recordStatus = OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID;
    octagonAndSOSViableRecord.recordViability = OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY;
    octagonAndSOSViableRecord.viabilityStatus = OTEscrowRecord_SOSViability_SOS_VIABLE;

    octagonAndSOSViableRecord.escrowInformationMetadata = [[OTEscrowRecordMetadata alloc] init];
    octagonAndSOSViableRecord.escrowInformationMetadata.bottleValidity = @"invalid";
    octagonAndSOSViableRecord.escrowInformationMetadata.bottleId = [[NSUUID UUID] UUIDString];

    octagonAndSOSViableRecord.serialNumber =[[NSUUID UUID] UUIDString];
    octagonAndSOSViableRecord.silentAttemptAllowed = 1;
    return octagonAndSOSViableRecord;
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnOneRecord:(OTConfigurationContext*)data
                                                                       error:(NSError* __autoreleasing *)error
{
    NSError* localError = nil;
    NSData *testVectorData = [accountInfoWithInfoSample dataUsingEncoding:kCFStringEncodingUTF8];
    NSPropertyListFormat format;
    NSDictionary *testVectorPlist = [NSPropertyListSerialization propertyListWithData:testVectorData options: NSPropertyListMutableContainersAndLeaves format:&format error:&localError];

    NSArray *testVectorICDPRecords = testVectorPlist[@"SecureBackupAlliCDPRecords"];
    XCTAssertNotNil(testVectorICDPRecords, "should not be nil");

    NSDictionary *testVectorRecord = testVectorICDPRecords[0];

    OTEscrowRecord *record = [OTEscrowTranslation dictionaryToEscrowRecord:testVectorRecord];

    NSArray *records = @[record.data];
    return records;
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnMixedRecords:(OTConfigurationContext*)data
                                                                          error:(NSError* __autoreleasing *)error
{
    NSError* localError = nil;
    NSData *testVectorData = [accountInfoWithInfoSample dataUsingEncoding:kCFStringEncodingUTF8];
    NSPropertyListFormat format;
    NSDictionary *testVectorPlist = [NSPropertyListSerialization propertyListWithData:testVectorData options: NSPropertyListMutableContainersAndLeaves format:&format error:&localError];

    NSArray *testVectorICDPRecords = testVectorPlist[@"SecureBackupAlliCDPRecords"];
    XCTAssertNotNil(testVectorICDPRecords, "should not be nil");

    NSDictionary *testVectorRecord = testVectorICDPRecords[0];

    OTEscrowRecord *record = [OTEscrowTranslation dictionaryToEscrowRecord:testVectorRecord];

    NSArray *records = @[record.data, [self createLegacyRecord].data, [self createLegacyRecord].data];
    return records;
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnPartiallyViableRecords:(OTConfigurationContext*)data
                                                                                    error:(NSError* __autoreleasing *)error
{
    return @[[self createPartialRecord].data, [self createPartialRecord].data];
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnOctagonViableSOSViableRecords:(OTConfigurationContext*)data
                                                                                           error:(NSError* __autoreleasing *)error
{
    return @[[self createOctagonViableSOSViableRecord].data, [self createOctagonViableSOSViableRecord].data];
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnLegacyRecords:(OTConfigurationContext*)data
                                                                           error:(NSError* __autoreleasing *)error
{
    return @[[self createLegacyRecord].data, [self createLegacyRecord].data];
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnLegacyNonViableSOSRecords:(OTConfigurationContext*)data
                                                                                       error:(NSError* __autoreleasing *)error
{
    return @[[self createLegacyRecordWithSOSNotViable].data, [self createLegacyRecordWithSOSNotViable].data];
}

- (NSArray<NSData*>* _Nullable)mockFetchEscrowRecordsInternalReturnOctagonNotViableSOSViableRecords:(OTConfigurationContext*)data
                                                                                              error:(NSError* __autoreleasing *)error
{
    return @[[self createOctagonNotViableSOSViableRecord].data, [self createOctagonNotViableSOSViableRecord].data];
}

- (void)setUp
{
    [super setUp];

    FakeOTControlEntitlementBearer *otControlEntitlementBearer = [[FakeOTControlEntitlementBearer alloc]init];
    id otControlEntitlementChecker = [OctagonXPCEntitlementChecker createWithManager:self.injectedOTManager entitlementBearer:otControlEntitlementBearer];

    NSXPCInterface *interface = OTSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(OTControlProtocol)]);
    self.otXPCProxy = [[ProxyXPCConnection alloc] initWithInterface:interface obj: otControlEntitlementChecker];

    self.otControl = [[OTControl alloc] initWithConnection:self.otXPCProxy.connection sync: true];

    self.mockClique = OCMClassMock([OTClique class]);
}

- (void)testFetchOneViableEscrowRecord
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnOneRecord:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 1, "should only return 1 record");
    OTEscrowRecord *firstRecord = escrowRecords[0];
    XCTAssertTrue([firstRecord.label isEqualToString:@"com.apple.icdp.record"], "label should be com.apple.icdp.record");
    XCTAssertTrue([firstRecord.serialNumber isEqualToString:@"C39V209AJ9L5"], "serial number should be C39V209AJ9L5");
    XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE, "record viability should be fully viable");
}

- (void)testFetchTwoPartiallyViableEscrowRecords
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnPartiallyViableRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 2, "should only return 2 records");
    OTEscrowRecord *firstRecord = escrowRecords[0];
    XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_PARTIALLY_VIABLE, "record viability should be partial");

    OTEscrowRecord *secondRecord = escrowRecords[1];
    XCTAssertTrue([secondRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(secondRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_PARTIALLY_VIABLE, "record viability should be partial");
}

- (void)testFetchViableAndLegacyEscrowRecords
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnMixedRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertEqual(escrowRecords.count, 1, "should only return 1 record");
    XCTAssertNil(localErrror, "error should be nil");

    OTEscrowRecord *firstRecord = escrowRecords[0];
    XCTAssertTrue([firstRecord.label isEqualToString:@"com.apple.icdp.record"], "label should be com.apple.icdp.record");
    XCTAssertTrue([firstRecord.serialNumber isEqualToString:@"C39V209AJ9L5"], "serial number should be C39V209AJ9L5");
    XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE, "record viability should be fully viable");
}

- (void)testFetchLegacyEscrowRecords
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnLegacyRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertEqual(escrowRecords.count, 0, "should return zero records");
    XCTAssertNil(localErrror, "error should be nil");
}

- (void)testFetchRecordsOctagonViableSOSUnknownViability
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnOneRecord:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 1, "should only return 1 record");
    OTEscrowRecord *firstRecord = escrowRecords[0];
    XCTAssertTrue([firstRecord.label isEqualToString:@"com.apple.icdp.record"], "label should be com.apple.icdp.record");
    XCTAssertTrue([firstRecord.serialNumber isEqualToString:@"C39V209AJ9L5"], "serial number should be C39V209AJ9L5");
    XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE, "record viability should be fully viable");
    XCTAssertEqual(firstRecord.viabilityStatus, OTEscrowRecord_SOSViability_SOS_VIABLE_UNKNOWN, "record SOS viability should be unknown");
}

- (void)testFetchRecordsOctagonViableSOSViable
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnOctagonViableSOSViableRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 2, "should only return 2 records");
    OTEscrowRecord *firstRecord = escrowRecords[0];
    XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE, "record viability should be fully viable");
    XCTAssertEqual(firstRecord.viabilityStatus, OTEscrowRecord_SOSViability_SOS_VIABLE, "record sos viability should be fully viable");

    OTEscrowRecord *secondRecord = escrowRecords[1];
    XCTAssertTrue([secondRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"valid"], "bottle validity should be valid");
    XCTAssertEqual(secondRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE, "record viability should be fully viable");
    XCTAssertEqual(secondRecord.viabilityStatus, OTEscrowRecord_SOSViability_SOS_VIABLE, "record sos viability should be fully viable");
}

- (void)testFetchRecordsOctagonNotViableSOSViable
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnOctagonNotViableSOSViableRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    if (OctagonPlatformSupportsSOS()) {
        XCTAssertEqual(escrowRecords.count, 2, "should only return 2 records");
        OTEscrowRecord *firstRecord = escrowRecords[0];
        XCTAssertTrue([firstRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"invalid"], "bottle validity should be invalid");
        XCTAssertEqual(firstRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY, "record viability should be set to legacy");
        XCTAssertEqual(firstRecord.viabilityStatus, OTEscrowRecord_SOSViability_SOS_VIABLE, "record sos viability should be fully viable");

        OTEscrowRecord *secondRecord = escrowRecords[1];
        XCTAssertTrue([secondRecord.escrowInformationMetadata.bottleValidity isEqualToString: @"invalid"], "bottle validity should be invalid");
        XCTAssertEqual(secondRecord.recordViability, OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY, "record viability should be set to legacy");
        XCTAssertEqual(secondRecord.viabilityStatus, OTEscrowRecord_SOSViability_SOS_VIABLE, "record sos viability should be fully viable");
    } else {
        XCTAssertEqual(escrowRecords.count, 0, "should return 0 records");

    }
}

- (void)testFetchRecordsOctagonNotViableSOSNotViable
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnLegacyNonViableSOSRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 0, "should return 0 records");
}

- (void)testFetchAllRecords
{
    OTConfigurationContext *cliqueContextConfiguration = [[OTConfigurationContext alloc]init];
    cliqueContextConfiguration.context = OTDefaultContext;
    cliqueContextConfiguration.dsid = @"1234";
    cliqueContextConfiguration.altDSID = @"altdsid";
    cliqueContextConfiguration.otControl = self.otControl;

    OCMStub([self.mockClique fetchEscrowRecordsInternal:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFetchEscrowRecordsInternalReturnLegacyNonViableSOSRecords:error:));

    NSError* localErrror = nil;
    NSArray* escrowRecords = [OTClique fetchAllEscrowRecords:cliqueContextConfiguration error:&localErrror];
    XCTAssertNil(localErrror, "error should be nil");

    XCTAssertEqual(escrowRecords.count, 2, "should return 2 records");
}

- (void)testCDPRecordContextTranslation
{
    NSError* localError = nil;
    NSData *testVectorData = [testCDPRemoteRecordContextTestVector dataUsingEncoding:kCFStringEncodingUTF8];
    NSPropertyListFormat format;
    NSDictionary *testVectorPlist = [NSPropertyListSerialization propertyListWithData:testVectorData options: NSPropertyListMutableContainersAndLeaves format:&format error:&localError];
    XCTAssertNotNil(testVectorPlist, "testVectorPlist should not be nil");
    OTICDPRecordContext *cdpContext = [OTEscrowTranslation dictionaryToCDPRecordContext:testVectorPlist];
    XCTAssertNotNil(cdpContext, "cdpContext should not be nil");
    XCTAssertTrue([cdpContext.authInfo.authenticationAppleid isEqualToString:@"anna.535.paid@icloud.com"], "authenticationAppleid should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationAuthToken isEqualToString:@"EAAbAAAABLwIAAAAAF5PGvkRDmdzLmljbG91ZC5hdXRovQBx359KJvlZTwe1q6BwXvK4gQUYo2WQbKT8UDtn8rcA6FvEYBANaAk1ofWx/bcfB4pcLiXR3Y0kncELCwFCEEpqpZS+klD9AY1oT9zW6VtyOgQTZJ4mfWz103+FoMh8nLJAVpYVfM/UjsiNsLfTX+rUmevfeA=="], "authenticationAuthToken should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationDsid isEqualToString:@"16187698960"], "authenticationDsid should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationEscrowproxyUrl isEqualToString:@"https://p97-escrowproxy.icloud.com:443"], "authenticationEscrowproxyUrl should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationIcloudEnvironment isEqualToString:@"PROD"], "authenticationIcloudEnvironment should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationPassword isEqualToString: @"PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET"], "authenticationPassword should match");
    XCTAssertFalse(cdpContext.authInfo.fmipRecovery, "fmipRecovery should be false");
    XCTAssertFalse(cdpContext.authInfo.idmsRecovery, "idmsRecovery should be false");


    XCTAssertTrue(cdpContext.cdpInfo.containsIcdpData, "containsIcdpData should be true");
    XCTAssertFalse(cdpContext.cdpInfo.silentRecoveryAttempt, "silentRecoveryAttempt should be false");
    XCTAssertTrue(cdpContext.cdpInfo.usesMultipleIcsc, "usesMultipleIcsc should match");
    XCTAssertFalse(cdpContext.cdpInfo.useCachedSecret, "useCachedSecret should be false");
    XCTAssertFalse(cdpContext.cdpInfo.usePreviouslyCachedRecoveryKey, "usePreviouslyCachedRecoveryKey should be false");
    XCTAssertNil(cdpContext.cdpInfo.recoveryKey, "recoveryKey should be nil");
    XCTAssertTrue([cdpContext.cdpInfo.recoverySecret isEqualToString:@"333333"], "recoverySecret should be 333333");

    NSDictionary *translateBack = [OTEscrowTranslation CDPRecordContextToDictionary:cdpContext];
    XCTAssertNotNil(translateBack, "translateBack should not be nil");

    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationAppleID"]) isEqualToString:@"anna.535.paid@icloud.com"], "SecureBackupAuthenticationAppleID should be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationAuthToken"]) isEqualToString:@"EAAbAAAABLwIAAAAAF5PGvkRDmdzLmljbG91ZC5hdXRovQBx359KJvlZTwe1q6BwXvK4gQUYo2WQbKT8UDtn8rcA6FvEYBANaAk1ofWx/bcfB4pcLiXR3Y0kncELCwFCEEpqpZS+klD9AY1oT9zW6VtyOgQTZJ4mfWz103+FoMh8nLJAVpYVfM/UjsiNsLfTX+rUmevfeA=="], "SecureBackupAuthenticationAuthToken should be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationDSID"]) isEqualToString:@"16187698960"], "SecureBackupAuthenticationDSIDshould be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationEscrowProxyURL"]) isEqualToString:@"https://p97-escrowproxy.icloud.com:443"], "SecureBackupAuthenticationEscrowProxyURL be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationPassword"]) isEqualToString:@"PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET"], "SecureBackupAuthenticationPassword be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationiCloudEnvironment"]) isEqualToString:@"PROD"], "SecureBackupAuthenticationiCloudEnvironment be equal");
    XCTAssertTrue(translateBack[@"SecureBackupContainsiCDPData"], "SecureBackupContainsiCDPData true");
    XCTAssertTrue([translateBack[@"SecureBackupFMiPRecoveryKey"] isEqualToNumber:@NO], "SecureBackupFMiPRecoveryKey is false");
    XCTAssertTrue([translateBack[@"SecureBackupIDMSRecovery"] isEqualToNumber:@NO], "SecureBackupIDMSRecovery false");
    XCTAssertTrue([translateBack[@"SecureBackupPassphrase"] isEqualToString: @"333333"], "SecureBackupPassphrase true");
    XCTAssertTrue([translateBack[@"SecureBackupSilentRecoveryAttempt"] isEqualToNumber:@NO], "SecureBackupSilentRecoveryAttempt false");
    XCTAssertTrue([translateBack[@"SecureBackupUseCachedPassphrase"] isEqualToNumber:@NO], "SecureBackupUseCachedPassphrase false");
    XCTAssertTrue([translateBack[@"SecureBackupUsesMultipleiCSCs"] isEqualToNumber:@YES], "SecureBackupUsesMultipleiCSCs true");
    XCTAssertTrue([translateBack[@"SecureBackupUsesRecoveryKey"] isEqualToNumber:@NO], "SecureBackupUsesRecoveryKey false");
}

- (void)testCDPSilentRecordContextTranslation
{
    NSError* localError = nil;
    NSData *testVectorData = [CDPRecordContextSilentTestVector dataUsingEncoding:kCFStringEncodingUTF8];
    NSPropertyListFormat format;
    NSDictionary *testVectorPlist = [NSPropertyListSerialization propertyListWithData:testVectorData options: NSPropertyListMutableContainersAndLeaves format:&format error:&localError];
    XCTAssertNotNil(testVectorPlist, "testVectorPlist should not be nil");
       OTICDPRecordContext *cdpContext = [OTEscrowTranslation dictionaryToCDPRecordContext:testVectorPlist];
    XCTAssertNotNil(cdpContext, "cdpContext should not be nil");
    XCTAssertTrue([cdpContext.authInfo.authenticationAppleid isEqualToString:@"anna.535.paid@icloud.com"], "authenticationAppleid should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationAuthToken isEqualToString:@"EAAbAAAABLwIAAAAAF5PHOERDmdzLmljbG91ZC5hdXRovQDwjwm2kXoklEtO/xeL3YCPlBr7IkVuV26y2BfLco+QhJFm4VhgFZSBFUg5l4g/uV2DG95xadgk0+rWLhyXDGZwHN2V9jju3eo6sRwGVj4g5iBFStuj4unTKylu3iFkNSKtTMXAyBXpn4EiRX+8dwumC2FKkA=="], "authenticationAuthToken should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationDsid isEqualToString:@"16187698960"], "authenticationDsid should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationEscrowproxyUrl isEqualToString:@"https://p97-escrowproxy.icloud.com:443"], "authenticationEscrowproxyUrl should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationIcloudEnvironment isEqualToString:@"PROD"], "authenticationIcloudEnvironment should match");
    XCTAssertTrue([cdpContext.authInfo.authenticationPassword isEqualToString: @"PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET"], "authenticationPassword should match");
    XCTAssertFalse(cdpContext.authInfo.fmipRecovery, "fmipRecovery should be false");
    XCTAssertFalse(cdpContext.authInfo.idmsRecovery, "idmsRecovery should be false");


    XCTAssertTrue(cdpContext.cdpInfo.containsIcdpData, "containsIcdpData should be true");
    XCTAssertTrue(cdpContext.cdpInfo.silentRecoveryAttempt, "silentRecoveryAttempt should be true");
    XCTAssertTrue(cdpContext.cdpInfo.usesMultipleIcsc, "usesMultipleIcsc should match");
    XCTAssertFalse(cdpContext.cdpInfo.useCachedSecret, "useCachedSecret should be false");
    XCTAssertFalse(cdpContext.cdpInfo.usePreviouslyCachedRecoveryKey, "usePreviouslyCachedRecoveryKey should be false");
    XCTAssertNil(cdpContext.cdpInfo.recoveryKey, "recoveryKey should be nil");
    XCTAssertTrue([cdpContext.cdpInfo.recoverySecret isEqualToString:@"333333"], "recoverySecret should be 333333");

    NSDictionary *translateBack = [OTEscrowTranslation CDPRecordContextToDictionary:cdpContext];
    XCTAssertNotNil(translateBack, "translateBack should not be nil");

    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationAppleID"]) isEqualToString:@"anna.535.paid@icloud.com"], "SecureBackupAuthenticationAppleID should be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationAuthToken"]) isEqualToString:@"EAAbAAAABLwIAAAAAF5PHOERDmdzLmljbG91ZC5hdXRovQDwjwm2kXoklEtO/xeL3YCPlBr7IkVuV26y2BfLco+QhJFm4VhgFZSBFUg5l4g/uV2DG95xadgk0+rWLhyXDGZwHN2V9jju3eo6sRwGVj4g5iBFStuj4unTKylu3iFkNSKtTMXAyBXpn4EiRX+8dwumC2FKkA=="], "SecureBackupAuthenticationAuthToken should be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationDSID"]) isEqualToString:@"16187698960"], "SecureBackupAuthenticationDSIDshould be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationEscrowProxyURL"]) isEqualToString:@"https://p97-escrowproxy.icloud.com:443"], "SecureBackupAuthenticationEscrowProxyURL be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationPassword"]) isEqualToString:@"PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET"], "SecureBackupAuthenticationPassword be equal");
    XCTAssertTrue([((NSString*)translateBack[@"SecureBackupAuthenticationiCloudEnvironment"]) isEqualToString:@"PROD"], "SecureBackupAuthenticationiCloudEnvironment be equal");
    XCTAssertTrue(translateBack[@"SecureBackupContainsiCDPData"], "SecureBackupContainsiCDPData true");
    XCTAssertTrue([translateBack[@"SecureBackupFMiPRecoveryKey"] isEqualToNumber:@NO], "SecureBackupFMiPRecoveryKey is false");
    XCTAssertTrue([translateBack[@"SecureBackupIDMSRecovery"] isEqualToNumber:@NO], "SecureBackupIDMSRecovery false");
    XCTAssertTrue([translateBack[@"SecureBackupPassphrase"] isEqualToString: @"333333"], "SecureBackupPassphrase true");
    XCTAssertTrue([translateBack[@"SecureBackupSilentRecoveryAttempt"] isEqualToNumber:@YES], "SecureBackupSilentRecoveryAttempt true");
    XCTAssertTrue([translateBack[@"SecureBackupUseCachedPassphrase"] isEqualToNumber:@NO], "SecureBackupUseCachedPassphrase false");
    XCTAssertTrue([translateBack[@"SecureBackupUsesMultipleiCSCs"] isEqualToNumber:@YES], "SecureBackupUsesMultipleiCSCs true");
    XCTAssertTrue([translateBack[@"SecureBackupUsesRecoveryKey"] isEqualToNumber:@NO], "SecureBackupUsesRecoveryKey false");
}

@end


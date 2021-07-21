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
#import "OTEscrowTranslation.h"

#import <SoftLinking/SoftLinking.h>
#import <CloudServices/SecureBackup.h>
#import <CloudServices/SecureBackupConstants.h>
#import "keychain/ot/categories/OctagonEscrowRecoverer.h"
#import <OctagonTrust/OTEscrowRecordMetadata.h>
#import <OctagonTrust/OTEscrowRecordMetadataClientMetadata.h>
#import <Security/OTConstants.h>
#import "keychain/ot/OTClique+Private.h"
#import <utilities/debugging.h>

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CloudServices);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"

SOFT_LINK_CLASS(CloudServices, SecureBackup);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupErrorDomain, NSErrorDomain);

/* Escrow Authentication Information used for SRP*/
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationAppleID, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationPassword, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationiCloudEnvironment, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationAuthToken, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationEscrowProxyURL, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupIDMSRecoveryKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupFMiPRecoveryKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupFMiPUUIDKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAuthenticationDSID, NSString*);

/* CDP recovery information */
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUseCachedPassphraseKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupPassphraseKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecoveryKeyKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupContainsiCDPDataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupSilentRecoveryAttemptKey, NSString*);


/* Escrow Record Fields set by SecureBackup*/
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupIsEnabledKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUsesRecoveryKeyKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupLastBackupTimestampKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupLastBackupDateKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupEscrowTrustStatusKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordStatusKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordIDKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupPeerInfoDataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupPeerInfoSerialNumberKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupPeerInfoOSVersionKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAlliCDPRecordsKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordLabelKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupEscrowedSPKIKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupBottleIDKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupSerialNumberKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupBuildVersionKey, NSString*);

SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUsesRandomPassphraseKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUsesComplexPassphraseKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUsesNumericPassphraseKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupNumericPassphraseLengthKey, NSString*);

SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecoveryRequiresVerificationTokenKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupAccountIsHighSecurityKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupSMSTargetInfoKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupMetadataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupUsesMultipleiCSCKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupClientMetadataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupBottleValidityKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupEscrowDateKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRemainingAttemptsKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupCoolOffEndKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecoveryStatusKey, NSString*);

#pragma clang diagnostic pop

static NSString * const kCliqueSecureBackupTimestampKey                = @"com.apple.securebackup.timestamp";
static NSString * const kCliqueEscrowServiceRecordMetadataKey          = @"metadata";
static NSString * const kCliqueSecureBackupEncodedMetadataKey          = @"encodedMetadata";
static NSString * const kCliqueSecureBackupKeybagDigestKey             = @"BackupKeybagDigest";
static NSString * const kCliqueSecureBackupMetadataTimestampKey        = @"SecureBackupMetadataTimestamp";
static NSString * const kCliqueSecureBackupDeviceColor                 = @"device_color";
static NSString * const kCliqueSecureBackupDeviceEnclosureColor        = @"device_enclosure_color";
static NSString * const kCliqueSecureBackupDeviceMID                   = @"device_mid";
static NSString * const kCliqueSecureBackupDeviceModel                 = @"device_model";
static NSString * const kCliqueSecureBackupDeviceModelClass            = @"device_model_class";
static NSString * const kCliqueSecureBackupDeviceModelVersion          = @"device_model_version";
static NSString * const kCliqueSecureBackupDeviceName                  = @"device_name";
static NSString * const kCliqueSecureBackupDevicePlatform              = @"device_platform";
static NSString * const kCliqueSecureBackupSilentAttemptAllowed        = @"silentAttemptAllowed";
static NSString * const kCliqueSecureBackupFederationID                = @"FEDERATIONID";
static NSString * const kCliqueSecureBackupExpectedFederationID        = @"EXPECTEDFEDERATIONID";

@implementation OTEscrowTranslation

//dictionary to escrow auth
+ (OTEscrowAuthenticationInformation*) dictionaryToEscrowAuthenticationInfo:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }
    
    OTEscrowAuthenticationInformation* escrowAuthInfo = [[OTEscrowAuthenticationInformation alloc] init];
    escrowAuthInfo.authenticationAppleid = dictionary[getkSecureBackupAuthenticationAppleID()];
    escrowAuthInfo.authenticationAuthToken = dictionary[getkSecureBackupAuthenticationAuthToken()];
    escrowAuthInfo.authenticationDsid = dictionary[getkSecureBackupAuthenticationDSID()];
    escrowAuthInfo.authenticationEscrowproxyUrl = dictionary[getkSecureBackupAuthenticationEscrowProxyURL()];
    escrowAuthInfo.authenticationIcloudEnvironment = dictionary[getkSecureBackupAuthenticationiCloudEnvironment()];
    escrowAuthInfo.authenticationPassword = dictionary[getkSecureBackupAuthenticationPassword()];
    escrowAuthInfo.fmipUuid = dictionary[getkSecureBackupFMiPUUIDKey()];
    escrowAuthInfo.fmipRecovery = [dictionary[getkSecureBackupFMiPRecoveryKey()] boolValue];
    escrowAuthInfo.idmsRecovery = [dictionary[getkSecureBackupIDMSRecoveryKey()] boolValue];

    return escrowAuthInfo;
}

//escrow auth to dictionary
+ (NSDictionary*) escrowAuthenticationInfoToDictionary:(OTEscrowAuthenticationInformation*)escrowAuthInfo
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    if(![escrowAuthInfo.authenticationAppleid isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationAppleID()] = escrowAuthInfo.authenticationAppleid;
    }
    if(![escrowAuthInfo.authenticationAuthToken isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationAuthToken()] = escrowAuthInfo.authenticationAuthToken;
    }
    if(![escrowAuthInfo.authenticationDsid isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationDSID()] = escrowAuthInfo.authenticationDsid;
    }
    if(![escrowAuthInfo.authenticationEscrowproxyUrl isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationEscrowProxyURL()] = escrowAuthInfo.authenticationEscrowproxyUrl;
    }
    if(![escrowAuthInfo.authenticationIcloudEnvironment isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationiCloudEnvironment()] = escrowAuthInfo.authenticationIcloudEnvironment;
    }
    if(![escrowAuthInfo.authenticationPassword isEqualToString:@""]){
        dictionary[getkSecureBackupAuthenticationPassword()] = escrowAuthInfo.authenticationPassword;
    }
    if(![escrowAuthInfo.fmipUuid isEqualToString:@""]){
        dictionary[getkSecureBackupFMiPUUIDKey()] = escrowAuthInfo.fmipUuid;
    }
    dictionary[getkSecureBackupFMiPRecoveryKey()] = escrowAuthInfo.fmipRecovery ? @YES : @NO;
    dictionary[getkSecureBackupIDMSRecoveryKey()] = escrowAuthInfo.idmsRecovery ? @YES : @NO;

    return dictionary;
}

+ (OTCDPRecoveryInformation*)dictionaryToCDPRecoveryInformation:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    OTCDPRecoveryInformation* info = [[OTCDPRecoveryInformation alloc] init];
    info.recoverySecret = dictionary[getkSecureBackupPassphraseKey()];
    info.useCachedSecret = [dictionary[getkSecureBackupUseCachedPassphraseKey()] boolValue];
    info.recoveryKey = dictionary[getkSecureBackupRecoveryKeyKey()];
    info.usePreviouslyCachedRecoveryKey = [dictionary[getkSecureBackupUsesRecoveryKeyKey()] boolValue];
    info.silentRecoveryAttempt = [dictionary[@"SecureBackupSilentRecoveryAttempt"] boolValue];
    info.containsIcdpData =[dictionary[getkSecureBackupContainsiCDPDataKey()] boolValue];
    info.usesMultipleIcsc = [dictionary[getkSecureBackupUsesMultipleiCSCKey()] boolValue];
    return info;
}

+ (NSDictionary*)cdpRecoveryInformationToDictionary:(OTCDPRecoveryInformation*)info
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    dictionary[getkSecureBackupPassphraseKey()] = info.recoverySecret;
    dictionary[getkSecureBackupUseCachedPassphraseKey()] = info.useCachedSecret ? @YES : @NO;
    dictionary[getkSecureBackupRecoveryKeyKey()] = info.recoveryKey;
    dictionary[getkSecureBackupUsesRecoveryKeyKey()] = info.usePreviouslyCachedRecoveryKey ? @YES : @NO;
    dictionary[@"SecureBackupSilentRecoveryAttempt"] = info.silentRecoveryAttempt ? @YES : @NO;
    dictionary[getkSecureBackupContainsiCDPDataKey()] = info.containsIcdpData ? @YES : @NO;
    dictionary[getkSecureBackupUsesMultipleiCSCKey()] = info.usesMultipleIcsc ? @YES : @NO;
    
    return dictionary;
}

+ (NSDate *)_dateWithSecureBackupDateString:(NSString *)dateString
{
    NSDateFormatter *dateFormatter = [NSDateFormatter new];
    dateFormatter.dateFormat = @"dd-MM-yyyy HH:mm:ss";
    NSDate *ret = [dateFormatter dateFromString:dateString];

    if (ret) {
        return ret;
    }
    // New date format is GMT
    dateFormatter.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
    dateFormatter.dateFormat = @"yyyy-MM-dd HH:mm:ss";
    return [dateFormatter dateFromString:dateString];
}

+ (NSString*)_stringWithSecureBackupDate:(NSDate*) date
{
    NSDateFormatter *dateFormatter = [NSDateFormatter new];
    dateFormatter.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
    dateFormatter.dateFormat = @"yyyy-MM-dd HH:mm:ss";
    return [dateFormatter stringFromDate: date];
}

+ (OTEscrowRecordMetadata *) dictionaryToMetadata:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    OTEscrowRecordMetadata *metadata = [[OTEscrowRecordMetadata alloc] init];

    metadata.backupKeybagDigest = dictionary[kCliqueSecureBackupKeybagDigestKey];
    metadata.secureBackupUsesMultipleIcscs = [dictionary[getkSecureBackupUsesMultipleiCSCKey()] boolValue];
    metadata.bottleId = dictionary[getkSecureBackupBottleIDKey()];
    metadata.bottleValidity = dictionary[@"bottleValid"];
    NSDate* secureBackupTimestamp = [OTEscrowTranslation _dateWithSecureBackupDateString: dictionary[kCliqueSecureBackupTimestampKey]];

    metadata.secureBackupTimestamp = [secureBackupTimestamp timeIntervalSince1970];
    metadata.escrowedSpki = dictionary[getkSecureBackupEscrowedSPKIKey()];
    metadata.peerInfo = dictionary[getkSecureBackupPeerInfoDataKey()];
    metadata.serial = dictionary[getkSecureBackupSerialNumberKey()];

    NSDictionary* escrowInformationMetadataClientMetadata = dictionary[getkSecureBackupClientMetadataKey()];
    metadata.clientMetadata = [[OTEscrowRecordMetadataClientMetadata alloc] init];
    NSNumber *platform = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDevicePlatform];
    metadata.clientMetadata.devicePlatform = [platform longLongValue];

    NSDate* secureBackupMetadataTimestamp = [OTEscrowTranslation _dateWithSecureBackupDateString: escrowInformationMetadataClientMetadata[kCliqueSecureBackupMetadataTimestampKey]];
    metadata.clientMetadata.secureBackupMetadataTimestamp = [secureBackupMetadataTimestamp timeIntervalSince1970];

    NSNumber *passphraseLength = escrowInformationMetadataClientMetadata[getkSecureBackupNumericPassphraseLengthKey()];
    metadata.clientMetadata.secureBackupNumericPassphraseLength = [passphraseLength longLongValue];
    metadata.clientMetadata.secureBackupUsesComplexPassphrase = [escrowInformationMetadataClientMetadata[getkSecureBackupUsesComplexPassphraseKey()] boolValue];
    metadata.clientMetadata.secureBackupUsesNumericPassphrase = [escrowInformationMetadataClientMetadata[getkSecureBackupUsesNumericPassphraseKey()] boolValue];
    metadata.clientMetadata.deviceColor = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceColor];
    metadata.clientMetadata.deviceEnclosureColor = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceEnclosureColor];
    metadata.clientMetadata.deviceMid = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceMID];
    metadata.clientMetadata.deviceModel = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceModel];
    metadata.clientMetadata.deviceModelClass = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceModelClass];
    metadata.clientMetadata.deviceModelVersion = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceModelVersion];
    metadata.clientMetadata.deviceName = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDeviceName];

    return metadata;
}

//dictionary to escrow record
+ (OTEscrowRecord*) dictionaryToEscrowRecord:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    OTEscrowRecord* record = [[OTEscrowRecord alloc] init];
    NSDate* creationDate = dictionary[getkSecureBackupEscrowDateKey()];
    record.creationDate = [creationDate timeIntervalSince1970];
    NSDictionary* escrowInformationMetadata = dictionary[kCliqueEscrowServiceRecordMetadataKey];
    record.escrowInformationMetadata = [OTEscrowTranslation dictionaryToMetadata:escrowInformationMetadata];

    NSNumber *remainingAttempts = dictionary[getkSecureBackupRemainingAttemptsKey()];

    record.remainingAttempts = [remainingAttempts longLongValue];
    record.label = dictionary[getkSecureBackupRecordLabelKey()];
    record.recordStatus = [dictionary[getkSecureBackupRecordStatusKey()] isEqualToString:@"valid"] ? OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID : OTEscrowRecord_RecordStatus_RECORD_STATUS_INVALID;
    record.silentAttemptAllowed = [dictionary[kCliqueSecureBackupSilentAttemptAllowed] boolValue];
    record.federationId = dictionary[kCliqueSecureBackupFederationID];
    record.expectedFederationId = dictionary[kCliqueSecureBackupExpectedFederationID];
    record.recordId = dictionary[getkSecureBackupRecordIDKey()];
    record.serialNumber = dictionary[getkSecureBackupPeerInfoSerialNumberKey()];
    if(dictionary[getkSecureBackupCoolOffEndKey()]) {
        record.coolOffEnd = [dictionary[getkSecureBackupCoolOffEndKey()] longLongValue];
    }
    record.recoveryStatus = [dictionary[getkSecureBackupRecoveryStatusKey()] intValue];
    return record;
}

+ (NSDictionary *) metadataToDictionary:(OTEscrowRecordMetadata*)metadata
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
    dictionary[getkSecureBackupClientMetadataKey()] = [NSMutableDictionary dictionary];

    dictionary[kCliqueSecureBackupKeybagDigestKey] = metadata.backupKeybagDigest;
    dictionary[getkSecureBackupUsesMultipleiCSCKey()]  = [[NSNumber alloc]initWithUnsignedLongLong:metadata.secureBackupUsesMultipleIcscs];
    dictionary[getkSecureBackupBottleIDKey()] = metadata.bottleId;
    dictionary[@"bottleValid"] = metadata.bottleValidity;
    dictionary[kCliqueSecureBackupTimestampKey]  = [OTEscrowTranslation _stringWithSecureBackupDate: [NSDate dateWithTimeIntervalSince1970: metadata.secureBackupTimestamp]];
    dictionary[getkSecureBackupEscrowedSPKIKey()] = metadata.escrowedSpki;
    dictionary[getkSecureBackupPeerInfoDataKey()] = metadata.peerInfo;
    dictionary[getkSecureBackupSerialNumberKey()] = metadata.serial;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDevicePlatform] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.devicePlatform];
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupMetadataTimestampKey] = [OTEscrowTranslation _stringWithSecureBackupDate: [NSDate dateWithTimeIntervalSince1970: metadata.clientMetadata.secureBackupMetadataTimestamp]];
    dictionary[getkSecureBackupClientMetadataKey()][getkSecureBackupNumericPassphraseLengthKey()] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupNumericPassphraseLength];
    dictionary[getkSecureBackupClientMetadataKey()][getkSecureBackupUsesComplexPassphraseKey()] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupUsesComplexPassphrase];
    dictionary[getkSecureBackupClientMetadataKey()][getkSecureBackupUsesNumericPassphraseKey()] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupUsesNumericPassphrase];
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceColor] = metadata.clientMetadata.deviceColor;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceEnclosureColor] = metadata.clientMetadata.deviceEnclosureColor;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceMID] = metadata.clientMetadata.deviceMid;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceModel] = metadata.clientMetadata.deviceModel;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceModelClass] = metadata.clientMetadata.deviceModelClass;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceModelVersion] = metadata.clientMetadata.deviceModelVersion;
    dictionary[getkSecureBackupClientMetadataKey()][kCliqueSecureBackupDeviceName] = metadata.clientMetadata.deviceName;

    return dictionary;
}

//escrow record to dictionary
+ (NSDictionary*) escrowRecordToDictionary:(OTEscrowRecord*)escrowRecord
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    dictionary[getkSecureBackupEscrowDateKey()] = [NSDate dateWithTimeIntervalSince1970: escrowRecord.creationDate];

    dictionary[kCliqueEscrowServiceRecordMetadataKey] = [OTEscrowTranslation metadataToDictionary: escrowRecord.escrowInformationMetadata];

    dictionary[getkSecureBackupRemainingAttemptsKey()] = [[NSNumber alloc]initWithUnsignedLongLong:escrowRecord.remainingAttempts];
    dictionary[getkSecureBackupRecordLabelKey()] = escrowRecord.label;
    dictionary[getkSecureBackupRecordStatusKey()] = escrowRecord.recordStatus == OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID ? @"valid" : @"invalid";
    dictionary[kCliqueSecureBackupSilentAttemptAllowed] = [[NSNumber alloc] initWithUnsignedLongLong: escrowRecord.silentAttemptAllowed];
    dictionary[kCliqueSecureBackupFederationID] = escrowRecord.federationId;
    dictionary[kCliqueSecureBackupExpectedFederationID] = escrowRecord.expectedFederationId;
    dictionary[getkSecureBackupRecordIDKey()] = escrowRecord.recordId;
    dictionary[getkSecureBackupPeerInfoSerialNumberKey()] = escrowRecord.serialNumber;
    dictionary[getkSecureBackupCoolOffEndKey()] = @(escrowRecord.coolOffEnd);
    dictionary[getkSecureBackupRecoveryStatusKey()] = @(escrowRecord.recoveryStatus);

    return dictionary;
}

+ (OTICDPRecordContext*)dictionaryToCDPRecordContext:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    OTICDPRecordContext* context = [[OTICDPRecordContext alloc] init];
    context.authInfo = [OTEscrowTranslation dictionaryToEscrowAuthenticationInfo:dictionary];
    context.cdpInfo = [OTEscrowTranslation dictionaryToCDPRecoveryInformation:dictionary];

    return context;
}

+ (NSDictionary*)CDPRecordContextToDictionary:(OTICDPRecordContext*)context
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];

    [dictionary addEntriesFromDictionary:[OTEscrowTranslation escrowAuthenticationInfoToDictionary:context.authInfo]];
    [dictionary addEntriesFromDictionary:[OTEscrowTranslation cdpRecoveryInformationToDictionary:context.cdpInfo]];

    return dictionary;
}

+ (BOOL)supportedRestorePath:(OTICDPRecordContext *)cdpContext
{
    return (cdpContext.authInfo.idmsRecovery == false
            && (cdpContext.authInfo.fmipUuid == nil || [cdpContext.authInfo.fmipUuid isEqualToString:@""])
            && cdpContext.authInfo.fmipRecovery == false
            && (cdpContext.cdpInfo.recoveryKey == nil || [cdpContext.cdpInfo.recoveryKey isEqualToString:@""])
            && cdpContext.cdpInfo.usePreviouslyCachedRecoveryKey == false);
}

@end


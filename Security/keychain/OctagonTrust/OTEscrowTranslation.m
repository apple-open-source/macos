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

#import <SoftLinking/WeakLinking.h>
#import <CloudServices/SecureBackup.h>
#import <CloudServices/SecureBackupConstants.h>
#import "keychain/ot/categories/OctagonEscrowRecoverer.h"
#import <OctagonTrust/OTEscrowRecordMetadata.h>
#import <OctagonTrust/OTEscrowRecordMetadataClientMetadata.h>
#import <OctagonTrust/OTEscrowRecordMetadataPasscodeGeneration.h>
#import <Security/OTConstants.h>
#import "keychain/ot/OTClique+Private.h"
#import <utilities/debugging.h>

/* Escrow Authentication Information used for SRP*/
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationAppleID);
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationPassword);
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationiCloudEnvironment);
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationAuthToken);
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationEscrowProxyURL);
WEAK_LINK_FORCE_IMPORT(kSecureBackupIDMSRecoveryKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupFMiPRecoveryKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupFMiPUUIDKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupAuthenticationDSID);

/* CDP recovery information */
WEAK_LINK_FORCE_IMPORT(kSecureBackupUseCachedPassphraseKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupPassphraseKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRecoveryKeyKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupContainsiCDPDataKey);

/* Escrow Record Fields set by SecureBackup*/
WEAK_LINK_FORCE_IMPORT(kSecureBackupUsesRecoveryKeyKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRecordStatusKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRecordIDKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupPeerInfoDataKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupPeerInfoSerialNumberKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRecordLabelKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupEscrowedSPKIKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupBottleIDKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupSerialNumberKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupPasscodeGenerationKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupBuildVersionKey);

WEAK_LINK_FORCE_IMPORT(kSecureBackupUsesComplexPassphraseKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupUsesNumericPassphraseKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupNumericPassphraseLengthKey);

WEAK_LINK_FORCE_IMPORT(kSecureBackupUsesMultipleiCSCKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupClientMetadataKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupEscrowDateKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRemainingAttemptsKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupCoolOffEndKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupRecoveryStatusKey);

WEAK_LINK_FORCE_IMPORT(kSecureBackupSilentRecoveryAttemptKey);
WEAK_LINK_FORCE_IMPORT(kSecureBackupNonViableRepairKey);

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
    escrowAuthInfo.authenticationAppleid = dictionary[kSecureBackupAuthenticationAppleID];
    escrowAuthInfo.authenticationAuthToken = dictionary[kSecureBackupAuthenticationAuthToken];
    escrowAuthInfo.authenticationDsid = dictionary[kSecureBackupAuthenticationDSID];
    escrowAuthInfo.authenticationEscrowproxyUrl = dictionary[kSecureBackupAuthenticationEscrowProxyURL];
    escrowAuthInfo.authenticationIcloudEnvironment = dictionary[kSecureBackupAuthenticationiCloudEnvironment];
    escrowAuthInfo.authenticationPassword = dictionary[kSecureBackupAuthenticationPassword];
    escrowAuthInfo.fmipUuid = dictionary[kSecureBackupFMiPUUIDKey];
    escrowAuthInfo.fmipRecovery = [dictionary[kSecureBackupFMiPRecoveryKey] boolValue];
    escrowAuthInfo.idmsRecovery = [dictionary[kSecureBackupIDMSRecoveryKey] boolValue];

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
        dictionary[kSecureBackupAuthenticationAppleID] = escrowAuthInfo.authenticationAppleid;
    }
    if(![escrowAuthInfo.authenticationAuthToken isEqualToString:@""]){
        dictionary[kSecureBackupAuthenticationAuthToken] = escrowAuthInfo.authenticationAuthToken;
    }
    if(![escrowAuthInfo.authenticationDsid isEqualToString:@""]){
        dictionary[kSecureBackupAuthenticationDSID] = escrowAuthInfo.authenticationDsid;
    }
    if(![escrowAuthInfo.authenticationEscrowproxyUrl isEqualToString:@""]){
        dictionary[kSecureBackupAuthenticationEscrowProxyURL] = escrowAuthInfo.authenticationEscrowproxyUrl;
    }
    if(![escrowAuthInfo.authenticationIcloudEnvironment isEqualToString:@""]){
        dictionary[kSecureBackupAuthenticationiCloudEnvironment] = escrowAuthInfo.authenticationIcloudEnvironment;
    }
    if(![escrowAuthInfo.authenticationPassword isEqualToString:@""]){
        dictionary[kSecureBackupAuthenticationPassword] = escrowAuthInfo.authenticationPassword;
    }
    if(![escrowAuthInfo.fmipUuid isEqualToString:@""]){
        dictionary[kSecureBackupFMiPUUIDKey] = escrowAuthInfo.fmipUuid;
    }
    dictionary[kSecureBackupFMiPRecoveryKey] = escrowAuthInfo.fmipRecovery ? @YES : @NO;
    dictionary[kSecureBackupIDMSRecoveryKey] = escrowAuthInfo.idmsRecovery ? @YES : @NO;

    return dictionary;
}

+ (OTCDPRecoveryInformation*)dictionaryToCDPRecoveryInformation:(NSDictionary*)dictionary
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    OTCDPRecoveryInformation* info = [[OTCDPRecoveryInformation alloc] init];
    info.recoverySecret = dictionary[kSecureBackupPassphraseKey];
    info.useCachedSecret = [dictionary[kSecureBackupUseCachedPassphraseKey] boolValue];
    info.recoveryKey = dictionary[kSecureBackupRecoveryKeyKey];
    info.usePreviouslyCachedRecoveryKey = [dictionary[kSecureBackupUsesRecoveryKeyKey] boolValue];
    info.silentRecoveryAttempt = [dictionary[kSecureBackupSilentRecoveryAttemptKey] boolValue];
    info.containsIcdpData =[dictionary[kSecureBackupContainsiCDPDataKey] boolValue];
    info.usesMultipleIcsc = [dictionary[kSecureBackupUsesMultipleiCSCKey] boolValue];
    info.nonViableRepair = [dictionary[kSecureBackupNonViableRepairKey] boolValue];
    return info;
}

+ (NSDictionary*)cdpRecoveryInformationToDictionary:(OTCDPRecoveryInformation*)info
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    dictionary[kSecureBackupPassphraseKey] = info.recoverySecret;
    dictionary[kSecureBackupUseCachedPassphraseKey] = info.useCachedSecret ? @YES : @NO;
    dictionary[kSecureBackupRecoveryKeyKey] = info.recoveryKey;
    dictionary[kSecureBackupUsesRecoveryKeyKey] = info.usePreviouslyCachedRecoveryKey ? @YES : @NO;
    dictionary[kSecureBackupSilentRecoveryAttemptKey] = info.silentRecoveryAttempt ? @YES : @NO;
    dictionary[kSecureBackupContainsiCDPDataKey] = info.containsIcdpData ? @YES : @NO;
    dictionary[kSecureBackupUsesMultipleiCSCKey] = info.usesMultipleIcsc ? @YES : @NO;
    dictionary[kSecureBackupNonViableRepairKey] = info.nonViableRepair ? @YES : @NO;
    
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
    metadata.secureBackupUsesMultipleIcscs = [dictionary[kSecureBackupUsesMultipleiCSCKey] boolValue];
    metadata.bottleId = dictionary[kSecureBackupBottleIDKey];
    metadata.bottleValidity = dictionary[@"bottleValid"];
    NSDate* secureBackupTimestamp = [OTEscrowTranslation _dateWithSecureBackupDateString: dictionary[kCliqueSecureBackupTimestampKey]];

    metadata.secureBackupTimestamp = [secureBackupTimestamp timeIntervalSince1970];
    metadata.escrowedSpki = dictionary[kSecureBackupEscrowedSPKIKey];
    metadata.peerInfo = dictionary[kSecureBackupPeerInfoDataKey];
    metadata.serial = dictionary[kSecureBackupSerialNumberKey];
    metadata.build = dictionary[kSecureBackupBuildVersionKey];
    if (dictionary[kSecureBackupPasscodeGenerationKey]) {
        metadata.passcodeGeneration = [[OTEscrowRecordMetadataPasscodeGeneration alloc] init];
        NSNumber* passcodeGeneration = dictionary[kSecureBackupPasscodeGenerationKey];
        metadata.passcodeGeneration.value = [passcodeGeneration longLongValue];
    }

    NSDictionary* escrowInformationMetadataClientMetadata = dictionary[kSecureBackupClientMetadataKey];
    metadata.clientMetadata = [[OTEscrowRecordMetadataClientMetadata alloc] init];
    NSNumber *platform = escrowInformationMetadataClientMetadata[kCliqueSecureBackupDevicePlatform];
    metadata.clientMetadata.devicePlatform = [platform longLongValue];

    NSDate* secureBackupMetadataTimestamp = [OTEscrowTranslation _dateWithSecureBackupDateString: escrowInformationMetadataClientMetadata[kCliqueSecureBackupMetadataTimestampKey]];
    metadata.clientMetadata.secureBackupMetadataTimestamp = [secureBackupMetadataTimestamp timeIntervalSince1970];

    NSNumber *passphraseLength = escrowInformationMetadataClientMetadata[kSecureBackupNumericPassphraseLengthKey];
    metadata.clientMetadata.secureBackupNumericPassphraseLength = [passphraseLength longLongValue];
    metadata.clientMetadata.secureBackupUsesComplexPassphrase = [escrowInformationMetadataClientMetadata[kSecureBackupUsesComplexPassphraseKey] boolValue];
    metadata.clientMetadata.secureBackupUsesNumericPassphrase = [escrowInformationMetadataClientMetadata[kSecureBackupUsesNumericPassphraseKey] boolValue];
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
    NSDate* creationDate = dictionary[kSecureBackupEscrowDateKey];
    record.creationDate = [creationDate timeIntervalSince1970];
    NSDictionary* escrowInformationMetadata = dictionary[kCliqueEscrowServiceRecordMetadataKey];
    record.escrowInformationMetadata = [OTEscrowTranslation dictionaryToMetadata:escrowInformationMetadata];

    NSNumber *remainingAttempts = dictionary[kSecureBackupRemainingAttemptsKey];

    record.remainingAttempts = [remainingAttempts longLongValue];
    record.label = dictionary[kSecureBackupRecordLabelKey];
    record.recordStatus = [dictionary[kSecureBackupRecordStatusKey] isEqualToString:@"valid"] ? OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID : OTEscrowRecord_RecordStatus_RECORD_STATUS_INVALID;
    record.silentAttemptAllowed = [dictionary[kCliqueSecureBackupSilentAttemptAllowed] boolValue];
    record.federationId = dictionary[kCliqueSecureBackupFederationID];
    record.expectedFederationId = dictionary[kCliqueSecureBackupExpectedFederationID];
    record.recordId = dictionary[kSecureBackupRecordIDKey];
    record.serialNumber = dictionary[kSecureBackupPeerInfoSerialNumberKey];
    if(dictionary[kSecureBackupCoolOffEndKey]) {
        record.coolOffEnd = [dictionary[kSecureBackupCoolOffEndKey] longLongValue];
    }
    record.recoveryStatus = [dictionary[kSecureBackupRecoveryStatusKey] intValue];
    return record;
}

+ (NSDictionary *) metadataToDictionary:(OTEscrowRecordMetadata*)metadata
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
    dictionary[kSecureBackupClientMetadataKey] = [NSMutableDictionary dictionary];

    dictionary[kCliqueSecureBackupKeybagDigestKey] = metadata.backupKeybagDigest;
    dictionary[kSecureBackupUsesMultipleiCSCKey]  = [[NSNumber alloc]initWithUnsignedLongLong:metadata.secureBackupUsesMultipleIcscs];
    dictionary[kSecureBackupBottleIDKey] = metadata.bottleId;
    dictionary[@"bottleValid"] = metadata.bottleValidity;
    dictionary[kCliqueSecureBackupTimestampKey]  = [OTEscrowTranslation _stringWithSecureBackupDate: [NSDate dateWithTimeIntervalSince1970: metadata.secureBackupTimestamp]];
    dictionary[kSecureBackupEscrowedSPKIKey] = metadata.escrowedSpki;
    dictionary[kSecureBackupPeerInfoDataKey] = metadata.peerInfo;
    dictionary[kSecureBackupSerialNumberKey] = metadata.serial;
    dictionary[kSecureBackupBuildVersionKey] = metadata.build;
    if (metadata.passcodeGeneration.hasValue) {
        dictionary[kSecureBackupPasscodeGenerationKey] = @(metadata.passcodeGeneration.value);
    }
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDevicePlatform] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.devicePlatform];
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupMetadataTimestampKey] = [OTEscrowTranslation _stringWithSecureBackupDate: [NSDate dateWithTimeIntervalSince1970: metadata.clientMetadata.secureBackupMetadataTimestamp]];
    dictionary[kSecureBackupClientMetadataKey][kSecureBackupNumericPassphraseLengthKey] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupNumericPassphraseLength];
    dictionary[kSecureBackupClientMetadataKey][kSecureBackupUsesComplexPassphraseKey] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupUsesComplexPassphrase];
    dictionary[kSecureBackupClientMetadataKey][kSecureBackupUsesNumericPassphraseKey] = [[NSNumber alloc]initWithUnsignedLongLong: metadata.clientMetadata.secureBackupUsesNumericPassphrase];
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceColor] = metadata.clientMetadata.deviceColor;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceEnclosureColor] = metadata.clientMetadata.deviceEnclosureColor;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceMID] = metadata.clientMetadata.deviceMid;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceModel] = metadata.clientMetadata.deviceModel;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceModelClass] = metadata.clientMetadata.deviceModelClass;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceModelVersion] = metadata.clientMetadata.deviceModelVersion;
    dictionary[kSecureBackupClientMetadataKey][kCliqueSecureBackupDeviceName] = metadata.clientMetadata.deviceName;

    return dictionary;
}

//escrow record to dictionary
+ (NSDictionary*) escrowRecordToDictionary:(OTEscrowRecord*)escrowRecord
{
    if ([OTClique isCloudServicesAvailable] == NO) {
        return nil;
    }

    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    dictionary[kSecureBackupEscrowDateKey] = [NSDate dateWithTimeIntervalSince1970: escrowRecord.creationDate];

    dictionary[kCliqueEscrowServiceRecordMetadataKey] = [OTEscrowTranslation metadataToDictionary: escrowRecord.escrowInformationMetadata];

    dictionary[kSecureBackupRemainingAttemptsKey] = [[NSNumber alloc]initWithUnsignedLongLong:escrowRecord.remainingAttempts];
    dictionary[kSecureBackupRecordLabelKey] = escrowRecord.label;
    dictionary[kSecureBackupRecordStatusKey] = escrowRecord.recordStatus == OTEscrowRecord_RecordStatus_RECORD_STATUS_VALID ? @"valid" : @"invalid";
    dictionary[kCliqueSecureBackupSilentAttemptAllowed] = [[NSNumber alloc] initWithUnsignedLongLong: escrowRecord.silentAttemptAllowed];
    dictionary[kCliqueSecureBackupFederationID] = escrowRecord.federationId;
    dictionary[kCliqueSecureBackupExpectedFederationID] = escrowRecord.expectedFederationId;
    dictionary[kSecureBackupRecordIDKey] = escrowRecord.recordId;
    dictionary[kSecureBackupPeerInfoSerialNumberKey] = escrowRecord.serialNumber;
    dictionary[kSecureBackupCoolOffEndKey] = @(escrowRecord.coolOffEnd);
    dictionary[kSecureBackupRecoveryStatusKey] = @(escrowRecord.recoveryStatus);

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

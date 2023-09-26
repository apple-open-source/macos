//
//  SecCMS.m
//  Security
//
//  This file implements the SecCMS shims using MessageSecurity.fw.
//  See SecCMS.c for shims implemented using Security.fw

#import <Foundation/Foundation.h>
#import "SecCMSInternal.h"
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>
#include <os/feature_private.h>
#import <MessageSecurity/MessageSecurity.h>

#include "secoid.h"

static bool isMessageSecurityAllowedForCurrentBundleID(void) {
    static dispatch_once_t onceToken;
    static bool blockedBundleIDFound = false;
    dispatch_once(&onceToken, ^{
        CFBundleRef bundle = CFBundleGetMainBundle();
        if (bundle) {
            CFStringRef bundleID = CFBundleGetIdentifier(bundle);
            blockedBundleIDFound = (bundleID != NULL) &&
                 /* Enterprise-related processes on macOS (mdmclient, CertificateService) */
                (CFStringHasPrefix(bundleID, CFSTR("com.apple.mdmclient")) ||
                 CFStringHasPrefix(bundleID, CFSTR("com.apple.managedclient.pds.Certificate")) ||
                 /* Enterprise-related processes on iOS / tvOS / watchOS (profiled, remotemanagementd, RemoteManagementAgent) */
                 CFStringHasPrefix(bundleID, CFSTR("com.apple.managedconfiguration.profiled")) ||
                 CFStringHasPrefix(bundleID, CFSTR("com.apple.remotemanagementd")) ||
                 CFStringHasPrefix(bundleID, CFSTR("com.apple.RemoteManagementAgent")));
        }
        secnotice("SecCMS", "isMessageSecurityAllowedForCurrentBundleID %s",
                  (blockedBundleIDFound == false) ? "true" : "false");
    });
    return (blockedBundleIDFound == false);
}

bool useMessageSecurityEnabled(void) {
    static bool useMSEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        useMSEnabled = os_feature_enabled(Security, SecCMSMessageSecurityShim);
        secnotice("SecCMS", "SecCMSMessageSecurityShim is %s (via feature flags)",
                  useMSEnabled ? "enabled" : "disabled");
    });
    return useMSEnabled && isMessageSecurityAllowedForCurrentBundleID();
}

CFArrayRef MS_SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message) {
    NSError *error = nil;
    MSCMSContentInfo *cinfo = [MSCMSContentInfo decodeMessageSecurityObject:(__bridge NSData *)message options:nil error:&error];
    if (!cinfo) {
        secerror("failed to decode CMS message: %@", error);
        return NULL;
    }
    if (![cinfo.contentType isEqualToString:MSCMSContentTypeSignedData] ||
        !cinfo.embeddedContent ||
        ![cinfo.embeddedContent isKindOfClass:[MSCMSSignedData class]]) {
        secerror("CMS message does not contain a SignedData");
        return NULL;
    }
    MSCMSSignedData *sigd = (MSCMSSignedData *)cinfo.embeddedContent;
    if ([[sigd signers] count] > 0) {
        secerror("certs-only message has signers");
        return NULL;
    }
    NSMutableSet *certificates = [sigd certificates];
    if ([certificates count] < 1) {
        return NULL;
    }
    return CFBridgingRetain([certificates allObjects]);
}

static NSDate *getSigningTime(MSCMSSignerInfo *signer) {
    MSCMSMutableAttributeArray *signingTimes = [signer getAttributesWithType:[MSOID OIDWithString:MSCMSAttributeTypeSigningTime error:nil]];
    if (!signingTimes) {
        return nil;
    }
    if (signingTimes.count > 1) {
        secwarning("too many signing time attributes (%d), skipping", (int)signingTimes.count);
        return nil;
    }
    NSError *localError = nil;
    id <MSCMSAttributeCoder>attr = signingTimes[0];
    MSCMSSigningTimeAttribute *signingTime = nil;
    if ([attr isKindOfClass:[MSCMSAttribute class]]) {
        signingTime = [[MSCMSSigningTimeAttribute alloc] initWithAttribute:(MSCMSAttribute *)attr error:&localError];
    } else if ([attr isKindOfClass:[MSCMSSigningTimeAttribute class]]) {
        signingTime = (MSCMSSigningTimeAttribute *)attr;
    }
    if (!signingTime) {
        secwarning("skipping signing time that failed to decode: %@", localError);
        return nil;
    }
    return signingTime.signingTime;
}

static void addSigningTimeAttribute(MSCMSSignerInfo *signer, NSMutableDictionary *returnedAttributes)
{
    returnedAttributes[(__bridge NSString *)kSecCMSSignDate] = getSigningTime(signer);
}

static void addHashAgility(MSCMSSignerInfo *signer, NSMutableDictionary *returnedAttributes)
{
    MSCMSMutableAttributeArray *hashAgilities = [signer getAttributesWithType:[MSOID OIDWithString:MSCMSAttributeTypeAppleHashAgility error:nil]];
    if (!hashAgilities) {
        return;
    }
    if (hashAgilities.count > 1) {
        secwarning("too many hash agility attributes (%d), skipping", (int)hashAgilities.count);
        return;
    }
    NSError *localError = nil;
    id <MSCMSAttributeCoder>attr = hashAgilities[0];
    MSCMSAppleHashAgilityAttribute *hashAgility = nil;
    if ([attr isKindOfClass:[MSCMSAttribute class]]) {
        hashAgility = [[MSCMSAppleHashAgilityAttribute alloc] initWithAttribute:(MSCMSAttribute *)attr error:&localError];
    } else if ([attr isKindOfClass:[MSCMSAppleHashAgilityAttribute class]]) {
        hashAgility = (MSCMSAppleHashAgilityAttribute *)attr;
    }
    if (!hashAgility) {
        secwarning("skipping hash agility attribute that failed to decode: %@", localError);
        return;
    }
    returnedAttributes[(__bridge NSString *)kSecCMSHashAgility] = hashAgility.hashAgilityValue;
}

static NSDictionary *changeOIDsToTags(NSDictionary <MSOIDString, NSData *>*hashAgilityValues)
{
    NSMutableDictionary <NSNumber *, NSData *>*tagKeyedDictionary = [NSMutableDictionary dictionaryWithCapacity:hashAgilityValues.count];
    for (NSString *oidString in hashAgilityValues) {
        NSError *localError =nil;
        MSOID *oid = [MSOID OIDWithString:oidString error:&localError];
        if (!oid) {
            secwarning("skipping hash agility entry with invalid OID: %@", oidString);
            continue;
        }
        NSData *oidData = oid.OIDBytes;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SecAsn1Item oidItem = { .Data = (uint8_t *)oidData.bytes, .Length = oidData.length };
        int64_t tag = SECOID_FindOIDTag(&oidItem);
#pragma clang diagnostic pop
        [tagKeyedDictionary setObject:hashAgilityValues[oidString] forKey:@(tag)];
    }
    return [NSDictionary dictionaryWithDictionary:tagKeyedDictionary];
}

static void addHashAgilityV2(MSCMSSignerInfo *signer, NSMutableDictionary *returnedAttributes)
{
    MSCMSMutableAttributeArray *hashAgilitiesV2 = [signer getAttributesWithType:[MSOID OIDWithString:MSCMSAttributeTypeAppleHashAgilityV2 error:nil]];
    if (!hashAgilitiesV2) {
        return;
    }
    if (hashAgilitiesV2.count > 1) {
        secwarning("too many hash agility V2 attributes (%d), skipping", (int)hashAgilitiesV2.count);
        return;
    }
    NSError *localError = nil;
    id <MSCMSAttributeCoder>attr = hashAgilitiesV2[0];
    MSCMSAppleHashAgilityV2Attribute *hashAgilityV2 = nil;
    if ([attr isKindOfClass:[MSCMSAttribute class]]) {
        hashAgilityV2 = [[MSCMSAppleHashAgilityV2Attribute alloc] initWithAttribute:(MSCMSAttribute *)attr error:&localError];
    } else if ([attr isKindOfClass:[MSCMSAppleHashAgilityV2Attribute class]]) {
        hashAgilityV2 = (MSCMSAppleHashAgilityV2Attribute *)attr;
    }
    if (!hashAgilityV2) {
        secwarning("skipping hash agility V2 attribute that failed to decode: %@", localError);
        return;
    }
    returnedAttributes[(__bridge NSString *)kSecCMSHashAgilityV2] = changeOIDsToTags(hashAgilityV2.hashAgilityValues);
}

static void addExpirationTime(MSCMSSignerInfo *signer, NSMutableDictionary *returnedAttributes)
{
    MSCMSMutableAttributeArray *expirationTimes = [signer getAttributesWithType:[MSOID OIDWithString:MSCMSAttributeTypeAppleExpirationTime error:nil]];
    if (!expirationTimes) {
        return;
    }
    if (expirationTimes.count > 1) {
        secwarning("too many expiration time attributes (%d), skipping", (int)expirationTimes.count);
        return;
    }
    NSError *localError = nil;
    id <MSCMSAttributeCoder>attr = expirationTimes[0];
    MSCMSAppleExpirationTimeAttribute *expirationTime = nil;
    if ([attr isKindOfClass:[MSCMSAttribute class]]) {
        expirationTime = [[MSCMSAppleExpirationTimeAttribute alloc] initWithAttribute:(MSCMSAttribute *)attr error:&localError];
    } else if ([attr isKindOfClass:[MSCMSAppleExpirationTimeAttribute class]]) {
        expirationTime = (MSCMSAppleExpirationTimeAttribute *)attr;
    }
    if (!expirationTime) {
        secwarning("skipping expration time that failed to decode: %@", localError);
        return;
    }
    returnedAttributes[(__bridge NSString *)kSecCMSExpirationDate] = expirationTime.expirationTime;
}

static void addSignedAttribues(MSCMSSignedData *sigd, CFDictionaryRef *signed_attributes)
{
    NSMutableDictionary *returnedAttributes = [NSMutableDictionary dictionary];
    if (sigd.certificates) {
        returnedAttributes[(__bridge NSString*)kSecCMSAllCerts] = [sigd.certificates allObjects];
    }

    // Add all attributes
    MSCMSSignerInfo *signer = [sigd signers][0];
    for (MSCMSAttribute *attribute in signer.protectedAttributes) {
        /* add all attributes */
        NSData *type = attribute.attributeType.OIDBytes;
        NSMutableArray *attrValues = [attribute.attributeValues mutableCopy];
        NSMutableArray *existingValues = [returnedAttributes objectForKey:type];
        if (existingValues) {
            [existingValues addObjectsFromArray:attrValues];
        } else {
            [returnedAttributes setObject:attrValues forKey:type];
        }
    }

    // Workaround for rdar://86134698 (getAttributesWithType gets all attributes; need way to get protected only)
    [signer.unprotectedAttributes removeAllObjects];

    // Add "cooked attributes
    addSigningTimeAttribute(signer, returnedAttributes);
    addHashAgility(signer, returnedAttributes);
    addHashAgilityV2(signer, returnedAttributes);
    addExpirationTime(signer, returnedAttributes);

    *signed_attributes = CFBridgingRetain([NSDictionary dictionaryWithDictionary:returnedAttributes]);
}

OSStatus MS_SecCMSVerifySignedData_internal(CFDataRef message, CFDataRef detached_contents,
                                            CFTypeRef policy, SecTrustRef *trustref, CFArrayRef additional_certs,
                                            CFDataRef *attached_contents, CFDictionaryRef *signed_attributes)
{
    NSError *error = nil;
    MSDecodeOptions *options = nil;
    if (additional_certs) {
        options = [[MSDecodeOptions alloc] init];
        [options setAdditionalCertificates:(__bridge NSArray*)additional_certs];
    }
    MSCMSContentInfo *cinfo = [MSCMSContentInfo decodeMessageSecurityObject:(__bridge NSData *)message options:options error:&error];
    if (!cinfo) {
        secerror("failed to decode CMS message: %@", error);
        return errSecAuthFailed;
    }
    if (![cinfo.contentType isEqualToString:MSCMSContentTypeSignedData] ||
        !cinfo.embeddedContent ||
        ![cinfo.embeddedContent isKindOfClass:[MSCMSSignedData class]]) {
        secerror("CMS message does not contain a SignedData");
        return errSecDecode;
    }
    MSCMSSignedData *sigd = (MSCMSSignedData *)cinfo.embeddedContent;

    if (detached_contents) {
        if (!sigd.detached) {
            secwarning("CMS message has attached content but caller passed in detached contents, using detached contents");
        }
        if (![sigd.contentType isEqualToString:MSCMSContentTypeData]) {
            secwarning("Caller passed a detached content by the content is not a data type.");
        }
        [sigd setDataContent:(__bridge NSData *)detached_contents];
    }

    if (policy) {
        NSArray *policies = nil;
        if (CFGetTypeID(policy) == SecPolicyGetTypeID()) {
            policies = @[(__bridge id)policy];
        } else if (CFGetTypeID(policy) == CFArrayGetTypeID()) {
            policies = (__bridge NSArray*)policy;
        } else {
            secerror("policy is not a SecPolicy or CFArr0ay");
            return errSecParam;
        }
        if ([sigd signers].count != 1) {
            secerror("CMS message has %d signers, expected 1", (int)[sigd signers].count);
            return errSecAuthFailed;
        }
        MSCMSSignerInfo *signer = [sigd signers][0];
        // TODO: rdar://28550104 (Implement Timestamping) need to choose timestamp date over signingTime
        // The legacy implementation verifies against the asserted SigningTime
        NSDate *verifyDate = getSigningTime(signer);
        // The legacy implementation does not verify the signers if a trust ref is requested
        if (trustref) {
            if (![sigd verifySignatures:&error]) {
                secerror("CMS signature verification failed: %@", error);
                return errSecAuthFailed;
            }
            SecTrustRef trust = [signer createTrustObjectWithPolicies:policies error:&error];
            if (!trust) {
                secerror("Failed to create trust ref to verify signer: %@", error);
                return (OSStatus)error.code;
            }
            if (verifyDate) {
                SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate);
            }
            *trustref = trust;
        } else { // otherwise verify everything for the caller
            if (![sigd verifySignaturesAndSignersWithPolicies:policies verifyTime:verifyDate error:&error]) {
                secerror("Signature/signer verification failed: %@", error);
                return errSecAuthFailed;
            }
        }
    } else {
        secwarning("SecCMSVerify called without policy -- skipping verification!");
    }

    if (attached_contents) {
        if (![sigd detached]) {
            if (![sigd.contentType isEqualToString:MSCMSContentTypeData]) {
                secwarning("returning attached embedded content of type %@", sigd.contentType);
            }
            *attached_contents = CFBridgingRetain(sigd.dataContent);
        } else {
            *attached_contents = CFBridgingRetain([NSData data]); // Same as what SecCMS.c does
        }
    }

    if (signed_attributes) {
        addSignedAttribues(sigd, signed_attributes);
    }

    return errSecSuccess;
}

OSStatus MS_SecCMSDecodeSignedData(CFDataRef message,
                                CFDataRef *attached_contents, CFDictionaryRef *signed_attributes)
{
    NSError *error = nil;
    MSCMSContentInfo *cinfo = [MSCMSContentInfo decodeMessageSecurityObject:(__bridge NSData *)message options:nil error:&error];
    if (!cinfo) {
        secerror("failed to decode CMS message: %@", error);
        return errSecAuthFailed;
    }
    if (![cinfo.contentType isEqualToString:MSCMSContentTypeSignedData] ||
        !cinfo.embeddedContent ||
        ![cinfo.embeddedContent isKindOfClass:[MSCMSSignedData class]]) {
        secerror("CMS message does not contain a SignedData");
        return errSecDecode;
    }
    MSCMSSignedData *sigd = (MSCMSSignedData *)cinfo.embeddedContent;

    if (attached_contents) {
        if (![sigd detached]) {
            if (![sigd.contentType isEqualToString:MSCMSContentTypeData]) {
                secwarning("returning attached embedded content of type %@", sigd.contentType);
            }
            *attached_contents = CFBridgingRetain(sigd.dataContent);
        } else {
            *attached_contents = CFBridgingRetain([NSData data]); // Same as what SecCMS.c does
        }
    }

    if (signed_attributes) {
        addSignedAttribues(sigd, signed_attributes);
    }

    return errSecSuccess;
}

OSStatus MS_SecCMSDecryptEnvelopedData(CFDataRef message, CFMutableDataRef data,
                                       SecCertificateRef *recipient)
{
    NSError *error = nil;
    MSCMSContentInfo *cinfo = [MSCMSContentInfo decodeMessageSecurityObject:(__bridge NSData *)message options:nil error:&error];
    if (!cinfo) {
        secerror("failed to decode CMS message: %@", error);
        return errSecDecode;
    }
    if (![cinfo.contentType isEqualToString:MSCMSContentTypeEnvelopedData] ||
        !cinfo.embeddedContent ||
        ![cinfo.embeddedContent isKindOfClass:[MSCMSEnvelopedData class]]) {
        secerror("CMS message does not contain a EnvelopedData");
        return errSecDecode;
    }

    MSCMSEnvelopedData *envelopedDataObject = cinfo.embeddedContent;

    MSOID *contentType = [envelopedDataObject contentType];
    if ([contentType isEqualToString:MSCMSContentTypeData]) {
        NSData *decryptedContent = [envelopedDataObject dataContent];
        if (decryptedContent) {
            CFDataAppendBytes(data, [decryptedContent bytes], [decryptedContent length]);
        } else {
            secerror("failed to read the decrypted content after decoding");
            return errSecInternalError;
        }
    } else {
        secerror("unexpected content type %@", contentType);
        return errSecInternalError;
    }

    if (recipient) {
        if ([[envelopedDataObject recipients] count] > 0) {
            MSCMSRecipientInfo *recipientInfo = [[envelopedDataObject recipients] objectAtIndex:0];
            *recipient = CFRetainSafe(recipientInfo.recipientCertificate);
        } else {
            secerror("failed to read the recipient after decoding");
            return errSecInternalError;
        }
    }

    return errSecSuccess;
}

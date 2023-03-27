/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import "remotesigner.h"
#import "cfmunge.h"

#import <MessageSecurity/MessageSecurity.h>
#import <Security/CMSEncoder.h>
#import <Security/SecCmsBase.h>


namespace Security {
namespace CodeSigning {

static NSDictionary *
createHashAgilityV2Dictionary(NSDictionary *hashes)
{
	// Converts the hash dictionary provided by the signing flow to one that uses
	// the appropriate keys from digest algorithm OID strings...so just iterate
	// the input dictionary and map the old keys to new keys.
	NSMutableDictionary *output = [NSMutableDictionary dictionary];
	for (NSNumber *key in hashes) {
		MSOIDString newKey = nil;
		switch (key.intValue) {
			case SEC_OID_SHA1: 		newKey = MSDigestAlgorithmSHA1; 	break;
			case SEC_OID_SHA256: 	newKey = MSDigestAlgorithmSHA256; 	break;
			case SEC_OID_SHA384: 	newKey = MSDigestAlgorithmSHA384; 	break;
			case SEC_OID_SHA512: 	newKey = MSDigestAlgorithmSHA512; 	break;
			default:
				secerror("Unexpected digest algorithm: %@", key);
				return nil;
		}
		output[newKey] = hashes[key];
	}
	return output;
}

static NSData *
createHashAgilityV1Data(CFArrayRef hashList)
{
	CFTemp<CFDictionaryRef> v1HashDict("{cdhashes=%O}", hashList);
	CFRef<CFDataRef> hashAgilityV1Attribute = makeCFData(v1HashDict.get());
	return (__bridge NSData *)hashAgilityV1Attribute.get();
}

OSStatus
doRemoteSigning(const CodeDirectory *cd,
				CFDictionaryRef hashDict,
				CFArrayRef hashList,
				CFAbsoluteTime signingTime,
				CFArrayRef certificateChain,
				SecCodeRemoteSignHandler signHandler,
				CFDataRef *outputCMS)
{
	NSError *error = nil;
	MSCMSSignedData *signedData = nil;
	MSCMSSignerInfo *signerInfo = nil;
	CFRef<SecCertificateRef> firstCert;

	// Verify all inputs are valid.
	if (cd == NULL || cd->length() == 0) {
		secerror("Remote signing requires valid code directory.");
		return errSecParam;
	} else if (outputCMS == NULL) {
		secerror("Remote signing requires output CMS parameter.");
		return errSecParam;
	} else if (hashDict == NULL) {
		secerror("Remote signing requires hash dictionary.");
		return errSecParam;
	} else if (hashList == NULL) {
		secerror("Remote signing requires hash list.");
		return errSecParam;
	} else if (signHandler == NULL) {
		secerror("Remote signing requires signing block.");
		return errSecParam;
	} else if (certificateChain == NULL || CFArrayGetCount(certificateChain) == 0) {
		secerror("Unable to perform remote signing with no certificates: %@", certificateChain);
		return errSecParam;
	}

	// Make a signer info with the identity above and using a SHA256 digest algorithm.
	firstCert = (SecCertificateRef)CFArrayGetValueAtIndex(certificateChain, 0);
	MSOID *signingAlgoOID = [MSOID OIDWithString:MSSignatureAlgorithmRSAPKCS1v5SHA256 error:&error];
	if (!signingAlgoOID) {
		secerror("Unable to create signer info: %@, %@", firstCert.get(), error);
		return errSecMemoryError;
	}
	signerInfo = [[MSCMSSignerInfo alloc] initWithCertificate:firstCert
										   signatureAlgorithm:signingAlgoOID
														error:&error];
	if (!signerInfo || error) {
		secerror("Unable to create signer info: %@, %@, %@", firstCert.get(), signingAlgoOID.OIDString, error);
		return errSecCSCMSConstructionFailed;
	}

	// Initialize the top level signed data with detached data.
	NSData *codeDir = [NSData dataWithBytes:cd length:cd->length()];
	signedData = [[MSCMSSignedData alloc] initWithDataContent:codeDir
												   isDetached:YES
													   signer:signerInfo
									   additionalCertificates:(__bridge NSArray *)certificateChain
														error:&error];
	if (!signedData) {
		secerror("Unable to create signed data: %@", error);
		return errSecCSCMSConstructionFailed;
	}

	// Add signing time into attributes, if necessary.
	if (signingTime != 0) {
		NSDate *signingDate = [NSDate dateWithTimeIntervalSinceReferenceDate:signingTime];
		MSCMSSigningTimeAttribute *signingTimeAttribute = [[MSCMSSigningTimeAttribute alloc] initWithSigningTime:signingDate];
		[signerInfo addProtectedAttribute: signingTimeAttribute];
	}

	// Generate hash agility v1 attribute from the hash list.
	NSData *hashAgilityV1Data = createHashAgilityV1Data(hashList);
	MSCMSAppleHashAgilityAttribute *hashAgility = [[MSCMSAppleHashAgilityAttribute alloc] initWithHashAgilityValue:hashAgilityV1Data];
	[signerInfo addProtectedAttribute: hashAgility];

	// Pass in the hash dictionary to generate the hash agility v2 attribute.
	NSDictionary *hashAgilityV2Dict = createHashAgilityV2Dictionary((__bridge NSDictionary *)hashDict);
	MSCMSAppleHashAgilityV2Attribute *hashAgility2 = [[MSCMSAppleHashAgilityV2Attribute alloc] initWithHashAgilityValues:hashAgilityV2Dict];
	[signerInfo addProtectedAttribute:hashAgility2];

	// Top level object is a content info with pkcs7 signed data type, embedding the signed data above.
	MSCMSContentInfo *topLevelInfo = [[MSCMSContentInfo alloc] initWithEmbeddedContent:signedData];

	// Calculate the signer digest info.
	NSData *signatureDigest = [signerInfo calculateSignerInfoDigest:&error];
	if (!signatureDigest) {
		secerror("Unable to create signature digest: %@, %@", signerInfo, error);
		return errSecCSCMSConstructionFailed;
	}

	// Call remote block with message digest, and transfer the ownership to objc ARC object.
	secinfo("remotesigner", "Passing out external digest: %@", signatureDigest);
	NSData *externalSignature = (__bridge_transfer NSData *)signHandler((__bridge CFDataRef)signatureDigest, kSecCodeSignatureHashSHA256);
	if (!externalSignature) {
		secerror("External block did not provide a signature, failing.");
		return errSecCSRemoteSignerFailed;
	}
	secinfo("remotesigner", "Got external signature blob: %@", externalSignature);

	// Pass the external signature into the signer info so it can be encoded in the output.
	[signerInfo setSignature:externalSignature];

	// Encode the full CMS blob and pass it out.
	NSData *fullCMSSignature = [topLevelInfo encodeMessageSecurityObject:&error];
	if (!fullCMSSignature || error) {
		secerror("Failed to encode signature: %@", error);
		return errSecCSCMSConstructionFailed;
	}

	// Return the signature, bridging with a retain to meet the API that the caller must release it.
	secinfo("remotesigner", "Encoded CMS signature: %@", fullCMSSignature);
	*outputCMS = (__bridge_retained CFDataRef)fullCMSSignature;
	return errSecSuccess;
}

}
}

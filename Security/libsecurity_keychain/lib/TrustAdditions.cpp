/*
 * Copyright (c) 2002-2009,2012 Apple Inc. All Rights Reserved.
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

//
// TrustAdditions.cpp
//
#include "TrustAdditions.h"
#include "TrustKeychains.h"
#include "SecBridge.h"
#include <security_keychain/SecCFTypes.h>
#include <security_keychain/Globals.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Item.h>
#include <security_keychain/KCCursor.h>
#include <security_keychain/KCUtilities.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/unistd.h>
#include <string>
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecBase.h>
#include <Security/Security.h>
#include <Security/cssmtype.h>
#include <Security/cssmapplePriv.h>            // for CSSM_APPLE_TP_OCSP_OPTIONS, CSSM_APPLE_TP_OCSP_OPT_FLAGS

#include "SecTrustPriv.h"
#include "SecTrustSettings.h"
#include "SecTrustSettingsPriv.h"

//
// Macros
//
#define BEGIN_SECAPI_INTERNAL_CALL \
	try {
#define END_SECAPI_INTERNAL_CALL \
	} /* status is only set on error */ \
	catch (const MacOSError &err) { status=err.osStatus(); } \
	catch (const CommonError &err) { status=SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { status=errSecAllocate; } \
	catch (...) { status=errSecInternalComponent; }

#ifdef	NDEBUG
/* this actually compiles to nothing */
#define trustDebug(args...)		secdebug("trust", ## args)
#else
#define trustDebug(args...)		printf(args)
#endif

//
// Static constants
//
static const char *EV_ROOTS_PLIST_SYSTEM_PATH = "/System/Library/Keychains/EVRoots.plist";
static const char *SYSTEM_ROOTS_PLIST_SYSTEM_PATH = "/System/Library/Keychains/SystemRootCertificates.keychain";
static const char *X509ANCHORS_SYSTEM_PATH = "/System/Library/Keychains/X509Anchors";

//
// Static functions
//
static CFArrayRef _allowedRootCertificatesForOidString(CFStringRef oidString);
static CSSM_DATA_PTR _copyFieldDataForOid(CSSM_OID_PTR oid, CSSM_DATA_PTR cert, CSSM_CL_HANDLE clHandle);
static CFStringRef _decimalStringForOid(CSSM_OID_PTR oid);
static CFDictionaryRef _evCAOidDict();
static void _freeFieldData(CSSM_DATA_PTR value, CSSM_OID_PTR oid, CSSM_CL_HANDLE clHandle);
static CFStringRef _oidStringForCertificatePolicies(const CE_CertPolicies *certPolicies);
static SecCertificateRef _rootCertificateWithSubjectOfCertificate(SecCertificateRef certificate);
static SecCertificateRef _rootCertificateWithSubjectKeyIDOfCertificate(SecCertificateRef certificate);

// utility function to safely release (and clear) the given CFTypeRef variable.
//
static void SafeCFRelease(void *cfTypeRefPtr)
{
	CFTypeRef *obj = (CFTypeRef *)cfTypeRefPtr;
	if (obj && *obj) {
		CFRelease(*obj);
		*obj = NULL;
	}
}

// utility function to create a CFDataRef from the contents of the specified file;
// caller must release
//
static CFDataRef dataWithContentsOfFile(const char *fileName)
{
	int rtn;
	int fd;
	struct stat	sb;
	size_t fileSize;
	UInt8 *fileData = NULL;
	CFDataRef outCFData = NULL;

	fd = open(fileName, O_RDONLY, 0);
	if(fd < 0)
		return NULL;

	rtn = fstat(fd, &sb);
	if(rtn)
		goto errOut;

	fileSize = (size_t)sb.st_size;
	fileData = (UInt8 *) malloc(fileSize);
	if(fileData == NULL)
		goto errOut;

	rtn = (int)lseek(fd, 0, SEEK_SET);
	if(rtn < 0)
		goto errOut;

	rtn = (int)read(fd, fileData, fileSize);
	if(rtn != (int)fileSize) {
		rtn = EIO;
	} else {
		rtn = 0;
		outCFData = CFDataCreate(NULL, fileData, fileSize);
	}
errOut:
	close(fd);
	if (fileData) {
		free(fileData);
	}
	return outCFData;
}

// returns a SecKeychainRef for the system root certificate store; caller must release
//
static SecKeychainRef systemRootStore()
{
    SecKeychainStatus keychainStatus = 0;
    SecKeychainRef systemRoots = NULL;
	OSStatus status = errSecSuccess;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecKeychainOpen(SYSTEM_ROOTS_PLIST_SYSTEM_PATH, &systemRoots);
	BEGIN_SECAPI_INTERNAL_CALL
	systemRoots=globals().storageManager.make(SYSTEM_ROOTS_PLIST_SYSTEM_PATH, false)->handle();
	END_SECAPI_INTERNAL_CALL

	// SecKeychainOpen will return errSecSuccess even if the file didn't exist on disk.
	// We need to do a further check using SecKeychainGetStatus().
    if (!status && systemRoots) {
		// note: Sec* APIs are not re-entrant due to the API lock
		// status = SecKeychainGetStatus(systemRoots, &keychainStatus);
		BEGIN_SECAPI_INTERNAL_CALL
		keychainStatus=(SecKeychainStatus)Keychain::optional(systemRoots)->status();
		END_SECAPI_INTERNAL_CALL
	}
    if (status || !systemRoots) {
	    // SystemRootCertificates.keychain can't be opened; look in X509Anchors instead.
        SafeCFRelease(&systemRoots);
		// note: Sec* APIs are not re-entrant due to the API lock
        // status = SecKeychainOpen(X509ANCHORS_SYSTEM_PATH, &systemRoots);
		BEGIN_SECAPI_INTERNAL_CALL
		systemRoots=globals().storageManager.make(X509ANCHORS_SYSTEM_PATH, false)->handle();
		END_SECAPI_INTERNAL_CALL
        // SecKeychainOpen will return errSecSuccess even if the file didn't exist on disk.
		// We need to do a further check using SecKeychainGetStatus().
        if (!status && systemRoots) {
			// note: Sec* APIs are not re-entrant due to the API lock
            // status = SecKeychainGetStatus(systemRoots, &keychainStatus);
			BEGIN_SECAPI_INTERNAL_CALL
			keychainStatus=(SecKeychainStatus)Keychain::optional(systemRoots)->status();
			END_SECAPI_INTERNAL_CALL
		}
    }
    if (status || !systemRoots) {
		// Cannot get root certificates if there is no trusted system root certificate store.
        SafeCFRelease(&systemRoots);
        return NULL;
    }
	return systemRoots;
}

// returns a CFDictionaryRef created from the specified XML plist file; caller must release
//
static CFDictionaryRef dictionaryWithContentsOfPlistFile(const char *fileName)
{
	CFDictionaryRef resultDict = NULL;
	CFDataRef fileData = dataWithContentsOfFile(fileName);
	if (fileData) {
		CFPropertyListRef xmlPlist = CFPropertyListCreateFromXMLData(NULL, fileData, kCFPropertyListImmutable, NULL);
		if (xmlPlist && CFGetTypeID(xmlPlist) == CFDictionaryGetTypeID()) {
			resultDict = (CFDictionaryRef)xmlPlist;
		} else {
			SafeCFRelease(&xmlPlist);
		}
		SafeCFRelease(&fileData);
	}
	return resultDict;
}

// returns the Organization component of the given certificate's subject name,
// or nil if that component could not be found. Caller must release the string.
//
static CFStringRef organizationNameForCertificate(SecCertificateRef certificate)
{
    CFStringRef organizationName = nil;
	OSStatus status = errSecSuccess;

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
    CSSM_OID_PTR oidPtr = (CSSM_OID_PTR) &CSSMOID_OrganizationName;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateCopySubjectComponent(certificate, oidPtr, &organizationName);
	BEGIN_SECAPI_INTERNAL_CALL
	organizationName = Certificate::required(certificate)->distinguishedName(&CSSMOID_X509V1SubjectNameCStruct, oidPtr);
	END_SECAPI_INTERNAL_CALL
    if (status) {
        return (CFStringRef)NULL;
	}
#else
    // SecCertificateCopySubjectComponent() doesn't exist on Tiger, so we have
	// to go get the CSSMOID_OrganizationName the hard way, ourselves.
    CSSM_DATA_PTR *fieldValues = NULL;
	// note: Sec* APIs are not re-entrant due to the API lock
    // status = SecCertificateCopyFieldValues(certificate, &CSSMOID_X509V1SubjectNameCStruct, &fieldValues);
	BEGIN_SECAPI_INTERNAL_CALL
	fieldValues = Certificate::required(certificate)->copyFieldValues(&CSSMOID_X509V1SubjectNameCStruct);
	END_SECAPI_INTERNAL_CALL
    if (*fieldValues == NULL) {
        return (CFStringRef)NULL;
	}
    if (status || (*fieldValues)->Length == 0 || (*fieldValues)->Data == NULL) {
		// note: Sec* APIs are not re-entrant due to the API lock
		// status = SecCertificateReleaseFieldValues(certificate, &CSSMOID_X509V1SubjectNameCStruct, fieldValues);
		BEGIN_SECAPI_INTERNAL_CALL
		Certificate::required(certificate)->releaseFieldValues(&CSSMOID_X509V1SubjectNameCStruct, fieldValues);
		END_SECAPI_INTERNAL_CALL
        return (CFStringRef)NULL;
    }

    CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)(*fieldValues)->Data;

    // Iterate over all the relative distinguished name (RDN) entries...
    unsigned rdnIndex = 0;
    bool foundIt = FALSE;
    for (rdnIndex = 0; rdnIndex < x509Name->numberOfRDNs; rdnIndex++) {
        CSSM_X509_RDN *rdnPtr = x509Name->RelativeDistinguishedName + rdnIndex;

        // And then iterate over the attribute-value pairs of each RDN, looking for a CSSMOID_OrganizationName.
        unsigned pairIndex;
        for (pairIndex = 0; pairIndex < rdnPtr->numberOfPairs; pairIndex++) {
            CSSM_X509_TYPE_VALUE_PAIR *pair = rdnPtr->AttributeTypeAndValue + pairIndex;

            // If this pair isn't the organization name, move on to check the next one.
            if (!oidsAreEqual(&pair->type, &CSSMOID_OrganizationName))
                continue;

            // We've found the organization name. Convert value to a string (eg, "Apple Inc.")
            // Note: there can be more than one organization name in any given CSSM_X509_RDN.
			// In practice, it's OK to use the first one. In future, if we have a means for
			// displaying more than one name, this would be where they should be collected
			// into an array.
            switch (pair->valueType) {
                case BER_TAG_PKIX_UTF8_STRING:
                case BER_TAG_PKIX_UNIVERSAL_STRING:
                case BER_TAG_GENERAL_STRING:
					organizationName = CFStringCreateWithBytes(NULL, pair->value.Data, pair->value.Length, kCFStringEncodingUTF8, FALSE);
                    break;
                case BER_TAG_PRINTABLE_STRING:
                case BER_TAG_IA5_STRING:
					organizationName = CFStringCreateWithBytes(NULL, pair->value.Data, pair->value.Length, kCFStringEncodingASCII, FALSE);
                    break;
                case BER_TAG_T61_STRING:
                case BER_TAG_VIDEOTEX_STRING:
                case BER_TAG_ISO646_STRING:
					organizationName = CFStringCreateWithBytes(NULL, pair->value.Data, pair->value.Length, kCFStringEncodingUTF8, FALSE);
                    // If the data cannot be represented as a UTF-8 string, fall back to ISO Latin 1
                    if (!organizationName) {
						organizationName = CFStringCreateWithBytes(NULL, pair->value.Data, pair->value.Length, kCFStringEncodingISOLatin1, FALSE);
                    }
					break;
                case BER_TAG_PKIX_BMP_STRING:
					organizationName = CFStringCreateWithBytes(NULL, pair->value.Data, pair->value.Length, kCFStringEncodingUnicode, FALSE);
                    break;
                default:
                    break;
            }

            // If we found the organization name, there's no need to keep looping.
            if (organizationName) {
                foundIt = TRUE;
                break;
            }
        }
        if (foundIt)
            break;
    }
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateReleaseFieldValues(certificate, &CSSMOID_X509V1SubjectNameCStruct, fieldValues);
	BEGIN_SECAPI_INTERNAL_CALL
	Certificate::required(certificate)->releaseFieldValues(&CSSMOID_X509V1SubjectNameCStruct, fieldValues);
	END_SECAPI_INTERNAL_CALL
#endif
    return organizationName;
}

#if !defined(NDEBUG)
void showCertSKID(const void *value, void *context);
#endif

static ModuleNexus<Mutex> gPotentialEVChainWithCertificatesMutex;

// returns a CFArrayRef of SecCertificateRef instances; caller must release the returned array
//
CFArrayRef potentialEVChainWithCertificates(CFArrayRef certificates)
{
	StLock<Mutex> _(gPotentialEVChainWithCertificatesMutex());

    // Given a partial certificate chain (which may or may not include the root,
    // and does not have a guaranteed order except the first item is the leaf),
    // examine intermediate certificates to see if they are cross-certified (i.e.
    // have the same subject and public key as a trusted root); if so, remove the
    // intermediate from the returned certificate array.

	CFIndex chainIndex, chainLen = (certificates) ? CFArrayGetCount(certificates) : 0;
	secdebug("trusteval", "potentialEVChainWithCertificates: chainLen: %ld", chainLen);
    if (chainLen < 2) {
		if (certificates) {
			CFRetain(certificates);
		}
        return certificates;
	}

	CFMutableArrayRef certArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (chainIndex = 0; chainIndex < chainLen; chainIndex++) {
        SecCertificateRef aCert = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, chainIndex);
        SecCertificateRef replacementCert = NULL;
		secdebug("trusteval", "potentialEVChainWithCertificates: examining chainIndex: %ld", chainIndex);
        if (chainIndex > 0) {
            // if this is not the leaf, then look for a possible replacement root to end the chain
			// Try lookup using Subject Key ID first
			replacementCert = _rootCertificateWithSubjectKeyIDOfCertificate(aCert);
			if (!replacementCert)
			{
				secdebug("trusteval", "  not found using SKID, try by subject");
            replacementCert = _rootCertificateWithSubjectOfCertificate(aCert);
        }
        }
        if (!replacementCert) {
			secdebug("trusteval", "  No replacement found using SKID or subject; keeping original intermediate");
            CFArrayAppendValue(certArray, aCert);
        }
        SafeCFRelease(&replacementCert);
    }
	secdebug("trusteval", "potentialEVChainWithCertificates: exit: new chainLen: %ld", CFArrayGetCount(certArray));
#if !defined(NDEBUG)
	CFArrayApplyFunction(certArray, CFRangeMake(0, CFArrayGetCount(certArray)), showCertSKID, NULL);
#endif

    return certArray;
}

// returns a reference to a root certificate, if one can be found in the
// system root store whose subject name and public key are identical to
// that of the provided certificate, otherwise returns nil.
//
static SecCertificateRef _rootCertificateWithSubjectOfCertificate(SecCertificateRef certificate)
{
    if (!certificate)
        return NULL;

	StLock<Mutex> _(SecTrustKeychainsGetMutex());

    // get data+length for the provided certificate
    CSSM_CL_HANDLE clHandle = 0;
    CSSM_DATA certData = { 0, NULL };
	OSStatus status = errSecSuccess;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateGetCLHandle(certificate, &clHandle);
	BEGIN_SECAPI_INTERNAL_CALL
	clHandle = Certificate::required(certificate)->clHandle();
	END_SECAPI_INTERNAL_CALL
	if (status)
		return NULL;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateGetData(certificate, &certData);
	BEGIN_SECAPI_INTERNAL_CALL
	certData = Certificate::required(certificate)->data();
	END_SECAPI_INTERNAL_CALL
	if (status)
		return NULL;

	// get system roots keychain reference
    SecKeychainRef systemRoots = systemRootStore();
	if (!systemRoots)
		return NULL;

    // copy (normalized) subject for the provided certificate
    const CSSM_OID_PTR oidPtr = (const CSSM_OID_PTR) &CSSMOID_X509V1SubjectName;
    const CSSM_DATA_PTR subjectDataPtr = _copyFieldDataForOid(oidPtr, &certData, clHandle);
    if (!subjectDataPtr)
        return NULL;

    // copy public key for the provided certificate
    SecKeyRef keyRef = NULL;
    SecCertificateRef resultCert = NULL;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateCopyPublicKey(certificate, &keyRef);
	BEGIN_SECAPI_INTERNAL_CALL
	keyRef = Certificate::required(certificate)->publicKey()->handle();
	END_SECAPI_INTERNAL_CALL
    if (!status) {
        const CSSM_KEY *cssmKey = NULL;
		// note: Sec* APIs are not re-entrant due to the API lock
		// status = SecKeyGetCSSMKey(keyRef, &cssmKey);
		BEGIN_SECAPI_INTERNAL_CALL
		cssmKey = KeyItem::required(keyRef)->key();
		END_SECAPI_INTERNAL_CALL
        if (!status) {
            // get SHA-1 hash of the public key
            uint8 buf[CC_SHA1_DIGEST_LENGTH];
            CSSM_DATA digest = { sizeof(buf), buf };
			if (!cssmKey || !cssmKey->KeyData.Data || !cssmKey->KeyData.Length) {
				status = errSecParam;
			} else {
				CC_SHA1(cssmKey->KeyData.Data, (CC_LONG)cssmKey->KeyData.Length, buf);
			}
            if (!status) {
                // set up attribute vector (each attribute consists of {tag, length, pointer})
                // we want to match on the public key hash and the normalized subject name
                // as well as ensure that the issuer matches the subject
                SecKeychainAttribute attrs[] = {
                    { kSecPublicKeyHashItemAttr, (UInt32)digest.Length, (void *)digest.Data },
                    { kSecSubjectItemAttr, (UInt32)subjectDataPtr->Length, (void *)subjectDataPtr->Data },
                    { kSecIssuerItemAttr, (UInt32)subjectDataPtr->Length, (void *)subjectDataPtr->Data }
                };
                const SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
                SecKeychainSearchRef searchRef = NULL;
				// note: Sec* APIs are not re-entrant due to the API lock
				// status = SecKeychainSearchCreateFromAttributes(systemRoots, kSecCertificateItemClass, &attributes, &searchRef);
				BEGIN_SECAPI_INTERNAL_CALL
				StorageManager::KeychainList keychains;
				globals().storageManager.optionalSearchList(systemRoots, keychains);
				KCCursor cursor(keychains, kSecCertificateItemClass, &attributes);
				searchRef = cursor->handle();
				END_SECAPI_INTERNAL_CALL
                if (!status && searchRef) {
                    SecKeychainItemRef certRef = nil;
					// note: Sec* APIs are not re-entrant due to the API lock
					// status = SecKeychainSearchCopyNext(searchRef, &certRef); // only need the first one that matches
					BEGIN_SECAPI_INTERNAL_CALL
					Item item;
					if (!KCCursorImpl::required(searchRef)->next(item)) {
						status=errSecItemNotFound;
					} else {
						certRef=item->handle();
					}
					END_SECAPI_INTERNAL_CALL
                    if (!status)
                        resultCert = (SecCertificateRef)certRef; // caller must release
                    SafeCFRelease(&searchRef);
                }
            }
        }
    }
    _freeFieldData(subjectDataPtr, oidPtr, clHandle);
    SafeCFRelease(&keyRef);
	SafeCFRelease(&systemRoots);

    return resultCert;
}


#if !defined(NDEBUG)
static void logSKID(const char *msg, const CssmData &subjectKeyID)
{
	const unsigned char *px = (const unsigned char *)subjectKeyID.data();
	char buffer[256]={0,};
	char bytes[16];
	if (px && msg)
	{
		strcpy(buffer, msg);
		for (unsigned int ix=0; ix<20; ix++)
		{
			sprintf(bytes, "%02X", px[ix]);
			strcat(buffer, bytes);
		}
		secdebug("trusteval", " SKID: %s",buffer);
	}
}

void showCertSKID(const void *value, void *context)
{
	SecCertificateRef certificate = (SecCertificateRef)value;
	OSStatus status = errSecSuccess;
	BEGIN_SECAPI_INTERNAL_CALL
	const CssmData &subjectKeyID = Certificate::required(certificate)->subjectKeyIdentifier();
	logSKID("subjectKeyID: ", subjectKeyID);
	END_SECAPI_INTERNAL_CALL
}
#endif

// returns a reference to a root certificate, if one can be found in the
// system root store whose subject key ID are identical to
// that of the provided certificate, otherwise returns nil.
//
static SecCertificateRef _rootCertificateWithSubjectKeyIDOfCertificate(SecCertificateRef certificate)
{
    SecCertificateRef resultCert = NULL;
	OSStatus status = errSecSuccess;

    if (!certificate)
        return NULL;

	StLock<Mutex> _(SecTrustKeychainsGetMutex());

	// get system roots keychain reference
    SecKeychainRef systemRoots = systemRootStore();
	if (!systemRoots)
		return NULL;

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(systemRoots, keychains);

	BEGIN_SECAPI_INTERNAL_CALL
	const CssmData &subjectKeyID = Certificate::required(certificate)->subjectKeyIdentifier();
#if !defined(NDEBUG)
	logSKID("search for SKID: ", subjectKeyID);
#endif
	// caller must release
	resultCert = Certificate::required(certificate)->findBySubjectKeyID(keychains, subjectKeyID)->handle();
#if !defined(NDEBUG)
	logSKID("  found SKID: ", subjectKeyID);
#endif
	END_SECAPI_INTERNAL_CALL

	SafeCFRelease(&systemRoots);

    return resultCert;
}

// returns an array of possible root certificates (SecCertificateRef instances)
// for the given EV OID (a hex string); caller must release the array
//
static
CFArrayRef _possibleRootCertificatesForOidString(CFStringRef oidString)
{
	StLock<Mutex> _(SecTrustKeychainsGetMutex());

    if (!oidString)
        return NULL;
	CFDictionaryRef evOidDict = _evCAOidDict();
	if (!evOidDict)
		return NULL;
	CFArrayRef possibleCertificateHashes = (CFArrayRef) CFDictionaryGetValue(evOidDict, oidString);
    SecKeychainRef systemRoots = systemRootStore();
    if (!possibleCertificateHashes || !systemRoots) {
		SafeCFRelease(&evOidDict);
        return NULL;
	}

	CFMutableArrayRef possibleRootCertificates = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFIndex hashCount = CFArrayGetCount(possibleCertificateHashes);
	secdebug("evTrust", "_possibleRootCertificatesForOidString: %d possible hashes", (int)hashCount);

	OSStatus status = errSecSuccess;
	SecKeychainSearchRef searchRef = NULL;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecKeychainSearchCreateFromAttributes(systemRoots, kSecCertificateItemClass, NULL, &searchRef);
	BEGIN_SECAPI_INTERNAL_CALL
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(systemRoots, keychains);
	KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	searchRef = cursor->handle();
	END_SECAPI_INTERNAL_CALL
	if (searchRef) {
		while (!status) {
			SecKeychainItemRef certRef = NULL;
			// note: Sec* APIs are not re-entrant due to the API lock
			// status = SecKeychainSearchCopyNext(searchRef, &certRef);
			BEGIN_SECAPI_INTERNAL_CALL
			Item item;
			if (!KCCursorImpl::required(searchRef)->next(item)) {
				certRef=NULL;
				status=errSecItemNotFound;
			} else {
				certRef=item->handle();
			}
			END_SECAPI_INTERNAL_CALL
			if (status || !certRef) {
				break;
			}

			CSSM_DATA certData = { 0, NULL };
			// note: Sec* APIs are not re-entrant due to the API lock
			// status = SecCertificateGetData((SecCertificateRef) certRef, &certData);
			BEGIN_SECAPI_INTERNAL_CALL
			certData = Certificate::required((SecCertificateRef)certRef)->data();
			END_SECAPI_INTERNAL_CALL
			if (!status) {
				uint8 buf[CC_SHA1_DIGEST_LENGTH];
				CSSM_DATA digest = { sizeof(buf), buf };
				if (!certData.Data || !certData.Length) {
					status = errSecParam;
				} else {
					CC_SHA1(certData.Data, (CC_LONG)certData.Length, buf);
				}
				if (!status) {
					CFDataRef hashData = CFDataCreateWithBytesNoCopy(NULL, digest.Data, digest.Length, kCFAllocatorNull);
					if (hashData && CFArrayContainsValue(possibleCertificateHashes, CFRangeMake(0, hashCount), hashData)) {
						CFArrayAppendValue(possibleRootCertificates, certRef);
					}
					SafeCFRelease(&hashData);
				}
			}
			SafeCFRelease(&certRef);
		}
    }
	SafeCFRelease(&searchRef);
    SafeCFRelease(&systemRoots);
	SafeCFRelease(&evOidDict);

    return possibleRootCertificates;
}

// returns an array of allowed root certificates (SecCertificateRef instances)
// for the given EV OID (a hex string); caller must release the array.
// This differs from _possibleRootCertificatesForOidString in that each possible
// certificate is further checked for trust settings, so we don't include
// a certificate which is untrusted (or explicitly distrusted).
//
CFArrayRef _allowedRootCertificatesForOidString(CFStringRef oidString)
{
	CFMutableArrayRef allowedRootCertificates = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayRef possibleRootCertificates = _possibleRootCertificatesForOidString(oidString);
	if (possibleRootCertificates) {
		CFIndex idx, count = CFArrayGetCount(possibleRootCertificates);
		for (idx=0; idx<count; idx++) {
			SecCertificateRef cert = (SecCertificateRef) CFArrayGetValueAtIndex(possibleRootCertificates, idx);
			CFStringRef hashStr = SecTrustSettingsCertHashStrFromCert(cert);
			if (hashStr) {
				bool foundMatch = false;
				bool foundAny = false;
				CSSM_RETURN *errors = NULL;
				uint32 errorCount = 0;
				SecTrustSettingsDomain foundDomain = 0;
				SecTrustSettingsResult result = kSecTrustSettingsResultInvalid;
				OSStatus status = SecTrustSettingsEvaluateCert(
					hashStr,		/* certHashStr */
					NULL,			/* policyOID (optional) */
					NULL,			/* policyString (optional) */
					0,				/* policyStringLen */
					0,				/* keyUsage */
					true,			/* isRootCert */
					&foundDomain,	/* foundDomain */
					&errors,		/* allowedErrors */
					&errorCount,	/* numAllowedErrors */
					&result,		/* resultType */
					&foundMatch,	/* foundMatchingEntry */
					&foundAny);		/* foundAnyEntry */

				if (status == errSecSuccess) {
					secdebug("evTrust", "_allowedRootCertificatesForOidString: cert %lu has result %d from domain %d",
						idx, (int)result, (int)foundDomain);
					// Root certificates must be trusted by the system (and not have
					// any explicit trust overrides) to be allowed for EV use.
					if (foundMatch && foundDomain == kSecTrustSettingsDomainSystem &&
						result == kSecTrustSettingsResultTrustRoot) {
						CFArrayAppendValue(allowedRootCertificates, cert);
					}
				} else {
					secdebug("evTrust", "_allowedRootCertificatesForOidString: cert %lu SecTrustSettingsEvaluateCert error %d",
						idx, (int)status);
				}
				if (errors) {
					free(errors);
				}
				CFRelease(hashStr);
			}
		}
		CFRelease(possibleRootCertificates);
	}

	return allowedRootCertificates;
}

// return a CSSM_DATA_PTR containing field data; caller must release with _freeFieldData
//
static CSSM_DATA_PTR _copyFieldDataForOid(CSSM_OID_PTR oid, CSSM_DATA_PTR cert, CSSM_CL_HANDLE clHandle)
{
    uint32 numFields = 0;
    CSSM_HANDLE results = 0;
    CSSM_DATA_PTR value = 0;
    CSSM_RETURN crtn = CSSM_CL_CertGetFirstFieldValue(clHandle, cert, oid, &results, &numFields, &value);

	// we aren't going to look for any further fields, so free the results handle immediately
	if (results) {
		CSSM_CL_CertAbortQuery(clHandle, results);
	}

    return (crtn || !numFields) ? NULL : value;
}

// Some errors are ignorable errors because they do not indicate a problem
// with the certificate itself, but rather a problem getting a response from
// the CA server. The EV Certificate spec does not mandate that the application
// software vendor *must* get a response from OCSP or CRL, it is a "best
// attempt" approach which will not fail if the server does not respond.
//
// The EV spec (26. EV Certificate Status Checking) says that CAs have to
// maintain either a CRL or OCSP server. They are not required to maintain
// an OCSP server until after Dec 31, 2010.
//
// As to the responsibility of the application software vendor to perform
// revocation checking, this is only covered by the following section (37.2.):
//
// This [indemnification of Application Software Vendors]
// shall not apply, however, to any claim, damages, or loss
// suffered by such Application Software Vendor related to an EV Certificate
// issued by the CA where such claim, damage, or loss was directly caused by
// such Application Software Vendor’s software displaying as not trustworthy an
// EV Certificate that is still valid, or displaying as trustworthy: (1) an EV
// Certificate that has expired, or (2) an EV Certificate that has been revoked
// (but only in cases where the revocation status is currently available from the
// CA online, and the browser software either failed to check such status or
// ignored an indication of revoked status).
//
// The last section describes what a browser is required to do: it must attempt
// to check revocation status (as indicated by the OCSP or CRL server info in
// the certificate), and it cannot ignore an indication of revoked status
// (i.e. a positive thumbs-down response from the server, which would be a
// different error than the ones being skipped.) However, given that we meet
// those requirements, if the revocation server is down or will not give us a
// response for whatever reason, that is not our problem.

bool isRevocationServerMetaError(CSSM_RETURN statusCode)
{
   switch (statusCode) {
       case CSSMERR_APPLETP_CRL_NOT_FOUND:             // 13. CRL not found
       case CSSMERR_APPLETP_CRL_SERVER_DOWN:           // 14. CRL server down
       case CSSMERR_APPLETP_OCSP_UNAVAILABLE:          // 33. OCSP service unavailable
       case CSSMERR_APPLETP_NETWORK_FAILURE:           // 36. General network failure
       case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:   // 41. OCSP responder status: malformed request
       case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:    // 42. OCSP responder status: internal error
       case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:       // 43. OCSP responder status: try later
       case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:    // 44. OCSP responder status: signature required
       case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:    // 45. OCSP responder status: unauthorized
           return true;
       default:
           return false;
   }
}

// returns true if the given status code is related to performing an OCSP revocation check
//
bool isOCSPStatusCode(CSSM_RETURN statusCode)
{
    switch (statusCode)
    {
        case CSSMERR_APPLETP_OCSP_BAD_RESPONSE:         // 31. Unparseable OCSP response
        case CSSMERR_APPLETP_OCSP_BAD_REQUEST:          // 32. Unparseable OCSP request
        case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:   // 41. OCSP responder status: malformed request
        case CSSMERR_APPLETP_OCSP_UNAVAILABLE:          // 33. OCSP service unavailable
        case CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED:  // 34. OCSP status: cert unrecognized
        case CSSMERR_APPLETP_OCSP_NOT_TRUSTED:          // 37. OCSP response not verifiable to anchor or root
        case CSSMERR_APPLETP_OCSP_INVALID_ANCHOR_CERT:  // 38. OCSP response verified to untrusted root
        case CSSMERR_APPLETP_OCSP_SIG_ERROR:            // 39. OCSP response signature error
        case CSSMERR_APPLETP_OCSP_NO_SIGNER:            // 40. No signer for OCSP response found
        case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:    // 42. OCSP responder status: internal error
        case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:       // 43. OCSP responder status: try later
        case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:    // 44. OCSP responder status: signature required
        case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:    // 45. OCSP responder status: unauthorized
        case CSSMERR_APPLETP_OCSP_NONCE_MISMATCH:       // 46. OCSP response nonce did not match request
            return true;
        default:
            return false;
    }
}

// returns true if the given status code is related to performing a CRL revocation check
//
bool isCRLStatusCode(CSSM_RETURN statusCode)
{
    switch (statusCode)
    {
        case CSSMERR_APPLETP_CRL_EXPIRED:               // 11. CRL expired
        case CSSMERR_APPLETP_CRL_NOT_VALID_YET:         // 12. CRL not yet valid
        case CSSMERR_APPLETP_CRL_NOT_FOUND:             // 13. CRL not found
        case CSSMERR_APPLETP_CRL_SERVER_DOWN:           // 14. CRL server down
        case CSSMERR_APPLETP_CRL_BAD_URI:               // 15. Illegal CRL distribution point URI
        case CSSMERR_APPLETP_CRL_NOT_TRUSTED:           // 18. CRL not verifiable to anchor or root
        case CSSMERR_APPLETP_CRL_INVALID_ANCHOR_CERT:   // 19. CRL verified to untrusted root
        case CSSMERR_APPLETP_CRL_POLICY_FAIL:           // 20. CRL failed policy verification
            return true;
        default:
            return false;
    }
}

// returns true if the given status code is related to performing a revocation check
//
bool isRevocationStatusCode(CSSM_RETURN statusCode)
{
    if (statusCode == CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK ||  // 35. Revocation check not successful for each cert
        statusCode == CSSMERR_APPLETP_NETWORK_FAILURE ||              // 36. General network error
        isOCSPStatusCode(statusCode) == true ||                       // OCSP error
        isCRLStatusCode(statusCode) == true)                          // CRL error
        return true;
    else
        return false;
}

// returns true if the given revocation status code can be ignored.
//
bool ignorableRevocationStatusCode(CSSM_RETURN statusCode)
{
    if (!isRevocationStatusCode(statusCode))
		return false;

	// if OCSP and/or CRL revocation info was unavailable for this certificate,
	// and revocation checking is not required, we can ignore this status code.

	CFStringRef ocsp_val = (CFStringRef) CFPreferencesCopyValue(kSecRevocationOcspStyle, CFSTR(kSecRevocationDomain), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFStringRef crl_val = (CFStringRef) CFPreferencesCopyValue(kSecRevocationCrlStyle, CFSTR(kSecRevocationDomain), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	bool ocspRequired = (ocsp_val && CFEqual(ocsp_val, kSecRevocationRequireForAll));
	bool crlRequired = (crl_val && CFEqual(crl_val, kSecRevocationRequireForAll));
	if (!ocspRequired && ocsp_val && CFEqual(ocsp_val, kSecRevocationRequireIfPresent))
		ocspRequired = (statusCode != CSSMERR_APPLETP_OCSP_UNAVAILABLE);
	if (!crlRequired && crl_val && CFEqual(crl_val, kSecRevocationRequireIfPresent))
		crlRequired = (statusCode != CSSMERR_APPLETP_CRL_NOT_FOUND);
	if (ocsp_val)
		CFRelease(ocsp_val);
	if (crl_val)
		CFRelease(crl_val);

	if (isOCSPStatusCode(statusCode))
		return (ocspRequired) ? false : true;
	if (isCRLStatusCode(statusCode))
		return (crlRequired) ? false : true;

	return false;
}

// returns a CFArrayRef of allowed root certificates for the provided leaf certificate
// if it passes initial EV evaluation criteria and should be subject to OCSP revocation
// checking; otherwise, NULL is returned. (Caller must release the result if not NULL.)
//
CFArrayRef allowedEVRootsForLeafCertificate(CFArrayRef certificates)
{
    // Given a partial certificate chain (which may or may not include the root,
    // and does not have a guaranteed order except the first item is the leaf),
	// determine whether the leaf claims to have a supported EV policy OID.
	//
	// Unless this function returns NULL, a full SSL trust evaluation with OCSP revocation
	// checking must be performed successfully for the certificate to be considered valid.
	// This function is intended to be called before the chain has been evaluated,
	// in order to obtain the list of allowed roots for the evaluation. Once the "regular"
	// TP evaluation has taken place, chainMeetsExtendedValidationCriteria() should be
	// called to complete extended validation checking.

	CFIndex count = (certificates) ? CFArrayGetCount(certificates) : 0;
	if (count < 1)
        return NULL;

    CSSM_CL_HANDLE clHandle = 0;
    CSSM_DATA certData = { 0, NULL };
    SecCertificateRef certRef = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, 0);
	OSStatus status = errSecSuccess;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateGetCLHandle(certRef, &clHandle);
	BEGIN_SECAPI_INTERNAL_CALL
	clHandle = Certificate::required(certRef)->clHandle();
	END_SECAPI_INTERNAL_CALL
	if (status)
		return NULL;
	// note: Sec* APIs are not re-entrant due to the API lock
	// status = SecCertificateGetData(certRef, &certData);
	BEGIN_SECAPI_INTERNAL_CALL
	certData = Certificate::required(certRef)->data();
	END_SECAPI_INTERNAL_CALL
	if (status)
		return NULL;

    // Does the leaf certificate contain a Certificate Policies extension?
    const CSSM_OID_PTR oidPtr = (CSSM_OID_PTR) &CSSMOID_CertificatePolicies;
    CSSM_DATA_PTR extensionDataPtr = _copyFieldDataForOid(oidPtr, &certData, clHandle);
    if (!extensionDataPtr)
        return NULL;

    // Does the extension contain one of the magic EV CA OIDs we know about?
    CSSM_X509_EXTENSION *cssmExtension = (CSSM_X509_EXTENSION *)extensionDataPtr->Data;
    CE_CertPolicies *certPolicies = (CE_CertPolicies *)cssmExtension->value.parsedValue;
    CFStringRef oidString = _oidStringForCertificatePolicies(certPolicies);
	_freeFieldData(extensionDataPtr, oidPtr, clHandle);

    // Fetch the allowed root CA certificates for this OID, if any
    CFArrayRef allowedRoots = (oidString) ? _allowedRootCertificatesForOidString(oidString) : NULL;
	CFIndex rootCount = (allowedRoots) ? CFArrayGetCount(allowedRoots) : 0;
	secdebug("evTrust", "allowedEVRootsForLeafCertificate: found %d allowed roots", (int)rootCount);
	SafeCFRelease(&oidString);
	if (!allowedRoots || !rootCount) {
		SafeCFRelease(&allowedRoots);
		return NULL;
	}

	// The leaf certificate needs extended validation (with revocation checking).
	// Return the array of allowed roots for this leaf certificate.
	return allowedRoots;
}

// returns true if the provided certificate contains a wildcard in either
// its common name or subject alternative name.
//
static
bool hasWildcardDNSName(SecCertificateRef certRef)
{
	OSStatus status = errSecSuccess;
	CFArrayRef dnsNames = NULL;

	BEGIN_SECAPI_INTERNAL_CALL
	Required(&dnsNames) = Certificate::required(certRef)->copyDNSNames();
	END_SECAPI_INTERNAL_CALL
	if (status || !dnsNames)
		return false;

	bool hasWildcard = false;
	const CFStringRef wildcard = CFSTR("*");
	CFIndex index, count = CFArrayGetCount(dnsNames);
	for (index = 0; index < count; index ++) {
		CFStringRef name = (CFStringRef) CFArrayGetValueAtIndex(dnsNames, index);
		if (name) {
			CFRange foundRange = CFStringFind(name, wildcard, 0);
			if (foundRange.length != 0 && foundRange.location != kCFNotFound) {
				hasWildcard = true;
				break;
			}
		}
	}
	CFRelease(dnsNames);
	return hasWildcard;
}

// returns a CFDictionaryRef of extended validation results for the given chain,
// or NULL if the certificate chain did not meet all EV criteria. (Caller must
// release the result if not NULL.)
//
static
CFDictionaryRef extendedValidationResults(CFArrayRef certChain, SecTrustResultType trustResult, OSStatus tpResult)
{
	// This function is intended to be called after the "regular" TP evaluation
	// has taken place (i.e. trustResult and tpResult are available), and there
	// is a full certificate chain to examine.

    CFIndex chainIndex, chainLen = (certChain) ? CFArrayGetCount(certChain) : 0;
	if (chainLen < 2) {
		return NULL; // invalid chain length
	}

    if (trustResult != kSecTrustResultUnspecified) {

        // "Recoverable" means the certificate failed to meet all policy requirements, but is intrinsically OK.
        // One of the failures we might encounter is if the OCSP responder tells us to go away. Since this is a
        // real-world case, we'll check for OCSP and CRL meta-errors specifically.
        bool recovered = false;
        if (trustResult == kSecTrustResultRecoverableTrustFailure) {
            recovered = isRevocationServerMetaError((CSSM_RETURN)tpResult);
        }
        if (!recovered) {
            return NULL;
        }
    }

	//
    // What we know at this point:
	//
	// 1. From a previous call to allowedEVRootsForLeafCertificate
	// (or we wouldn't be getting called by extendedTrustResults):
    // - a leaf certificate exists
    // - that certificate contains a Certificate Policies extension
    // - that extension contains an OID from one of the trusted EV CAs we know about
	// - we have found at least one allowed EV root for that OID
	//
	// 2. From the TP evaluation:
    // - the leaf certificate verifies back to a trusted EV root (with no trust settings overrides)
    // - SSL trust evaluation with OCSP revocation checking enabled returned no (fatal) errors
    //
    // We need to verify the following additional requirements for the leaf (as of EV 1.1, 6(a)(2)):
    // - cannot specify a wildcard in commonName or subjectAltName
    // (note: this is a change since EV 1.0 (9.2.1), which stated that "Wildcard FQDNs are permitted.")
	//
	// Finally, we need to check the following requirements (EV 1.1 specification, Appendix B):
    // - the trusted root, if created after 10/31/2006, must have:
    //      - critical basicConstraints extension with CA bit set
    //      - critical keyUsage extension with keyCertSign and cRLSign bits set
    // - intermediate certs, if present, must have:
    //      - certificatePolicies extension, containing either a known EV CA OID, or anyPolicy
    //      - non-critical cRLDistributionPoint extension
    //      - critical basicConstraints extension with CA bit set
    //      - critical keyUsage extension with keyCertSign and cRLSign bits set
    //

	// check leaf certificate for wildcard names
	if (hasWildcardDNSName((SecCertificateRef) CFArrayGetValueAtIndex(certChain, 0))) {
		trustDebug("has wildcard name (does not meet EV criteria)");
		return NULL;
	}

    // check intermediate CA certificates for required extensions per Appendix B of EV 1.1 specification.
    bool hasRequiredExtensions = true;
	CSSM_CL_HANDLE clHandle = 0;
	CSSM_DATA certData = { 0, NULL };
	CSSM_OID_PTR oidPtr = (CSSM_OID_PTR) &CSSMOID_CertificatePolicies;
    for (chainIndex = 1; hasRequiredExtensions && chainLen > 2 && chainIndex < chainLen - 1; chainIndex++) {
        SecCertificateRef intermediateCert = (SecCertificateRef) CFArrayGetValueAtIndex(certChain, chainIndex);
		OSStatus status = errSecSuccess;
		// note: Sec* APIs are not re-entrant due to the API lock
		// status = SecCertificateGetCLHandle(intermediateCert, &clHandle);
		BEGIN_SECAPI_INTERNAL_CALL
		clHandle = Certificate::required(intermediateCert)->clHandle();
		END_SECAPI_INTERNAL_CALL
		if (status)
			return NULL;
		// note: Sec* APIs are not re-entrant due to the API lock
		// status = SecCertificateGetData(intermediateCert, &certData);
		BEGIN_SECAPI_INTERNAL_CALL
		certData = Certificate::required(intermediateCert)->data();
		END_SECAPI_INTERNAL_CALL
		if (status)
			return NULL;

        CSSM_DATA_PTR extensionDataPtr = _copyFieldDataForOid(oidPtr, &certData, clHandle);
        if (!extensionDataPtr)
            return NULL;

        CSSM_X509_EXTENSION *cssmExtension = (CSSM_X509_EXTENSION *)extensionDataPtr->Data;
        CE_CertPolicies *certPolicies = (CE_CertPolicies *)cssmExtension->value.parsedValue;
		CFStringRef oidString = _oidStringForCertificatePolicies(certPolicies);
        hasRequiredExtensions = (oidString != NULL);
		SafeCFRelease(&oidString);
        _freeFieldData(extensionDataPtr, oidPtr, clHandle);

        // FIX: add checks for the following (not essential to this implementation):
        //      - non-critical cRLDistributionPoint extension
        //      - critical basicConstraints extension with CA bit set
        //      - critical keyUsage extension with keyCertSign and cRLSign bits set
        // Tracked by <rdar://problem/6119322>
    }

    if (hasRequiredExtensions) {
		SecCertificateRef leafCert = (SecCertificateRef) CFArrayGetValueAtIndex(certChain, 0);
		CFStringRef organizationName = organizationNameForCertificate(leafCert);
		if (organizationName != NULL) {
			CFMutableDictionaryRef resultDict = CFDictionaryCreateMutable(NULL, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionaryAddValue(resultDict, kSecEVOrganizationName, organizationName);
			trustDebug("[EV] extended validation succeeded");
			SafeCFRelease(&organizationName);
			return resultDict;
		}
	}

	return NULL;
}

// returns a CFDictionaryRef containing extended trust results.
// Caller must release this dictionary.
//
// If the isEVCandidate argument is true, extended validation checking is performed
// and the kSecEVOrganizationName key will be set in the dictionary if EV criteria is met.
// In all cases, kSecTrustEvaluationDate and kSecTrustExpirationDate will be set.
//
CFDictionaryRef extendedTrustResults(CFArrayRef certChain, SecTrustResultType trustResult, OSStatus tpResult, bool isEVCandidate)
{
	CFMutableDictionaryRef resultDict = NULL;
	if (isEVCandidate) {
		resultDict = (CFMutableDictionaryRef) extendedValidationResults(certChain, trustResult, tpResult);
	}
	if (!resultDict) {
		resultDict = CFDictionaryCreateMutable(NULL, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!resultDict) {
			return NULL;
		}
	}
	CFAbsoluteTime at = CFAbsoluteTimeGetCurrent();
	CFDateRef trustEvaluationDate = CFDateCreate(kCFAllocatorDefault, at);
	// by default, permit caching of trust evaluation results for up to 2 hours
	// FIXME: need to modify this based on cert expiration and OCSP/CRL validity
	CFDateRef trustExpirationDate = CFDateCreate(kCFAllocatorDefault, at + (60*60*2));
	CFDictionaryAddValue(resultDict, kSecTrustEvaluationDate, trustEvaluationDate);
	SafeCFRelease(&trustEvaluationDate);
	CFDictionaryAddValue(resultDict, kSecTrustExpirationDate, trustExpirationDate);
	SafeCFRelease(&trustExpirationDate);

	return resultDict;
}

// returns a CFDictionaryRef containing mappings from supported EV CA OIDs to SHA-1 hash values;
// caller must release
//
static CFDictionaryRef _evCAOidDict()
{
    static CFDictionaryRef s_evCAOidDict = NULL;
    if (s_evCAOidDict) {
		CFRetain(s_evCAOidDict);
		secdebug("evTrust", "_evCAOidDict: returning static instance (rc=%d)", (int)CFGetRetainCount(s_evCAOidDict));
        return s_evCAOidDict;
	}
	secdebug("evTrust", "_evCAOidDict: initializing static instance");

	s_evCAOidDict = dictionaryWithContentsOfPlistFile(EV_ROOTS_PLIST_SYSTEM_PATH);
	if (!s_evCAOidDict)
		return NULL;

#if !defined MAC_OS_X_VERSION_10_6 || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
	// Work around rdar://6302788 by hard coding a hash that was missed when addressing <rdar://problem/6238289&6238296>
	// This is being addressed in SnowLeopard by rdar://6305989
	CFStringRef oidString = CFSTR("2.16.840.1.114028.10.1.2");
	CFMutableArrayRef hashes = (CFMutableArrayRef) CFDictionaryGetValue(s_evCAOidDict, oidString);
	if (hashes) {
		uint8 hashBytes[] = {0xB3, 0x1E, 0xB1, 0xB7, 0x40, 0xE3, 0x6C, 0x84, 0x02, 0xDA, 0xDC, 0x37, 0xD4, 0x4D, 0xF5, 0xD4, 0x67, 0x49, 0x52, 0xF9};
		CFDataRef hashData = CFDataCreate(NULL, hashBytes, sizeof(hashBytes));
		CFIndex hashCount = CFArrayGetCount(hashes);
		if (hashData && CFArrayContainsValue(hashes, CFRangeMake(0, hashCount), hashData)) {
			secdebug("evTrust", "_evCAOidDict: added hardcoded hash value");
			CFArrayAppendValue(hashes, hashData);
		}
		SafeCFRelease(&hashData);
	}
#endif
	CFRetain(s_evCAOidDict);
	secdebug("evTrust", "_evCAOidDict: returning static instance (rc=%d)", (int)CFGetRetainCount(s_evCAOidDict));
    return s_evCAOidDict;
}

// returns a CFStringRef containing a decimal representation of the given OID.
// Caller must release.

static CFStringRef _decimalStringForOid(CSSM_OID_PTR oid)
{
    CFMutableStringRef str = CFStringCreateMutable(NULL, 0);
    if (!str || oid->Length > 32)
        return str;

    // The first two levels are encoded into one byte, since the root level
    // has only 3 nodes (40*x + y).  However if x = joint-iso-itu-t(2) then
    // y may be > 39, so we have to add special-case handling for this.
    unsigned long value = 0;
    unsigned int x = oid->Data[0] / 40;
    unsigned int y = oid->Data[0] % 40;
    if (x > 2) {
        // Handle special case for large y if x = 2
        y += (x - 2) * 40;
        x = 2;
    }

	CFStringAppendFormat(str, NULL, CFSTR("%d.%d"), x, y);

    for (x = 1; x < oid->Length; x++) {
        value = (value << 7) | (oid->Data[x] & 0x7F);
        if(!(oid->Data[x] & 0x80)) {
			CFStringAppendFormat(str, NULL, CFSTR(".%ld"), value);
            value = 0;
        }
    }

#if !defined(NDEBUG)
	CFIndex nameLen = CFStringGetLength(str);
	CFIndex bufLen = 1 + CFStringGetMaximumSizeForEncoding(nameLen, kCFStringEncodingUTF8);
	char *nameBuf = (char *)malloc(bufLen);
	if (!CFStringGetCString(str, nameBuf, bufLen-1, kCFStringEncodingUTF8))
		nameBuf[0]=0;
	secdebug("evTrust", "_decimalStringForOid: \"%s\"", nameBuf);
	free(nameBuf);
#endif

    return str;
}

static void _freeFieldData(CSSM_DATA_PTR value, CSSM_OID_PTR oid, CSSM_CL_HANDLE clHandle)
{
	if (value && value->Data) {
		CSSM_CL_FreeFieldValue(clHandle, oid, value);
	}
    return;
}

static ModuleNexus<Mutex> gOidStringForCertificatePoliciesMutex;

static CFStringRef _oidStringForCertificatePolicies(const CE_CertPolicies *certPolicies)
{
	StLock<Mutex> _(gOidStringForCertificatePoliciesMutex());

    // returns the first EV OID (as a string) found in the given Certificate Policies extension,
    // or NULL if the extension does not contain any known EV OIDs. (Note that the "any policy" OID
    // is a special case and will be returned if present, although its presence is only meaningful
    // in an intermediate CA.)

    if (!certPolicies) {
		secdebug("evTrust", "oidStringForCertificatePolicies: missing certPolicies!");
        return NULL;
	}

	CFDictionaryRef evOidDict = _evCAOidDict();
	if (!evOidDict) {
		secdebug("evTrust", "oidStringForCertificatePolicies: nil OID dictionary!");
		return NULL;
	}

	CFStringRef foundOidStr = NULL;
    uint32 policyIndex, maxIndex = 10; // sanity check; EV certs normally have EV OID as first policy
    for (policyIndex = 0; policyIndex < certPolicies->numPolicies && policyIndex < maxIndex; policyIndex++) {
        CE_PolicyInformation *certPolicyInfo = &certPolicies->policies[policyIndex];
        CSSM_OID_PTR oid = &certPolicyInfo->certPolicyId;
        CFStringRef oidStr = _decimalStringForOid(oid);
		if (!oidStr)
			continue;
		if (!CFStringCompare(oidStr, CFSTR("2.5.29.32.0"), 0) ||	// is it the "any" OID, or
			CFDictionaryGetValue(evOidDict, oidStr) != NULL) {		// a known EV CA OID?
			foundOidStr = CFStringCreateCopy(NULL, oidStr);
		}
		SafeCFRelease(&oidStr);
		if (foundOidStr)
			break;
    }
	SafeCFRelease(&evOidDict);

    return foundOidStr;
}


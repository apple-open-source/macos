/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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

#include "SecBridge.h"
#include "SecInternal.h"
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>
#include <Security/SecBase.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecCertificate.h>
#include <sys/param.h>
#include "cssmdatetime.h"
#include "SecItem.h"
#include "SecItemPriv.h"
#include "SecIdentitySearchPriv.h"
#include "SecKeychainPriv.h"
#include "SecCertificatePriv.h"
#include "SecCertificatePrivP.h"
#include "TrustAdditions.h"
#include "TrustSettingsSchema.h"
#include <Security/SecTrustPriv.h>
#include "utilities/array_size.h"

#include <AssertMacros.h>
#include <syslog.h>
#include <dlfcn.h>

#include <Security/SecTrustedApplication.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <Security/SecCode.h>
#include <Security/SecCodePriv.h>
#include <Security/SecRequirement.h>

#include <login/SessionAgentCom.h>
#include <login/SessionAgentStatusCom.h>


const uint8_t kUUIDStringLength = 36;

OSStatus SecItemAdd_osx(CFDictionaryRef attributes, CFTypeRef *result);
OSStatus SecItemCopyMatching_osx(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecItemUpdate_osx(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete_osx(CFDictionaryRef query);

extern "C" {
OSStatus SecItemAdd_ios(CFDictionaryRef attributes, CFTypeRef *result);
OSStatus SecItemCopyMatching_ios(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecItemUpdate_ios(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete_ios(CFDictionaryRef query);
OSStatus SecItemUpdateTokenItems_ios(CFTypeRef tokenID, CFArrayRef tokenItemsAttributes);


OSStatus SecItemValidateAppleApplicationGroupAccess(CFStringRef group);
CFDictionaryRef SecItemCopyTranslatedAttributes(CFDictionaryRef inOSXDict, CFTypeRef itemClass,
	bool iOSOut, bool pruneMatch, bool pruneSync, bool pruneReturn, bool pruneData, bool pruneAccess);
}

static Boolean SecItemSynchronizable(CFDictionaryRef query);

static void secitemlog(int priority, const char *format, ...)
{
#ifndef NDEBUG
	// log everything
#else
	if (priority < LOG_NOTICE) // log warnings and errors
#endif
	{
		va_list list;
		va_start(list, format);
		vsyslog(priority, format, list);
		va_end(list);
	}
}

static void secitemshow(CFTypeRef obj, const char *context)
{
#ifndef NDEBUG
	CFStringRef desc = CFCopyDescription(obj);
	if (!desc) return;

	CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(desc), kCFStringEncodingUTF8) + 1;
	char* buffer = (char*) malloc(length);
	if (buffer) {
		Boolean converted = CFStringGetCString(desc, buffer, length, kCFStringEncodingUTF8);
		if (converted) {
			const char *prefix = (context) ? context : "";
			const char *separator = (context) ? " " : "";
			secitemlog(LOG_NOTICE, "%s%s%s", prefix, separator, buffer);
		}
		free(buffer);
	}
	CFRelease(desc);
#endif
}


#define CFDataGetBytePtrVoid CFDataGetBytePtr

#pragma mark SecItem private utility functions

/******************************************************************************/

struct ProtocolAttributeInfo {
	const CFStringRef *protocolValue;
	SecProtocolType protocolType;
};

static ProtocolAttributeInfo gProtocolTypes[] = {
	{ &kSecAttrProtocolFTP, kSecProtocolTypeFTP },
	{ &kSecAttrProtocolFTPAccount, kSecProtocolTypeFTPAccount },
	{ &kSecAttrProtocolHTTP, kSecProtocolTypeHTTP },
	{ &kSecAttrProtocolIRC, kSecProtocolTypeIRC },
	{ &kSecAttrProtocolNNTP, kSecProtocolTypeNNTP },
	{ &kSecAttrProtocolPOP3, kSecProtocolTypePOP3 },
	{ &kSecAttrProtocolSMTP, kSecProtocolTypeSMTP },
	{ &kSecAttrProtocolSOCKS, kSecProtocolTypeSOCKS },
	{ &kSecAttrProtocolIMAP, kSecProtocolTypeIMAP },
	{ &kSecAttrProtocolLDAP, kSecProtocolTypeLDAP },
	{ &kSecAttrProtocolAppleTalk, kSecProtocolTypeAppleTalk },
	{ &kSecAttrProtocolAFP, kSecProtocolTypeAFP },
	{ &kSecAttrProtocolTelnet, kSecProtocolTypeTelnet },
	{ &kSecAttrProtocolSSH, kSecProtocolTypeSSH },
	{ &kSecAttrProtocolFTPS, kSecProtocolTypeFTPS },
	{ &kSecAttrProtocolHTTPS, kSecProtocolTypeHTTPS },
	{ &kSecAttrProtocolHTTPProxy, kSecProtocolTypeHTTPProxy },
	{ &kSecAttrProtocolHTTPSProxy, kSecProtocolTypeHTTPSProxy },
	{ &kSecAttrProtocolFTPProxy, kSecProtocolTypeFTPProxy },
	{ &kSecAttrProtocolSMB, kSecProtocolTypeSMB },
	{ &kSecAttrProtocolRTSP, kSecProtocolTypeRTSP },
	{ &kSecAttrProtocolRTSPProxy, kSecProtocolTypeRTSPProxy },
	{ &kSecAttrProtocolDAAP, kSecProtocolTypeDAAP },
	{ &kSecAttrProtocolEPPC, kSecProtocolTypeEPPC },
	{ &kSecAttrProtocolIPP, kSecProtocolTypeIPP },
	{ &kSecAttrProtocolNNTPS, kSecProtocolTypeNNTPS },
	{ &kSecAttrProtocolLDAPS, kSecProtocolTypeLDAPS },
	{ &kSecAttrProtocolTelnetS, kSecProtocolTypeTelnetS },
	{ &kSecAttrProtocolIMAPS, kSecProtocolTypeIMAPS },
	{ &kSecAttrProtocolIRCS, kSecProtocolTypeIRCS },
	{ &kSecAttrProtocolPOP3S, kSecProtocolTypePOP3S }
};

static const int kNumberOfProtocolTypes = sizeof(gProtocolTypes) / sizeof(ProtocolAttributeInfo);

/*
 * _SecProtocolTypeForSecAttrProtocol converts a SecAttrProtocol to a SecProtocolType.
 */
static SecProtocolType
_SecProtocolTypeForSecAttrProtocol(
	CFTypeRef protocol)
{
	SecProtocolType result = kSecProtocolTypeAny;

	if (protocol != NULL) {
		CFIndex count;
		for (count=0; count<kNumberOfProtocolTypes; count++) {
			if (CFEqual(protocol, *(gProtocolTypes[count].protocolValue))) {
				result = gProtocolTypes[count].protocolType;
				break;
			}
		}
	}

	return result;
}

/*
 * _SecAttrProtocolForSecProtocolType converts a SecProtocolType to a SecAttrProtocol.
 */
static CFTypeRef
_SecAttrProtocolForSecProtocolType(
	SecProtocolType protocolType)
{
	CFTypeRef result = NULL;
	CFIndex count;
	for (count=0; count<kNumberOfProtocolTypes; count++) {
		if (gProtocolTypes[count].protocolType == protocolType) {
			result = *(gProtocolTypes[count].protocolValue);
			break;
		}
	}

	return result;
}


/******************************************************************************/

struct AuthenticationAttributeInfo {
	const CFStringRef *authValue;
	SecAuthenticationType authType;
};

static AuthenticationAttributeInfo gAuthTypes[] = {
	{ &kSecAttrAuthenticationTypeNTLM, kSecAuthenticationTypeNTLM },
	{ &kSecAttrAuthenticationTypeMSN, kSecAuthenticationTypeMSN },
	{ &kSecAttrAuthenticationTypeDPA, kSecAuthenticationTypeDPA },
	{ &kSecAttrAuthenticationTypeRPA, kSecAuthenticationTypeRPA },
	{ &kSecAttrAuthenticationTypeHTTPBasic, kSecAuthenticationTypeHTTPBasic },
	{ &kSecAttrAuthenticationTypeHTTPDigest, kSecAuthenticationTypeHTTPDigest },
	{ &kSecAttrAuthenticationTypeHTMLForm, kSecAuthenticationTypeHTMLForm },
	{ &kSecAttrAuthenticationTypeDefault, kSecAuthenticationTypeDefault }
};

static const int kNumberOfAuthenticationTypes = sizeof(gAuthTypes) / sizeof(AuthenticationAttributeInfo);

/*
 * _SecAuthenticationTypeForSecAttrAuthenticationType converts a
 * SecAttrAuthenticationType to a SecAuthenticationType.
 */
static SecAuthenticationType
_SecAuthenticationTypeForSecAttrAuthenticationType(
	CFTypeRef authenticationType)
{
	SecAuthenticationType result = kSecAuthenticationTypeAny;

	if (authenticationType != NULL) {
		CFIndex count;
		for (count=0; count<kNumberOfAuthenticationTypes; count++) {
			if (CFEqual(authenticationType, *(gAuthTypes[count].authValue))) {
				result = gAuthTypes[count].authType;
				break;
			}
		}
	}

	return result;
}

/*
 * _SecAttrAuthenticationTypeForSecAuthenticationType converts a SecAuthenticationType
 * to a SecAttrAuthenticationType.
 */
static CFTypeRef
_SecAttrAuthenticationTypeForSecAuthenticationType(
	SecAuthenticationType authenticationType)
{
	CFTypeRef result = NULL;
	CFIndex count;
	for (count=0; count<kNumberOfAuthenticationTypes; count++) {
		if (gAuthTypes[count].authType == authenticationType) {
			result = *(gAuthTypes[count].authValue);
			break;
		}
	}

	return result;
}


/******************************************************************************/

struct KeyAlgorithmInfo {
	const CFStringRef *keyType;
	UInt32 keyValue;
};

static KeyAlgorithmInfo gKeyTypes[] = {
	{ &kSecAttrKeyTypeRSA, CSSM_ALGID_RSA },
	{ &kSecAttrKeyTypeDSA, CSSM_ALGID_DSA },
	{ &kSecAttrKeyTypeAES, CSSM_ALGID_AES },
	{ &kSecAttrKeyTypeDES, CSSM_ALGID_DES },
	{ &kSecAttrKeyType3DES, CSSM_ALGID_3DES },
	{ &kSecAttrKeyTypeRC4, CSSM_ALGID_RC4 },
	{ &kSecAttrKeyTypeRC2, CSSM_ALGID_RC2 },
	{ &kSecAttrKeyTypeCAST, CSSM_ALGID_CAST },
	{ &kSecAttrKeyTypeECDSA, CSSM_ALGID_ECDSA },
	{ &kSecAttrKeyTypeEC, CSSM_ALGID_ECDSA }
};

static const int kNumberOfKeyTypes = sizeof(gKeyTypes) / sizeof (KeyAlgorithmInfo);


static UInt32 _SecAlgorithmTypeFromSecAttrKeyType(
	CFTypeRef keyTypeRef)
{
	UInt32 keyAlgValue = 0;
	if (CFStringGetTypeID() != CFGetTypeID(keyTypeRef))
		return keyAlgValue;

	int ix;
	for (ix=0; ix<kNumberOfKeyTypes; ix++) {
		if (CFEqual(keyTypeRef, *(gKeyTypes[ix].keyType))) {
			keyAlgValue = gKeyTypes[ix].keyValue;
			return keyAlgValue;
		}
	}

	//%%%TODO try to convert the input string to a number here

	return keyAlgValue;
}


enum ItemRepresentation
{
	kStringRepresentation,
	kDataRepresentation,
	kNumberRepresentation,
	kBooleanRepresentation,
	kDateRepresentation
};


struct InternalAttributeListInfo
{
	UInt32 oldItemType;
	const CFStringRef *newItemType;
	ItemRepresentation itemRepresentation;
};


static InternalAttributeListInfo gGenericPasswordAttributes[] =
{
	{ kSecCreationDateItemAttr, &kSecAttrCreationDate, kDateRepresentation },
	{ kSecModDateItemAttr, &kSecAttrModificationDate, kDateRepresentation },
	{ kSecDescriptionItemAttr, &kSecAttrDescription, kStringRepresentation },
	{ kSecCommentItemAttr, &kSecAttrComment, kStringRepresentation },
	{ kSecCreatorItemAttr, &kSecAttrCreator, kNumberRepresentation }, // UInt32, a.k.a. FourCharCode
	{ kSecTypeItemAttr, &kSecAttrType, kNumberRepresentation }, // UInt32, a.k.a. FourCharCode
	{ kSecLabelItemAttr, &kSecAttrLabel, kStringRepresentation },
	{ kSecInvisibleItemAttr, &kSecAttrIsInvisible, kBooleanRepresentation },
	{ kSecNegativeItemAttr, &kSecAttrIsNegative, kBooleanRepresentation },
	{ kSecAccountItemAttr, &kSecAttrAccount, kStringRepresentation },
	{ kSecServiceItemAttr, &kSecAttrService, kStringRepresentation },
	{ kSecGenericItemAttr, &kSecAttrGeneric, kDataRepresentation }
};

static const int kNumberOfGenericPasswordAttributes = sizeof(gGenericPasswordAttributes) / sizeof (InternalAttributeListInfo);

#if 0
static InternalAttributeListInfo gInternetPasswordAttributes[] =
{
	{ kSecCreationDateItemAttr, &kSecAttrCreationDate, kDateRepresentation },
	{ kSecModDateItemAttr, &kSecAttrModificationDate, kDateRepresentation },
	{ kSecDescriptionItemAttr, &kSecAttrDescription, kStringRepresentation },
	{ kSecCommentItemAttr, &kSecAttrComment, kStringRepresentation },
	{ kSecCreatorItemAttr, &kSecAttrCreator, kNumberRepresentation }, // UInt32, a.k.a. FourCharCode
	{ kSecTypeItemAttr, &kSecAttrType, kNumberRepresentation }, // UInt32, a.k.a. FourCharCode
	{ kSecLabelItemAttr, &kSecAttrLabel, kStringRepresentation },
	{ kSecInvisibleItemAttr, &kSecAttrIsInvisible, kBooleanRepresentation },
	{ kSecNegativeItemAttr, &kSecAttrIsNegative, kBooleanRepresentation },
	{ kSecAccountItemAttr, &kSecAttrAccount, kStringRepresentation },
	{ kSecSecurityDomainItemAttr, &kSecAttrSecurityDomain, kStringRepresentation },
	{ kSecServerItemAttr, &kSecAttrServer, kStringRepresentation },
	{ kSecAuthenticationTypeItemAttr, &kSecAttrAuthenticationType, kStringRepresentation }, // maps from UInt32 value to string constant
	{ kSecPortItemAttr, &kSecAttrPort, kNumberRepresentation },
	{ kSecPathItemAttr, &kSecAttrPath, kStringRepresentation }
};

static const int kNumberOfInternetPasswordAttributes = sizeof(gInternetPasswordAttributes) / sizeof (InternalAttributeListInfo);
#endif

static InternalAttributeListInfo gCertificateAttributes[] =
{
	{ kSecLabelItemAttr, &kSecAttrLabel, kStringRepresentation },
	{ kSecSubjectItemAttr, &kSecAttrSubject, kDataRepresentation },
	{ kSecIssuerItemAttr, &kSecAttrIssuer, kDataRepresentation },
	{ kSecSerialNumberItemAttr, &kSecAttrSerialNumber, kDataRepresentation },
	{ kSecPublicKeyHashItemAttr, &kSecAttrPublicKeyHash, kDataRepresentation },
	{ kSecSubjectKeyIdentifierItemAttr, &kSecAttrSubjectKeyID, kDataRepresentation },
	{ kSecCertTypeItemAttr, &kSecAttrCertificateType, kDataRepresentation },
	{ kSecCertEncodingItemAttr, &kSecAttrCertificateEncoding, kDataRepresentation }
};

static const int kNumberOfCertificateAttributes = sizeof(gCertificateAttributes) / sizeof(InternalAttributeListInfo);


static InternalAttributeListInfo gKeyAttributes[] =
{
    { kSecKeyKeyClass, &kSecAttrKeyClass, kStringRepresentation }, // key class maps from UInt32 value to string constant
    { kSecKeyPrintName, &kSecAttrLabel, kStringRepresentation }, // note that "print name" maps to the user-visible label
//  { kSecKeyAlias, /* not yet exposed by SecItem */, kDataRepresentation },
    { kSecKeyPermanent, &kSecAttrIsPermanent, kBooleanRepresentation },
//  { kSecKeyPrivate, /* not yet exposed by SecItem */, kBooleanRepresentation },
//  { kSecKeyModifiable, /* not yet exposed by SecItem */, kBooleanRepresentation },
    { kSecKeyLabel, &kSecAttrApplicationLabel, kDataRepresentation }, // this contains the hash of the key (or the public key hash, if asymmetric) as a CFData. Legacy keys may contain a UUID as a CFString
    { kSecKeyApplicationTag, &kSecAttrApplicationTag, kDataRepresentation },
//  { kSecKeyKeyCreator, /* not yet exposed by SecItem */, kStringRepresentation }, // this is the GUID of the CSP that owns this key
    { kSecKeyKeyType, &kSecAttrKeyType, kStringRepresentation }, // algorithm type is given as a string constant (e.g. kSecAttrKeyTypeAES)
    { kSecKeyKeySizeInBits, &kSecAttrKeySizeInBits, kNumberRepresentation },
    { kSecKeyEffectiveKeySize, &kSecAttrEffectiveKeySize, kNumberRepresentation },
//  { kSecKeyStartDate, /* not yet exposed by SecItem */, kDateRepresentation },
//  { kSecKeyEndDate, /* not yet exposed by SecItem */, kDateRepresentation },
//  { kSecKeySensitive, /* not yet exposed by SecItem */, kBooleanRepresentation },
//  { kSecKeyAlwaysSensitive, /* not yet exposed by SecItem */, kBooleanRepresentation },
//  { kSecKeyExtractable, /* not yet exposed by SecItem */, kBooleanRepresentation },
//  { kSecKeyNeverExtractable, /* not yet exposed by SecItem */, kBooleanRepresentation },
    { kSecKeyEncrypt, &kSecAttrCanEncrypt, kBooleanRepresentation },
    { kSecKeyDecrypt, &kSecAttrCanDecrypt, kBooleanRepresentation },
    { kSecKeyDerive, &kSecAttrCanDerive, kBooleanRepresentation },
    { kSecKeySign, &kSecAttrCanSign, kBooleanRepresentation },
    { kSecKeyVerify, &kSecAttrCanVerify, kBooleanRepresentation },
//  { kSecKeySignRecover, /* not yet exposed by SecItem */, kBooleanRepresentation },
//  { kSecKeyVerifyRecover, /* not yet exposed by SecItem */, kBooleanRepresentation },
    { kSecKeyWrap, &kSecAttrCanWrap, kBooleanRepresentation },
    { kSecKeyUnwrap, &kSecAttrCanUnwrap, kBooleanRepresentation }
};

static const int kNumberOfKeyAttributes = sizeof(gKeyAttributes) / sizeof(InternalAttributeListInfo);


static void* CloneDataByType(ItemRepresentation type, CFTypeRef value, UInt32& length)
{
	switch (type)
	{
		case kStringRepresentation:
		{
			if (CFStringGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef) value), kCFStringEncodingUTF8) + 1;
			char* buffer = (char*) malloc(maxLength);
			Boolean converted = CFStringGetCString((CFStringRef) value, buffer, maxLength, kCFStringEncodingUTF8);
			if (converted) {
				length = (UInt32)strlen(buffer);
			}
			else {
				length = 0;
				free(buffer);
				buffer = NULL;
			}
			return buffer;
		}

		case kDataRepresentation:
		{
			if (CFStringGetTypeID() == CFGetTypeID(value)) {
                // We may have a string here, since the key label may be a GUID for the symmetric keys
                CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef) value), kCFStringEncodingUTF8) + 1;
                char* buffer = (char*) malloc(maxLength);
                Boolean converted = CFStringGetCString((CFStringRef) value, buffer, maxLength, kCFStringEncodingUTF8);
                if (converted) {
                    length = (UInt32)strlen(buffer);
                }
                else {
                    length = 0;
                    free(buffer);
                    buffer = NULL;
                }
                return buffer;
			}

			if (CFDataGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			length = (UInt32)CFDataGetLength((CFDataRef) value);
			uint8_t* buffer = (uint8_t*) malloc(length);
			CFDataGetBytes((CFDataRef) value, CFRangeMake(0, length), buffer);
			return buffer;
		}

		case kNumberRepresentation:
		{
			if (CFNumberGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			uint32_t* buffer = (uint32_t*) malloc(sizeof(uint32_t));
			Boolean converted = CFNumberGetValue((CFNumberRef) value, kCFNumberSInt32Type, buffer);
			if (converted) {
				length = sizeof(uint32_t);
			}
			else {
				length = 0;
				free(buffer);
				buffer = NULL;
			}
			return buffer;
		}

		case kBooleanRepresentation:
		{
			if (CFBooleanGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			uint32_t* buffer = (uint32_t*) malloc(sizeof(uint32_t));
			*buffer = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;
			length = sizeof(uint32_t);
			return buffer;
		}

		case kDateRepresentation:
		{
			if (CFDateGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			char* buffer = (char*) calloc(1, 32); // max length of a CSSM date string
			CSSMDateTimeUtils::CFDateToCssmDate((CFDateRef) value, buffer);
			length = (UInt32)strlen(buffer);
			return buffer;
		}

		default:
		{
			length = 0;
			return NULL;
		}
	}
}


static OSStatus
_ConvertNewFormatToOldFormat(
	CFAllocatorRef allocator,
	const InternalAttributeListInfo* info,
	int infoNumItems,
	CFDictionaryRef dictionaryRef,
	SecKeychainAttributeList* &attrList
	)
{
	// get the keychain attributes array from the data item
	// here's the problem.  On the one hand, we have a dictionary that is purported to contain
	// attributes for our type.  On the other hand, the dictionary may contain items we don't support,
	// and we therefore don't know how many attributes we will have unless we count them first

	// setup the return
	attrList = (SecKeychainAttributeList*) calloc(1, sizeof(SecKeychainAttributeList));

	// make storage to extract the dictionary items
	CFIndex itemsInDictionary = CFDictionaryGetCount(dictionaryRef);
	CFTypeRef keys[itemsInDictionary];
	CFTypeRef values[itemsInDictionary];

	CFTypeRef *keysPtr = keys;
	CFTypeRef *valuesPtr = values;

	CFDictionaryGetKeysAndValues(dictionaryRef, keys, values);

	// count the number of items we are interested in
	CFIndex count = 0;
	CFIndex i;

	// since this is one of those nasty order n^2 loops, we cache as much stuff as possible so that
	// we don't pay the price for this twice
	SecKeychainAttrType tags[itemsInDictionary];
	ItemRepresentation types[itemsInDictionary];

	for (i = 0; i < itemsInDictionary; ++i)
	{
		CFTypeRef key = keysPtr[i];

		int j;
		for (j = 0; j < infoNumItems; ++j)
		{
			if (CFEqual(*(info[j].newItemType), key))
			{
				tags[i] = info[j].oldItemType;
				types[i] = info[j].itemRepresentation;
				count += 1;
				break;
			}
		}

		if (j >= infoNumItems)
		{
			// if we got here, we aren't interested in this item.
			valuesPtr[i] = NULL;
		}
	}

	// now we can make the result array
	attrList->count = (UInt32)count;
	attrList->attr = (SecKeychainAttribute*) malloc(sizeof(SecKeychainAttribute) * count);

	// fill out the array
	int resultPointer = 0;
	for (i = 0; i < itemsInDictionary; ++i)
	{
		if (values[i] != NULL)
		{
			attrList->attr[resultPointer].tag = tags[i];

			// we have to clone the data pointer.  The caller will need to make sure to throw these away
			// with _FreeAttrList when it is done...
			attrList->attr[resultPointer].data = CloneDataByType(types[i], valuesPtr[i], attrList->attr[resultPointer].length);
			resultPointer += 1;
		}
	}

	return errSecSuccess;
}



static OSStatus
_ConvertOldFormatToNewFormat(
	CFAllocatorRef allocator,
	const InternalAttributeListInfo* info,
	int infoNumItems,
	SecKeychainItemRef itemRef,
	CFMutableDictionaryRef& dictionaryRef)
{
	SecKeychainAttributeList list;
	list.count = infoNumItems;
	list.attr = (SecKeychainAttribute*) calloc(infoNumItems, sizeof(SecKeychainAttribute));

	// fill out the array.  We only need to fill in the tags, since calloc zeros what it returns
	int i;
	for (i = 0; i < infoNumItems; ++i)
	{
		list.attr[i].tag = info[i].oldItemType;
	}

	OSStatus result = SecKeychainItemCopyContent(itemRef, NULL, &list, NULL, NULL);
	if (result != errSecSuccess)
	{
		dictionaryRef = NULL;
		free(list.attr);
		return result;
	}

	// create the dictionary
	dictionaryRef = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// add the pairs
	for (i = 0; i < infoNumItems; ++i)
	{
		if (list.attr[i].data == NULL)
			continue;

		switch (info[i].itemRepresentation)
		{
			case kStringRepresentation:
			{
				CFStringRef stringRef;
				if (info[i].oldItemType == kSecKeyKeyClass) {
					// special case: kSecKeyKeyClass is a UInt32 value that maps to a CFStringRef constant
					uint32_t keyRecordValue = *((uint32_t*)list.attr[i].data);
					bool retainString = true;
					switch (keyRecordValue) {
						case CSSM_DL_DB_RECORD_PUBLIC_KEY :
							stringRef = (CFStringRef) kSecAttrKeyClassPublic;
							break;
						case CSSM_DL_DB_RECORD_PRIVATE_KEY:
							stringRef = (CFStringRef) kSecAttrKeyClassPrivate;
							break;
						case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
							stringRef = (CFStringRef) kSecAttrKeyClassSymmetric;
							break;
						default:
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%d"), keyRecordValue);
							break;
					}
					if (stringRef) {
						if (retainString) CFRetain(stringRef);
						CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), stringRef);
						CFRelease(stringRef);
					}
				}
				else if (info[i].oldItemType == kSecKeyKeyType) {
					// special case: kSecKeyKeyType is a UInt32 value that maps to a CFStringRef constant
					uint32_t keyAlgValue = *((uint32_t*)list.attr[i].data);
					bool retainString = true;
					switch (keyAlgValue) {
						case CSSM_ALGID_RSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeRSA;
							break;
						case CSSM_ALGID_DSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeDSA;
							break;
						case CSSM_ALGID_AES :
							stringRef = (CFStringRef) kSecAttrKeyTypeAES;
							break;
						case CSSM_ALGID_DES :
							stringRef = (CFStringRef) kSecAttrKeyTypeDES;
							break;
						case CSSM_ALGID_3DES :
							stringRef = (CFStringRef) kSecAttrKeyType3DES;
							break;
						case CSSM_ALGID_RC4 :
							stringRef = (CFStringRef) kSecAttrKeyTypeRC4;
							break;
						case CSSM_ALGID_RC2 :
							stringRef = (CFStringRef) kSecAttrKeyTypeRC2;
							break;
						case CSSM_ALGID_CAST :
							stringRef = (CFStringRef) kSecAttrKeyTypeCAST;
							break;
						case CSSM_ALGID_ECDSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeEC;
							break;
						default :
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%d"), keyAlgValue);
							retainString = false;
							break;
					}
					if (stringRef) {
						if (retainString) CFRetain(stringRef);
						CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), stringRef);
						CFRelease(stringRef);
					}
				}
				else {
					// normal case: attribute contains a string
					stringRef = CFStringCreateWithBytes(allocator, (UInt8*)list.attr[i].data, list.attr[i].length, kCFStringEncodingUTF8, FALSE);
					if (stringRef == NULL)
						stringRef = (CFStringRef) CFRetain(kCFNull);
					CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), stringRef);
					CFRelease(stringRef);
				}
			}
			break;

			case kDataRepresentation:
			{
                if ((info[i].oldItemType == kSecKeyLabel) && (list.attr[i].length == kUUIDStringLength)) {
					// It's possible that there could be a string here because the key label may have a UUID
					CFStringRef stringRef = CFStringCreateWithBytes(allocator, (UInt8*)list.attr[i].data, list.attr[i].length, kCFStringEncodingUTF8, FALSE);
					if (stringRef == NULL)
						stringRef = (CFStringRef) CFRetain(kCFNull);
					CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), stringRef);
					CFRelease(stringRef);
                    break;
                }
                CFDataRef dataRef = CFDataCreate(allocator, (UInt8*) list.attr[i].data, list.attr[i].length);
                if (dataRef == NULL)
                    dataRef = (CFDataRef) CFRetain(kCFNull);
                CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), dataRef);
                CFRelease(dataRef);
			}
			break;

			case kNumberRepresentation:
			{
				CFNumberRef numberRef = CFNumberCreate(allocator, kCFNumberSInt32Type, list.attr[i].data);
				if (numberRef == NULL)
					numberRef = (CFNumberRef) CFRetain(kCFNull);
				CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), numberRef);
				CFRelease(numberRef);
			}
			break;

			case kBooleanRepresentation:
			{
				uint32_t value = *((uint32_t*)list.attr[i].data);
				CFBooleanRef boolRef = (value) ? kCFBooleanTrue : kCFBooleanFalse;
				CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), boolRef);
			}
			break;

			case kDateRepresentation:
			{
				CFDateRef dateRef = NULL;
				CSSMDateTimeUtils::CssmDateStringToCFDate((const char *)list.attr[i].data, list.attr[i].length, &dateRef);
				if (dateRef == NULL)
					dateRef = (CFDateRef) CFRetain(kCFNull);
				CFDictionaryAddValue(dictionaryRef, *(info[i].newItemType), dateRef);
				CFRelease(dateRef);
			}
			break;
		}
	}

	// cleanup
	SecKeychainItemFreeContent(&list, NULL);
	free(list.attr);

	return result;
}



//
/*
 * _CreateAttributesDictionaryFromGenericPasswordItem creates a CFDictionaryRef using the
 * attributes of item.
 */
static OSStatus
_CreateAttributesDictionaryFromGenericPasswordItem(
	CFAllocatorRef allocator,
	SecKeychainItemRef item,
	CFDictionaryRef *dictionary)
{
	// do the basic allocations
	CFMutableDictionaryRef dict = NULL;
	OSStatus result = _ConvertOldFormatToNewFormat(allocator, gGenericPasswordAttributes, kNumberOfGenericPasswordAttributes, item, dict);
	if (result == errSecSuccess) // did we complete OK
	{
		CFDictionaryAddValue(dict, kSecClass, kSecClassGenericPassword);
	}

	*dictionary = dict;

	return result;
}



/*
 * _CreateAttributesDictionaryFromCertificateItem creates a CFDictionaryRef using the
 * attributes of item.
 */
static OSStatus
_CreateAttributesDictionaryFromCertificateItem(
	CFAllocatorRef allocator,
	SecKeychainItemRef item,
	CFDictionaryRef *dictionary)
{
	// do the basic allocations
	CFMutableDictionaryRef dict = NULL;
	OSStatus result = _ConvertOldFormatToNewFormat(allocator, gCertificateAttributes, kNumberOfCertificateAttributes, item, dict);
	if (result == errSecSuccess) // did we complete OK
	{
		CFDictionaryAddValue(dict, kSecClass, kSecClassCertificate);
	}

	*dictionary = dict;

	return errSecSuccess;
}

/*
 * _CreateAttributesDictionaryFromKeyItem creates a CFDictionaryRef using the
 * attributes of item.
 */
static OSStatus
_CreateAttributesDictionaryFromKeyItem(
	CFAllocatorRef allocator,
	SecKeychainItemRef item,
	CFDictionaryRef *dictionary)
{
#if 0
//%%%FIXME this ought to work, but the call to SecKeychainCopyContent in _ConvertOldFormatToNewFormat fails.
// Need to rewrite _ConvertOldFormatToNewFormat so that it uses SecKeychainAttributeInfoForItemID and
// SecKeychainItemCopyAttributesAndData to get the attributes, rather than SecKeychainCopyContent.

	if (status) {
		goto error_exit; // unable to get the attribute info (i.e. database schema)
	}

	status = SecKeychainItemCopyAttributesAndData(item, info, &itemClass, &attrList, NULL, NULL);

	// do the basic allocations
	CFMutableDictionaryRef dict = NULL;
	OSStatus result = _ConvertOldFormatToNewFormat(allocator, gKeyAttributes, kNumberOfKeyAttributes, item, dict);
	if (result == errSecSuccess) // did we complete OK
	{
		CFDictionaryAddValue(dict, kSecClass, kSecClassKey);
	}

	*dictionary = dict;

	return errSecSuccess;
#endif

	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	unsigned int ix;
	SecItemClass itemClass = (SecItemClass) 0;
	UInt32 itemID;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttributeInfo *info = NULL;
	SecKeychainRef keychain = NULL;

	OSStatus status = SecKeychainItemCopyAttributesAndData(item, NULL, &itemClass, NULL, NULL, NULL);
	if (status) {
		goto error_exit; // item must have an itemClass
	}

	switch (itemClass)
	{
    case kSecInternetPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_INTERNET_PASSWORD;
		break;
    case kSecGenericPasswordItemClass:
		itemID = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
		break;
    case 'ashp': /* kSecAppleSharePasswordItemClass */
		itemID = CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD;
		break;
	default:
		itemID = itemClass;
		break;
	}

	status = SecKeychainItemCopyKeychain(item, &keychain);
	if (status) {
		goto error_exit; // item must have a keychain, so we can get the attribute info for it
	}

	status = SecKeychainAttributeInfoForItemID(keychain, itemID, &info);
	if (status) {
		goto error_exit; // unable to get the attribute info (i.e. database schema)
	}

	status = SecKeychainItemCopyAttributesAndData(item, info, &itemClass, &attrList, NULL, NULL);
	if (status) {
		goto error_exit; // unable to get the attribute info (i.e. database schema)
	}

	for (ix = 0; ix < info->count; ++ix)
	{
		SecKeychainAttribute *attribute = &attrList->attr[ix];
		if (!attribute->length && !attribute->data)
			continue;

		UInt32 j, count = kNumberOfKeyAttributes;
		InternalAttributeListInfo *intInfo = NULL;
		for (j=0; j<count; j++) {
			if (gKeyAttributes[j].oldItemType == info->tag[ix]) {
				intInfo = &gKeyAttributes[j];
				break;
			}
		}
		if (!intInfo)
			continue;

		switch (intInfo->itemRepresentation)
		{
			case kStringRepresentation:
			{
				CFStringRef stringRef;
				if (intInfo->oldItemType == kSecKeyKeyClass) {
					// special case: kSecKeyKeyClass is a UInt32 value that maps to a CFStringRef constant
					UInt32 keyRecordValue = *((UInt32*)attribute->data);
					bool retainString = true;
					switch (keyRecordValue) {
						case CSSM_DL_DB_RECORD_PUBLIC_KEY :
							stringRef = (CFStringRef) kSecAttrKeyClassPublic;
							break;
						case CSSM_DL_DB_RECORD_PRIVATE_KEY:
							stringRef = (CFStringRef) kSecAttrKeyClassPrivate;
							break;
						case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
							stringRef = (CFStringRef) kSecAttrKeyClassSymmetric;
							break;
						default:
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%u"), (unsigned int)keyRecordValue);
							break;
					}
					if (stringRef) {
						if (retainString) CFRetain(stringRef);
						CFDictionaryAddValue(dict, *(intInfo->newItemType), stringRef);
						CFRelease(stringRef);
					}
				}
				else if (intInfo->oldItemType == kSecKeyKeyType) {
					// special case: kSecKeyKeyType is a UInt32 value that maps to a CFStringRef constant
					UInt32 keyAlgValue = *((UInt32*)attribute->data);
					bool retainString = true;
					switch (keyAlgValue) {
						case CSSM_ALGID_RSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeRSA;
							break;
						case CSSM_ALGID_DSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeDSA;
							break;
						case CSSM_ALGID_AES :
							stringRef = (CFStringRef) kSecAttrKeyTypeAES;
							break;
						case CSSM_ALGID_DES :
							stringRef = (CFStringRef) kSecAttrKeyTypeDES;
							break;
						case CSSM_ALGID_3DES :
							stringRef = (CFStringRef) kSecAttrKeyType3DES;
							break;
						case CSSM_ALGID_RC4 :
							stringRef = (CFStringRef) kSecAttrKeyTypeRC4;
							break;
						case CSSM_ALGID_RC2 :
							stringRef = (CFStringRef) kSecAttrKeyTypeRC2;
							break;
						case CSSM_ALGID_CAST :
							stringRef = (CFStringRef) kSecAttrKeyTypeCAST;
							break;
						case CSSM_ALGID_ECDSA :
							stringRef = (CFStringRef) kSecAttrKeyTypeEC;
							break;
						default :
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%u"), (unsigned int)keyAlgValue);
							retainString = false;
							break;
					}
					if (stringRef) {
						if (retainString) CFRetain(stringRef);
						CFDictionaryAddValue(dict, *(intInfo->newItemType), stringRef);
						CFRelease(stringRef);
					}
                }
				else {
					// normal case: attribute contains a string
					stringRef = CFStringCreateWithBytes(allocator, (UInt8*)attribute->data, attribute->length, kCFStringEncodingUTF8, FALSE);
					if (stringRef == NULL)
						stringRef = (CFStringRef) CFRetain(kCFNull);
					CFDictionaryAddValue(dict, *(intInfo->newItemType), stringRef);
					CFRelease(stringRef);
				}
			}
			break;

			case kDataRepresentation:
			{
                if ((intInfo->oldItemType == kSecKeyLabel) && (attribute->length == kUUIDStringLength)) {
					// It's possible that there could be a string here because the key label may have a UUID
                    CFStringRef stringRef = CFStringCreateWithBytes(allocator, (UInt8*)attribute->data, attribute->length, kCFStringEncodingUTF8, FALSE);
					if (stringRef == NULL)
						stringRef = (CFStringRef) CFRetain(kCFNull);
					CFDictionaryAddValue(dict, *(intInfo->newItemType), stringRef);
					CFRelease(stringRef);
                    break;
                }

				CFDataRef dataRef = CFDataCreate(allocator, (UInt8*)attribute->data, attribute->length);
				if (dataRef == NULL)
					dataRef = (CFDataRef) CFRetain(kCFNull);
				CFDictionaryAddValue(dict, *(intInfo->newItemType), dataRef);
				CFRelease(dataRef);
			}
			break;

			case kNumberRepresentation:
			{
				CFNumberRef numberRef = CFNumberCreate(allocator, kCFNumberSInt32Type, attribute->data);
				if (numberRef == NULL)
					numberRef = (CFNumberRef) CFRetain(kCFNull);
				CFDictionaryAddValue(dict, *(intInfo->newItemType), numberRef);
				CFRelease(numberRef);
			}
			break;

			case kBooleanRepresentation:
			{
				UInt32 value = *((UInt32*)attribute->data);
				CFBooleanRef boolRef = (value) ? kCFBooleanTrue : kCFBooleanFalse;
				CFDictionaryAddValue(dict, *(intInfo->newItemType), boolRef);
			}
			break;

			case kDateRepresentation:
			{
				//%%% FIXME need to convert from a CSSM date string to a CFDateRef here
				CFDateRef dateRef = NULL;
				if (dateRef == NULL)
					dateRef = (CFDateRef) CFRetain(kCFNull);
				CFDictionaryAddValue(dict, *(intInfo->newItemType), dateRef);
				CFRelease(dateRef);
			}
			break;
		}
	}

	CFDictionaryAddValue(dict, kSecClass, kSecClassKey);

error_exit:

	if (attrList)
		SecKeychainItemFreeAttributesAndData(attrList, NULL);

	if (info)
		SecKeychainFreeAttributeInfo(info);

	if (keychain)
		CFRelease(keychain);

	*dictionary = dict;

	return status;
}


/*
 * _CreateAttributesDictionaryFromInternetPasswordItem creates a CFDictionaryRef using the
 * attributes of item.
 */
static OSStatus
_CreateAttributesDictionaryFromInternetPasswordItem(
	CFAllocatorRef allocator,
	SecKeychainItemRef item,
	CFDictionaryRef *dictionary)
{
	OSStatus status;
	SecKeychainAttribute attr[] = {
		{ kSecServerItemAttr, 0, NULL },		/* [0] server */
		{ kSecSecurityDomainItemAttr, 0, NULL },	/* [1] securityDomain */
		{ kSecAccountItemAttr, 0, NULL },		/* [2] account */
		{ kSecPathItemAttr, 0, NULL },			/* [3] path */
		{ kSecPortItemAttr, 0, NULL },			/* [4] port */
		{ kSecProtocolItemAttr, 0, NULL },		/* [5] protocol */
		{ kSecAuthenticationTypeItemAttr, 0, NULL },	/* [6] authenticationType */
		{ kSecCommentItemAttr, 0, NULL },		/* [7] comment */
		{ kSecDescriptionItemAttr, 0, NULL },		/* [8] description */
		{ kSecLabelItemAttr, 0, NULL },			/* [9] label */
		{ kSecCreationDateItemAttr, 0, NULL },	/* [10] creation date */
		{ kSecModDateItemAttr, 0, NULL },		/* [11] modification date */
		{ kSecCreatorItemAttr, 0, NULL },		/* [12] creator */
		{ kSecTypeItemAttr, 0, NULL },			/* [13] type */
		{ kSecInvisibleItemAttr, 0, NULL },		/* [14] invisible */
		{ kSecNegativeItemAttr, 0, NULL },		/* [15] negative */
	};
	SecKeychainAttributeList attrList = { sizeof(attr) / sizeof(SecKeychainAttribute), attr };
	CFIndex numValues;
	CFIndex index;
	CFTypeRef keys[(sizeof(attr) / sizeof(SecKeychainAttribute)) + 2];
	CFTypeRef values[(sizeof(attr) / sizeof(SecKeychainAttribute)) + 2];

	*dictionary = NULL;

	// copy the item's attributes
	status = SecKeychainItemCopyContent(item, NULL, &attrList, NULL, NULL);
	require_noerr(status, SecKeychainItemCopyContent_failed);

	numValues = 0;

	// add kSecClass
	keys[numValues] = kSecClass;
	values[numValues] = kSecClassInternetPassword;
	++numValues;

	// add kSecAttrServer
	if ( attrList.attr[0].length > 0 ) {
		keys[numValues] = kSecAttrServer;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[0].data, attrList.attr[0].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrSecurityDomain
	if ( attrList.attr[1].length > 0 ) {
		keys[numValues] = kSecAttrSecurityDomain;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[1].data, attrList.attr[1].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrAccount
	if ( attrList.attr[2].length > 0 ) {
		keys[numValues] = kSecAttrAccount;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[2].data, attrList.attr[2].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrPath
	if ( attrList.attr[3].length > 0 ) {
		keys[numValues] = kSecAttrPath;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[3].data, attrList.attr[3].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrPort
	if ( attrList.attr[4].length > 0 ) {
		keys[numValues] = kSecAttrPort;
		values[numValues] = CFNumberCreate(allocator, kCFNumberSInt32Type, attrList.attr[4].data);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrProtocol
	if ( attrList.attr[5].length > 0 ) {
		keys[numValues] = kSecAttrProtocol;
		values[numValues] = _SecAttrProtocolForSecProtocolType(*(SecProtocolType*)attrList.attr[5].data);
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// add kSecAttrAuthenticationType
	if ( attrList.attr[6].length > 0 ) {
		keys[numValues] = kSecAttrAuthenticationType;
		values[numValues] = _SecAttrAuthenticationTypeForSecAuthenticationType( (SecAuthenticationType) (*(SecProtocolType*)attrList.attr[6].data));
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// add kSecAttrComment
	if ( attrList.attr[7].length > 0 ) {
		keys[numValues] = kSecAttrComment;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[7].data, attrList.attr[7].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrDescription
	if ( attrList.attr[8].length > 0 ) {
		keys[numValues] = kSecAttrDescription;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[8].data, attrList.attr[8].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrLabel
	if ( attrList.attr[9].length > 0 ) {
		keys[numValues] = kSecAttrLabel;
		values[numValues] = CFStringCreateWithBytes(allocator, (UInt8 *)attrList.attr[9].data, attrList.attr[9].length, kCFStringEncodingUTF8, FALSE);
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrCreationDate
	if ( attrList.attr[10].length > 0 ) {
		CFDateRef creationDate = NULL;
		CSSMDateTimeUtils::CssmDateStringToCFDate((const char *)attrList.attr[10].data, attrList.attr[10].length, &creationDate);
		keys[numValues] = kSecAttrCreationDate;
		values[numValues] = creationDate;
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecAttrModificationDate
	if ( attrList.attr[11].length > 0 ) {
		CFDateRef modDate = NULL;
		CSSMDateTimeUtils::CssmDateStringToCFDate((const char *)attrList.attr[11].data, attrList.attr[11].length, &modDate);
		keys[numValues] = kSecAttrModificationDate;
		values[numValues] = modDate;
		if ( values[numValues] != NULL ) {
			++numValues;
		}
	}

	// add kSecCreatorItemAttr
	if ( attrList.attr[12].length > 0 ) {
		CFNumberRef numberRef = CFNumberCreate(allocator, kCFNumberSInt32Type, attrList.attr[12].data);
		keys[numValues] = kSecAttrCreator;
		values[numValues] = numberRef;
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// add kSecTypeItemAttr
	if ( attrList.attr[13].length > 0 ) {
		CFNumberRef numberRef = CFNumberCreate(allocator, kCFNumberSInt32Type, attrList.attr[13].data);
		keys[numValues] = kSecAttrType;
		values[numValues] = numberRef;
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// add kSecInvisibleItemAttr
	if ( attrList.attr[14].length > 0 ) {
		uint32_t value = *((uint32_t*)attrList.attr[14].data);
		CFBooleanRef boolRef = (value) ? kCFBooleanTrue : kCFBooleanFalse;
		keys[numValues] = kSecAttrIsInvisible;
		values[numValues] = boolRef;
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// add kSecNegativeItemAttr
	if ( attrList.attr[15].length > 0 ) {
		uint32_t value = *((uint32_t*)attrList.attr[15].data);
		CFBooleanRef boolRef = (value) ? kCFBooleanTrue : kCFBooleanFalse;
		keys[numValues] = kSecAttrIsNegative;
		values[numValues] = boolRef;
		if ( values[numValues] != NULL ) {
			CFRetain(values[numValues]);
			++numValues;
		}
	}

	// create the dictionary
	*dictionary = CFDictionaryCreate(allocator, keys, values, numValues, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// release the values added to the dictionary
	for ( index = 0; index < numValues; ++index )
	{
		CFRelease(values[index]);
	}

	// and free the attributes
	(void) SecKeychainItemFreeContent(&attrList, NULL);

SecKeychainItemCopyContent_failed:

	return ( status );
}


/*
 * _CreateAttributesDictionaryFromItem creates a CFDictionaryRef using the
 * attributes of the specified item class and item.
 */
static OSStatus
_CreateAttributesDictionaryFromItem(
	CFAllocatorRef allocator,
	SecItemClass itemClass,
	SecKeychainItemRef item,
	CFDictionaryRef *dictionary)
{
	switch (itemClass)
	{
		case kSecInternetPasswordItemClass:
			return _CreateAttributesDictionaryFromInternetPasswordItem(allocator, item, dictionary);

		case kSecGenericPasswordItemClass:
			return _CreateAttributesDictionaryFromGenericPasswordItem(allocator, item, dictionary);

		case kSecCertificateItemClass:
			return _CreateAttributesDictionaryFromCertificateItem(allocator, item, dictionary);

		case kSecPublicKeyItemClass:
		case kSecPrivateKeyItemClass:
		case kSecSymmetricKeyItemClass:
			return _CreateAttributesDictionaryFromKeyItem(allocator, item, dictionary);

		default:
			*dictionary = NULL;
			break;
	}
	return errSecParam;
}


/*
 * _FreeAttrList frees the memory allocated for the SecKeychainAttributeList
 * by the _CreateSecKeychainAttributeListFromDictionary function.
 */
static void
_FreeAttrList(
	SecKeychainAttributeList *attrListPtr)
{
	UInt32 index;

	if ( attrListPtr != NULL ) {
		if ( attrListPtr->attr != NULL ) {
			// free any attribute data
			for ( index = 0; index < attrListPtr->count; ++index ) {
				free(attrListPtr->attr[index].data);
			}
			// free the attribute array
			free(attrListPtr->attr);
		}
		// free the attribute list
		free(attrListPtr);
	}
}

/*
 * _CFDataCreateAttribute initializes the SecKeychainAttribute pointed to by
 * attr using the data and tag parameters.
 *
 * The memory for the SecKeychainAttribute's data field is allocated with malloc
 * and must be released by the caller (this is normally done by calling _FreeAttrList).
 */
static OSStatus
_CFDataCreateAttribute(
	CFDataRef data,
	SecKeychainAttrType tag,
	SecKeychainAttributePtr attr)
{
	OSStatus status = errSecSuccess;
	CFRange range;

	// set the attribute tag
	attr->tag = tag;

	// determine the attribute length
	attr->length = (UInt32) CFDataGetLength(data);
	range = CFRangeMake(0, (CFIndex)attr->length);

	// allocate memory for the attribute bytes
	attr->data = malloc(attr->length);
	require_action(attr->data != NULL, malloc_failed, status = errSecBufferTooSmall);

	// get the attribute bytes
	CFDataGetBytes(data, range, (UInt8 *)attr->data);

malloc_failed:

	return ( status );
}

/*
 * _CFStringCreateAttribute initializes the SecKeychainAttribute pointed to by
 * attr using the string and tag parameters.
 *
 * The memory for the SecKeychainAttribute's data field is allocated with malloc
 * and must be released by the caller (this is normally done by calling _FreeAttrList).
 */
static OSStatus
_CFStringCreateAttribute(
	CFStringRef string,
	SecKeychainAttrType tag,
	SecKeychainAttributePtr attr)
{
	OSStatus status = errSecSuccess;
	CFRange range;

	// set the attribute tag
	attr->tag = tag;

	// determine the attribute length
	range = CFRangeMake(0, CFStringGetLength(string));
	CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, FALSE, NULL, 0, (CFIndex *)&attr->length);

	// allocate memory for the attribute bytes
	attr->data = malloc(attr->length);
	require_action(attr->data != NULL, malloc_failed, status = errSecBufferTooSmall);

	// get the attribute bytes
	CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, FALSE, (UInt8 *)attr->data, attr->length, NULL);

malloc_failed:

	return ( status );
}


/*
 * _CreateSecKeychainGenericPasswordAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns errSecSuccess, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainGenericPasswordAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecKeychainAttributeList **attrList)
{
	return _ConvertNewFormatToOldFormat(NULL, gGenericPasswordAttributes, kNumberOfGenericPasswordAttributes, attrDictionary, *attrList);
}


/*
 * _CreateSecKeychainCertificateAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns errSecSuccess, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainCertificateAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecKeychainAttributeList **attrList)
{
	return _ConvertNewFormatToOldFormat(NULL, gCertificateAttributes, kNumberOfCertificateAttributes, attrDictionary, *attrList);
}


/*
 * _CreateSecKeychainKeyAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns errSecSuccess, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainKeyAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecKeychainAttributeList **attrList)
{
#if 0
	//%%%FIXME this function should work for key attributes, but currently doesn't; need to debug
	return _ConvertNewFormatToOldFormat(NULL, gKeyAttributes, kNumberOfKeyAttributes, attrDictionary, *attrList);
#else
	// explicitly build attribute list for supported key attributes
	// NOTE: this code supports only MaxSecKeyAttributes (15) attributes
	const int MaxSecKeyAttributes = 15;

	OSStatus status;
	CFTypeRef value;
	SecKeychainAttributeList *attrListPtr;

	attrListPtr = (SecKeychainAttributeList*)calloc(1, sizeof(SecKeychainAttributeList));
	require_action(attrListPtr != NULL, calloc_attrListPtr_failed, status = errSecBufferTooSmall);

	attrListPtr->attr = (SecKeychainAttribute*)calloc(MaxSecKeyAttributes, sizeof(SecKeychainAttribute));
	require_action(attrListPtr->attr != NULL, malloc_attrPtr_failed, status = errSecBufferTooSmall);

	// [0] get the kSecKeyKeyClass value
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrKeyClass, (const void **)&value) && value) {
		UInt32 keyRecordValue = 0;
		if (CFEqual(kSecAttrKeyClassPublic, value))
			keyRecordValue = CSSM_DL_DB_RECORD_PUBLIC_KEY;
		else if (CFEqual(kSecAttrKeyClassPrivate, value))
			keyRecordValue = CSSM_DL_DB_RECORD_PRIVATE_KEY;
		else if (CFEqual(kSecAttrKeyClassSymmetric, value))
			keyRecordValue = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;

		// only use this attribute if we recognize the value!
		if (keyRecordValue != 0) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyKeyClass;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = keyRecordValue;

			++attrListPtr->count;
		}
	}

	// [1] get the kSecKeyPrintName string
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrLabel, (const void **)&value) && value) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecKeyPrintName, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [2] get the kSecKeyPermanent boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrIsPermanent, (const void **)&value) && value) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

		attrListPtr->attr[attrListPtr->count].tag = kSecKeyPermanent;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

		++attrListPtr->count;
	}

	// [3] get the kSecKeyLabel string
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrApplicationLabel, (const void **)&value) && value) {
        if (CFStringGetTypeID() == CFGetTypeID(value))
            status = _CFStringCreateAttribute((CFStringRef)value, kSecKeyLabel, &attrListPtr->attr[attrListPtr->count]);
        else if (CFDataGetTypeID() == CFGetTypeID(value))
            status = _CFDataCreateAttribute((CFDataRef)value, kSecKeyLabel, &attrListPtr->attr[attrListPtr->count]);
        else
            status = errSecParam;

		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [4] get the kSecKeyApplicationTag data
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrApplicationTag, (const void **)&value) && value) {
		if (CFStringGetTypeID() == CFGetTypeID(value))
			status = _CFStringCreateAttribute((CFStringRef)value, kSecKeyApplicationTag, &attrListPtr->attr[attrListPtr->count]);
		else if (CFDataGetTypeID() == CFGetTypeID(value))
			status = _CFDataCreateAttribute((CFDataRef)value, kSecKeyApplicationTag, &attrListPtr->attr[attrListPtr->count]);
		else
			status = errSecParam;

		require_noerr_quiet(status, CFDataCreateAttribute_failed);
		++attrListPtr->count;
	}

	// [5] get the kSecKeyKeyType number
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrKeyType, (const void **)&value) && value) {
		UInt32 keyAlgValue = _SecAlgorithmTypeFromSecAttrKeyType(kSecAttrKeyType);
		if (keyAlgValue != 0) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyKeyType;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = keyAlgValue;

			++attrListPtr->count;
		}
	}

	// [6] get the kSecKeyKeySizeInBits number
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrKeySizeInBits, (const void **)&value) && value) {
		if (CFNumberGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyKeySizeInBits;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);

			++attrListPtr->count;
		}
	}

	// [7] get the kSecKeyEffectiveKeySize number
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrEffectiveKeySize, (const void **)&value) && value) {
		if (CFNumberGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyEffectiveKeySize;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);

			++attrListPtr->count;
		}
	}

	// [8] get the kSecKeyEncrypt boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanEncrypt, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyEncrypt;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [9] get the kSecKeyDecrypt boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanDecrypt, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyDecrypt;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [10] get the kSecKeyDerive boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanDerive, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyDerive;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [11] get the kSecKeySign boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanSign, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeySign;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [12] get the kSecKeyVerify boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanVerify, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyVerify;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [13] get the kSecKeyWrap boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanWrap, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyWrap;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// [14] get the kSecKeyUnwrap boolean
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCanUnwrap, (const void **)&value) && value) {
		if (CFBooleanGetTypeID() == CFGetTypeID(value)) {
			attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
			require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_number_failed, status = errSecBufferTooSmall);

			attrListPtr->attr[attrListPtr->count].tag = kSecKeyUnwrap;
			attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
			*((UInt32*)attrListPtr->attr[attrListPtr->count].data) = (CFEqual(kCFBooleanTrue, value)) ? 1 : 0;

			++attrListPtr->count;
		}
	}

	// return the pointer to the attrList
	*attrList = attrListPtr;

	return ( errSecSuccess );

	/***************/

malloc_number_failed:
CFDataCreateAttribute_failed:
CFStringCreateAttribute_failed:
malloc_attrPtr_failed:

	// free any attributes
	_FreeAttrList(attrListPtr);

calloc_attrListPtr_failed:

	return ( errSecBufferTooSmall );

#endif
}

static CFTypeRef copyNumber(CFTypeRef obj)
{
    if (!obj)
        return NULL;

    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFNumberGetTypeID())
    {
        CFRetain(obj);
        return obj;
    }

    if (tid == CFBooleanGetTypeID())
    {
        SInt32 value = CFBooleanGetValue((CFBooleanRef)obj);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    }

    if (tid == CFStringGetTypeID())
    {
        SInt32 value = CFStringGetIntValue((CFStringRef)obj);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long) value);
        /* If a string converted to an int isn't equal to the int printed as
         a string, return a NULL instead. */
        if (!CFEqual(t, obj))
        {
            CFRelease(t);
            return NULL;
        }
        CFRelease(t);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    }
    return NULL;
}

/*
 * _CreateSecKeychainInternetPasswordAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns errSecSuccess, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainInternetPasswordAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecKeychainAttributeList **attrList)
{
	// explicitly build attribute list for supported key attributes
	// NOTE: this code supports only MaxSecKeychainAttributes (14) attributes
	const int MaxSecKeychainAttributes = 14;

	OSStatus status;
	CFTypeRef value;
	SecKeychainAttributeList *attrListPtr;

	attrListPtr = (SecKeychainAttributeList*)calloc(1, sizeof(SecKeychainAttributeList));
	require_action(attrListPtr != NULL, calloc_attrListPtr_failed, status = errSecBufferTooSmall);

	attrListPtr->attr = (SecKeychainAttribute*)calloc(MaxSecKeychainAttributes, sizeof(SecKeychainAttribute));
	require_action(attrListPtr->attr != NULL, malloc_attrPtr_failed, status = errSecBufferTooSmall);


	// [0] get the serverName string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrServer, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecServerItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [1] get the securityDomain string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrSecurityDomain, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecSecurityDomainItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [2] get the accountName string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrAccount, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecAccountItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [3] get the path string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrPath, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecPathItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [4] get the port number
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrPort, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt16));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);

        CFTypeRef num = copyNumber(value);
		require_action(num != NULL, CFStringCreateAttribute_failed, status = errSecParam);
		attrListPtr->attr[attrListPtr->count].tag = kSecPortItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt16);
		CFNumberGetValue((CFNumberRef)num, kCFNumberSInt16Type, attrListPtr->attr[attrListPtr->count].data);
        CFRelease(num);

		++attrListPtr->count;
	}

	// [5] get the protocol
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrProtocol, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(SecProtocolType));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_protocol_failed, status = errSecBufferTooSmall);

		attrListPtr->attr[attrListPtr->count].tag = kSecProtocolItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(SecProtocolType);
		*(SecProtocolType *)(attrListPtr->attr[attrListPtr->count].data) = _SecProtocolTypeForSecAttrProtocol(value);

		++attrListPtr->count;
	}

	// [6] get the authenticationType
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrAuthenticationType, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(SecAuthenticationType));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_authenticationType_failed, status = errSecBufferTooSmall);

		attrListPtr->attr[attrListPtr->count].tag = kSecAuthenticationTypeItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(SecAuthenticationType);
		*(SecAuthenticationType *)(attrListPtr->attr[attrListPtr->count].data) = _SecAuthenticationTypeForSecAttrAuthenticationType(value);

		++attrListPtr->count;
	}

	// [7] get the comment string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrComment, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecCommentItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [8] get the description string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrDescription, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecDescriptionItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [9] get the label string
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrLabel, (const void **)&value) ) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecLabelItemAttr, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);

		++attrListPtr->count;
	}

	// [10] get the creator code
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrCreator, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);

        CFTypeRef num = copyNumber(value);
		require_action(num != NULL, CFStringCreateAttribute_failed, status = errSecParam);
		attrListPtr->attr[attrListPtr->count].tag = kSecCreatorItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		CFNumberGetValue((CFNumberRef)num, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);
        CFRelease(num);

		++attrListPtr->count;
	}

	// [11] get the type code
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrType, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);

        CFTypeRef num = copyNumber(value);
		require_action(num != NULL, CFStringCreateAttribute_failed, status = errSecParam);
		attrListPtr->attr[attrListPtr->count].tag = kSecTypeItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		CFNumberGetValue((CFNumberRef)num, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);
        CFRelease(num);

		++attrListPtr->count;
	}

	// [12] get the invisible flag
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrIsInvisible, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);

		attrListPtr->attr[attrListPtr->count].tag = kSecInvisibleItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		*(UInt32 *)(attrListPtr->attr[attrListPtr->count].data) = (CFBooleanGetValue((CFBooleanRef)value)) ? 1 : 0;

		++attrListPtr->count;
	}

	// [13] get the negative flag
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrIsNegative, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);

		attrListPtr->attr[attrListPtr->count].tag = kSecNegativeItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		*(UInt32 *)(attrListPtr->attr[attrListPtr->count].data) = (CFBooleanGetValue((CFBooleanRef)value)) ? 1 : 0;

		++attrListPtr->count;
	}

	// return the pointer to the attrList
	*attrList = attrListPtr;

	return ( errSecSuccess );

	/***************/

malloc_authenticationType_failed:
malloc_protocol_failed:
malloc_port_failed:
CFStringCreateAttribute_failed:
malloc_attrPtr_failed:

	// free any attributes
	_FreeAttrList(attrListPtr);

calloc_attrListPtr_failed:

	return ( errSecBufferTooSmall );
}


/*
 * _CreateSecKeychainAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary for the specified item class.
 *
 * If this function returns errSecSuccess, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecItemClass itemClass,
	SecKeychainAttributeList **attrList)
{
	switch (itemClass)
	{
		case kSecInternetPasswordItemClass:
			return _CreateSecKeychainInternetPasswordAttributeListFromDictionary(attrDictionary, attrList);

		case kSecGenericPasswordItemClass:
			return _CreateSecKeychainGenericPasswordAttributeListFromDictionary(attrDictionary, attrList);

		case kSecCertificateItemClass:
			return _CreateSecKeychainCertificateAttributeListFromDictionary(attrDictionary, attrList);

		case kSecPublicKeyItemClass:
		case kSecPrivateKeyItemClass:
		case kSecSymmetricKeyItemClass:
			return _CreateSecKeychainKeyAttributeListFromDictionary(attrDictionary, attrList);

		default:
			break;
	}
	return errSecParam;
}


/*
 * _AppNameFromSecTrustedApplication attempts to pull the name of the
 * application/tool from the SecTrustedApplicationRef.
 */
static CFStringRef CF_RETURNS_RETAINED
_AppNameFromSecTrustedApplication(
	CFAllocatorRef alloc,
	SecTrustedApplicationRef appRef)
{
	CFStringRef result;
	OSStatus status;
	CFDataRef appDataRef;

	result = NULL;

	// get the data for item's application/tool
	status = SecTrustedApplicationCopyData(appRef, &appDataRef);
	if ( status == errSecSuccess ) {
		CFStringRef path;

		// convert it to a CFString potentially containing the path
		path = CFStringCreateWithCString(NULL, (char *)CFDataGetBytePtrVoid(appDataRef), kCFStringEncodingUTF8);
		if ( path != NULL ) {
			// the path has to start with a "/" and cannot contain "://"
			if ( CFStringHasPrefix(path, CFSTR("/")) && (CFStringFind(path, CFSTR("://"), 0).location == kCFNotFound) ) {
				CFRange nameRange, compRg;

				nameRange = CFRangeMake(0, CFStringGetLength(path));

				// remove the trailing slashes (if any)
				while ( (nameRange.length > 0) && (CFStringGetCharacterAtIndex(path, nameRange.length - 1) == '/') ) {
					nameRange.length --;
				}

				if ( nameRange.length > 0 ) {
					// find last slash and adjust nameRange to be everything after it
					if ( CFStringFindWithOptions(path, CFSTR("/"), nameRange, kCFCompareBackwards, &compRg) ) {
						nameRange.length = nameRange.location + nameRange.length - (compRg.location + 1);
						nameRange.location = compRg.location + 1;
					}

					result = CFStringCreateWithSubstring(alloc, path, nameRange);
				}
			}
			CFRelease(path);
		}
		CFRelease(appDataRef);
	}

	return ( result );
}

/* (This function really belongs in SecIdentity.cpp!)
 *
 * Returns the public key item corresponding to the identity, if it exists in
 * the same keychain as the private key. Note that the public key might not
 * exist in the same keychain (e.g. if the identity was imported via PKCS12),
 * in which case it will not be found.
 */
static OSStatus
_SecIdentityCopyPublicKey(
	SecIdentityRef identityRef,
	SecKeyRef *publicKeyRef)
{
	OSStatus status;
	UInt32 count;
	SecKeychainAttribute attr = { kSecKeyLabel, 0, NULL };
	SecKeychainAttributeList attrList = { 1, &attr };
	SecKeychainAttributeList *keyAttrList = NULL;
	SecKeychainAttributeInfo *info = NULL;
	SecKeychainSearchRef search = NULL;
	SecKeychainRef keychain = NULL;
	SecKeychainItemRef privateKey = NULL;
	SecKeychainItemRef publicKey = NULL;

	status = SecIdentityCopyPrivateKey(identityRef, (SecKeyRef *)&privateKey);
	if (status) {
		goto error_exit; // identity must have a private key
	}
	status = SecKeychainItemCopyKeychain(privateKey, &keychain);
	if (status) {
		goto error_exit; // private key must have a keychain, so we can get the attribute info for it
	}
	status = SecKeychainAttributeInfoForItemID(keychain, kSecPrivateKeyItemClass, &info);
	if (status) {
		goto error_exit; // unable to get the attribute info (i.e. database schema) for private keys
	}
	status = SecKeychainItemCopyAttributesAndData(privateKey, info, NULL, &keyAttrList, NULL, NULL);
	if (status) {
		goto error_exit; // unable to get the key label attribute for the private key
	}

	// use the found kSecKeyLabel attribute from the private key in a separate attribute list for searching
	for (count = 0; count < keyAttrList->count; count++) {
		if (keyAttrList->attr[count].tag == kSecKeyLabel) {
			attr.length = keyAttrList->attr[count].length;
			attr.data = keyAttrList->attr[count].data;
			break;
		}
	}
	if (!attr.length || !attr.data) {
		status = errSecNoSuchAttr;
		goto error_exit; // the private key didn't have the hash of the public key in its kSecKeyLabel
	}
	status = SecKeychainSearchCreateFromAttributes(keychain, kSecPublicKeyItemClass, &attrList, &search);
	if (status) {
		goto error_exit; // unable to create the search reference
	}
	status = SecKeychainSearchCopyNext(search, &publicKey);
	if (status) {
		goto error_exit; // unable to find the public key
	}

	if (publicKeyRef)
		*publicKeyRef = (SecKeyRef)publicKey;
	else
		CFRelease(publicKey);

error_exit:
	if (status != errSecSuccess) {
		if (publicKeyRef)
			*publicKeyRef = NULL;
		if (publicKey)
			CFRelease(publicKey);
	}
	if (search)
		CFRelease(search);

	if (keyAttrList)
		SecKeychainItemFreeAttributesAndData(keyAttrList, NULL);

	if (info)
		SecKeychainFreeAttributeInfo(info);

	if (keychain)
		CFRelease(keychain);

	if (privateKey)
		CFRelease(privateKey);

	return status;
}


/*
 * Deletes a keychain item if the current application/tool is the only application/tool
 * with decrypt access to that keychain item. If more than one application/tool
 * has decrypt access to the keychain item, the item is left on the keychain.
 *
 * TBD: If more than one app/tool has access to the keychain item, we should remove
 * the current app/tool's decrypt access. There's no easy way to do that with
 * current keychain APIs without bringing up the security UI.
 */
static OSStatus
_SafeSecKeychainItemDelete(
	SecKeychainItemRef itemRef)
{
	OSStatus status;
	SecAccessRef access = NULL;
	CFArrayRef aclList = NULL;
	SecACLRef acl = NULL;
	CFArrayRef appList = NULL;
	CFStringRef description = NULL;
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
	CFIndex idx, count = 0;
	SecTrustedApplicationRef currentAppRef = NULL;
	CFStringRef itemAppName = NULL, currentAppName = NULL;

	SecItemClass itemClass = (SecItemClass)0;
	status = SecKeychainItemCopyAttributesAndData(itemRef, NULL, &itemClass, NULL, NULL, NULL);
	if (!(itemClass == kSecInternetPasswordItemClass || itemClass == kSecGenericPasswordItemClass)) {
		// only perform the access control safety check on deletion of password credentials;
		// if the item is of some other type, delete it normally.
		return SecKeychainItemDelete(itemRef);
	}

	// skip access control checking for web form passwords: <rdar://10957301>
	// This permits Safari to manage the removal of all web form passwords,
	// regardless of whether they are shared by multiple applications.
	if (itemClass == kSecInternetPasswordItemClass) {
		UInt32 tags[1] = { kSecAuthenticationTypeItemAttr };
		SecKeychainAttributeInfo attrInfo = { 1, tags, NULL };
		SecKeychainAttributeList *attrs = NULL;
		status = SecKeychainItemCopyAttributesAndData(itemRef, &attrInfo, NULL, &attrs, NULL, NULL);
		if (!status && attrs) {
			bool webFormPassword = (attrs->attr[0].length == 4 && (!memcmp(attrs->attr[0].data, "form", 4)));
			SecKeychainItemFreeAttributesAndData(attrs, NULL);
			if (webFormPassword) {
				return SecKeychainItemDelete(itemRef);
			}
		}
	}

	// copy the access of the keychain item
	status = SecKeychainItemCopyAccess(itemRef, &access);
	require_noerr(status, finish);
	require_quiet(access != NULL, finish);

	// copy the decrypt access control lists -- this is what has access to the keychain item
	status = SecAccessCopySelectedACLList(access, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
	require_noerr(status, finish);
	require_quiet(aclList != NULL, finish);

	// get the access control list
	acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
	require_quiet(acl != NULL, finish);

	// copy the application list, description, and CSSM prompt selector for a given access control list entry
	status = SecACLCopySimpleContents(acl, &appList, &description, &promptSelector);
	require_noerr(status, finish);
	require_quiet(appList != NULL, finish);

	// does the calling application/tool have decrypt access to this item?
	count = CFArrayGetCount(appList);
	for ( idx = 0; idx < count; idx++ ) {
		// get SecTrustedApplicationRef for this entry
		SecTrustedApplicationRef itemAppRef = (SecTrustedApplicationRef)CFArrayGetValueAtIndex(appList, idx);
		require_quiet(itemAppRef != NULL, finish);

		// copy the name out
		CFReleaseSafe(itemAppName);
		itemAppName = _AppNameFromSecTrustedApplication(CFGetAllocator(itemRef), itemAppRef);
		if (itemAppName == NULL) {
			/*
			 * If there is no app name, it's probably because it's not an appname
			 * in the ACE but an entitlement/info.plist based rule instead;
			 * just let the caller have it. */
			count = 0;
			goto finish;
		}

		// create SecTrustedApplicationRef for current application/tool
		CFReleaseSafe(currentAppRef);
		status = SecTrustedApplicationCreateFromPath(NULL, &currentAppRef);
		require_noerr(status, finish);
		require_quiet(currentAppRef != NULL, finish);

		// copy the name out
		CFReleaseSafe(currentAppName);
		currentAppName = _AppNameFromSecTrustedApplication(CFGetAllocator(itemRef), currentAppRef);
		require_quiet(currentAppName != NULL, finish);

		// compare the names to see if we own the decrypt access
		// TBD: validation of membership in an application group
		if ( CFStringCompare(currentAppName, itemAppName, 0) == kCFCompareEqualTo ) {
			count = 0;
			goto finish;
		}
	}

finish:

	CFReleaseSafe(currentAppName);
	CFReleaseSafe(itemAppName);
	CFReleaseSafe(currentAppRef);
	CFReleaseSafe(description);
	CFReleaseSafe(appList);
	CFReleaseSafe(aclList);
	CFReleaseSafe(access);

	if ((count == 0) || (status == errSecVerifyFailed)) {
		// no "owners" remain in the ACL list (or unable to get ACL)
		status = SecKeychainItemDelete(itemRef);
	} else {
		// caller is not the "owner" of the item
		status = errSecInvalidOwnerEdit;
	}

	return status;
}

static OSStatus
_ReplaceKeychainItem(
	SecKeychainItemRef itemToUpdate,
	SecKeychainAttributeList *changeAttrList,
	CFDataRef itemData)
{
	OSStatus status;
	UInt32 itemID;
	SecItemClass itemClass;
	SecKeychainAttributeInfo *info = NULL;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttributeList newAttrList = { 0, NULL};
	SecKeychainRef keychain = NULL;
	SecKeychainItemRef newItem = NULL;

	int priority = LOG_DEBUG;
	const char *format = "ReplaceKeychainItem (%d) error %d";

	// get existing item's keychain
	status = SecKeychainItemCopyKeychain(itemToUpdate, &keychain);
	if (status) { secitemlog(priority, format, 1, (int)status); }
	require_noerr(status, replace_failed);

	// get attribute info (i.e. database schema) for the item class
	status = SecKeychainItemCopyAttributesAndData(itemToUpdate, NULL, &itemClass, NULL, NULL, NULL);
	if (status) { secitemlog(priority, format, 2, (int)status); }
	require_noerr(status, replace_failed);

	switch (itemClass)
	{
		case kSecInternetPasswordItemClass:
			itemID = CSSM_DL_DB_RECORD_INTERNET_PASSWORD;
			break;
		case kSecGenericPasswordItemClass:
			itemID = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
			break;
		default:
			itemID = itemClass;
			break;
	}
	status = SecKeychainAttributeInfoForItemID(keychain, itemID, &info);
	if (status) { secitemlog(priority, format, 3, (int)status); }

	// get item's existing attributes (but not data!)
	status = SecKeychainItemCopyAttributesAndData(itemToUpdate, info, &itemClass, &attrList, NULL, NULL);
	if (status) { secitemlog(priority, format, 4, (int)status); }
	require(attrList != NULL, replace_failed);

	// move aside the item by changing a primary attribute
    // (currently only for passwords)
	if (itemClass == kSecInternetPasswordItemClass || itemClass == kSecGenericPasswordItemClass) {
		CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
		CFStringRef uuidStr = (uuid) ? CFUUIDCreateString(kCFAllocatorDefault, uuid) : CFSTR("MOVED");
		CFReleaseSafe(uuid);
		if (uuidStr) {
			CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(uuidStr), kCFStringEncodingUTF8) + 1;
			char* buffer = (char*) malloc(maxLength);
			if (buffer) {
				if (CFStringGetCString(uuidStr, buffer, maxLength, kCFStringEncodingUTF8)) {
					UInt32 length = (UInt32)strlen(buffer);
					SecKeychainAttribute attrs[] = { { kSecAccountItemAttr, length, (char*)buffer }, };
					SecKeychainAttributeList updateAttrList = { sizeof(attrs) / sizeof(attrs[0]), attrs };
					status = SecKeychainItemModifyAttributesAndData(itemToUpdate, &updateAttrList, 0, NULL);
					if (status) { secitemlog(priority, format, 5, (int)status); }
					if (status == errSecVerifyFailed) {
						// still unable to change attrs? delete unconditionally here
						status = SecKeychainItemDelete(itemToUpdate);
						if (status) { secitemlog(priority, format, 6, (int)status); }
					}
				}
				free(buffer);
			}
			CFReleaseSafe(uuidStr);
		}
	}
	require_noerr(status, replace_failed);

	// make attribute list for new item (the data is still owned by attrList)
	newAttrList.count = attrList->count;
	newAttrList.attr = (SecKeychainAttribute *) malloc(sizeof(SecKeychainAttribute) * attrList->count);
	int i, newCount;
	for (i=0, newCount=0; i < attrList->count; i++) {
		if (attrList->attr[i].length > 0) {
			newAttrList.attr[newCount++] = attrList->attr[i];
		#if 0
			// debugging code to log item attributes
			SecKeychainAttrType tag = attrList->attr[i].tag;
			SecKeychainAttrType htag=(SecKeychainAttrType)OSSwapConstInt32(tag);
			char tmp[sizeof(SecKeychainAttrType) + 1];
			char tmpdata[attrList->attr[i].length + 1];
			memcpy(tmp, &htag, sizeof(SecKeychainAttrType));
			tmp[sizeof(SecKeychainAttrType)]=0;
			memcpy(tmpdata, attrList->attr[i].data, attrList->attr[i].length);
			tmpdata[attrList->attr[i].length]=0;
			secitemlog(priority, "item attr '%s' = %d bytes: \"%s\"",
				tmp, (int)attrList->attr[i].length, tmpdata);
		#endif
		}
	}
	newAttrList.count = newCount;

	// create new item in the same keychain
	status = SecKeychainItemCreateFromContent(itemClass, &newAttrList,
		(UInt32)((itemData) ? CFDataGetLength(itemData) : 0),
		(const void *)((itemData) ? CFDataGetBytePtr(itemData) : NULL),
		keychain, NULL, &newItem);
	if (status) { secitemlog(priority, format, 7, (int)status); }
	require_noerr(status, replace_failed);

	// delete the old item unconditionally once new item exists
	status = SecKeychainItemDelete(itemToUpdate);

	// update the new item with changed attributes, if any
	status = (changeAttrList) ? SecKeychainItemModifyContent(newItem, changeAttrList, 0, NULL) : errSecSuccess;
	if (status) { secitemlog(priority, format, 8, (int)status); }
	if (status == errSecSuccess) {
		// say the item already exists, because it does now. <rdar://19063674>
		status = errSecDuplicateItem;
	}

replace_failed:
	if (newAttrList.attr) {
		free(newAttrList.attr);
	}
    if (attrList) {
        SecKeychainItemFreeAttributesAndData(attrList, NULL);
    }
    if (info) {
        SecKeychainFreeAttributeInfo(info);
    }
	CFReleaseSafe(newItem);
	CFReleaseSafe(keychain);

	return status;
}

static OSStatus
_UpdateKeychainItem(CFTypeRef item, CFDictionaryRef changedAttributes)
{
	// This function updates a single keychain item, which may be specified as
	// a reference, persistent reference or attribute dictionary, with the
	// attributes provided.

	OSStatus status = errSecSuccess;
	if (!item) {
		return errSecParam;
	}

	SecItemClass itemClass = (SecItemClass) 0;
	SecAccessRef access = NULL;
	SecKeychainAttributeList *changeAttrList = NULL;
	SecKeychainItemRef itemToUpdate = NULL;
	CFDataRef theData = NULL;
	CFTypeID itemType = CFGetTypeID(item);

	// validate input item (must be convertible to a SecKeychainItemRef)
	if (SecKeychainItemGetTypeID() == itemType ||
		SecCertificateGetTypeID() == itemType ||
		SecKeyGetTypeID() == itemType) {
		// item is already a reference, retain it
		itemToUpdate = (SecKeychainItemRef) CFRetain(item);
	}
	else if (CFDataGetTypeID() == itemType) {
		// item is a persistent reference, must convert it
		status = SecKeychainItemCopyFromPersistentReference((CFDataRef)item, &itemToUpdate);
	}
	else if (CFDictionaryGetTypeID() == itemType) {
		// item is a dictionary
		CFTypeRef value = NULL;
		if (CFDictionaryGetValueIfPresent((CFDictionaryRef)item, kSecValueRef, &value)) {
			// kSecValueRef value is a SecKeychainItemRef, retain it
			itemToUpdate = (SecKeychainItemRef) CFRetain(value);
		}
		else if (CFDictionaryGetValueIfPresent((CFDictionaryRef)item, kSecValuePersistentRef, &value)) {
			// kSecValuePersistentRef value is a persistent reference, must convert it
			status = SecKeychainItemCopyFromPersistentReference((CFDataRef)value, &itemToUpdate);
		}
	}
	else if (SecIdentityGetTypeID() == itemType) {
		// item is a certificate + private key; since we can't really change the
		// certificate's attributes, assume we want to update the private key
		status = SecIdentityCopyPrivateKey((SecIdentityRef)item, (SecKeyRef*)&itemToUpdate);
	}
	require_action(itemToUpdate != NULL, update_failed, status = errSecInvalidItemRef);
	require_noerr(status, update_failed);

	status = SecKeychainItemCopyContent(itemToUpdate, &itemClass, NULL, NULL, NULL);
	require_noerr(status, update_failed);

	// build changeAttrList from changedAttributes dictionary
	switch (itemClass)
	{
		case kSecInternetPasswordItemClass:
		{
			status = _CreateSecKeychainInternetPasswordAttributeListFromDictionary(changedAttributes, &changeAttrList);
			require_noerr(status, update_failed);
		}
		break;

		case kSecGenericPasswordItemClass:
		{
			status = _CreateSecKeychainGenericPasswordAttributeListFromDictionary(changedAttributes, &changeAttrList);
			require_noerr(status, update_failed);
		}
		break;

		case kSecCertificateItemClass:
		{
			status = _CreateSecKeychainCertificateAttributeListFromDictionary(changedAttributes, &changeAttrList);
			require_noerr(status, update_failed);
		}
		break;

		case kSecPublicKeyItemClass:
		case kSecPrivateKeyItemClass:
		case kSecSymmetricKeyItemClass:
		{
			status = _CreateSecKeychainKeyAttributeListFromDictionary(changedAttributes, &changeAttrList);
			require_noerr(status, update_failed);
		}
		break;
		case kSecAppleSharePasswordItemClass:
		{
			// do nothing (legacy behavior).
		}
		break;

	}

	// get the password
	// (if the caller is not updating the password, this value will be NULL)
	theData = (CFDataRef)CFDictionaryGetValue(changedAttributes, kSecValueData);
	if (theData != NULL) {
		require_action(CFDataGetTypeID() == CFGetTypeID(theData), update_failed, status = errSecParam);
	}
	// update item
	status = SecKeychainItemModifyContent(itemToUpdate,
				(changeAttrList->count == 0) ? NULL : changeAttrList,
				(theData != NULL) ? (UInt32)CFDataGetLength(theData) : 0,
				(theData != NULL) ? CFDataGetBytePtrVoid(theData) : NULL);
	require_noerr(status, update_failed);

	// one more thing... update access?
	if (CFDictionaryGetValueIfPresent(changedAttributes, kSecAttrAccess, (const void **)&access)) {
		status = SecKeychainItemSetAccess(itemToUpdate, access);
	}

update_failed:
	if (status == errSecVerifyFailed &&
		(itemClass == kSecInternetPasswordItemClass || itemClass == kSecGenericPasswordItemClass)) {
		// if we got a cryptographic failure updating a password item, it needs to be replaced
		status = _ReplaceKeychainItem(itemToUpdate,
					(changeAttrList->count == 0) ? NULL : changeAttrList,
					theData);
	}
	if (itemToUpdate)
		CFRelease(itemToUpdate);
	_FreeAttrList(changeAttrList);
	return status;
}

static OSStatus
_DeleteKeychainItem(CFTypeRef item)
{
	// This function deletes a single keychain item, which may be specified as
	// a reference, persistent reference or attribute dictionary. It will not
	// delete non-keychain items or aggregate items (such as a SecIdentityRef);
	// it is assumed that the caller will pass identity components separately.

	OSStatus status = errSecSuccess;
	if (!item) {
		return errSecParam;
	}

	SecKeychainItemRef itemToDelete = NULL;
	CFTypeID itemType = CFGetTypeID(item);
	if (SecKeychainItemGetTypeID() == itemType ||
		SecCertificateGetTypeID() == itemType ||
		SecKeyGetTypeID() == itemType) {
		// item is already a reference, retain it
		itemToDelete = (SecKeychainItemRef) CFRetain(item);
	}
	else if (CFDataGetTypeID() == itemType) {
		// item is a persistent reference, must convert it
		status = SecKeychainItemCopyFromPersistentReference((CFDataRef)item, &itemToDelete);
	}
	else if (CFDictionaryGetTypeID() == itemType) {
		// item is a dictionary
		CFTypeRef value = NULL;
		if (CFDictionaryGetValueIfPresent((CFDictionaryRef)item, kSecValueRef, &value)) {
			// kSecValueRef value is a SecKeychainItemRef, retain it
			itemToDelete = (SecKeychainItemRef) CFRetain(value);
		}
		else if (CFDictionaryGetValueIfPresent((CFDictionaryRef)item, kSecValuePersistentRef, &value)) {
			// kSecValuePersistentRef value is a persistent reference, must convert it
			status = SecKeychainItemCopyFromPersistentReference((CFDataRef)value, &itemToDelete);
		}
	}

	if (itemToDelete) {
		if (!status) {
			status = _SafeSecKeychainItemDelete(itemToDelete);
		}
		CFRelease(itemToDelete);
	}

	return status;
}

static OSStatus
_DeleteIdentity(SecIdentityRef identity)
{
	OSStatus status, result = errSecSuccess;
	SecKeyRef privateKey = NULL;
	SecCertificateRef certificate = NULL;

	status = SecIdentityCopyPrivateKey(identity, &privateKey);
	if (!status) {
		SecKeyRef publicKey = NULL;
		status = _SecIdentityCopyPublicKey(identity, &publicKey);
		if (!status) {
			status = _DeleteKeychainItem(publicKey);
			CFRelease(publicKey);
		}
		status = _DeleteKeychainItem(privateKey);
	}

	if (privateKey) CFRelease(privateKey);
	if (status) result = status;

	status = SecIdentityCopyCertificate(identity, &certificate);
	if (!status) {
		status = _DeleteKeychainItem(certificate);
	}

	if (certificate) CFRelease(certificate);
	if (status) result = status;

	return result;
}

static OSStatus
_UpdateAggregateStatus(OSStatus newStatus, OSStatus curStatus, OSStatus baseStatus)
{
	// This function is used when atomically processing multiple items,
	// where an overall error result must be returned for the entire operation.
	// When newStatus is something other than errSecSuccess, we want to keep the "most
	// interesting" status (which usually will be newStatus, unless curStatus is
	// already set; in that case, newStatus can trump curStatus only by being
	// something different than baseStatus.)

	OSStatus result = curStatus;

	if (newStatus != errSecSuccess) {
		result = newStatus;
		if (curStatus != errSecSuccess) {
			result = (newStatus != baseStatus) ? newStatus : curStatus;
		}
	}
	return result;
}

static void
_AddDictValueToOtherDict(const void *key, const void *value, void *context)
{
	// CFDictionaryApplierFunction
	// This function just takes the given key/value pair,
	// and adds it to another dictionary supplied in the context argument.

	CFMutableDictionaryRef dict = *((CFMutableDictionaryRef*) context);
	if (key && value) {
		CFDictionaryAddValue(dict, key, value);
	}
}

static CFStringCompareFlags
_StringCompareFlagsFromQuery(CFDictionaryRef query)
{
	CFTypeRef value;
	CFStringCompareFlags flags = 0;
	if (!query) return flags;

	if (CFDictionaryGetValueIfPresent(query, kSecMatchSubjectStartsWith, (const void **)&value) ||
		CFDictionaryGetValueIfPresent(query, kSecMatchSubjectEndsWith, (const void **)&value))
		flags |= kCFCompareAnchored;

	if (CFDictionaryGetValueIfPresent(query, kSecMatchSubjectEndsWith, (const void **)&value))
		flags |= kCFCompareBackwards;

	if (CFDictionaryGetValueIfPresent(query, kSecMatchCaseInsensitive, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		flags |= kCFCompareCaseInsensitive;

	if (CFDictionaryGetValueIfPresent(query, kSecMatchDiacriticInsensitive, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		flags |= kCFCompareDiacriticInsensitive;

	if (CFDictionaryGetValueIfPresent(query, kSecMatchWidthInsensitive, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		flags |= kCFCompareWidthInsensitive;

	return flags;
}

static uint32
_CssmKeyUsageFromQuery(CFDictionaryRef query)
{
	CFTypeRef value;
	uint32 keyUsage = 0;
	if (!query) return keyUsage;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanEncrypt, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_ENCRYPT;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanDecrypt, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_DECRYPT;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanSign, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_SIGN;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanVerify, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_VERIFY;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanWrap, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_WRAP;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanUnwrap, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_UNWRAP;

	if (CFDictionaryGetValueIfPresent(query, kSecAttrCanDerive, (const void **)&value) && CFEqual(kCFBooleanTrue, value))
		keyUsage |= CSSM_KEYUSE_DERIVE;

	return keyUsage;
}

static SecItemClass
_ConvertItemClass(const void* item, const void* keyClass, Boolean *isIdentity)
{
	SecItemClass itemClass = (SecItemClass) 0;
	if (isIdentity) *isIdentity = false;

	if (CFEqual(item, kSecClassGenericPassword)) {
		itemClass = kSecGenericPasswordItemClass;
	}
	else if (CFEqual(item, kSecClassInternetPassword)) {
		itemClass = kSecInternetPasswordItemClass;
	}
	else if (CFEqual(item, kSecClassCertificate)) {
		itemClass = kSecCertificateItemClass;
	}
	else if (CFEqual(item, kSecClassIdentity)) {
		// will perform a certificate lookup
		itemClass = kSecCertificateItemClass;
		if (isIdentity) *isIdentity = true;
	}
	else if (CFEqual(item, kSecClassKey)) {
		// examine second parameter to determine type of key
		if (!keyClass || CFEqual(keyClass, kSecAttrKeyClassSymmetric)) {
			itemClass = kSecSymmetricKeyItemClass;
		}
		else if (keyClass && CFEqual(keyClass, kSecAttrKeyClassPublic)) {
			itemClass = kSecPublicKeyItemClass;
		}
		else if (keyClass && CFEqual(keyClass, kSecAttrKeyClassPrivate)) {
			itemClass = kSecPrivateKeyItemClass;
		}
	}

	return itemClass;
}

static SecItemClass
_ItemClassFromItemList(CFArrayRef itemList)
{
	// Given a list of items (standard or persistent references),
	// determine whether they all have the same item class. Returns
	// the item class, or 0 if multiple classes in list.
	SecItemClass result = (SecItemClass) 0;
	CFIndex index, count = (itemList) ? CFArrayGetCount(itemList) : 0;
	for (index=0; index < count; index++) {
		CFTypeRef item = (CFTypeRef) CFArrayGetValueAtIndex(itemList, index);
		if (item) {
			SecKeychainItemRef itemRef = NULL;
			OSStatus status;
			if (CFGetTypeID(item) == CFDataGetTypeID()) {
				// persistent reference, resolve first
				status = SecKeychainItemCopyFromPersistentReference((CFDataRef)item, &itemRef);
			}
			else {
				itemRef = (SecKeychainItemRef) CFRetain(item);
			}
			if (itemRef) {
				SecItemClass itemClass = (SecItemClass) 0;
				CFTypeID itemTypeID = CFGetTypeID(itemRef);
				if (itemTypeID == SecIdentityGetTypeID() || itemTypeID == SecCertificateGetTypeID()) {
					// Identities and certificates have the same underlying item class
					itemClass = kSecCertificateItemClass;
				}
				else if (itemTypeID == SecKeychainItemGetTypeID()) {
					// Reference to item in a keychain
					status = SecKeychainItemCopyAttributesAndData(itemRef, NULL, &itemClass, NULL, NULL, NULL);
				}
				else if (itemTypeID == SecKeyGetTypeID()) {
					// SecKey that isn't stored in a keychain
					// %%% will need to change this code when SecKey is no longer CSSM-based %%%
					const CSSM_KEY *cssmKey;
					status = SecKeyGetCSSMKey((SecKeyRef)itemRef, &cssmKey);
					if (status == errSecSuccess) {
						if (cssmKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PUBLIC_KEY)
							itemClass = kSecPublicKeyItemClass;
						else if (cssmKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY)
							itemClass = kSecPrivateKeyItemClass;
						else
							itemClass = kSecSymmetricKeyItemClass;
					}
				}
				CFRelease(itemRef);
				if (itemClass != 0) {
					if (result != 0 && result != itemClass) {
						return (SecItemClass) 0; // different item classes in list; bail out
					}
					result = itemClass;
				}
			}
		}
	}
	return result;
}

// SecItemParams contains a validated set of input parameters, as well as a
// search reference and attribute list built from those parameters. It is
// designed to be allocated with _CreateSecItemParamsFromDictionary, and
// freed with _FreeSecItemParams.

struct SecItemParams {
	CFDictionaryRef query;				// caller-supplied query
	int numResultTypes;					// number of result types requested
	int maxMatches;						// max number of matches to return
	uint32 keyUsage;					// key usage(s) requested
	Boolean returningAttributes;		// true if returning attributes dictionary
	Boolean returningData;				// true if returning item's data
	Boolean returningRef;				// true if returning item reference
	Boolean returningPersistentRef;		// true if returing a persistent reference
	Boolean returnAllMatches;			// true if we should return all matches
	Boolean returnIdentity;				// true if we are returning a SecIdentityRef
	Boolean trustedOnly;				// true if we only return trusted certs
	Boolean	issuerAndSNToMatch;			// true if both issuer and SN were provided
	SecItemClass itemClass;				// item class for this query
	SecPolicyRef policy;				// value for kSecMatchPolicy (may be NULL)
	SecKeychainRef keychain;			// value for kSecUseKeychain (may be NULL)
	CFArrayRef useItems;				// value for kSecUseItemList (may be NULL)
	CFArrayRef itemList;				// value for kSecMatchItemList (may be NULL)
	CFTypeRef searchList;				// value for kSecMatchSearchList (may be NULL)
	CFTypeRef matchLimit;				// value for kSecMatchLimit (may be NULL)
	CFTypeRef emailAddrToMatch;			// value for kSecMatchEmailAddressIfPresent (may be NULL)
	CFTypeRef validOnDate;				// value for kSecMatchValidOnDate (may be NULL)
	CFTypeRef keyClass;					// value for kSecAttrKeyClass (may be NULL)
	CFTypeRef service;					// value for kSecAttrService (may be NULL)
	CFTypeRef issuer;					// value for kSecAttrIssuer (may be NULL)
	CFTypeRef serialNumber;				// value for kSecAttrSerialNumber (may be NULL)
	CFTypeRef search;					// search reference for this query (SecKeychainSearchRef or SecIdentitySearchRef)
	CFTypeRef assumedKeyClass;			// if no kSecAttrKeyClass provided, holds the current class we're searching for
	CFIndex itemListIndex;				// if no search reference but we have itemList, holds index of next item to return
	SecKeychainAttributeList *attrList;	// attribute list for this query
	SecAccessRef access;				// access reference (for SecItemAdd only, not used to find items)
	CFDataRef itemData;					// item data (for SecItemAdd only, not used to find items)
	CFTypeRef itemRef;					// item reference (to find, add, update or delete, depending on context)
	SecIdentityRef identityRef;			// identity reference (input as kSecValueRef)
	CFDataRef itemPersistentRef;		// item persistent reference (to find, add, update or delete, depending on context)
	Boolean isPCSItem;					// true if this query is for a Protected Cloud Storage item
};

static OSStatus
_ValidateDictionaryEntry(CFDictionaryRef dict, CFTypeRef key, const void **value, CFTypeID expectedTypeID, CFTypeID altTypeID)
{
	if (!dict || !key || !value || !expectedTypeID)
		return errSecParam;

	if (!CFDictionaryGetValueIfPresent(dict, key, value)) {
		// value was not provided for this key (not an error!)
		*value = NULL;
	}
	else if (!(*value)) {
		// provided value is NULL (also not an error!)
		return errSecSuccess;
	}
	else {
		CFTypeID actualTypeID = CFGetTypeID(*value);
		if (!((expectedTypeID == actualTypeID) || (altTypeID && altTypeID == actualTypeID))) {
			// provided value does not have the expected (or alternate) CF type ID
			if ((expectedTypeID == SecKeychainItemGetTypeID()) &&
				(actualTypeID == SecKeyGetTypeID() || actualTypeID == SecCertificateGetTypeID())) {
				// provided value is a "floating" reference which is not yet in a keychain
				CFRetain(*value);
				return errSecSuccess;
			}
			return errSecItemInvalidValue;
		}
		else {
			// provided value is OK; retain it
			CFRetain(*value);
		}
	}
	return errSecSuccess;
}

static void
_EnsureUserDefaultKeychainIsSearched(SecItemParams *itemParams)
{
	OSStatus status;
	CFArrayRef tmpList = (CFArrayRef) itemParams->searchList;
	if (tmpList) {
		// search list exists; make it mutable
		itemParams->searchList = (CFArrayRef) CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, tmpList);
		CFRelease(tmpList);
	} else {
		// no search list; start with default list
		status = SecKeychainCopySearchList(&tmpList);
		if (!status && tmpList) {
			itemParams->searchList = (CFArrayRef) CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, tmpList);
			CFRelease(tmpList);
		}
		else {
			itemParams->searchList = (CFArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}
	}

	SecKeychainRef userKeychain = NULL;
	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainUser, &userKeychain);
	if (!status && userKeychain) {
		if (!CFArrayContainsValue((CFArrayRef)itemParams->searchList,
			CFRangeMake(0, CFArrayGetCount((CFArrayRef)itemParams->searchList)), userKeychain)) {
			// user's default keychain isn't currently in the search list, so append it
			CFArrayAppendValue((CFMutableArrayRef)itemParams->searchList, userKeychain);
		}
		CFRelease(userKeychain);
	}
}

static void
_EnsureUserDefaultKeychainIsTargeted(SecItemParams *itemParams)
{
	if (itemParams->keychain) {
		return; // keychain is already explicitly specified, assume it's correct
	}
	SecKeychainRef userKeychain = NULL;
	OSStatus status = SecKeychainCopyDomainDefault(kSecPreferencesDomainUser, &userKeychain);
	if (!status && userKeychain) {
		itemParams->keychain = userKeychain;
	}
}

static void
_FreeSecItemParams(SecItemParams *itemParams)
{
	if (!itemParams)
		return;

	if (itemParams->query) CFRelease(itemParams->query);
	if (itemParams->policy) CFRelease(itemParams->policy);
	if (itemParams->keychain) CFRelease(itemParams->keychain);
	if (itemParams->useItems) CFRelease(itemParams->useItems);
	if (itemParams->itemList) CFRelease(itemParams->itemList);
	if (itemParams->searchList) CFRelease(itemParams->searchList);
	if (itemParams->matchLimit) CFRelease(itemParams->matchLimit);
	if (itemParams->emailAddrToMatch) CFRelease(itemParams->emailAddrToMatch);
	if (itemParams->validOnDate) CFRelease(itemParams->validOnDate);
	if (itemParams->keyClass) CFRelease(itemParams->keyClass);
	if (itemParams->service) CFRelease(itemParams->service);
	if (itemParams->issuer) CFRelease(itemParams->issuer);
	if (itemParams->serialNumber) CFRelease(itemParams->serialNumber);
	if (itemParams->search) CFRelease(itemParams->search);
	if (itemParams->access) CFRelease(itemParams->access);
	if (itemParams->itemData) CFRelease(itemParams->itemData);
	if (itemParams->itemRef) CFRelease(itemParams->itemRef);
	if (itemParams->identityRef) CFRelease(itemParams->identityRef);
	if (itemParams->itemPersistentRef) CFRelease(itemParams->itemPersistentRef);

	_FreeAttrList(itemParams->attrList);

	free(itemParams);
}

static SecItemParams*
_CreateSecItemParamsFromDictionary(CFDictionaryRef dict, OSStatus *error)
{
	OSStatus status;
	CFTypeRef value = NULL;
	SecItemParams *itemParams = (SecItemParams *)calloc(1, sizeof(struct SecItemParams));

	require_action(itemParams != NULL, error_exit, status = errSecAllocate);
	require_action(dict && (CFDictionaryGetTypeID() == CFGetTypeID(dict)), error_exit, status = errSecParam);

	itemParams->query = (CFDictionaryRef) CFRetain(dict);

	// validate input search parameters
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchPolicy, (const void **)&itemParams->policy, SecPolicyGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchSearchList, (const void **)&itemParams->searchList, CFArrayGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchItemList, (const void **)&itemParams->itemList, CFArrayGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchEmailAddressIfPresent, (const void **)&itemParams->emailAddrToMatch, CFStringGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchValidOnDate, (const void **)&itemParams->validOnDate, CFDateGetTypeID(), CFNullGetTypeID()), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecMatchLimit, (const void **)&itemParams->matchLimit, CFStringGetTypeID(), CFNumberGetTypeID()), error_exit);

	require_noerr(status = _ValidateDictionaryEntry(dict, kSecUseItemList, (const void **)&itemParams->useItems, CFArrayGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecUseKeychain, (const void **)&itemParams->keychain, SecKeychainGetTypeID(), NULL), error_exit);

	// validate a subset of input attributes (used to create an appropriate search reference)
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecAttrIssuer, (const void **)&itemParams->issuer, CFDataGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecAttrSerialNumber, (const void **)&itemParams->serialNumber, CFDataGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecAttrService, (const void **)&itemParams->service, CFStringGetTypeID(), NULL), error_exit);
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecAttrKeyClass, (const void **)&itemParams->keyClass, CFStringGetTypeID(), NULL), error_exit);

	if (itemParams->service && CFStringHasPrefix((CFStringRef)itemParams->service, CFSTR("ProtectedCloudStorage"))) {
		itemParams->isPCSItem = true;
		if (!SecItemSynchronizable(dict)) {
			_EnsureUserDefaultKeychainIsSearched(itemParams); // for SecItemCopyMatching, SecItemUpdate, SecItemDelete
			_EnsureUserDefaultKeychainIsTargeted(itemParams); // for SecItemAdd
		}
	}

	// validate the payload (password, key or certificate data), used for SecItemAdd but not for finding items
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecValueData, (const void **)&itemParams->itemData, CFDataGetTypeID(), CFStringGetTypeID()), error_exit);
	if (itemParams->itemData && CFGetTypeID(itemParams->itemData) == CFStringGetTypeID()) {
		/* If we got a string, convert it into a data object */
		CFStringRef string = (CFStringRef)itemParams->itemData;
		CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8) + 1;
		CFMutableDataRef data = CFDataCreateMutable(NULL, maxLength);
		require_action(data, error_exit, status = errSecAllocate);

		CFDataSetLength(data, maxLength);

		if (!CFStringGetCString(string, (char *)CFDataGetMutableBytePtr(data), maxLength, kCFStringEncodingUTF8)) {
			CFRelease(data);
			status = errSecAllocate;
			goto error_exit;
		}

		CFDataSetLength(data, strlen((const char *)CFDataGetBytePtr(data))); /* dont include NUL in string */
		itemParams->itemData = data;
		CFRelease(string);
	}

	// validate item references
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecValueRef, (const void **)&itemParams->itemRef, SecKeychainItemGetTypeID(), SecIdentityGetTypeID()), error_exit);
	if (itemParams->itemRef && (CFGetTypeID(itemParams->itemRef) == SecIdentityGetTypeID())) {
		itemParams->identityRef = (SecIdentityRef)itemParams->itemRef;
		itemParams->itemRef = NULL;
		SecIdentityCopyCertificate(itemParams->identityRef, (SecCertificateRef *)&itemParams->itemRef);
	}
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecValuePersistentRef, (const void **)&itemParams->itemPersistentRef, CFDataGetTypeID(), NULL), error_exit);
	if (itemParams->itemRef || itemParams->itemPersistentRef) {
		// Caller is trying to add or find an item by reference.
		// The supported method for doing that is to provide a kSecUseItemList array
		// for SecItemAdd, or a kSecMatchItemList array for SecItemCopyMatching et al,
		// so add the item reference to those arrays here.
		if (itemParams->useItems) {
			CFArrayRef tmpItems = itemParams->useItems;
			itemParams->useItems = (CFArrayRef) CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, tmpItems);
			CFRelease(tmpItems);
		} else {
			itemParams->useItems = (CFArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}
		if (itemParams->itemRef) CFArrayAppendValue((CFMutableArrayRef)itemParams->useItems, itemParams->itemRef);
		if (itemParams->itemPersistentRef) CFArrayAppendValue((CFMutableArrayRef)itemParams->useItems, itemParams->itemPersistentRef);

		if (itemParams->itemList) {
			CFArrayRef tmpItems = itemParams->itemList;
			itemParams->itemList = (CFArrayRef) CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, tmpItems);
			CFRelease(tmpItems);
		} else {
			itemParams->itemList = (CFArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}
		if (itemParams->itemRef) CFArrayAppendValue((CFMutableArrayRef)itemParams->itemList, itemParams->itemRef);
		if (itemParams->itemPersistentRef) CFArrayAppendValue((CFMutableArrayRef)itemParams->itemList, itemParams->itemPersistentRef);
	}

	// must have an explicit item class, unless one of the following is true:
	//   - we have an item list to add or search (kSecUseItemList)
	//   - we have an item reference or persistent reference for the thing we want to look up
	// Note that both of these cases will set itemParams->useItems.
	// If we have an item list to match (kSecMatchItemList), that still requires an item class,
	// so we can perform a search and see if the results match items in the list.
	//
	if (!CFDictionaryGetValueIfPresent(dict, kSecClass, (const void**) &value) && !itemParams->useItems) {
		require_action(false, error_exit, status = errSecItemClassMissing);
	}
	else if (value) {
		itemParams->itemClass = _ConvertItemClass(value, itemParams->keyClass, &itemParams->returnIdentity);
		if (itemParams->itemClass == kSecSymmetricKeyItemClass && !itemParams->keyClass) {
            // no key class specified, so start with symmetric key class; will search the others later in UpdateKeychainSearchAndCopyNext
            itemParams->itemClass = kSecSymmetricKeyItemClass;
            itemParams->assumedKeyClass = kSecAttrKeyClassPublic;
		}
		require_action(!(itemParams->itemClass == 0 && !itemParams->useItems), error_exit, status = errSecItemClassMissing);
	}

	itemParams->keyUsage = _CssmKeyUsageFromQuery(dict);
	itemParams->trustedOnly = CFDictionaryGetValueIfPresent(dict, kSecMatchTrustedOnly, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	itemParams->issuerAndSNToMatch = (itemParams->issuer != NULL && itemParams->serialNumber != NULL);

	// other input attributes, used for SecItemAdd but not for finding items
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecAttrAccess, (const void **)&itemParams->access, SecAccessGetTypeID(), NULL), error_exit);
	if (itemParams->access == NULL) {
		// check for the old definition of kSecAttrAccess from SecItem-shim (see <rdar://7987447>)
		require_noerr(status = _ValidateDictionaryEntry(dict, CFSTR("kSecAttrAccess"), (const void **)&itemParams->access, SecAccessGetTypeID(), NULL), error_exit);
	}

	// determine how to return the result
	itemParams->numResultTypes = 0;
	itemParams->returningRef = CFDictionaryGetValueIfPresent(dict, kSecReturnRef, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	if (itemParams->returningRef) ++itemParams->numResultTypes;
	itemParams->returningPersistentRef = CFDictionaryGetValueIfPresent(dict, kSecReturnPersistentRef, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	if (itemParams->returningPersistentRef) ++itemParams->numResultTypes;
	itemParams->returningAttributes = CFDictionaryGetValueIfPresent(dict, kSecReturnAttributes, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	if (itemParams->returningAttributes) ++itemParams->numResultTypes;
	itemParams->returningData = CFDictionaryGetValueIfPresent(dict, kSecReturnData, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	if (itemParams->returningData) ++itemParams->numResultTypes;

	// default is kSecReturnRef if no result types were specified
	if (!itemParams->numResultTypes) {
		itemParams->returningRef = TRUE;
		itemParams->numResultTypes = 1;
	}

	// determine if one, some or all matches should be returned (default is kSecMatchLimitOne)
	itemParams->maxMatches = 1;
	itemParams->returnAllMatches = FALSE;
	if (itemParams->matchLimit) {
		if (CFStringGetTypeID() == CFGetTypeID(itemParams->matchLimit)) {
			itemParams->returnAllMatches = CFEqual(kSecMatchLimitAll, itemParams->matchLimit);
		}
		else if (CFNumberGetTypeID() == CFGetTypeID(itemParams->matchLimit)) {
			CFNumberGetValue((CFNumberRef)itemParams->matchLimit, kCFNumberIntType, &itemParams->maxMatches);
			require_action(!(itemParams->maxMatches < 0), error_exit, status = errSecMatchLimitUnsupported);
		}
	}
	if (itemParams->returnAllMatches) {
		itemParams->maxMatches = INT32_MAX;
		// if we're returning all matches, then we don't support getting passwords as data (which could require authentication for each)
		if ((itemParams->itemClass==kSecInternetPasswordItemClass || itemParams->itemClass==kSecGenericPasswordItemClass) && itemParams->returningData)
			status = errSecReturnDataUnsupported;
		require_noerr(status, error_exit);
	}

	// if we already have an item list (to add or find items in), we don't need an item class, attribute list or a search reference
	if (itemParams->useItems) {
		if (itemParams->itemClass == 0) {
			itemParams->itemClass = _ItemClassFromItemList(itemParams->useItems);
		}
		status = errSecSuccess;
		goto error_exit; // all done here
	}

	// build a SecKeychainAttributeList from the query dictionary for the specified item class
	require_noerr(status = _CreateSecKeychainAttributeListFromDictionary(dict, itemParams->itemClass, &itemParams->attrList), error_exit);
	
    // if policy is a SMIME policy, copy email address in policy into emailAddrToMatch parameter
    if(itemParams->policy) {
        CFDictionaryRef policyDict = SecPolicyCopyProperties(itemParams->policy);
        CFStringRef oidStr = (CFStringRef) CFDictionaryGetValue(policyDict, kSecPolicyOid);
        if(oidStr && CFStringCompare(kSecPolicyAppleSMIME,oidStr,0) == 0) {
            require_noerr(status = _ValidateDictionaryEntry(policyDict, kSecPolicyName, (const void **)&itemParams->emailAddrToMatch, CFStringGetTypeID(), NULL), error_exit);
        }
        CFRelease(policyDict);
    }

	// create a search reference (either a SecKeychainSearchRef or a SecIdentitySearchRef)
	if ((itemParams->itemClass == kSecCertificateItemClass) && itemParams->emailAddrToMatch) {
		// searching for certificates by email address
		char *nameBuf = (char*)malloc(MAXPATHLEN);
		if (!nameBuf) {
			status = errSecAllocate;
		}
		else if (CFStringGetCString((CFStringRef)itemParams->emailAddrToMatch, nameBuf, (CFIndex)MAXPATHLEN-1, kCFStringEncodingUTF8)) {
			status = SecKeychainSearchCreateForCertificateByEmail(itemParams->searchList, (const char *)nameBuf, (SecKeychainSearchRef*)&itemParams->search);
		}
		else {
			status = errSecItemInvalidValue;
		}
		if (nameBuf) free(nameBuf);
	}
	else if ((itemParams->itemClass == kSecCertificateItemClass) && itemParams->issuerAndSNToMatch) {
		// searching for certificates by issuer and serial number
		status = SecKeychainSearchCreateForCertificateByIssuerAndSN_CF(itemParams->searchList,
				(CFDataRef)itemParams->issuer,
				(CFDataRef)itemParams->serialNumber,
				(SecKeychainSearchRef*)&itemParams->search);
	}
	else if (itemParams->returnIdentity && itemParams->policy) {
		// searching for identities by policy
		status = SecIdentitySearchCreateWithPolicy(itemParams->policy,
				(CFStringRef)itemParams->service,
				itemParams->keyUsage,
				itemParams->searchList,
				itemParams->trustedOnly,
				(SecIdentitySearchRef*)&itemParams->search);
	}
	else if (itemParams->returnIdentity) {
		// searching for identities
		status = SecIdentitySearchCreate(itemParams->searchList,
				itemParams->keyUsage,
				(SecIdentitySearchRef*)&itemParams->search);
	}
	else {
		// normal keychain item search
		status = SecKeychainSearchCreateFromAttributes(itemParams->searchList,
				itemParams->itemClass,
				(itemParams->attrList->count == 0) ? NULL : itemParams->attrList,
				(SecKeychainSearchRef*)&itemParams->search);
	}

error_exit:
	if (status) {
		_FreeSecItemParams(itemParams);
		itemParams = NULL;
	}
	if (error) {
		*error = status;
	}
	return itemParams;
}


static OSStatus
_ImportKey(
	SecKeyRef keyRef,
	SecKeychainRef keychainRef,
	SecAccessRef accessRef,
	SecKeychainAttributeList *attrList,
	SecKeychainItemRef *outItemRef)
{
    BEGIN_SECAPI

		// We must specify the access, since a free-floating key won't have one yet by default
		SecPointer<Access> access;
		if (accessRef) {
			access = Access::required(accessRef);
		}
		else {
			CFStringRef descriptor = NULL;
			if (attrList) {
				for (UInt32 index=0; index < attrList->count; index++) {
					SecKeychainAttribute attr = attrList->attr[index];
					if (attr.tag == kSecKeyPrintName) {
						descriptor = CFStringCreateWithBytes(NULL, (const UInt8 *)attr.data, attr.length, kCFStringEncodingUTF8, FALSE);
						break;
					}
				}
			}
			if (descriptor == NULL) {
				descriptor = (CFStringRef) CFRetain(CFSTR("<unknown>"));
			}
			access = new Access(cfString(descriptor));
			CFRelease(descriptor);
		}

		KeyItem *key = KeyItem::required(keyRef);
		Item item = key->importTo(Keychain::optional(keychainRef), access, attrList);
		if (outItemRef)
			*outItemRef = item->handle();

	END_SECAPI
}

static OSStatus
_FilterWithPolicy(SecPolicyRef policy, CFDateRef date, SecCertificateRef cert)
{
	CFDictionaryRef props = NULL;
	CFArrayRef keychains = NULL;
	CFArrayRef anchors = NULL;
	CFArrayRef certs = NULL;
	CFArrayRef chain = NULL;
	SecTrustRef trust = NULL;

	SecTrustResultType	trustResult;
	Boolean needChain = false;
	OSStatus status;
	if (!policy || !cert) return errSecParam;

	certs = CFArrayCreate(NULL, (const void **)&cert, (CFIndex)1, &kCFTypeArrayCallBacks);
	status = SecTrustCreateWithCertificates(certs, policy, &trust);
	if(status) goto cleanup;

	/* Set evaluation date, if specified (otherwise current date is implied) */
	if (date && (CFGetTypeID(date) == CFDateGetTypeID())) {
		status = SecTrustSetVerifyDate(trust, date);
		if(status) goto cleanup;
	}

	/* Check whether this is the X509 Basic policy, which means chain building */
	props = SecPolicyCopyProperties(policy);
	if (props) {
		CFTypeRef oid = (CFTypeRef) CFDictionaryGetValue(props, kSecPolicyOid);
		if (oid && (CFEqual(oid, kSecPolicyAppleX509Basic) ||
                    CFEqual(oid, kSecPolicyAppleRevocation))) {
			needChain = true;
		}
	}

	if (!needChain) {
		status = SecTrustEvaluateLeafOnly(trust, &trustResult);
	} else {
		status = SecTrustEvaluate(trust, &trustResult);
	}

	if (!(trustResult == kSecTrustResultProceed ||
		  trustResult == kSecTrustResultUnspecified ||
		  trustResult == kSecTrustResultRecoverableTrustFailure)) {
		/* The evaluation failed in a non-recoverable way */
		status = errSecCertificateCannotOperate;
		goto cleanup;
	}

	/* If there are no per-cert policy status codes,
	 * and the cert has not expired, consider it valid for the policy.
	 */
	if (true) {
		(void)SecTrustGetCssmResultCode(trust, &status);
	}

cleanup:
	if(props) CFRelease(props);
	if(chain) CFRelease(chain);
	if(anchors) CFRelease(anchors);
	if(keychains) CFRelease(keychains);
	if(certs) CFRelease(certs);
	if(trust) CFRelease(trust);

	return status;
}

static OSStatus
_FilterWithDate(CFTypeRef validOnDate, SecCertificateRef cert)
{
	if (!validOnDate || !cert) return errSecParam;

	CFAbsoluteTime at, nb, na;
	if (CFGetTypeID(validOnDate) == CFDateGetTypeID())
		at = CFDateGetAbsoluteTime((CFDateRef)validOnDate);
	else
		at = CFAbsoluteTimeGetCurrent();

	OSStatus status = errSecSuccess;
	nb = SecCertificateNotValidBefore(cert);
	na = SecCertificateNotValidAfter(cert);

	if (nb == 0 || na == 0 || nb == na)
		status = errSecCertificateCannotOperate;
	else if (at < nb)
		status = errSecCertificateNotValidYet;
	else if (at > na)
		status = errSecCertificateExpired;

	return status;
}

static OSStatus
_FilterWithTrust(Boolean trustedOnly, SecCertificateRef cert)
{
	if (!cert) return errSecParam;
	if (!trustedOnly) return errSecSuccess;

	CFArrayRef certArray = CFArrayCreate(NULL, (const void**)&cert, 1, &kCFTypeArrayCallBacks);
	SecPolicyRef policy = SecPolicyCreateWithOID(kSecPolicyAppleX509Basic);
	OSStatus status = (policy == NULL) ? errSecPolicyNotFound : errSecSuccess;

	if (!status) {
		SecTrustRef trust = NULL;
		status = SecTrustCreateWithCertificates(certArray, policy, &trust);
		if (!status) {
			SecTrustResultType trustResult;
			status = SecTrustEvaluate(trust, &trustResult);
			if (!status) {
				if (!(trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified)) {
					status = (trustResult == kSecTrustResultDeny) ? errSecTrustSettingDeny : errSecNotTrusted;
				}
			}
			CFRelease(trust);
		}
		CFRelease(policy);
	}
	if (certArray) {
		CFRelease(certArray);
	}

	return status;
}

static SecKeychainItemRef
CopyResolvedKeychainItem(CFTypeRef item)
{
	SecKeychainItemRef kcItem = NULL;
	OSStatus status = errSecSuccess;
	if (item) {
		if (CFGetTypeID(item) == CFDataGetTypeID()) {
			// persistent reference, resolve first
			status = SecKeychainItemCopyFromPersistentReference((CFDataRef)item, &kcItem);
		}
		else {
			// normal reference
			kcItem = (SecKeychainItemRef) CFRetain(item);
		}
		if (kcItem) {
			// ask for the item's class:
			// will return an error if the item has been deleted
			SecItemClass itemClass;
			SecCertificateRef certRef = NULL;
			CFTypeID itemTypeID = CFGetTypeID(kcItem);
			if (itemTypeID == SecIdentityGetTypeID()) {
				status = SecIdentityCopyCertificate((SecIdentityRef)kcItem, &certRef);
			}
			else if (itemTypeID == SecCertificateGetTypeID()) {
				certRef = (SecCertificateRef) CFRetain(kcItem);
			}
			if (certRef) {
				// can't call SecKeychainItemCopyAttributesAndData on a SecCertificateRef
				itemClass = kSecCertificateItemClass;
			}
			else {
				status = SecKeychainItemCopyAttributesAndData(kcItem, NULL, &itemClass, NULL, NULL, NULL);
			}
			if (certRef) {
				CFRelease(certRef);
			}
			if (status) {
				CFRelease(kcItem);
				kcItem = NULL;
			}
		}
	}
	return kcItem;
}

static OSStatus
UpdateKeychainSearchAndCopyNext(SecItemParams *params, CFTypeRef *item)
{
	// This function refreshes the search parameters in the specific case where
	// the caller is searching for kSecClassKey items but did not provide the
	// kSecAttrKeyClass. In that case, params->assumedKeyClass will be set, and
	// we must perform separate searches to obtain all results.

	OSStatus status = errSecItemNotFound;
	if (!params || !params->assumedKeyClass || !params->query || !item)
		return status;

	// Free the previous search reference and attribute list.
	if (params->search)
		CFRelease(params->search);
	params->search = NULL;
	_FreeAttrList(params->attrList);
	params->attrList = NULL;

	// Make a copy of the query dictionary so we can set the key class parameter.
	CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(NULL, 0, params->query);
	CFRelease(params->query);
	params->query = dict;
	CFDictionarySetValue(dict, kSecAttrKeyClass, params->assumedKeyClass);

	// Determine the current item class for this search, and the next assumed key class.
	if (CFEqual(params->assumedKeyClass, kSecAttrKeyClassSymmetric)) {
		params->itemClass = kSecSymmetricKeyItemClass;
		params->assumedKeyClass = kSecAttrKeyClassPublic;
	} else if (CFEqual(params->assumedKeyClass, kSecAttrKeyClassPublic)) {
		params->itemClass = kSecPublicKeyItemClass;
		params->assumedKeyClass = kSecAttrKeyClassPrivate;
	} else {
		params->itemClass = kSecPrivateKeyItemClass;
		params->assumedKeyClass = NULL;
	}

	// Rebuild the attribute list for the new key class.
	if (_CreateSecKeychainAttributeListFromDictionary(dict, params->itemClass, &params->attrList) == errSecSuccess) {
		// Create a new search reference for the new attribute list.
		if (SecKeychainSearchCreateFromAttributes(params->searchList,
			params->itemClass,
			(params->attrList->count == 0) ? NULL : params->attrList,
			(SecKeychainSearchRef*)&params->search) == errSecSuccess) {
			// Return the first matching item from the new search.
			// We won't come back here again until there are no more matching items for this search.
			status = SecKeychainSearchCopyNext((SecKeychainSearchRef)params->search, (SecKeychainItemRef*)item);
		}
	}
	return status;
}


static OSStatus
SecItemSearchCopyNext(SecItemParams *params, CFTypeRef *item)
{
	// Generic "copy next match" function for SecKeychainSearchRef or SecIdentitySearchRef.
	// Returns either a SecKeychainItemRef or a SecIdentityRef in the output parameter,
	// depending on the type of search reference.

	OSStatus status;
	CFTypeRef search = (params) ? params->search : NULL;
	CFTypeID typeID = (search) ? CFGetTypeID(search) : 0;
	if (typeID == SecIdentitySearchGetTypeID()) {
		status = SecIdentitySearchCopyNext((SecIdentitySearchRef)search, (SecIdentityRef*)item);
	}
	else if (typeID == SecKeychainSearchGetTypeID()) {
		status = SecKeychainSearchCopyNext((SecKeychainSearchRef)search, (SecKeychainItemRef*)item);
		// Check if we need to refresh the search for the next key class
		while (status == errSecItemNotFound && params->assumedKeyClass != NULL)
			status = UpdateKeychainSearchAndCopyNext(params, item);
	}
	else if (typeID == 0 && (params->useItems || params->itemList)) {
		// No search available, but there is an item list available.
		// Return the next candidate item from the caller's item list
		CFArrayRef itemList = (params->useItems) ? params->useItems : params->itemList;
		CFIndex count = CFArrayGetCount(itemList);
		*item = (CFTypeRef) NULL;
		if (params->itemListIndex < count) {
			*item = (CFTypeRef)CFArrayGetValueAtIndex(itemList, params->itemListIndex++);
			if (*item) {
				// Potentially resolve persistent item references here, and
				// verify the item reference we're about to hand back is still
				// valid (it could have been deleted from the keychain while
				// our query was holding onto the itemList).
				*item = CopyResolvedKeychainItem(*item);
				if (*item && (CFGetTypeID(*item) == SecIdentityGetTypeID())) {
					// Persistent reference resolved to an identity, so return that type.
					params->returnIdentity = true;
				}
			}
		}
		status = (*item) ? errSecSuccess : errSecItemNotFound;
	}
	else {
		status = errSecItemNotFound;
	}
	return status;
}

static OSStatus
FilterCandidateItem(CFTypeRef *item, SecItemParams *itemParams, SecIdentityRef *identity)
{
	if (!item || *item == NULL || !itemParams)
		return errSecItemNotFound;

	OSStatus status;
	CFStringRef commonName = NULL;
	SecIdentityRef foundIdentity = NULL;
	if (CFGetTypeID(*item) == SecIdentityGetTypeID()) {
		// we found a SecIdentityRef, rather than a SecKeychainItemRef;
		// replace the found "item" with its associated certificate (which is the
		// item we actually want for purposes of getting attributes, data, or a
		// persistent data reference), and return the identity separately.
		SecCertificateRef certificate;
		status = SecIdentityCopyCertificate((SecIdentityRef) *item, &certificate);
		if (itemParams->returnIdentity) {
			foundIdentity = (SecIdentityRef) *item;
			if (identity) {
				*identity = foundIdentity;
			}
		}
		else {
			CFRelease(*item);
		}
		*item = (CFTypeRef)certificate;
	}

	CFDictionaryRef query = itemParams->query;

	if (itemParams->itemClass == kSecCertificateItemClass) {
		// perform string comparisons first
		CFStringCompareFlags flags = _StringCompareFlagsFromQuery(query);
		CFStringRef nameContains, nameStarts, nameEnds, nameExact;
		if (!CFDictionaryGetValueIfPresent(query, kSecMatchSubjectContains, (const void **)&nameContains))
			nameContains = NULL;
		if (!CFDictionaryGetValueIfPresent(query, kSecMatchSubjectStartsWith, (const void **)&nameStarts))
			nameStarts = NULL;
		if (!CFDictionaryGetValueIfPresent(query, kSecMatchSubjectEndsWith, (const void **)&nameEnds))
			nameEnds = NULL;
		if (!CFDictionaryGetValueIfPresent(query, kSecMatchSubjectWholeString, (const void **)&nameExact))
			nameExact = NULL;
		if (nameContains || nameStarts || nameEnds || nameExact) {
			status = SecCertificateCopyCommonName((SecCertificateRef)*item, &commonName);
			if (status || !commonName) goto filterOut;
		}
		if (nameContains) {
			CFRange range = CFStringFind(commonName, nameContains, flags);
			if (range.length < 1)
				goto filterOut;
			// certificate item contains string; proceed to next check
		}
		if (nameStarts) {
			CFRange range = CFStringFind(commonName, nameStarts, flags);
			if (range.length < 1 || range.location > 1)
				goto filterOut;
			// certificate item starts with string; proceed to next check
		}
		if (nameEnds) {
			CFRange range = CFStringFind(commonName, nameEnds, flags);
			if (range.length < 1 || range.location != (CFStringGetLength(commonName) - CFStringGetLength(nameEnds)))
				goto filterOut;
			// certificate item ends with string; proceed to next check
		}
		if (nameExact) {
			CFRange range = CFStringFind(commonName, nameExact, flags);
			if (range.length < 1 || (CFStringGetLength(commonName) != CFStringGetLength(nameExact)))
				goto filterOut;
			// certificate item exactly matches string; proceed to next check
		}
		if (itemParams->returnIdentity) {
			// if we already found and returned the identity, we can skip this
			if (!foundIdentity) {
				status = SecIdentityCreateWithCertificate(itemParams->searchList, (SecCertificateRef) *item, identity);
				if (status) goto filterOut;
			}
			// certificate item is part of an identity; proceed to next check
		}
		if (itemParams->policy) {
			status = _FilterWithPolicy(itemParams->policy, (CFDateRef)itemParams->validOnDate, (SecCertificateRef) *item);
			if (status) goto filterOut;
			// certificate item is valid for specified policy (and optionally specified date)
		}
		if (itemParams->validOnDate) {
			status = _FilterWithDate(itemParams->validOnDate, (SecCertificateRef) *item);
			if (status) goto filterOut;
			// certificate item is valid for specified date
		}
		if (itemParams->trustedOnly) {
			// if we are getting candidate items from a SecIdentitySearchCreateWithPolicy search,
			// their trust has already been validated and we can skip this part.
			if (!(foundIdentity && itemParams->returnIdentity && itemParams->policy)) {
				status = _FilterWithTrust(itemParams->trustedOnly, (SecCertificateRef) *item);
				if (status) goto filterOut;
			}
			// certificate item is trusted on this system
		}
	}
	if (itemParams->itemList) {
		Boolean foundMatch = FALSE;
		CFIndex idx, count = CFArrayGetCount(itemParams->itemList);
		for (idx=0; idx<count; idx++) {
			CFTypeRef anItem = (CFTypeRef) CFArrayGetValueAtIndex(itemParams->itemList, idx);
			SecKeychainItemRef realItem = NULL;
			SecCertificateRef aCert = NULL;
			if (anItem == NULL) {
				continue;
			}
			if (CFDataGetTypeID() == CFGetTypeID(anItem) &&
				errSecSuccess == SecKeychainItemCopyFromPersistentReference((CFDataRef)anItem, &realItem)) {
				anItem = realItem;
			}
			if (SecIdentityGetTypeID() == CFGetTypeID(anItem) &&
				errSecSuccess == SecIdentityCopyCertificate((SecIdentityRef)anItem, &aCert)) {
				anItem = aCert;
			}
			if (CFEqual(anItem, (CFTypeRef) *item)) {
				foundMatch = TRUE;
			}
			if (aCert) {
				CFRelease(aCert);
			}
			if (realItem) {
				CFRelease(realItem);
			}
			if (foundMatch) {
				break;
			}
		}
		if (!foundMatch) goto filterOut;
		// item was found on provided list
	}

	if (foundIdentity && !identity) {
		CFRelease(foundIdentity);
	}
	if (commonName) {
		CFRelease(commonName);
	}

	// if we get here, consider the item a match
	return errSecSuccess;

filterOut:
	if (commonName) {
		CFRelease(commonName);
	}
	CFRelease(*item);
	*item = NULL;
	if (foundIdentity) {
		CFRelease(foundIdentity);
		if (identity) {
			*identity = NULL;
		}
	}
	return errSecItemNotFound;
}

static OSStatus
AddItemResults(SecKeychainItemRef item,
	SecIdentityRef identity,
	SecItemParams *itemParams,
	CFAllocatorRef allocator,
	CFMutableArrayRef *items,
	CFTypeRef *result)
{
	// Given a found item (which may also be an identity), this function adds
	// the requested result types (specified in itemParams) to the appropriate
	// container as follows:
	//
	// 1. If there is only one result type (numResultTypes == 1) and only one
	//    match requested (maxMatches == 1), set *result directly.
	//
	// 2. If there are multiple result types (numResultTypes > 1), and only one
	//    match requested (maxMatches == 1), add each result type to itemDict
	//    and set itemDict as the value of *result.
	//
	// 3. If there is only one result type (numResultTypes == 1) and multiple
	//    possible matches (maxMatches > 1), add the result type to *items
	//    and set *items as the value of *result.
	//
	// 4. If there are multiple result types (numResultTypes > 1) and multiple
	//    possible matches (maxMatches > 1), add each result type to itemDict,
	//    add itemDict to *items, and set *items as the value of *result.
	//
	// Note that we allocate *items if needed.

	if (!item || !itemParams || !result)
		return errSecParam;

	if (itemParams->maxMatches > 1) {
		// if we can return more than one item, we must have an array
		if (!items)
			return errSecParam;
		else if (*items == NULL)
			*items = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
	}

	OSStatus tmpStatus, status = errSecSuccess;
	CFMutableArrayRef itemArray = (items) ? *items : NULL;
	CFMutableDictionaryRef itemDict = NULL;
	if (itemParams->numResultTypes > 1) {
		// if we're returning more than one result type, each item we return must be a dictionary
		itemDict = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}

	if (itemParams->returningRef) {
		const void* itemRef = (identity) ? (const void*)identity : (const void*)item;
		if (itemDict) {
			CFDictionaryAddValue(itemDict, kSecValueRef, itemRef);
		}
		else if (itemArray) {
			CFArrayAppendValue(itemArray, itemRef);
		}
		else {
			*result = CFRetain((CFTypeRef)itemRef);
		}
	}

	if (itemParams->returningPersistentRef) {
		CFDataRef persistentRef;
		SecKeychainItemRef tmpItem = item;
		if (itemParams->identityRef) {
			tmpItem = (SecKeychainItemRef)itemParams->identityRef;
		}
		tmpStatus = SecKeychainItemCreatePersistentReference(tmpItem, &persistentRef);
		if (tmpStatus == errSecSuccess) {
			if (itemDict) {
				CFDictionaryAddValue(itemDict, kSecValuePersistentRef, persistentRef);
			}
			else if (itemArray) {
				CFArrayAppendValue(itemArray, persistentRef);
			}
			else {
				*result = CFRetain(persistentRef);
			}
			CFRelease(persistentRef);
		}
		else if (status == errSecSuccess) {
			status = tmpStatus;
		}
	}

	if (itemParams->returningData) {
		// Use SecCertificateCopyData if we have a SecCertificateRef item.
		// Note that a SecCertificateRef may not actually be a SecKeychainItem,
		// in which case SecKeychainItemCopyContent will not obtain its data.

		if (CFGetTypeID(item) == SecCertificateGetTypeID()) {
			CFDataRef dataRef = SecCertificateCopyData((SecCertificateRef)item);
			if (dataRef) {
				if (itemDict) {
					CFDictionaryAddValue(itemDict, kSecValueData, dataRef);
				}
				else if (itemArray) {
					CFArrayAppendValue(itemArray, dataRef);
				}
				else {
					*result = CFRetain(dataRef);
				}
				CFRelease(dataRef);
				status = errSecSuccess;
			}
			else {
				status = errSecAllocate;
			}
		}
		else {
			UInt32 length;
			void *data;
			tmpStatus = SecKeychainItemCopyContent(item, NULL, NULL, &length, &data);
			if (tmpStatus == errSecSuccess) {
				CFDataRef dataRef = CFDataCreate(allocator, (UInt8 *)data, length);
				if (itemDict) {
					CFDictionaryAddValue(itemDict, kSecValueData, dataRef);
				}
				else if (itemArray) {
					CFArrayAppendValue(itemArray, dataRef);
				}
				else {
					*result = CFRetain(dataRef);
				}
				CFRelease(dataRef);
				(void) SecKeychainItemFreeContent(NULL, data);
			}
			else if (status == errSecSuccess) {
				status = tmpStatus;
			}
		}
	}

	if (itemParams->returningAttributes) {
		CFDictionaryRef attrsDict = NULL;
		SecItemClass itemClass;
		// since we have an item, allow its actual class to override the query-specified item class
		tmpStatus = SecKeychainItemCopyAttributesAndData(item, NULL, &itemClass, NULL, NULL, NULL);
		if (tmpStatus) {
			itemClass = itemParams->itemClass;
		}
		tmpStatus = _CreateAttributesDictionaryFromItem(allocator, itemClass, item, &attrsDict);
		if (attrsDict) {
			if (itemDict) {
				// add all keys and values from attrsDict to the item dictionary
				CFDictionaryApplyFunction(attrsDict, _AddDictValueToOtherDict, &itemDict);
			}
			else if (itemArray) {
				CFArrayAppendValue(itemArray, attrsDict);
			}
			else {
				*result = CFRetain(attrsDict);
			}
			CFRelease(attrsDict);
		}
		if (tmpStatus && (status == errSecSuccess)) {
			status = tmpStatus;
		}
	}

	if (itemDict) {
		if (itemArray) {
			CFArrayAppendValue(itemArray, itemDict);
			CFRelease(itemDict);
			*result = itemArray;
		}
		else {
			*result = itemDict;
		}
	}
	else if (itemArray) {
		*result = itemArray;
	}

	return status;
}

CFDataRef _SecItemGetPersistentReference(CFTypeRef raw_item)
{
	try {
		Item item = ItemImpl::required((SecKeychainItemRef)raw_item);
		return item->getPersistentRef();
	} catch(...) {
		return NULL;
	}
}

/******************************************************************************/
#pragma mark SecItem API functions
/******************************************************************************/

//
// Approximate result of using iOS sec's copyNumber, 0 return could be zero, or error.
//
static SInt32 readNumber(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    SInt32 v = 0;
    if (tid == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)obj, kCFNumberSInt32Type, &v);
        return v;
    } else if (tid == CFBooleanGetTypeID()) {
        v = CFBooleanGetValue((CFBooleanRef)obj);
        return v;
    } else if (tid == CFStringGetTypeID()) {
        v = CFStringGetIntValue((CFStringRef)obj);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long)v);
        /* If a string converted to an int isn't equal to the int printed as
         a string, return a CFStringRef instead. */
        if (!CFEqual(t, obj)) {
            CFRelease(t);
            return 0;
        }
        CFRelease(t);
        return v;
    } else
        return NULL;
}

//
// Function to check whether the kSecAttrSynchronizable flag is set in the query.
//
static Boolean SecItemSynchronizable(CFDictionaryRef query)
{
	CFTypeRef value = CFDictionaryGetValue(query, kSecAttrSynchronizable);
	Boolean result = (value && readNumber(value));

	return result;
}

//
// Function to check whether a synchronizable persistent reference was provided.
//
static Boolean SecItemIsIOSPersistentReference(CFTypeRef value)
{
	if (value) {
		/* Synchronizable persistent ref consists of the sqlite rowid and 4-byte class value */
		const CFIndex kSynchronizablePersistentRefLength = sizeof(int64_t) + 4;
		return (CFGetTypeID(value) == CFDataGetTypeID() &&
				CFDataGetLength((CFDataRef)value) == kSynchronizablePersistentRefLength);
	}
	return false;
}

extern "C" Boolean SecKeyIsCDSAKey(SecKeyRef ref);

//
// Function to find out which keychains are targetted by the query.
//
static OSStatus SecItemCategorizeQuery(CFDictionaryRef query, bool &can_target_ios, bool &can_target_osx)
{
	// By default, target both keychain.
	can_target_osx = can_target_ios = true;

	// Check no-legacy flag.
    CFTypeRef value = CFDictionaryGetValue(query, kSecAttrNoLegacy);
	if (value != NULL) {
		can_target_ios = readNumber(value) != 0;
		can_target_osx = !can_target_ios;
		return errSecSuccess;
	}

	// Check whether the query contains kSecValueRef and modify can_ flags according to the kind and type of the value.
	value = CFDictionaryGetValue(query, kSecValueRef);
	if (value != NULL) {
		CFTypeID typeID = CFGetTypeID(value);
		if (typeID == SecKeyGetTypeID()) {
			can_target_osx = SecKeyIsCDSAKey((SecKeyRef)value);
			can_target_ios = !can_target_osx;
		} else if (typeID == SecCertificateGetTypeID()) {
			// All types of certificates can target OSX keychains, but OSX certificates won't work on iOS
			can_target_ios &= !SecCertificateIsItemImplInstance((SecCertificateRef)value);
		} else if (typeID == SecKeychainItemGetTypeID()) {
			// SecKeychainItemRef can target iOS keychain only when it has attached iOS-style persistent reference.
			if (_SecItemGetPersistentReference(value) == NULL) {
				can_target_ios = false;
			}
		}
	}

	// Check presence of kSecAttrTokenID and kSecAttrAccessControl; they are not defined for CDSA keychain.
	if (CFDictionaryContainsKey(query, kSecAttrTokenID) || CFDictionaryContainsKey(query, kSecAttrAccessControl)) {
		can_target_osx = false;
	}

	// Check for special token access groups.  If present, redirect query to iOS keychain.
	value = CFDictionaryGetValue(query, kSecAttrAccessGroup);
	if (value != NULL && CFEqual(value, kSecAttrAccessGroupToken)) {
		can_target_osx = false;
	}

	// Synchronizable items should go to iOS keychain only.
	if (SecItemSynchronizable(query)) {
		can_target_osx = false;
	}

	value = CFDictionaryGetValue(query, kSecValuePersistentRef);
	if (value != NULL) {
		if (SecItemIsIOSPersistentReference(value)) {
			can_target_osx = false;
		} else {
			// Non-iOS-style persistent references should not be fed to iOS keychain queries.
			can_target_ios = false;
		}
	}

	// Presence of following atributes means that query is OSX-only.
	static const CFStringRef *osx_only_items[] = {
		&kSecMatchItemList,
		&kSecMatchSearchList,
		&kSecMatchSubjectStartsWith,
		&kSecMatchSubjectEndsWith,
		&kSecMatchSubjectWholeString,
		&kSecMatchDiacriticInsensitive,
		&kSecMatchWidthInsensitive,
		&kSecUseItemList,
		&kSecUseKeychain,
		&kSecAttrAccess,
		&kSecAttrPRF,
		&kSecAttrSalt,
		&kSecAttrRounds,
	};

	for (CFIndex i = 0; i < array_size(osx_only_items); i++) {
		can_target_ios = can_target_ios && !CFDictionaryContainsKey(query, *osx_only_items[i]);
	}

	return (can_target_ios || can_target_osx) ? errSecSuccess : errSecParam;
}

//
// Function to check whether the kSecAttrSynchronizable attribute is being updated.
//
static Boolean SecItemHasSynchronizableUpdate(Boolean synchronizable, CFDictionaryRef changes)
{
	CFTypeRef newValue = CFDictionaryGetValue(changes, kSecAttrSynchronizable);
	if (!newValue)
		return false;

	Boolean new_sync = readNumber(newValue);
	Boolean old_sync = synchronizable;

	return (old_sync != new_sync);
}

//
// Function to apply changes to a mutable dictionary.
// (CFDictionaryApplierFunction, called by CFDictionaryApplyFunction)
//
static void SecItemApplyChanges(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef dict = (CFMutableDictionaryRef) context;
	if (!dict) return;

	CFDictionarySetValue(dict, key, value);
}

//
// Function to change matching items from non-syncable to syncable
// (if toSyncable is true), otherwise from syncable to non-syncable.
// This currently moves items between keychain containers.
//
static OSStatus SecItemChangeSynchronizability(CFDictionaryRef query, CFDictionaryRef changes, Boolean toSyncable)
{
	// Note: the input query dictionary is a mutable copy of the query originally
	// provided by the caller as the first parameter to SecItemUpdate. It may not
	// specify returning attributes or data, but we will need both to make a copy.
	//
	CFDictionaryRemoveValue((CFMutableDictionaryRef)query, kSecReturnRef);
	CFDictionaryRemoveValue((CFMutableDictionaryRef)query, kSecReturnPersistentRef);
	CFDictionaryRemoveValue((CFMutableDictionaryRef)query, kSecReturnData);
	CFDictionarySetValue((CFMutableDictionaryRef)query, kSecReturnAttributes, kCFBooleanTrue);
	if (NULL == CFDictionaryGetValue(changes, kSecValueData))
		CFDictionarySetValue((CFMutableDictionaryRef)query, kSecReturnData, kCFBooleanTrue);

	CFTypeRef result;
	OSStatus status;
	if (toSyncable)
		status = SecItemCopyMatching_osx(query, &result);
	else
		status = SecItemCopyMatching_ios(query, &result);

	if (status)
		return status;
	if (!result)
		return errSecItemNotFound;

	CFMutableArrayRef items;
	if (CFGetTypeID(result) != CFArrayGetTypeID()) {
		items = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(items, result);
		CFRelease(result);
	}
	else {
		items = (CFMutableArrayRef)result;
	}

	CFIndex idx, count = (items) ? CFArrayGetCount(items) : 0;
	int priority = LOG_DEBUG;
	OSStatus err = 0;
	for (idx = 0; idx < count; idx++) {
		CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(items, idx);
		CFMutableDictionaryRef item = (CFMutableDictionaryRef)
			SecItemCopyTranslatedAttributes(dict,
				CFDictionaryGetValue(query, kSecClass),
				(toSyncable) ? true : false /*iOSOut*/,
				true /*pruneMatch*/,
				true /*pruneSync*/,
				true /*pruneReturn*/,
				false /*pruneData*/,
				(toSyncable) ? true : false /*pruneAccess*/);
		// hold onto the query before applying changes, in case the item already exists.
		// note that we cannot include the creation or modification dates from our
		// found item in this query, as they may not match the item in the other keychain.
		CFMutableDictionaryRef itemQuery = CFDictionaryCreateMutableCopy(NULL, 0, item);
		CFDictionaryRemoveValue(itemQuery, kSecAttrCreationDate);
		CFDictionaryRemoveValue(itemQuery, kSecAttrModificationDate);
		// apply changes to the item dictionary that we will pass to SecItemAdd
		CFDictionaryApplyFunction(changes, SecItemApplyChanges, item);
		if (toSyncable) {
			CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanTrue);
			status = SecItemAdd_ios(item, NULL);
			secitemlog(priority, "ChangeSync: SecItemAdd_ios=%d", status);
			if (errSecDuplicateItem == status) {
				// find and apply changes to the existing syncable item.
				CFDictionarySetValue(itemQuery, kSecAttrSynchronizable, kCFBooleanTrue);
				status = SecItemUpdate_ios(itemQuery, changes);
				secitemlog(priority, "ChangeSync: SecItemUpdate_ios=%d", status);
			}
			if (errSecSuccess == status) {
				CFDictionarySetValue(itemQuery, kSecAttrSynchronizable, kCFBooleanFalse);
				status = SecItemDelete_osx(itemQuery);
				secitemlog(priority, "ChangeSync: SecItemDelete_osx=%d", status);
			}
		}
		else {
			CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
			status = SecItemAdd_osx(item, NULL);
			secitemlog(priority, "ChangeSync: SecItemAdd_osx=%d", status);
			if (errSecDuplicateItem == status) {
				// find and apply changes to the existing non-syncable item.
				CFDictionarySetValue(itemQuery, kSecAttrSynchronizable, kCFBooleanFalse);
				status = SecItemUpdate_osx(itemQuery, changes);
				secitemlog(priority, "ChangeSync: SecItemUpdate_osx=%d", status);
			}
			if (errSecSuccess == status) {
				CFDictionarySetValue(itemQuery, kSecAttrSynchronizable, kCFBooleanTrue);
				status = SecItemDelete_ios(itemQuery);
				secitemlog(priority, "ChangeSync: SecItemDelete_ios=%d", status);
			}
		}
		CFReleaseSafe(item);
		CFReleaseSafe(itemQuery);
		if (status)
			err = status;
	}
	CFReleaseSafe(items);

	return err;
}


extern "C" {

CFTypeRef
SecItemCreateFromAttributeDictionary_osx(CFDictionaryRef refAttributes) {
	CFTypeRef ref = NULL;
	CFStringRef item_class_string = (CFStringRef)CFDictionaryGetValue(refAttributes, kSecClass);
	SecItemClass item_class = (SecItemClass) 0;

	if (CFEqual(item_class_string, kSecClassGenericPassword)) {
		item_class = kSecGenericPasswordItemClass;
	} else if (CFEqual(item_class_string, kSecClassInternetPassword)) {
		item_class = kSecInternetPasswordItemClass;
	}

	if (item_class != 0) {
		// we carry v_Data around here so the *_ios calls can find it and locate
		// their own data.   Putting things in the attribute list doesn't help as
		// the osx keychainitem and item calls bail when they don't see a keychain
		// object.   If we need to make them work we either have to bridge them, or
		// find a way to craft a workable keychain object.   #if'ed code left below
		// in case we need to go down that path.

    SecKeychainAttributeList attrs = {};
    SecKeychainAttribute attr = {};

    attrs.attr = &attr;
    attrs.count = 0;
		CFTypeRef v;

    Item item = Item(item_class, &attrs, 0, "");
		v = CFDictionaryGetValue(refAttributes, kSecValuePersistentRef);
		if (v) {
			item->setPersistentRef((CFDataRef)v);
		}
		ref = item->handle();
	}

	return ref;
}

/*
 * SecItemValidateAppleApplicationGroupAccess determines if the caller
 * is a member of the specified application group, and is signed by Apple.
 */
OSStatus
SecItemValidateAppleApplicationGroupAccess(CFStringRef group)
{
	SecTrustedApplicationRef app = NULL;
	SecRequirementRef requirement = NULL;
	SecCodeRef code = NULL;
	OSStatus status = errSecParam;

	if (group) {
		CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(group), kCFStringEncodingUTF8) + 1;
		char* buffer = (char*) malloc(length);
		if (buffer) {
			if (CFStringGetCString(group, buffer, length, kCFStringEncodingUTF8)) {
				status = SecTrustedApplicationCreateApplicationGroup(buffer, NULL, &app);
			}
			free(buffer);
		} else {
			status = errSecMemoryError;
		}
	}
	if (!status) {
		status = SecTrustedApplicationCopyRequirement(app, &requirement);
	}
	if (!status) {
		status = SecCodeCopySelf(kSecCSDefaultFlags, &code);
	}
	if (!status) {
		status = SecCodeCheckValidity(code, kSecCSDefaultFlags, requirement);
	}

	CFReleaseSafe(code);
	CFReleaseSafe(requirement);
	CFReleaseSafe(app);
	return status;
}

static Mutex gParentCertCacheLock;
static CFMutableDictionaryRef gParentCertCache;
static CFMutableArrayRef gParentCertCacheList;
#define PARENT_CACHE_SIZE 100

void SecItemParentCachePurge() {
    StLock<Mutex> _(gParentCertCacheLock);
    CFReleaseNull(gParentCertCache);
    CFReleaseNull(gParentCertCacheList);
}

static CFArrayRef parentCacheRead(SecCertificateRef certificate) {
    CFArrayRef parents = NULL;
    CFIndex ix;
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    if (!digest) return NULL;

    StLock<Mutex> _(gParentCertCacheLock);
    if (gParentCertCache && gParentCertCacheList) {
        if (0 <= (ix = CFArrayGetFirstIndexOfValue(gParentCertCacheList,
                                                   CFRangeMake(0, CFArrayGetCount(gParentCertCacheList)),
                                                   digest))) {
            // Cache hit. Get value and move entry to the top of the list.
            parents = (CFArrayRef)CFDictionaryGetValue(gParentCertCache, digest);
            CFArrayRemoveValueAtIndex(gParentCertCacheList, ix);
            CFArrayAppendValue(gParentCertCacheList, digest);
        }
    }
    CFRetainSafe(parents);
    return parents;
}

static void parentCacheWrite(SecCertificateRef certificate, CFArrayRef parents) {
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    if (!digest) return;

    StLock<Mutex> _(gParentCertCacheLock);
    if (!gParentCertCache || !gParentCertCacheList) {
        CFReleaseNull(gParentCertCache);
        gParentCertCache = makeCFMutableDictionary();
        CFReleaseNull(gParentCertCacheList);
        gParentCertCacheList = makeCFMutableArray(0);
    }

    if (gParentCertCache && gParentCertCacheList) {
        // check to make sure another thread didn't add this entry to the cache already
        if (0 > CFArrayGetFirstIndexOfValue(gParentCertCacheList,
                                            CFRangeMake(0, CFArrayGetCount(gParentCertCacheList)),
                                            digest)) {
            CFDictionaryAddValue(gParentCertCache, digest, parents);
            if (PARENT_CACHE_SIZE <= CFArrayGetCount(gParentCertCacheList)) {
                // Remove least recently used cache entry.
                CFArrayRemoveValueAtIndex(gParentCertCacheList, 0);
            }
            CFArrayAppendValue(gParentCertCacheList, digest);
        }
    }
}

/*
 * SecItemCopyParentCertificates returns an array of zero of more possible
 * issuer certificates for the provided certificate. No cryptographic validation
 * of the signature is performed in this function; its purpose is only to
 * provide a list of candidate certificates.
 */
CFArrayRef
SecItemCopyParentCertificates(SecCertificateRef certificate, void *context)
{
#pragma unused (context) /* for now; in future this can reference a container object */
	/* Check for parents in keychain cache */
	CFArrayRef parents = parentCacheRead(certificate);
	if (parents) {
		return parents;
	}

	/* Cache miss. Query for parents. */
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	CFDataRef normalizedIssuer = SecCertificateCopyNormalizedIssuerContent(certificate, NULL);
#else
	CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);
	CFRetainSafe(normalizedIssuer);
#endif
	OSStatus status;
	CFMutableArrayRef combinedSearchList = NULL;

	/* Define the array of keychains which will be searched for parents. */
	CFArrayRef searchList = NULL;
	status = SecKeychainCopySearchList(&searchList);
	if (searchList) {
		combinedSearchList = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, searchList);
		CFRelease(searchList);
	} else {
		combinedSearchList = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	}
	SecKeychainRef rootStoreKeychain = NULL;
	status = SecKeychainOpen(SYSTEM_ROOT_STORE_PATH, &rootStoreKeychain);
	if (rootStoreKeychain) {
		if (combinedSearchList) {
			CFArrayAppendValue(combinedSearchList, rootStoreKeychain);
		}
		CFRelease(rootStoreKeychain);
	}

	/* Create and populate a fixed-size query dictionary. */
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 5,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(query, kSecClass, kSecClassCertificate);
	CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);
	CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitAll);
	if (combinedSearchList) {
		CFDictionaryAddValue(query, kSecMatchSearchList, combinedSearchList);
		CFRelease(combinedSearchList);
	}

	CFTypeRef results = NULL;
	if (normalizedIssuer) {
		/* Look up certs whose subject is the same as this cert's issuer. */
		CFDictionaryAddValue(query, kSecAttrSubject, normalizedIssuer);
		status = SecItemCopyMatching_osx(query, &results);
	}
	else {
		/* Cannot match anything without an issuer! */
		status = errSecItemNotFound;
	}

	if ((status != errSecSuccess) && (status != errSecItemNotFound)) {
		secitemlog(LOG_WARNING, "SecItemCopyParentCertificates: %d", (int)status);
	}
	CFRelease(query);

	CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	CFTypeID resultType = (results) ? CFGetTypeID(results) : 0;
	if (resultType == CFArrayGetTypeID()) {
		CFIndex index, count = CFArrayGetCount((CFArrayRef)results);
		for (index = 0; index < count; index++) {
			CFDataRef data = (CFDataRef) CFArrayGetValueAtIndex((CFArrayRef)results, index);
			if (data) {
				SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, data);
				if (cert) {
					CFArrayAppendValue(result, cert);
					CFRelease(cert);
				}
			}
		}
	} else if (resultType == CFDataGetTypeID()) {
		SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)results);
		if (cert) {
			CFArrayAppendValue(result, cert);
			CFRelease(cert);
		}
	}
	CFReleaseSafe(results);
	CFReleaseSafe(normalizedIssuer);

	/* Add to cache. */
	parentCacheWrite(certificate, result);

	return result;
}

SecCertificateRef SecItemCopyStoredCertificate(SecCertificateRef certificate, void *context)
{
#pragma unused (context) /* for now; in future this can reference a container object */

	/* Certificates are unique by issuer and serial number. */
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	CFDataRef serialNumber = SecCertificateCopySerialNumber(certificate, NULL);
	CFDataRef normalizedIssuer = SecCertificateCopyNormalizedIssuerContent(certificate, NULL);
#else
	CFDataRef serialNumber = SecCertificateCopySerialNumber(certificate);
	CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);
	CFRetainSafe(normalizedIssuer);
#endif

	const void *keys[] = {
		kSecClass,
		kSecMatchLimit,
		kSecAttrIssuer,
		kSecAttrSerialNumber,
		kSecReturnRef
	},
	*values[] = {
		kSecClassCertificate,
		kSecMatchLimitOne,
		normalizedIssuer,
		serialNumber,
		kCFBooleanTrue
	};
	CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 5,	NULL, NULL);
	CFTypeRef result = NULL;

	OSStatus status = SecItemCopyMatching_osx(query, &result);
	if ((status != errSecSuccess) && (status != errSecItemNotFound)) {
		secitemlog(LOG_WARNING, "SecItemCopyStoredCertificate: %d", (int)status);
		CFReleaseNull(result);
	}
	CFReleaseSafe(query);
	CFReleaseSafe(serialNumber);
	CFReleaseSafe(normalizedIssuer);

	return (SecCertificateRef)result;
}

/*
 * SecItemCopyTranslatedAttributes accepts a user-provided attribute dictionary
 * and attempts to return a sanitized copy for passing to the underlying
 * platform-specific implementation code.
 *
 * If iOSOut is true, one or more translations may apply:
 *   - SecKeychain refs are removed, since there aren't multiple keychains
 *   - SecPolicy refs are removed, since they can't be externalized
 *   - SecAccess refs are removed, and potentially translated to entitlements
 *
 * If pruneMatch is true, kSecMatch* attributes are removed; this avoids
 * parameter errors due to strict input checks in secd, which only permits
 * these constants for calls to SecItemCopyMatching.
 *
 * If pruneSync is true, the kSecAttrSynchronizable attribute is removed.
 * This permits a query to be reused for non-synchronizable items, or to
 * resolve a search based on a persistent item reference for iOS.
 *
 * If pruneReturn is true, kSecReturn* attributes are removed; this avoids
 * parameter errors due to strict input checks in secd, which do not permit
 * these constants for calls to SecItemUpdate.
 */
CFDictionaryRef
SecItemCopyTranslatedAttributes(CFDictionaryRef inOSXDict, CFTypeRef itemClass,
	bool iOSOut, bool pruneMatch, bool pruneSync, bool pruneReturn, bool pruneData, bool pruneAccess)
{
	CFMutableDictionaryRef result = CFDictionaryCreateMutableCopy(NULL, 0, inOSXDict);
	if (result == NULL) {
		return result;
	}

	if (pruneSync) {
		CFDictionaryRemoveValue(result, kSecAttrSynchronizable);
	}

	if (pruneMatch) {
		/* Match constants are only supported on iOS for SecItemCopyMatching,
		 * and will generate an error if passed to other SecItem API functions;
		 * on OS X, they're just ignored if not applicable for the context.
		 */
		CFDictionaryRemoveValue(result, kSecMatchPolicy);
		CFDictionaryRemoveValue(result, kSecMatchItemList);
		CFDictionaryRemoveValue(result, kSecMatchSearchList);
		CFDictionaryRemoveValue(result, kSecMatchIssuers);
		CFDictionaryRemoveValue(result, kSecMatchEmailAddressIfPresent);
		CFDictionaryRemoveValue(result, kSecMatchSubjectContains);
		CFDictionaryRemoveValue(result, kSecMatchCaseInsensitive);
		CFDictionaryRemoveValue(result, kSecMatchTrustedOnly);
		CFDictionaryRemoveValue(result, kSecMatchValidOnDate);
		CFDictionaryRemoveValue(result, kSecMatchLimit);
		CFDictionaryRemoveValue(result, kSecMatchLimitOne);
		CFDictionaryRemoveValue(result, kSecMatchLimitAll);
	}

	if (pruneReturn) {
		/* Return constants are not supported on iOS for SecItemUpdate,
		 * where they will generate an error; on OS X, they're just ignored
		 * if not applicable for the context.
		 */
		CFDictionaryRemoveValue(result, kSecReturnData);
		CFDictionaryRemoveValue(result, kSecReturnAttributes);
		CFDictionaryRemoveValue(result, kSecReturnRef);
		CFDictionaryRemoveValue(result, kSecReturnPersistentRef);
	}

	if (pruneData) {
		/* Searching on data is not supported. */
		CFDictionaryRemoveValue(result, kSecValueData);
	}

    if (pruneAccess) {
        /* Searching on access lists is not supported */
        CFDictionaryRemoveValue(result, kSecAttrAccess);
    }

	if (iOSOut) {
		/* Remove kSecMatchSearchList (value is array of SecKeychainRef);
		 * cannot specify a keychain search list on iOS
		 */
		CFDictionaryRemoveValue(result, kSecMatchSearchList);

		/* Remove kSecUseKeychain (value is a SecKeychainRef);
		 * cannot specify a keychain on iOS
		 */
		CFDictionaryRemoveValue(result, kSecUseKeychain);

		/* Potentially translate kSecAttrAccess (value is a SecAccessRef),
		 * unless kSecAttrAccessGroup has already been specified.
		 */
		SecAccessRef access = (SecAccessRef) CFDictionaryGetValue(result, kSecAttrAccess);
		CFStringRef accessGroup = (CFStringRef) CFDictionaryGetValue(result, kSecAttrAccessGroup);
		if (access != NULL && accessGroup == NULL) {
			/* Translate "InternetAccounts" application group to an access group */
			if (errSecSuccess == SecItemValidateAppleApplicationGroupAccess(CFSTR("InternetAccounts"))) {
				/* The caller is a valid member of the application group. */
				CFStringRef groupName = CFSTR("appleaccount");
				CFTypeRef value = CFDictionaryGetValue(result, kSecAttrAuthenticationType);
				if (value && CFEqual(value, kSecAttrAuthenticationTypeHTMLForm)) {
					groupName = CFSTR("com.apple.cfnetwork");
				}
				CFDictionarySetValue(result, kSecAttrAccessGroup, groupName);
			}
		}
		CFDictionaryRemoveValue(result, kSecAttrAccess);

		/* If item is specified by direct reference, and this is an iOS search,
		 * replace it with a persistent reference, if it was recorded inside ItemImpl.
		 */
		CFTypeRef directRef = CFDictionaryGetValue(result, kSecValueRef);
		if (directRef != NULL) {
			CFTypeID typeID = CFGetTypeID(directRef);
			if ((typeID != SecKeyGetTypeID() || SecKeyIsCDSAKey((SecKeyRef)directRef)) &&
				(typeID != SecCertificateGetTypeID() || SecCertificateIsItemImplInstance((SecCertificateRef)directRef)) &&
				(typeID != SecIdentityGetTypeID())) {
				CFDataRef persistentRef = _SecItemGetPersistentReference(directRef);
				if (persistentRef) {
					CFDictionarySetValue(result, kSecValuePersistentRef, persistentRef);
					CFDictionaryRemoveValue(result, kSecValueRef);
				}
			}
		}

		/* If item is specified by persistent reference, and this is an iOS search,
		 * remove the synchronizable attribute as it will be rejected by secd.
		 */
		CFTypeRef persistentRef = CFDictionaryGetValue(result, kSecValuePersistentRef);
		if (persistentRef) {
			CFDictionaryRemoveValue(result, kSecAttrSynchronizable);
		}

		/* Remove kSecAttrModificationDate; this should never be used as criteria
		 * for a search, or to add/modify an item. (If we are cloning an item
		 * and want to keep its modification date, we don't call this function.)
		 * It turns out that some clients are using the full attributes dictionary
		 * returned by SecItemCopyMatching as a query to find the same item later,
		 * which won't work once the item is updated.
		 */
		CFDictionaryRemoveValue(result, kSecAttrModificationDate);
    }
	else {
		/* iOS doesn't add the class attribute, so we must do it here. */
		if (itemClass)
			CFDictionarySetValue(result, kSecClass, itemClass);

		/* Remove attributes which are not part of the OS X database schema. */
		CFDictionaryRemoveValue(result, kSecAttrAccessible);
		CFDictionaryRemoveValue(result, kSecAttrAccessControl);
		CFDictionaryRemoveValue(result, kSecAttrAccessGroup);
		CFDictionaryRemoveValue(result, kSecAttrSynchronizable);
		CFDictionaryRemoveValue(result, kSecAttrTombstone);
	}

    /* This attribute is consumed by the bridge itself. */
    CFDictionaryRemoveValue(result, kSecAttrNoLegacy);

	return result;
}

} /* extern "C" */

static OSStatus
SecItemMergeResults(bool can_target_ios, OSStatus status_ios, CFTypeRef result_ios,
					bool can_target_osx, OSStatus status_osx, CFTypeRef result_osx,
					CFTypeRef *result) {
	// When querying both keychains and iOS keychain fails because of missing
	// entitlements, completely ignore iOS keychain result.  This is to keep
	// backward compatibility with applications which know nothing about iOS keychain
	// and use SecItem API to access OSX keychain which does not need any entitlements.
	if (can_target_osx && can_target_ios && status_ios == errSecMissingEntitlement) {
		can_target_ios = false;
	}

	if (can_target_osx && can_target_ios) {
		// If both keychains were targetted, examine returning statuses and decide what to do.
		if (status_ios != errSecSuccess) {
			// iOS keychain failed to produce results because of some error, go with results from OSX keychain.
            // Since iOS keychain queries will fail without a keychain-access-group or proper entitlements, SecItemCopyMatching
            // calls against the OSX keychain API that should return errSecItemNotFound will return nonsense from the iOS keychain.
			AssignOrReleaseResult(result_osx, result);
            return status_osx;
		} else if (status_osx != errSecSuccess) {
			if (status_osx != errSecItemNotFound) {
				// OSX failed to produce results with some failure mode (else than not_found), but iOS produced results.
				// We have to either return OSX failure result and discard iOS results, or vice versa.  For now, we just
				// ignore OSX error and return just iOS results.
				secitemlog(LOG_NOTICE, "SecItemMergeResults: osx_result=%d, ignoring it, iOS succeeded fine", status_osx);
			}

			// OSX failed to produce results, but we have success from iOS keychain; go with results from iOS keychain.
			AssignOrReleaseResult(result_ios, result);
			return errSecSuccess;
		} else {
			// Both searches succeeded, merge results.
			if (result != NULL) {
				CFTypeID id_osx = (result_osx) ? CFGetTypeID(result_osx) : 0;
				CFTypeID id_ios = (result_ios) ? CFGetTypeID(result_ios) : 0;
				CFTypeID id_array = CFArrayGetTypeID();
				if ((id_osx == id_array) && (id_ios == id_array)) {
					// Fold the arrays into one.
					*result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
					CFArrayAppendArray((CFMutableArrayRef)*result, (CFArrayRef)result_ios,
									   CFRangeMake(0, CFArrayGetCount((CFArrayRef)result_ios)));
					CFArrayAppendArray((CFMutableArrayRef)*result, (CFArrayRef)result_osx,
									   CFRangeMake(0, CFArrayGetCount((CFArrayRef)result_osx)));
				} else {
					// Result type is not an array, so only one match can be returned.
					*result = (id_ios) ? result_ios : result_osx;
					CFRetainSafe(*result);
				}
			}
			CFReleaseSafe(result_osx);
			CFReleaseSafe(result_ios);
			return errSecSuccess;
		}
	} else if (can_target_ios) {
		// Only iOS keychain was targetted.
		AssignOrReleaseResult(result_ios, result);
		return status_ios;
	} else if (can_target_osx) {
		// Only OSX keychain was targetted.
		AssignOrReleaseResult(result_osx, result);
		return status_osx;
	} else {
		// Query could not run at all?
		return errSecParam;
	}
}

static bool
ShouldTryUnlockKeybag(CFDictionaryRef query, OSErr status)
{
    static __typeof(SASSessionStateForUser) *soft_SASSessionStateForUser = NULL;
	static dispatch_once_t onceToken;
	static void *framework;

	if (status != errSecInteractionNotAllowed)
		return false;

    // If the query disabled authUI, respect it.
    CFTypeRef authUI = NULL;
    if (query) {
        authUI = CFDictionaryGetValue(query, kSecUseAuthenticationUI);
        if (authUI == NULL) {
            authUI = CFDictionaryGetValue(query, kSecUseNoAuthenticationUI);
            authUI = (authUI != NULL && CFEqual(authUI, kCFBooleanTrue)) ? kSecUseAuthenticationUIFail : NULL;
        }
    }
    if (authUI && !CFEqual(authUI, kSecUseAuthenticationUIAllow))
        return false;

    dispatch_once(&onceToken, ^{
		framework = dlopen("/System/Library/PrivateFrameworks/login.framework/login", RTLD_LAZY);
		if (framework == NULL)
			return;
		soft_SASSessionStateForUser = (__typeof(soft_SASSessionStateForUser)) dlsym(framework, "SASSessionStateForUser");
    });

    if (soft_SASSessionStateForUser == NULL)
        return false;

    SessionAgentState sessionState = soft_SASSessionStateForUser(getuid());
    if(sessionState != kSA_state_desktopshowing)
        return false;

    return true;
}

OSStatus
SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
{
	secitemlog(LOG_NOTICE, "SecItemCopyMatching");
	if (!query) {
		return errSecParam;
	}
	secitemshow(query, "SecItemCopyMatching query:");

	OSStatus status_osx = errSecItemNotFound, status_ios = errSecItemNotFound;
	CFTypeRef result_osx = NULL, result_ios = NULL;
	bool can_target_ios, can_target_osx;
	OSStatus status = SecItemCategorizeQuery(query, can_target_ios, can_target_osx);
	if (status != errSecSuccess) {
		return status;
	}

	if (can_target_ios) {
		CFDictionaryRef attrs_ios = SecItemCopyTranslatedAttributes(query,
			CFDictionaryGetValue(query, kSecClass), true, false, false, false, true, true);
		if (!attrs_ios) {
			status_ios = errSecParam;
		}
		else {
			status_ios = SecItemCopyMatching_ios(attrs_ios, &result_ios);
            if(ShouldTryUnlockKeybag(query, status_ios)) {
                // The keybag is locked. Attempt to unlock it...
				secitemlog(LOG_WARNING, "SecItemCopyMatching triggering SecurityAgent");
                if(errSecSuccess == SecKeychainVerifyKeyStorePassphrase(1)) {
                    CFReleaseNull(result_ios);
                    status_ios = SecItemCopyMatching_ios(attrs_ios, &result_ios);
                }
            }
			CFRelease(attrs_ios);
		}
		secitemlog(LOG_NOTICE, "SecItemCopyMatching_ios result: %d", status_ios);
	}

	if (can_target_osx) {
		CFDictionaryRef attrs_osx = SecItemCopyTranslatedAttributes(query,
		    CFDictionaryGetValue(query, kSecClass), false, false, true, false, true, true);
		if (!attrs_osx) {
			status_osx = errSecParam;
		}
		else {
			status_osx = SecItemCopyMatching_osx(attrs_osx, &result_osx);
			CFRelease(attrs_osx);
		}
		secitemlog(LOG_NOTICE, "SecItemCopyMatching_osx result: %d", status_osx);
	}

	status = SecItemMergeResults(can_target_ios, status_ios, result_ios,
								 can_target_osx, status_osx, result_osx, result);
	secitemlog(LOG_NOTICE, "SecItemCopyMatching result: %d", status);
	return status;
}

OSStatus
SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
{
	secitemlog(LOG_NOTICE, "SecItemAdd");
	if (!attributes) {
		return errSecParam;
	}
	else if (result) {
		*result = NULL;
	}
	secitemshow(attributes, "SecItemAdd attrs:");

	CFTypeRef result_osx = NULL, result_ios = NULL;
	bool can_target_ios, can_target_osx;
	OSStatus status = SecItemCategorizeQuery(attributes, can_target_ios, can_target_osx);
	if (status != errSecSuccess) {
		return status;
	}

	// SecItemAdd cannot be really done on both keychains.  In order to keep backward compatibility
	// with existing applications, we prefer to add items into legacy keychain and fallback
	// into iOS (modern) keychain only when the query is not suitable for legacy keychain.
	if (!can_target_osx) {
		CFDictionaryRef attrs_ios = SecItemCopyTranslatedAttributes(attributes,
			NULL, true, true, false, false, false, false);
		if (!attrs_ios) {
			status = errSecParam;
		} else {
            status = SecItemAdd_ios(attrs_ios, &result_ios);
            if(ShouldTryUnlockKeybag(attributes, status)) {
                // The keybag is locked. Attempt to unlock it...
				secitemlog(LOG_WARNING, "SecItemAdd triggering SecurityAgent");
                if(errSecSuccess == SecKeychainVerifyKeyStorePassphrase(3)) {
                    CFReleaseNull(result_ios);
                    status = SecItemAdd_ios(attrs_ios, &result_ios);
                }
            }
			CFRelease(attrs_ios);
		}
		secitemlog(LOG_NOTICE, "SecItemAdd_ios result: %d", status);
		AssignOrReleaseResult(result_ios, result);
		return status;
	} else {
		CFDictionaryRef attrs_osx = SecItemCopyTranslatedAttributes(attributes,
		    NULL, false, false, true, false, false, false);
		if (!attrs_osx) {
			status = errSecParam;
		} else {
			status = SecItemAdd_osx(attrs_osx, &result_osx);
			CFRelease(attrs_osx);
		}
		secitemlog(LOG_NOTICE, "SecItemAdd_osx result: %d", status);
		AssignOrReleaseResult(result_osx, result);
		return status;
	}
}

OSStatus
SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
	secitemlog(LOG_NOTICE, "SecItemUpdate");
	if (!query || !attributesToUpdate) {
		return errSecParam;
	}
	secitemshow(query, "SecItemUpdate query:");
	secitemshow(attributesToUpdate, "SecItemUpdate attrs:");

	OSStatus status_osx = errSecItemNotFound, status_ios = errSecItemNotFound;
	bool can_target_ios, can_target_osx;
	OSStatus status = SecItemCategorizeQuery(query, can_target_ios, can_target_osx);
	if (status != errSecSuccess) {
		return status;
	}

	if (can_target_ios) {
		CFDictionaryRef attrs_ios = SecItemCopyTranslatedAttributes(query,
			CFDictionaryGetValue(query, kSecClass), true, true, false, true, true, true);
		if (!attrs_ios) {
			status_ios = errSecParam;
		}
		else {
            if (SecItemHasSynchronizableUpdate(true, attributesToUpdate)) {
                status_ios = SecItemChangeSynchronizability(attrs_ios, attributesToUpdate, false);
                if(ShouldTryUnlockKeybag(query, status_ios)) {
                    // The keybag is locked. Attempt to unlock it...
					secitemlog(LOG_WARNING, "SecItemUpdate triggering SecurityAgent");
                    if(errSecSuccess == SecKeychainVerifyKeyStorePassphrase(1)) {
                        status_ios = SecItemChangeSynchronizability(attrs_ios, attributesToUpdate, false);
                    }
                }
            } else {
                status_ios = SecItemUpdate_ios(attrs_ios, attributesToUpdate);
                if(ShouldTryUnlockKeybag(query, status_ios)) {
                    // The keybag is locked. Attempt to unlock it...
					secitemlog(LOG_WARNING, "SecItemUpdate triggering SecurityAgent");
                    if(errSecSuccess == SecKeychainVerifyKeyStorePassphrase(1)) {
                        status_ios = SecItemUpdate_ios(attrs_ios, attributesToUpdate);
                    }
                }
            }
			CFRelease(attrs_ios);
		}
		secitemlog(LOG_NOTICE, "SecItemUpdate_ios result: %d", status_ios);
	}

	if (can_target_osx) {
		CFDictionaryRef attrs_osx = SecItemCopyTranslatedAttributes(query,
		    CFDictionaryGetValue(query, kSecClass), false, false, true, true, true, true);
		if (!attrs_osx) {
			status_osx = errSecParam;
		}
		else {
			if (SecItemHasSynchronizableUpdate(false, attributesToUpdate))
				status_osx = SecItemChangeSynchronizability(attrs_osx, attributesToUpdate, true);
			else
				status_osx = SecItemUpdate_osx(attrs_osx, attributesToUpdate);

			CFRelease(attrs_osx);
		}
		secitemlog(LOG_NOTICE, "SecItemUpdate_osx result: %d", status_osx);
	}

	status = SecItemMergeResults(can_target_ios, status_ios, NULL,
								 can_target_osx, status_osx, NULL, NULL);
	secitemlog(LOG_NOTICE, "SecItemUpdate result: %d", status);
	return status;
}

OSStatus
SecItemDelete(CFDictionaryRef query)
{
	secitemlog(LOG_NOTICE, "SecItemDelete");
	if (!query) {
		return errSecParam;
	}
	secitemshow(query, "SecItemDelete query:");

	OSStatus status_osx = errSecItemNotFound, status_ios = errSecItemNotFound;
	bool can_target_ios, can_target_osx;
	OSStatus status = SecItemCategorizeQuery(query, can_target_ios, can_target_osx);
	if (status != errSecSuccess) {
		return status;
	}

	if (can_target_ios) {
		CFDictionaryRef attrs_ios = SecItemCopyTranslatedAttributes(query,
			NULL, true, true, false, true, true, true);
		if (!attrs_ios) {
			status_ios = errSecParam;
		} else {
            status_ios = SecItemDelete_ios(attrs_ios);
			CFRelease(attrs_ios);
		}
		secitemlog(LOG_NOTICE, "SecItemDelete_ios result: %d", status_ios);
	}

	if (can_target_osx) {
		CFDictionaryRef attrs_osx = SecItemCopyTranslatedAttributes(query,
		    NULL, false, false, true, true, true, true);
		if (!attrs_osx) {
			status_osx = errSecParam;
		} else {
			status_osx = SecItemDelete_osx(attrs_osx);
			CFRelease(attrs_osx);
		}
		secitemlog(LOG_NOTICE, "SecItemDelete_osx result: %d", status_osx);
	}

	status = SecItemMergeResults(can_target_ios, status_ios, NULL,
								 can_target_osx, status_osx, NULL, NULL);
	secitemlog(LOG_NOTICE, "SecItemCopyDelete result: %d", status);
	return status;
}

OSStatus
SecItemUpdateTokenItems(CFTypeRef tokenID, CFArrayRef tokenItemsAttributes)
{
	OSStatus status = SecItemUpdateTokenItems_ios(tokenID, tokenItemsAttributes);
    if(ShouldTryUnlockKeybag(NULL, status)) {
        // The keybag is locked. Attempt to unlock it...
        if(errSecSuccess == SecKeychainVerifyKeyStorePassphrase(1)) {
			secitemlog(LOG_WARNING, "SecItemUpdateTokenItems triggering SecurityAgent");
            status = SecItemUpdateTokenItems_ios(tokenID, tokenItemsAttributes);
        }
    }
	secitemlog(LOG_NOTICE, "SecItemUpdateTokenItems_ios result: %d", status);
	return status;
}

OSStatus
SecItemCopyMatching_osx(
	CFDictionaryRef query,
	CFTypeRef *result)
{
	if (!query || !result)
		return errSecParam;
	else
		*result = NULL;

	CFAllocatorRef allocator = CFGetAllocator(query);
	CFIndex matchCount = 0;
	CFMutableArrayRef itemArray = NULL;
	SecKeychainItemRef item = NULL;
	SecIdentityRef identity = NULL;
	OSStatus tmpStatus, status = errSecSuccess;

	// validate input query parameters and create the search reference
	SecItemParams *itemParams = _CreateSecItemParamsFromDictionary(query, &status);
	require_action(itemParams != NULL, error_exit, itemParams = NULL);

	// find the next match until we hit maxMatches, or no more matches found
	while ( !(!itemParams->returnAllMatches && matchCount >= itemParams->maxMatches) &&
			SecItemSearchCopyNext(itemParams, (CFTypeRef*)&item) == errSecSuccess) {

		if (FilterCandidateItem((CFTypeRef*)&item, itemParams, &identity))
			continue; // move on to next item

		++matchCount; // we have a match

		tmpStatus = AddItemResults(item, identity, itemParams, allocator, &itemArray, result);
		if (tmpStatus && (status == errSecSuccess))
			status = tmpStatus;

		if (item) {
			CFRelease(item);
			item = NULL;
		}
		if (identity) {
			CFRelease(identity);
			identity = NULL;
		}
	}

	if (status == errSecSuccess)
		status = (matchCount > 0) ? errSecSuccess : errSecItemNotFound;

error_exit:
	if (status != errSecSuccess && result != NULL && *result != NULL) {
		CFRelease(*result);
		*result = NULL;
	}
	_FreeSecItemParams(itemParams);

	return status;
}

OSStatus
SecItemCopyDisplayNames(
	CFArrayRef items,
	CFArrayRef *displayNames)
{
    BEGIN_SECAPI
	Required(items);
	Required(displayNames);
    //%%%TBI
    return errSecUnimplemented;
    END_SECAPI
}

OSStatus
SecItemAdd_osx(
	CFDictionaryRef attributes,
	CFTypeRef *result)
{
	if (!attributes)
		return errSecParam;
	else if (result)
		*result = NULL;

	CFAllocatorRef allocator = CFGetAllocator(attributes);
	CFMutableArrayRef itemArray = NULL;
	SecKeychainItemRef item = NULL;
	OSStatus tmpStatus, status = errSecSuccess;

	// validate input attribute parameters
	SecItemParams *itemParams = _CreateSecItemParamsFromDictionary(attributes, &status);
	require_action(itemParams != NULL, error_exit, itemParams = NULL);

	// currently, we don't support adding SecIdentityRef items (an aggregate item class),
	// since the private key should already be in a keychain by definition. We could support
	// this as a copy operation for the private key if a different keychain is specified,
	// but in any case it should try to add the certificate. See <rdar://8317887>.
	require_action(!itemParams->returnIdentity, error_exit, status = errSecItemInvalidValue);

	if (itemParams->useItems == NULL) {

		require_action(itemParams->itemData == NULL || CFGetTypeID(itemParams->itemData) == CFDataGetTypeID(),
					   error_exit, status = errSecItemInvalidValue);

		// create a single keychain item specified by the input attributes
		status = SecKeychainItemCreateFromContent(itemParams->itemClass,
			itemParams->attrList,
			(itemParams->itemData) ? (UInt32)CFDataGetLength(itemParams->itemData) : 0,
			(itemParams->itemData) ? CFDataGetBytePtrVoid(itemParams->itemData) : NULL,
			itemParams->keychain,
			itemParams->access,
			&item);
		require_noerr(status, error_exit);

		// return results (if requested)
		if (result) {
			itemParams->maxMatches = 1; // in case kSecMatchLimit was set to > 1
			tmpStatus = AddItemResults(item, NULL, itemParams, allocator, &itemArray, result);
			if (tmpStatus && (status == errSecSuccess))
				status = tmpStatus;
		}
		CFRelease(item);
	}
	else {
		// add multiple items which are specified in the itemParams->useItems array.
		// -- SecCertificateRef or SecKeyRef items may or may not be in a keychain.
		// -- SecKeychainItemRef items are in a keychain (by definition), but may be copied to another keychain.
		// -- CFDataRef items are a persistent reference; the represented item may be copied to another keychain.
		//
		OSStatus aggregateStatus = errSecSuccess;
		CFIndex ix, count = CFArrayGetCount(itemParams->useItems);
		itemParams->maxMatches = (count > 1) ? (int)count : 2; // force results to always be returned as an array
		for (ix=0; ix < count; ix++) {
			CFTypeRef anItem = (CFTypeRef) CFArrayGetValueAtIndex(itemParams->useItems, ix);
			if (anItem) {
				if (SecCertificateGetTypeID() == CFGetTypeID(anItem)) {
					// SecCertificateRef item
					tmpStatus = SecCertificateAddToKeychain((SecCertificateRef)anItem, itemParams->keychain);
					if (!tmpStatus && result) {
						tmpStatus = AddItemResults((SecKeychainItemRef)anItem, NULL, itemParams, allocator, &itemArray, result);
					}
					aggregateStatus = _UpdateAggregateStatus(tmpStatus, aggregateStatus, errSecDuplicateItem);
				}
				else if (SecKeyGetTypeID() == CFGetTypeID(anItem)) {
					// SecKeyRef item
					SecKeychainRef itemKeychain = NULL;
					tmpStatus = SecKeychainItemCopyKeychain((SecKeychainItemRef)anItem, &itemKeychain);
					if (tmpStatus == errSecSuccess) {
						// key was in a keychain, so we can attempt to copy it
						SecKeychainItemRef itemCopy = NULL;
						tmpStatus = SecKeychainItemCreateCopy((SecKeychainItemRef)anItem, itemParams->keychain, itemParams->access, &itemCopy);
						if (!tmpStatus && result) {
							tmpStatus = AddItemResults(itemCopy, NULL, itemParams, allocator, &itemArray, result);
						}
						if (itemCopy) {
							CFRelease(itemCopy);
						}
					}
					else {
						// key was not in any keychain, so must be imported
						SecKeychainItemRef keyItem = NULL;
						tmpStatus = _ImportKey((SecKeyRef)anItem, itemParams->keychain, itemParams->access, itemParams->attrList, &keyItem);
						if (!tmpStatus && result) {
							tmpStatus = AddItemResults(keyItem, NULL, itemParams, allocator, &itemArray, result);
						}
						if (keyItem) {
							CFRelease(keyItem);
						}
					}
					if (itemKeychain) {
						CFRelease(itemKeychain);
					}
					aggregateStatus = _UpdateAggregateStatus(tmpStatus, aggregateStatus, errSecDuplicateItem);
				}
				else if (SecKeychainItemGetTypeID() == CFGetTypeID(anItem)) {
					// SecKeychainItemRef item
					SecKeychainItemRef itemCopy = NULL;
					tmpStatus = SecKeychainItemCreateCopy((SecKeychainItemRef)anItem, itemParams->keychain, itemParams->access, &itemCopy);
					if (!tmpStatus && result) {
						tmpStatus = AddItemResults(itemCopy, NULL, itemParams, allocator, &itemArray, result);
					}
					if (itemCopy) {
						CFRelease(itemCopy);
					}
					aggregateStatus = _UpdateAggregateStatus(tmpStatus, aggregateStatus, errSecDuplicateItem);
				}
				else if (CFDataGetTypeID() == CFGetTypeID(anItem)) {
					// CFDataRef item (persistent reference)
					SecKeychainItemRef realItem = NULL;
					tmpStatus = SecKeychainItemCopyFromPersistentReference((CFDataRef)anItem, &realItem);
					if (tmpStatus == errSecSuccess) {
						// persistent reference resolved to a keychain item, so we can attempt to copy it
						SecKeychainItemRef itemCopy = NULL;
						tmpStatus = SecKeychainItemCreateCopy(realItem, itemParams->keychain, itemParams->access, &itemCopy);
						if (!tmpStatus && result) {
							tmpStatus = AddItemResults(itemCopy, NULL, itemParams, allocator, &itemArray, result);
						}
						if (itemCopy) {
							CFRelease(itemCopy);
						}
					}
					if (realItem) {
						CFRelease(realItem);
					}
					aggregateStatus = _UpdateAggregateStatus(tmpStatus, aggregateStatus, errSecDuplicateItem);
				}
			}
		} // end of itemList array loop
		status = aggregateStatus;
	} // end processing multiple items

error_exit:
	if (status != errSecSuccess && result != NULL && *result != NULL) {
		CFRelease(*result);
		*result = NULL;
	}
	_FreeSecItemParams(itemParams);

	return status;
}

OSStatus
SecItemUpdate_osx(
	CFDictionaryRef query,
	CFDictionaryRef attributesToUpdate)
{
	if (!query || !attributesToUpdate)
		return errSecParam;

	// run the provided query to get a list of items to update
	CFTypeRef results = NULL;
	OSStatus status = SecItemCopyMatching_osx(query, &results);
	if (status != errSecSuccess)
		return status; // nothing was matched, or the query was bad

	CFArrayRef items = NULL;
	if (CFArrayGetTypeID() == CFGetTypeID(results)) {
		items = (CFArrayRef) results;
	}
	else {
		items = CFArrayCreate(NULL, &results, 1, &kCFTypeArrayCallBacks);
		CFRelease(results);
	}

	OSStatus result = errSecSuccess;
	CFIndex ix, count = CFArrayGetCount(items);
	for (ix=0; ix < count; ix++) {
		CFTypeRef anItem = (CFTypeRef) CFArrayGetValueAtIndex(items, ix);
		if (anItem) {
			status = _UpdateKeychainItem(anItem, attributesToUpdate);
			result = _UpdateAggregateStatus(status, result, errSecSuccess);
		}
	}

	if (items) {
		CFRelease(items);
	}
	return result;
}

OSStatus
SecItemDelete_osx(
	CFDictionaryRef query)
{
	if (!query)
		return errSecParam;

	// run the provided query to get a list of items to delete
	CFTypeRef results = NULL;
	OSStatus status = SecItemCopyMatching_osx(query, &results);
	if (status != errSecSuccess)
		return status; // nothing was matched, or the query was bad

	CFArrayRef items = NULL;
	if (CFArrayGetTypeID() == CFGetTypeID(results)) {
		items = (CFArrayRef) results;
	}
	else {
		items = CFArrayCreate(NULL, &results, 1, &kCFTypeArrayCallBacks);
		CFRelease(results);
	}

	OSStatus result = errSecSuccess;
	CFIndex ix, count = CFArrayGetCount(items);
	for (ix=0; ix < count; ix++) {
		CFTypeRef anItem = (CFTypeRef) CFArrayGetValueAtIndex(items, ix);
		if (anItem) {
			if (SecIdentityGetTypeID() == CFGetTypeID(anItem)) {
				status = _DeleteIdentity((SecIdentityRef)anItem);
			}
			else {
				status = _DeleteKeychainItem(anItem);
			}
			result = _UpdateAggregateStatus(status, result, errSecSuccess);
		}
	}

	if (items)
		CFRelease(items);

	return result;
}

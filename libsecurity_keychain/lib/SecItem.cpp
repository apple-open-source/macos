/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecCertificate.h>
#include <sys/param.h>
#include "cssmdatetime.h"
#include "SecItem.h"
#include "SecItemPriv.h"
#include "SecIdentitySearchPriv.h"
#include "SecCertificatePriv.h"
#include "SecCertificatePrivP.h"

#include <AssertMacros.h>

#define CFDataGetBytePtrVoid CFDataGetBytePtr

#pragma mark SecItem private utility functions

/******************************************************************************/

struct ProtocolAttributeInfo {
	const CFTypeRef *protocolValue;
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
	const CFTypeRef *authValue;
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
	const CFTypeRef *keyType;
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
	{ &kSecAttrKeyTypeECDSA, CSSM_ALGID_ECDSA }
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
	const CFTypeRef *newItemType;
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
    { kSecKeyLabel, &kSecAttrApplicationLabel, kStringRepresentation }, // this contains the hash of the key (or the public key hash, if asymmetric)
    { kSecKeyApplicationTag, &kSecAttrApplicationTag, kStringRepresentation },
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
				length = strlen(buffer);
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
			if (CFDataGetTypeID() != CFGetTypeID(value)) {
				length = 0;
				return NULL;
			}
			length = CFDataGetLength((CFDataRef) value);
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
			length = strlen(buffer);
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
	int itemsInDictionary = CFDictionaryGetCount(dictionaryRef);
	CFTypeRef keys[itemsInDictionary];
	CFTypeRef values[itemsInDictionary];

	CFTypeRef *keysPtr = keys;
	CFTypeRef *valuesPtr = values;
	
	CFDictionaryGetKeysAndValues(dictionaryRef, keys, values);
	
	// count the number of items we are interested in
	int count = 0;
	int i;
	
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
	attrList->count = count;
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
	
	return noErr;
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
	if (result != noErr)
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
							stringRef = (CFStringRef) kSecAttrKeyTypeECDSA;
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
	if (result == noErr) // did we complete OK
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
	if (result == noErr) // did we complete OK
	{
		CFDictionaryAddValue(dict, kSecClass, kSecClassCertificate);
	}
	
	*dictionary = dict;
	
	return noErr;
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
	if (result == noErr) // did we complete OK
	{
		CFDictionaryAddValue(dict, kSecClass, kSecClassKey);
	}

	*dictionary = dict;

	return noErr;
#endif
	
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	unsigned int ix;
	SecItemClass itemClass = 0;
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
    case kSecAppleSharePasswordItemClass:
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
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%d"), keyRecordValue);
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
							stringRef = (CFStringRef) kSecAttrKeyTypeECDSA;
							break;
						default :
							stringRef = CFStringCreateWithFormat(allocator, NULL, CFSTR("%d"), keyAlgValue);
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
		values[numValues] = _SecAttrAuthenticationTypeForSecAuthenticationType(*(SecProtocolType*)attrList.attr[6].data);
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
	return paramErr;
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
	OSStatus status;
	CFRange range;
	
	status = noErr;
	
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
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
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
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
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
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
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
		status = _CFStringCreateAttribute((CFStringRef)value, kSecKeyLabel, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);
		
		++attrListPtr->count;
	}
	
	// [4] get the kSecKeyApplicationTag string
	if (CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrApplicationTag, (const void **)&value) && value) {
		status = _CFStringCreateAttribute((CFStringRef)value, kSecKeyApplicationTag, &attrListPtr->attr[attrListPtr->count]);
		require_noerr_quiet(status, CFStringCreateAttribute_failed);
		
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
	
	return ( noErr );
	
	/***************/
	
malloc_number_failed:
CFStringCreateAttribute_failed:
malloc_attrPtr_failed:
	
	// free any attributes
	_FreeAttrList(attrListPtr);
	
calloc_attrListPtr_failed:
	
	return ( errSecBufferTooSmall );

#endif
}


/*
 * _CreateSecKeychainInternetPasswordAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
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
		
		attrListPtr->attr[attrListPtr->count].tag = kSecPortItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt16);
		CFNumberGetValue((CFNumberRef)value, kCFNumberSInt16Type, attrListPtr->attr[attrListPtr->count].data);
		
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
		
		attrListPtr->attr[attrListPtr->count].tag = kSecCreatorItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);
		
		++attrListPtr->count;
	}

	// [11] get the type code
	if ( CFDictionaryGetValueIfPresent(attrDictionary, kSecAttrType, (const void **)&value) ) {
		attrListPtr->attr[attrListPtr->count].data = malloc(sizeof(UInt32));
		require_action(attrListPtr->attr[attrListPtr->count].data != NULL, malloc_port_failed, status = errSecBufferTooSmall);
		
		attrListPtr->attr[attrListPtr->count].tag = kSecTypeItemAttr;
		attrListPtr->attr[attrListPtr->count].length = sizeof(UInt32);
		CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, attrListPtr->attr[attrListPtr->count].data);
		
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
	
	return ( noErr );
	
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
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
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
	return paramErr;
}


/*
 * _AppNameFromSecTrustedApplication attempts to pull the name of the
 * application/tool from the SecTrustedApplicationRef.
 */
static CFStringRef
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
	if ( status == noErr ) {
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
					// find last slash and adjust nameRange be everything after it
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
	if (status != noErr) {
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
	SecAccessRef access;
	CFArrayRef aclList;
	SecACLRef acl;
	CFArrayRef appList;
	CFStringRef description;
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
	
	SecItemClass itemClass;
	status = SecKeychainItemCopyAttributesAndData(itemRef, NULL, &itemClass, NULL, NULL, NULL);
	if (!status && (itemClass == kSecCertificateItemClass || itemClass == kSecPublicKeyItemClass)) {
		// the item doesn't have any access controls, so delete it normally
		return SecKeychainItemDelete(itemRef);
	}

	// copy the access of the keychain item
	status = SecKeychainItemCopyAccess(itemRef, &access);
	require_noerr(status, SecKeychainItemCopyAccessFailed);
	
	// copy the decrypt access control lists -- this is what has access to the keychain item
	status = SecAccessCopySelectedACLList(access, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
	require_noerr(status, SecAccessCopySelectedACLListFailed);
	require_quiet(aclList != NULL, noACLList);
	
	// get the access control list
	acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
	require_quiet(acl != NULL, noACL);
	
	// copy the application list, description, and CSSM prompt selector for a given access control list entry 
	status = SecACLCopySimpleContents(acl, &appList, &description, &promptSelector);
	require_noerr(status, SecACLCopySimpleContentsFailed);
	require_quiet(appList != NULL, noAppList);
	
	// does only a single application/tool have decrypt access to this item? 
	if ( CFArrayGetCount(appList) == 1 ) {
		SecTrustedApplicationRef itemAppRef, currentAppRef;
		CFStringRef itemAppName, currentAppName;
		
		// get SecTrustedApplicationRef for item's application/tool
		itemAppRef = (SecTrustedApplicationRef)CFArrayGetValueAtIndex(appList, 0);
		require(itemAppRef != NULL, noItemAppRef);
		
		// copy the name out
		itemAppName = _AppNameFromSecTrustedApplication(CFGetAllocator(itemRef), itemAppRef);
		require(itemAppName != NULL, noAppName);
		
		// create SecTrustedApplicationRef for current application/tool
		status = SecTrustedApplicationCreateFromPath(NULL, &currentAppRef);
		require((status == noErr) && (currentAppRef != NULL), SecTrustedApplicationCreateFromPathFailed);
		
		// copy the name out
		currentAppName = _AppNameFromSecTrustedApplication(CFGetAllocator(itemRef), currentAppRef);
		require(currentAppName != NULL, noCurrentAppName);
		
		// compare the current application/tool's name to this item's application/tool's name to see if we own the decrypt access
		if ( CFStringCompare(currentAppName, itemAppName, 0) == kCFCompareEqualTo ) {
			// delete the keychain item
			SecKeychainItemDelete(itemRef);
		}
		
		CFRelease(currentAppName);
	noCurrentAppName:
		CFRelease(currentAppRef);
	SecTrustedApplicationCreateFromPathFailed:
		CFRelease(itemAppName);
	noAppName:	
	noItemAppRef:
		;
	}
	
	if ( description ) {
		CFRelease(description);
	}
	CFRelease(appList);
noAppList:
SecACLCopySimpleContentsFailed:
noACL:
	CFRelease(aclList);
noACLList:
SecAccessCopySelectedACLListFailed:
	CFRelease(access);
SecKeychainItemCopyAccessFailed:
	
	return status;
}

OSStatus
_UpdateKeychainItem(CFTypeRef item, CFDictionaryRef changedAttributes)
{
	// This function updates a single keychain item, which may be specified as
	// a reference, persistent reference or attribute dictionary, with the
	// attributes provided.

	OSStatus status = noErr;
	if (!item) {
		return paramErr;
	}

	SecItemClass itemClass;
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
				(theData != NULL) ? CFDataGetLength(theData) : 0,
				(theData != NULL) ? CFDataGetBytePtrVoid(theData) : NULL);
	
	// one more thing... update access?
	if (CFDictionaryGetValueIfPresent(changedAttributes, kSecAttrAccess, (const void **)&access)) {
		status = SecKeychainItemSetAccess(itemToUpdate, access);
	}

update_failed:
	if (itemToUpdate)
		CFRelease(itemToUpdate);
	_FreeAttrList(changeAttrList);
	return status;
}

OSStatus
_DeleteKeychainItem(CFTypeRef item)
{
	// This function deletes a single keychain item, which may be specified as
	// a reference, persistent reference or attribute dictionary. It will not
	// delete non-keychain items or aggregate items (such as a SecIdentityRef);
	// it is assumed that the caller will pass identity components separately.

	OSStatus status = noErr;
	if (!item) {
		return paramErr;
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

OSStatus
_DeleteIdentity(SecIdentityRef identity)
{
	OSStatus status, result = noErr;
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

OSStatus
_UpdateAggregateStatus(OSStatus newStatus, OSStatus curStatus, OSStatus baseStatus)
{
	// This function is used when atomically processing multiple items,
	// where an overall error result must be returned for the entire operation.
	// When newStatus is something other than noErr, we want to keep the "most
	// interesting" status (which usually will be newStatus, unless curStatus is
	// already set; in that case, newStatus can trump curStatus only by being
	// something different than baseStatus.)

	OSStatus result = curStatus;

	if (newStatus != noErr) {
		result = newStatus;
		if (curStatus != noErr) {
			result = (newStatus != baseStatus) ? newStatus : curStatus;
		}
	}
	return result;
}

void
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
	SecKeychainAttributeList *attrList;	// attribute list for this query
	SecAccessRef access;				// access reference (for SecItemAdd only, not used to find items)
	CFDataRef itemData;					// item data (for SecItemAdd only, not used to find items)
};

static OSStatus
_ValidateDictionaryEntry(CFDictionaryRef dict, CFTypeRef key, const void **value, CFTypeID expectedTypeID, CFTypeID altTypeID)
{
	if (!dict || !key || !value || !expectedTypeID)
		return paramErr;
	
	if (!CFDictionaryGetValueIfPresent(dict, key, value)) {
		// value was not provided for this key (not an error!)
		*value = NULL; 
	}
	else if (!(*value)) {
		// provided value is NULL (also not an error!)
		return noErr;
	}
	else if (!((expectedTypeID == CFGetTypeID(*value)) || (altTypeID && altTypeID == CFGetTypeID(*value)))) {
		// provided value does not have the expected (or alternate) CF type ID
		return errSecItemInvalidValue; 
	}
	else {
		// provided value is OK; retain it
		CFRetain(*value);
	}

	return noErr;
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

	_FreeAttrList(itemParams->attrList);

	free(itemParams);
}

static SecItemParams*
_CreateSecItemParamsFromDictionary(CFDictionaryRef dict, OSStatus *error)
{
	OSStatus status;
	CFTypeRef value = NULL;
	SecItemParams *itemParams = (SecItemParams *) malloc(sizeof(SecItemParams));

	require_action(itemParams != NULL, error_exit, status = memFullErr);
	require_action(dict && (CFDictionaryGetTypeID() == CFGetTypeID(dict)), error_exit, status = paramErr);

	memset(itemParams, 0, sizeof(SecItemParams));
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

	// must have an item class, unless we have an item list to add
	if (!CFDictionaryGetValueIfPresent(dict, kSecClass, (const void**) &value) && !itemParams->useItems)
		require_action(false, error_exit, status = errSecItemClassMissing);
	else if (value) {
		itemParams->itemClass = _ConvertItemClass(value, itemParams->keyClass, &itemParams->returnIdentity);
		if (itemParams->itemClass == kSecSymmetricKeyItemClass && !itemParams->keyClass) {
			itemParams->assumedKeyClass = kSecAttrKeyClassSymmetric; // no key class specified, so start with symmetric key class; will search the others later
		}
		require_action(!(itemParams->itemClass == 0), error_exit, status = errSecItemClassMissing);
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

	// validate the payload (password, key or certificate data), used for SecItemAdd but not for finding items
	require_noerr(status = _ValidateDictionaryEntry(dict, kSecValueData, (const void **)&itemParams->itemData, CFDataGetTypeID(), CFStringGetTypeID()), error_exit);

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

	// if useItems was provided, we don't need an attribute list or a search reference for adding, although we definitely need one for searching
	if (itemParams->useItems && itemParams->itemClass == 0) {
		require_action(false, error_exit, status = noErr); // all done here
	}

	// build a SecKeychainAttributeList from the query dictionary for the specified item class
	require_noerr(status = _CreateSecKeychainAttributeListFromDictionary(dict, itemParams->itemClass, &itemParams->attrList), error_exit);
	
	// create a search reference (either a SecKeychainSearchRef or a SecIdentitySearchRef)
	if ((itemParams->itemClass == kSecCertificateItemClass) && itemParams->emailAddrToMatch) {
		// searching for certificates by email address
		char *nameBuf = (char*)malloc(MAXPATHLEN);
		if (!nameBuf) {
			status = memFullErr;
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


OSStatus
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


OSStatus
_FilterWithPolicy(SecPolicyRef policy, SecCertificateRef cert)
{
	CFArrayRef keychains = NULL;
	CFArrayRef anchors = NULL;
	CFArrayRef certs = NULL;
	CFArrayRef chain = NULL;
	SecTrustRef trust = NULL;
	
	SecTrustResultType	trustResult;
	CSSM_TP_APPLE_EVIDENCE_INFO *evidence = NULL;
	OSStatus status;
	if (!policy || !cert) return paramErr;

	certs = CFArrayCreate(NULL, (const void **)&cert, (CFIndex)1, &kCFTypeArrayCallBacks);
	status = SecTrustCreateWithCertificates(certs, policy, &trust); 
	if(status) goto cleanup;

	/* To make the evaluation as lightweight as possible, specify an empty array
	 * of keychains which will be searched for certificates.
	 */
	keychains = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	status = SecTrustSetKeychains(trust, keychains);
	if(status) goto cleanup;
	
	/* To make the evaluation as lightweight as possible, specify an empty array
	 * of trusted anchors.
	 */
	anchors = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	status = SecTrustSetAnchorCertificates(trust, anchors);
	if(status) goto cleanup;
	
	/* All parameters are locked and loaded, ready to evaluate! */
	status = SecTrustEvaluate(trust, &trustResult);
	if(status) goto cleanup;
	
	/* Because we didn't provide trust anchors or a way to look for them,
	 * the evaluation will fail with kSecTrustResultRecoverableTrustFailure.
	 * However, we can tell whether the policy evaluation succeeded by
	 * looking at the per-cert status codes in the returned evidence.
	 */
	status = SecTrustGetResult(trust, &trustResult, &chain, &evidence);
	if(status) goto cleanup;

	/* If there are no per-cert policy status codes,
	 * and the cert has not expired, consider it valid.
	 */
	if((evidence != NULL) && (evidence[0].NumStatusCodes == 0) &&
	   ((evidence[0].StatusBits & CSSM_CERT_STATUS_EXPIRED) == 0) &&
	   ((evidence[0].StatusBits & CSSM_CERT_STATUS_NOT_VALID_YET) == 0)) {
		status = noErr;
	}
	else {
		status = errSecCertificateCannotOperate;
	}

cleanup:
	if(chain) CFRelease(chain);
	if(anchors) CFRelease(anchors);
	if(keychains) CFRelease(keychains);
	if(certs) CFRelease(certs);
	if(trust) CFRelease(trust);
	
	return status;
}

OSStatus
_FilterWithDate(CFTypeRef validOnDate, SecCertificateRef cert)
{
	if (!validOnDate || !cert) return paramErr;
	
	CFAbsoluteTime at, nb, na;
	if (CFGetTypeID(validOnDate) == CFDateGetTypeID())
		at = CFDateGetAbsoluteTime((CFDateRef)validOnDate);
	else
		at = CFAbsoluteTimeGetCurrent();

	OSStatus status = noErr;
	SecCertificateRefP certP = NULL;
	CFDataRef certData = SecCertificateCopyData(cert);
	if (certData) {
		certP = SecCertificateCreateWithDataP(kCFAllocatorDefault, certData);
	}
	if (certP) {
		nb = SecCertificateNotValidBefore(certP);
		na = SecCertificateNotValidAfter(certP);
		
		if(at < nb)
			status = errSecCertificateNotValidYet;
		else if (at > na)
			status = errSecCertificateExpired;
	}
	else {
		status = errSecCertificateCannotOperate;
	}

	if(certData) CFRelease(certData);
	if(certP) CFRelease(certP);
	return status;
}

OSStatus
_FilterWithTrust(Boolean trustedOnly, SecCertificateRef cert)
{
	if (!cert) return paramErr;
	if (!trustedOnly) return noErr;
	
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

OSStatus
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
	if (_CreateSecKeychainAttributeListFromDictionary(dict, params->itemClass, &params->attrList) == noErr) {
		// Create a new search reference for the new attribute list.
		if (SecKeychainSearchCreateFromAttributes(params->searchList,
			params->itemClass,
			(params->attrList->count == 0) ? NULL : params->attrList,
			(SecKeychainSearchRef*)&params->search) == noErr) {
			// Return the first matching item from the new search.
			// We won't come back here again until there are no more matching items for this search.
			status = SecKeychainSearchCopyNext((SecKeychainSearchRef)params->search, (SecKeychainItemRef*)item);
		}
	}
	return status;
}


OSStatus
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
	else {
		status = errSecItemNotFound;
	}
	return status;
}

OSStatus
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
			status = _FilterWithPolicy(itemParams->policy, (SecCertificateRef) *item);
			if (status) goto filterOut;
			// certificate item is valid for specified policy
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
				noErr == SecKeychainItemCopyFromPersistentReference((CFDataRef)anItem, &realItem)) {
				anItem = realItem;
			}
			if (SecIdentityGetTypeID() == CFGetTypeID(anItem) &&
				noErr == SecIdentityCopyCertificate((SecIdentityRef)anItem, &aCert)) {
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
	return noErr;

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

OSStatus
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
		return paramErr;
	
	if (itemParams->maxMatches > 1) {
		// if we can return more than one item, we must have an array
		if (!items)
			return paramErr;
		else if (*items == NULL)
			*items = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
	}

	OSStatus tmpStatus, status = noErr;
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
		tmpStatus = SecKeychainItemCreatePersistentReference(item, &persistentRef);
		if (tmpStatus == noErr) {
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
		else if (status == noErr) {
			status = tmpStatus;
		}
	}

	if (itemParams->returningData) {
		UInt32 length;
		void *data;
		tmpStatus = SecKeychainItemCopyContent(item, NULL, NULL, &length, &data);
		if (tmpStatus == noErr) {
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
		else if (status == noErr) {
			status = tmpStatus;
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
		if (tmpStatus && (status == noErr)) {
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


/******************************************************************************/
#pragma mark SecItem API functions
/******************************************************************************/

OSStatus
SecItemCopyMatching(
	CFDictionaryRef query,
	CFTypeRef *result)
{
	if (!query || !result)
		return paramErr;
	else
		*result = NULL;

	CFAllocatorRef allocator = CFGetAllocator(query);
	CFIndex matchCount = 0;
	CFMutableArrayRef itemArray = NULL;
	SecKeychainItemRef item = NULL;
	SecIdentityRef identity = NULL;
	OSStatus tmpStatus, status = noErr;

	// validate input query parameters and create the search reference
	SecItemParams *itemParams = _CreateSecItemParamsFromDictionary(query, &status);
	require_action(itemParams != NULL, error_exit, itemParams = NULL);

	// find the next match until we hit maxMatches, or no more matches found
	while ( !(!itemParams->returnAllMatches && matchCount >= itemParams->maxMatches) &&
			SecItemSearchCopyNext(itemParams, (CFTypeRef*)&item) == noErr) {
		
		if (FilterCandidateItem((CFTypeRef*)&item, itemParams, &identity))
			continue; // move on to next item
		
		++matchCount; // we have a match
		
		tmpStatus = AddItemResults(item, identity, itemParams, allocator, &itemArray, result);
		if (tmpStatus && (status == noErr))
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
	
	if (status == noErr)
		status = (matchCount > 0) ? errSecSuccess : errSecItemNotFound;

error_exit:
	if (status != noErr && result != NULL && *result != NULL) {
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
    return unimpErr;
    END_SECAPI
}

OSStatus
SecItemAdd(
	CFDictionaryRef attributes,
	CFTypeRef *result)
{
	if (!attributes)
		return paramErr;
	else if (result)
		*result = NULL;

	CFAllocatorRef allocator = CFGetAllocator(attributes);
	CFMutableArrayRef itemArray = NULL;
	SecKeychainItemRef item = NULL;
	OSStatus tmpStatus, status = noErr;

	// validate input attribute parameters
	SecItemParams *itemParams = _CreateSecItemParamsFromDictionary(attributes, &status);
	require_action(itemParams != NULL, error_exit, itemParams = NULL);

	// currently, we don't support adding SecIdentityRef items (an aggregate item class),
	// since the private key should already be in a keychain by definition. We could support
	// this as a copy operation for the private key if a different keychain is specified,
	// but in any case it should try to add the certificate. See <rdar://8317887>.
	require_action(!itemParams->returnIdentity, error_exit, status = errSecItemInvalidValue);
	
	if (!itemParams->useItems) {
		// create a single keychain item specified by the input attributes
		status = SecKeychainItemCreateFromContent(itemParams->itemClass,
			itemParams->attrList,
			(itemParams->itemData) ? CFDataGetLength(itemParams->itemData) : 0,
			(itemParams->itemData) ? CFDataGetBytePtrVoid(itemParams->itemData) : NULL,
			itemParams->keychain,
			itemParams->access,
			&item);
		require_noerr(status, error_exit);

		// return results (if requested)
		if (result) {
			itemParams->maxMatches = 1; // in case kSecMatchLimit was set to > 1
			tmpStatus = AddItemResults(item, NULL, itemParams, allocator, &itemArray, result);
			if (tmpStatus && (status == noErr))
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
		OSStatus aggregateStatus = noErr;
		CFIndex ix, count = CFArrayGetCount(itemParams->useItems);
		itemParams->maxMatches = (count > 1) ? count : 2; // force results to always be returned as an array
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
					if (tmpStatus == noErr) {
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
					if (tmpStatus == noErr) {
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
	if (status != noErr && result != NULL && *result != NULL) {
		CFRelease(*result);
		*result = NULL;
	}
	_FreeSecItemParams(itemParams);
	
	return status;
}

OSStatus
SecItemUpdate(
	CFDictionaryRef query,
	CFDictionaryRef attributesToUpdate)
{
	if (!query || !attributesToUpdate)
		return paramErr;

	// run the provided query to get a list of items to update
	CFTypeRef results = NULL;
	OSStatus status = SecItemCopyMatching(query, &results);
	if (status != noErr)
		return status; // nothing was matched, or the query was bad

	CFArrayRef items = NULL;
	if (CFArrayGetTypeID() == CFGetTypeID(results)) {
		items = (CFArrayRef) results;
	}
	else {
		items = CFArrayCreate(NULL, &results, 1, &kCFTypeArrayCallBacks);
		CFRelease(results);
	}

	OSStatus result = noErr;
	CFIndex ix, count = CFArrayGetCount(items);
	for (ix=0; ix < count; ix++) {
		CFTypeRef anItem = (CFTypeRef) CFArrayGetValueAtIndex(items, ix);
		if (anItem) {
			status = _UpdateKeychainItem(anItem, attributesToUpdate);
			result = _UpdateAggregateStatus(status, result, noErr);
		}
	}

	if (items) {
		CFRelease(items);
	}
	return result;
}

OSStatus
SecItemDelete(
	CFDictionaryRef query)
{
	if (!query)
		return paramErr;

	// run the provided query to get a list of items to delete
	CFTypeRef results = NULL;
	OSStatus status = SecItemCopyMatching(query, &results);
	if (status != noErr)
		return status; // nothing was matched, or the query was bad

	CFArrayRef items = NULL;
	if (CFArrayGetTypeID() == CFGetTypeID(results)) {
		items = (CFArrayRef) results;
	}
	else {
		items = CFArrayCreate(NULL, &results, 1, &kCFTypeArrayCallBacks);
		CFRelease(results);
	}

	OSStatus result = noErr;
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
			result = _UpdateAggregateStatus(status, result, noErr);
		}
	}

	if (items)
		CFRelease(items);

	return result;
}

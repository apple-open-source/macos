/*
 * Copyright (c) 2006-2008 Apple Inc. All Rights Reserved.
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
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#include <AssertMacros.h>

#define CFDataGetBytePtrVoid CFDataGetBytePtr

#pragma mark SecItem private utility functions

/******************************************************************************/

/*
 * _SecProtocolTypeForSecAttrProtocol converts a SecAttrProtocol to a SecProtocolType.
 */
static SecProtocolType
_SecProtocolTypeForSecAttrProtocol(
	CFTypeRef protocol)
{
	SecProtocolType result = kSecProtocolTypeAny;
	
	if ( protocol != NULL ) {
		if ( CFEqual(protocol, kSecAttrProtocolHTTP) ) {
			result = kSecProtocolTypeHTTP;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolHTTPS) ) {
			result = kSecProtocolTypeHTTPS;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolFTP) ) {
			result = kSecProtocolTypeFTP;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolFTPS) ) {
			result = kSecProtocolTypeFTPS;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolHTTPProxy) ) {
			result = kSecProtocolTypeHTTPProxy;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolHTTPSProxy) ) {
			result = kSecProtocolTypeHTTPSProxy;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolFTPProxy) ) {
			result = kSecProtocolTypeFTPProxy;
		}
		else if ( CFEqual(protocol, kSecAttrProtocolSOCKS) ) {
			result = kSecProtocolTypeSOCKS;
		}
	}
	
	return ( result );
}

/*
 * _SecAttrProtocolForSecProtocolType converts a SecProtocolType to a SecAttrProtocol.
 */
static CFTypeRef
_SecAttrProtocolForSecProtocolType(
	SecProtocolType protocolType)
{
	CFTypeRef result;
	
	switch ( protocolType ) {
		case kSecProtocolTypeHTTP:
			result = kSecAttrProtocolHTTP;
			break;
		case kSecProtocolTypeHTTPS:
			result = kSecAttrProtocolHTTPS;
			break;
		case kSecProtocolTypeFTP:
			result = kSecAttrProtocolFTP;
			break;
		case kSecProtocolTypeFTPS:
			result = kSecAttrProtocolFTPS;
			break;
		case kSecProtocolTypeHTTPProxy:
			result = kSecAttrProtocolHTTPProxy;
			break;
		case kSecProtocolTypeHTTPSProxy:
			result = kSecAttrProtocolHTTPSProxy;
			break;
		case kSecProtocolTypeFTPProxy:
			result = kSecAttrProtocolFTPProxy;
			break;
		case kSecProtocolTypeSOCKS:
			result = kSecAttrProtocolSOCKS;
			break;
		default:
			result = NULL;
			break;
	}
	
	return ( result );
}

/*
 * _SecAuthenticationTypeForSecAttrAuthenticationType converts a
 * SecAttrAuthenticationType to a SecAuthenticationType.
 */
static SecAuthenticationType
_SecAuthenticationTypeForSecAttrAuthenticationType(
	CFTypeRef authenticationType)
{
	SecAuthenticationType result = kSecAuthenticationTypeAny;
	
	if ( authenticationType != NULL ) {
		if ( CFEqual(authenticationType, kSecAttrAuthenticationTypeDefault) ) {
			result = kSecAuthenticationTypeDefault;
		}
		else if ( CFEqual(authenticationType, kSecAttrAuthenticationTypeHTTPBasic) ) {
			result = kSecAuthenticationTypeHTTPBasic;
		}
		else if ( CFEqual(authenticationType, kSecAttrAuthenticationTypeHTTPDigest) ) {
			result = kSecAuthenticationTypeHTTPDigest;
		}
		else if ( CFEqual(authenticationType, kSecAttrAuthenticationTypeHTMLForm) ) {
			result = kSecAuthenticationTypeHTMLForm;
		}
		else if ( CFEqual(authenticationType, kSecAttrAuthenticationTypeNTLM) ) {
			result = kSecAuthenticationTypeNTLM;
		}
		/* else negotiate credentials are not stored */
	}
	
	return ( result );
}

/*
 * _SecAttrAuthenticationTypeForSecAuthenticationType converts a SecAuthenticationType
 * to a SecAttrAuthenticationType.
 */
static CFTypeRef
_SecAttrAuthenticationTypeForSecAuthenticationType(
	SecAuthenticationType authenticationType)
{
	CFTypeRef result;
	
	switch ( authenticationType ) {
		case kSecAuthenticationTypeDefault:
			result = kSecAttrAuthenticationTypeDefault;
			break;
		case kSecAuthenticationTypeHTTPBasic:
			result = kSecAttrAuthenticationTypeHTTPBasic;
			break;
		case kSecAuthenticationTypeHTTPDigest:
			result = kSecAttrAuthenticationTypeHTTPDigest;
			break;
		case kSecAuthenticationTypeHTMLForm:
			result = kSecAttrAuthenticationTypeHTMLForm;
			break;
		case kSecAuthenticationTypeNTLM:
			result = kSecAttrAuthenticationTypeNTLM;
			break;
			/* negotiate credentials are not stored */
		default:
			result = NULL;
			break;
	}
	
	return ( result );
}

/*
 * _CreateAttributesDictionaryFromItem creates a CFDictionaryRef using the
 * attributes of item.
 */
static OSStatus
_CreateAttributesDictionaryFromItem(
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
		{ kSecLabelItemAttr, 0, NULL }			/* [9] label */
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
 * _CreateSecKeychainAttributeListFromDictionary creates a SecKeychainAttributeList
 * from the attribute key/values in attrDictionary.
 *
 * If this function returns noErr, the pointer to the SecKeychainAttributeList
 * must be freed by the caller with _FreeAttrList()
 */
static OSStatus
_CreateSecKeychainAttributeListFromDictionary(
	CFDictionaryRef attrDictionary,
	SecKeychainAttributeList **attrList)
{
	OSStatus status;
	CFTypeRef value;
	SecKeychainAttributeList *attrListPtr;
	
	attrListPtr = (SecKeychainAttributeList*)calloc(1, sizeof(SecKeychainAttributeList));
	require_action(attrListPtr != NULL, calloc_attrListPtr_failed, status = errSecBufferTooSmall);
	
	#define kMaxSecKeychainAttributes 10
	
	// this code supports only kMaxSecKeychainAttributes (10) attributes
	attrListPtr->attr = (SecKeychainAttribute*)calloc(kMaxSecKeychainAttributes, sizeof(SecKeychainAttribute));
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
	
	return ( status );
}

/******************************************************************************/
#pragma mark SecItem API functions
/******************************************************************************/

OSStatus
SecItemCopyMatching(
	CFDictionaryRef query,
	CFTypeRef *result)
{
	OSStatus status;
	Boolean returningAttributes, returnAllMatches;
	CFAllocatorRef allocator;
	SecKeychainAttributeList *attrList;
	SecKeychainSearchRef search;
	SecKeychainItemRef item;
	CFTypeRef value;
	
	// default result
	*result = NULL;
	
	// get the allocator
	allocator = CFGetAllocator(query);
	
	// only kSecClassInternetPassword class is supported
	require_action(CFDictionaryGetValueIfPresent(query, kSecClass, (const void **)&value) &&
				   CFEqual(kSecClassInternetPassword, value), NotInternetPasswordClass, status = errSecItemClassMissing);
	
	// determine how to return the result
	returningAttributes = CFDictionaryGetValueIfPresent(query, kSecReturnAttributes, (const void **)&value) && CFEqual(kCFBooleanTrue, value);
	if ( !returningAttributes ) {
		// if we aren't returning attributes dictionary, then we only support returning the result as data
		require_action(CFDictionaryGetValueIfPresent(query, kSecReturnData, (const void **)&value) &&
					   CFEqual(kCFBooleanTrue, value), UnsupportedResultType, status = errSecReturnRefUnsupported);
	}
	
	// determine if one or all matches should be returned (default is kSecMatchLimitOne)
	returnAllMatches = CFDictionaryGetValueIfPresent(query, kSecMatchLimit, (const void **)&value) && CFEqual(kSecMatchLimitAll, value);
	if ( returnAllMatches ) {
		// if we're returning all matches, then we only support returning them as an array of attributes
		require_action(returningAttributes, UnsupportedResultTypeForAllMatches, status = errSecReturnDataUnsupported);
	}
	
	// build a SecKeychainAttributeList from the query dictionary
	status = _CreateSecKeychainAttributeListFromDictionary(query, &attrList);
	require_noerr(status, CreateAttributeListFromDictionary_failed);
	
	// create a search reference
	status = SecKeychainSearchCreateFromAttributes(NULL, kSecInternetPasswordItemClass, (attrList->count == 0) ? NULL : attrList, &search);
	require_noerr(status, SecKeychainSearchCreateFromAttributes_failed);
	
	if ( !returnAllMatches ) {
		// find the first match
		status = SecKeychainSearchCopyNext(search, &item);
		if ( status == noErr ) {
			if ( !returningAttributes ) {
				UInt32 length;
				void *data;
				
				status = SecKeychainItemCopyContent(item, NULL, NULL, &length, &data);
				if ( status == noErr ) {
					// return the content as a CFDataRef object
					*result = CFDataCreate(allocator, (UInt8 *)data, length);
					(void) SecKeychainItemFreeContent(NULL, data);
				}
			}
			else { // returning attributes
				// build a CFDictionaryRef of attributes from the info in itemAttrList -- it's the result
				status = _CreateAttributesDictionaryFromItem(allocator, item, (CFDictionaryRef *)result);
			}
			CFRelease(item);
		}
	}
	else {
		// find all matches
		CFMutableArrayRef mutableResult;
		CFIndex matchCount;
		
		// create a mutable dictionary to build the result in
		mutableResult = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
		require_action( mutableResult != NULL, CFArrayCreateMutable_failed, status = errSecBufferTooSmall);
		
		while ( SecKeychainSearchCopyNext(search, &item) == noErr ) {
			CFDictionaryRef dictionary;
			
			// build a CFDictionaryRef of attributes from the info in itemAttrList and add it to the mutableResult array
			status = _CreateAttributesDictionaryFromItem(allocator, item, &dictionary);
			check_noerr(status);
			CFArrayAppendValue(mutableResult, dictionary);
			CFRelease(dictionary);
			CFRelease(item);
		}
		
		matchCount = CFArrayGetCount(mutableResult);
		if ( matchCount != 0 ) {
			// return an immutable array
			*result = CFArrayCreateCopy(allocator, mutableResult);
		}
		else {
			status = errSecItemNotFound;
		}
		
		CFRelease(mutableResult);
	}
	
CFArrayCreateMutable_failed:
	CFRelease(search);
SecKeychainSearchCreateFromAttributes_failed:
	_FreeAttrList(attrList);
CreateAttributeListFromDictionary_failed:
UnsupportedResultTypeForAllMatches:
UnsupportedResultType:
NotInternetPasswordClass:
	
	return ( status );
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
	OSStatus status;
	SecKeychainAttributeList *attrList;
	CFDataRef theData;
	SecKeychainItemRef item;
	CFTypeRef value;
	
	// only kSecClassInternetPassword class is supported
	require_action(CFDictionaryGetValueIfPresent(attributes, kSecClass, (const void **)&value) &&
				   CFEqual(kSecClassInternetPassword, value), NotInternetPasswordClass, status = errSecItemClassMissing);
	
	// we only support returning the result as attributes
	require_action(result == NULL || CFDictionaryGetValueIfPresent(attributes, kSecReturnAttributes, (const void **)&value) && CFEqual(kCFBooleanTrue, value), UnsupportedResultType, status = errSecReturnDataUnsupported);
	
	// build a SecKeychainAttributeList from the attributes dictionary
	status = _CreateSecKeychainAttributeListFromDictionary(attributes, &attrList);
	require_noerr(status, CreateAttributeListFromDictionary_failed);
	
	// get the password
	theData = (CFDataRef)CFDictionaryGetValue(attributes, kSecValueData);
	require_action(theData != NULL, CFDictionaryGetValue_failed, status = errSecUnsupportedOperation);
	
	// create the keychain item
	status = SecKeychainItemCreateFromContent(kSecInternetPasswordItemClass, attrList, CFDataGetLength(theData), CFDataGetBytePtrVoid(theData), NULL, NULL, &item);
	require_noerr(status, SecKeychainItemCreateFromContent_failed);
	
	// build a CFDictionaryRef of attributes from the item -- it's the result
	if ( result != NULL ) {
		status = _CreateAttributesDictionaryFromItem(CFGetAllocator(attributes), item, (CFDictionaryRef *)result);
		check_noerr(status);
	}
	
	CFRelease(item);
SecKeychainItemCreateFromContent_failed:
CFDictionaryGetValue_failed:
	_FreeAttrList(attrList);
CreateAttributeListFromDictionary_failed:
UnsupportedResultType:
NotInternetPasswordClass:
	
	return ( status );
}

OSStatus
SecItemUpdate(
	CFDictionaryRef query,
	CFDictionaryRef attributesToUpdate)
{
	OSStatus status;
	SecKeychainAttributeList *attrList;
	SecKeychainSearchRef search;
	SecKeychainItemRef item;
	CFTypeRef value;
	SecKeychainAttributeList *changeAttrList;
	CFDataRef theData;
	Boolean itemFound;
	
	// only kSecClassInternetPassword class is supported
	require_action(CFDictionaryGetValueIfPresent(query, kSecClass, (const void **)&value) &&
				   CFEqual(kSecClassInternetPassword, value), NotInternetPasswordClass, status = errSecItemClassMissing);
	
	// build a SecKeychainAttributeList from the query dictionary
	status = _CreateSecKeychainAttributeListFromDictionary(query, &attrList);
	require_noerr(status, CreateAttributeListFromDictionary_failed);
	
	// create a search reference
	status = SecKeychainSearchCreateFromAttributes(NULL, kSecInternetPasswordItemClass, (attrList->count == 0) ? NULL : attrList, &search);
	require_noerr(status, SecKeychainSearchCreateFromAttributes_failed);
	
	// build a SecKeychainAttributeList from the attributesToUpdate dictionary
	status = _CreateSecKeychainAttributeListFromDictionary(attributesToUpdate, &changeAttrList);
	require_noerr(status, CreateAttributeListFromDictionary_changeAttrList_failed);
	
	// get the password
	theData = (CFDataRef)CFDictionaryGetValue(attributesToUpdate, kSecValueData);
	
	itemFound = FALSE;
	while ( SecKeychainSearchCopyNext(search, &item) == noErr ) {
		// update the keychain item
		itemFound = TRUE;
		status = SecKeychainItemModifyContent(item, (changeAttrList->count == 0) ? NULL : changeAttrList,
											  (theData != NULL) ? CFDataGetLength(theData) : 0, (theData != NULL) ? CFDataGetBytePtrVoid(theData) : NULL);
		check_noerr(status);
		CFRelease(item);
	}
	
	if ( !itemFound ) {
		status = errSecItemNotFound;
	}
	
	_FreeAttrList(changeAttrList);
CreateAttributeListFromDictionary_changeAttrList_failed:
	CFRelease(search);
SecKeychainSearchCreateFromAttributes_failed:
	_FreeAttrList(attrList);
CreateAttributeListFromDictionary_failed:
NotInternetPasswordClass:
	
	return ( status );
}

OSStatus
SecItemDelete(
	CFDictionaryRef query)
{
	OSStatus status;
	SecKeychainAttributeList *attrList;
	SecKeychainSearchRef search;
	SecKeychainItemRef item;
	CFTypeRef value;
	Boolean itemFound;
	
	// only kSecClassInternetPassword class is supported
	require_action(CFDictionaryGetValueIfPresent(query, kSecClass, (const void **)&value) &&
				   CFEqual(kSecClassInternetPassword, value), NotInternetPasswordClass, status = errSecItemClassMissing);
	
	// build a SecKeychainAttributeList from the query dictionary
	status = _CreateSecKeychainAttributeListFromDictionary(query, &attrList);
	require_noerr(status, CreateAttributeListFromDictionary_failed);
	
	// create a search reference
	status = SecKeychainSearchCreateFromAttributes(NULL, kSecInternetPasswordItemClass, (attrList->count == 0) ? NULL : attrList, &search);
	require_noerr(status, SecKeychainSearchCreateFromAttributes_failed);
	
	itemFound = FALSE;
	while ( SecKeychainSearchCopyNext(search, &item) == noErr ) {
		// and delete it
		itemFound = TRUE;
		status = _SafeSecKeychainItemDelete(item);
		check_noerr(status);
		
		CFRelease(item);
	}
	
	if ( !itemFound ) {
		status = errSecItemNotFound;
	}
	
	CFRelease(search);
SecKeychainSearchCreateFromAttributes_failed:
	_FreeAttrList(attrList);
CreateAttributeListFromDictionary_failed:
NotInternetPasswordClass:
	
	return ( status );
}

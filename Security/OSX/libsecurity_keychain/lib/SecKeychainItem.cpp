/*
 * Copyright (c) 2000-2004,2011-2016 Apple Inc. All Rights Reserved.
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

#include <Security/SecBase.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecItemPriv.h>

#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <security_keychain/Item.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Identity.h>
#include <security_keychain/KCCursor.h> // @@@ Remove this when SecKeychainItemFindFirst moves to SecKeychainSearch

#include <securityd_client/dictionary.h>
#include <security_cdsa_utilities/Schema.h>
#include <Security/cssmapplePriv.h>
#include <syslog.h>

#include "SecBridge.h"
#include "KCExceptions.h"
#include "Access.h"
#include "SecKeychainItemExtendedAttributes.h"

extern "C" Boolean SecKeyIsCDSAKey(SecKeyRef ref);

//
// Given a polymorphic Sec type object, return
// its AclBearer component.
// Note: Login ACLs are not hooked into this layer;
// modules or attachments have no Sec* layer representation.
//
static
RefPointer<AclBearer> aclBearer(CFTypeRef itemRef)
{
	// well, exactly what kind of something are you?
	CFTypeID id = CFGetTypeID(itemRef);
	if (id == gTypes().ItemImpl.typeID) {
		// keychain item. If it's in a protected group, return the group key
		if (SSGroup group = ItemImpl::required(SecKeychainItemRef(itemRef))->group())
			return &*group;
	} else if (id == SecKeyGetTypeID() && SecKeyIsCDSAKey((SecKeyRef)itemRef)) {
		// key item, return the key itself.
		if (CssmClient::Key key = KeyItem::required(SecKeyRef(itemRef))->key())
			return &*key;
	} else if (id == gTypes().KeychainImpl.typeID) {
		// keychain (this yields the database ACL)
		//@@@ not hooked up yet
	}
	// Guess not. Bummer
	MacOSError::throwMe(errSecNoAccessForItem);
}


CFTypeID
SecKeychainItemGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().ItemImpl.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList *attrList,
		UInt32 length, const void *data, SecKeychainRef keychainRef,
		SecAccessRef initialAccess, SecKeychainItemRef *itemRef)
{
	BEGIN_SECAPI

	KCThrowParamErrIf_(length!=0 && data==NULL);
	Item item(itemClass, attrList, length, data);
	if (initialAccess) {
		item->setAccess(Access::required(initialAccess));
	}
	Keychain keychain = nil;
	try
	{
		keychain = Keychain::optional(keychainRef);
		if ( !keychain->exists() )
		{
			MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
		}
	}
	catch(...)
	{
		keychain = globals().storageManager.defaultKeychainUI(item);
	}

	keychain->add(item);
	if (itemRef) {
		*itemRef = item->handle();
	}

	END_SECAPI
}


OSStatus
SecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->modifyContent(attrList, length, data);

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->getContent(itemClass, attrList, length, outData);

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI

	ItemImpl::freeContent(attrList, data);

	END_SECAPI
}


OSStatus
SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->modifyAttributesAndData(attrList, length, data);

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->getAttributesAndData(info, itemClass, attrList, length, outData);

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI

	ItemImpl::freeAttributesAndData(attrList, data);

	END_SECAPI
}


OSStatus
SecKeychainItemDelete(SecKeychainItemRef itemRef)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	Keychain keychain = item->keychain();
	// item must be persistent.
	KCThrowIf_( !keychain, errSecInvalidItemRef );

	/*
	 * Before deleting the item, delete any existing Extended Attributes.
	 */
	OSStatus ortn;
	CFArrayRef attrNames = NULL;
	ortn = SecKeychainItemCopyAllExtendedAttributes(__itemImplRef, &attrNames, NULL);
	if(ortn == errSecSuccess) {
		CFIndex numAttrs = CFArrayGetCount(attrNames);
		for(CFIndex dex=0; dex<numAttrs; dex++) {
			CFStringRef attrName = (CFStringRef)CFArrayGetValueAtIndex(attrNames, dex);
			/* setting value to NULL ==> delete */
			SecKeychainItemSetExtendedAttribute(__itemImplRef, attrName, NULL);
		}
	}

	/* now delete the item */
	keychain->deleteItem( item );

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemCopyKeychain(SecKeychainItemRef itemRef, SecKeychainRef* keychainRef)
{
	BEGIN_SECKCITEMAPI

	// make sure this item has a keychain
	Keychain kc = ItemImpl::required(__itemImplRef)->keychain();
	if (kc == NULL)
	{
		MacOSError::throwMe(errSecNoSuchKeychain);
	}

	Required(keychainRef) = kc->handle();

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemCreateCopy(SecKeychainItemRef itemRef, SecKeychainRef destKeychainRef,
	SecAccessRef initialAccess, SecKeychainItemRef *itemCopy)
{
	BEGIN_SECKCITEMAPI

	Item copy = ItemImpl::required(__itemImplRef)->copyTo(Keychain::optional(destKeychainRef), Access::optional(initialAccess));
	if (itemCopy) {
		*itemCopy = copy->handle();
	}

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemGetUniqueRecordID(SecKeychainItemRef itemRef, const CSSM_DB_UNIQUE_RECORD **uniqueRecordID)
{
	BEGIN_SECKCITEMAPI

	Required(uniqueRecordID) = ItemImpl::required(__itemImplRef)->dbUniqueRecord();

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemGetDLDBHandle(SecKeychainItemRef itemRef, CSSM_DL_DB_HANDLE* dldbHandle)
{
	BEGIN_SECKCITEMAPI

	*dldbHandle = ItemImpl::required(__itemImplRef)->keychain()->database()->handle();

	END_SECKCITEMAPI
}

#if 0
static
OSStatus SecAccessCreateFromObject(CFTypeRef sourceRef,
	SecAccessRef *accessRef)
{
	BEGIN_SECAPI

	Required(accessRef);	// preflight
	SecPointer<Access> access = new Access(*aclBearer(sourceRef));
	*accessRef = access->handle();

	END_SECAPI
}


/*!
 */
static
OSStatus SecAccessModifyObject(SecAccessRef accessRef, CFTypeRef sourceRef)
{
	BEGIN_SECAPI

	Access::required(accessRef)->setAccess(*aclBearer(sourceRef), true);

	END_SECAPI
}
#endif

OSStatus
SecKeychainItemCopyAccess(SecKeychainItemRef itemRef, SecAccessRef* accessRef)
{
	BEGIN_SECKCITEMAPI

	Required(accessRef);	// preflight
	SecPointer<Access> access = new Access(*aclBearer(reinterpret_cast<CFTypeRef>(__itemImplRef)));
	*accessRef = access->handle();

	END_SECKCITEMAPI
}


OSStatus
SecKeychainItemSetAccess(SecKeychainItemRef itemRef, SecAccessRef accessRef)
{
	BEGIN_SECKCITEMAPI

	Access::required(accessRef)->setAccess(*aclBearer(reinterpret_cast<CFTypeRef>(__itemImplRef)), true);

	ItemImpl::required(__itemImplRef)->postItemEvent(kSecUpdateEvent);

	END_SECKCITEMAPI
}

OSStatus SecKeychainItemSetAccessWithPassword(SecKeychainItemRef itemRef, SecAccessRef accessRef, UInt32 passwordLength, const void * password)
{
    BEGIN_SECKCITEMAPI

    OSStatus result;

    // try to unlock the keychain with this password first
    SecKeychainRef kc = NULL;
    result = SecKeychainItemCopyKeychain(__itemImplRef, &kc);
    if(!result) {
        SecKeychainUnlock(kc, passwordLength, password, true);
        if(kc) {
            CFRelease(kc);
        }
    }

    // Create some credentials with this password
    CssmAutoData data(Allocator::standard(), password, passwordLength);
    AclFactory::PassphraseUnlockCredentials cred(data, Allocator::standard());

    Access::required(accessRef)->editAccess(*aclBearer(reinterpret_cast<CFTypeRef>(__itemImplRef)), true, cred.getAccessCredentials());
    ItemImpl::required(itemRef)->postItemEvent (kSecUpdateEvent);

    END_SECKCITEMAPI
}


/*  Sets an item's data for legacy "KC" CoreServices APIs.
    Note this version sets the data, but doesn't update the item
    as the KC behavior dictates.
*/
OSStatus SecKeychainItemSetData(SecKeychainItemRef itemRef, UInt32 length, const void* data)
{
	BEGIN_SECKCITEMAPI

	ItemImpl::required(__itemImplRef)->setData(length, data);

	END_SECKCITEMAPI
}

/*  Gets an item's data for legacy "KC" CoreServices APIs.
    Note this version doesn't take a SecItemClass parameter.
*/
OSStatus SecKeychainItemGetData(SecKeychainItemRef itemRef, UInt32 maxLength, void* data, UInt32* actualLength)
{
	BEGIN_SECKCITEMAPI

	/* The caller either needs to specify data and maxLength or an actualLength,
	 * so we return either the data itself or the actual length of the data or both.
	 */
	if (!((data && maxLength) || actualLength)) {
		MacOSError::throwMe(errSecParam);
	}
	CssmDataContainer aData;
	ItemImpl::required(__itemImplRef)->getData(aData);
	if (actualLength) {
		*actualLength = (UInt32)aData.length();
	}
	if (data) {
		// Make sure the buffer is big enough
		if (aData.length() > maxLength) {
			MacOSError::throwMe(errSecBufferTooSmall);
		}
		memcpy(data, aData.data(), aData.length());
	}

	END_SECKCITEMAPI
}

/*  Update a keychain item for legacy "KC" CoreServices APIs.
    The "KC" API's do a 'set attribute', then an 'update'.
*/
OSStatus SecKeychainItemUpdate(SecKeychainItemRef itemRef)
{
	BEGIN_SECKCITEMAPI

	ItemImpl::required(__itemImplRef)->update();

	END_SECKCITEMAPI
}

/* Add a 'floating' keychain item without UI for legacy "KC" CoreServices APIs.
*/
OSStatus SecKeychainItemAddNoUI(SecKeychainRef keychainRef, SecKeychainItemRef itemRef)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	Keychain::optional(keychainRef)->add(item);

	END_SECKCITEMAPI
}

/* Add a 'floating' keychain item to the default keychain with possible UI for legacy "KC" Carbon APIs.
*/
OSStatus SecKeychainItemAdd(SecKeychainItemRef itemRef)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	Keychain defaultKeychain = globals().storageManager.defaultKeychainUI(item);
	defaultKeychain->add(item);

	END_SECKCITEMAPI
}

/* Creates a floating keychain item for legacy "KC" CoreServices APIs
*/
OSStatus SecKeychainItemCreateNew(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, SecKeychainItemRef* itemRef)
{
	BEGIN_SECAPI

	RequiredParam(itemRef) = Item(itemClass, itemCreator, length, data, false)->handle();

	END_SECAPI
}

/* Gets an individual attribute for legacy "KC" CoreServices APIs
*/
OSStatus SecKeychainItemGetAttribute(SecKeychainItemRef itemRef, SecKeychainAttribute* attribute, UInt32* actualLength)
{
	BEGIN_SECKCITEMAPI

	ItemImpl::required(__itemImplRef)->getAttribute(RequiredParam(attribute), actualLength);

	END_SECKCITEMAPI
}

/* Sets an individual attribute for legacy "KC" CoreServices APIs
*/
OSStatus SecKeychainItemSetAttribute(SecKeychainItemRef itemRef, SecKeychainAttribute* attribute)
{
	BEGIN_SECKCITEMAPI

	ItemImpl::required(__itemImplRef)->setAttribute(RequiredParam(attribute));

	END_SECKCITEMAPI
}

/*  Finds a keychain item for legacy "KC" CoreServices APIs.
    Note: This version doesn't take a SecItemClass because
            SecKeychainSearchCreateFromAttributes() requires it.
    @@@ This should move to SecKeychainSearch.cpp
*/
OSStatus SecKeychainItemFindFirst(SecKeychainRef keychainRef, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef, SecKeychainItemRef *itemRef)
{
	BEGIN_SECAPI

	KCCursor cursor;
	if (keychainRef) {
		cursor = KeychainImpl::required(keychainRef)->createCursor(attrList);
	} else {
		cursor = globals().storageManager.createCursor(attrList);
	}

	Item item;
	if (!cursor->next(item))
		return errSecItemNotFound;

	*itemRef=item->handle();
	if (searchRef) {
		*searchRef=cursor->handle();
	}

	END_SECAPI
}

static OSStatus SecKeychainItemCreatePersistentReferenceFromCertificate(SecCertificateRef certRef,
    CFDataRef *persistentItemRef, Boolean isIdentity)
{
	OSStatus __secapiresult;
	if (!certRef || !persistentItemRef) {
		return errSecParam;
	}

	// If we already have a keychain item, we won't need to look it up by serial and issuer
	SecKeychainItemRef kcItem = NULL;
	if (SecCertificateIsItemImplInstance(certRef)) {
		kcItem = (SecKeychainItemRef) CFRetain(certRef);
	}
	else {
		kcItem = (SecKeychainItemRef) SecCertificateCopyKeychainItem(certRef);
	}
	if (kcItem) {
		__secapiresult = errSecParam;
		try {
			Item item = ItemImpl::required((kcItem));
			item->copyPersistentReference(*persistentItemRef, isIdentity);
			__secapiresult = errSecSuccess;
		}
		catch(...) {}
		CFRelease(kcItem);
		if (__secapiresult == errSecSuccess) {
			return __secapiresult;
		}
	}

	// Certificate does not have a keychain item reference; look it up by serial and issuer
	SecCertificateRef certItem = NULL;
	if (SecCertificateIsItemImplInstance(certRef)) {
		certItem = SecCertificateCreateFromItemImplInstance(certRef);
	}
	else {
		certItem = (SecCertificateRef) CFRetain(certRef);
	}

	CFErrorRef errorRef = NULL;
	CFDataRef serialData = SecCertificateCopySerialNumber(certItem, &errorRef);
	if (errorRef) {
		CFIndex err = CFErrorGetCode(errorRef);
		CFRelease(errorRef);
		if (serialData) { CFRelease(serialData); }
		if (certItem) { CFRelease(certItem); }
		return (OSStatus)err;
	}
	CFDataRef issuerData = SecCertificateCopyNormalizedIssuerContent(certItem, &errorRef);
	if (errorRef) {
		CFIndex err = CFErrorGetCode(errorRef);
		CFRelease(errorRef);
		if (serialData) { CFRelease(serialData); }
		if (issuerData) { CFRelease(issuerData); }
		if (certItem) { CFRelease(certItem); }
		return (OSStatus)err;
	}

	try {
		// look up ItemImpl cert in keychain by normalized issuer and serial number
		StorageManager::KeychainList keychains;
		globals().storageManager.optionalSearchList(NULL, keychains);
		KCCursor cursor(Certificate::cursorForIssuerAndSN_CF(keychains, issuerData, serialData));
		Item item;
		if (!cursor->next(item)) {
			MacOSError::throwMe(errSecItemNotFound);
		}
		item->copyPersistentReference(*persistentItemRef, false);
		__secapiresult = errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (serialData)
		CFRelease(serialData);
	if (issuerData)
		CFRelease(issuerData);
	if (certItem)
		CFRelease(certItem);

	return __secapiresult;
}

OSStatus SecKeychainItemCreatePersistentReference(SecKeychainItemRef itemRef, CFDataRef *persistentItemRef)
{
    /* We're in the unified world, where SecCertificateRef is not a SecKeychainItemRef. */
    if (!itemRef || !persistentItemRef) {
        return errSecParam;
    }
    // first, query the iOS keychain
    {
        const void *keys[] = { kSecValueRef, kSecReturnPersistentRef, kSecAttrNoLegacy };
        const void *values[] = { itemRef, kCFBooleanTrue, kCFBooleanTrue };
        CFRef<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                                                          sizeof(keys) / sizeof(*keys),
                                                          &kCFTypeDictionaryKeyCallBacks,
                                                          &kCFTypeDictionaryValueCallBacks);
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef *)persistentItemRef);
        if (status == errSecSuccess) {
            return status;
        }
    }
    // otherwise, handle certificate
    SecCertificateRef certRef = NULL;
    CFTypeID itemType = CFGetTypeID(itemRef);
    bool isIdentity = false;
    if (itemType == SecIdentityGetTypeID()) {
        SecIdentityCopyCertificate((SecIdentityRef)itemRef, &certRef);
        isIdentity = true;
    }
    else if (itemType == SecCertificateGetTypeID()) {
        certRef = (SecCertificateRef) CFRetain(itemRef);
    }
    if (certRef) {
        OSStatus status = SecKeychainItemCreatePersistentReferenceFromCertificate(certRef, persistentItemRef, isIdentity);
        CFRelease(certRef);
        return status;
    }
    // otherwise, not a certificate, so proceed as usual for keychain item

    BEGIN_SECAPI
    Item item = ItemImpl::required(itemRef);
    item->copyPersistentReference(*persistentItemRef, false);
    END_SECAPI
}

OSStatus SecKeychainItemCopyFromPersistentReference(CFDataRef persistentItemRef, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

    KCThrowParamErrIf_(!persistentItemRef || !itemRef);
    // first, query the iOS keychain
    {
        const void *keys[] = { kSecValuePersistentRef, kSecReturnRef, kSecAttrNoLegacy};
        const void *values[] = { persistentItemRef, kCFBooleanTrue, kCFBooleanTrue };
        CFRef<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                                                          sizeof(keys) / sizeof(*keys),
                                                          &kCFTypeDictionaryKeyCallBacks,
                                                          &kCFTypeDictionaryValueCallBacks);
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef *)itemRef);
        if (status == errSecSuccess) {
            return status;
        }
    }
    // otherwise, proceed as usual for keychain item
    CFTypeRef result = NULL;
    bool isIdentityRef = false;
    Item item = ItemImpl::makeFromPersistentReference(persistentItemRef, &isIdentityRef);
    if (isIdentityRef) {
        // item was stored as an identity, attempt to reconstitute it
        SecPointer<Certificate> certificatePtr(static_cast<Certificate *>(item.get()));
        StorageManager::KeychainList keychains;
        globals().storageManager.optionalSearchList(NULL, keychains);
        SecPointer<Identity> identityPtr(new Identity(keychains, certificatePtr));
        result = identityPtr->handle();
        KCThrowIf_( !result, errSecItemNotFound );
    }
    if (!result) {
        result = item->handle();
    }
    *itemRef = (SecKeychainItemRef) result;

    /* see if we should convert outgoing item to a unified SecCertificateRef */
    SecItemClass tmpItemClass = Schema::itemClassFor(item->recordType());
    if (tmpItemClass == kSecCertificateItemClass && !isIdentityRef) {
        SecPointer<Certificate> certificate(static_cast<Certificate *>(&*item));
        CssmData certData = certificate->data();
        CFDataRef data = NULL;
        if (certData.Data && certData.Length) {
            data = CFDataCreate(NULL, certData.Data, certData.Length);
        }
        if (!data) {
            *itemRef = NULL;
            if (certData.Data && !certData.Length) {
                syslog(LOG_ERR, "WARNING: SecKeychainItemCopyFromPersistentReference skipped a zero-length certificate (data=0x%lX)",
                       (uintptr_t)certData.Data);
                return errSecDataNotAvailable;
            }
            else {
                syslog(LOG_ERR, "WARNING: SecKeychainItemCopyFromPersistentReference failed to retrieve certificate data (length=%ld, data=0x%lX)",
                       (long)certData.Length, (uintptr_t)certData.Data);
                return errSecInternal;
            }
        }
        SecKeychainItemRef tmpRef = *itemRef;
        *itemRef = (SecKeychainItemRef) SecCertificateCreateWithKeychainItem(NULL, data, tmpRef);
        if (data)
            CFRelease(data);
        if (tmpRef)
            CFRelease(tmpRef);
    }

	END_SECAPI
}

OSStatus SecKeychainItemCopyRecordIdentifier(SecKeychainItemRef itemRef, CFDataRef *recordIdentifier)
{
	BEGIN_SECKCITEMAPI

	CSSM_DATA data;
	RequiredParam (recordIdentifier);
	Item item = ItemImpl::required(__itemImplRef);
	item->copyRecordIdentifier (data);
	*recordIdentifier = ::CFDataCreate(kCFAllocatorDefault, (UInt8*) data.Data, data.Length);
	free (data.Data);

	END_SECKCITEMAPI
}

OSStatus
SecKeychainItemCopyFromRecordIdentifier(SecKeychainRef keychainRef,
										SecKeychainItemRef *itemRef,
										CFDataRef recordIdentifier)
{
	BEGIN_SECAPI

	// make a local Keychain reference
	RequiredParam (keychainRef);
	Keychain keychain = KeychainImpl::optional (keychainRef);
	RequiredParam (itemRef);
	RequiredParam (recordIdentifier);

	Db db(keychain->database());

	// make a raw database call to get the data
	CSSM_DL_DB_HANDLE dbHandle = db.handle ();
	CSSM_DB_UNIQUE_RECORD uniqueRecord;

	// according to source, we should be able to reconsitute the uniqueRecord
	// from the data we earlier retained

	// prepare the record id
	memset (&uniqueRecord, 0, sizeof (uniqueRecord));
	uniqueRecord.RecordIdentifier.Data = (uint8*) CFDataGetBytePtr (recordIdentifier);
	uniqueRecord.RecordIdentifier.Length = CFDataGetLength (recordIdentifier);

	// convert this unique id to a CSSM_DB_UNIQUE_RECORD that works for the CSP/DL
	CSSM_DB_UNIQUE_RECORD_PTR outputUniqueRecordPtr;
	CSSM_RETURN result;
	result = CSSM_DL_PassThrough (dbHandle, CSSM_APPLECSPDL_DB_CONVERT_RECORD_IDENTIFIER, &uniqueRecord, (void**) &outputUniqueRecordPtr);
	KCThrowIf_(result != 0, errSecItemNotFound);

	// from this, get the record type
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributeData;
	memset (&attributeData, 0, sizeof (attributeData));

	result = CSSM_DL_DataGetFromUniqueRecordId (dbHandle, outputUniqueRecordPtr, &attributeData, NULL);
	KCThrowIf_(result != 0, errSecItemNotFound);
	CSSM_DB_RECORDTYPE recordType = attributeData.DataRecordType;

	// make the unique record item -- precursor to creation of a SecKeychainItemRef
	DbUniqueRecord unique(db);
	CSSM_DB_UNIQUE_RECORD_PTR *uniquePtr = unique;
	*uniquePtr = outputUniqueRecordPtr;

	unique->activate ();
	Item item = keychain->item (recordType, unique);
	if (itemRef)
	{
		*itemRef = item->handle();
	}

	END_SECAPI
}

OSStatus SecKeychainItemCreateFromEncryptedContent(SecItemClass itemClass,
		UInt32 length, const void *data, SecKeychainRef keychainRef,
		SecAccessRef initialAccess, SecKeychainItemRef *itemRef, CFDataRef *localID)
{
	BEGIN_SECAPI

	KCThrowParamErrIf_(length!=0 && data==NULL);
	RequiredParam (localID);
	RequiredParam (keychainRef);

	Item item(itemClass, (uint32) 0, length, data, true);
	if (initialAccess)
		item->setAccess(Access::required(initialAccess));

	Keychain keychain = Keychain::optional(keychainRef);
	if (!keychain->exists())
	{
		MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
	}

	item->doNotEncrypt ();
	try
	{
		keychain->add(item);
	}
	catch (const CommonError &err)
	{
		if (err.osStatus () == errSecNoSuchClass)
		{
			// the only time this should happen is if the item is a certificate (for keychain syncing)
			if (itemClass == CSSM_DL_DB_RECORD_X509_CERTIFICATE)
			{
				// create the certificate relation
				Db db(keychain->database());

				db->createRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE,
						"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
						Schema::X509CertificateSchemaAttributeCount,
						Schema::X509CertificateSchemaAttributeList,
						Schema::X509CertificateSchemaIndexCount,
						Schema::X509CertificateSchemaIndexList);
				keychain->keychainSchema()->didCreateRelation(
						CSSM_DL_DB_RECORD_X509_CERTIFICATE,
						"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
						Schema::X509CertificateSchemaAttributeCount,
						Schema::X509CertificateSchemaAttributeList,
						Schema::X509CertificateSchemaIndexCount,
						Schema::X509CertificateSchemaIndexList);

				// add the item again
				keychain->add(item);
			}
		}
		else
		{
			throw;
		}
	}

	if (itemRef)
		*itemRef = item->handle();

	CSSM_DATA recordID;
	item->copyRecordIdentifier (recordID);

	*localID = CFDataCreate(kCFAllocatorDefault, (UInt8*) recordID.Data, recordID.Length);
	free (recordID.Data);

	END_SECAPI
}

OSStatus SecKeychainItemCopyAttributesAndEncryptedData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
													   SecItemClass *itemClass, SecKeychainAttributeList **attrList,
													   UInt32 *length, void **outData)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->doNotEncrypt ();
	item->getAttributesAndData(info, itemClass, attrList, length, outData);

	END_SECKCITEMAPI
}

OSStatus SecKeychainItemModifyEncryptedData(SecKeychainItemRef itemRef, UInt32 length, const void *data)
{
	BEGIN_SECKCITEMAPI

	Item item = ItemImpl::required(__itemImplRef);
	item->doNotEncrypt ();
	item->modifyAttributesAndData(NULL, length, data);

	END_SECKCITEMAPI
}

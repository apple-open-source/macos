/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include "keychain_regressions.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKeychainPriv.h>
#include <TargetConditionals.h>
#include <Security/cssmapi.h>
#include <stdlib.h>

#include "kc-30-xara-item-helpers.h"
#include "kc-30-xara-key-helpers.h"
#include "kc-30-xara-upgrade-helpers.h"

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

/* Test basic add delete update copy matching stuff. */


/* Standard memory functions required by CSSM. */
static void *cssmMalloc(CSSM_SIZE size, void *allocRef) { return malloc(size); }
static void cssmFree(void *mem_ptr, void *allocRef) { free(mem_ptr); return; }
static void *cssmRealloc(void *ptr, CSSM_SIZE size, void *allocRef) { return realloc( ptr, size ); }
static void *cssmCalloc(uint32 num, CSSM_SIZE size, void *allocRef) { return calloc( num, size ); }
static CSSM_API_MEMORY_FUNCS memFuncs = { cssmMalloc, cssmFree, cssmRealloc, cssmCalloc, NULL };

static CSSM_DL_DB_HANDLE initializeDL() {
    CSSM_VERSION version = { 2, 0 };
    CSSM_DL_DB_HANDLE dldbHandle;
    CSSM_GUID myGuid = { 0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 } };
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;

    ok_status(CSSM_Init(&version, CSSM_PRIVILEGE_SCOPE_NONE, &myGuid, CSSM_KEY_HIERARCHY_NONE, &pvcPolicy, NULL), "cssm_init");
    ok_status(CSSM_ModuleLoad(&gGuidAppleFileDL, CSSM_KEY_HIERARCHY_NONE, NULL, NULL), "module_load");
    ok_status(CSSM_ModuleAttach(&gGuidAppleFileDL, &version, &memFuncs, 0, CSSM_SERVICE_DL, 0, CSSM_KEY_HIERARCHY_NONE, NULL, 0, NULL, &dldbHandle.DLHandle), "module_attach");

    return dldbHandle;
}
#define initializeDLTests 3

static void unloadDL(CSSM_DL_DB_HANDLE* dldbHandle) {
    ok_status(CSSM_ModuleDetach(dldbHandle->DLHandle), "detach");
    ok_status(CSSM_ModuleUnload(&gGuidAppleFileDL, NULL, NULL), "unload");
    ok_status(CSSM_Terminate(), "terminate");
}
#define unloadDLTests 3

static void modifyAttributeInKeychain(char * name, CSSM_DL_DB_HANDLE dldbHandle, char * keychainName, CSSM_DB_RECORDTYPE recordType, char* attributeName, char* newValue, size_t len) {
    CSSM_RETURN status = CSSM_OK;
    ok_status(CSSM_DL_DbOpen(dldbHandle.DLHandle, keychainName,
                   NULL,
                   CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                   NULL, /* Access cred? */
                   NULL, /* Open Parameters? */
                   &dldbHandle.DBHandle), "%s: CSSM_DL_DbOpen", name);

    CSSM_QUERY queryAll = {};
    queryAll.RecordType = recordType;

    CSSM_HANDLE results = 0;
    CSSM_DATA data = {};
    CSSM_DB_UNIQUE_RECORD_PTR uniqueIdPtr = NULL;

    CSSM_DB_RECORD_ATTRIBUTE_DATA attributes = {};
    attributes.NumberOfAttributes = 1;
    attributes.AttributeData = malloc(sizeof(CSSM_DB_ATTRIBUTE_DATA) * attributes.NumberOfAttributes);
    attributes.AttributeData[0].Info.Label.AttributeName = attributeName;
    attributes.AttributeData[0].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;

    attributes.AttributeData[0].NumberOfValues = 1;
    attributes.AttributeData[0].Value = malloc(sizeof(CSSM_DATA)*attributes.AttributeData[0].NumberOfValues);


    status = CSSM_DL_DataGetFirst(dldbHandle, &queryAll, &results, &attributes, &data, &uniqueIdPtr);
    while(status == CSSM_OK) {
        // I'm sure it has one thing and that thing needs to change.
        attributes.AttributeData[0].Value[0].Data = (void*)newValue;
        attributes.AttributeData[0].Value[0].Length = strlen(newValue);

        CSSM_DL_DataModify(dldbHandle,
                           attributes.DataRecordType,
                           uniqueIdPtr,
                           &attributes,
                           NULL, // no data modification
                           CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

        CSSM_DL_FreeUniqueRecord(dldbHandle, uniqueIdPtr);
        status = CSSM_DL_DataGetNext(dldbHandle, results, &attributes, &data, &uniqueIdPtr);
    }
    ok_status(CSSM_DL_DbClose(dldbHandle), "%s: CSSM_DL_DbClose", name);
}
#define modifyAttributeInKeychainTests 2

static void testAttackItem(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAttackItem";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    SecKeychainItemRef item = checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_GENERIC_PASSWORD, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 0);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testAttackItemTests (newKeychainTests + checkNTests + makeItemWithIntegrityTests + checkNTests + modifyAttributeInKeychainTests + openKeychainTests + checkNTests + 1)

static void testAttackKey(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAttackKey";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    SecKeychainItemRef item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "Label", modification, strlen(modification));

    kc = openKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testAttackKeyTests (newKeychainTests + checkNTests + makeKeyWithIntegrityTests + checkNTests + modifyAttributeInKeychainTests + openKeychainTests + checkNTests + 1)


static void testAddAfterCorruptItem(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAddAfterCorruptItem";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeCustomItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("test_label"), CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    SecKeychainItemRef item = checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    CFReleaseNull(item);

    makeDuplicateItem(name, kc, kSecClassGenericPassword);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_GENERIC_PASSWORD, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeCustomItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("evil_application"), CFSTR("d2aa97b30a1f96f9e61fcade2b00d9f4284976a83a5b68392251ee5ec827f8cc"));
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("evil_application"));
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testAddAfterCorruptItemTests (newKeychainTests + checkNTests + makeCustomItemWithIntegrityTests + checkNTests + makeDuplicateItemTests \
        + modifyAttributeInKeychainTests + openKeychainTests + checkNTests + makeCustomItemWithIntegrityTests + checkNTests + makeCustomDuplicateItemTests + 1)

static void testAddAfterCorruptKey(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAddAfterCorruptKey";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    // Make a symmetric key
    makeCustomKeyWithIntegrity(name, kc, CFSTR("test_key"), CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    SecKeychainItemRef item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    makeCustomDuplicateKey(name, kc, CFSTR("test_key"));
    CFReleaseNull(item);

    // Make a key pair
    SecKeyRef pub;
    SecKeyRef priv;
    makeCustomKeyPair(name, kc, CFSTR("test_key_pair"), &pub, &priv);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    CFReleaseNull(pub);
    CFReleaseNull(priv);

    ok_status(SecKeychainListRemoveKeychain(&kc), "%s: SecKeychainListRemoveKeychain", name);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "PrintName", modification, strlen(modification));
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_PUBLIC_KEY, "PrintName", modification, strlen(modification));
    modifyAttributeInKeychain(name, dldbHandle, keychainFile, CSSM_DL_DB_RECORD_PRIVATE_KEY, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    makeCustomKeyWithIntegrity(name, kc, CFSTR("evil_application"), CFSTR("ca6d90a0b053113e43bbb67f64030230c96537f77601f66bdf821d8684431dfc"));
    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    makeCustomDuplicateKey(name, kc, CFSTR("evil_application"));

    makeCustomKeyPair(name, kc, CFSTR("evil_application"), &pub, &priv);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    // We cannot create a duplicate key pair, so don't try.

    CFReleaseNull(item);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testAddAfterCorruptKeyTests (newKeychainTests \
        + checkNTests + checkNTests + checkNTests \
        + makeCustomKeyWithIntegrityTests + checkNTests + makeCustomDuplicateKeyTests \
        + makeCustomKeyPairTests + checkNTests + checkNTests \
        + 1 \
        + modifyAttributeInKeychainTests \
        + modifyAttributeInKeychainTests \
        + modifyAttributeInKeychainTests \
        + openKeychainTests \
        + checkNTests + checkNTests + checkNTests \
        + makeCustomKeyWithIntegrityTests + checkNTests \
        + makeCustomDuplicateKeyTests \
        + makeCustomKeyPairTests + checkNTests + checkNTests \
        + 1)


static void testKeychainUpgrade() {
    char name[100];
    sprintf(name, "testKeychainUpgrade");
    secdebugfunc("integrity", "************************************* %s", name);

    writeOldKeychain(name, keychainFile);
    SecKeychainRef kc = openCustomKeychain(name, "test.keychain", "password");

    SecKeychainItemRef item;

    item = checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkIntegrityHash(name, item, CFSTR("39c56eadd3e3b496b6099e5f3d5ff88eaee9ca2e3a50c1be8319807a72e451e5"));
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkN(name, makeQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    checkIntegrityHash(name, item, CFSTR("4f1b64e3c156968916e72d8ff3f1a8eb78b32abe0b2b43f0578eb07c722aaf03"));
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("42d29fd5e9935edffcf6d0261eabddb00782ec775caa93716119e8e553ab5578"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("bdf219cdbc2dc6c4521cf39d1beda2e3491ef0330ba59eb41229dd909632f48d"));

    // Now close the keychain and open it again
    CFReleaseNull(kc);
    kc = openCustomKeychain(name, "test.keychain", "password");

    item = checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkIntegrityHash(name, item, CFSTR("39c56eadd3e3b496b6099e5f3d5ff88eaee9ca2e3a50c1be8319807a72e451e5"));
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkN(name, makeQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    checkIntegrityHash(name, item, CFSTR("4f1b64e3c156968916e72d8ff3f1a8eb78b32abe0b2b43f0578eb07c722aaf03"));
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("42d29fd5e9935edffcf6d0261eabddb00782ec775caa93716119e8e553ab5578"));

    item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("bdf219cdbc2dc6c4521cf39d1beda2e3491ef0330ba59eb41229dd909632f48d"));

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testKeychainUpgradeTests (openCustomKeychainTests \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + \
        + checkNTests + checkIntegrityHashTests + \
        + checkNTests + checkIntegrityHashTests + \
        + openCustomKeychainTests \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests +\
        + checkNTests + checkIntegrityHashTests +\
        + checkNTests + checkIntegrityHashTests +\
        1)

static SecAccessRef makeUidAccess(uid_t uid)
{
    // make the "uid/gid" ACL subject
    // this is a CSSM_LIST_ELEMENT chain
    CSSM_ACL_PROCESS_SUBJECT_SELECTOR selector = {
        CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION, // selector version
        CSSM_ACL_MATCH_UID, // set mask: match uids (only)
        uid,                // uid to match
        0                   // gid (not matched here)
    };
    CSSM_LIST_ELEMENT subject2 = { NULL, 0 };
    subject2.Element.Word.Data = (UInt8 *)&selector;
    subject2.Element.Word.Length = sizeof(selector);
    CSSM_LIST_ELEMENT subject1 = {
        &subject2, CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_LIST_ELEMENT_WORDID
    };

    // rights granted (replace with individual list if desired)
    CSSM_ACL_AUTHORIZATION_TAG rights[] = {
        CSSM_ACL_AUTHORIZATION_ANY  // everything
    };
    // owner component (right to change ACL)
    CSSM_ACL_OWNER_PROTOTYPE owner = {
        // TypedSubject
        { CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
        // Delegate
        false
    };
    // ACL entries (any number, just one here)
    CSSM_ACL_ENTRY_INFO acls[] = {
        {
            // prototype
            {
                // TypedSubject
                { CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
                false, // Delegate
                // rights for this entry
                { sizeof(rights) / sizeof(rights[0]), rights },
                // rest is defaulted
            }
        }
    };

    SecAccessRef access;
    SecAccessCreateFromOwnerAndACL(&owner, sizeof(acls) / sizeof(acls[0]), acls, &access);
    return access;
}

static void checkAccessLength(const char * name, SecAccessRef access, int expected) {
    CFArrayRef acllist = NULL;
    ok_status(SecAccessCopyACLList(access, &acllist), "%s: SecAccessCopyACLList", name);

    // Count the number of non-integrity ACLs in this access
    int aclsFound = 0;
    CFStringRef output = NULL;

    if(acllist) {
        for(int i = 0; i < CFArrayGetCount(acllist); i++) {
            SecACLRef acl = (SecACLRef) CFArrayGetValueAtIndex(acllist, i);

            CFArrayRef auths = SecACLCopyAuthorizations(acl);
            CFRange searchrange = CFRangeMake(0, CFArrayGetCount(auths));
            if(!CFArrayContainsValue(auths, searchrange, kSecACLAuthorizationIntegrity) &&
               !CFArrayContainsValue(auths, searchrange, kSecACLAuthorizationPartitionID)) {

                aclsFound += 1;
            }

            CFReleaseNull(auths);
        }

        CFReleaseNull(acllist);
    }
    is(aclsFound, expected, "%s: ACL has correct number of entries", name);
}
#define checkAccessLengthTests 2

static void testUidAccess() {
    char name[100];
    sprintf(name, "testUidAccess");
    secdebugfunc("integrity", "************************************* %s", name);

    SecAccessRef access = makeUidAccess(getuid());

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = makeAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    SecKeychainItemRef item = checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainItemSetAccess(item, access), "%s: SecKeychainItemSetAccess", name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    // Check to make sure the ACL stays
    access = NULL;
    ok_status(SecKeychainItemCopyAccess(item, &access), "%s: SecKeychainItemCopyAccess", name);
    checkAccessLength(name, access, 2);

    const char * newPassword = "newPassword";
    ok_status(SecKeychainItemModifyContent(item, NULL, (UInt32) strlen(newPassword), newPassword), "%s: SecKeychainItemModifyContent", name);

    access = NULL;
    ok_status(SecKeychainItemCopyAccess(item, &access), "%s: SecKeychainItemCopyAccess", name);
    checkAccessLength(name, access, 2);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testUidAccessTests (newKeychainTests + 2 + checkNTests + 1 + checkNTests + 1 + checkAccessLengthTests \
        + 2 + checkAccessLengthTests + 1)


static SecAccessRef makeMultipleUidAccess(uid_t* uids, uint32 count)
{
    // rights granted (replace with individual list if desired)
    CSSM_ACL_AUTHORIZATION_TAG rights[] =
    {
        CSSM_ACL_AUTHORIZATION_ANY    // everything
    };
    size_t numRights = sizeof(rights) / sizeof(rights[0]);

    // allocate the arrays of objects used to define the ACL
    CSSM_ACL_PROCESS_SUBJECT_SELECTOR selectors[count];
    CSSM_LIST_ELEMENT heads[count], tails[count];
    CSSM_ACL_ENTRY_INFO acls[count];
    // clear all the ACL objects
    memset(heads, 0, sizeof(heads));
    memset(acls, 0, sizeof(acls));

    uint32 i = count;
    while (i--)
    {
        // make the "uid/gid" ACL subject
        selectors[i].version = CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION;
        selectors[i].mask = CSSM_ACL_MATCH_UID;        // set mask: match uids (only)
        selectors[i].uid = uids[i];                    // uid to match
        selectors[i].gid = 0;                        // gid (not matched here)

        // this is a CSSM_LIST_ELEMENT chain
        heads[i].NextElement = &(tails[i]);
        heads[i].WordID = CSSM_ACL_SUBJECT_TYPE_PROCESS;
        heads[i].ElementType = CSSM_LIST_ELEMENT_WORDID;
        // Element is unused

        tails[i].NextElement = NULL;
        tails[i].WordID = CSSM_WORDID__NLU_;
        tails[i].ElementType = CSSM_LIST_ELEMENT_DATUM;
        tails[i].Element.Word.Data = (UInt8 *)&selectors[i];
        tails[i].Element.Word.Length = sizeof(selectors[i]);

        // ACL entry
        acls[i].EntryPublicInfo.TypedSubject.ListType = CSSM_LIST_TYPE_UNKNOWN;
        acls[i].EntryPublicInfo.TypedSubject.Head = &heads[i];
        acls[i].EntryPublicInfo.TypedSubject.Tail = &tails[i];
        acls[i].EntryPublicInfo.Delegate = CSSM_FALSE;
        acls[i].EntryPublicInfo.Authorization.NumberOfAuthTags = (uint32) numRights;

        acls[i].EntryPublicInfo.Authorization.AuthTags = rights;
        acls[i].EntryHandle = i;
    }

    // owner component (right to change ACL)
    CSSM_ACL_OWNER_PROTOTYPE owner;
    owner.TypedSubject = acls[0].EntryPublicInfo.TypedSubject;
    owner.Delegate = acls[0].EntryPublicInfo.Delegate;

    SecAccessRef access;
    SecAccessCreateFromOwnerAndACL(&owner, count, acls, &access);
    return access;
}
static void testMultipleUidAccess() {
    char name[100];
    sprintf(name, "testMultipleUidAccess");
    secdebugfunc("integrity", "************************************* %s", name);

    uid_t uids[5];
    uids[0] = getuid();
    uids[1] = 0;
    uids[2] = 500;
    uids[3] = 501;
    uids[4] = 502;

    SecAccessRef access = makeMultipleUidAccess(uids, 5);

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = makeAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testMultipleUidAccessTests (newKeychainTests + checkNTests + 3)

static void testRootUidAccess() {
    char name[100];
    sprintf(name, "testRootUidAccess");
    secdebugfunc("integrity", "************************************* %s", name);

    SecAccessRef access = SecAccessCreateWithOwnerAndACL(getuid(), 0, (kSecUseOnlyUID | kSecHonorRoot), NULL, NULL);

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = makeAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    query = makeQueryItemDictionary(kc, kSecClassGenericPassword);

    SecKeychainItemRef item = checkN(name, query, 1);

    ok_status(SecKeychainItemSetAccess(item, access), "%s: SecKeychainItemSetAccess", name);
    checkN(name, makeQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testRootUidAccessTests (newKeychainTests + checkNTests + 4 + checkNTests)

#define kTestCount (0 \
       + testAddItemTests \
       + testAddItemTests \
       + testCopyMatchingItemTests \
       + testCopyMatchingItemTests \
       + testUpdateItemTests \
       + testUpdateItemTests \
       + testAddDuplicateItemTests \
       + testAddDuplicateItemTests \
       + testDeleteItemTests \
       + testDeleteItemTests \
       + testUpdateRetainedItemTests \
       + testUpdateRetainedItemTests \
       \
       + testAddKeyTests \
       + testAddFreeKeyTests \
       + testCopyMatchingKeyTests \
       + testUpdateKeyTests \
       + testAddDuplicateKeyTests \
       + testKeyPairTests \
       + testExportImportKeyPairTests \
       \
       + initializeDLTests \
       + testAttackItemTests \
       + testAttackKeyTests \
       + testAddAfterCorruptItemTests \
       + testAddAfterCorruptKeyTests \
       + unloadDLTests \
       \
       + testKeychainUpgradeTests \
       + testUidAccessTests \
       + testMultipleUidAccessTests \
       + testRootUidAccessTests \
       )

static void tests(void)
{
    const char *home_dir = getenv("HOME");
    sprintf(keychainFile, "%s/Library/Keychains/test.keychain", home_dir);
    sprintf(keychainName, "test.keychain");

    testAddItem(kSecClassGenericPassword,  CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    testAddItem(kSecClassInternetPassword, CFSTR("be34c4562153063ce9cdefc2c34451d5e6e98a447f293d68a67349c1b5d1164f"));

    testCopyMatchingItem(kSecClassGenericPassword,  CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    testCopyMatchingItem(kSecClassInternetPassword, CFSTR("be34c4562153063ce9cdefc2c34451d5e6e98a447f293d68a67349c1b5d1164f"));

    testUpdateItem(kSecClassGenericPassword,  CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"),
                                              CFSTR("7b7be2fd6ee9f81ba4c5575ea451f2c21117fc0f241625a6cf90c65180b8c9f5"));
    testUpdateItem(kSecClassInternetPassword, CFSTR("be34c4562153063ce9cdefc2c34451d5e6e98a447f293d68a67349c1b5d1164f"),
                                              CFSTR("d71af9e4d54127a5dbc10c5ec097b828065cfbaf2b775caf1a3c4e3410f80851"));

    testAddDuplicateItem(kSecClassGenericPassword,  CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    testAddDuplicateItem(kSecClassInternetPassword, CFSTR("be34c4562153063ce9cdefc2c34451d5e6e98a447f293d68a67349c1b5d1164f"));

    testDeleteItem(kSecClassGenericPassword,  CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    testDeleteItem(kSecClassInternetPassword, CFSTR("be34c4562153063ce9cdefc2c34451d5e6e98a447f293d68a67349c1b5d1164f"));

    testUpdateRetainedItem(kSecClassGenericPassword);
    testUpdateRetainedItem(kSecClassInternetPassword);

    testAddKey(         CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    testAddFreeKey(     CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    testCopyMatchingKey(CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    testUpdateKey(      CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"),
                        CFSTR("a744ce6db8359ad264ed5f4a35ecfcc8b6599b89319e7ea316035acd3fb02c22"));
    testAddDuplicateKey(CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    testAddDuplicateFreeKey(CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    testKeyPair();
    testExportImportKeyPair();

    CSSM_DL_DB_HANDLE dldbHandle = initializeDL();
    testAttackItem(dldbHandle);
    testAttackKey(dldbHandle);

    testAddAfterCorruptItem(dldbHandle);
    testAddAfterCorruptKey(dldbHandle);
    unloadDL(&dldbHandle);

    testKeychainUpgrade();
    testUidAccess();
    testMultipleUidAccess();
    testRootUidAccess();

    //makeOldKeychainBlob();
}

#pragma clang pop
#else

#define kTestCount (0)


static void tests(void)
{
}

#endif /* TARGET_OS_MAC */


int kc_30_xara(int argc, char *const *argv)
{
    plan_tests(kTestCount);

    tests();

    return 0;
}

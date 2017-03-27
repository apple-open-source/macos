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
#include <sys/stat.h>
#include <copyfile.h>
#include <unistd.h>

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
    CSSM_DL_DB_HANDLE dldbHandle = { 0, 0 };
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
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    CFReleaseNull(item);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_GENERIC_PASSWORD, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    readPasswordContentsWithResult(item, errSecInvalidItemRef, NULL);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAttackItemTests (newKeychainTests + checkNTests + makeItemWithIntegrityTests + checkNTests + modifyAttributeInKeychainTests + openKeychainTests + checkNTests + readPasswordContentsWithResultTests + 1)

static void testAttackKey(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAttackKey";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    SecKeychainItemRef item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    checkKeyUse((SecKeyRef) item, errSecSuccess);

    CFReleaseNull(item);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "Label", modification, strlen(modification));

    kc = openKeychain(name);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkKeyUse((SecKeyRef) item, errSecInvalidItemRef);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAttackKeyTests (newKeychainTests + checkNTests + makeKeyWithIntegrityTests + checkNTests + checkKeyUseTests + modifyAttributeInKeychainTests \
        + openKeychainTests + checkNTests + checkKeyUseTests + 1)


static void testAddAfterCorruptItem(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAddAfterCorruptItem";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeCustomItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("test_label"), CFSTR("265438ea6807b509c9c6962df3f5033fd1af118f76c5f550e3ed90cb0d3ffce4"));
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    makeDuplicateItem(name, kc, kSecClassGenericPassword);
    CFReleaseNull(kc);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_GENERIC_PASSWORD, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);
    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    deleteItem(item);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    makeCustomItemWithIntegrity(name, kc, kSecClassGenericPassword, CFSTR("evil_application"), CFSTR("d2aa97b30a1f96f9e61fcade2b00d9f4284976a83a5b68392251ee5ec827f8cc"));
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("evil_application"));
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAddAfterCorruptItemTests (newKeychainTests + checkNTests + makeCustomItemWithIntegrityTests + checkNTests + makeDuplicateItemTests \
        + modifyAttributeInKeychainTests + openKeychainTests + checkNTests + deleteItemTests \
        + checkNTests + makeCustomItemWithIntegrityTests + checkNTests + makeCustomDuplicateItemTests + 1)

static void testAddAfterCorruptKey(CSSM_DL_DB_HANDLE dldbHandle) {
    char * name = "testAddAfterCorruptKey";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    // Make a symmetric key
    makeCustomKeyWithIntegrity(name, kc, CFSTR("test_key"), CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    makeCustomDuplicateKey(name, kc, CFSTR("test_key"));
    CFReleaseNull(item);

    // Make a key pair
    SecKeyRef pub;
    SecKeyRef priv;
    makeCustomKeyPair(name, kc, CFSTR("test_key_pair"), &pub, &priv);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    CFReleaseNull(pub);
    CFReleaseNull(priv);

    ok_status(SecKeychainListRemoveKeychain(&kc), "%s: SecKeychainListRemoveKeychain", name);

    char * modification = "evil_application";
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "PrintName", modification, strlen(modification));
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_PUBLIC_KEY, "PrintName", modification, strlen(modification));
    modifyAttributeInKeychain(name, dldbHandle, keychainDbFile, CSSM_DL_DB_RECORD_PRIVATE_KEY, "PrintName", modification, strlen(modification));

    kc = openKeychain(name);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    deleteItem(item);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    deleteItem(item);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    deleteItem(item);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    makeCustomKeyWithIntegrity(name, kc, CFSTR("evil_application"), CFSTR("ca6d90a0b053113e43bbb67f64030230c96537f77601f66bdf821d8684431dfc"));
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    makeCustomDuplicateKey(name, kc, CFSTR("evil_application"));

    makeCustomKeyPair(name, kc, CFSTR("evil_application"), &pub, &priv);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    // We cannot create a duplicate key pair, so don't try.

    CFReleaseNull(item);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
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
        + checkNTests + deleteItemTests + checkNTests \
        + checkNTests + deleteItemTests + checkNTests \
        + checkNTests + deleteItemTests + checkNTests \
        + makeCustomKeyWithIntegrityTests + checkNTests \
        + makeCustomDuplicateKeyTests \
        + makeCustomKeyPairTests + checkNTests + checkNTests \
        + 1)


// These constants are in CommonBlob, but we're in C and can't access them
#define version_MacOS_10_0 0x00000100
#define version_partition 0x00000200

static void testKeychainUpgrade() {
    char name[100];
    sprintf(name, "testKeychainUpgrade");
    secnotice("integrity", "************************************* %s", name);
    UInt32 version;
    char* path = malloc(sizeof(char) * 400);
    UInt32 len = 400;

    // To test multi-threading, we want the upgrade to take a while. Add a bunch of passwords...
    char oldkcFile[100];
    sprintf(oldkcFile, "%s/Library/test.keychain", getenv("HOME"));
    unlink(oldkcFile);
    writeOldKeychain(name, oldkcFile);

    SecKeychainRef kc = openCustomKeychain(name, oldkcFile, "password");

    for(int i = 0; i < 200; i++) {
        CFTypeRef result = NULL;
        CFStringRef cflabel = CFStringCreateWithFormat(NULL, NULL, CFSTR("item%d"), i);
        CFMutableDictionaryRef query = createAddCustomItemDictionaryWithService(kc, kSecClassInternetPassword, cflabel, cflabel, CFSTR("no service"));
        SecItemAdd(query, &result); // don't particuluarly care if this fails...
        CFReleaseNull(query);
        CFReleaseNull(cflabel);
        CFReleaseNull(result);
    }

    CFReleaseNull(kc);

    ok_status(copyfile(oldkcFile, keychainFile, NULL, COPYFILE_UNLINK | COPYFILE_ALL), "%s: copyfile", name);
    unlink(oldkcFile);
    unlink(keychainDbFile);

    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t release_queue = NULL;
    dispatch_once(&onceToken, ^{
        release_queue = dispatch_queue_create("com.apple.security.keychain-upgrade-queue", DISPATCH_QUEUE_CONCURRENT);
    });

    dispatch_group_t g = dispatch_group_create();
    SecKeychainItemRef item;

    char* __block blockName = NULL;
    asprintf(&blockName, "%s", name);

    kc = openCustomKeychain(name, keychainName, "password");

    // Directly after an upgrade, no items should have partition ID lists
    dispatch_group_async(g, release_queue, ^() {
        secerror("beginning 1\n");
        SecKeychainRef blockKc;
        SecKeychainOpen(keychainName, &blockKc);
        SecKeychainItemRef item = checkNCopyFirst(blockName, createQueryItemDictionary(blockKc, kSecClassGenericPassword), 1);
        checkIntegrityHash(blockName, item, CFSTR("39c56eadd3e3b496b6099e5f3d5ff88eaee9ca2e3a50c1be8319807a72e451e5"));
        checkPartitionIDs(blockName, item, 0);
        CFReleaseSafe(blockKc);
        CFReleaseSafe(item);
        secerror("ending 1\n");
    });

    dispatch_group_async(g, release_queue, ^() {
        usleep(0.1 * USEC_PER_SEC); // use different timings to try to find multithreaded upgrade bugs
        secerror("beginning 2\n");
        SecKeychainRef blockKc;
        SecKeychainOpen(keychainName, &blockKc);
        SecKeychainItemRef item = checkNCopyFirst(blockName, createQueryItemDictionaryWithService(blockKc, kSecClassInternetPassword, CFSTR("test_service")), 1);
        checkIntegrityHash(blockName, item, CFSTR("4f1b64e3c156968916e72d8ff3f1a8eb78b32abe0b2b43f0578eb07c722aaf03"));
        checkPartitionIDs(blockName, item, 0);
        CFReleaseSafe(blockKc);
        CFReleaseSafe(item);
        secerror("ending 2\n");
    });

    dispatch_group_async(g, release_queue, ^() {
        usleep(0.3 * USEC_PER_SEC);
        secerror("beginning 3\n");
        SecKeychainRef blockKc;
        SecKeychainOpen(keychainName, &blockKc);
        SecKeychainItemRef item = checkNCopyFirst(blockName, createQueryKeyDictionary(blockKc, kSecAttrKeyClassSymmetric), 1);
        checkIntegrityHash(blockName, (SecKeychainItemRef) item, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
        checkPartitionIDs(blockName, (SecKeychainItemRef) item, 0);
        CFReleaseSafe(blockKc);
        CFReleaseSafe(item);
        secerror("ending 3\n");
    });

    dispatch_group_async(g, release_queue, ^() {
        usleep(0.5 * USEC_PER_SEC);
        secerror("beginning 4\n");
        SecKeychainRef blockKc;
        SecKeychainOpen(keychainName, &blockKc);
        SecKeychainItemRef item = checkNCopyFirst(blockName, createQueryKeyDictionary(blockKc, kSecAttrKeyClassPublic), 1);
        checkIntegrityHash(blockName, (SecKeychainItemRef) item, CFSTR("42d29fd5e9935edffcf6d0261eabddb00782ec775caa93716119e8e553ab5578"));
        checkPartitionIDs(blockName, (SecKeychainItemRef) item, 0);
        CFReleaseSafe(blockKc);
        CFReleaseSafe(item);
        secerror("ending 4\n");
    });

    dispatch_group_async(g, release_queue, ^() {
        usleep(1 * USEC_PER_SEC);
        secerror("beginning 5\n");
        SecKeychainRef blockKc;
        SecKeychainOpen(keychainName, &blockKc);
        SecKeychainItemRef item = checkNCopyFirst(blockName, createQueryKeyDictionary(blockKc, kSecAttrKeyClassPrivate), 1);
        checkIntegrityHash(blockName, (SecKeychainItemRef) item, CFSTR("bdf219cdbc2dc6c4521cf39d1beda2e3491ef0330ba59eb41229dd909632f48d"));
        checkPartitionIDs(blockName, (SecKeychainItemRef) item, 0);
        CFReleaseSafe(blockKc);
        CFReleaseSafe(item);
        secerror("ending 5\n");
    });

    dispatch_group_wait(g, DISPATCH_TIME_FOREVER);

    // @@@ I'm worried that there are still some thread issues in AppleDatabase; if these are run in the blocks above
    //     you can sometimes get CSSMERR_DL_INVALID_RECORD_UID/errSecInvalidRecord instead of errSecDuplicateItem
    //     <rdar://problem/27085024> Multi-threading duplicate item creation sometimes returns -67701
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    // Check the keychain's version and path
    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_partition, "%s: version of upgraded keychain is incorrect", name);
    ok_status(SecKeychainGetPath(kc, &len, path), "%s: SecKeychainGetKeychainPath", name);
    eq_stringn(path, len, keychainDbFile, strlen(keychainDbFile), "%s: paths do not match", name);
    free(path);

    // Now close the keychain and open it again
    CFReleaseNull(kc);
    kc = openCustomKeychain(name, keychainName, "password");

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkIntegrityHash(name, item, CFSTR("39c56eadd3e3b496b6099e5f3d5ff88eaee9ca2e3a50c1be8319807a72e451e5"));
    checkPartitionIDs(name, item, 0);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    checkIntegrityHash(name, item, CFSTR("4f1b64e3c156968916e72d8ff3f1a8eb78b32abe0b2b43f0578eb07c722aaf03"));
    checkPartitionIDs(name, item, 0);
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));
    checkPartitionIDs(name, (SecKeychainItemRef) item, 0);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("42d29fd5e9935edffcf6d0261eabddb00782ec775caa93716119e8e553ab5578"));
    checkPartitionIDs(name, (SecKeychainItemRef) item, 0);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("bdf219cdbc2dc6c4521cf39d1beda2e3491ef0330ba59eb41229dd909632f48d"));
    checkPartitionIDs(name, (SecKeychainItemRef) item, 0);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);

    // make sure we clean up any files left over
    unlink(keychainDbFile);
    unlink(keychainFile);
    unlink(oldkcFile);
}
#define testKeychainUpgradeTests (openCustomKeychainTests + 1 + openCustomKeychainTests + 4 \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + openCustomKeychainTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + 1)

// tests that SecKeychainCreate over an old .keychain file returns an empty keychain
static void testKeychainCreateOver() {
    char name[100];
    sprintf(name, "testKeychainCreateOver");
    secnotice("integrity", "************************************* %s", name);
    UInt32 version;
    char* path = malloc(sizeof(char) * 400);
    UInt32 len = 400;

    writeOldKeychain(name, keychainFile);
    unlink(keychainDbFile);

    SecKeychainItemRef item = NULL;

    // Check that we upgrade on SecKeychainOpen
    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkIntegrityHash(name, item, CFSTR("39c56eadd3e3b496b6099e5f3d5ff88eaee9ca2e3a50c1be8319807a72e451e5"));
    CFReleaseNull(item);

    ok_status(SecKeychainDelete(kc));
    CFReleaseNull(kc);

    // the old file should still exist, but the -db file should not.
    struct stat filebuf;
    is(stat(keychainFile, &filebuf), 0, "%s: check %s exists", name, keychainFile);
    isnt(stat(keychainDbFile, &filebuf), 0, "%s: check %s does not exist", name, keychainDbFile);

    // Now create a new keychain over the old remnants.
    ok_status(SecKeychainCreate(keychainFile, (UInt32) strlen("password"), "password", false, NULL, &kc), "%s: SecKeychainCreate", name);

    // Directly after creating a keychain, there shouldn't be any items (even though an old keychain exists underneath)
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 0);
    checkN(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    // Check the keychain's version and path
    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_partition, "%s: version of upgraded keychain is incorrect", name);
    ok_status(SecKeychainGetPath(kc, &len, path), "%s: SecKeychainGetKeychainPath", name);
    eq_stringn(path, len, keychainDbFile, strlen(keychainDbFile), "%s: paths do not match", name);
    free(path);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);

    // final check that the files on-disk are as we expect
    is(stat(keychainFile, &filebuf), 0, "%s: check %s exists", name, keychainFile);
    isnt(stat(keychainDbFile, &filebuf), 0, "%s: check %s does not exist", name, keychainDbFile);

    // make sure we clean up any files left over
    unlink(keychainDbFile);
    unlink(keychainFile);
}
#define testKeychainCreateOverTests (openCustomKeychainTests + \
+ checkNTests + checkIntegrityHashTests \
+ 1 + 2 + 1 \
+ checkNTests \
+ checkNTests \
+ checkNTests \
+ checkNTests \
+ checkNTests \
+ 4 + 1 + 2)

static void testKeychainDowngrade() {
    char *name = "testKeychainDowngrade";
    secnotice("integrity", "************************************* %s", name);

    // For now, don't worry about filenames
    writeFullV512Keychain(name, keychainDbFile);
    unlink(keychainFile);
    writeFullV512Keyfile(name, keychainTempFile);

    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");
    UInt32 version;

    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_partition, "%s: version of initial keychain is incorrect", name);

    SecKeychainItemRef item;

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkIntegrityHash(name, item, CFSTR("6ba8d9f77ddba54d9373b11ae5c8f7b55a5e81da27e05e86723eeceb0a9a8e0c"));
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    checkIntegrityHash(name, item, CFSTR("630a9fe4f0191db8a99d6e8455e7114f628ce8f0f9eb3559efa572a98877a2b2"));
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("44f10f6bb508d47f8905859efc06eaee500304bc4da408b1f4d2a58c6502147b"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("d27ee2be4920d5b6f47f6b19696d09c9a6c1a5d80c6f148f778db27b4ba99d9a"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, CFSTR("4b3f7bd7f9e48dc71006ce670990aed9dba6d5089b84d4113121bab41d0a3228"));



    ok_status(SecKeychainAttemptMigrationWithMasterKey(kc, version_MacOS_10_0, keychainTempFile), "%s: SecKeychainAttemptKeychainMigrationWithMasterKey", name);
    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_MacOS_10_0, "%s: version of downgraded keychain is incorrect", name);

    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));
    checkN(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);

    // make sure we clean up
    unlink(keychainTempFile);
    unlink(keychainDbFile);
    unlink(keychainFile);
}
#define testKeychainDowngradeTests (openCustomKeychainTests + 2 \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests + makeCustomDuplicateItemTests \
        + checkNTests + checkIntegrityHashTests +\
        + checkNTests + checkIntegrityHashTests +\
        + checkNTests + checkIntegrityHashTests +\
        + 3 + \
        + checkNTests + makeCustomDuplicateItemTests \
        + checkNTests + makeCustomDuplicateItemTests \
        + checkNTests \
        + checkNTests \
        + checkNTests \
        + 1)\

// Test opening and upgrading a v256 keychain at a -db filename.
static void testKeychainWrongFile256() {
    char name[100];
    sprintf(name, "testKeychainWrongFile256");
    secnotice("integrity", "************************************* %s", name);
    UInt32 version;

    unlink(keychainFile);
    writeOldKeychain(name, keychainDbFile);

    // Only keychainDb file should exist
    struct stat filebuf;
    isnt(stat(keychainFile, &filebuf), 0, "%s: %s exists and shouldn't", name, keychainFile);
    is(stat(keychainDbFile, &filebuf), 0, "%s: %s does not exist", name, keychainDbFile);

    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    SecKeychainItemRef item;

    // Iterate over the keychain to trigger upgrade
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    // We should have created keychainFile, check for it
    is(stat(keychainFile, &filebuf), 0, "%s: %s does not exist", name, keychainFile);
    is(stat(keychainDbFile, &filebuf), 0, "%s: %s does not exist", name, keychainDbFile);

    // Check the keychain's version and path
    char path[400];
    UInt32 len = sizeof(path);

    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_partition, "%s: version of re-upgraded keychain is incorrect", name);
    ok_status(SecKeychainGetPath(kc, &len, path), "%s: SecKeychainGetPath", name);
    eq_stringn(path, len, keychainDbFile, strlen(keychainDbFile), "%s: paths do not match", name);

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);

    // make sure we clean up any files left over
    unlink(keychainDbFile);
    unlink(keychainFile);
}
#define testKeychainWrongFile256Tests (2 + openCustomKeychainTests \
        + checkNTests + makeCustomDuplicateItemTests \
        + 2 + 4 \
        + checkNTests + makeCustomDuplicateItemTests \
        + checkNTests + makeCustomDuplicateItemTests \
        + checkNTests \
        + checkNTests \
        + checkNTests \
        + 1)

// Test opening and upgrading a v512 keychain at a .keychain filename.
static void testKeychainWrongFile512() {
    char name[100];
    sprintf(name, "testKeychainWrongFile512");
    secnotice("integrity", "************************************* %s", name);
    UInt32 version;

    writeFullV512Keychain(name, keychainFile);
    unlink(keychainDbFile);

    // Only keychain file should exist
    struct stat filebuf;
    isnt(stat(keychainDbFile, &filebuf), 0, "%s: %s exists and shouldn't", name, keychainFile);
    is(stat(keychainFile, &filebuf), 0, "%s: %s does not exist", name, keychainDbFile);

    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    SecKeychainItemRef item;

    // Iterate over the keychain to trigger upgrade
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    // We should have move the keychain to keychainDbFile, check for it
    isnt(stat(keychainFile, &filebuf), 0, "%s: %s still exists", name, keychainFile);
    is(stat(keychainDbFile, &filebuf), 0, "%s: %s does not exist", name, keychainDbFile);

    // Check the keychain's version and path
    char path[400];
    UInt32 len = sizeof(path);

    ok_status(SecKeychainGetKeychainVersion(kc, &version), "%s: SecKeychainGetKeychainVersion", name);
    is(version, version_partition, "%s: version of moved keychain is incorrect", name);
    ok_status(SecKeychainGetPath(kc, &len, path), "%s: SecKeychainGetPath", name);
    eq_stringn(path, len, keychainDbFile, strlen(keychainDbFile), "%s: paths do not match", name);

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    makeCustomDuplicateItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);

    // make sure we clean up any files left over
    unlink(keychainDbFile);
    unlink(keychainFile);
}
#define testKeychainWrongFile512Tests (2 + openCustomKeychainTests \
+ checkNTests + makeCustomDuplicateItemTests \
+ 2 + 4 \
+ checkNTests + makeCustomDuplicateItemTests \
+ checkNTests + makeCustomDuplicateItemTests \
+ checkNTests \
+ checkNTests \
+ checkNTests \
+ 1)


#undef version_partition
#undef version_MacOS_10_0

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
    secnotice("integrity", "************************************* %s", name);

    SecAccessRef access = makeUidAccess(getuid());

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = createAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainItemSetAccess(item, access), "%s: SecKeychainItemSetAccess", name);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);

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
    CFReleaseNull(kc);
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
    secnotice("integrity", "************************************* %s", name);

    uid_t uids[5];
    uids[0] = getuid();
    uids[1] = 0;
    uids[2] = 500;
    uids[3] = 501;
    uids[4] = 502;

    SecAccessRef access = makeMultipleUidAccess(uids, 5);

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = createAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testMultipleUidAccessTests (newKeychainTests + checkNTests + 3)

static void testRootUidAccess() {
    char name[100];
    sprintf(name, "testRootUidAccess");
    secnotice("integrity", "************************************* %s", name);

    SecAccessRef access = SecAccessCreateWithOwnerAndACL(getuid(), 0, (kSecUseOnlyUID | kSecHonorRoot), NULL, NULL);

    SecKeychainRef kc = newKeychain(name);
    CFMutableDictionaryRef query = createAddItemDictionary(kc, kSecClassGenericPassword, CFSTR("test label"));
    CFDictionarySetValue(query, kSecAttrAccess, access);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    query = createQueryItemDictionary(kc, kSecClassGenericPassword);

    SecKeychainItemRef item = checkNCopyFirst(name, query, 1);

    ok_status(SecKeychainItemSetAccess(item, access), "%s: SecKeychainItemSetAccess", name);
    CFReleaseNull(access);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testRootUidAccessTests (newKeychainTests + checkNTests + 4 + checkNTests)

static void testBadACL() {
    char name[100];
    sprintf(name, "testBadACL");
    secnotice("integrity", "************************************* %s", name);

    SecKeychainItemRef item = NULL;

    unlink(keychainFile);
    writeFullV512Keychain(name, keychainDbFile);

    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    // Check that these exist in this keychain...
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    checkN(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFRelease(kc);

    // Corrupt all the ACLs, by changing the partition id plist entry
    uint8_t * fileBuffer = (uint8_t*) malloc(FULL_V512_SIZE);
    memcpy(fileBuffer, full_v512, FULL_V512_SIZE);

    void* p;
    char * str = "<key>Partitions</key>";
    while( (p = memmem(fileBuffer, FULL_V512_SIZE, (void*) str, strlen(str))) ) {
        *(uint8_t*) p = 0;
    }
    writeFile(keychainDbFile, fileBuffer, FULL_V512_SIZE);
    free(fileBuffer);

    kc = openCustomKeychain(name, keychainName, "password");

    // These items exist in this keychain, but their ACL is corrupted. We should be able to find them, but not fetch data.
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    readPasswordContentsWithResult(item, errSecInvalidItemRef, NULL); // we don't expect to be able to read this
    deleteItem(item);
    checkN(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 0);

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    readPasswordContentsWithResult(item, errSecInvalidItemRef, NULL); // we don't expect to be able to read this
    deleteItem(item);
    checkN(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 0);

    // These should work
    makeItem(name, kc, kSecClassGenericPassword, CFSTR("test_generic"));
    makeItem(name, kc, kSecClassInternetPassword, CFSTR("test_internet"));

    // And now the items should exist
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    readPasswordContents(item, CFSTR("data"));
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);
    readPasswordContents(item, CFSTR("data"));

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testBadACLTests (openCustomKeychainTests + checkNTests * 2 + 1 + openCustomKeychainTests \
       + 2*(checkNTests + readPasswordContentsWithResultTests + deleteItemTests + checkNTests) \
       + makeItemTests*2 + checkNTests*2 + readPasswordContentsTests*2 + 1)

static void testIterateLockedKeychain() {
    char name[100];
    sprintf(name, "testIterateLockedKeychain");
    secnotice("integrity", "************************************* %s", name);

    SecKeychainItemRef item = NULL;

    unlink(keychainFile);
    writeFullV512Keychain(name, keychainDbFile);

    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    ok_status(SecKeychainLock(kc), "%s: SecKeychainLock", name);

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassGenericPassword), 1);
    item = checkNCopyFirst(name, createQueryItemDictionary(kc, kSecClassInternetPassword), 1);

    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testIterateLockedKeychainTests (openCustomKeychainTests + 1 + checkNTests*5 + 1)

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
       + testKeychainCreateOverTests \
       + testKeychainDowngradeTests \
       + testKeychainWrongFile256Tests \
       + testKeychainWrongFile512Tests \
       + testUidAccessTests \
       + testMultipleUidAccessTests \
       + testRootUidAccessTests \
       + testBadACLTests \
       + testIterateLockedKeychainTests \
       )

static void tests(void)
{
    initializeKeychainTests("kc-30-xara");

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
    testKeychainCreateOver();
    testKeychainDowngrade();
    testKeychainWrongFile256();
    testKeychainWrongFile512();
    testUidAccess();
    testMultipleUidAccess();
    testRootUidAccess();
    testBadACL();
    testIterateLockedKeychain();

    //makeOldKeychainBlob();
}

#pragma clang diagnostic pop
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

    deleteTestFiles();
    return 0;
}

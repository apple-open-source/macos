/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include "SOSTestDataSource.h"

#include <corecrypto/ccder.h>
#include <Security/SecureObjectSync/SOSDataSource.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSViews.h>

#include <utilities/array_size.h>
#include <utilities/der_plist.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <AssertMacros.h>

CFStringRef sSOSDataSourceErrorDomain = CFSTR("com.apple.datasource");

typedef struct SOSTestDataSource *SOSTestDataSourceRef;

struct SOSTestDataSource {
    struct SOSDataSource ds;
    unsigned gm_count;
    unsigned cm_count;
    unsigned co_count;
    CFMutableDictionaryRef d2database;
    CFMutableDictionaryRef p2database;
    CFMutableDictionaryRef statedb;
    uint8_t manifest_digest[SOSDigestSize];
    bool clean;

    CFMutableArrayRef changes;
    SOSDataSourceNotifyBlock notifyBlock;
};

typedef struct SOSTestDataSourceFactory *SOSTestDataSourceFactoryRef;

struct SOSTestDataSourceFactory {
    struct SOSDataSourceFactory dsf;
    CFMutableDictionaryRef data_sources;
};


/* DataSource protocol. */
static SOSManifestRef dsCopyManifestWithViewNameSet(SOSDataSourceRef data_source, CFSetRef viewNameSet, CFErrorRef *error) {
    if (!CFSetContainsValue(viewNameSet, kSOSViewKeychainV0))
        return SOSManifestCreateWithData(NULL, error);

    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    ds->cm_count++;
    __block struct SOSDigestVector dv = SOSDigestVectorInit;
    CFDictionaryForEach(ds->d2database, ^(const void *key, const void *value) {
        SOSDigestVectorAppend(&dv, CFDataGetBytePtr((CFDataRef)key));
    });
    SOSDigestVectorSort(&dv);
    SOSManifestRef manifest = SOSManifestCreateWithDigestVector(&dv, error);
    SOSDigestVectorFree(&dv);
    ccdigest(ccsha1_di(), SOSManifestGetSize(manifest), SOSManifestGetBytePtr(manifest), ds->manifest_digest);
    ds->clean = true;

    return manifest;
}

static bool foreach_object(SOSDataSourceRef data_source, SOSTransactionRef txn, SOSManifestRef manifest, CFErrorRef *error, void (^handle_object)(CFDataRef key, SOSObjectRef object, bool *stop)) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    ds->co_count++;
    __block bool result = true;
    SOSManifestForEach(manifest, ^(CFDataRef key, bool *stop) {
        handle_object(key, (SOSObjectRef)CFDictionaryGetValue(ds->d2database, key), stop);
    });
    return result;
}

static bool dispose(SOSDataSourceRef data_source, CFErrorRef *error) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    CFReleaseSafe(ds->d2database);
    CFReleaseSafe(ds->p2database);
    CFReleaseSafe(ds->statedb);
    CFReleaseSafe(ds->changes);
    free(ds);
    return true;
}

static SOSObjectRef createWithPropertyList(CFDictionaryRef plist, CFErrorRef *error) {
    return (SOSObjectRef)CFDictionaryCreateCopy(kCFAllocatorDefault, plist);
}

static CFDataRef SOSObjectCopyDER(SOSObjectRef object, CFErrorRef *error) {
    CFDictionaryRef dict = (CFDictionaryRef)object;
    size_t size = der_sizeof_plist(dict, error);
    CFMutableDataRef data = CFDataCreateMutable(0, size);
    if (data) {
        CFDataSetLength(data, size);
        uint8_t *der = (uint8_t *)CFDataGetMutableBytePtr(data);
        uint8_t *der_end = der + size;
        der_end = der_encode_plist(dict, error, der, der_end);
        assert(der_end == der);
        (void)der_end;
    } else if (error && *error == NULL) {
        *error = CFErrorCreate(0, sSOSDataSourceErrorDomain, kSOSDataSourceObjectMallocFailed, NULL);
    }
    return data;
}

static CFDataRef ccdigest_copy_data(const struct ccdigest_info *di, size_t len,
                                    const void *data, CFErrorRef *error) {
    CFMutableDataRef digest = CFDataCreateMutable(0, di->output_size);
    if (digest) {
        CFDataSetLength(digest, di->output_size);
        ccdigest(di, len, data, CFDataGetMutableBytePtr(digest));
    } else if (error && *error == NULL) {
        *error = CFErrorCreate(0, sSOSDataSourceErrorDomain, kSOSDataSourceObjectMallocFailed, NULL);
    }
    return digest;
}

static CFDataRef copyDigest(SOSObjectRef object, CFErrorRef *error) {
    CFMutableDictionaryRef ocopy = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, (CFDictionaryRef)object);
    CFDictionaryRemoveValue(ocopy, kSecClass);
    CFDataRef der = SOSObjectCopyDER((SOSObjectRef)ocopy, error);
    CFRelease(ocopy);
    CFDataRef digest = NULL;
    if (der) {
        digest = ccdigest_copy_data(ccsha1_di(), CFDataGetLength(der), CFDataGetBytePtr(der), error);
        CFRelease(der);
    }
    return digest;
}

static CFDateRef copyModDate(SOSObjectRef object, CFErrorRef *error) {
    return CFRetainSafe(asDate(CFDictionaryGetValue((CFDictionaryRef) object, kSecAttrModificationDate), NULL));
}

static CFDataRef copyPrimaryKey(SOSObjectRef object, CFErrorRef *error) {
    CFMutableDictionaryRef ocopy = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFTypeRef pkNames[] = {
        CFSTR("acct"),
        CFSTR("agrp"),
        CFSTR("svce"),
        CFSTR("sync"),
        CFSTR("sdmn"),
        CFSTR("srvr"),
        CFSTR("ptcl"),
        CFSTR("atyp"),
        CFSTR("port"),
        CFSTR("path"),
        CFSTR("ctyp"),
        CFSTR("issr"),
        CFSTR("slnr"),
        CFSTR("kcls"),
        CFSTR("klbl"),
        CFSTR("atag"),
        CFSTR("crtr"),
        CFSTR("type"),
        CFSTR("bsiz"),
        CFSTR("esiz"),
        CFSTR("sdat"),
        CFSTR("edat"),
    };
    CFSetRef pkAttrs = CFSetCreate(kCFAllocatorDefault, pkNames, array_size(pkNames), &kCFTypeSetCallBacks);
    CFDictionaryForEach((CFDictionaryRef)object, ^(const void *key, const void *value) {
        if (CFSetContainsValue(pkAttrs, key))
            CFDictionaryAddValue(ocopy, key, value);
    });
    CFRelease(pkAttrs);
    CFDataRef der = SOSObjectCopyDER((SOSObjectRef)ocopy, error);
    CFRelease(ocopy);
    CFDataRef digest = NULL;
    if (der) {
        digest = ccdigest_copy_data(ccsha1_di(), CFDataGetLength(der), CFDataGetBytePtr(der), error);
        CFRelease(der);
    }
    return digest;
}

static CFDictionaryRef copyPropertyList(SOSObjectRef object, CFErrorRef *error) {
    return (CFDictionaryRef) CFRetain(object);
}

// Return the newest object
static SOSObjectRef copyMergedObject(SOSObjectRef object1, SOSObjectRef object2, CFErrorRef *error) {
    CFDictionaryRef dict1 = (CFDictionaryRef)object1;
    CFDictionaryRef dict2 = (CFDictionaryRef)object2;
    SOSObjectRef result = NULL;
    CFDateRef m1, m2;
    m1 = CFDictionaryGetValue(dict1, kSecAttrModificationDate);
    m2 = CFDictionaryGetValue(dict2, kSecAttrModificationDate);
    switch (CFDateCompare(m1, m2, NULL)) {
        case kCFCompareGreaterThan:
            result = (SOSObjectRef)dict1;
            break;
        case kCFCompareLessThan:
            result = (SOSObjectRef)dict2;
            break;
        case kCFCompareEqualTo:
        {
            // Return the item with the smallest digest.
            CFDataRef digest1 = copyDigest(object1, error);
            CFDataRef digest2 = copyDigest(object2, error);
            if (digest1 && digest2) switch (CFDataCompare(digest1, digest2)) {
                case kCFCompareGreaterThan:
                case kCFCompareEqualTo:
                    result = (SOSObjectRef)dict2;
                    break;
                case kCFCompareLessThan:
                    result = (SOSObjectRef)dict1;
                    break;
            }
            CFReleaseSafe(digest2);
            CFReleaseSafe(digest1);
            break;
        }
    }
    CFRetainSafe(result);
    return result;
}

static SOSMergeResult mergeObject(SOSTransactionRef txn, SOSObjectRef object, SOSObjectRef *mergedObject, CFErrorRef *error) {
    SOSTestDataSourceRef ds = (SOSTestDataSourceRef)txn;
    SOSMergeResult mr = kSOSMergeFailure;
    CFDataRef pk = copyPrimaryKey(object, error);
    if (!pk) return mr;
    SOSObjectRef myObject = (SOSObjectRef)CFDictionaryGetValue(ds->p2database, pk);
    if (myObject) {
        SOSObjectRef merged = copyMergedObject(object, myObject, error);
        if (mergedObject) *mergedObject = CFRetainSafe(merged);
        if (CFEqualSafe(merged, myObject)) {
            mr = kSOSMergeLocalObject;
        } else if (CFEqualSafe(merged, object)) {
            mr = kSOSMergePeersObject;
        } else {
            mr = kSOSMergeCreatedObject;
        }
        if (mr != kSOSMergeLocalObject) {
            CFDataRef myKey = copyDigest(myObject, error);
            CFDictionaryRemoveValue(ds->d2database, myKey);
            CFReleaseSafe(myKey);
            CFDataRef key = copyDigest(merged, error);
            CFDictionarySetValue(ds->d2database, key, merged);
            const void *values[2] = { myObject, merged };
            CFTypeRef entry = CFArrayCreate(kCFAllocatorDefault, values, 2, &kCFTypeArrayCallBacks);
            if (entry) {
                CFArrayAppendValue(ds->changes, entry);
                CFRelease(entry);
            }
            CFReleaseSafe(key);
            CFDictionarySetValue(ds->p2database, pk, merged);
        }
        CFReleaseSafe(merged);
    } else {
        SOSTestDataSourceAddObject((SOSDataSourceRef)ds, object, error);
        mr = kSOSMergePeersObject;
    }
    CFReleaseSafe(pk);
    return mr;
}

static CFStringRef dsGetName(SOSDataSourceRef ds) {
    return CFSTR("The sky is made of butterflies");
}

static void dsAddNotifyPhaseBlock(SOSDataSourceRef ds, SOSDataSourceNotifyBlock notifyBlock) {
    SOSTestDataSourceRef tds = (SOSTestDataSourceRef)ds;
    assert(tds->notifyBlock == NULL);
    tds->notifyBlock = Block_copy(notifyBlock);
}

static CFDataRef dsCopyStateWithKey(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error) {
    SOSTestDataSourceRef tds = (SOSTestDataSourceRef)ds;
    CFStringRef dbkey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-%@"), pdmn, key);
    CFDataRef state = CFDictionaryGetValue(tds->statedb, dbkey);
    CFReleaseSafe(dbkey);
    return CFRetainSafe(state);
}

static CFDataRef dsCopyItemDataWithKeys(SOSDataSourceRef data_source, CFDictionaryRef keys, CFErrorRef *error) {
    SecError(errSecUnimplemented, error, CFSTR("dsCopyItemDataWithKeys on test data source not implemented"));
    return NULL;
}

static bool dsWith(SOSDataSourceRef ds, CFErrorRef *error, SOSDataSourceTransactionSource source, bool onCommitQueue, void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    SOSTestDataSourceRef tds = (SOSTestDataSourceRef)ds;
    bool commit = true;
    transaction((SOSTransactionRef)ds, &commit);
    if (commit && ((SOSTestDataSourceRef)ds)->notifyBlock && (CFArrayGetCount(tds->changes))) {
        ((SOSTestDataSourceRef)ds)->notifyBlock(ds, (SOSTransactionRef)ds, kSOSDataSourceTransactionWillCommit, source, tds->changes);
        CFArrayRemoveAllValues(tds->changes);
    }
    return true;
}

static bool dsReadWith(SOSDataSourceRef ds, CFErrorRef *error, SOSDataSourceTransactionSource source, void(^perform)(SOSTransactionRef txn)) {
    SOSTestDataSourceRef tds = (SOSTestDataSourceRef)ds;
    perform((SOSTransactionRef)tds);
    return true;
}

static bool dsSetStateWithKey(SOSDataSourceRef ds, SOSTransactionRef txn, CFStringRef key, CFStringRef pdmn, CFDataRef state, CFErrorRef *error) {
    SOSTestDataSourceRef tds = (SOSTestDataSourceRef)ds;
    CFStringRef dbkey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-%@"), pdmn, key);
    CFDictionarySetValue(tds->statedb, dbkey, state);
    CFReleaseSafe(dbkey);
    return true;
}

static bool dsRestoreObject(SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error) {
    // TODO: Just call merge, probably doesn't belong in protocol at all
    assert(false);
    return true;
}

static CFDictionaryRef objectCopyBackup(SOSObjectRef object, uint64_t handle, CFErrorRef *error) {
    // OMG We failed without an error.
    assert(false);
    return NULL;
}

SOSDataSourceRef SOSTestDataSourceCreate(void) {
    SOSTestDataSourceRef ds = calloc(1, sizeof(struct SOSTestDataSource));

    ds->ds.engine = NULL;
    ds->ds.dsGetName = dsGetName;
    ds->ds.dsAddNotifyPhaseBlock = dsAddNotifyPhaseBlock;
    ds->ds.dsCopyManifestWithViewNameSet = dsCopyManifestWithViewNameSet;
    ds->ds.dsForEachObject = foreach_object;
    ds->ds.dsCopyStateWithKey = dsCopyStateWithKey;
    ds->ds.dsCopyItemDataWithKeys = dsCopyItemDataWithKeys;

    ds->ds.dsWith = dsWith;
    ds->ds.dsRelease = dispose;
    ds->ds.dsReadWith = dsReadWith;

    ds->ds.dsMergeObject = mergeObject;
    ds->ds.dsSetStateWithKey = dsSetStateWithKey;
    ds->ds.dsRestoreObject = dsRestoreObject;

    ds->ds.objectCopyDigest = copyDigest;
    ds->ds.objectCopyModDate = copyModDate;
    ds->ds.objectCreateWithPropertyList = createWithPropertyList;
    ds->ds.objectCopyPropertyList = copyPropertyList;
    ds->ds.objectCopyBackup = objectCopyBackup;

    ds->d2database = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ds->p2database = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ds->statedb = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ds->clean = false;
    ds->changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    return (SOSDataSourceRef)ds;
}

static CFStringRef SOSTestDataSourceFactoryCopyName(SOSDataSourceFactoryRef factory)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;

    __block CFStringRef result = NULL;
    CFDictionaryForEach(dsf->data_sources, ^(const void*key, const void*value) { if (isString(key)) result = key; });
    
    return CFRetainSafe(result);
}

static SOSDataSourceRef SOSTestDataSourceFactoryCreateDataSource(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;

    return (SOSDataSourceRef) CFDictionaryGetValue(dsf->data_sources, dataSourceName);
}

static void SOSTestDataSourceFactoryDispose(SOSDataSourceFactoryRef factory)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;
    
    CFReleaseNull(dsf->data_sources);
    free(dsf);
}

SOSDataSourceFactoryRef SOSTestDataSourceFactoryCreate() {
    SOSTestDataSourceFactoryRef dsf = calloc(1, sizeof(struct SOSTestDataSourceFactory));
    
    dsf->dsf.copy_name = SOSTestDataSourceFactoryCopyName;
    dsf->dsf.create_datasource = SOSTestDataSourceFactoryCreateDataSource;
    dsf->dsf.release = SOSTestDataSourceFactoryDispose;
    dsf->data_sources = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    
    return &(dsf->dsf);
}

static bool do_nothing(SOSDataSourceRef ds, CFErrorRef *error) {
    return true;
}

void SOSTestDataSourceFactorySetDataSource(SOSDataSourceFactoryRef factory, CFStringRef name, SOSDataSourceRef ds)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;

    // TODO This hack sucks. It leaks now.
    ds->dsRelease = do_nothing;

    CFDictionaryRemoveAllValues(dsf->data_sources);
    CFDictionarySetValue(dsf->data_sources, name, ds);

}

SOSMergeResult SOSTestDataSourceAddObject(SOSDataSourceRef data_source, SOSObjectRef object, CFErrorRef *error) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    bool result = false;
    CFDataRef key = copyDigest(object, error);
    CFDataRef pk = copyPrimaryKey(object, error);
    if (key && pk) {
        SOSObjectRef myObject = (SOSObjectRef)CFDictionaryGetValue(ds->p2database, pk);
        SOSObjectRef merged = NULL;
        if (myObject) {
            merged = copyMergedObject(object, myObject, error);
        } else {
            merged = object;
            CFRetain(merged);
        }
        if (merged) {
            result = true;
            if (!CFEqualSafe(merged, myObject)) {
                if (myObject) {
                    CFDataRef myKey = copyDigest(myObject, error);
                    CFDictionaryRemoveValue(ds->d2database, myKey);
                    CFReleaseSafe(myKey);
                    const void *values[2] = { myObject, merged };
                    CFTypeRef entry = CFArrayCreate(kCFAllocatorDefault, values, 2, &kCFTypeArrayCallBacks);
                    if (entry) {
                        CFArrayAppendValue(ds->changes, entry);
                        CFRelease(entry);
                    }
                } else {
                    CFArrayAppendValue(ds->changes, merged);
                }
                CFDictionarySetValue(ds->d2database, key, merged);
                CFDictionarySetValue(ds->p2database, pk, merged);
                ds->clean = false;
            }
            CFRelease(merged);
        }
    }
    CFReleaseSafe(pk);
    CFReleaseSafe(key);
    return result;
}

bool SOSTestDataSourceDeleteObject(SOSDataSourceRef data_source, CFDataRef key, CFErrorRef *error) {
    //struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    return false;
}

CFMutableDictionaryRef SOSTestDataSourceGetDatabase(SOSDataSourceRef data_source) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    return ds->d2database;
}

// This works for any datasource, not just the test one, but it's only used in testcases, so it's here for now.
SOSObjectRef SOSDataSourceCreateGenericItemWithData(SOSDataSourceRef ds, CFStringRef account, CFStringRef service, bool is_tomb, CFDataRef data) {
    int32_t value = 0;
    CFNumberRef zero = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    value = 1;
    CFNumberRef one = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFAbsoluteTime timestamp = 3700000 + (is_tomb ? 1 : 0);
    CFDateRef now = CFDateCreate(kCFAllocatorDefault, timestamp);
    CFDictionaryRef dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass,                  kSecClassGenericPassword,
                                                        kSecAttrSynchronizable,     one,
                                                        kSecAttrTombstone,          is_tomb ? one : zero,
                                                        kSecAttrAccount,            account,
                                                        kSecAttrService,            service,
                                                        kSecAttrCreationDate,       now,
                                                        kSecAttrModificationDate,   now,
                                                        kSecAttrAccessGroup,        CFSTR("test"),
                                                        kSecAttrAccessible,         kSecAttrAccessibleWhenUnlocked,
                                                        !is_tomb && data ?  kSecValueData : NULL,data,
                                                        NULL);
    CFRelease(one);
    CFRelease(zero);
    CFReleaseSafe(now);
    CFErrorRef localError = NULL;
    SOSObjectRef object = ds->objectCreateWithPropertyList(dict, &localError);
    if (!object) {
        secerror("createWithPropertyList: %@ failed: %@", dict, localError);
        CFRelease(localError);
    }
    CFRelease(dict);
    return object;
}

SOSObjectRef SOSDataSourceCreateGenericItem(SOSDataSourceRef ds, CFStringRef account, CFStringRef service) {
    return SOSDataSourceCreateGenericItemWithData(ds, account, service, false, NULL);
}

SOSObjectRef SOSDataSourceCreateV0EngineStateWithData(SOSDataSourceRef ds, CFDataRef engineStateData) {
    /*
     MANGO-iPhone:~ mobile$ security item class=genp,acct=engine-state
     acct       : engine-state
     agrp       : com.apple.security.sos
     cdat       : 2016-04-18 20:40:33 +0000
     mdat       : 2016-04-18 20:40:33 +0000
     musr       : //
     pdmn       : dk
     svce       : SOSDataSource-ak
     sync       : 0
     tomb       : 0
     */
    CFAbsoluteTime timestamp = 3700000;
    CFDateRef now = CFDateCreate(kCFAllocatorDefault, timestamp);
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass,                  kSecClassGenericPassword,
                                                        kSecAttrSynchronizable,     kCFBooleanFalse,
                                                        kSecAttrTombstone,          kCFBooleanFalse,
                                                        kSecAttrAccount,            CFSTR("engine-state"),
                                                        kSecAttrService,            CFSTR("SOSDataSource-ak"),
                                                        kSecAttrCreationDate,       now,
                                                        kSecAttrModificationDate,   now,
                                                        kSecAttrAccessGroup,        CFSTR("com.apple.security.sos"),
                                                        kSecAttrAccessible,         kSecAttrAccessibleAlwaysPrivate,
                                                        engineStateData ?  kSecValueData : NULL, engineStateData,
                                                        NULL);
    CFReleaseSafe(now);
    CFErrorRef localError = NULL;
    SOSObjectRef object = ds->objectCreateWithPropertyList(item, &localError);
    if (!object) {
        secerror("createWithPropertyList: %@ failed: %@", item, localError);
        CFRelease(localError);
    }
    CFRelease(item);
    return object;
}

SOSObjectRef SOSDataSourceCopyObject(SOSDataSourceRef ds, SOSObjectRef match, CFErrorRef *error)
{
    __block SOSObjectRef result = NULL;

    CFDataRef digest = SOSObjectCopyDigest(ds, match, error);
    SOSManifestRef manifest = NULL;

    require(digest, exit);
    manifest = SOSManifestCreateWithData(digest, error);

    SOSDataSourceForEachObject(ds, NULL, manifest, error, ^void (CFDataRef key, SOSObjectRef object, bool *stop) {
        if (object == NULL) {
            if (error && !*error) {
                SecCFCreateErrorWithFormat(kSOSDataSourceObjectNotFoundError, sSOSDataSourceErrorDomain, NULL, error, 0, CFSTR("key %@ not in database"), key);
            }
        } else if (result == NULL) {
            result = CFRetainSafe(object);
        }
    });

exit:
    CFReleaseNull(manifest);
    CFReleaseNull(digest);
    return result;
}

//
//  SOSTestDataSource.c
//  sec
//
//  Created by Michael Brouwer on 9/28/12.
//
//

#include "SOSTestDataSource.h"

#include <corecrypto/ccder.h>
#include <SecureObjectSync/SOSEngine.h>
#include <utilities/array_size.h>
#include <utilities/der_plist.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecItemPriv.h>

static CFStringRef sErrorDomain = CFSTR("com.apple.testdatasource");

enum {
    kSOSObjectMallocFailed = 1,
    kAddDuplicateEntry,
    kSOSObjectNotFouncError = 1,
};

typedef struct SOSTestDataSource *SOSTestDataSourceRef;

struct SOSTestDataSource {
    struct SOSDataSource ds;
    unsigned gm_count;
    unsigned cm_count;
    unsigned co_count;
    CFMutableDictionaryRef database;
    uint8_t manifest_digest[SOSDigestSize];
    bool clean;
};

typedef struct SOSTestDataSourceFactory *SOSTestDataSourceFactoryRef;

struct SOSTestDataSourceFactory {
    struct SOSDataSourceFactory dsf;
    CFMutableDictionaryRef data_sources;
};


/* DataSource protocol. */
static bool get_manifest_digest(SOSDataSourceRef data_source, uint8_t *out_digest, CFErrorRef *error) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    if (!ds->clean) {
        SOSManifestRef mf = data_source->copy_manifest(data_source, error);
        if (mf) {
            CFRelease(mf);
        } else {
            return false;
        }
    }
    memcpy(out_digest, ds->manifest_digest, SOSDigestSize);
    ds->gm_count++;
    return true;
}

static SOSManifestRef copy_manifest(SOSDataSourceRef data_source, CFErrorRef *error) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    ds->cm_count++;
    __block struct SOSDigestVector dv = SOSDigestVectorInit;
    CFDictionaryForEach(ds->database, ^(const void *key, const void *value) {
        SOSDigestVectorAppend(&dv, CFDataGetBytePtr((CFDataRef)key));
    });
    SOSDigestVectorSort(&dv);
    SOSManifestRef manifest = SOSManifestCreateWithBytes((const uint8_t *)dv.digest, dv.count * SOSDigestSize, error);
    SOSDigestVectorFree(&dv);
    ccdigest(ccsha1_di(), SOSManifestGetSize(manifest), SOSManifestGetBytePtr(manifest), ds->manifest_digest);
    ds->clean = true;

    return manifest;
}

static bool foreach_object(SOSDataSourceRef data_source, SOSManifestRef manifest, CFErrorRef *error, bool (^handle_object)(SOSObjectRef object, CFErrorRef *error)) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    ds->co_count++;
    __block bool result = true;
    SOSManifestForEach(manifest, ^(CFDataRef key) {
        CFDictionaryRef dict = (CFDictionaryRef)CFDictionaryGetValue(ds->database, key);
        if (dict) {
            result = result && handle_object((SOSObjectRef)dict, error);
        } else {
            result = false;
            if (error) {
                // TODO: Collect all missing keys in an array and return an single error at the end with all collected keys
                // Collect all errors as chained errors.
                CFErrorRef old_error = *error;
                *error = NULL;
                SecCFCreateErrorWithFormat(kSOSObjectNotFouncError, sErrorDomain, old_error, error, 0, CFSTR("key %@ not in database"), key);
            }
        }
    });
    return result;
}

static void dispose(SOSDataSourceRef data_source) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    free(ds);
}

static SOSObjectRef createWithPropertyList(SOSDataSourceRef ds, CFDictionaryRef plist, CFErrorRef *error) {
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
    } else if (error && *error == NULL) {
        *error = CFErrorCreate(0, sErrorDomain, kSOSObjectMallocFailed, NULL);
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
        *error = CFErrorCreate(0, sErrorDomain, kSOSObjectMallocFailed, NULL);
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

SOSDataSourceRef SOSTestDataSourceCreate(void) {
    SOSTestDataSourceRef ds = calloc(1, sizeof(struct SOSTestDataSource));

    ds->ds.get_manifest_digest = get_manifest_digest;
    ds->ds.copy_manifest = copy_manifest;
    ds->ds.foreach_object = foreach_object;
    ds->ds.release = dispose;
    ds->ds.add = SOSTestDataSourceAddObject;

    ds->ds.createWithPropertyList = createWithPropertyList;
    ds->ds.copyDigest = copyDigest;
    ds->ds.copyPrimaryKey = copyPrimaryKey;
    ds->ds.copyPropertyList = copyPropertyList;
    ds->ds.copyMergedObject = copyMergedObject;

    ds->database = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ds->clean = false;

    return (SOSDataSourceRef)ds;
}

static CFArrayRef SOSTestDataSourceFactoryCopyNames(SOSDataSourceFactoryRef factory)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(dsf->data_sources, ^(const void*key, const void*value) { CFArrayAppendValue(result, key); });
    
    return result;
}

static SOSDataSourceRef SOSTestDataSourceFactoryCreateDataSource(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, bool readOnly __unused, CFErrorRef *error)
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
    
    dsf->dsf.copy_names = SOSTestDataSourceFactoryCopyNames;
    dsf->dsf.create_datasource = SOSTestDataSourceFactoryCreateDataSource;
    dsf->dsf.release = SOSTestDataSourceFactoryDispose;
    dsf->data_sources = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    
    return &(dsf->dsf);
}

static void do_nothing(SOSDataSourceRef ds)
{
}

void SOSTestDataSourceFactoryAddDataSource(SOSDataSourceFactoryRef factory, CFStringRef name, SOSDataSourceRef ds)
{
    SOSTestDataSourceFactoryRef dsf = (SOSTestDataSourceFactoryRef) factory;

    // TODO This hack sucks. It leaks now.
    ds->release = do_nothing;

    CFDictionarySetValue(dsf->data_sources, name, ds);

}

SOSMergeResult SOSTestDataSourceAddObject(SOSDataSourceRef data_source, SOSObjectRef object, CFErrorRef *error) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    bool result = false;
    CFDataRef key = copyDigest(object, error);
    if (key) {
        SOSObjectRef myObject = (SOSObjectRef)CFDictionaryGetValue(ds->database, key);
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
                CFDictionarySetValue(ds->database, key, merged);
                ds->clean = false;
            }
            CFRelease(merged);
        }
        CFRelease(key);
    }
    return result;
}

bool SOSTestDataSourceDeleteObject(SOSDataSourceRef data_source, CFDataRef key, CFErrorRef *error) {
    //struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    return false;
}

CFMutableDictionaryRef SOSTestDataSourceGetDatabase(SOSDataSourceRef data_source) {
    struct SOSTestDataSource *ds = (struct SOSTestDataSource *)data_source;
    return ds->database;
}

// This works for any datasource, not just the test one, but it's only used in testcases, so it's here for now.
SOSObjectRef SOSDataSourceCreateGenericItemWithData(SOSDataSourceRef ds, CFStringRef account, CFStringRef service, bool is_tomb, CFDataRef data) {
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    abort();
#else
    int32_t value = 0;
    CFNumberRef zero = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    value = 1;
    CFNumberRef one = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFAbsoluteTime timestamp = 3700000;
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
    SOSObjectRef object = ds->createWithPropertyList(ds, dict, &localError);
    if (!object) {
        secerror("createWithPropertyList: %@ failed: %@", dict, localError);
        CFRelease(localError);
    }
    CFRelease(dict);
    return object;
#endif
}

SOSObjectRef SOSDataSourceCreateGenericItem(SOSDataSourceRef ds, CFStringRef account, CFStringRef service) {
    return SOSDataSourceCreateGenericItemWithData(ds, account, service, false, NULL);
}

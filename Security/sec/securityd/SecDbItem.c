/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

/*
 * SecDbItem.c - CoreFoundation-based constants and functions representing
 * database items (certificates, keys, identities, and passwords.)
 * Created by Michael Brouwer on 11/15/12.
 */

#include <securityd/SecDbItem.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_date.h>
#include <utilities/debugging.h>

#include <Security/SecBasePriv.h>
#include <Security/SecInternal.h>
#include <corecrypto/ccsha1.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

// MARK: type converters

CFStringRef copyString(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFStringGetTypeID())
        return CFStringCreateCopy(0, obj);
    else if (tid == CFDataGetTypeID())
        return CFStringCreateFromExternalRepresentation(0, obj, kCFStringEncodingUTF8);
    else
        return NULL;
}

CFDataRef copyData(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDataGetTypeID()) {
        return CFDataCreateCopy(0, obj);
    } else if (tid == CFStringGetTypeID()) {
        return CFStringCreateExternalRepresentation(0, obj, kCFStringEncodingUTF8, 0);
    } else if (tid == CFNumberGetTypeID()) {
        SInt32 value;
        CFNumberGetValue(obj, kCFNumberSInt32Type, &value);
        return CFDataCreate(0, (const UInt8 *)&value, sizeof(value));
    } else {
        return NULL;
    }
}

CFTypeRef copyBlob(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDataGetTypeID()) {
        return CFDataCreateCopy(0, obj);
    } else if (tid == CFStringGetTypeID()) {
        return CFStringCreateCopy(0, obj);
    } else if (tid == CFNumberGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else {
        return NULL;
    }
}

CFDataRef copySHA1(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDataGetTypeID() && CFDataGetLength(obj) == CCSHA1_OUTPUT_SIZE) {
        return CFDataCreateCopy(CFGetAllocator(obj), obj);
    } else {
        return NULL;
    }
}

CFTypeRef copyNumber(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFNumberGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else if (tid == CFBooleanGetTypeID()) {
        SInt32 value = CFBooleanGetValue(obj);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    } else if (tid == CFStringGetTypeID()) {
        SInt32 value = CFStringGetIntValue(obj);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long) value);
        /* If a string converted to an int isn't equal to the int printed as
           a string, return a CFStringRef instead. */
        if (!CFEqual(t, obj)) {
            CFRelease(t);
            return CFStringCreateCopy(0, obj);
        }
        CFRelease(t);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    } else
        return NULL;
}

CFDateRef copyDate(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDateGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else
        return NULL;
}

// MARK: SecDbColumn accessors, to retrieve values as CF types in SecDbStep.

static CFDataRef SecDbColumnCopyData(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    return CFDataCreate(allocator, sqlite3_column_blob(stmt, col),
                        sqlite3_column_bytes(stmt, col));
    //return CFDataCreateWithBytesNoCopy(0, sqlite3_column_blob(stmt, col),
    //                                   sqlite3_column_bytes(stmt, col),
    //                                   kCFAllocatorNull);
}

static CFDateRef SecDbColumnCopyDate(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    return CFDateCreate(allocator, sqlite3_column_double(stmt, col));
}

static CFNumberRef SecDbColumnCopyDouble(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    double number = sqlite3_column_double(stmt, col);
    return CFNumberCreate(allocator, kCFNumberDoubleType, &number);
}

static CFNumberRef SecDbColumnCopyNumber64(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    sqlite_int64 number = sqlite3_column_int64(stmt, col);
    return CFNumberCreate(allocator, kCFNumberSInt64Type, &number);
}

static CFNumberRef SecDbColumnCopyNumber(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    sqlite_int64 number = sqlite3_column_int64(stmt, col);
    if (INT32_MIN <= number && number <= INT32_MAX) {
        int32_t num32 = (int32_t)number;
        return CFNumberCreate(allocator, kCFNumberSInt32Type, &num32);
    } else {
        return CFNumberCreate(allocator, kCFNumberSInt64Type, &number);
    }
}

static CFStringRef SecDbColumnCopyString(CFAllocatorRef allocator, sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    const unsigned char *text = sqlite3_column_text(stmt, col);
    return CFStringCreateWithBytes(allocator, text, strlen((const char *)text), kCFStringEncodingUTF8, false);
}

// MARK: SecDbClass helpers

const SecDbAttr *SecDbClassAttrWithKind(const SecDbClass *class, SecDbAttrKind kind, CFErrorRef *error) {
    const SecDbAttr *result = NULL;
    SecDbForEachAttr(class, desc) {
        if (desc->kind == kind)
            result = desc;
    }

    if (!result)
        SecError(errSecInternal, error, CFSTR("Can't find attribute of kind %d in class %@"), kind, class->name);

    return result;
}

// MARK: SecDbAttr helpers

static bool SecDbIsTombstoneDbSelectAttr(const SecDbAttr *attr) {
    return attr->flags & kSecDbPrimaryKeyFlag || attr->kind == kSecDbTombAttr;
}

#if 0
static bool SecDbIsTombstoneDbInsertAttr(const SecDbAttr *attr) {
    return SecDbIsTombstoneDbSelectAttr(attr) || attr->kind == kSecDbAccessAttr || attr->kind == kSecDbCreationDateAttr || attr->kind == kSecDbModificationDateAttr;
}
#endif

static bool SecDbIsTombstoneDbUpdateAttr(const SecDbAttr *attr) {
    return SecDbIsTombstoneDbSelectAttr(attr) || attr->kind == kSecDbAccessAttr || attr->kind == kSecDbCreationDateAttr || attr->kind == kSecDbRowIdAttr;
}

static bool SecDbAttrBind(const SecDbAttr *attr, sqlite3_stmt *stmt, int col, CFTypeRef value, CFErrorRef *error) {
    bool ok = true;
    if (value && !CFEqual(kCFNull, value) && attr->flags & kSecDbSHA1ValueInFlag) {
        CFDataRef data = copyData(value);
        if (!data) {
            SecError(errSecInternal, error, CFSTR("failed to get attribute %@ data"), attr->name);
            return false;
        }

        CFMutableDataRef digest = CFDataCreateMutable(kCFAllocatorDefault, CCSHA1_OUTPUT_SIZE);
        CFDataSetLength(digest, CCSHA1_OUTPUT_SIZE);
        /* 64 bits cast: worst case is we generate the wrong hash */
        assert((unsigned long)CFDataGetLength(data)<UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
        ccdigest(ccsha1_di(), CFDataGetLength(data), CFDataGetBytePtr(data), CFDataGetMutableBytePtr(digest));
        CFRelease(data);
        ok &= SecDbBindObject(stmt, col, digest, error);
        CFRelease(digest);
    } else {
        ok &= SecDbBindObject(stmt, col, value, error);
    }
    return ok;
}

// MARK: SecDbItem

CFTypeRef SecDbItemGetCachedValueWithName(SecDbItemRef item, CFStringRef name) {
    return CFDictionaryGetValue(item->attributes, name);
}

static CFTypeRef SecDbItemGetCachedValue(SecDbItemRef item, const SecDbAttr *desc) {
    return CFDictionaryGetValue(item->attributes, desc->name);
}

CFMutableDictionaryRef SecDbItemCopyPListWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SecDbForEachAttrWithMask(item->class, desc, mask) {
        CFTypeRef value = SecDbItemGetValue(item, desc, error);
        if (value) {
            if (!CFEqual(kCFNull, value)) {
                CFDictionarySetValue(dict, desc->name, value);
            } else if (desc->flags & kSecDbNotNullFlag) {
                SecError(errSecInternal, error, CFSTR("attribute %@ has NULL value"), desc->name);
                secerror("%@", error ? *error : (CFErrorRef)CFSTR("error == NULL"));
                CFReleaseNull(dict);
                break;
            }
        } else {
            CFReleaseNull(dict);
            break;
        }
    }
    return dict;
}

static CFDataRef SecDbItemCopyDERWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error) {
    CFDataRef der = NULL;
    CFMutableDictionaryRef dict = SecDbItemCopyPListWithMask(item, mask, error);
    if (dict) {
        der = kc_plist_copy_der(dict, error);
        CFRelease(dict);
    }
    return der;
}

static CFDataRef SecDbItemCopyDigestWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error) {
    CFDataRef digest = NULL;
    CFDataRef der = SecDbItemCopyDERWithMask(item, mask, error);
    if (der) {
        digest = kc_copy_sha1(CFDataGetLength(der), CFDataGetBytePtr(der), error);
        CFRelease(der);
    }
    return digest;
}

static CFDataRef SecDbItemCopyPrimaryKey(SecDbItemRef item, CFErrorRef *error) {
    return SecDbItemCopyDigestWithMask(item, kSecDbPrimaryKeyFlag, error);
}

static CFDataRef SecDbItemCopySHA1(SecDbItemRef item, CFErrorRef *error) {
    return SecDbItemCopyDigestWithMask(item, kSecDbInHashFlag, error);
}

static CFDataRef SecDbItemCopyUnencryptedData(SecDbItemRef item, CFErrorRef *error) {
    return SecDbItemCopyDERWithMask(item, kSecDbInCryptoDataFlag, error);
}

static keyclass_t SecDbItemParseKeyclass(CFTypeRef value, CFErrorRef *error) {
    if (!isString(value)) {
        SecError(errSecParam, error, CFSTR("accessible attribute %@ not a string"), value);
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlocked)) {
        return key_class_ak;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlock)) {
        return key_class_ck;
    } else if (CFEqual(value, kSecAttrAccessibleAlways)) {
        return key_class_dk;
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)) {
        return key_class_aku;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)) {
        return key_class_cku;
    } else if (CFEqual(value, kSecAttrAccessibleAlwaysThisDeviceOnly)) {
        return key_class_dku;
    } else {
        SecError(errSecParam, error, CFSTR("accessible attribute %@ unknown"), value);
    }
    return 0;
}

keyclass_t SecDbItemGetKeyclass(SecDbItemRef item, CFErrorRef *error) {
    if (!item->keyclass) {
        const SecDbAttr *desc = SecDbClassAttrWithKind(item->class, kSecDbAccessAttr, error);
        if (desc) {
            CFTypeRef value = SecDbItemGetValue(item, desc, error);
            if (value) {
                item->keyclass = SecDbItemParseKeyclass(value, error);
            }
        }
    }
    return item->keyclass;
}

static CFDataRef SecDbItemCopyEncryptedData(SecDbItemRef item, CFErrorRef *error) {
    CFDataRef edata = NULL;
    CFDataRef plain = SecDbItemCopyUnencryptedData(item, error);
    if (plain) {
        keyclass_t keyclass = SecDbItemGetKeyclass(item, error);
        if (keyclass) {
            if (ks_encrypt_data(item->keybag, keyclass, plain, &edata, error)) {
                item->_edataState = kSecDbItemEncrypting;
            } else {
                seccritical("ks_encrypt_data (db): failed: %@", error ? *error : (CFErrorRef)CFSTR(""));
            }
        }
        CFRelease(plain);
    }

    return edata;
}

CFDataRef SecDbItemCopyEncryptedDataToBackup(SecDbItemRef item, uint64_t handle, CFErrorRef *error) {
    CFDataRef edata = NULL;
    keybag_handle_t keybag = (keybag_handle_t)handle;
    CFDataRef plain = SecDbItemCopyUnencryptedData(item, error);
    if (plain) {
        keyclass_t keyclass = SecDbItemGetKeyclass(item, error);
        if (keyclass) {
            if (!ks_encrypt_data(keybag, keyclass, plain, &edata, error))
                seccritical("ks_encrypt_data (db): failed: %@", error ? *error : (CFErrorRef)CFSTR(""));
        }
        CFRelease(plain);
    }
    return edata;
}

static bool SecDbItemEnsureDecrypted(SecDbItemRef item, CFErrorRef *error) {

    // If we haven't yet decrypted the item, make sure we do so now
    bool result = true;
    if (item->_edataState == kSecDbItemEncrypted) {
        const SecDbAttr *attr = SecDbClassAttrWithKind(item->class, kSecDbEncryptedDataAttr, error);
        if (attr) {
            CFDataRef edata = SecDbItemGetCachedValue(item, attr);
            if (!edata)
                return SecError(errSecInternal, error, CFSTR("state= encrypted but edata is NULL"));
            // Decrypt calls set value a bunch of times which clears our edata and changes our state.
            item->_edataState = kSecDbItemDecrypting;
            result = SecDbItemDecrypt(item, edata, error);
            if (result)
                item->_edataState = kSecDbItemClean;
            else
                item->_edataState = kSecDbItemEncrypted;
        }
    }
    return result;
}

// Only called if cached value is not found.
static CFTypeRef SecDbItemCopyValue(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error) {
    CFTypeRef value = NULL;
    switch (attr->kind) {
        case kSecDbSHA1Attr:
            value = SecDbItemCopySHA1(item, error);
            break;
        case kSecDbEncryptedDataAttr:
            value = SecDbItemCopyEncryptedData(item, error);
            break;
        case kSecDbPrimaryKeyAttr:
            value = SecDbItemCopyPrimaryKey(item, error);
            break;
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
        case kSecDbBlobAttr:
            if (attr->flags & kSecDbNotNullFlag) {
                if (attr->flags & kSecDbDefault0Flag) {
                    value = CFSTR("0");
                    break;
                } else if (attr->kind != kSecDbBlobAttr && attr->flags & kSecDbDefaultEmptyFlag) {
                    // blob drops through to data everything else is empty string
                    value = CFSTR("");
                    break;
                }
            }
            //DROPTHROUGH
        case kSecDbDataAttr:
            if (attr->flags & kSecDbNotNullFlag && attr->flags & kSecDbDefaultEmptyFlag) {
                value = CFDataCreate(CFGetAllocator(item), NULL, 0);
            } else {
                value = kCFNull;
            }
            break;
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
            if (attr->flags & kSecDbNotNullFlag) {
                int32_t zero = 0;
                value = CFNumberCreate(CFGetAllocator(item), kCFNumberSInt32Type, &zero);
            } else {
                value = kCFNull;
            }
            break;
        case kSecDbDateAttr:
            if (attr->flags & kSecDbNotNullFlag && attr->flags & kSecDbDefault0Flag) {
                value = CFDateCreate(kCFAllocatorDefault, 0.0);
            } else {
                value = kCFNull;
            }
            break;
        case kSecDbRowIdAttr:
            if (attr->flags & kSecDbNotNullFlag) {
                // No can do, error?
            }
            value = kCFNull;
            break;
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            value = CFDateCreate(CFGetAllocator(item), CFAbsoluteTimeGetCurrent());
            break;
    }

    return value;
}

// SecDbItemGetValue will return kCFNull if there is no value for an attribute and this was not
// an error.  It will return NULL and optionally set *error if there was an error computing an
// attribute, or if a required attribute was missing a value and had no known way to compute
// it's value.
CF_RETURNS_NOT_RETAINED CFTypeRef SecDbItemGetValue(SecDbItemRef item, const SecDbAttr *desc, CFErrorRef *error) {
    // Propagate chained errors
    if (!desc)
        return NULL;

    if (desc->flags & kSecDbInCryptoDataFlag) {
        if (!SecDbItemEnsureDecrypted(item, error))
            return NULL;
    }

    CFTypeRef value = SecDbItemGetCachedValue(item, desc);
    if (!value) {
        value = SecDbItemCopyValue(item, desc, error);
        if (value) {
            if (!CFEqual(kCFNull, value)) {
                SecDbItemSetValue(item, desc, value, error);
                CFRelease(value);
                value = SecDbItemGetCachedValue(item, desc);
            }
        }
    }
    return value;
}

static bool SecDbItemGetBoolValue(SecDbItemRef item, const SecDbAttr *desc, bool *bvalue, CFErrorRef *error) {
    CFTypeRef value = SecDbItemGetValue(item, desc, error);
    if (!value)
        return false;
    char cvalue;
    *bvalue = (isNumber(value) && CFNumberGetValue(value, kCFNumberCharType, &cvalue) && cvalue == 1);
    return true;
}

static CFStringRef SecDbItemCopyDescription(CFTypeRef cf) {
#if 0 //defined(DEBUG) && DEBUG != 0
    SecDbItemRef item = (SecDbItemRef)cf;
    CFMutableStringRef desc = CFStringCreateMutable(CFGetAllocator(cf), 0);
    CFStringAppendFormat(desc, NULL, CFSTR("<%@"), item->class->name);
    SecDbForEachAttr(item->class, attr) {
            CFTypeRef value = SecDbItemGetValue(item, attr, NULL);
            if (value) {
                CFStringAppend(desc, CFSTR(","));
                CFStringAppend(desc, attr->name);
                CFStringAppend(desc, CFSTR("="));
                if (CFEqual(CFSTR("data"), attr->name)) {
                    CFStringAppendEncryptedData(desc, value);
                } else if (CFEqual(CFSTR("v_Data"), attr->name)) {
                    CFStringAppend(desc, CFSTR("<?>"));
                } else if (isData(value)) {
                    CFStringAppendHexData(desc, value);
                } else {
                    CFStringAppendFormat(desc, 0, CFSTR("%@"), value);
                }
            }
    }
    CFStringAppend(desc, CFSTR(">"));
#else
    SecDbItemRef item = (SecDbItemRef)cf;
    const UInt8 zero4[4] = {};
    const UInt8 *pk = &zero4[0], *sha1 = &zero4[0];
    char sync = 0;
    char tomb = 0;
    SInt64 rowid = 0;
    CFStringRef access = NULL;
    uint8_t mdatbuf[32] = {};
    uint8_t *mdat = &mdatbuf[0];
    CFMutableStringRef attrs = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef agrp = NULL;

    SecDbForEachAttr(item->class, attr) {
        CFTypeRef value;
        switch (attr->kind) {
            case kSecDbBlobAttr:
            case kSecDbDataAttr:
            case kSecDbStringAttr:
            case kSecDbNumberAttr:
            case kSecDbDateAttr:
            case kSecDbEncryptedDataAttr:
                if (attr->flags & (kSecDbReturnAttrFlag | kSecDbReturnDataFlag) && (value = SecDbItemGetValue(item, attr, NULL)) && !CFEqual(value, kCFNull)) {
                    if (isString(value) && CFEqual(attr->name, kSecAttrAccessGroup)) {
                        agrp = value;
                    } else {
                        // We don't log these, just record that we saw the attribute.
                        CFStringAppend(attrs, CFSTR(","));
                        CFStringAppend(attrs, attr->name);
                    }
                }
                break;
            case kSecDbCreationDateAttr:
                // We don't care about this and every object has one.
                break;
            case kSecDbModificationDateAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isDate(value))
                    mdat = der_encode_generalizedtime_body(CFDateGetAbsoluteTime(value), NULL, mdat, &mdatbuf[31]);
                break;
            case kSecDbSHA1Attr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isData(value))
                    sha1 = CFDataGetBytePtr(value);
                break;
            case kSecDbRowIdAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isNumber(value))
                    CFNumberGetValue(value, kCFNumberSInt64Type, &rowid);
                break;
            case kSecDbPrimaryKeyAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isData(value))
                    pk = CFDataGetBytePtr(value);
                break;
            case kSecDbSyncAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isNumber(value))
                    CFNumberGetValue(value, kCFNumberCharType, &sync);
                break;
            case kSecDbTombAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isNumber(value))
                    CFNumberGetValue(value, kCFNumberCharType, &tomb);
                break;
            case kSecDbAccessAttr:
                value = SecDbItemGetValue(item, attr, NULL);
                if (isString(value))
                    access = value;
                break;
        }
    }

    CFStringRef desc = CFStringCreateWithFormat(CFGetAllocator(cf), NULL,
        CFSTR(
              "%s,"
              "%@,"
              "%02X%02X%02X%02X,"
              "%s,"
              "%@,"
              "%@,"
              "%"PRId64
              "%@,"
              "%s,"
              "%02X%02X%02X%02X"),
        tomb ? "T" : "O",
        item->class->name,
        pk[0], pk[1], pk[2], pk[3],
        sync ? "S" : "L",
        access,
        agrp,
        rowid,
        attrs,
        mdat,
        sha1[0], sha1[1], sha1[2], sha1[3]);
    CFReleaseSafe(attrs);
#endif

    return desc;
}

static void SecDbItemDestroy(CFTypeRef cf) {
    SecDbItemRef item = (SecDbItemRef)cf;
    CFReleaseSafe(item->attributes);
}

static CFHashCode SecDbItemHash(CFTypeRef cf) {
    SecDbItemRef item = (SecDbItemRef)cf;
    CFDataRef digest = SecDbItemGetSHA1(item, NULL);
    CFHashCode code;
    const UInt8 *p = CFDataGetBytePtr(digest);
    // Read first 8 bytes of digest in order
    code = p[0] + ((p[1] + ((p[2] + ((p[3] + ((p[4] + ((p[5] + ((p[6] + (p[7] << 8)) << 8)) << 8)) << 8)) << 8)) << 8)) << 8);
    return code;
}

static Boolean SecDbItemCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SecDbItemRef item1 = (SecDbItemRef)cf1;
    SecDbItemRef item2 = (SecDbItemRef)cf2;
    CFDataRef digest1 = NULL;
    CFDataRef digest2 = NULL;
    if (item1)
        digest1 = SecDbItemGetSHA1(item1, NULL);
    if (item2)
        digest2 = SecDbItemGetSHA1(item2, NULL);
    Boolean equal = CFEqual(digest1, digest2);
    return equal;
}

CFGiblisWithHashFor(SecDbItem)

static SecDbItemRef SecDbItemCreate(CFAllocatorRef allocator, const SecDbClass *class, keybag_handle_t keybag) {
    SecDbItemRef item = CFTypeAllocate(SecDbItem, struct SecDbItem, allocator);
    item->class = class;
    item->attributes = CFDictionaryCreateMutableForCFTypes(allocator);
    item->keybag = keybag;
    item->_edataState = kSecDbItemDirty;
    return item;
}

const SecDbClass *SecDbItemGetClass(SecDbItemRef item) {
    return item->class;
}

const keybag_handle_t SecDbItemGetKeybag(SecDbItemRef item) {
    return item->keybag;
}

bool SecDbItemSetKeybag(SecDbItemRef item, keybag_handle_t keybag, CFErrorRef *error) {
    if (!SecDbItemEnsureDecrypted(item, error))
        return false;
    if (item->keybag != keybag) {
        item->keybag = keybag;
        if (item->_edataState == kSecDbItemClean) {
            SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbEncryptedDataAttr, NULL), kCFNull, NULL);
        }
    }

    return true;
}

bool SecDbItemSetValue(SecDbItemRef item, const SecDbAttr *desc, CFTypeRef value, CFErrorRef *error) {
    // Propagate chained errors.
    if (!desc)
        return false;

    bool changed = false;
    CFTypeRef attr = NULL;
    if (desc->flags & kSecDbInCryptoDataFlag)
        if (!SecDbItemEnsureDecrypted(item, error))
            return false;

    switch (desc->kind) {
        case kSecDbPrimaryKeyAttr:
        case kSecDbDataAttr:
            attr = copyData(value);
            break;
        case kSecDbEncryptedDataAttr:
            attr = copyData(value);
            if (attr) {
                if (item->_edataState == kSecDbItemEncrypting)
                    item->_edataState = kSecDbItemClean;
                else
                    item->_edataState = kSecDbItemEncrypted;
            } else if (!value || CFEqual(kCFNull, value)) {
                item->_edataState = kSecDbItemDirty;
            }
            break;
        case kSecDbBlobAttr:
            attr = copyBlob(value);
            break;
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            attr = copyDate(value);
            break;
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
        case kSecDbRowIdAttr:
            attr = copyNumber(value);
            break;
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            attr = copyString(value);
            break;
        case kSecDbSHA1Attr:
            attr = copySHA1(value);
            break;
    }

    if (attr) {
        CFTypeRef ovalue = CFDictionaryGetValue(item->attributes, desc->name);
        changed = (!ovalue || !CFEqual(ovalue, attr));
        CFDictionarySetValue(item->attributes, desc->name, attr);
        CFRelease(attr);
    } else {
        if (value && !CFEqual(kCFNull, value)) {
            SecError(errSecItemInvalidValue, error, CFSTR("attribute %@: value: %@ failed to convert"), desc->name, value);
            return false;
        }
        CFTypeRef ovalue = CFDictionaryGetValue(item->attributes, desc->name);
        changed = (ovalue && !CFEqual(ovalue, kCFNull));
        CFDictionaryRemoveValue(item->attributes, desc->name);
    }

    if (changed) {
        if (desc->flags & kSecDbInHashFlag)
            SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, NULL), kCFNull, NULL);
        if (desc->flags & kSecDbPrimaryKeyFlag)
            SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbPrimaryKeyAttr, NULL), kCFNull, NULL);
        if (desc->flags & kSecDbInCryptoDataFlag && item->_edataState == kSecDbItemClean)
            SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbEncryptedDataAttr, NULL), kCFNull, NULL);
    }

    return true;
}

bool SecDbItemSetValues(SecDbItemRef item, CFDictionaryRef values, CFErrorRef *error) {
    SecDbForEachAttr(item->class, attr) {
        CFTypeRef value = CFDictionaryGetValue(values, attr->name);
        if (value && !SecDbItemSetValue(item, attr, value, error))
            return false;
    }
    return true;
}

bool SecDbItemSetValueWithName(SecDbItemRef item, CFStringRef name, CFTypeRef value, CFErrorRef *error) {
    SecDbForEachAttr(item->class, attr) {
        if (CFEqual(attr->name, name)) {
            return SecDbItemSetValue(item, attr, value, error);
        }
    }
    return false;
}

static bool SecDbItemSetNumber(SecDbItemRef item, const SecDbAttr *desc, int32_t number, CFErrorRef *error) {
    bool ok = true;
    CFNumberRef value = CFNumberCreate(CFGetAllocator(item), kCFNumberSInt32Type, &number);
    if (value) {
        ok = SecDbItemSetValue(item, desc, value, error);
        CFRelease(value);
    } else {
        ok = SecError(errSecInternal, error, CFSTR("Failed to create CFNumber for %" PRId32), number);
    }
    return ok;
}

bool SecDbItemSetKeyclass(SecDbItemRef item, keyclass_t keyclass, CFErrorRef *error) {
    bool ok = SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbEncryptedDataAttr, error), kCFNull, error);
    if (ok) {
        item->_edataState = kSecDbItemDirty;
        ok = SecDbItemSetNumber(item, SecDbClassAttrWithKind(item->class, kSecDbAccessAttr, error), keyclass, error);
    }
    return ok;
}

SecDbItemRef SecDbItemCreateWithAttributes(CFAllocatorRef allocator, const SecDbClass *class, CFDictionaryRef attributes, keybag_handle_t keybag, CFErrorRef *error) {
    SecDbItemRef item = SecDbItemCreate(kCFAllocatorDefault, class, keybag);
    if (item && !SecDbItemSetValues(item, attributes, error))
        CFReleaseNull(item);
    return item;
}

static CFTypeRef
SecDbColumnCopyValueWithAttr(CFAllocatorRef allocator, sqlite3_stmt *stmt, const SecDbAttr *attr, int col, CFErrorRef *error) {
    CFTypeRef value = NULL;
    switch (attr->kind) {
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            value = SecDbColumnCopyDate(allocator, stmt, col, error);
            break;
        case kSecDbBlobAttr:
            switch (sqlite3_column_type(stmt, col)) {
                case SQLITE_INTEGER:
                    value = SecDbColumnCopyNumber(allocator, stmt, col, error);
                    break;
                case SQLITE_FLOAT:
                    value = SecDbColumnCopyDouble(allocator, stmt, col, error);
                    break;
                case SQLITE_TEXT:
                    value = SecDbColumnCopyString(allocator, stmt, col, error);
                    break;
                case SQLITE_BLOB:
                    value = SecDbColumnCopyData(allocator, stmt, col, error);
                    break;
                case SQLITE_NULL:
                    value = kCFNull;
                    break;
            }
            break;
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            value = SecDbColumnCopyString(allocator, stmt, col, error);
            break;
        case kSecDbDataAttr:
        case kSecDbSHA1Attr:
        case kSecDbPrimaryKeyAttr:
            value = SecDbColumnCopyData(allocator, stmt, col, error);
            break;
        case kSecDbEncryptedDataAttr:
            value = SecDbColumnCopyData(allocator, stmt, col, error);
            break;
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
        case kSecDbNumberAttr:
            value = SecDbColumnCopyNumber(allocator, stmt, col, error);
            break;
        case kSecDbRowIdAttr:
            value = SecDbColumnCopyNumber64(allocator, stmt, col, error);
            break;
    }
    return value;
}

SecDbItemRef SecDbItemCreateWithStatement(CFAllocatorRef allocator, const SecDbClass *class, sqlite3_stmt *stmt, keybag_handle_t keybag, CFErrorRef *error, bool (^return_attr)(const SecDbAttr *attr)) {
    SecDbItemRef item = SecDbItemCreate(allocator, class, keybag);
    int col = 0;
    SecDbForEachAttr(class, attr) {
        if (return_attr(attr)) {
            CFTypeRef value = SecDbColumnCopyValueWithAttr(allocator, stmt, attr, col++, error);
            if (value) {
                SecDbItemSetValue(item, attr, value, error);
                CFRelease(value);
            }
        }
    }

    return item;
}

SecDbItemRef SecDbItemCreateWithEncryptedData(CFAllocatorRef allocator, const SecDbClass *class,
                                              CFDataRef edata, keybag_handle_t keybag, CFErrorRef *error) {
    SecDbItemRef item = SecDbItemCreate(allocator, class, keybag);
    const SecDbAttr *edata_attr = SecDbClassAttrWithKind(class, kSecDbEncryptedDataAttr, error);
    if (edata_attr) {
        if (!SecDbItemSetValue(item, edata_attr, edata, error))
            CFReleaseNull(item);
    }
    return item;
}

#if 0
SecDbItemRef SecDbItemCreateWithRowId(CFAllocatorRef allocator, const SecDbClass *class, sqlite_int64 row_id, keybag_handle_t keybag, CFErrorRef *error) {
    SecDbItemRef item = SecDbItemCreate(allocator, class, keybag);
    if (!SecDbItemSetRowId(item, row_id, error))
        CFReleaseNull(item);
    return item;
}
#endif

SecDbItemRef SecDbItemCopyWithUpdates(SecDbItemRef item, CFDictionaryRef updates, CFErrorRef *error) {
    SecDbItemRef new_item = SecDbItemCreate(CFGetAllocator(item), item->class, item->keybag);
    SecDbForEachAttr(item->class, attr) {
        // Copy each attribute, except the mod date attribute (it will be reset to now when needed),
        // from the updates dict unless it's not there in which case we copy the attribute from the passed in item.
        if (attr->kind != kSecDbModificationDateAttr && attr->kind != kSecDbEncryptedDataAttr && attr->kind != kSecDbSHA1Attr && attr->kind != kSecDbPrimaryKeyAttr) {
            CFTypeRef value = NULL;
            if (CFDictionaryGetValueIfPresent(updates, attr->name, &value)) {
                if (!value)
                    SecError(errSecParam, error, CFSTR("NULL value in dictionary"));
            } else {
                value = SecDbItemGetValue(item, attr, error);
            }
            if (!value || !SecDbItemSetValue(new_item, attr, value, error)) {
                CFReleaseNull(new_item);
                break;
            }
        }
    }
    return new_item;
}

// Ensure that the date value of attr of new_item is greater than that of old_item.
static bool SecDbItemMakeAttrYounger(SecDbItemRef new_item, SecDbItemRef old_item, const SecDbAttr *attr, CFErrorRef *error) {
    CFDateRef old_date = SecDbItemGetValue(old_item, attr, error);
    if (!old_date)
        return false;
    CFDateRef new_date = SecDbItemGetValue(new_item, attr, error);
    if (!new_date)
        return false;
    bool ok = true;
    if (CFDateCompare(new_date, old_date, NULL) != kCFCompareGreaterThan) {
        CFDateRef adjusted_date = CFDateCreate(kCFAllocatorDefault, CFDateGetAbsoluteTime(old_date) + 0.001);
        if (adjusted_date) {
            ok = SecDbItemSetValue(new_item, attr, adjusted_date, error);
            CFRelease(adjusted_date);
        }
    }
    return ok;
}

// Ensure that the mod date of new_item is greater than that of old_item.
static bool SecDbItemMakeYounger(SecDbItemRef new_item, SecDbItemRef old_item, CFErrorRef *error) {
    const SecDbAttr *attr = SecDbClassAttrWithKind(new_item->class, kSecDbModificationDateAttr, error);
    return attr && SecDbItemMakeAttrYounger(new_item, old_item, attr, error);
}

SecDbItemRef SecDbItemCopyTombstone(SecDbItemRef item, CFErrorRef *error) {
    SecDbItemRef new_item = SecDbItemCreate(CFGetAllocator(item), item->class, item->keybag);
    SecDbForEachAttr(item->class, attr) {
        if (attr->kind == kSecDbTombAttr) {
            // Set the tomb attr to true to indicate a tombstone.
            if (!SecDbItemSetValue(new_item, attr, kCFBooleanTrue, error)) {
                CFReleaseNull(new_item);
                break;
            }
        } else if (SecDbIsTombstoneDbUpdateAttr(attr)) {
            // Copy all primary key attributes and creation timestamps from the original item.
            CFTypeRef value = SecDbItemGetValue(item, attr, error);
            if (!value || (!CFEqual(kCFNull, value) && !SecDbItemSetValue(new_item, attr, value, error))) {
                CFReleaseNull(new_item);
                break;
            }
        } else if (attr->kind == kSecDbModificationDateAttr) {
            if (!SecDbItemMakeAttrYounger(new_item, item, attr, error)) {
                CFReleaseNull(new_item);
                break;
            }
        }
    }

    return new_item;
}

// MARK: -
// MARK: SQL Construction helpers -- These should become private in the future

void SecDbAppendElement(CFMutableStringRef sql, CFStringRef value, bool *needComma) {
    assert(needComma);
    if (*needComma) {
        CFStringAppend(sql, CFSTR(","));
    } else {
        *needComma = true;
    }
    CFStringAppend(sql, value);
}

static void SecDbAppendElementEquals(CFMutableStringRef sql, CFStringRef value, bool *needComma) {
    SecDbAppendElement(sql, value, needComma);
    CFStringAppend(sql, CFSTR("=?"));
}

/* Append AND is needWhere is NULL or *needWhere is false.  Append WHERE
 otherwise.  Upon return *needWhere will be false.  */
void
SecDbAppendWhereOrAnd(CFMutableStringRef sql, bool *needWhere) {
    if (!needWhere || !*needWhere) {
        CFStringAppend(sql, CFSTR(" AND "));
    } else {
        CFStringAppend(sql, CFSTR(" WHERE "));
        *needWhere = false;
    }
}

void
SecDbAppendWhereOrAndEquals(CFMutableStringRef sql, CFStringRef col, bool *needWhere) {
    SecDbAppendWhereOrAnd(sql, needWhere);
    CFStringAppend(sql, col);
    CFStringAppend(sql, CFSTR("=?"));
}

static CFStringRef SecDbItemCopyInsertSQL(SecDbItemRef item, bool(^use_attr)(const SecDbAttr *attr)) {
    CFMutableStringRef sql = CFStringCreateMutable(CFGetAllocator(item), 0);
    CFStringAppend(sql, CFSTR("INSERT INTO "));
    CFStringAppend(sql, item->class->name);
    CFStringAppend(sql, CFSTR("("));
    bool needComma = false;
    CFIndex used_attr = 0;
    SecDbForEachAttr(item->class, attr) {
        if (use_attr(attr)) {
            ++used_attr;
            SecDbAppendElement(sql, attr->name, &needComma);
        }
    }
    CFStringAppend(sql, CFSTR(")VALUES(?"));
    while (used_attr-- > 1) {
        CFStringAppend(sql, CFSTR(",?"));
    }
    CFStringAppend(sql, CFSTR(")"));
    return sql;

}

static bool SecDbItemInsertBind(SecDbItemRef item, sqlite3_stmt *stmt, CFErrorRef *error, bool(^use_attr)(const SecDbAttr *attr)) {
    bool ok = true;
    int param = 0;
    SecDbForEachAttr(item->class, attr) {
        if (use_attr(attr)) {
            CFTypeRef value = SecDbItemGetValue(item, attr, error);
            if (!value || !SecDbAttrBind(attr, stmt, ++param, value, error)) {
                ok = false;
                break;
            }
        }
    }
    return ok;
}

sqlite3_int64 SecDbItemGetRowId(SecDbItemRef item, CFErrorRef *error) {
    sqlite3_int64 row_id = 0;
    const SecDbAttr *attr = SecDbClassAttrWithKind(item->class, kSecDbRowIdAttr, error);
    if (attr) {
        CFNumberRef number = SecDbItemGetValue(item, attr, error);
        if (!isNumber(number)|| !CFNumberGetValue(number, kCFNumberSInt64Type, &row_id))
            SecDbError(SQLITE_ERROR, error, CFSTR("rowid %@ is not a 64 bit number"), number);
    }

    return row_id;
}

static CFNumberRef SecDbItemCreateRowId(SecDbItemRef item, sqlite3_int64 rowid, CFErrorRef *error) {
    return CFNumberCreate(CFGetAllocator(item), kCFNumberSInt64Type, &rowid);
}

bool SecDbItemSetRowId(SecDbItemRef item, sqlite3_int64 rowid, CFErrorRef *error) {
    bool ok = true;
    const SecDbAttr *attr = SecDbClassAttrWithKind(item->class, kSecDbRowIdAttr, error);
    if (attr) {
        CFNumberRef value = SecDbItemCreateRowId(item, rowid, error);
        if (!value)
            return false;

        ok = SecDbItemSetValue(item, attr, value, error);
        CFRelease(value);
    }
    return ok;
}

static bool SecDbItemClearRowId(SecDbItemRef item, CFErrorRef *error) {
    bool ok = true;
    const SecDbAttr *attr = SecDbClassAttrWithKind(item->class, kSecDbRowIdAttr, error);
    if (attr) {
        CFDictionaryRemoveValue(item->attributes, attr->name);
        //ok = SecDbItemSetValue(item, attr, kCFNull, error);
    }
    return ok;
}

static bool SecDbItemSetLastInsertRowId(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error) {
    sqlite3_int64 rowid = sqlite3_last_insert_rowid(SecDbHandle(dbconn));
    return SecDbItemSetRowId(item, rowid, error);
}

bool SecDbItemIsSyncable(SecDbItemRef item) {
    bool is_syncable;
    if (SecDbItemGetBoolValue(item, SecDbClassAttrWithKind(item->class, kSecDbSyncAttr, NULL), &is_syncable, NULL))
        return is_syncable;
    return false;
}

bool SecDbItemSetSyncable(SecDbItemRef item, bool sync, CFErrorRef *error)
{
    return SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSyncAttr, error), sync ? kCFBooleanTrue : kCFBooleanFalse, error);
}

bool SecDbItemIsTombstone(SecDbItemRef item) {
    bool is_tomb;
    if (SecDbItemGetBoolValue(item, SecDbClassAttrWithKind(item->class, kSecDbTombAttr, NULL), &is_tomb, NULL))
        return is_tomb;
    return false;
}

CFDataRef SecDbItemGetPrimaryKey(SecDbItemRef item, CFErrorRef *error) {
    return SecDbItemGetValue(item, SecDbClassAttrWithKind(item->class, kSecDbPrimaryKeyAttr, error), error);
}

CFDataRef SecDbItemGetSHA1(SecDbItemRef item, CFErrorRef *error) {
    return SecDbItemGetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, error), error);
}

static SecDbQueryRef SecDbQueryCreateWithItemPrimaryKey(SecDbItemRef item, CFErrorRef *error) {
    CFMutableDictionaryRef dict = SecDbItemCopyPListWithMask(item, kSecDbPrimaryKeyFlag, error);
    if (!dict)
        return NULL;

    SecDbQueryRef query = query_create(item->class, NULL, error);
    if (query)
        query->q_item = dict;
    else
        CFRelease(dict);

    return query;
}

static bool SecDbItemIsCorrupt(SecDbItemRef item, bool *is_corrupt, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    bool ok = SecDbItemEnsureDecrypted(item, &localError);
    if (localError) {
        if (SecErrorGetOSStatus(localError) == errSecDecode) {
            // We failed to decrypt the item
            secerror("error %@ reading item %@ (corrupted)", localError, item);
            __security_simulatecrash(CFSTR("Corrupted item found in keychain"), __sec_exception_code_CorruptItem);
            *is_corrupt = true;
            ok = true;
        } else if (error && *error == NULL) {
            *error = localError;
            localError = NULL;
        }
        CFReleaseSafe(localError);
    }
    return ok;
}

static bool SecDbItemDoInsert(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error) {
    bool (^use_attr)(const SecDbAttr *attr) = ^bool(const SecDbAttr *attr) {
        return (attr->flags & kSecDbInFlag);
    };
    CFStringRef sql = SecDbItemCopyInsertSQL(item, use_attr);
    __block bool ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = (SecDbItemInsertBind(item, stmt, error, use_attr) &&
                  SecDbStep(dbconn, stmt, error, NULL) &&
                  SecDbItemSetLastInsertRowId(item, dbconn, error));
        });
        CFRelease(sql);
    }
    if (ok)
        secnotice("item", "inserted %@", item);

    return ok;
}

bool SecDbItemInsertOrReplace(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error, void(^duplicate)(SecDbItemRef item, SecDbItemRef *replace)) {
    __block CFErrorRef localError = NULL;
    __block bool ok = SecDbItemDoInsert(item, dbconn, &localError);
    if (!ok && localError && CFErrorGetCode(localError) == SQLITE_CONSTRAINT && CFEqual(kSecDbErrorDomain, CFErrorGetDomain(localError))) {
        SecDbQueryRef query = SecDbQueryCreateWithItemPrimaryKey(item, error);
        if (query) {
            SecDbItemSelect(query, dbconn, error, ^bool(const SecDbAttr *attr) {
                return attr->flags & kSecDbPrimaryKeyFlag;
            }, NULL, NULL, ^(SecDbItemRef old_item, bool *stop) {
                bool is_corrupt = false;
                ok = SecDbItemIsCorrupt(old_item, &is_corrupt, error);
                SecDbItemRef replace = NULL;
                if (is_corrupt) {
                    // If old_item is corrupted pretend it's not there and just replace it.
                    replace = item;
                    CFRetain(replace);
                } else if (ok && duplicate) {
                    duplicate(old_item, &replace);
                }
                if (replace) {
                    const SecDbAttr *rowid_attr = SecDbClassAttrWithKind(old_item->class, kSecDbRowIdAttr, error);
                    CFNumberRef oldrowid = SecDbItemGetCachedValue(old_item, rowid_attr);
                    if (oldrowid) {
                        ok = SecDbItemSetValue(replace, rowid_attr, oldrowid, &localError);
                        if (ok && !is_corrupt) {
                            ok = SecDbItemMakeYounger(replace, old_item, error);
                        }
                        ok = ok && SecDbItemDoUpdate(old_item, replace, dbconn, &localError, ^bool (const SecDbAttr *attr) {
                            return attr->kind == kSecDbRowIdAttr;
                        });
                    } else {
                        ok = SecError(errSecInternal, &localError, CFSTR("no rowid for %@"), old_item);
                    }
                    CFRelease(replace);
                    if (ok)
                        CFReleaseNull(localError); // Clear the error, since we replaced the item.
                }
            });
            ok &= query_destroy(query, error);
        }
        if (localError)
            ok = false;  // We didn't clear the error so we're failing again.
    }

    if (!ok) {
        /*
            CFErrorRef e = localError ? localError : error ? *error : NULL;
            secerror("INSERT %@ failed: %@%s", item, e, (e && CFErrorGetCode(e) == SQLITE_CONSTRAINT) ?
                " and SELECT failed to find tombstone" : "");
         */
        secerror("INSERT failed: %@", localError);
        if (localError) {
            if (error && *error == NULL) {
                *error = localError;
                localError = NULL;
            } else {
                CFReleaseNull(localError);
            }
        }
    }

    return ok;
}

bool SecDbItemInsert(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error) {
    return SecDbItemInsertOrReplace(item, dbconn, error, ^(SecDbItemRef old_item, SecDbItemRef *replace) {
        if (SecDbItemIsTombstone(old_item)) {
            CFRetain(item);
            *replace = item;
        }
    });
}

static CFStringRef SecDbItemCopyUpdateSQL(SecDbItemRef old_item, SecDbItemRef new_item, bool(^use_attr_in_where)(const SecDbAttr *attr)) {
    CFMutableStringRef sql = CFStringCreateMutable(CFGetAllocator(new_item), 0);
    CFStringAppend(sql, CFSTR("UPDATE "));
    CFStringAppend(sql, new_item->class->name);
    CFStringAppend(sql, CFSTR(" SET "));
    bool needComma = false;
    CFIndex used_attr = 0;
    SecDbForEachAttrWithMask(new_item->class, attr, kSecDbInFlag) {
        ++used_attr;
        SecDbAppendElementEquals(sql, attr->name, &needComma);
    }

    bool needWhere = true;
    SecDbForEachAttr(old_item->class, attr) {
        if (use_attr_in_where(attr)) {
            SecDbAppendWhereOrAndEquals(sql, attr->name, &needWhere);
        }
    }

    return sql;
}

static bool SecDbItemUpdateBind(SecDbItemRef old_item, SecDbItemRef new_item, sqlite3_stmt *stmt, CFErrorRef *error, bool(^use_attr_in_where)(const SecDbAttr *attr)) {
    bool ok = true;
    int param = 0;
    SecDbForEachAttrWithMask(new_item->class, attr, kSecDbInFlag) {
        CFTypeRef value = SecDbItemGetValue(new_item, attr, error);
        ok &= value && SecDbAttrBind(attr, stmt, ++param, value, error);
        if (!ok)
            break;
    }
    SecDbForEachAttr(old_item->class, attr) {
        if (use_attr_in_where(attr)) {
            CFTypeRef value = SecDbItemGetValue(old_item, attr, error);
            ok &= value && SecDbAttrBind(attr, stmt, ++param, value, error);
            if (!ok)
                break;
        }
    }
    return ok;
}

// Primary keys are the same -- do an update
bool SecDbItemDoUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, CFErrorRef *error, bool (^use_attr_in_where)(const SecDbAttr *attr)) {
    CFStringRef sql = SecDbItemCopyUpdateSQL(old_item, new_item, use_attr_in_where);
    __block bool ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = SecDbItemUpdateBind(old_item, new_item, stmt, error, use_attr_in_where) && SecDbStep(dbconn, stmt, error, NULL);
        });
        CFRelease(sql);
    }
    if (ok)
        secnotice("item", "replaced %@ with %@ in %@", old_item, new_item, dbconn);
    return ok;
}

static CFStringRef SecDbItemCopyDeleteSQL(SecDbItemRef item, bool(^use_attr_in_where)(const SecDbAttr *attr)) {
    CFMutableStringRef sql = CFStringCreateMutable(CFGetAllocator(item), 0);
    CFStringAppend(sql, CFSTR("DELETE FROM "));
    CFStringAppend(sql, item->class->name);
    bool needWhere = true;
    SecDbForEachAttr(item->class, attr) {
        if (use_attr_in_where(attr)) {
            SecDbAppendWhereOrAndEquals(sql, attr->name, &needWhere);
        }
    }

    return sql;
}

static bool SecDbItemDeleteBind(SecDbItemRef item, sqlite3_stmt *stmt, CFErrorRef *error, bool(^use_attr_in_where)(const SecDbAttr *attr)) {
    bool ok = true;
    int param = 0;
    SecDbForEachAttr(item->class, attr) {
        if (use_attr_in_where(attr)) {
            CFTypeRef value = SecDbItemGetValue(item, attr, error);
            ok &= value && SecDbAttrBind(attr, stmt, ++param, value, error);
            if (!ok)
                break;
        }
    }
    return ok;
}

static bool SecDbItemDoDelete(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error, bool (^use_attr_in_where)(const SecDbAttr *attr)) {
    CFStringRef sql = SecDbItemCopyDeleteSQL(item, use_attr_in_where);
    __block bool ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = SecDbItemDeleteBind(item, stmt, error, use_attr_in_where) && SecDbStep(dbconn, stmt, error, NULL);
        });
        CFRelease(sql);
    }
    if (ok)
        secnotice("item", "deleted %@ from %@", item, dbconn);
    return ok;
}

#if 0
static bool SecDbItemDeleteTombstone(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error) {
    bool ok = true;
    // TODO: Treat non decryptable items like tombstones here too and delete them
    SecDbItemRef tombstone = SecDbItemCopyTombstone(item, error);
    ok = tombstone;
    if (tombstone) {
        ok = SecDbItemClearRowId(tombstone, error);
        if (ok) {
            ok = SecDbItemDoDelete(tombstone, dbconn, error, ^bool (const SecDbAttr *attr) {
                return SecDbIsTombstoneDbSelectAttr(attr);
            });
        }
        CFRelease(tombstone);
    }
    return ok;
}
#endif

// Replace old_item with new_item.  If primary keys are the same this does an update otherwise it does a delete + add
bool SecDbItemUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;

    CFDataRef old_pk = SecDbItemGetPrimaryKey(old_item, error);
    CFDataRef new_pk = SecDbItemGetPrimaryKey(new_item, error);

    ok = old_pk && new_pk;

    bool pk_equal = ok && CFEqual(old_pk, new_pk);
    if (pk_equal) {
        ok = SecDbItemMakeYounger(new_item, old_item, error);
    }
    ok = ok && SecDbItemDoUpdate(old_item, new_item, dbconn, &localError, ^bool(const SecDbAttr *attr) {
        return attr->kind == kSecDbRowIdAttr;
    });

    if (localError) {
        if(CFErrorGetCode(localError) == SQLITE_CONSTRAINT && CFEqual(kSecDbErrorDomain, CFErrorGetDomain(localError))) {
            /* Update failed because we changed the PrimaryKey and there was a dup.
               Find the dup and see if it is a tombstone or corrupted item. */
            SecDbQueryRef query = SecDbQueryCreateWithItemPrimaryKey(new_item, error);
            ok = query;
            if (query) {
                ok &= SecDbItemSelect(query, dbconn, error, ^bool(const SecDbAttr *attr) {
                    return attr->flags & kSecDbPrimaryKeyFlag;
                }, NULL, NULL, ^(SecDbItemRef duplicate_item, bool *stop) {
                    bool is_corrupt = false;
                    bool is_tomb = false;
                    ok = SecDbItemIsCorrupt(duplicate_item, &is_corrupt, error);
                    if (ok && !is_corrupt) {
                        if ((is_tomb = SecDbItemIsTombstone(duplicate_item)))
                            ok = SecDbItemMakeYounger(new_item, duplicate_item, error);
                    }
                    if (ok && (is_corrupt || is_tomb)) {
                        ok = SecDbItemDoDelete(old_item, dbconn, error, ^bool (const SecDbAttr *attr) {
                            return attr->kind == kSecDbRowIdAttr;
                        });
                        ok = ok && SecDbItemDoUpdate(duplicate_item, new_item, dbconn, error, ^bool (const SecDbAttr *attr) {
                            return attr->kind == kSecDbRowIdAttr;
                        });
                        CFReleaseNull(localError);
                    }
                });
                ok &= query_destroy(query, error);
            }
        }

        if (localError) {
            ok = false;
            if (error && *error == NULL) {
                *error = localError;
                localError = NULL;
            }
            CFReleaseSafe(localError);
        }
    }

    if (ok && !pk_equal && makeTombstone) {
        /* The primary key of new_item is different than that of old_item, we
           have been asked to make a tombstone so leave one for the old_item. */
        SecDbItemRef tombstone = SecDbItemCopyTombstone(old_item, error);
        ok = tombstone;
        if (tombstone) {
            ok = (SecDbItemClearRowId(tombstone, error) &&
                  SecDbItemDoInsert(tombstone, dbconn, error));
            CFRelease(tombstone);
        }
    }

    return ok;
}

// Replace the object with a tombstone
bool SecDbItemDelete(SecDbItemRef item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error) {
    bool ok = false;
    if (makeTombstone) {
        SecDbItemRef tombstone = SecDbItemCopyTombstone(item, error);
        if (tombstone) {
            ok = SecDbItemDoUpdate(item, tombstone, dbconn, error, ^bool(const SecDbAttr *attr) {
                return attr->kind == kSecDbRowIdAttr;
            });
            CFRelease(tombstone);
        }
    } else {
        ok = SecDbItemDoDelete(item, dbconn, error, ^bool(const SecDbAttr *attr) {
            return attr->kind == kSecDbRowIdAttr;
        });
    }
    return ok;
}

CFStringRef SecDbItemCopySelectSQL(SecDbQueryRef query,
                                   bool (^return_attr)(const SecDbAttr *attr),
                                   bool (^use_attr_in_where)(const SecDbAttr *attr),
                                   bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere)) {
    CFMutableStringRef sql = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppend(sql, CFSTR("SELECT "));
    // What are we selecting?
    bool needComma = false;
    SecDbForEachAttr(query->q_class, attr) {
        if (return_attr(attr))
            SecDbAppendElement(sql, attr->name, &needComma);
    }

    // From which table?
    CFStringAppend(sql, CFSTR(" FROM "));
    CFStringAppend(sql, query->q_class->name);

    // And which elements do we want to select
    bool needWhere = true;
    SecDbForEachAttr(query->q_class, attr) {
        if (use_attr_in_where(attr)) {
            SecDbAppendWhereOrAndEquals(sql, attr->name, &needWhere);
        }
    }
    // Append SQL for access groups and limits.
    if (add_where_sql)
        add_where_sql(sql, &needWhere);

    return sql;
}

bool SecDbItemSelectBind(SecDbQueryRef query, sqlite3_stmt *stmt, CFErrorRef *error,
                         bool (^use_attr_in_where)(const SecDbAttr *attr),
                         bool (^bind_added_where)(sqlite3_stmt *stmt, int col)) {
    bool ok = true;
    int param = 0;
    SecDbForEachAttr(query->q_class, attr) {
        if (use_attr_in_where(attr)) {
            CFTypeRef value = CFDictionaryGetValue(query->q_item, attr->name);
            ok &= SecDbAttrBind(attr, stmt, ++param, value, error);
            if (!ok)
                break;
        }
    }
    // TODO: Bind arguments for access groups and limits.
    if (bind_added_where)
        bind_added_where(stmt, ++param);

    return ok;
}

bool SecDbItemSelect(SecDbQueryRef query, SecDbConnectionRef dbconn, CFErrorRef *error,
                     bool (^use_attr_in_where)(const SecDbAttr *attr),
                     bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                     bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                     void (^handle_row)(SecDbItemRef item, bool *stop)) {
    __block bool ok = true;
    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr;
    };
    CFStringRef sql = SecDbItemCopySelectSQL(query, return_attr, use_attr_in_where, add_where_sql);
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = (SecDbItemSelectBind(query, stmt, error, use_attr_in_where, bind_added_where) &&
                  SecDbStep(dbconn, stmt, error, ^(bool *stop) {
                SecDbItemRef item = SecDbItemCreateWithStatement(kCFAllocatorDefault, query->q_class, stmt, query->q_keybag, error, return_attr);
                if (item) {
                    handle_row(item, stop);
                    CFRelease(item);
                } else {
                    //*stop = true;
                    //ok = false;
                }
            }));
        });
        CFRelease(sql);
    } else {
        ok = false;
    }
    return ok;
}


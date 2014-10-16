/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

#if 0
static void persistentRefIs(CFDataRef pref, CFDataRef data) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFTypeRef result = NULL;
    CFDictionaryAddValue(dict, kSecValuePersistentRef, pref);
    CFDictionaryAddValue(dict, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(dict, &result), "lookup item data by persistent ref");
    ok(CFEqual(data, result), "result %@ equals expected data %@", result, data);
    CFReleaseNull(result);
    CFReleaseNull(dict);
}
#endif

enum ItemAttrType {
    kBoolItemAttr,
    kNumberItemAttr,
    kStringItemAttr,
    kDataItemAttr,
    kBlobItemAttr,
    kDateItemAttr,
    kAccessabilityItemAttr,
    kAccessGroupItemAttr,
};

static void WithEachString(void(^each)(CFStringRef attr, enum ItemAttrType atype), ...) {
    va_list ap;
    va_start(ap, each);
    CFStringRef attr;
    while((attr = va_arg(ap, CFStringRef)) != NULL) {
        enum ItemAttrType atype = va_arg(ap, enum ItemAttrType);
        each(attr, atype);
    }
    va_end(ap);
}

#if 0
static void ItemForEachAttr(CFMutableDictionaryRef item, void(^each)(CFStringRef attr, enum ItemAttrType atype)) {
    CFStringRef iclass = CFDictionaryGetValue(item, kSecClass);
    if (!iclass) {
        return;
    } else if (CFEqual(iclass, kSecClassGenericPassword)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCreationDate,        kDateItemAttr,
                       kSecAttrModificationDate,    kDateItemAttr,
                       kSecAttrDescription,         kStringItemAttr,
                       kSecAttrComment,             kStringItemAttr,
                       kSecAttrCreator,             kNumberItemAttr,
                       kSecAttrType,                kNumberItemAttr,
                       kSecAttrLabel,               kStringItemAttr,
                       kSecAttrIsInvisible,         kBoolItemAttr,
                       kSecAttrIsNegative,          kBoolItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrService,             kStringItemAttr,
                       kSecAttrGeneric,             kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassInternetPassword)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCreationDate,        kDateItemAttr,
                       kSecAttrModificationDate,    kDateItemAttr,
                       kSecAttrDescription,         kStringItemAttr,
                       kSecAttrComment,             kStringItemAttr,
                       kSecAttrCreator,             kNumberItemAttr,
                       kSecAttrType,                kNumberItemAttr,
                       kSecAttrLabel,               kStringItemAttr,
                       kSecAttrIsInvisible,         kBoolItemAttr,
                       kSecAttrIsNegative,          kBoolItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrSecurityDomain,      kStringItemAttr,
                       kSecAttrServer,              kStringItemAttr,
                       kSecAttrProtocol,            kNumberItemAttr,
                       kSecAttrAuthenticationType,  kNumberItemAttr,
                       kSecAttrPort,                kNumberItemAttr,
                       kSecAttrPath,                kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassCertificate)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrCertificateEncoding, kNumberItemAttr,
                       kSecAttrLabel,               kStringItemAttr,
                       kSecAttrSubject,             kDataItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSubjectKeyID,        kDataItemAttr,
                       kSecAttrPublicKeyHash,       kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassKey)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // Might be Number on replies
                       kSecAttrLabel,               kStringItemAttr,
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrIsPermanent,         kBoolItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrCanEncrypt,          kBoolItemAttr,
                       kSecAttrCanDecrypt,          kBoolItemAttr,
                       kSecAttrCanDerive,           kBoolItemAttr,
                       kSecAttrCanSign,             kBoolItemAttr,
                       kSecAttrCanVerify,           kBoolItemAttr,
                       kSecAttrCanWrap,             kBoolItemAttr,
                       kSecAttrCanUnwrap,           kBoolItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassIdentity)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrCertificateEncoding, kNumberItemAttr,
                       kSecAttrLabel,               kStringItemAttr,
                       kSecAttrSubject,             kDataItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSubjectKeyID,        kDataItemAttr,
                       kSecAttrPublicKeyHash,       kDataItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // Might be Number on replies
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrIsPermanent,         kBoolItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrCanEncrypt,          kBoolItemAttr,
                       kSecAttrCanDecrypt,          kBoolItemAttr,
                       kSecAttrCanDerive,           kBoolItemAttr,
                       kSecAttrCanSign,             kBoolItemAttr,
                       kSecAttrCanVerify,           kBoolItemAttr,
                       kSecAttrCanWrap,             kBoolItemAttr,
                       kSecAttrCanUnwrap,           kBoolItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    }
}
#endif

static void ItemForEachPKAttr(CFMutableDictionaryRef item, void(^each)(CFStringRef attr, enum ItemAttrType atype)) {
    CFStringRef iclass = CFDictionaryGetValue(item, kSecClass);
    if (!iclass) {
        return;
    } else if (CFEqual(iclass, kSecClassGenericPassword)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrService,             kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassInternetPassword)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrSecurityDomain,      kStringItemAttr,
                       kSecAttrServer,              kStringItemAttr,
                       kSecAttrProtocol,            kNumberItemAttr,
                       kSecAttrAuthenticationType,  kNumberItemAttr,
                       kSecAttrPort,                kNumberItemAttr,
                       kSecAttrPath,                kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassCertificate)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassKey)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // kNumberItemAttr on replies
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassIdentity)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // kNumberItemAttr on replies
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    }
}

static CFMutableDictionaryRef ItemCreate(int num) {
    CFStringRef iclass = NULL;
    switch (num % 4) {
        case 0:
            iclass = kSecClassInternetPassword;
            break;
        case 1:
            iclass = kSecClassGenericPassword;
            break;
        case 2:
            iclass = kSecClassKey;
            break;
        case 3:
            iclass = kSecClassCertificate;
            break;
    }
    return CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, iclass, NULL);
}

/* Test add api in all it's variants. */
static void tests(void)
{
    for (int num = 0 ; num < 8; ++num) {
        CFMutableDictionaryRef item = ItemCreate(num);
        ItemForEachPKAttr(item, ^(CFStringRef attr, enum ItemAttrType atype) {
            CFTypeRef value = NULL;
            switch (atype) {
                case kBoolItemAttr:
                    value = (num % 2 == 0 ? kCFBooleanTrue : kCFBooleanFalse);
                    CFRetain(value);
                    break;
                case kNumberItemAttr:
                    value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
                    break;
                case kStringItemAttr:
                case kBlobItemAttr:
                    value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("string-%d"), num);
                    break;
                case kDataItemAttr:
                {
                    char buf[10];
                    int len = snprintf(buf, sizeof(buf), "data-%d", num);
                    value = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buf, len);
                    break;
                }
                case kDateItemAttr:
                    value = NULL; // Don't mess with dates on create.
                    break;
                case kAccessabilityItemAttr:
                {
                    CFStringRef accessabilites[] = {
                        kSecAttrAccessibleWhenUnlocked,
                        kSecAttrAccessibleAfterFirstUnlock,
                        kSecAttrAccessibleAlways,
                        kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                        kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                        kSecAttrAccessibleAlwaysThisDeviceOnly,
                    };
                    value = accessabilites[num % array_size(accessabilites)];
                    break;
                }
                case kAccessGroupItemAttr:
                {
                    CFStringRef accessGroups[] = {
                        NULL,
#if 0
#if NO_SERVER
                        CFSTR("test"),
                        CFSTR("apple"),
                        CFSTR("lockdown-identities"),
#else
                        CFSTR("sync"),
#endif
                        CFSTR("com.apple.security.sos"),          // Secd internally uses this

                        CFSTR("com.apple.security.regressions"),  // SecurityTestApp is in this group.
#endif
                    };
                    value = accessGroups[num % array_size(accessGroups)];
                    break;
                }
            }
            if (value)
                CFDictionarySetValue(item, attr, value);
            CFReleaseSafe(value);
        });

        CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanTrue);
        ok_status(SecItemAdd(item, NULL), "add sync");

        // No tombstones by default per rdar://14680869, so explicitly add one
        CFDictionarySetValue(item, kSecUseTombstones, kCFBooleanTrue);
        ok_status(SecItemDelete(item), "delete sync");

        CFDictionarySetValue(item, kSecAttrTombstone, kCFBooleanTrue);
        ok_status(SecItemCopyMatching(item, NULL), "find tombstone after delete sync");
        ok_status(SecItemDelete(item), "delete sync tombstone");
        CFDictionaryRemoveValue(item, kSecAttrTombstone);

        ok_status(SecItemAdd(item, NULL), "add sync again");

        CFDictionarySetValue(item, kSecUseTombstones, kCFBooleanFalse);
        ok_status(SecItemDelete(item), "delete sync without leaving a tombstone behind");
        CFDictionaryRemoveValue(item, kSecUseTombstones);

        CFDictionarySetValue(item, kSecAttrTombstone, kCFBooleanTrue);
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find tombstone after delete sync with kSecUseTombstones=false");

        CFDictionaryRemoveValue(item, kSecAttrSynchronizable);
        ok_status(SecItemAdd(item, NULL), "add local");
        ok_status(SecItemDelete(item), "delete local");

        CFDictionarySetValue(item, kSecAttrTombstone, kCFBooleanTrue);
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find tombstone after delete local");
        is_status(SecItemDelete(item), errSecItemNotFound, "do not delete tombstone after delete local");
        CFDictionaryRemoveValue(item, kSecAttrTombstone);

        ok_status(SecItemAdd(item, NULL), "add local again");

        CFDictionarySetValue(item, kSecUseTombstones, kCFBooleanTrue);
        ok_status(SecItemDelete(item), "delete local and leave a tombstone behind");
        CFDictionaryRemoveValue(item, kSecUseTombstones);

        CFDictionarySetValue(item, kSecAttrTombstone, kCFBooleanTrue);
        ok_status(SecItemCopyMatching(item, NULL), "find tombstone after delete sync with kSecUseTombstones=true");

        CFDictionarySetValue(item, kSecUseTombstones, kCFBooleanTrue);
        ok_status(SecItemDelete(item), "delete local tombstone kSecUseTombstones=true");
        CFDictionaryRemoveValue(item, kSecUseTombstones);

        ok_status(SecItemCopyMatching(item, NULL), "find tombstone after delete local tombstone with kSecUseTombstones=true");
        ok_status(SecItemDelete(item), "delete local tombstone");
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find tombstone after delete local");

        CFRelease(item);
    }
}

int si_12_item_stress(int argc, char *const *argv)
{
	plan_tests(144);

	tests();

	return 0;
}

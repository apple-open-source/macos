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

/*
 * SOSBackupEvent.c -  Implementation of a secure object syncing peer
 */

#include <Security/SecureObjectSync/SOSBackupEvent.h>
#include <corecrypto/ccsha1.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/array_size.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <AssertMacros.h>

//
// MARK: statics
//

/*
    Event ASN.1 definitions

ResetEvent := SEQUENCE {
    keybag OCTET STRING OPTIONAL
}

AddEvent := SET {
    SEQUENCE {
        UTF8STRING :class
        class UTF8STRING
    }
    SEQUENCE {
        UTF8STRING :hash
        hash OCTET STRING
    }
    SEQUENCE {
        UTF8STRING :data
        data OCTET STRING
    }
}

DeleteEvent := OCTET STRING

CompleteEvent := INTEGER

Event := CHOICE {
    reset ResetEvent
    add AddEvent
    delete DeleteEvent
    complete CompleteEvent
}

 */

static size_t der_sizeof_backup_reset(CFDataRef keybag) {
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
        keybag ? ccder_sizeof_raw_octet_string(CFDataGetLength(keybag)) : 0);
}

static uint8_t* der_encode_backup_reset(CFDataRef keybag, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        keybag ? ccder_encode_raw_octet_string(CFDataGetLength(keybag), CFDataGetBytePtr(keybag), der, der_end) : der_end);
}

static size_t der_sizeof_backup_add(CFDictionaryRef add) {
    return der_sizeof_dictionary(add, NULL);
}

static uint8_t* der_encode_backup_add(CFDictionaryRef add, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    // der_dictionary tag is CCDER_CONSTRUCTED_SET
    return der_encode_dictionary(add, error, der, der_end);
}

static size_t der_sizeof_backup_delete(CFDataRef deletedDigest) {
    return ccder_sizeof_raw_octet_string(CFDataGetLength(deletedDigest));
}

static uint8_t* der_encode_backup_delete(CFDataRef deletedDigest, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    return ccder_encode_raw_octet_string(CFDataGetLength(deletedDigest), CFDataGetBytePtr(deletedDigest), der, der_end);
}

static size_t der_sizeof_backup_complete(uint64_t event_num) {
    return ccder_sizeof_uint64(event_num);
}

static uint8_t* der_encode_backup_complete(uint64_t event_num, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    return ccder_encode_uint64(event_num, der, der_end);
}


//
// MARK: SPI
//

static bool SOSBackupEventWrite(FILE *journalFile, CFErrorRef *error,
                                size_t len,
                                uint8_t *(^encode)(const uint8_t *der, uint8_t *der_end))
{
    bool ok = false;
    CFMutableDataRef derObject = CFDataCreateMutable(kCFAllocatorDefault, len);
    CFDataSetLength(derObject, len);
    uint8_t *der_end = CFDataGetMutableBytePtr(derObject);
    const uint8_t *der = der_end;
    der_end += len;

    require(der_end = encode(der, der_end), xit);
    require_action(der == der_end, xit, SecError(-1, error, CFSTR("size mismatch der: %zu der_end: %zu")));

    ok = SecCheckErrno(1 != fwrite(der, len, 1, journalFile), error, CFSTR("fwrite %s"), journalFile);
xit:
    CFReleaseSafe(derObject);
    return ok;
}

bool SOSBackupEventWriteReset(FILE *journalFile, CFDataRef keybag, CFErrorRef *error) {
    return SOSBackupEventWrite(journalFile, error, der_sizeof_backup_reset(keybag), ^uint8_t *(const uint8_t *der, uint8_t *der_end) {
        return der_encode_backup_reset(keybag, error, der, der_end);
    });
}

bool SOSBackupEventWriteDelete(FILE *journalFile, CFDataRef deletedDigest, CFErrorRef *error) {
    return SOSBackupEventWrite(journalFile, error, der_sizeof_backup_delete(deletedDigest), ^uint8_t *(const uint8_t *der, uint8_t *der_end) {
        return der_encode_backup_delete(deletedDigest, error, der, der_end);
    });
}

bool SOSBackupEventWriteAdd(FILE *journalFile, CFDictionaryRef backup_item, CFErrorRef *error) {
    return SOSBackupEventWrite(journalFile, error, der_sizeof_backup_add(backup_item), ^uint8_t *(const uint8_t *der, uint8_t *der_end) {
        return der_encode_backup_add(backup_item, error, der, der_end);
    });
}

bool SOSBackupEventWriteCompleteMarker(FILE *journalFile, uint64_t eventID, CFErrorRef *error) {
    bool ok = SOSBackupEventWrite(journalFile, error, der_sizeof_backup_complete(eventID), ^uint8_t *(const uint8_t *der, uint8_t *der_end) {
        return der_encode_backup_complete(eventID, error, der, der_end);
    });
    // TODO: Move this to right before we send a notification or something.
    fflush(journalFile);
    return ok;
}

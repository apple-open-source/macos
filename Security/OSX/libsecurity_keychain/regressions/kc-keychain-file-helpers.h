/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef kc_file_helpers_h
#define kc_file_helpers_h

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Security/SecItem.h>
#include <Security/SecKeychain.h>
#include "keychain_regressions.h"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

/* Deletes any keychain files that might exist at this location, and ignore any errors */
static void deleteKeychainFiles(const char* basename) {
    // remove the keychain if it exists, but ignore any errors
    unlink(basename);
    char * dbFilename = NULL;
    asprintf(&dbFilename, "%s-db", basename);
    unlink(dbFilename);
    free(dbFilename);
}

static SecKeychainRef createNewKeychainAt(const char * filename, const char * password) {
    deleteKeychainFiles(filename);

    SecKeychainRef keychain = NULL;
    ok_status(SecKeychainCreate(filename, (UInt32) strlen(password), password, FALSE, NULL, &keychain), "SecKeychainCreate");
    return keychain;
}

static SecKeychainRef createNewKeychain(const char * name, const char * password) {
    const char *home_dir = getenv("HOME");
    char * filename;

    asprintf(&filename, "%s/Library/Keychains/%s", home_dir, name);
    SecKeychainRef keychain = createNewKeychainAt(filename, password);
    free(filename);
    return keychain;
}

static void writeFile(const char* path, uint8_t* buf, size_t len) {
    FILE * fp = fopen(path, "w+");
    fwrite(buf, sizeof(uint8_t), len, fp);
    fclose(fp);
    sync();
}

SecKeychainRef CF_RETURNS_RETAINED getPopulatedTestKeychain(void);
#define getPopulatedTestKeychainTests 2

SecKeychainRef CF_RETURNS_RETAINED getEmptyTestKeychain(void);
#define getEmptyTestKeychainTests 1

// The following keychain includes:
//
// security add-internet-password -s test_service_restrictive_acl -a test_account -j "a useful comment" -r "htps" -t dflt -w test_password test.keychain
// security add-internet-password -s test_service -a test_account -j "a useful comment" -r "htps" -t dflt -w test_password -A test.keychain
// security add-generic-password -a test_account -s test_service -j "another useful comment" -w test_password -A test.keychain
// security add-generic-password -a test_account -s test_service_restrictive_acl -j "another useful comment" -w test_password test.keychain

// With certificate assistant, added a:
//   Code Signing identity
//   S/MIME identity

extern const char * test_keychain_password;

extern unsigned char test_keychain[];

extern unsigned int test_keychain_len;



#pragma clang diagnostic pop

#endif /* kc_file_helpers_h */

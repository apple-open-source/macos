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
 *
 * keychain_backup.c
 */

#include <TargetConditionals.h>
#if !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE

#include <fcntl.h>
#include <AppleKeyStore/libaks.h>

#include "SecurityCommands.h"

#include <AssertMacros.h>
#include <Security/SecItemPriv.h>

#include <utilities/SecCFWrappers.h>

#include "SecurityTool/sharedTool/readline.h"
#include "SecurityTool/sharedTool/tool_errors.h"


static int
do_keychain_import(const char *backupPath, const char *keybagPath, const char *passwordString)
{
    CFDataRef backup=NULL;
    CFDataRef keybag=NULL;
    CFDataRef password=NULL;
    bool ok=false;

    if(passwordString) {
        require(password = CFDataCreate(NULL, (UInt8 *)passwordString, strlen(passwordString)), out);
    }
    require(keybag=copyFileContents(keybagPath), out);
    require(backup=copyFileContents(backupPath), out);

    ok=_SecKeychainRestoreBackup(backup, keybag, password);

out:
    CFReleaseSafe(backup);
    CFReleaseSafe(keybag);
    CFReleaseSafe(password);

    return ok?0:1;
}

static int
do_keychain_export(const char *backupPath, const char *keybagPath, const char *passwordString, const bool useFd)
{
    CFDataRef backup=NULL;
    CFDataRef keybag=NULL;
    CFDataRef password=NULL;
    bool ok=false;

    if (keybagPath) {
        require(keybag=copyFileContents(keybagPath), out);
    }
    if(passwordString) {
        require(password = CFDataCreate(NULL, (UInt8 *)passwordString, strlen(passwordString)), out);
    }

    if (keybagPath && !useFd) {
        require(backup=_SecKeychainCopyBackup(keybag, password), out);
        ok=writeFileContents(backupPath, backup);
    } else {
        mode_t mode = 0644; // octal!
        int fd = open(backupPath, O_RDWR|O_CREAT|O_TRUNC, mode);
        if (fd < 0) {
            sec_error("failed to open file %s (%d) %s", backupPath, errno, strerror(errno));
            goto out;
        }
        CFErrorRef error = NULL;
        ok = _SecKeychainWriteBackupToFileDescriptor(keybag, password, fd, &error);
        if (!ok) {
            sec_error("_SecKeychainWriteBackupToFileDescriptor error: %ld", (long)CFErrorGetCode(error));
        }
    }

out:
    CFReleaseSafe(backup);
    CFReleaseSafe(keybag);
    CFReleaseSafe(password);

    return ok?0:1;
}


int
keychain_import(int argc, char * const *argv)
{
    int ch;
    const char *keybag=NULL;
    const char *password=NULL;

    while ((ch = getopt(argc, argv, "k:p:")) != -1)
    {
        switch (ch)
        {
            case 'k':
                keybag=optarg;
                break;
            case 'p':
                password=optarg;
                break;
             default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    argc -= optind;
    argv += optind;

    if(keybag==NULL) {
        sec_error("-k is required\n");
        return SHOW_USAGE_MESSAGE;
    }

    if (argc != 1) {
        sec_error("<backup> is required\n");
        return SHOW_USAGE_MESSAGE;
    }
    
    return do_keychain_import(argv[0], keybag, password);
}

int
keychain_export(int argc, char * const *argv)
{
    int ch;
    const char *keybag=NULL;
    const char *password=NULL;
    bool useFd = false;

    while ((ch = getopt(argc, argv, "k:p:f")) != -1)
    {
        switch (ch)
        {
            case 'k':
                keybag=optarg;
                break;
            case 'p':
                password=optarg;
                break;
            case 'f':
                useFd=true;
                break;
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    argc -= optind;
    argv += optind;

    if (keybag == NULL && password != NULL) {
        sec_error("-k is required when -p is specified\n");
        return SHOW_USAGE_MESSAGE;
    }

    if (argc != 1) {
        sec_error("<plist> is required\n");
        return SHOW_USAGE_MESSAGE;
    }

    return do_keychain_export(argv[0], keybag, password, useFd);
}

int
keychain_backup_get_uuid(int argc, char * const *argv)
{
    // Skip subcommand
    argc--;
    argv++;

    if (argc != 1) {
        sec_error("<plist> is required\n");
        return SHOW_USAGE_MESSAGE;
    }

    const char* const backupPath = argv[0];
    int fd = open(backupPath, O_RDWR);
    if (fd < 0) {
        sec_error("failed to open file %s (%d) %s", backupPath, errno, strerror(errno));
        return 1;
    }
    CFErrorRef error = NULL;
    CFStringRef uuidStr = _SecKeychainCopyKeybagUUIDFromFileDescriptor(fd, &error);
    if (!uuidStr) {
        sec_error("error: %ld", (long)CFErrorGetCode(error));
        return 1;
    }

    printf("%s\n", CFStringGetCStringPtr(uuidStr, kCFStringEncodingUTF8));
    CFReleaseNull(uuidStr);
    return 0;
}

int
keychain_backup_generate_keybag(int argc, char * const *argv)
{
    int ch;
    const char *keybag=NULL;
    const char *password=NULL;
    bool asym = false;

    while ((ch = getopt(argc, argv, "ap:")) != -1)
    {
        switch (ch)
        {
            case 'a':
                asym=true;
                break;
            case 'p':
                password=optarg;
                break;
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    argc -= optind;
    argv += optind;

    if (password == NULL) {
        sec_error("password is required\n");
        return SHOW_USAGE_MESSAGE;
    }

    if (argc != 1) {
        sec_error("keybag path is required\n");
        return SHOW_USAGE_MESSAGE;
    }

    keybag = argv[0];

    keybag_handle_t handle;
    kern_return_t result;
    char uuidstr[37];
    uuid_t uuid;
    void *data = NULL;
    int length;

    result = aks_create_bag(password, (int)strlen(password), asym ? kAppleKeyStoreAsymmetricBackupBag : kAppleKeyStoreBackupBag, &handle);
    if (result) {
        sec_error("aks_create_bag: %08x", result);
        return -1;
    }

    result = aks_save_bag(handle, &data, &length);
    if (result) {
        sec_error("aks_save_bag: %08x", result);
        return -1;
    }

    result = aks_get_bag_uuid(handle, uuid);
    if (result) {
        sec_error("aks_get_bag_uuid: %08x", result);
        return -1;
    }

    uuid_unparse_lower(uuid, uuidstr);

    CFDataRef bytes = CFDataCreate(NULL, (UInt8 *)data, length);
    if (!bytes) {
        sec_error("CFData create");
        return -1;
    }

    result = aks_unload_bag(handle);
    if (result) {
        sec_error("aks_unload_bag: %08x", result);
        CFReleaseSafe(bytes);
        return -1;
    }

    printf("UUID: %s\n", uuidstr);

    bool ok = writeFileContents(keybag, bytes);

    CFReleaseSafe(bytes);

    return ok?0:1;
}

#endif /* !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE */

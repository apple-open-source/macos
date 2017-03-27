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
#if TARGET_OS_EMBEDDED

#include "SecurityCommands.h"

#include <AssertMacros.h>
#include <Security/SecItemPriv.h>

#include <utilities/SecCFWrappers.h>

#include <SecurityTool/readline.h>
#include <SecurityTool/tool_errors.h>


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
do_keychain_export(const char *backupPath, const char *keybagPath, const char *passwordString)
{
    CFDataRef backup=NULL;
    CFDataRef keybag=NULL;
    CFDataRef password=NULL;
    bool ok=false;

    if(passwordString) {
        require(password = CFDataCreate(NULL, (UInt8 *)passwordString, strlen(passwordString)), out);
    }
    require(keybag=copyFileContents(keybagPath), out);
    require(backup=_SecKeychainCopyBackup(keybag, password), out);

    ok=writeFileContents(backupPath, backup);

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
    int verbose=0;
    const char *keybag=NULL;
    const char *password=NULL;

    while ((ch = getopt(argc, argv, "vk:p:")) != -1)
    {
        switch (ch)
        {
            case 'v':
                verbose++;
                break;
            case 'k':
                keybag=optarg;
                break;
            case 'p':
                password=optarg;
                break;
             default:
                return 2; /* Trigger usage message. */
        }
    }

    argc -= optind;
    argv += optind;

    if(keybag==NULL) {
        sec_error("-k is required\n");
        return 2;
    }

    if (argc != 1) {
        sec_error("<backup> is required\n");
        return 2; /* Trigger usage message. */
    }
    
    return do_keychain_import(argv[0], keybag, password);
}

int
keychain_export(int argc, char * const *argv)
{
    int ch;
    int verbose=0;
    const char *keybag=NULL;
    const char *password=NULL;

    while ((ch = getopt(argc, argv, "vk:p:")) != -1)
    {
        switch (ch)
        {
            case 'v':
                verbose++;
                break;
            case 'k':
                keybag=optarg;
                break;
            case 'p':
                password=optarg;
                break;
            default:
                return 2; /* Trigger usage message. */
        }
    }

    argc -= optind;
    argv += optind;

    if(keybag==NULL) {
        sec_error("-k is required\n");
        return 2;
    }

    if (argc != 1) {
        sec_error("<backup> is required\n");
        return 2; /* Trigger usage message. */
    }

    return do_keychain_export(argv[0], keybag, password);
}

#endif /* TARGET_OS_EMBEDDED */


/*
 * Copyright (c) 2003-2007,2009-2010,2013-2016 Apple Inc. All Rights Reserved.
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
 */

//
//  syncbackup.c
//  sec
//
//
//

#include "syncbackup.h"


#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSBackupInformation.h>

#include <utilities/SecCFWrappers.h>

#include <SecurityTool/readline.h>
#include "secToolFileIO.h"


static bool dumpBackupInfo(CFErrorRef *error) {
    CFReleaseNull(*error);
    bool isLast = SOSCCIsThisDeviceLastBackup(error);
    
    printmsg(CFSTR("This %s the last backup peer.\n"), (isLast) ? "is": "isn't");
    return *error != NULL;
}

static bool longListing(CFErrorRef *error) {
    CFDataRef rkbgder = NULL;
    CFDictionaryRef bskbders = NULL;
    
    CFDictionaryRef backupInfo = SOSCCCopyBackupInformation(error);
    SOSRecoveryKeyBagRef rkbg = NULL;
    CFNumberRef status = CFDictionaryGetValue(backupInfo, kSOSBkpInfoStatus);
    int infoStatus;
    CFNumberGetValue(status, kCFNumberIntType, &infoStatus);
    
    switch(infoStatus) {
        case noError:
            rkbgder = CFDictionaryGetValue(backupInfo, kSOSBkpInfoRKBG);
            bskbders = CFDictionaryGetValue(backupInfo, kSOSBkpInfoBSKB);
            break;
        case noTxnorAcct:
            break;
        case noAlloc:
            break;
        case noTrustedPubKey:
            break;
        case noBSKBs:
            rkbgder = CFDictionaryGetValue(backupInfo, kSOSBkpInfoRKBG);
            break;
        default:
            break;
    }
    
    if(rkbgder) {
        rkbg = SOSRecoveryKeyBagCreateFromData(kCFAllocatorDefault, rkbgder, NULL);
        printmsg(CFSTR("Recovery Keybag: %@\n"), rkbg);
    }
    
    if(bskbders) {
        CFDataRef rkPub = NULL;
        if(rkbg) rkPub = SOSRecoveryKeyBagGetKeyData(rkbg, NULL);
        CFDictionaryForEach(bskbders, ^(const void *key, const void *value) {
            CFDataRef bskbder = asData(value, NULL);
            SOSBackupSliceKeyBagRef bskb = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, bskbder, NULL);
            if(bskb) {
                bool reckeyPresent = (rkPub && SOSBKSBPrefixedKeyIsInKeyBag(bskb, bskbRkbgPrefix, rkPub));
                printmsg(CFSTR("BackupSliceKeybag %@: Recovery Key %s; %@\n"), key, (reckeyPresent) ? "Present": "Absent ", bskb);
                CFReleaseNull(bskb);
            }
        });
    }
    CFReleaseNull(backupInfo);
    CFReleaseNull(rkbg);
    return *error != NULL;
}



int
syncbackup(int argc, char * const *argv)
{
    /*
     "Circle Backup Information"
     "    -i     info (current status)"
     
     */
    SOSLogSetOutputTo(NULL, NULL);
    
    int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;
    
    while ((ch = getopt(argc, argv, "il")) != -1)
        switch  (ch) {
                
            case 'i':
                hadError = dumpBackupInfo(&error);
                break;
                
            case 'l':
                hadError = longListing(&error);
                break;
                
            case '?':
            default:
                return 2; /* Return 2 triggers usage message. */
        }
    
    if (hadError)
        printerr(CFSTR("Error: %@\n"), error);
    
    return result;
}

//
//  keychain_log.c
//  sec
//
//  Created by Richard Murphy on 1/26/16.
//
//

#include "keychain_log.h"

/*
 * Copyright (c) 2003-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 * keychain_add.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>

#include <Security/SecItem.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/securityd/SOSCloudCircleServer.h"
#include <Security/SecOTRSession.h>
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "SecurityTool/sharedTool/readline.h"
#include <notify.h>

#include "keychain_log.h"
#include "secToolFileIO.h"
#include "secViewDisplay.h"
#include "accountCirclesViewsPrint.h"
#include <utilities/debugging.h>


#include <Security/SecPasswordGenerate.h>

#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18

static bool logmark(const char *optarg) {
    if(!optarg) return false;
    secnotice("mark", "%s", optarg);
    return true;
}


// enable, disable, accept, reject, status, Reset, Clear
int
keychain_log(int argc, char * const *argv)
{
    /*
     "Keychain Logging"
     "    -i     info (current status)"
     "    -D     [itemName]  dump contents of KVS"
     "    -L     list all known view and their status"
     "    -M string   place a mark in the syslog - category \"mark\""

     */
    SOSLogSetOutputTo(NULL, NULL);

    int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;

    while ((ch = getopt(argc, argv, "DiLM:")) != -1)
        switch  (ch) {

            case 'i':
                SOSCCDumpCircleInformation();
                SOSCCDumpEngineInformation();
                break;


            case 'D':
                (void)SOSCCDumpCircleKVSInformation(optarg);
                break;
                
            case 'L':
                hadError = !listviewcmd(&error);
                break;
                
            case 'M':
                hadError = !logmark(optarg);
                break;

            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    
    if (hadError)
        printerr(CFSTR("Error: %@\n"), error);
    
    return result;
}

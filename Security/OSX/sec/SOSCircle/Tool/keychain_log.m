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

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecOTRSession.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <SecurityTool/readline.h>
#include <notify.h>

#include "SOSSysdiagnose.h"
#include "keychain_log.h"
#include "secToolFileIO.h"
#include "secViewDisplay.h"
#include "accountCirclesViewsPrint.h"
#include <utilities/debugging.h>


#include <Security/SecPasswordGenerate.h>

#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18

#define USE_NEW_SPI 1
#if ! USE_NEW_SPI

static char *createDateStrNow() {
    char *retval = NULL;
    time_t clock;

    struct tm *tmstruct;

    time(&clock);
    tmstruct = localtime(&clock);

    retval = malloc(15);
    sprintf(retval, "%04d%02d%02d%02d%02d%02d", tmstruct->tm_year+1900, tmstruct->tm_mon+1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    return retval;
}

// #include <CoreFoundation/CFPriv.h>

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;

static char *CFDictionaryCopyCString(CFDictionaryRef dict, const void *key) {
    CFStringRef val = CFDictionaryGetValue(dict, key);
    char *retval = CFStringToCString(val);
    return retval;
}

#include <pwd.h>

static void sysdiagnose_dump() {
    char *outputBase = NULL;
    char *outputParent = NULL;
    char *outputDir = NULL;
    char hostname[80];
    char *productName = "NA";
    char *productVersion = "NA";
    char *buildVersion = "NA";
    char *keysToRegister = NULL;
    char *cloudkeychainproxy3 = NULL;
    char *now = createDateStrNow();
    size_t length = 0;
    int status = 0;
    CFDictionaryRef sysfdef = _CFCopySystemVersionDictionary();

    if(gethostname(hostname, 80)) {
        strcpy(hostname, "unknownhost");
    }

    if(sysfdef) {
        productName = CFDictionaryCopyCString(sysfdef, _kCFSystemVersionProductNameKey);
        productVersion = CFDictionaryCopyCString(sysfdef, _kCFSystemVersionProductVersionKey);
        buildVersion = CFDictionaryCopyCString(sysfdef, _kCFSystemVersionBuildVersionKey);
    }

    //     OUTPUTBASE=ckcdiagnose_snapshot_${HOSTNAME}_${PRODUCT_VERSION}_${NOW}
    length = strlen("ckcdiagnose_snapshot___") + strlen(hostname) + strlen(productVersion) + strlen(now) + 1;
    outputBase = malloc(length);
    status = snprintf(outputBase, length, "ckcdiagnose_snapshot_%s_%s_%s", hostname, productVersion, now);
    if(status < 0) outputBase = "";

#if TARGET_OS_EMBEDDED
    outputParent = "/Library/Logs/CrashReporter";
    keysToRegister = "/private/var/preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist";
    cloudkeychainproxy3 = "/var/mobile/Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist";
#else
    outputParent = "/var/tmp";
    {
        char *homeDir = "";
        struct passwd* pwd = getpwuid(getuid());
        if (pwd) homeDir = pwd->pw_dir;

        char *k2regfmt = "%s/Library/Preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist";
        char *ckp3fmt = "%s/Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist";
        size_t k2rlen = strlen(homeDir) + strlen(k2regfmt) + 2;
        size_t ckp3len = strlen(homeDir) + strlen(ckp3fmt) + 2;
        keysToRegister = malloc(k2rlen);
        cloudkeychainproxy3 = malloc(ckp3len);
        snprintf(keysToRegister, k2rlen, k2regfmt, homeDir);
        snprintf(cloudkeychainproxy3, ckp3len, ckp3fmt, homeDir);
    }
#endif

    length = strlen(outputParent) + strlen(outputBase) + 2;
    outputDir = malloc(length);
    status = snprintf(outputDir, length, "%s/%s", outputParent, outputBase);
    if(status < 0) return;

    mkdir(outputDir, 0700);

    SOSLogSetOutputTo(outputDir, "sw_vers.log");
    // report uname stuff + hostname
    fprintf(outFile, "HostName:	  %s\n", hostname);
    fprintf(outFile, "ProductName:	  %s\n", productName);
    fprintf(outFile, "ProductVersion: %s\n", productVersion);
    fprintf(outFile, "BuildVersion:	  %s\n", buildVersion);
    closeOutput();

    SOSLogSetOutputTo(outputDir, "syncD.log");
    // do sync -D
    SOSCCDumpCircleKVSInformation(optarg);
    closeOutput();

    SOSLogSetOutputTo(outputDir, "synci.log");
    // do sync -i
    SOSCCDumpCircleInformation();
    SOSCCDumpEngineInformation();
    closeOutput();

    SOSLogSetOutputTo(outputDir, "syncL.log");
    // do sync -L
    listviewcmd(NULL);
    closeOutput();

    copyFileToOutputDir(outputDir, keysToRegister);
    copyFileToOutputDir(outputDir, cloudkeychainproxy3);

    free(now);
    CFReleaseNull(sysfdef);
#if ! TARGET_OS_EMBEDDED
    free(keysToRegister);
    free(cloudkeychainproxy3);
#endif

}
#else
static void sysdiagnose_dump() {
    SOSCCSysdiagnose(NULL);
}

#endif /* USE_NEW_SPI */

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
     "    -s     sysdiagnose log dumps"
     "    -M string   place a mark in the syslog - category \"mark\""

     */
    SOSLogSetOutputTo(NULL, NULL);

    int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;

    while ((ch = getopt(argc, argv, "DiLM:s")) != -1)
        switch  (ch) {

            case 'i':
                SOSCCDumpCircleInformation();
                SOSCCDumpEngineInformation();
                break;


            case 's':
                sysdiagnose_dump();
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

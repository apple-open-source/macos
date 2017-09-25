//
//  SOSSysdiagnose.c
//  sec
//
//  Created by Richard Murphy on 1/27/16.
//
//


#include "SOSCloudCircleInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>
#include <notify.h>
#include <pwd.h>

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

#include "keychain_log.h"
#include "secToolFileIO.h"
#include "secViewDisplay.h"
#include "accountCirclesViewsPrint.h"



#include <Security/SecPasswordGenerate.h>

/* Copied from CFPriv.h */
// #include <CoreFoundation/CFPriv.h>

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;



static char *CFDictionaryCopyCStringWithDefault(CFDictionaryRef dict, const void *key, char *defaultString) {
    char *retval = NULL;
    require_quiet(dict, use_default);
    CFStringRef val = CFDictionaryGetValue(dict, key);
    retval = CFStringToCString(val);
use_default:
    if(!retval) retval = strdup(defaultString);
    return retval;
}

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

#if !TARGET_OS_EMBEDDED
static char *assemblePath(char *dir, char *fname) {
    size_t length = strlen(dir) + strlen(fname) + 2;
    char *outputDir = malloc(length);
    int status = snprintf(outputDir, length, "%s/%s", dir, fname);
    if(status < 0) return NULL;
    return outputDir;
}

static char *homedirPath() {
    char *homeDir = "";
    struct passwd* pwd = getpwuid(getuid());
    if (pwd) homeDir = pwd->pw_dir;
    return homeDir;
}
#endif

static char *sysdiagnose_dir(const char *passedIn, const char *hostname, const char *productVersion, const char *now) {
    if(passedIn) return (char *) passedIn;
    
    //     OUTPUTBASE=ckcdiagnose_snapshot_${HOSTNAME}_${PRODUCT_VERSION}_${NOW}
    char *outputParent = NULL;
    size_t length = strlen("ckcdiagnose_snapshot___") + strlen(hostname) + strlen(productVersion) + strlen(now) + 1;
    char *outputBase = malloc(length);
    int status = snprintf(outputBase, length, "ckcdiagnose_snapshot_%s_%s_%s", hostname, productVersion, now);
    if(status < 0) outputBase = "";
    
#if TARGET_OS_EMBEDDED
    outputParent = "/Library/Logs/CrashReporter";
#else
    outputParent = "/var/tmp";
#endif
    length = strlen(outputParent) + strlen(outputBase) + 2;
    char *outputDir = malloc(length);
    status = snprintf(outputDir, length, "%s/%s", outputParent, outputBase);
    if(status < 0) return NULL;
    return outputDir;
}


static char *sysdiagnose_dump(const char *dirname) {
    char *outputDir = NULL;
    char hostname[80];
    char *productName = NULL;
    char *productVersion = NULL;
    char *buildVersion = NULL;
    char *keysToRegister = NULL;
    char *cloudkeychainproxy3 = NULL;
    char *now = createDateStrNow();
    
    CFDictionaryRef sysfdef = _CFCopySystemVersionDictionary();
    productName = CFDictionaryCopyCStringWithDefault(sysfdef, _kCFSystemVersionProductNameKey, "unknownProduct");
    productVersion = CFDictionaryCopyCStringWithDefault(sysfdef, _kCFSystemVersionProductVersionKey, "unknownProductVersion");
    buildVersion = CFDictionaryCopyCStringWithDefault(sysfdef, _kCFSystemVersionBuildVersionKey, "unknownVersion");

    if(gethostname(hostname, 80)) {
        strcpy(hostname, "unknownhost");
    }
    
#if TARGET_OS_EMBEDDED
    keysToRegister = "/private/var/preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist";
    cloudkeychainproxy3 = "/var/mobile/Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist";
#else
    char *homeDir = homedirPath();
    keysToRegister = assemblePath(homeDir, "Library/Preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist");
    cloudkeychainproxy3 = assemblePath(homeDir, "Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist");
#endif
    
    outputDir = sysdiagnose_dir(dirname, hostname, productVersion, now);
    if(!outputDir) goto errOut;
    
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
    closeOutput();
    
    SOSLogSetOutputTo(outputDir, "syncL.log");
    // do sync -L
    listviewcmd(NULL);
    closeOutput();
    
    copyFileToOutputDir(outputDir, keysToRegister);
    copyFileToOutputDir(outputDir, cloudkeychainproxy3);

errOut:
    if(now) free(now);
    CFReleaseNull(sysfdef);
#if ! TARGET_OS_EMBEDDED
    free(keysToRegister);
    free(cloudkeychainproxy3);
#endif
    if(productName) free(productName);
    if(productVersion) free(productVersion);
    if(buildVersion) free(buildVersion);
    return outputDir;
}


char *SOSCCSysdiagnose(const char *directoryname) {
    sysdiagnose_dump(directoryname);
    return NULL;
}


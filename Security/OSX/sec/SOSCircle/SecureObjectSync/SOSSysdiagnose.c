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



#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18

//
//  secToolFileIO.c
//  sec
//
//  Created by Richard Murphy on 1/22/16.
//
//

#include <copyfile.h>
#include <libgen.h>
#include <utilities/SecCFWrappers.h>

#define printmsg(format, ...) _printcfmsg(outFile, NULL, format, __VA_ARGS__)
#define printmsgWithFormatOptions(formatOptions, format, ...) _printcfmsg(outFile, formatOptions, format, __VA_ARGS__)
#define printerr(format, ...) _printcfmsg(errFile, NULL, format, __VA_ARGS__)


FILE *outFile = NULL;
FILE *errFile = NULL;

void _printcfmsg(FILE *ff, CFDictionaryRef formatOptions, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);
    CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, formatOptions, format, args);
    va_end(args);
    CFStringPerformWithCString(message, ^(const char *utf8String) { fprintf(ff, utf8String, ""); });
    CFRelease(message);
}


int setOutputTo(char *dir, char *filename) {
    size_t pathlen = 0;
    
    if(dir && filename) {
        pathlen = strlen(dir) + strlen(filename) + 2;
        char path[pathlen];
        snprintf(path, pathlen, "%s/%s", dir, filename);
        outFile = fopen(path, "a");
    } else if(dir || filename) {
        outFile = stdout;
        return -1;
    } else {
        outFile = stdout;
    }
    errFile = stderr;
    return 0;
}

void closeOutput(void) {
    if(outFile != stdout) {
        fclose(outFile);
    }
    outFile = stdout;
}

int copyFileToOutputDir(char *dir, char *toCopy) {
    char *bname = basename(toCopy);
    char destpath[256];
    int status;
    copyfile_state_t cpfilestate = copyfile_state_alloc();
    
    status = snprintf(destpath, 256, "%s/%s", dir, bname);
    if(status < 0 || status > 256) return -1;
    
    int retval = copyfile(toCopy, destpath, cpfilestate, COPYFILE_ALL);
    
    copyfile_state_free(cpfilestate);
    return retval;
}



static const char *getSOSCCStatusDescription(SOSCCStatus ccstatus)
{
    switch (ccstatus)
    {
        case kSOSCCInCircle:        return "In Circle";
        case kSOSCCNotInCircle:     return "Not in Circle";
        case kSOSCCRequestPending:  return "Request pending";
        case kSOSCCCircleAbsent:    return "Circle absent";
        case kSOSCCError:           return "Circle error";
            
        default:
            return "<unknown ccstatus>";
            break;
    }
}

static void printPeerInfos(char *label, CFArrayRef (^getArray)(CFErrorRef *error)) {
    CFErrorRef error = NULL;
    CFArrayRef ppi = getArray(&error);
    SOSPeerInfoRef me = SOSCCCopyMyPeerInfo(NULL);
    CFStringRef mypeerID = SOSPeerInfoGetPeerID(me);
    
    if(ppi) {
        printmsg(CFSTR("%s count: %ld\n"), label, (long)CFArrayGetCount(ppi));
        CFArrayForEach(ppi, ^(const void *value) {
            char buf[160];
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFIndex version = SOSPeerInfoGetVersion(peer);
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            CFStringRef devtype = SOSPeerInfoGetPeerDeviceType(peer);
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            CFStringRef transportType = CFSTR("KVS");
            CFStringRef deviceID = CFSTR("");
            CFDictionaryRef gestalt = SOSPeerInfoCopyPeerGestalt(peer);
            CFStringRef osVersion = CFDictionaryGetValue(gestalt, CFSTR("OSVersion"));
            CFReleaseNull(gestalt);
            
            
            if(version >= 2){
                CFDictionaryRef v2Dictionary = peer->v2Dictionary;
                transportType = CFDictionaryGetValue(v2Dictionary, sTransportType);
                deviceID = CFDictionaryGetValue(v2Dictionary, sDeviceID);
            }
            char *pname = CFStringToCString(peerName);
            char *dname = CFStringToCString(devtype);
            char *tname = CFStringToCString(transportType);
            char *iname = CFStringToCString(deviceID);
            char *osname = CFStringToCString(osVersion);
            const char *me = CFEqualSafe(mypeerID, peerID) ? "me>" : "   ";
            
            
            snprintf(buf, 160, "%s %s: %-16s %-16s %-16s %-16s", me, label, pname, dname, tname, iname);
            
            free(pname);
            free(dname);
            CFStringRef pid = SOSPeerInfoGetPeerID(peer);
            CFIndex vers = SOSPeerInfoGetVersion(peer);
            printmsg(CFSTR("%s %@ V%d OS:%s\n"), buf, pid, vers, osname);
            free(osname);
        });
    } else {
        printmsg(CFSTR("No %s, error: %@\n"), label, error);
    }
    CFReleaseNull(ppi);
    CFReleaseNull(error);
}

static void dumpCircleInfo()
{
    CFErrorRef error = NULL;
    CFArrayRef generations = NULL;
    CFArrayRef confirmedDigests = NULL;
    bool is_user_public_trusted = false;
    __block int count = 0;
    
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    if(ccstatus == kSOSCCError) {
        printmsg(CFSTR("End of Dump - unable to proceed due to ccstatus (%s) error: %@\n"), getSOSCCStatusDescription(ccstatus), error);
        return;
    }
    printmsg(CFSTR("ccstatus: %s (%d)\n"), getSOSCCStatusDescription(ccstatus), ccstatus, error);
    
    is_user_public_trusted = SOSCCValidateUserPublic(&error);
    if(is_user_public_trusted)
        printmsg(CFSTR("Account user public is trusted%@"),CFSTR("\n"));
    else
        printmsg(CFSTR("Account user public is not trusted error:(%@)\n"), error);
    CFReleaseNull(error);
    
    generations = SOSCCCopyGenerationPeerInfo(&error);
    if(generations) {
        CFArrayForEach(generations, ^(const void *value) {
            count++;
            if(count%2 == 0)
                printmsg(CFSTR("Circle name: %@, "),value);
            
            if(count%2 != 0) {
                CFStringRef genDesc = SOSGenerationCountCopyDescription(value);
                printmsg(CFSTR("Generation Count: %@"), genDesc);
                CFReleaseNull(genDesc);
            }
            printmsg(CFSTR("%s\n"), "");
        });
    } else {
        printmsg(CFSTR("No generation count: %@\n"), error);
    }
    CFReleaseNull(generations);
    CFReleaseNull(error);
    
    printPeerInfos("     Peers", ^(CFErrorRef *error) { return SOSCCCopyValidPeerPeerInfo(error); });
    printPeerInfos("   Invalid", ^(CFErrorRef *error) { return SOSCCCopyNotValidPeerPeerInfo(error); });
    printPeerInfos("   Retired", ^(CFErrorRef *error) { return SOSCCCopyRetirementPeerInfo(error); });
    printPeerInfos("    Concur", ^(CFErrorRef *error) { return SOSCCCopyConcurringPeerPeerInfo(error); });
    printPeerInfos("Applicants", ^(CFErrorRef *error) { return SOSCCCopyApplicantPeerInfo(error); });
    
    confirmedDigests = SOSCCCopyEngineState(&error);
    if(confirmedDigests)
    {
        count = 0;
        CFArrayForEach(confirmedDigests, ^(const void *value) {
            count++;
            if(count % 2 != 0)
                printmsg(CFSTR("%@"), value);
            
            if(count % 2 == 0) {
                CFStringRef hexDigest = CFDataCopyHexString(value);
                printmsg(CFSTR(" %@\n"), hexDigest);
                CFReleaseSafe(hexDigest);
            }
        });
    }
    else
        printmsg(CFSTR("No engine peers: %@\n"), error);
    CFReleaseNull(confirmedDigests);
}

static CFTypeRef getObjectsFromCloud(CFArrayRef keysToGet, dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    __block CFTypeRef object = NULL;
    
    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    
    dispatch_group_enter(dgroup);
    
    CloudKeychainReplyBlock replyBlock =
    ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
            //       CFRelease(*error);
        }
        dispatch_group_leave(dgroup);
        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    if (!keysToGet)
        SOSCloudKeychainGetAllObjectsFromCloud(processQueue, replyBlock);
    else
        SOSCloudKeychainGetObjectsFromCloud(keysToGet, processQueue, replyBlock);
    
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    dispatch_release(waitSemaphore);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
    }
    secerror("returned: %@", object);
    return object;
}

static CFStringRef printFullDataString(CFDataRef data){
    __block CFStringRef fullData = NULL;
    
    BufferPerformWithHexString(CFDataGetBytePtr(data), CFDataGetLength(data), ^(CFStringRef dataHex) {
        fullData = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), dataHex);
    });
    
    return fullData;
}

static void displayLastKeyParameters(CFTypeRef key, CFTypeRef value)
{
    CFDataRef valueAsData = asData(value, NULL);
    if(valueAsData){
        CFDataRef dateData = CFDataCreateCopyFromRange(kCFAllocatorDefault, valueAsData, CFRangeMake(0, DATE_LENGTH));
        CFDataRef keyParameterData = CFDataCreateCopyFromPositions(kCFAllocatorDefault, valueAsData, DATE_LENGTH, CFDataGetLength(valueAsData));
        CFStringRef dateString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, dateData, kCFStringEncodingUTF8);
        CFStringRef keyParameterDescription = UserParametersDescription(keyParameterData);
        if(keyParameterDescription)
            printmsg(CFSTR("%@: %@: %@\n"), key, dateString, keyParameterDescription);
        else
            printmsg(CFSTR("%@: %@\n"), key, printFullDataString(value));
        CFReleaseNull(dateString);
        CFReleaseNull(keyParameterData);
        CFReleaseNull(dateData);
        CFReleaseNull(keyParameterDescription);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayKeyParameters(CFTypeRef key, CFTypeRef value)
{
    if(isData(value)){
        CFStringRef keyParameterDescription = UserParametersDescription((CFDataRef)value);
        
        if(keyParameterDescription)
            printmsg(CFSTR("%@: %@\n"), key, keyParameterDescription);
        else
            printmsg(CFSTR("%@: %@\n"), key, value);
        
        CFReleaseNull(keyParameterDescription);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayLastCircle(CFTypeRef key, CFTypeRef value)
{
    CFDataRef valueAsData = asData(value, NULL);
    if(valueAsData){
        CFErrorRef localError = NULL;
        
        CFDataRef dateData = CFDataCreateCopyFromRange(kCFAllocatorDefault, valueAsData, CFRangeMake(0, DATE_LENGTH));
        CFDataRef circleData = CFDataCreateCopyFromPositions(kCFAllocatorDefault, valueAsData, DATE_LENGTH, CFDataGetLength(valueAsData));
        CFStringRef dateString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, dateData, kCFStringEncodingUTF8);
        SOSCircleRef circle = SOSCircleCreateFromData(NULL, (CFDataRef) circleData, &localError);
        
        if(circle){
            CFIndex size = 5;
            CFNumberRef idLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &size);
            CFDictionaryRef format = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("SyncD"), CFSTR("SyncD"), CFSTR("idLength"), idLength, NULL);
            printmsgWithFormatOptions(format, CFSTR("%@: %@: %@\n"), key, dateString, circle);
            CFReleaseNull(idLength);
            CFReleaseNull(format);
            
        }
        else
            printmsg(CFSTR("%@: %@\n"), key, printFullDataString(circleData));
        
        CFReleaseNull(dateString);
        CFReleaseNull(circleData);
        CFReleaseSafe(circle);
        CFReleaseNull(dateData);
        CFReleaseNull(localError);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayCircle(CFTypeRef key, CFTypeRef value)
{
    CFDataRef circleData = (CFDataRef)value;
    
    CFErrorRef localError = NULL;
    if (isData(circleData))
    {
        CFIndex size = 5;
        CFNumberRef idLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &size);
        CFDictionaryRef format = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("SyncD"), CFSTR("SyncD"), CFSTR("idLength"), idLength, NULL);
        SOSCircleRef circle = SOSCircleCreateFromData(NULL, circleData, &localError);
        printmsgWithFormatOptions(format, CFSTR("%@: %@\n"), key, circle);
        CFReleaseSafe(circle);
        CFReleaseNull(idLength);
        CFReleaseNull(format);
        
    }
    else
        printmsg(CFSTR("%@: %@\n"), key, value);
}

static void displayMessage(CFTypeRef key, CFTypeRef value)
{
    CFDataRef message = (CFDataRef)value;
    if(isData(message)){
        const char* messageType = SecOTRPacketTypeString(message);
        printmsg(CFSTR("%@: %s: %ld\n"), key, messageType, CFDataGetLength(message));
    }
    else
        printmsg(CFSTR("%@: %@\n"), key, value);
}

static void printEverything(CFTypeRef objects)
{
    CFDictionaryForEach(objects, ^(const void *key, const void *value) {
        if (isData(value))
        {
            printmsg(CFSTR("%@: %@\n\n"), key, printFullDataString(value));
        }
        else
            printmsg(CFSTR("%@: %@\n"), key, value);
    });
    
}

static void decodeForKeyType(CFTypeRef key, CFTypeRef value, SOSKVSKeyType type){
    switch (type) {
        case kCircleKey:
            displayCircle(key, value);
            break;
        case kRetirementKey:
        case kMessageKey:
            displayMessage(key, value);
            break;
        case kParametersKey:
            displayKeyParameters(key, value);
            break;
        case kLastKeyParameterKey:
            displayLastKeyParameters(key, value);
            break;
        case kLastCircleKey:
            displayLastCircle(key, value);
            break;
        case kInitialSyncKey:
        case kAccountChangedKey:
        case kDebugInfoKey:
        case kRingKey:
        case kPeerInfoKey:
        default:
            printmsg(CFSTR("%@: %@\n"), key, value);
            break;
    }
}

static void decodeAllTheValues(CFTypeRef objects){
    SOSKVSKeyType keyType = 0;
    __block bool didPrint = false;
    
    for (keyType = 0; keyType <= MAXKVSKEYTYPE; keyType++){
        CFDictionaryForEach(objects, ^(const void *key, const void *value) {
            if(SOSKVSKeyGetKeyType(key) == keyType){
                decodeForKeyType(key, value, keyType);
                didPrint = true;
            }
        });
        if(didPrint)
            printmsg(CFSTR("%@\n"), CFSTR(""));
        didPrint = false;
    }
}
static bool dumpKVS(char *itemName, CFErrorRef *err)
{
    CFArrayRef keysToGet = NULL;
    if (itemName)
    {
        CFStringRef itemStr = CFStringCreateWithCString(kCFAllocatorDefault, itemName, kCFStringEncodingUTF8);
        fprintf(outFile, "Retrieving %s from KVS\n", itemName);
        keysToGet = CFArrayCreateForCFTypes(kCFAllocatorDefault, itemStr, NULL);
        CFReleaseSafe(itemStr);
    }
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);
    dispatch_group_t work_group = dispatch_group_create();
    CFTypeRef objects = getObjectsFromCloud(keysToGet, generalq, work_group);
    CFReleaseSafe(keysToGet);
    if (objects)
    {
        fprintf(outFile, "All keys and values straight from KVS\n");
        printEverything(objects);
        fprintf(outFile, "\nAll values in decoded form...\n");
        decodeAllTheValues(objects);
    }
    fprintf(outFile, "\n");
    return true;
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
    
    setOutputTo(outputDir, "sw_vers.log");
    // report uname stuff + hostname
    fprintf(outFile, "HostName:	  %s\n", hostname);
    fprintf(outFile, "ProductName:	  %s\n", productName);
    fprintf(outFile, "ProductVersion: %s\n", productVersion);
    fprintf(outFile, "BuildVersion:	  %s\n", buildVersion);
    closeOutput();
    
    setOutputTo(outputDir, "syncD.log");
    // do sync -D
    dumpKVS(optarg, NULL);
    closeOutput();
    
    setOutputTo(outputDir, "synci.log");
    // do sync -i
    dumpCircleInfo();
    closeOutput();
    
    setOutputTo(outputDir, "syncL.log");
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


/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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
#include <libc.h>
#include <sysexits.h>
#include <asl.h>
#include <syslog.h>
#include <sys/resource.h>
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>

#include "kext_tools_util.h"


#if PRAGMA_MARK
#pragma mark Basic Utility
#endif /* PRAGMA_MARK */
/*********************************************************************
*********************************************************************/
char * createUTF8CStringForCFString(CFStringRef aString)
{
    char     * result = NULL;
    CFIndex    bufferLength = 0;

    if (!aString) {
        goto finish;
    }

    bufferLength = sizeof('\0') +
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(aString),
        kCFStringEncodingUTF8);

    result = (char *)malloc(bufferLength * sizeof(char));
    if (!result) {
        goto finish;
    }
    if (!CFStringGetCString(aString, result, bufferLength,
        kCFStringEncodingUTF8)) {

        SAFE_FREE_NULL(result);
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* createCFMutableArray()
*******************************************************************************/
Boolean createCFMutableArray(CFMutableArrayRef * array,
    const CFArrayCallBacks * callbacks)
{
    Boolean result = true;

    *array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        callbacks);
    if (!*array) {
        result = false;
    }
    return result;
}

/*******************************************************************************
* createCFMutableDictionary()
*******************************************************************************/
Boolean createCFMutableDictionary(CFMutableDictionaryRef * dict)
{
    Boolean result = true;

    *dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!*dict) {
        result = false;
    }
    return result;
}

/*******************************************************************************
* createCFMutableSet()
*******************************************************************************/
Boolean createCFMutableSet(CFMutableSetRef * setOut,
    const CFSetCallBacks * callbacks)
{
    Boolean result = true;

    *setOut = CFSetCreateMutable(kCFAllocatorDefault, 0,
        callbacks);
    if (!*setOut) {
        result = false;
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
void addToArrayIfAbsent(CFMutableArrayRef array, const void * value)
{
    if (kCFNotFound == CFArrayGetFirstIndexOfValue(array, RANGE_ALL(array),
        value)) {
        
        CFArrayAppendValue(array, value);
    }
    return;
}

#if PRAGMA_MARK
#pragma mark Path & File
#endif /* PRAGMA_MARK */
/*******************************************************************************
*******************************************************************************/
ExitStatus checkPath(
    const char * path,
    const char * suffix,  // w/o the dot
    Boolean      directoryRequired,
    Boolean      writableRequired)
{
    Boolean result  = EX_USAGE;
    Boolean nameBad = FALSE;
    struct  stat statBuffer;
    
    if (!path) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Internal error - %s - NULL path.",
                __FUNCTION__);
        result = EX_SOFTWARE;
        goto finish;
    }
    
    result = EX_USAGE;
    if (suffix) {
        size_t pathLength   = strlen(path);
        size_t suffixLength = strlen(suffix);
        size_t suffixIndex = 0;
        size_t periodIndex = 0;

        nameBad = TRUE;
        if (!pathLength || !suffixLength) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Internal error - %s - empty string.",
                __FUNCTION__);
            result = EX_SOFTWARE;
            goto finish;
        }

       /* Peel off any trailing '/' characters (silly shell completion),
        * then advance the length back to point to the character past
        * the real end (which will be a slash or '\0').
        */
        while (pathLength-- && path[pathLength] == '/') {
            /* just scanning for last non-slash */
            if (!pathLength) {
                goto finish;
            }
        }
        pathLength++;

        if (suffixLength >= pathLength) {
            goto finish;
        }
        suffixIndex = pathLength - suffixLength;
        periodIndex = suffixIndex - 1;
        if (path[periodIndex] != '.' ||
            strncmp(path + suffixIndex, suffix, suffixLength)) {
            goto finish;
        }
        nameBad = FALSE;
    }

    result = EX_NOINPUT;
    if (0 != stat(path, &statBuffer)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't stat %s - %s.", path,
            strerror(errno));
        goto finish;
    }
    
    if (directoryRequired && !(statBuffer.st_mode & S_IFDIR) ) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "%s is not a directory.",
            path);
        goto finish;
    }

    result = EX_NOPERM;
    if (writableRequired && access(path, W_OK) == -1) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
            "%s is not writable.", path);
        goto finish;
    }

    result = EX_OK;
    
finish:
    if (nameBad) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
            "%s not of type '%s'.", path, suffix);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
void
saveFile(const void * vKey, const void * vValue, void * vContext)
{
    CFStringRef       key      = (CFStringRef)vKey;
    CFDataRef         fileData = (CFDataRef)vValue;
    SaveFileContext * context  = (SaveFileContext *)vContext;

    CFURLRef          saveURL = NULL;     // must release
    CFBooleanRef      fileExists = NULL;  // must release
    char              savePath[PATH_MAX];
    SInt32   error;
    
    if (context->fatal) {
        goto finish;
    }
    
    saveURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
        context->saveDirURL, key, /* isDirectory */ false);
    if (!saveURL) {
        context->fatal = true;
        goto finish;
    }

    if (!CFURLGetFileSystemRepresentation(saveURL, /* resolveToBase */ false,
        (u_char *)savePath, sizeof(savePath))) {
        
        // qlog
        context->fatal = true;
        goto finish;
    }
    
    if (!context->overwrite) {
        fileExists = CFURLCreatePropertyFromResource(kCFAllocatorDefault, saveURL,
            kCFURLFileExists, &error);
        if (!fileExists || CFBooleanGetTypeID() != CFGetTypeID(fileExists)) {
            OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
                "Error checking file: CFError %d.", (int)error);
        }
        if (CFBooleanGetValue(fileExists)) {
            switch (user_approve(1, "%s exists, overwrite", savePath)) {
                case -1:
                    context->fatal = true;
                    goto finish;
                    break;
                case 0:
                    goto finish;
                    break;
                case 1:
                    // go ahead and overwrite.
                    break;
            }
        }
    }

    if (!CFURLWriteDataAndPropertiesToResource(saveURL, fileData,
        /* properties */ NULL, &error)) {
        
       /* Is this fatal to the whole program? I'd rather soldier on.
        */
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to save %s.", savePath);
    }
        

finish:
    SAFE_RELEASE(saveURL);
    return;
}

/*******************************************************************************
*******************************************************************************/
CFStringRef copyKextPath(OSKextRef aKext)
{
    CFStringRef result = NULL;
    CFURLRef    absURL = NULL;  // must release
    
    if (!OSKextGetURL(aKext)) {
        goto finish;
    }
    
    absURL = CFURLCopyAbsoluteURL(OSKextGetURL(aKext));
    if (!absURL) {
        goto finish;
    }
    result = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
finish:
    SAFE_RELEASE(absURL);
    return result;
}

#if PRAGMA_MARK
#pragma mark Logging
#endif /* PRAGMA_MARK */
/*******************************************************************************
*******************************************************************************/
OSKextLogSpec _sLogSpecsForVerboseLevels[] = {
    kOSKextLogErrorLevel    | kOSKextLogVerboseFlagsMask,   // [0xff1] -v 0
    kOSKextLogBasicLevel    | kOSKextLogVerboseFlagsMask,   // [0xff3] -v 1
    kOSKextLogProgressLevel | kOSKextLogVerboseFlagsMask,   // [0xff4] -v 2
    kOSKextLogStepLevel     | kOSKextLogVerboseFlagsMask,   // [0xff5] -v 3
    kOSKextLogDetailLevel   | kOSKextLogVerboseFlagsMask,   // [0xff6] -v 4
    kOSKextLogDebugLevel    | kOSKextLogVerboseFlagsMask,   // [0xff7] -v 5
    kOSKextLogDebugLevel    | kOSKextLogVerboseFlagsMask |  // [0xfff] -v 6
        kOSKextLogKextOrGlobalMask
};

/*******************************************************************************
* getopt_long_only() doesn't actually handle optional args very well. So, we
* jump through some hoops here to handle all six possibilities:
*
*   cmd line      optarg  argv[optind]
*   ----------------------------------------------
*   -v            (null)  (following arg or null)
*   -v arg        (null)  arg
*   -v=arg        (null)  -v=arg -- ILLEGAL
*   -verbose      (null)  (following arg or null)
*   -verbose arg  (null)  arg
*   -verbose=arg  arg     (following arg or null)
*
* Note that only in the -verbose=arg case does optarg actually get set
* correctly!
*
* If we have not optarg but a following argv[optind], we check it to see if
* it looks like a legal arg to -v/-verbose; if it matches we increment optind.
* -v has never allowed the argument to immediately follow (as in -v2), so
* we still don't handle that.
*******************************************************************************/
#define kBadVerboseOptPrefix  "-v="

ExitStatus setLogFilterForOpt(
    int            argc,
    char * const * argv,
    OSKextLogSpec  forceOnFlags)
{
    ExitStatus      result       = EX_USAGE;
    OSKextLogSpec   logFilter    = OSKextGetLogFilter(/* kernel? */ false);
    const char    * localOptarg  = NULL;

   /* Must be a bare -v; just use the extra flags.
    */
    if (!optarg && optind >= argc) {
        logFilter = _sLogSpecsForVerboseLevels[1];
        
    } else {

        if (optarg) {
            localOptarg = optarg;
        } else {
            localOptarg = argv[optind];
        }

        if (!strncmp(localOptarg, kBadVerboseOptPrefix,
            sizeof(kBadVerboseOptPrefix) - 1)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "%s - syntax error (don't use = with single-letter option args).",
                localOptarg);
            goto finish;
        }

       /* Look for a 0x#### style verbose arg.
        */
        if (localOptarg[0] == '0' && localOptarg[1] == 'x') {
            char          * endptr      = NULL;
            OSKextLogSpec   parsedFlags = strtoul(localOptarg, &endptr, 16);

            if (endptr[0]) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "Can't parse verbose argument %s.", localOptarg);
                goto finish;
            }
            logFilter = parsedFlags;
            
            if (!optarg) {
                optind++;
            }

       /* Now a 0-6 style verbose arg.
        */
        } else if (((localOptarg[0] >= '0') || (localOptarg[0] <= '6')) &&
            (localOptarg[1] == '\0')) {

            logFilter = _sLogSpecsForVerboseLevels[localOptarg[0] - '0'];

            if (!optarg) {
                optind++;
            }

       /* Must be a -v with command args following; just use the extra flag.
        */
        } else {
            logFilter = _sLogSpecsForVerboseLevels[1];
        }
    }

    logFilter = logFilter | forceOnFlags;

    OSKextSetLogFilter(logFilter, /* kernel? */ false);
    OSKextSetLogFilter(logFilter, /* kernel? */ true);

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
void beQuiet(void)
{
    fclose(stdout);
    fclose(stderr);
    close(1);
    close(2);
    OSKextSetLogFilter(kOSKextLogSilentFilter, /* kernel? */ false);
    OSKextSetLogFilter(kOSKextLogSilentFilter, /* kernel? */ true);
    return;
}

/*******************************************************************************
*******************************************************************************/
FILE * g_log_stream = NULL;
aslclient gASLClientHandle = NULL;
aslmsg    gASLMessage      = NULL;  // reused
// xxx - need to aslclose()

void tool_openlog(const char * name)
{
    // xxx - do we want separate name & facility?
    gASLClientHandle = asl_open(/* ident */ name, /* facility*/ name,
        /* options */ 0);
    gASLMessage     = asl_new(ASL_TYPE_MSG);
    return;
}

/*******************************************************************************
* Basic log function. If any log flags are set, log the message
* to syslog/stderr.
*******************************************************************************/

void tool_log(
    OSKextRef       aKext __unused,
    OSKextLogSpec   msgLogSpec,
    const char    * format, ...)
{
    va_list ap;

    if (gASLClientHandle) {
        int            aslLevel = ASL_LEVEL_ERR;
        OSKextLogSpec  kextLogLevel = msgLogSpec & kOSKextLogLevelMask;
        char           messageLogSpec[16];

        if (kextLogLevel == kOSKextLogErrorLevel) {
            aslLevel = ASL_LEVEL_ERR;
        } else if (kextLogLevel == kOSKextLogWarningLevel) {
            aslLevel = ASL_LEVEL_WARNING;
        } else if (kextLogLevel == kOSKextLogBasicLevel) {
            aslLevel = ASL_LEVEL_NOTICE;
        } else if (kextLogLevel < kOSKextLogDebugLevel) {
            aslLevel = ASL_LEVEL_INFO;
        } else {
            aslLevel = ASL_LEVEL_DEBUG;
        }
        
        snprintf(messageLogSpec, sizeof(messageLogSpec), "0x%x", msgLogSpec);
        asl_set(gASLMessage, "OSKextLogSpec", messageLogSpec);

        va_start(ap, format);
        asl_vlog(gASLClientHandle, gASLMessage, aslLevel, format, ap);
        va_end(ap);

    } else {
        // xxx - change to pick log stream based on log level
        // xxx - (0 == stdout, all others stderr)

        if (!g_log_stream) {
            g_log_stream = stderr;
        }

        va_start(ap, format);
        vfprintf(g_log_stream, format, ap);
        va_end(ap);

        fprintf(g_log_stream, "\n");
        fflush(g_log_stream);
    }

    return;
}

/*******************************************************************************
*******************************************************************************/
void log_CFError(
    OSKextRef     aKext __unused,
    OSKextLogSpec msgLogSpec,
    CFErrorRef    error)
{
    CFStringRef   errorString = NULL;  // do not release
    char        * cstring     = NULL;  // must release

    if (!error) {
        return;
    }
    errorString = CFErrorCopyDescription(error);
    if (errorString) {
        cstring = createUTF8CStringForCFString(errorString);
        OSKextLog(/* kext */ NULL, msgLogSpec,
            "CFError descripton: %s.", cstring);
        SAFE_FREE_NULL(cstring);
    }
    errorString = CFErrorCopyFailureReason(error);
    if (errorString) {
        cstring = createUTF8CStringForCFString(errorString);
        OSKextLog(/* kext */ NULL, msgLogSpec,
            "CFError reason: %s.", cstring);
        SAFE_FREE_NULL(cstring);
    }
    return;
}

/*******************************************************************************
* safe_mach_error_string()
*******************************************************************************/
const char * safe_mach_error_string(mach_error_t error_code)
{
    const char * result = mach_error_string(error_code);
    if (!result) {
        result = "(unknown)";
    }
    return result;
}

#if PRAGMA_MARK
#pragma mark User Input
#endif /* PRAGMA_MARK */
/*******************************************************************************
* user_approve()
*
* Ask the user a question and wait for a yes/no answer.
*******************************************************************************/
int user_approve(int default_answer, const char * format, ...)
{
    int result = 1;
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;
    char * prompt_string = NULL;
    int c, x;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        result = -1;
        goto finish;
    }

    va_start(ap, format);
    vsnprintf(output_string, output_length + 1, format, ap);
    va_end(ap);

    prompt_string = default_answer ? " [Y/n]" : " [y/N]";
    
    while ( 1 ) {
        fprintf(stderr, "%s%s%s", output_string, prompt_string, "? ");
        fflush(stderr);

        c = fgetc(stdin);

        if (c == EOF) {
            result = -1;
            goto finish;
        }

       /* Make sure we get a newline.
        */
        if ( c != '\n' ) {
            do {
                x = fgetc(stdin);
            } while (x != '\n' && x != EOF);

            if (x == EOF) {
                result = -1;
                goto finish;
            }
        }

        if (c == '\n') {
            result = default_answer ? 1 : 0;
            goto finish;
        } else if (tolower(c) == 'y') {
            result = 1;
            goto finish;
        } else if (tolower(c) == 'n') {
            result = 0;
            goto finish;
        } else {
            fprintf(stderr, "Please answer 'y' or 'n'.\n");
        }
    }

finish:
    if (output_string) free(output_string);

    return result;
}

/*******************************************************************************
* user_input()
*
* Ask the user for input.
*******************************************************************************/
const char * user_input(Boolean * eof, const char * format, ...)
{
    char * result = NULL;  // return value
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string = NULL;
    unsigned index;
    size_t size = 80;  // more than enough to input a hex address
    int c;

    if (eof) {
        *eof = false;
    }

    result = (char *)malloc(size);
    if (!result) {
        goto finish;
    }
    index = 0;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        result = NULL;
        goto finish;
    }

    va_start(ap, format);
    vsnprintf(output_string, output_length + 1, format, ap);
    va_end(ap);

    fprintf(stderr, "%s ", output_string);
    fflush(stderr);

    c = fgetc(stdin);
    while (c != '\n' && c != EOF) {
        if (index >= (size - 1)) {
            fprintf(stderr, "input line too long\n");
            if (result) free(result);
            result = NULL;
            goto finish;
        }
        result[index++] = (char)c;
        c = fgetc(stdin);
    }

    result[index] = '\0';

    if (c == EOF) {
        if (result) free(result);
        result = NULL;
        if (eof) {
            *eof = true;
        }
        goto finish;
    }

finish:
    if (output_string) free(output_string);

    return result;
}

#if PRAGMA_MARK
#pragma mark Caches
#endif /* PRAGMA_MARK */
/*******************************************************************************
*******************************************************************************/
OSReturn writePersonalitiesCache(
    CFArrayRef         kexts,
    const NXArchInfo * arch,
    CFURLRef           folderURL)
{
    OSReturn      result                  = kOSReturnError;
    char          folderPath[PATH_MAX];
    CFArrayRef    personalities           = NULL;  // must release

   /* Get the C string path for the folder.
    */
    if (!CFURLGetFileSystemRepresentation(folderURL,
        /* resolveToBase */ true, (UInt8 *)folderPath, sizeof(folderPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }
    
    if (!OSKextSetArchitecture(arch)) {
        goto finish;
    }

    personalities = OSKextCopyPersonalitiesOfKexts(kexts);
    if (!personalities) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't get personalities for kexts in %s.",
            folderPath);
        goto finish;
    }

    if (!_OSKextWriteCache(personalities,
        folderURL,
        CFSTR(kIOKitPersonalitiesKey),
        arch,
        _kOSKextCacheFormatIOXML)) {

        goto finish;
    }

    result = kOSReturnSuccess;

finish:
    if (result == kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag |
            kOSKextLogFileAccessFlag,
            "Saved %s personalities cache for %s", arch->name, folderPath);
    }

    SAFE_RELEASE(personalities);

    return result;
}

/*******************************************************************************
*******************************************************************************/
Boolean readKextPropertyValuesForDirectory(
    CFURLRef           directoryURL,
    CFStringRef        propertyKey,
    const NXArchInfo * arch,
    Boolean            forceUpdateFlag,
    CFArrayRef       * valuesOut)
{
    Boolean                result         = false;
    CFMutableArrayRef      values         = NULL;  // must release
    CFStringRef            cacheBasename  = NULL;  // must release
    CFArrayRef             kexts          = NULL;  // must release
    CFMutableDictionaryRef newDict        = NULL;  // must release
    CFStringRef            kextPath       = NULL;  // must release
    CFTypeRef              value          = NULL;  // do not release
    CFStringRef            kextVersion    = NULL;  // do not release
    CFIndex                count, i;

    cacheBasename = CFStringCreateWithFormat(kCFAllocatorDefault,
        /* formatOptions */ NULL, CFSTR("%s%@"),
        _kKextPropertyValuesCacheBasename,
        propertyKey);
    if (!cacheBasename) {
        OSKextLogMemError();
        goto finish;
    }

    if (OSKextGetUsesCaches() && !forceUpdateFlag) {

       /* See if we have an up-to-date cache containing an array, and return
        * that if we have one.
        */
        if (_OSKextReadCache(directoryURL, cacheBasename,
            arch, _kOSKextCacheFormatCFXML, /* parseXML? */ true,
            (CFPropertyListRef *)&values)) {

            if (values && CFGetTypeID(values) == CFArrayGetTypeID()) {
                result = true;
                goto finish;
            }
        }
    }

    if (!OSKextSetArchitecture(arch)) {
        goto finish;
    }

    values = CFArrayCreateMutable(kCFAllocatorDefault, /* capacity */ 0,
        &kCFTypeArrayCallBacks);
    if (!values) {
        OSKextLogMemError();
        goto finish;
    }

    kexts = OSKextCreateKextsFromURL(kCFAllocatorDefault, directoryURL);
    if (!kexts) {
        // Create function should log error
        goto finish;
    }

    count = CFArrayGetCount(kexts);

    for (i = 0; i < count; i++) {
        OSKextRef aKext = (OSKextRef)CFArrayGetValueAtIndex(kexts, i);

        SAFE_RELEASE_NULL(newDict);
        SAFE_RELEASE_NULL(kextPath);
        // do not release kextVersion
        kextVersion = NULL;

        if ((OSKextGetSimulatedSafeBoot() || OSKextGetActualSafeBoot()) &&
            !OSKextIsLoadableInSafeBoot(aKext)) {

            continue;
        }
        //??? if (OSKextGetLoadFailed(aKext)) continue;  -- don't have in OSKext

        value = OSKextGetValueForInfoDictionaryKey(aKext, propertyKey);
        if (!value) {
            continue;
        }

        newDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!newDict) {
            goto finish;
        }

        CFDictionarySetValue(newDict, CFSTR("Data"), value);

        CFDictionarySetValue(newDict, CFSTR("CFBundleIdentifier"),
            OSKextGetIdentifier(aKext));

        kextPath = copyKextPath(aKext);
        if (!kextPath) {
            goto finish;
        }
        CFDictionarySetValue(newDict, CFSTR("OSBundlePath"), kextPath);

        kextVersion = OSKextGetValueForInfoDictionaryKey(aKext,
            CFSTR("CFBundleVersion"));
        if (!kextVersion) {
            goto finish;
        }
        CFDictionarySetValue(newDict, CFSTR("CFBundleVersion"),
            kextVersion);

        CFArrayAppendValue(values, newDict);
    }

    if (OSKextGetUsesCaches() || forceUpdateFlag) {
        _OSKextWriteCache(values, directoryURL, cacheBasename,
            arch, _kOSKextCacheFormatCFXML);
    }

    result = true;

finish:
    if (result && valuesOut && values) {
        *valuesOut = (CFArrayRef)CFRetain(values);
    }

    SAFE_RELEASE(values);
    SAFE_RELEASE(cacheBasename);
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(newDict);
    SAFE_RELEASE(kextPath);

    return result;
}

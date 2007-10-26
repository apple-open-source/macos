/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CFBundlePriv.h>

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/kext/fat_util.h>
#include <IOKit/kext/macho_util.h>

#include "kextfind_commands.h"
#include "kextfind.h"
#include "kextfind_query.h"

/*******************************************************************************
*
*******************************************************************************/
char * getKextPath(
    KXKextRef theKext,
    PathSpec pathSpec)
{
    char * result = NULL;

    CFStringRef copiedPath = NULL;       // must release
    CFStringRef kextPath = NULL;         // do NOT release

    if (pathSpec == kPathsNone) {
        kextPath = KXKextGetBundleDirectoryName(theKext);
        if (!kextPath) {
            goto finish;
        }
        result = cStringForCFString(kextPath);
    } else if (pathSpec == kPathsRelative) {
        kextPath = KXKextGetBundlePathInRepository(theKext);
        if (!kextPath) {
            goto finish;
        }
        result = cStringForCFString(kextPath);
    } else {
        copiedPath = KXKextCopyAbsolutePath(theKext);
        if (!copiedPath) {
            goto finish;
        }
        result = cStringForCFString(copiedPath);
    }

finish:
    if (copiedPath) CFRelease(copiedPath);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void printKext(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd)
{
    CFBundleRef bundle = NULL;        // do NOT release
    CFStringRef bundleID = NULL;      // do NOT release
    CFStringRef bundleVersion = NULL; // do NOT release

    char * kext_path = NULL;          // must free
    char * bundle_id = NULL;          // must free
    char * bundle_version = NULL;     // must free

    kext_path = getKextPath(theKext, pathSpec);

    if (!kext_path) {
        goto finish;
    }

    if (extra_info) {
        bundle = KXKextGetBundle(theKext);
        bundleID = KXKextGetBundleIdentifier(theKext);
        bundleVersion = CFBundleGetValueForInfoDictionaryKey(bundle,
            kCFBundleVersionKey);

        bundle_id = cStringForCFString(bundleID);
        bundle_version = cStringForCFString(bundleVersion);
        if (!bundle_id || !bundle_version) {
            goto finish;
        }

        fprintf(stdout, "%s\t%s\t%s%c", kext_path, bundle_id,
            bundle_version, lineEnd);
    } else {
        fprintf(stdout, "%s%c", kext_path, lineEnd);
    }

finish:
    if (kext_path)      free(kext_path);
    if (bundle_id)      free(bundle_id);
    if (bundle_version) free(bundle_version);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
#define kMaxPrintableCFDataLength    (36)
#define kByteGroupSize                (4)

char * stringForData(const UInt8 * data, int numBytes)
{
    char * result = NULL;
    char * scan = NULL;
    int numByteGroups;
    int i;
    char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

   /* Put a space between every 2 bytes (4 chars).
    */
    numByteGroups = numBytes / kByteGroupSize;

    result = malloc(((numBytes * 2) + numByteGroups + 1) * sizeof(char));
    if (!result) {
        goto finish;
    }

    scan = result;

    for (i = 0; i < numBytes; i++) {
        int binaryDigit1 = (data[i] & 0xF0) >> 4;
        int binaryDigit2 = data[i] & 0xF;
        if (i > 0 && i % kByteGroupSize == 0) {
            *scan++ = ' ';
        }
        *scan++ = digits[binaryDigit1];
        *scan++ = digits[binaryDigit2];
    }
    *scan++ = '\0';

finish:
    return result;
}

void printProperty(
    CFStringRef label,
    CFStringRef propKey,
    CFTypeRef value,
    char lineEnd)
{
    CFTypeID type = CFGetTypeID(value);
    CFStringRef propString = NULL;     // must release
    CFStringRef labeledString = NULL;  // must release
    CFStringRef outputString = NULL;   // do not release
    char * allocString = NULL;         // must free
    char * dataString = NULL;  // must free

    if (type == CFStringGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = \"%@\"%c"),
            propKey, value, lineEnd);
    } else if (type == CFNumberGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = %@%c"),
            propKey, value, lineEnd);
    } else if (type == CFBooleanGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = %@%c"),
            propKey, value, lineEnd);
    } else if (type == CFDataGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = %@%c"),
            propKey, value, lineEnd);
        CFIndex length = 0;
        length = CFDataGetLength(value);
        const UInt8 * data = CFDataGetBytePtr(value);
        if (!data) {
            propString = CFStringCreateWithFormat(
                kCFAllocatorDefault, NULL, CFSTR("%@ = <null data pointer>%c"),
                propKey, length, lineEnd);
        } else {
            int numBytes = MIN(length, kMaxPrintableCFDataLength);
            dataString = stringForData(data, MIN(numBytes, kMaxPrintableCFDataLength));
            if (length > kMaxPrintableCFDataLength) {
                propString = CFStringCreateWithFormat(
                    kCFAllocatorDefault, NULL,
                    CFSTR("%@ = <data (%d bytes): %s...>%c"),
                    propKey, length, dataString, lineEnd);
            } else {
                propString = CFStringCreateWithFormat(
                    kCFAllocatorDefault, NULL,
                    CFSTR("%@ = <data (%d bytes): %s>%c"),
                    propKey, length, dataString, lineEnd);
            }
        }
    } else if (type == CFDictionaryGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = <dictionary of %d items>%c"),
            propKey, CFDictionaryGetCount(value), lineEnd);
    } else if (type == CFArrayGetTypeID()) {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = <array of %d items>%c"),
            propKey, CFArrayGetCount(value), lineEnd);
    } else {
        propString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@ = <value of unknown type>%c"),
            propKey, value, lineEnd);
    }

    if (!propString) {
        goto finish;
    }

    if (label) {
        labeledString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@: %@"), label, propString);
        outputString = labeledString;
    } else {
        labeledString = CFStringCreateWithFormat(
            kCFAllocatorDefault, NULL, CFSTR("%@"), propString);
        outputString = labeledString;
    }

    if (!outputString) {
        goto finish;
    }

    allocString = cStringForCFString(outputString);
    if (!allocString) {
        goto finish;
    }

    fprintf(stdout, "%s", allocString);

finish:
    if (propString)     CFRelease(propString);
    if (labeledString)  CFRelease(labeledString);
    if (allocString)    free(allocString);
    if (dataString)     free(dataString);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printKextProperty(
    KXKextRef theKext,
    CFStringRef propKey,
    char lineEnd)
{
    CFDictionaryRef infoDict = KXKextGetInfoDictionary(theKext);
    CFTypeRef       value = NULL;

    if (!infoDict) {
        goto finish;
    }

    value = CFDictionaryGetValue(infoDict, propKey);
    if (value) {
        printProperty(NULL, propKey, value, lineEnd);
    }

finish:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printKextMatchProperty(
    KXKextRef theKext,
    CFStringRef propKey,
    char lineEnd)
{
    CFDictionaryRef personalitiesDict = NULL;
    CFStringRef * names = NULL;
    CFDictionaryRef * personalities = NULL;
    CFIndex numPersonalities;
    CFIndex i;

    personalitiesDict = KXKextCopyPersonalities(theKext);
    if (!personalitiesDict) {
        goto finish;
    }

    numPersonalities = CFDictionaryGetCount(personalitiesDict);
    if (!numPersonalities) {
        goto finish;
    }

    names = malloc(numPersonalities * sizeof(CFStringRef));
    personalities = malloc(numPersonalities * sizeof(CFDictionaryRef));
    if (!names || !personalities) {
        goto finish;
    }

    CFDictionaryGetKeysAndValues(personalitiesDict, (const void **)names,
        (const void **)personalities);

    for (i = 0; i < numPersonalities; i++) {
        CFTypeRef value = CFDictionaryGetValue(personalities[i], propKey);
        if (value) {
            printProperty(names[i], propKey, value, lineEnd);
        }
    }

finish:
    if (personalitiesDict) CFRelease(personalitiesDict);
    if (names)             free(names);
    if (personalities)     free(personalities);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
extern fat_iterator _KXKextCopyFatIterator(KXKextRef aKext);

void printKextArches(
    KXKextRef theKext,
    char lineEnd,
    Boolean printLineEnd)
{
    fat_iterator fiter = NULL;
    struct mach_header * farch = NULL;
    const NXArchInfo * archinfo = NULL;
    Boolean printedOne = false;

    fiter = _KXKextCopyFatIterator(theKext);
    if (!fiter) {
        goto finish;
    }

    while ((farch = fat_iterator_next_arch(fiter, NULL))) {
        int swap = ISSWAPPEDMACHO(farch->magic);
        archinfo = NXGetArchInfoFromCpuType(CondSwapInt32(swap, farch->cputype),
            CondSwapInt32(swap, farch->cpusubtype));
        if (archinfo) {
            fprintf(stdout, "%s%s", printedOne ? "," : "", archinfo->name);
            printedOne = true;
        }
    }

finish:
    if (printLineEnd && printedOne) {
        fprintf(stdout, "%c", lineEnd);
    }
    if (fiter)  fat_iterator_close(fiter);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printKextDependencies(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd)
{
    CFArrayRef kextDependencies = KXKextCopyAllDependencies(theKext);
    CFIndex count, i;

    if (!kextDependencies) {
        goto finish;
    }

    count = CFArrayGetCount(kextDependencies);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(kextDependencies, i);
        printKext(thisKext, pathSpec, extra_info, lineEnd);
    }

finish:
    if (kextDependencies) CFRelease(kextDependencies);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printKextDependents(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd)
{
    CFArrayRef kextDependents = KXKextCopyAllDependents(theKext);
    CFIndex count, i;

    if (!kextDependents) {
        goto finish;
    }

    count = CFArrayGetCount(kextDependents);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(kextDependents, i);
        printKext(thisKext, pathSpec, extra_info, lineEnd);
    }

finish:
    if (kextDependents) CFRelease(kextDependents);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void printKextPlugins(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd)
{
    CFArrayRef plugins = KXKextGetPlugins(theKext);  // do NOT release
    CFIndex count, i;

    if (!plugins) {
        goto finish;
    }

    count = CFArrayGetCount(plugins);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(plugins, i);
        printKext(thisKext, pathSpec, extra_info, lineEnd);
    }

finish:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
const char * nameForIntegrityState(KXKextIntegrityState state)
{
    if (kKXKextIntegrityCorrect == state) {
        return kIntegrityCorrect;
    } else if (kKXKextIntegrityKextIsModified == state) {
        return kIntegrityModified;
    } else if (kKXKextIntegrityNoReceipt == state) {
        return kIntegrityNoReceipt;
    } else if (kKXKextIntegrityNotApple == state) {
        return kIntegrityNotApple;
    } else if (kKXKextIntegrityUnknown == state) {
        return kIntegrityUnknown;
    }
    return NULL;
}


/*******************************************************************************
* XXX: I'm really not sure this is completely reliable for getting a relative
* XXX: path.
*******************************************************************************/
char * getAdjustedPath(
    KXKextRef theKext,
    CFURLRef pathToAdjust,
    PathSpec pathSpec)
{
    char * result = NULL;
    KXKextRepositoryRef repository = NULL;   // do not release
    CFURLRef repositoryURL = NULL;  // must release
    CFURLRef kextAbsURL = NULL;     // do not release
    char infoDictPath[PATH_MAX];
    char containingPath[PATH_MAX];

    if (!CFURLGetFileSystemRepresentation(pathToAdjust, true,
        (UInt8 *)infoDictPath, PATH_MAX)) {

        goto finish;
    }

    if (pathSpec == kPathsFull) {
        result = strdup(infoDictPath);
        goto finish;
    }

    if (pathSpec == kPathsRelative) {
        repository = KXKextGetRepository(theKext);
        repositoryURL = KXKextRepositoryCopyURL(repository);
        if (!repositoryURL) {
            goto finish;
        }

        if (!CFURLGetFileSystemRepresentation(repositoryURL, true,
            (UInt8 *)containingPath, PATH_MAX)) {

            goto finish;
        }

        result = strdup(infoDictPath + strlen(containingPath) + 1);
    }

   /* No-paths for info-dict & executable are still relative to the kext.
    */
    if (pathSpec == kPathsNone) {
        kextAbsURL = KXKextGetAbsoluteURL(theKext);
        if (!kextAbsURL) {
            goto finish;
        }
        if (!CFURLGetFileSystemRepresentation(kextAbsURL, true,
            (UInt8 *)containingPath, PATH_MAX)) {

            goto finish;
        }

        result = strdup(infoDictPath + strlen(containingPath)  + 1);
    }

finish:
    if (repositoryURL) CFRelease(repositoryURL);
    return result;
}

/*******************************************************************************
* XXX: I'm really not sure this is completely reliable for getting a relative
* XXX: path.
*******************************************************************************/
char * getKextInfoDictionaryPath(
    KXKextRef theKext,
    PathSpec pathSpec)
{
    char * result = NULL;
    CFURLRef infoDictURL = NULL;    // must release

    infoDictURL = _CFBundleCopyInfoPlistURL(KXKextGetBundle(theKext));
    if (!infoDictURL) {
        goto finish;
    }

    result = getAdjustedPath(theKext, infoDictURL, pathSpec);

finish:
    if (infoDictURL)   CFRelease(infoDictURL);
    return result;
}

void printKextInfoDictionary(
    KXKextRef theKext,
    PathSpec pathSpec,
    char lineEnd)
{
    char * infoDictPath = NULL;  // must free

    infoDictPath = getKextInfoDictionaryPath(theKext, pathSpec);
    if (infoDictPath) {
        printf("%s%c", infoDictPath, lineEnd);
        free(infoDictPath);
    }

    return;
}

/*******************************************************************************
* XXX: I'm really not sure this is completely reliable for getting a relative
* XXX: path.
*******************************************************************************/
char * getKextExecutablePath(
    KXKextRef theKext,
    PathSpec pathSpec)
{
    char * result = NULL;
    CFURLRef executableURL = NULL;    // must release

    executableURL = CFBundleCopyExecutableURL(KXKextGetBundle(theKext));
    if (!executableURL) {
        goto finish;
    }
    result = getAdjustedPath(theKext, executableURL, pathSpec);

finish:
    if (executableURL)   CFRelease(executableURL);
    return result;
}

/*******************************************************************************
* XXX: I'm really not sure this is completely reliable for getting a relative
* XXX: path.
*******************************************************************************/
void printKextExecutable(
    KXKextRef theKext,
    PathSpec pathSpec,
    char lineEnd)
{
    char * executablePath = getKextExecutablePath(theKext, pathSpec);

    if (executablePath) {
        printf("%s%c", executablePath, lineEnd);
        free(executablePath);
    }

    return;
}

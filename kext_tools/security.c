/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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
#include <asl.h>
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/IOKitLib.h>

#include "security.h"
#include "kext_tools_util.h"


/*******************************************************************************
 * logMTMessage() - log MessageTracer message
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/
#define __kOSKextApplePrefix             CFSTR("com.apple.")

void logMTMessage(OSKextRef aKext)
{
    CFStringRef     versionString;
    CFStringRef     bundleIDString;
    CFURLRef        kextURL             = NULL;   // must release
    CFStringRef     kextPath            = NULL;   // must release
    CFStringRef     filename            = NULL;   // must release
    aslmsg          amsg                = NULL;   // must free
    char            *versionCString     = NULL;   // must free
    char            *bundleIDCString    = NULL;   // must free
    char            *filenameCString    = NULL;   // must free
    char            *tempBufPtr         = NULL;   // must free
    CFMutableDictionaryRef signdict     = NULL;   // must release
    SecCodeSignerRef    signerRef       = NULL;   // must release
    SecStaticCodeRef    staticCodeRef   = NULL;   // must release
    CFDataRef           signature       = NULL;   // must release
    CFDictionaryRef     signingDict     = NULL;   // must release
    CFDataRef           cdhash          = NULL;   // must release
    CFMutableDictionaryRef resourceRules = NULL;  // must release
    CFMutableDictionaryRef rules        = NULL;   // must release
    CFMutableDictionaryRef omitPlugins  = NULL;   // must release
    
    /* do not message trace this if boot-args has debug set */
    if (isDebugSetInBootargs()) {
        return;
    }

    if (!OSKextGetURL(aKext)) {
        OSKextLogMemError();
        goto finish;
    }
    kextURL = CFURLCopyAbsoluteURL(OSKextGetURL(aKext));
    if (!kextURL) {
        OSKextLogMemError();
        goto finish;
    }
    kextPath = CFURLCopyFileSystemPath(kextURL, kCFURLPOSIXPathStyle);
    if (!kextPath) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* we want to exclude all apple kexts except for ones that are NOT in /S/L/E
     */
    bundleIDString = OSKextGetIdentifier(aKext);
    if (CFStringHasPrefix(bundleIDString, __kOSKextApplePrefix)) {
        char            scratchPath[PATH_MAX];
        
        if (CFStringGetCString(kextPath, scratchPath,
                               sizeof(scratchPath),
                               kCFStringEncodingUTF8) == 0) {
            OSKextLogMemError();
            goto finish;
        }
        
        /* bump past extra '/'s at the beginning of the path */
        char     *pathPtr = &scratchPath[0];
        for (size_t i = 0; i < strlen(&scratchPath[0]); i++) {
            if (scratchPath[i] == '/' && scratchPath[i + 1] == '/') {
                pathPtr++;
                continue;
            }
            break;
        }
        if (strncmp(pathPtr,
                    _kOSKextSystemLibraryExtensionsFolder,
                    strlen(_kOSKextSystemLibraryExtensionsFolder)) == 0) {
            goto finish;
        }
    }
    
#if 0
    OSKextLogCFString(/* kext */ NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      CFSTR("%s - logging kext %@"),
                      __func__,
                      aKext);
#endif
    
    versionString = OSKextGetValueForInfoDictionaryKey(aKext,
                                                       kCFBundleVersionKey);
    if (versionString) {
        versionCString = createUTF8CStringForCFString(versionString);
    }
    bundleIDString = OSKextGetValueForInfoDictionaryKey(aKext,
                                                        kCFBundleIdentifierKey);
    if (bundleIDString) {
        bundleIDCString = createUTF8CStringForCFString(bundleIDString);
    }
    
    filename = CFURLCopyLastPathComponent(kextURL);
    if (filename) {
        filenameCString = createUTF8CStringForCFString(filename);
    }
    SAFE_RELEASE(filename);
    
    /* Ad-hoc sign the code temporarily so we can get its hash */
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &staticCodeRef) != 0 ||
        (staticCodeRef == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    signature = CFDataCreateMutable(NULL, 0);
    if (signature == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    signdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    if (signdict == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    CFDictionarySetValue(signdict, kSecCodeSignerIdentity, kCFNull);
    CFDictionarySetValue(signdict, kSecCodeSignerDetached, signature);
    
    resourceRules = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    if (resourceRules == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    rules = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    if (rules == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    omitPlugins = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    if (omitPlugins == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* exclude PlugIns directory
     */
    CFNumberRef myNumValue;
    int myNum = 100;
    myNumValue = CFNumberCreate(kCFAllocatorDefault,
                                kCFNumberIntType, &myNum);
    if (myNumValue == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    CFDictionarySetValue(omitPlugins, CFSTR("omit"), kCFBooleanTrue);
    CFDictionarySetValue(omitPlugins, CFSTR("weight"), myNumValue);
    CFRelease( myNumValue );
    
    CFDictionarySetValue(rules, CFSTR("^.*"), kCFBooleanTrue);
    CFDictionarySetValue(rules, CFSTR("^PlugIns/"), omitPlugins);
    CFDictionarySetValue(resourceRules, CFSTR("rules"), rules);
    CFDictionarySetValue(signdict, kSecCodeSignerResourceRules, resourceRules);
    
    if (SecCodeSignerCreate(signdict, kSecCSDefaultFlags, &signerRef) != 0) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSignerCreate failed", __func__);
        goto finish;
    }
    if (SecCodeSignerAddSignature(signerRef, staticCodeRef, kSecCSDefaultFlags) != 0) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSignerAddSignature failed", __func__);
        goto finish;
    }
    if (SecCodeSetDetachedSignature(staticCodeRef, signature, kSecCSDefaultFlags) != 0) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSetDetachedSignature failed", __func__);
        goto finish;
    }
    
    /* get the hash info
     */
    if (SecCodeCopySigningInformation(staticCodeRef, kSecCSDefaultFlags, &signingDict) != 0) {
        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeCopySigningInformation failed", __func__);
        goto finish;
    }
    
    cdhash = CFRetain(CFDictionaryGetValue(signingDict, kSecCodeInfoUnique));
    if (cdhash) {
        const UInt8 *   hashDataPtr     = NULL;  // don't free
        CFIndex         hashDataLen     = 0;
        
        hashDataPtr = CFDataGetBytePtr(cdhash);
        hashDataLen = CFDataGetLength(cdhash);
        tempBufPtr = (char *) malloc((hashDataLen + 1) * 2);
        if (tempBufPtr == NULL) {
            OSKextLogMemError();
            goto finish;
        }
#if 0
        OSKextLogCFString(/* kext */ NULL,
                          kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                          CFSTR("%s - cdhash %@"),
                          __func__,
                          cdhash);
#endif
        
        bzero(tempBufPtr, ((hashDataLen + 1) * 2));
        for (int i = 0; i < hashDataLen; i++) {
            sprintf(tempBufPtr + (i * 2), "%02.2x", *(hashDataPtr + i));
        }
    }
    
    /* log the message tracer data
     */
    amsg = asl_new(ASL_TYPE_MSG);
    if (!amsg) {
        OSKextLogMemError();
        goto finish;
    }
    
    asl_set(amsg, kMessageTracerDomainKey, kMTKextLoadingDomain);
    asl_set(amsg, kMessageTracerBundleIDKey,
            bundleIDCString ? bundleIDCString : "");
    asl_set(amsg, kMessageTracerVersionKey,
            versionCString ? versionCString : "");
    asl_set(amsg, kMessageTracerKextNameKey,
            filenameCString ? filenameCString : "");
    asl_set(amsg, kMessageTracerHashKey,
            tempBufPtr ? tempBufPtr : "");
    
    asl_log(NULL, amsg, ASL_LEVEL_NOTICE, "");
    
finish:
    SAFE_FREE(versionCString);
    SAFE_FREE(bundleIDCString);
    SAFE_FREE(filenameCString);
    SAFE_FREE(tempBufPtr);
    
    SAFE_RELEASE(kextURL);
    SAFE_RELEASE(kextPath);
    SAFE_RELEASE(signdict);
    SAFE_RELEASE(signerRef);
    SAFE_RELEASE(staticCodeRef);
    SAFE_RELEASE(signature);
    SAFE_RELEASE(signingDict);
    SAFE_RELEASE(cdhash);
    SAFE_RELEASE(resourceRules);
    SAFE_RELEASE(rules);
    SAFE_RELEASE(omitPlugins);
    
    if (amsg) {
        asl_free(amsg);
    }
    return;
}


/*******************************************************************************
 * isDebugSetInBootargs() - check to see if boot-args has debug set.  We cache
 * the result since boot-args / debug will not change until reboot.
 *******************************************************************************/
Boolean isDebugSetInBootargs(void)
{
    static int          didOnce         = 0;
    static Boolean      result          = false;
    io_registry_entry_t optionsNode     = MACH_PORT_NULL;   // must release
    CFStringRef         bootargsEntry   = NULL;             // must release
    
    if (didOnce) {
        return(result);
    }
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault,
                                          "IODeviceTree:/options");
    if (optionsNode) {
        bootargsEntry = (CFStringRef)
        IORegistryEntryCreateCFProperty(optionsNode,
                                        CFSTR("boot-args"),
                                        kCFAllocatorDefault, 0);
        if (bootargsEntry) {
            CFRange     findRange;
            findRange = CFStringFind(bootargsEntry, CFSTR("debug"), 0);
            
            if (findRange.length != 0) {
                result = true;
            }
        }
    }
    didOnce++;
    if (optionsNode)  IOObjectRelease(optionsNode);
    SAFE_RELEASE(bootargsEntry);
    
    return(result);
}

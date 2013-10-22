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
#include <Security/Security.h>

#include "security.h"
#include "kext_tools_util.h"

/*******************************************************************************
 * Helper functions
 *******************************************************************************/
static OSStatus     checkRootCertificateIsApple(OSKextRef aKext);
static CFStringRef  copyCDHash(SecStaticCodeRef code);
static CFStringRef  copyIssuerCN(SecCertificateRef certificate);
static void         copySigningInfo(CFURLRef kextURL,
                                    CFStringRef* cdhash,
                                    CFStringRef* teamId,
                                    CFStringRef* subjectCN,
                                    CFStringRef* issuerCN);
static CFArrayRef   copySubjectCNArray(CFURLRef kextURL);
static CFStringRef  copyTeamID(SecCertificateRef certificate);
static CFStringRef  createArchitectureList(OSKextRef aKext, CFBooleanRef *isFat);
static void         createHashForMT(CFURLRef kextURL, char ** signatureBuffer);
static void         filterKextLoadForMT(OSKextRef aKext, CFMutableArrayRef *kextList);

/*******************************************************************************
 * messageTraceExcludedKext() - log MessageTracer message for kexts in 
 * exclude list.
 *  <rdar://problem/12994418> MessageTrace when we block the load of something 
 *                            on the kext exclude list
 *******************************************************************************/

void messageTraceExcludedKext(OSKextRef theKext)
{
    CFStringRef     versionString;
    CFStringRef     bundleIDString;
    CFURLRef        kextURL             = NULL;   // must release
    CFStringRef     filename            = NULL;   // must release
    aslmsg          amsg                = NULL;   // must free
    char            *versionCString     = NULL;   // must free
    char            *bundleIDCString    = NULL;   // must free
    char            *filenameCString    = NULL;   // must free

    kextURL = CFURLCopyAbsoluteURL(OSKextGetURL(theKext));
    if (!kextURL) {
        OSKextLogMemError();
        goto finish;
    }
    versionString = OSKextGetValueForInfoDictionaryKey(theKext,
                                                       kCFBundleVersionKey);
    if (versionString) {
        versionCString = createUTF8CStringForCFString(versionString);
    }
    bundleIDString = OSKextGetValueForInfoDictionaryKey(theKext,
                                                        kCFBundleIdentifierKey);
    if (bundleIDString) {
        bundleIDCString = createUTF8CStringForCFString(bundleIDString);
    }
    
    filename = CFURLCopyLastPathComponent(kextURL);
    if (filename) {
        filenameCString = createUTF8CStringForCFString(filename);
    }
    SAFE_RELEASE(filename);
    
    /* log the message tracer data
     */
    amsg = asl_new(ASL_TYPE_MSG);
    if (!amsg) {
        OSKextLogMemError();
        goto finish;
    }
    
    asl_set(amsg, kMessageTracerDomainKey, kMTKextBlockedDomain);
    asl_set(amsg, kMessageTracerBundleIDKey,
            bundleIDCString ? bundleIDCString : "");
    asl_set(amsg, kMessageTracerVersionKey,
            versionCString ? versionCString : "");
    asl_set(amsg, kMessageTracerKextNameKey,
            filenameCString ? filenameCString : "");
    
    asl_log(NULL, amsg, ASL_LEVEL_NOTICE, "");

finish:
    SAFE_FREE(versionCString);
    SAFE_FREE(bundleIDCString);
    SAFE_FREE(filenameCString);
    
    SAFE_RELEASE(kextURL);
    
    if (amsg) {
        asl_free(amsg);
    }
    return;
}

/*******************************************************************************
 * checkRootCertificateIsApple() - check if the root certificate of the kext
 *  is issued by Apple
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/
static OSStatus checkRootCertificateIsApple(OSKextRef aKext)
{
    OSStatus                result          = -1;
    CFURLRef                kextURL         = NULL;   // must release
    SecStaticCodeRef        staticCodeRef   = NULL;   // must release
    SecRequirementRef       requirementRef  = NULL;   // must release
    CFStringRef             myCFString;
    CFStringRef             requirementsString;
    
    if (aKext == NULL) {
        return result;
    }
    
    kextURL = CFURLCopyAbsoluteURL(OSKextGetURL(aKext));
    if (!kextURL) {
        OSKextLogMemError();
        goto finish;
    }
    
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &staticCodeRef) != errSecSuccess ||
        (staticCodeRef == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* set up correct requirement string */
    myCFString = OSKextGetIdentifier(aKext);
    if (CFStringHasPrefix(myCFString, __kOSKextApplePrefix)) {
        requirementsString = CFSTR("anchor apple");
    }
    else {
        requirementsString = CFSTR("anchor apple generic");
    }
    
    if (SecRequirementCreateWithString(requirementsString,
                                       kSecCSDefaultFlags,
                                       &requirementRef) != errSecSuccess ||
        (requirementRef == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    // errSecCSUnsigned == -67062
    result = SecStaticCodeCheckValidity(staticCodeRef,
                                        kSecCSDefaultFlags,
                                        requirementRef);
    if (result != 0) {
        OSKextLogCFString(NULL,
                          kOSKextLogErrorLevel | kOSKextLogLoadFlag,
                          CFSTR("Invalid signature %ld for kext %@"),
                          (long)result, aKext);
    }
    
finish:
    SAFE_RELEASE(kextURL);
    SAFE_RELEASE(staticCodeRef);
    SAFE_RELEASE(requirementRef);
    
    return result;
}

/*******************************************************************************
 * createHashForMT() - create a hash signature for the kext
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/

static void createHashForMT(CFURLRef kextURL, char ** signatureBuffer)
{
    CFMutableDictionaryRef signdict     = NULL;   // must release
    SecCodeSignerRef    signerRef       = NULL;   // must release
    SecStaticCodeRef    staticCodeRef   = NULL;   // must release
    CFDataRef           signature       = NULL;   // must release
    CFDictionaryRef     signingDict     = NULL;   // must release
    CFDataRef           cdhash          = NULL;   // must release
    CFMutableDictionaryRef resourceRules = NULL;  // must release
    CFMutableDictionaryRef rules        = NULL;   // must release
    CFMutableDictionaryRef omitPlugins  = NULL;   // must release
    char *              tempBufPtr      = NULL;   // do not free
    
    if (!kextURL || !signatureBuffer) {
        return;
    }
    
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
        OSKextLog(NULL, kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSignerCreate failed", __func__);
        goto finish;
    }
    if (SecCodeSignerAddSignature(signerRef, staticCodeRef, kSecCSDefaultFlags) != 0) {
        OSKextLog(NULL, kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSignerAddSignature failed", __func__);
        goto finish;
    }
    if (SecCodeSetDetachedSignature(staticCodeRef, signature, kSecCSDefaultFlags) != 0) {
        OSKextLog(NULL, kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                  "%s - SecCodeSetDetachedSignature failed", __func__);
        goto finish;
    }
    
    /* get the hash info
     */
    if (SecCodeCopySigningInformation(staticCodeRef, kSecCSDefaultFlags, &signingDict) != 0) {
        OSKextLog(NULL, kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
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
    
finish:
    SAFE_RELEASE(signdict);
    SAFE_RELEASE(signerRef);
    SAFE_RELEASE(staticCodeRef);
    SAFE_RELEASE(signature);
    SAFE_RELEASE(signingDict);
    SAFE_RELEASE(cdhash);
    SAFE_RELEASE(resourceRules);
    SAFE_RELEASE(rules);
    SAFE_RELEASE(omitPlugins);
    
    *signatureBuffer = tempBufPtr;
}

/*******************************************************************************
 * createArchitectureList() - create the list of architectures for the kext
 *  <rdar://13529984> MessageTrace which kexts are FAT
 *  Note: the caller must release the created CFStringRef
 *******************************************************************************/
static CFStringRef createArchitectureList(OSKextRef aKext, CFBooleanRef *isFat)
{
    if (aKext == NULL || isFat == NULL) {
        return NULL;
    }
    
    *isFat = kCFBooleanFalse;
    
    const NXArchInfo **         archList            = NULL; // must free
    CFMutableArrayRef           archNamesList       = NULL; // must release
    CFStringRef                 archNames           = NULL; // do not release
    const char *                archNameCString     = NULL; // do not free
    int                         index               = 0;
    
    archList = OSKextCopyArchitectures(aKext);
    if (!archList) {
        goto finish;
    }
    
    archNamesList = CFArrayCreateMutable(kCFAllocatorDefault,
                                         0,
                                         &kCFTypeArrayCallBacks);
    if (!archNamesList) {
        goto finish;
    }
    
    for (index=0; archList[index]; index++) {
        archNameCString = archList[index]->name;
        if (archNameCString) {
            CFStringRef archName = NULL;
            archName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                 archNameCString,
                                                 kCFStringEncodingUTF8);
            if (archName) {
                CFArrayAppendValue(archNamesList, archName);
                SAFE_RELEASE_NULL(archName);
            }
        }
    }
    
    if (index>1) {
        *isFat = kCFBooleanTrue;
    }
    archNames = CFStringCreateByCombiningStrings(kCFAllocatorDefault,
                                                 archNamesList,
                                                 CFSTR(" "));
    if (!archNames) {
        goto finish;
    }
    
finish:
    SAFE_RELEASE(archNamesList);
    SAFE_FREE(archList);
    
    return archNames;
}

/*******************************************************************************
 * copyTeamID() - copy the team id field from the given certificate
 *  Note: the caller must release the created CFStringRef
 *******************************************************************************/
static CFStringRef copyTeamID(SecCertificateRef certificate)
{
    if (!certificate ||
        CFGetTypeID(certificate) !=SecCertificateGetTypeID()) {
        return NULL;
    }
    
    CFDictionaryRef     subjectDict     = NULL; // do not release
    CFArrayRef          subjectArray    = NULL; // do not release
    CFDictionaryRef     subjectInfo     = NULL; // do not release
    CFStringRef         teamID          = NULL; // do not release
    CFErrorRef          error           = NULL; // do not release
    
    CFMutableArrayRef   certificateKeys = NULL; // must release
    CFDictionaryRef     certificateDict = NULL; // must release
    
    certificateKeys = CFArrayCreateMutable(kCFAllocatorDefault,
                                           1,
                                           &kCFTypeArrayCallBacks);
    if (!certificateKeys) {
        goto finish;
    }
    
    CFArrayAppendValue(certificateKeys, kSecOIDX509V1SubjectName);
    
    certificateDict = SecCertificateCopyValues(certificate,
                                               certificateKeys,
                                               &error);
    if (error != errSecSuccess ||
        !certificateDict ||
        CFGetTypeID(certificateDict) != CFDictionaryGetTypeID()) {
        goto finish;
    }
    
    subjectDict = (CFDictionaryRef) CFDictionaryGetValue(certificateDict,
                                                         kSecOIDX509V1SubjectName);
    if (!subjectDict ||
        CFGetTypeID(subjectDict) != CFDictionaryGetTypeID()) {
        goto finish;
    }
    
    subjectArray = (CFArrayRef) CFDictionaryGetValue(subjectDict,
                                                     kSecPropertyKeyValue);
    if (!subjectArray ||
        CFGetTypeID(subjectArray) != CFArrayGetTypeID()) {
        goto finish;
    }
    
    // Try to look for UserID field ("0.9.2342.19200300.100.1.1")
    for (int index=0; index<CFArrayGetCount(subjectArray); index++) {
        subjectInfo = (CFDictionaryRef) CFArrayGetValueAtIndex(subjectArray,
                                                               index);
        if (!subjectInfo ||
            CFGetTypeID(subjectInfo) != CFDictionaryGetTypeID()) {
            continue;
        }
        
        CFStringRef label = NULL; // do not release
        label = CFDictionaryGetValue(subjectInfo,
                                     kSecPropertyKeyLabel);
        if (kCFCompareEqualTo == CFStringCompare(label,
                                                 CFSTR("0.9.2342.19200300.100.1.1"),
                                                 0)) {
            teamID = CFDictionaryGetValue(subjectInfo,
                                          kSecPropertyKeyValue);
            if (teamID &&
                CFGetTypeID(teamID) == CFStringGetTypeID()) {
                CFRetain(teamID);
                goto finish;
            }
            else {
                teamID = NULL;
            }
        }
    }
    
    // In case of failure, look for OU field ("2.5.4.11")
    if (!teamID) {
        for (int index=0; index<CFArrayGetCount(subjectArray); index++) {
            subjectInfo = (CFDictionaryRef) CFArrayGetValueAtIndex(subjectArray,
                                                                   index);
            if (!subjectInfo ||
                CFGetTypeID(subjectInfo) != CFDictionaryGetTypeID()) {
                continue;
            }
            
            CFStringRef label = NULL; // do not release
            label = CFDictionaryGetValue(subjectInfo,
                                         kSecPropertyKeyLabel);
            if (kCFCompareEqualTo == CFStringCompare(label,
                                                     CFSTR("2.5.4.11"),
                                                     0)) {
                teamID = CFDictionaryGetValue(subjectInfo,
                                              kSecPropertyKeyValue);
                if (teamID &&
                    CFGetTypeID(teamID) == CFStringGetTypeID()) {
                    CFRetain(teamID);
                    goto finish;
                }
                else {
                    teamID = NULL;
                }
            }
        }
    }
    
finish:
    if (!teamID && subjectArray) {
        CFShow(subjectArray);
    }
    SAFE_RELEASE(certificateKeys);
    SAFE_RELEASE(certificateDict);
    return teamID;
}

/*******************************************************************************
 * copyIssuerCN() - copy the issuer CN field from the given certificate
 *  Note: the caller must release the created CFStringRef
 *******************************************************************************/
static CFStringRef copyIssuerCN(SecCertificateRef certificate)
{
    if (!certificate ||
        CFGetTypeID(certificate) !=SecCertificateGetTypeID()) {
        return NULL;
    }
    
    CFStringRef         issuerCN        = NULL; // do not release
    CFDictionaryRef     issuerDict      = NULL; // do not release
    CFArrayRef          issuerArray     = NULL; // do not release
    CFDictionaryRef     issuerInfo      = NULL; // do not release
    CFErrorRef          error           = NULL; // do not release
    
    CFMutableArrayRef   certificateKeys = NULL; // must release
    CFDictionaryRef     certificateDict = NULL; // must release
    
    certificateKeys = CFArrayCreateMutable(kCFAllocatorDefault,
                                           1,
                                           &kCFTypeArrayCallBacks);
    if (!certificateKeys) {
        goto finish;
    }
    
    CFArrayAppendValue(certificateKeys, kSecOIDX509V1IssuerName);
    
    certificateDict = SecCertificateCopyValues(certificate,
                                               certificateKeys,
                                               &error);
    
    if (error != errSecSuccess ||
        !certificateDict ||
        CFGetTypeID(certificateDict) != CFDictionaryGetTypeID()) {
        goto finish;
    }
    
    issuerDict = (CFDictionaryRef) CFDictionaryGetValue(certificateDict,
                                                        kSecOIDX509V1IssuerName);
    if (!issuerDict ||
        CFGetTypeID(issuerDict) != CFDictionaryGetTypeID()) {
        goto finish;
    }
    
    issuerArray = (CFArrayRef) CFDictionaryGetValue(issuerDict,
                                                    kSecPropertyKeyValue);
    if (!issuerArray ||
        CFGetTypeID(issuerArray) != CFArrayGetTypeID()) {
        goto finish;
    }
    
    for (int index=0; index<CFArrayGetCount(issuerArray); index++) {
        issuerInfo = (CFDictionaryRef) CFArrayGetValueAtIndex(issuerArray,
                                                              index);
        if (!issuerInfo ||
            CFGetTypeID(issuerInfo) != CFDictionaryGetTypeID()) {
            continue;
        }
        
        CFStringRef label = NULL; // do not release
        label = CFDictionaryGetValue(issuerInfo,
                                     kSecPropertyKeyLabel);
        if (kCFCompareEqualTo == CFStringCompare(label,
                                                 CFSTR("2.5.4.3"),
                                                 0)) {
            issuerCN = CFDictionaryGetValue(issuerInfo,
                                            kSecPropertyKeyValue);
            if (issuerCN &&
                CFGetTypeID(issuerCN) == CFStringGetTypeID()) {
                CFRetain(issuerCN);
                goto finish;
            }
            else {
                issuerCN = NULL;
            }
        }
    }
    
finish:
    SAFE_RELEASE(certificateDict);
    SAFE_RELEASE(certificateKeys);
    return issuerCN;
}

/*******************************************************************************
 * copyCDHash() - copy the SHA-1 hash of the code
 *  Note: the caller must release the created CFStringRef
 *******************************************************************************/
static CFStringRef copyCDHash(SecStaticCodeRef code)
{
    CFDictionaryRef signingInfo     = NULL; // must release
    char *          tempBufPtr      = NULL; // free
    const UInt8 *   hashDataPtr     = NULL; // do not free
    CFDataRef       cdhash          = NULL; // do not release
    CFStringRef     hash            = NULL; // do not release
    CFIndex         hashDataLen     = 0;
    
    SecCodeCopySigningInformation(code,
                                  kSecCSDefaultFlags,
                                  &signingInfo);
    if (!signingInfo) {
        goto finish;
    }
    
    cdhash = CFDictionaryGetValue(signingInfo, kSecCodeInfoUnique);
    if (!cdhash ||
        CFGetTypeID(cdhash) != CFDataGetTypeID()) {
        goto finish;
    }
    
    hashDataPtr = CFDataGetBytePtr(cdhash);
    hashDataLen = CFDataGetLength(cdhash);
    tempBufPtr = (char *) malloc((hashDataLen + 1) * 2);
    if (tempBufPtr == NULL) {
        OSKextLogMemError();
        goto finish;
    }
    
    bzero(tempBufPtr, ((hashDataLen + 1) * 2));
    for (int i = 0; i < hashDataLen; i++) {
        sprintf(tempBufPtr + (i * 2), "%02.2x", *(hashDataPtr + i));
    }
    
    if (tempBufPtr) {
        hash = CFStringCreateWithCString(kCFAllocatorDefault,
                                         tempBufPtr,
                                         kCFStringEncodingUTF8);
    }
    
finish:
    SAFE_FREE(tempBufPtr);
    SAFE_RELEASE(signingInfo);
    return hash;
}

/*******************************************************************************
 * copySigningInfo() - copy a set of signing information for the given kext
 *  cdhash:     the SHA-1 hash
 *  teamid:     the team id of the leaf certificate
 *  subjectCN:  the subject common name of the leaf certificate
 *  issuerCN:   the issuer common name of the leaf certificate
 *  Note: the caller must release the created CFStringRefs
 *******************************************************************************/
static void copySigningInfo(CFURLRef kextURL,
                            CFStringRef* cdhash,
                            CFStringRef* teamId,
                            CFStringRef* subjectCN,
                            CFStringRef* issuerCN)
{
    if (!kextURL) {
        return;
    }
    
    SecStaticCodeRef    code                = NULL; // must release
    CFDictionaryRef     information         = NULL; // must release
    
    SecCertificateRef   issuerCertificate   = NULL; // do not release
    CFArrayRef          certificateChain    = NULL; // do not release
    OSStatus            status;
    CFIndex             count;
    
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &code) != 0
        || (code == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    if (cdhash) {
        *cdhash = copyCDHash(code);
    }
    
    status = SecCodeCopySigningInformation(code,
                                           kSecCSSigningInformation,
                                           &information);
    if (status != noErr) {
        goto finish;
    }
    
    // a CFArrayRef array of SecCertificateRef objects
    certificateChain = (CFArrayRef) CFDictionaryGetValue(information, kSecCodeInfoCertificates);
    if (!certificateChain ||
        CFGetTypeID(certificateChain) != CFArrayGetTypeID()) {
        goto finish;
    }
    
    count = CFArrayGetCount(certificateChain);
    if (count < 1) {
        goto finish;
    }
    
    issuerCertificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificateChain, 0);
    if (!issuerCertificate) {
        goto finish;
    }
    
    if (subjectCN) {
        SecCertificateCopyCommonName(issuerCertificate, subjectCN);
    }
    
    if (teamId) {
        *teamId = copyTeamID(issuerCertificate);
    }
    
    if (issuerCN) {
        *issuerCN = copyIssuerCN(issuerCertificate);
    }
    
finish:
    SAFE_RELEASE(code);
    SAFE_RELEASE(information);
    
    return;
}

/*******************************************************************************
 * copySubjectCNArray() - copy the subject CN from every certificate in the kext's
 *  certificate chain.
 *  Note: the caller must release the created CFArrayRef
 *******************************************************************************/
static CFArrayRef copySubjectCNArray(CFURLRef kextURL)
{
    if (!kextURL) {
        return NULL;
    }
    
    CFMutableArrayRef   subjectCNArray      = NULL; // do not release
    CFArrayRef          certificateChain    = NULL; // do not release
    SecCertificateRef   certificate         = NULL; // do not release
    
    SecStaticCodeRef    code                = NULL; // must release
    CFDictionaryRef     information         = NULL; // must release
    
    OSStatus            status;
    CFIndex             count;
    
    subjectCNArray = CFArrayCreateMutable(kCFAllocatorDefault,
                                          0,
                                          &kCFTypeArrayCallBacks);
    if (!subjectCNArray) {
        goto finish;
    }
    
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &code) != 0
        || (code == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    status = SecCodeCopySigningInformation(code,
                                           kSecCSSigningInformation,
                                           &information);
    if (status != noErr) {
        goto finish;
    }
    
    certificateChain = (CFArrayRef) CFDictionaryGetValue(information, kSecCodeInfoCertificates);
    if (!certificateChain ||
        CFGetTypeID(certificateChain) != CFArrayGetTypeID()) {
        goto finish;
    }
    
    count = CFArrayGetCount(certificateChain);
    if (count < 1) {
        goto finish;
    }
    
    for (CFIndex i=0; i<count; i++) {
        certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificateChain, i);
        CFStringRef subjectCN = NULL; // must release
        SecCertificateCopyCommonName(certificate, &subjectCN);
        if (subjectCN) {
            CFArrayAppendValue(subjectCNArray, subjectCN);
            SAFE_RELEASE(subjectCN);
        }
    }
    
finish:
    SAFE_RELEASE(code);
    SAFE_RELEASE(information);
    
    return subjectCNArray;
}

/*******************************************************************************
 * filterKextLoadForMT() - check that the kext is of interest, and place kext
 * information in the kext list
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/

static void filterKextLoadForMT(OSKextRef aKext, CFMutableArrayRef *kextList)
{
    if (aKext == NULL || kextList == NULL)
        return;
    
    CFStringRef     versionString;                // do not release
    CFStringRef     bundleIDString;               // do not release
    CFStringRef     kextSigningCategory = NULL;   // do not release
    CFBooleanRef    isFat               = kCFBooleanFalse; // do not release
    CFBooleanRef    isSigned            = kCFBooleanFalse; // do not release
    
    CFURLRef        kextURL             = NULL;   // must release
    CFStringRef     kextPath            = NULL;   // must release
    CFStringRef     filename            = NULL;   // must release
    CFStringRef     hashString          = NULL;   // must release
    CFStringRef     archString          = NULL;   // must release
    CFStringRef     teamId              = NULL;   // must release
    CFStringRef     subjectCN           = NULL;   // must release
    CFStringRef     issuerCN            = NULL;   // must release
    
    SecStaticCodeRef        code        = NULL;   // must release
    CFDictionaryRef         information = NULL;   // must release
    CFMutableDictionaryRef  kextDict    = NULL;   // must release
    
    char *          hashCString         = NULL;   // must free

    OSStatus status = noErr;
    
    /* do not message trace this if boot-args has debug set */
    if (isDebugSetInBootargs()) {
        return;
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
    
    versionString   = OSKextGetValueForInfoDictionaryKey(aKext,
                                                         kCFBundleVersionKey);
    bundleIDString  = OSKextGetValueForInfoDictionaryKey(aKext,
                                                         kCFBundleIdentifierKey);    
    filename = CFURLCopyLastPathComponent(kextURL);
    
    archString = createArchitectureList(aKext, &isFat);
    
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &code) != 0
        || (code == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    status = SecCodeCopySigningInformation(code,
                                           kSecCSSigningInformation,
                                           &information);
    if (status != noErr) {
        goto finish;
    }
    
    isSigned = CFDictionaryContainsKey(information, kSecCodeInfoIdentifier);
    if (!isSigned) {
        /* The kext is unsigned, so there is little information we can retrieve.
         * A hash of the kext is generated for data collection. */
        kextSigningCategory = CFSTR(kUnsignedKext);
        
        createHashForMT(kextURL, &hashCString);
        if (hashCString) {
            hashString = CFStringCreateWithCString(kCFAllocatorDefault,
                                                   hashCString,
                                                   kCFStringEncodingUTF8);
        }
    }
    else {
        CFStringRef myCFString = NULL; // do not release
        myCFString = OSKextGetIdentifier(aKext);
        status = checkKextSignature(aKext, true);
        if (CFStringHasPrefix(myCFString, __kOSKextApplePrefix)) {
            if (status == noErr) {
                /* This is a signed Apple kext, with an Apple root certificate.
                 * There is no need to retrieve additional signing information */
                kextSigningCategory = CFSTR(kAppleKextWithAppleRoot);
                copySigningInfo(kextURL,
                                &hashString,
                                NULL,
                                NULL,
                                NULL);
            }
            else {
                /* This is a signed Apple kext, but the root certificate is not Apple.
                 * This should not happen, so it is better to flag it as unsigned for
                 * collection purpose. */
                kextSigningCategory = CFSTR(kUnsignedKext);
                copySigningInfo(kextURL,
                                &hashString,
                                &teamId,
                                NULL,
                                &issuerCN);
                CFArrayRef subjectCNArray = NULL; // must release
                subjectCNArray = copySubjectCNArray(kextURL);
                if (subjectCNArray) {
                    subjectCN = CFStringCreateByCombiningStrings(kCFAllocatorDefault,
                                                                 subjectCNArray,
                                                                 CFSTR(";"));
                    SAFE_RELEASE(subjectCNArray);
                }
            }
        }
        else {
            if (status == noErr) {
                /* This 3rd-party kext is signed with a devid+ kext certificate */
                kextSigningCategory = CFSTR(k3rdPartyKextWithDevIdPlus);
                copySigningInfo(kextURL,
                                &hashString,
                                &teamId,
                                &subjectCN,
                                &issuerCN);
            }
            else if (status == CSSMERR_TP_CERT_REVOKED) {
                /* This 3rd-party kext is signed with a revoked devid+ kext certificate */
                kextSigningCategory = CFSTR(k3rdPartyKextWithRevokedDevIdPlus);
                copySigningInfo(kextURL,
                                &hashString,
                                &teamId,
                                &subjectCN,
                                &issuerCN);
            }
            else {
                status = checkRootCertificateIsApple(aKext);
                if (status == noErr) {
                    /* This 3rd-party kext is not signed with a devid+ certificate,
                     * but uses an Apple root certificate. */
                    kextSigningCategory = CFSTR(k3rdPartyKextWithAppleRoot);
                    /* The certificates may not have the expected format.
                     * Attempt to get the information if present, and also
                     * retrieve the subject cn of every certificate in the chain. */
                    copySigningInfo(kextURL,
                                    &hashString,
                                    &teamId,
                                    NULL,
                                    &issuerCN);
                    CFArrayRef subjectCNArray = NULL; // must release
                    subjectCNArray = copySubjectCNArray(kextURL);
                    if (subjectCNArray) {
                        subjectCN = CFStringCreateByCombiningStrings(kCFAllocatorDefault,
                                                                     subjectCNArray,
                                                                     CFSTR(";"));
                        SAFE_RELEASE(subjectCNArray);
                    }
                }
                else {
                    /* This 3rd-party kext is not signed with a devid+ certificate,
                     * and does not use an Apple root certificate. */
                    kextSigningCategory = CFSTR(k3rdPartyKextWithoutAppleRoot);
                    /* The certificates may not have the expected format.
                     * The subjectcn, issuercn and teamid must not be logged. */
                    copySigningInfo(kextURL,
                                    &hashString,
                                    NULL,
                                    NULL,
                                    NULL);
                }
            }
        }
    }
        
    kextDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                         0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    if (!kextDict) {
        OSKextLogMemError();
        goto finish;
    }

    if (bundleIDString) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerBundleIDKey),
                             bundleIDString);
    }
    if (versionString) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerVersionKey),
                             versionString);
    }
    if (filename) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerKextNameKey),
                             filename);
    }
    if (isFat != NULL) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerFatKey),
                             isFat);
    }
    if (archString) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerArchKey),
                             archString);
    }
    
    if (kextSigningCategory) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerSignatureTypeKey),
                             kextSigningCategory);
    }
    
    if (hashString) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerHashKey),
                             hashString);
    }
    
    if (teamId) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerTeamIdKey),
                             teamId);
    }
    if (subjectCN) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerSubjectCNKey),
                             subjectCN);
    }
    if (issuerCN) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerIssuerCNKey),
                             issuerCN);
    }
    
    if (kextPath) {
        CFDictionaryAddValue(kextDict,
                             CFSTR(kMessageTracerPathKey),
                             kextPath);
    }

    
    CFArrayAppendValue(*kextList, kextDict);
    
finish:
    SAFE_FREE(hashCString);
    SAFE_RELEASE(kextURL);
    SAFE_RELEASE(kextPath);
    SAFE_RELEASE(filename);
    SAFE_RELEASE(hashString);
    SAFE_RELEASE(kextDict);
    SAFE_RELEASE(archString);
    SAFE_RELEASE(teamId);
    SAFE_RELEASE(subjectCN);
    SAFE_RELEASE(issuerCN);
    SAFE_RELEASE(code);
    SAFE_RELEASE(information);
    return;
}

/*******************************************************************************
 * recordKextLoadListForMT() - record the list of loaded kexts
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/
void
recordKextLoadListForMT(CFArrayRef kextList)
{
    CFIndex             count, i;
    CFMutableArrayRef   kextsToMessageTrace = NULL; //must release
    OSKextRef           aKext;                      //do not release
    
    if (kextList && (count = CFArrayGetCount(kextList))) {
        kextsToMessageTrace = CFArrayCreateMutable(kCFAllocatorDefault,
                                                  CFArrayGetCount(kextList),
                                                  &kCFTypeArrayCallBacks);
        if (kextsToMessageTrace) {
            for (i = 0; i < count; i ++) {                
                aKext = (OSKextRef)CFArrayGetValueAtIndex(kextList, i);
                filterKextLoadForMT(aKext, &kextsToMessageTrace);
            }
            if (CFArrayGetCount(kextsToMessageTrace)) {
                postNoteAboutKextLoadsMT(CFSTR("Loaded Kext Notification"),
                                         kextsToMessageTrace);
            }
            SAFE_RELEASE(kextsToMessageTrace);
        }
    }
}

/*******************************************************************************
 * recordKextLoadForMT() - record the loaded kext
 *  <rdar://problem/12435992> Message tracing for kext loads
 *******************************************************************************/
void recordKextLoadForMT(OSKextRef aKext)
{
    CFMutableArrayRef myArray = NULL; // must release
    
    if (!aKext)
        return;
    
    myArray = CFArrayCreateMutable(kCFAllocatorDefault,
                                   1,
                                   &kCFTypeArrayCallBacks);
    if (myArray) {
        CFArrayAppendValue(myArray, aKext);
        recordKextLoadListForMT(myArray);
        SAFE_RELEASE(myArray);
    }
}

/*******************************************************************************
 * checkKextSignature() - check the signature for given kext.
 *******************************************************************************/
OSStatus checkKextSignature(OSKextRef aKext, Boolean checkExceptionList)
{
    OSStatus                result          = -1;
    CFURLRef                kextURL         = NULL;   // must release
    SecStaticCodeRef        staticCodeRef   = NULL;   // must release
    SecRequirementRef       requirementRef  = NULL;   // must release
    CFStringRef             myCFString;
    CFStringRef             requirementsString;
    
    if (aKext == NULL) {
        return result;
    }
    
    kextURL = CFURLCopyAbsoluteURL(OSKextGetURL(aKext));
    if (!kextURL) {
        OSKextLogMemError();
        goto finish;
    }
    
    if (SecStaticCodeCreateWithPath(kextURL,
                                    kSecCSDefaultFlags,
                                    &staticCodeRef) != errSecSuccess ||
        (staticCodeRef == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* set up correct requirement string.  Apple kexts are signed by B&I while
     * 3rd party kexts are signed through a special developer kext devid
     * program
     */
    myCFString = OSKextGetIdentifier(aKext);
    if (CFStringHasPrefix(myCFString, __kOSKextApplePrefix)) {
        requirementsString = CFSTR("anchor apple");
    }
    else {
        /* DevID for kexts cert
         */
        requirementsString =
            CFSTR("anchor apple generic "
                  "and certificate 1[field.1.2.840.113635.100.6.2.6] "
                  "and certificate leaf[field.1.2.840.113635.100.6.1.13] "
                  "and certificate leaf[field.1.2.840.113635.100.6.1.18]" );
    }

    if (SecRequirementCreateWithString(requirementsString,
                                       kSecCSDefaultFlags,
                                       &requirementRef) != errSecSuccess ||
        (requirementRef == NULL)) {
        OSKextLogMemError();
        goto finish;
    }
    
    // errSecCSUnsigned == -67062
    result = SecStaticCodeCheckValidity(staticCodeRef,
                                        kSecCSEnforceRevocationChecks,
                                        requirementRef);
    if ( result != 0 &&
         checkExceptionList &&
         isInExceptionList(aKext, true) ) {
        result = 0;
    }
    
finish:
    SAFE_RELEASE(kextURL);
    SAFE_RELEASE(staticCodeRef);
    SAFE_RELEASE(requirementRef);
    
    return result;
}

#define GET_CSTRING_PTR(the_cfstring, the_ptr, the_buffer, the_size) \
do { \
the_ptr = CFStringGetCStringPtr(the_cfstring, kCFStringEncodingUTF8); \
if (the_ptr == NULL) { \
the_buffer[0] = 0x00; \
the_ptr = the_buffer;  \
CFStringGetCString(the_cfstring, the_buffer, the_size, kCFStringEncodingUTF8); \
} \
} while(0)

/*********************************************************************
 * isInExceptionList checks to see if the given kext is in the
 * kext signing exception list (in com.apple.driver.KextExcludeList).  
 * If useCache is TRUE, we will use the cached copy of the exception list.
 * If useCache is FALSE, we will refresh the cache from disk.  
 *
 * The kext signing exception list rarely changes but to insure you have the 
 * most recent copy in the cache pass FALSE for the first call and TRUE for
 * subsequent calls (when dealing with a large list of kexts).
 * theKext can be NULL if you just want the invalidate the cache.
 *********************************************************************/
Boolean isInExceptionList(OSKextRef theKext, Boolean useCache)
{
    Boolean             result              = false;
    CFStringRef         kextID              = NULL;  // must release
    OSKextRef           excludelistKext     = NULL;  // must release
    CFDictionaryRef     exceptionlistDict   = NULL;  // do NOT release
    static CFDictionaryRef myDictionary     = NULL;  // do NOT release
    
    /* invalidate the cache or create it if not present */
    if (useCache == false || myDictionary == NULL) {
        if (myDictionary != NULL) {
            SAFE_RELEASE_NULL(myDictionary);
        }
        kextID = CFStringCreateWithCString(kCFAllocatorDefault,
                                           "com.apple.driver.KextExcludeList",
                                           kCFStringEncodingUTF8);
        if (!kextID) {
            OSKextLogStringError(/* kext */ NULL);
            goto finish;
        }
        
        excludelistKext = OSKextCreateWithIdentifier(kCFAllocatorDefault,
                                                     kextID);
        if (!excludelistKext) {
            OSKextLog(/* kext */ NULL,
                      kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                      "Warning: %s could not find com.apple.driver.KextExcludeList",
                      __FUNCTION__);
            goto finish;
        }
        
        exceptionlistDict = OSKextGetValueForInfoDictionaryKey(
                                            excludelistKext,
                                            CFSTR("OSKextSigExceptionList"));
        if (!exceptionlistDict) {
            goto finish;
        }
        
        if ((unsigned int)CFDictionaryGetCount(exceptionlistDict) > 0) {
            myDictionary = CFDictionaryCreateCopy(NULL, exceptionlistDict);
        }
        if (myDictionary == NULL) {
            OSKextLogMemError();
            goto finish;
        }
    }
    if (theKext == NULL) {
        goto finish;
    }
   
    /*********************************************************************
     * myDictionary is a dictionary with keys / values of:
     *  key = bundleID string of kext we will allow to load inspite of signing
     *      failure.
     *  value = version string of kext to allow to load.
     *      The value is used to check equal or less than a kext with a matching
     *      version string.  For example if an entry in the list has key:
     *      com.foocompany.fookext 
     *      and value:
     *      4.2.10
     *      Then any kext with bundle ID of com.foocompany.fookext and a version
     *      string of 4.2.10 or less will be allowed to load even if there is a
     *      a kext signing validation failure.
     *
     * NOTE - Kext versions use an extended Mac OS 'vers' format with double 
     * the number of digits before the build stage: ####.##.##s{1-255} where 's'
     * is a build stage 'd', 'a', 'b', 'f' or 'fc'.  We parse this with
     * OSKextParseVersionString
     *********************************************************************/
    CFStringRef     bundleID                = NULL;  // do NOT release
    CFStringRef     exceptionKextVersString = NULL;  // do NOT release
    OSKextVersion   kextVers                = -1;
    const char *    versCString             = NULL;  // do not free
    OSKextVersion   exceptionKextVers;
    char            versBuffer[256];
    
    bundleID = OSKextGetIdentifier(theKext);
    if (!bundleID) {
        OSKextLog(/* kext */ NULL,
                  kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                  "%s could not get bundleID",
                  __FUNCTION__);
        goto finish;
    }
    
    kextVers = OSKextGetVersion(theKext);
    if (!kextVers) {
        OSKextLog(/* kext */ NULL,
                  kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                  "%s could not get kextVers",
                  __FUNCTION__);
        goto finish;
    }
    
    exceptionKextVersString = CFDictionaryGetValue(myDictionary, bundleID);
    if (!exceptionKextVersString) {
        goto finish;
    }
    
    /* parse version strings */
    GET_CSTRING_PTR(exceptionKextVersString,
                    versCString,
                    versBuffer,
                    sizeof(versBuffer));
    if (strlen(versCString) < 1) {
        goto finish;
    }
    
    exceptionKextVers = OSKextParseVersionString(versCString);
    if (kextVers <= exceptionKextVers) {
        OSKextLogCFString(NULL,
                          kOSKextLogGeneralFlag | kOSKextLogErrorLevel,
                          CFSTR("kext %@  %lld is in exception list, allowing to load"),
                          bundleID, kextVers);
        result = true;
    }
    
finish:
    SAFE_RELEASE(kextID);
    SAFE_RELEASE(excludelistKext);
    return result;
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
        if (bootargsEntry &&
            (CFGetTypeID(bootargsEntry) == CFStringGetTypeID())) {
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


/*********************************************************************
 *********************************************************************/
Boolean isInLibraryExtensionsFolder(OSKextRef theKext)
{
    CFStringRef     myKextPath = NULL; // must release
    Boolean         myResult = false;

    myKextPath = copyKextPath(theKext);
    if ( myKextPath ) {
        if ( CFStringHasPrefix(myKextPath,
                               CFSTR(_kOSKextLibraryExtensionsFolder)) ) {
            myResult = true;
        }
    }
    SAFE_RELEASE(myKextPath);
    return(myResult);
}




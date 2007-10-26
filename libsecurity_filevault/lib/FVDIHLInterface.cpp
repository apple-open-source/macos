/*
 *  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  FVDIHLInterface.cpp -- Dynamically find and load DiskImages functions
 */

//     OSStatus error = _dihlDiskImageAttach((CFDictionaryRef)dict, nil, nil, (CFDictionaryRef*)&results);
//    OSStatus status = DIHLDiskImageCreate(operation, NULL, NULL, &outResults);

#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>

#include "FVDIHLInterface.h"

#pragma mark -------------------- Static initializations --------------------

bool FVDIHLInterface::instanceFlag = false;
FVDIHLInterface* FVDIHLInterface::fvdihlInterface = NULL;

#pragma mark -------------------- Implementation --------------------

FVDIHLInterface *FVDIHLInterface::getInstance()
{
    if (instanceFlag)
        return fvdihlInterface;

    fvdihlInterface = new FVDIHLInterface();
    instanceFlag = fvdihlInterface->loadDiskImagesFramework();
    return fvdihlInterface;
}

bool FVDIHLInterface::loadDiskImagesFramework()
{
	if (_dihlDiskImageAttach!=NULL && _dihlDiskImageCreate!=NULL && _dihlDiskImageChangePassword!=NULL &&
        _dihlDIInitialize!=NULL && _dihlDIDeinitialize!=NULL)
		return true;

#if 1 || defined(NDEBUG)
    const char *dyld_framework_path = NULL;
#else
    const char *dyld_framework_path = getenv("DYLD_FRAMEWORK_PATH");
#endif

    CFRef<CFStringRef> difPathStr = dyld_framework_path?
        CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/DiskImages.framework"), dyld_framework_path, NULL)
        :CFSTR("/System/Library/PrivateFrameworks/DiskImages.framework");
    CFRef<CFURLRef> bundleURL = CFURLCreateWithFileSystemPath(NULL, difPathStr, kCFURLPOSIXPathStyle, false);
    if (!bundleURL)
        return false;

    CFRef<CFBundleRef> bundleRef = CFBundleCreate(NULL, bundleURL);
    if (!bundleRef)
        return false;

	if (!CFBundleIsExecutableLoaded(bundleRef))
        CFBundleLoadExecutable(bundleRef);
    
    _dihlDiskImageCreate = (_dihlDiskImageCreateProc *)CFBundleGetFunctionPointerForName(bundleRef, CFSTR("DIHLDiskImageCreate"));
    _dihlDiskImageAttach = (_dihlDiskImageAttachProc *)CFBundleGetFunctionPointerForName(bundleRef, CFSTR("DIHLDiskImageAttach"));
    _dihlDiskImageChangePassword = (_dihlDiskImageChangePasswordProc *)CFBundleGetFunctionPointerForName(bundleRef,
        CFSTR("DIHLDiskImageChangePassword"));
    _dihlDIInitialize = (_dihlDIInitializeProc *)CFBundleGetFunctionPointerForName(bundleRef, CFSTR("DIInitialize"));
    _dihlDIDeinitialize = (_dihlDIDeinitializeProc *)CFBundleGetFunctionPointerForName(bundleRef, CFSTR("DIDeinitialize"));
    _dihlDiskImageCompact = (_dihlDiskImageCompactProc *)CFBundleGetFunctionPointerForName(bundleRef, CFSTR("DIHLDiskImageCompact"));

    if (_dihlDiskImageAttach==NULL || _dihlDiskImageCreate==NULL || _dihlDiskImageChangePassword==NULL || _dihlDIInitialize==NULL ||
        _dihlDIDeinitialize==NULL || _dihlDiskImageCompact==NULL)
        MacOSError::throwMe(kUnsupportedFunctionErr);
        
    return true;
}


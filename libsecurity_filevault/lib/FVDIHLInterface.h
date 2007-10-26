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
 *  FVDIHLInterface.h -- Dynamically find and load DiskImages functions
 */

#include <CoreFoundation/CoreFoundation.h>
#include <DiskImages/DIHLInterface.h>

typedef OSStatus _dihlDiskImageCreateProc(CFDictionaryRef inOptions, DIHLStatusProcPtr inStatusProc, void *inContext,
    CFDictionaryRef *outResults);
typedef OSStatus _dihlDiskImageAttachProc(CFDictionaryRef inOptions, DIHLStatusProcPtr inStatusProc, void *inContext,
    CFDictionaryRef *outResults);
typedef OSStatus _dihlDiskImageChangePasswordProc(CFDictionaryRef inOptions);
typedef OSStatus _dihlDiskImageCompactProc(CFDictionaryRef inOptions, void *inContext, DIHLStatusProcPtr inStatusProc);

// From DIHighLevelAPI.h
typedef int _dihlDIInitializeProc();
typedef void _dihlDIDeinitializeProc();

class FVDIHLInterface
{
private:
    static bool instanceFlag;
    static FVDIHLInterface *fvdihlInterface;
    FVDIHLInterface() : _dihlDiskImageCreate(NULL), _dihlDiskImageAttach(NULL), _dihlDiskImageChangePassword(NULL),
        _dihlDIInitialize(NULL), _dihlDIDeinitialize(NULL) {};		//private constructor
public:
    static FVDIHLInterface* getInstance();
    ~FVDIHLInterface() { instanceFlag = false; }
    
	static _dihlDiskImageCreateProc *DIHLDiskImageCreate()	{ return getInstance()->_dihlDiskImageCreate; }
	static _dihlDiskImageAttachProc *DIHLDiskImageAttach()	{ return getInstance()->_dihlDiskImageAttach; }
	static _dihlDiskImageCompactProc *DIHLDiskImageCompact()	{ return getInstance()->_dihlDiskImageCompact; }
	static _dihlDIInitializeProc    *DIInitialize()			{ return getInstance()->_dihlDIInitialize;    }
	static _dihlDIDeinitializeProc  *DIDeinitialize()		{ return getInstance()->_dihlDIDeinitialize;  }
    static _dihlDiskImageChangePasswordProc *DIHLDiskImageChangePassword() 		{ return getInstance()->_dihlDiskImageChangePassword;  }
private:
	bool loadDiskImagesFramework();

    _dihlDiskImageCreateProc *_dihlDiskImageCreate;
    _dihlDiskImageAttachProc *_dihlDiskImageAttach;
    _dihlDiskImageChangePasswordProc *_dihlDiskImageChangePassword;
	_dihlDiskImageCompactProc *_dihlDiskImageCompact;
	_dihlDIInitializeProc    *_dihlDIInitialize;
	_dihlDIDeinitializeProc  *_dihlDIDeinitialize;
};

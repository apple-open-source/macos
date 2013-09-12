/*
 * Copyright © 1998-2012 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_IOUSBIUNKNOWN_H
#define _IOKIT_IOUSBIUNKNOWN_H

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

__BEGIN_DECLS
extern void *IOUSBLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID);
__END_DECLS

class IOUSBIUnknown {

public:
    struct InterfaceMap {
        IUnknownVTbl *pseudoVTable;
        IOUSBIUnknown *obj;
    };

private:
    IOUSBIUnknown(IOUSBIUnknown &src);	// Disable copy constructor
    void operator =(IOUSBIUnknown &src);
    IOUSBIUnknown() : refCount(1) { };

protected:

    static int factoryRefCount;
    static void factoryAddRef();
    static void factoryRelease();

    IOUSBIUnknown(void *unknownVTable);
    virtual ~IOUSBIUnknown(); // Also virtualise destructor

    static HRESULT genericQueryInterface(void *self, REFIID iid, void **ppv);
    static UInt32 genericAddRef(void *self);
    static UInt32 genericRelease(void *self);

    UInt32      _versionNumberFromString(CFStringRef versStr);
    Boolean     _isDigit(UniChar aChar);
    
protected:

    UInt32 refCount;
    InterfaceMap iunknown;

public:
    virtual HRESULT queryInterface(REFIID iid, void **ppv) = 0;
    virtual UInt32 addRef();
    virtual UInt32 release();
    
    virtual IOReturn				GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion);
};

#endif /* !_IOKIT_IOUSBIUNKNOWN_H */

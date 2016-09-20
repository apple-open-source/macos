/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef _IOKIT_IOHIDIUNKNOWN_H
#define _IOKIT_IOHIDIUNKNOWN_H

#include <IOKit/IOCFPlugIn.h>

__BEGIN_DECLS
//extern void *IOHIDLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID);
__END_DECLS

class IOHIDIUnknown {

public:
    struct InterfaceMap {
        IUnknownVTbl *pseudoVTable;
        IOHIDIUnknown *obj;
    };

private:
    IOHIDIUnknown(IOHIDIUnknown &src);	// Disable copy constructor
    void operator =(IOHIDIUnknown &src);
    IOHIDIUnknown() : refCount(1) { };

protected:

    static int factoryRefCount;
    static void factoryAddRef();
    static void factoryRelease();

    IOHIDIUnknown(void *unknownVTable);
    virtual ~IOHIDIUnknown(); // Also virtualise destructor

    static HRESULT genericQueryInterface(void *self, REFIID iid, void **ppv);
    static UInt32 genericAddRef(void *self);
    static UInt32 genericRelease(void *self);

protected:

    UInt32 refCount;
    InterfaceMap iunknown;

public:
    virtual HRESULT queryInterface(REFIID iid, void **ppv) = 0;
    virtual UInt32 addRef();
    virtual UInt32 release();
};

#endif /* !_IOKIT_IOHIDIUNKNOWN_H */

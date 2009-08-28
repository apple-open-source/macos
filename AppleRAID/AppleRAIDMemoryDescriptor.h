/*
 * Copyright (c) 2001-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLERAIDMEMORYDESCRIPTOR_H
#define _APPLERAIDMEMORYDESCRIPTOR_H

class AppleLVMVolume;

class AppleRAIDMemoryDescriptor : public IOMemoryDescriptor
{
    OSDeclareAbstractStructors(AppleRAIDMemoryDescriptor);
    
    friend class AppleRAIDEventSource;		// XXX remove this
    friend class AppleRAIDStorageRequest;	// XXX remove this
    friend class AppleLVMStorageRequest;	// XXX remove this
    friend class AppleLVMGroup;			// XXX remove this

protected:
    IOMemoryDescriptor		*mdMemoryDescriptor;
    IOLock			*mdMemoryDescriptorLock;
    
    AppleRAIDStorageRequest	*mdStorageRequest;
    UInt32			mdMemberIndex;
    UInt64			mdMemberByteStart;
    
    virtual bool initWithAddress(void *address, IOByteCount withLength, IODirection withDirection) { return false; }
    virtual bool initWithAddress(vm_address_t address, IOByteCount withLength, IODirection withDirection, task_t withTask)
				{ return false; }
    virtual bool initWithPhysicalAddress(IOPhysicalAddress address, IOByteCount withLength, IODirection withDirection)
                                         { return false; }
    virtual bool initWithRanges(IOVirtualRange *ranges, UInt32 withCount, IODirection withDirection, task_t withTask,
                                bool asReference = false) { return false; }
    virtual bool initWithPhysicalRanges(IOPhysicalRange *ranges, UInt32 withCount, IODirection withDirection,
                                        bool asReference = false) { return false; }

 public:
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex);
    virtual void free(void);
    
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart, UInt32 activeIndex) { return false; }
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor * memoryDescriptor, UInt64 requestStart, UInt64 requestSize, AppleLVMVolume * lv) { return false; }
    
    virtual addr64_t getPhysicalSegment(IOByteCount offset, IOByteCount * length, IOOptionBits options = 0) = 0;
    virtual IOReturn prepare(IODirection forDirection = kIODirectionNone);
    virtual IOReturn complete(IODirection forDirection = kIODirectionNone);
};

#endif  /* ! _APPLERAIDMEMORYDESCRIPTOR_H */

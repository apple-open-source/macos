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

#ifndef _APPLELVMSTORAGEREQUEST_H
#define _APPLELVMSTORAGEREQUEST_H

class AppleLVMMemoryDescriptor;

class AppleLVMStorageRequest : public AppleRAIDStorageRequest
{
    OSDeclareDefaultStructors(AppleLVMStorageRequest);
    
    friend class AppleRAIDSet;					// XXX remove this
    friend class AppleRAIDEventSource;				// XXX remove this
    friend class AppleLVMMemoryDescriptor;			// XXX remove this
    
    virtual void read(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
		      IOStorageAttributes *attributes, IOStorageCompletion *completion);
    virtual void write(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
		      IOStorageAttributes *attributes, IOStorageCompletion *completion);

public:
    static AppleLVMStorageRequest *withAppleRAIDSet(AppleRAIDSet * xsset);
};

#endif  /* ! _APPLELVMSTORAGEREQUEST_H */

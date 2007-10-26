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

#ifndef _APPLERAIDEVENTSOURCE_H
#define _APPLERAIDEVENTSOURCE_H

class AppleRAIDSet;

class AppleRAIDEventSource : public IOEventSource
{
    OSDeclareDefaultStructors(AppleRAIDEventSource);
    
protected:
    queue_head_t fCompletedHead;
    
    virtual bool checkForWork(void);
    
public:
    typedef void (*Action)(AppleRAIDSet * appleRAID, AppleRAIDStorageRequest * storageRequest);
    static AppleRAIDEventSource * withAppleRAIDSet(AppleRAIDSet *appleRAID, Action action);
    virtual bool initWithAppleRAIDSet(AppleRAIDSet *appleRAID, Action action);
    
    virtual void completeRequest(AppleRAIDMemoryDescriptor * memoryDescriptor,
				 IOReturn status, UInt64 actualByteCount);
    virtual void completeRequestLVG(AppleLVMMemoryDescriptor * memoryDescriptor,
				    IOReturn status, UInt64 actualByteCount);

    virtual IOStorageCompletionAction getStorageCompletionAction(void);
    virtual IOStorageCompletionAction getStorageCompletionActionLVG(void);
};


#endif /* ! _APPLERAIDEVENTSOURCE_H */

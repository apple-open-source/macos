/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLERAIDSTORAGEREQUEST_H
#define _APPLERAIDSTORAGEREQUEST_H

#define kAppleRAIDStorageRequestCount (16)

class AppleRAIDStorageRequest : public IOCommand
{
    OSDeclareDefaultStructors(AppleRAIDStorageRequest);
    
    friend class AppleRAIDSet;					// XXX remove this
    friend class AppleRAIDMirrorSet;				// XXX remove this
    friend class AppleRAIDEventSource;				// XXX remove this
    friend class AppleRAIDStripeMemoryDescriptor;		// XXX remove this
    friend class AppleRAIDMirrorMemoryDescriptor;		// XXX remove this
    friend class AppleRAIDConcatMemoryDescriptor;		// XXX remove this
    
private:
    AppleRAIDSet		*srRAIDSet;
    AppleRAIDEventSource	*srEventSource;
    AppleRAIDMemoryDescriptor   **srMemberMemoryDescriptors;
    IOReturn			srStatus;
    
    virtual void free(void);
    
protected:
    UInt64			srSetBlockSize;
    UInt64			srMemberBaseOffset;
    UInt32			srActiveCount;
    UInt32			srMemberCount;
    IOReturn                    *srMemberStatus;
    UInt64			*srMemberByteCounts;
    UInt32			srCompletedCount;
    UInt64			srByteStart;
    UInt64			srByteCount;
    IOService			*srClient;
    IOStorageCompletion		srCompletion;
    IOMemoryDescriptor		*srMemoryDescriptor;
    IODirection			srMemoryDescriptorDirection;
    
    virtual void read(IOService *client, UInt64 byteStart, IOMemoryDescriptor * buffer,
                      IOStorageCompletion completion);
    virtual void write(IOService *client, UInt64 byteStart, IOMemoryDescriptor * buffer,
                       IOStorageCompletion completion);

public:
    static AppleRAIDStorageRequest *withAppleRAIDSet(AppleRAIDSet * xsset);
    virtual bool initWithAppleRAIDSet(AppleRAIDSet * set);
    virtual void extractRequest(IOService **client, UInt64 *byteStart, IOMemoryDescriptor **buffer, IOStorageCompletion *completion);
};

#endif  /* ! _APPLERAIDSTORAGEREQUEST_H */

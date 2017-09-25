/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

class _IOFramebufferNotifier : public IONotifier
{
    friend class IOFramebuffer;

    OSDeclareDefaultStructors(_IOFramebufferNotifier)

public:
    OSOrderedSet *                      fWhence;

    IOFramebufferNotificationHandler    fHandler;
    OSObject *                          fTarget;
    void *                              fRef;
    bool                                fEnable;
    int32_t                             fGroup;
    IOIndex                             fGroupPriority;
    IOSelect                            fEvents;
    IOSelect                            fLastEvent;

    char                                fName[64];
    uint64_t                            fStampStart;
    uint64_t                            fStampEnd;

    virtual void remove() APPLE_KEXT_OVERRIDE;
    virtual bool disable() APPLE_KEXT_OVERRIDE;
    virtual void enable( bool was ) APPLE_KEXT_OVERRIDE;

    bool init(IOFramebufferNotificationHandler handler, OSObject * target, void * ref,
              IOIndex groupPriority, IOSelect events, int32_t groupIndex);
};

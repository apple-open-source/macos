/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOAUDIODMAENGINEUSERCLIENT_H
#define _IOKIT_IOAUDIODMAENGINEUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/audio/IOAudioTypes.h>

class IOAudioDMAEngine;
class IOAudioStream;
class IOMemoryDescriptor;
class IOCommandGate;
class IOWorkLoop;

typedef struct IOAudioClientBuffer;
typedef struct IOAudioFormatNotification;

class IOAudioDMAEngineUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOAudioDMAEngineUserClient)

    friend class IOAudioDMAEngine;

protected:
    IOAudioDMAEngine	*audioDMAEngine;
    
    IOWorkLoop			*workLoop;
    IOCommandGate		*commandGate;

    IOExternalMethod	methods[IOAUDIODMAENGINE_NUM_CALLS];
    IOExternalTrap		trap;
    
    task_t	clientTask;
    UInt32	numSampleFrames;
    
    IOAudioClientBuffer	*clientBufferList;
    IORecursiveLock *clientBufferListLock;
    
    IOAudioFormatNotification *formatNotificationList;
    IOAudioNotificationMessage *notificationMessage;
    
    virtual IOReturn clientClose();
    virtual IOReturn clientDied();
    
    virtual IOReturn clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory);
    virtual IOExternalMethod *getExternalMethodForIndex(UInt32 index);
    virtual IOExternalTrap *getExternalTrapForIndex(UInt32 index);
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon);
    
    static IOReturn registerFormatNotificationAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    virtual IOReturn registerFormatNotification(mach_port_t port, IOAudioStream *audioStream);
    
public:

    static IOAudioDMAEngineUserClient *withDMAEngine(IOAudioDMAEngine *dmaEngine, task_t clientTask, void *securityToken, UInt32 type);

    virtual bool initWithDMAEngine(IOAudioDMAEngine *dmaEngine, task_t task, void *securityToken, UInt32 type);
    virtual void free();
    
    virtual IOReturn registerClientBuffer(IOAudioStream *audioStream, void *sourceBuffer, UInt32 bufSizeInBytes);
    virtual IOReturn unregisterClientBuffer(void *sourceBuffer);
    
    virtual IOReturn getConnectionID(UInt32 *connectionID);
    
    virtual IOReturn performClientIO(UInt32 firstSampleFrame, bool inputIO);
    
    virtual void sendFormatChangeNotification(IOAudioStream *audioStream);

};

#endif /* _IOKIT_IOAUDIODMAENGINEUSERCLIENT_H */

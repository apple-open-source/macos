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

#include <IOKit/audio/IOAudioControlUserClient.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>

#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioControlUserClient, IOUserClient)

IOAudioControlUserClient *IOAudioControlUserClient::withAudioControl(IOAudioControl *control, task_t clientTask, void *securityID, UInt32 type)
{
    IOAudioControlUserClient *client;

    client = new IOAudioControlUserClient;

    if (client) {
        if (!client->initWithAudioControl(control, clientTask, securityID, type)) {
            client->release();
            client = 0;
        }
    }
    
    return client;
}

bool IOAudioControlUserClient::initWithAudioControl(IOAudioControl *control, task_t task, void *securityID, UInt32 type)
{
    if (!initWithTask(task, securityID, type)) {
        return false;
    }

    if (!control) {
        return false;
    }

    audioControl = control;
    clientTask = task;
    notificationMessage = 0;

    methods[kAudioControlSetValue].object = this;
    methods[kAudioControlSetValue].func = (IOMethod) &IOAudioControlUserClient::setControlValue;
    methods[kAudioControlSetValue].count0 = 1;
    methods[kAudioControlSetValue].count1 = 0;
    methods[kAudioControlSetValue].flags = kIOUCScalarIScalarO;

    methods[kAudioControlGetValue].object = this;
    methods[kAudioControlGetValue].func = (IOMethod) &IOAudioControlUserClient::getControlValue;
    methods[kAudioControlGetValue].count0 = 0;
    methods[kAudioControlGetValue].count1 = 1;
    methods[kAudioControlGetValue].flags = kIOUCScalarIScalarO;

    return true;
}

void IOAudioControlUserClient::free()
{
    clientClose();
    
    if (notificationMessage) {
        IOFree(notificationMessage, sizeof(IOAudioNotificationMessage));
        notificationMessage = 0;
    }

    super::free();
}

IOReturn IOAudioControlUserClient::clientClose()
{
    if (audioControl) {
        audioControl->clientClosed(this);
        audioControl = 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn IOAudioControlUserClient::clientDied()
{
    return clientClose();
}

IOReturn IOAudioControlUserClient::registerNotificationPort(mach_port_t port,
                                                            UInt32 type,
                                                            UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;

    if (notificationMessage == 0) {
        notificationMessage = (IOAudioNotificationMessage *)IOMalloc(sizeof(IOAudioNotificationMessage));
        if (!notificationMessage) {
            return kIOReturnNoMemory;
        }
    }

    notificationMessage->messageHeader.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    notificationMessage->messageHeader.msgh_size = sizeof(IOAudioNotificationMessage);
    notificationMessage->messageHeader.msgh_remote_port = port;
    notificationMessage->messageHeader.msgh_local_port = MACH_PORT_NULL;
    notificationMessage->messageHeader.msgh_reserved = 0;
    notificationMessage->messageHeader.msgh_id = 0;

    notificationMessage->type = type;
    notificationMessage->ref = refCon;

    return result;
}

void IOAudioControlUserClient::sendValueChangeNotification()
{
    if (notificationMessage) {
        kern_return_t kr;
        kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
        if ((kr != MACH_MSG_SUCCESS) && (kr != MACH_SEND_TIMED_OUT)) {
            IOLog("IOAudioControlUserClient: sendValueChangeNotification() failed - msg_send returned: %d\n", kr);
        }
    }
}

IOExternalMethod *IOAudioControlUserClient::getExternalMethodForIndex(UInt32 index)
{
    IOExternalMethod *method = 0;

    if (index < IOAUDIOCONTROL_NUM_CALLS) {
        method = &methods[index];
    }

    return method;
}

IOReturn IOAudioControlUserClient::setControlValue(UInt32 value)
{
    IOReturn result = kIOReturnError;
    
    if (audioControl) {
        IOCommandGate *cg;
        
        cg = audioControl->getCommandGate();
        
        if (cg) {
            cg->runAction(IOAudioControl::setValueAction, (void *)value);
        }
    }
    
    return result;
}

IOReturn IOAudioControlUserClient::getControlValue(UInt32 *value)
{
    if (audioControl) {
        *value = audioControl->getValue();
    } else {
        return kIOReturnError;
    }

    return kIOReturnSuccess;
}

IOReturn IOAudioControlUserClient::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnError;

    if (audioControl && properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSCollectionIterator *iterator;
        OSObject *iteratorKey;

        iterator = OSCollectionIterator::withCollection(props);
        if (iterator) {
            while (iteratorKey = iterator->getNextObject()) {
                OSSymbol *key;

                key = OSDynamicCast(OSSymbol, iteratorKey);
                if (key && key->isEqualTo(IOAUDIOCONTROL_VALUE_KEY)) {
                    OSNumber *value = OSDynamicCast(OSNumber, props->getObject(key));
                    if (value) {
                        IOCommandGate *cg;
                        
                        assert(audioControl);
                        
                        cg = audioControl->getCommandGate();
                        
                        if (cg) {
                            result = cg->runAction(IOAudioControl::setValueAction, (void *)value->unsigned32BitValue());
                        }
                    }
                }
            }

            iterator->release();
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

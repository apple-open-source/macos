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

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioControlUserClient.h>
#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#define super IOService

OSDefineMetaClassAndStructors(IOAudioControl, IOService)

IOAudioControl *IOAudioControl::withAttributes(const char *type,
                                               UInt32 initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID)
{
    IOAudioControl *control;

    control = new IOAudioControl;

    if (control) {
        if (!control->init(type, initialValue, channelID, channelName, cntrlID)) {
            control->release();
            control = 0;
        }
    }

    return control;
}

bool IOAudioControl::init(const char *type,
                          UInt32 initialValue,
                          UInt32 newChannelID,
                          const char *channelName,
                          UInt32 cntrlID,
                          OSDictionary *properties)
{
    if (!super::init(properties)) {
        return false;
    }

    if (!type) {
        return false;
    }

    audioPort = 0;

    value = initialValue;
    setProperty(IOAUDIOCONTROL_VALUE_KEY, value, sizeof(UInt32)*8);
    
    setProperty(IOAUDIOCONTROL_TYPE_KEY, type);
    
    setChannelID(newChannelID);
    setControlID(cntrlID);

    if (channelName) {
        setChannelName(channelName);
    }

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }

    return true;
}

void IOAudioControl::free()
{
    if (userClients) {
        // should we do some sort of notification here?
        userClients->release();
        userClients = NULL;
    }

    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }

        commandGate->release();
        commandGate = NULL;
    }

    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }

    super::free();
}

bool IOAudioControl::start(IOService *provider)
{
    if (!super::start(provider)) {
        return false;
    }

    if (!(audioPort = OSDynamicCast(IOAudioPort, provider))) {
        return false;
    }

    return true;
}

void IOAudioControl::stop(IOService *provider)
{
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }

        commandGate->release();
        commandGate = NULL;
    }

    super::stop(provider);
}

IOWorkLoop *IOAudioControl::getWorkLoop()
{
    if (!workLoop) {
        IOAudioDevice *audioDevice;
        
        assert(audioPort);
        
        audioDevice = audioPort->getAudioDevice();
        
        if (audioDevice) {
            workLoop = audioDevice->getWorkLoop();
            
            if (workLoop) {
                workLoop->retain();
            }
        }
    }
    
    return workLoop;
}

IOCommandGate *IOAudioControl::getCommandGate()
{
    if (!commandGate) {
        IOWorkLoop *wl;
        
        wl = getWorkLoop();
        if (wl) {
            commandGate = IOCommandGate::commandGate(this);
            
            if (commandGate) {
                wl->addEventSource(commandGate);
            }
        }
    }
    
    return commandGate;
}

IOReturn IOAudioControl::setValueAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->setValue((UInt32)arg1);
        }
    }

    return result;
}
IOReturn IOAudioControl::setValue(UInt32 newValue)
{
    IOReturn result = kIOReturnSuccess;

    if (value != newValue) {
        result = performValueChange(newValue);
        if (result == kIOReturnSuccess) { 
            value = newValue;
            setProperty(IOAUDIOCONTROL_VALUE_KEY, newValue, sizeof(UInt32)*8);
            sendValueChangeNotification();
        }
    }

    return result;
}

IOReturn IOAudioControl::performValueChange(UInt32 newValue)
{
    IOReturn result = kIOReturnError;

    if (audioPort) {
        result = audioPort->performAudioControlValueChange(this, newValue);
    }

    return result;
}

IOReturn IOAudioControl::flushValue()
{
    return performValueChange(getValue());
}

UInt32 IOAudioControl::getValue()
{
    return value;
}

void IOAudioControl::sendValueChangeNotification()
{
    OSCollectionIterator *iterator;
    IOAudioControlUserClient *client;
    
    if (!userClients) {
        return;
    }

    iterator = OSCollectionIterator::withCollection(userClients);
    while (client = (IOAudioControlUserClient *)iterator->getNextObject()) {
        client->sendValueChangeNotification();
    }
}

void IOAudioControl::setControlID(UInt32 cntrlID)
{
    controlID = cntrlID;
}

UInt32 IOAudioControl::getControlID()
{
    return controlID;
}

void IOAudioControl::setChannelID(UInt32 newChannelID)
{
    channelID = newChannelID;
    setProperty(IOAUDIOCONTROL_CHANNEL_ID_KEY, newChannelID, sizeof(UInt32)*8);
}

UInt32 IOAudioControl::getChannelID()
{
    return channelID;
}

void IOAudioControl::setChannelName(const char *channelName)
{
    if (channelName) {
        setProperty(IOAUDIOCONTROL_CHANNEL_NAME_KEY, channelName);
    }
}

IOReturn IOAudioControl::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *client;

    client = IOAudioControlUserClient::withAudioControl(this, task, securityID, type);

    if (client) {
        if (!client->attach(this)) {
            client->release();
            result = kIOReturnError;
        } else if (!client->start(this) || !userClients) {
            client->detach(this);
            client->release();
            result = kIOReturnError;
        } else {
            IOCommandGate *cg;
            
            cg = getCommandGate();
            
            if (cg) {
                result = cg->runAction(addUserClientAction, client);
    
                if (result == kIOReturnSuccess) {
                    *handler = client;
                }
            } else {
                result = kIOReturnError;
            }
        }
    } else {
        result = kIOReturnNoMemory;
    }

    return result;
}

void IOAudioControl::clientClosed(IOAudioControlUserClient *client)
{
    if (client) {
        IOCommandGate *cg;
        
        cg = getCommandGate();

        if (cg) {
            cg->runAction(removeUserClientAction, client);
            client->detach(this);
        }
    }
}

IOReturn IOAudioControl::addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->addUserClient((IOAudioControlUserClient *)arg1);
        }
    }

    return result;
}

IOReturn IOAudioControl::removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->removeUserClient((IOAudioControlUserClient *)arg1);
        }
    }

    return result;
}

IOReturn IOAudioControl::addUserClient(IOAudioControlUserClient *newUserClient)
{
    assert(userClients);

    userClients->setObject(newUserClient);

    return kIOReturnSuccess;
}

IOReturn IOAudioControl::removeUserClient(IOAudioControlUserClient *userClient)
{
    assert(userClients);

    userClients->setObject(userClient);

    return kIOReturnSuccess;
}

IOReturn IOAudioControl::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
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
                        
                        cg = getCommandGate();
                        
                        if (cg) {
                            result = cg->runAction(setValueAction, (void *)value->unsigned32BitValue());
                        } else {
                            result = kIOReturnError;
                        }
                    }
                }
            }
            iterator->release();
        } else {
            result = kIOReturnError;
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

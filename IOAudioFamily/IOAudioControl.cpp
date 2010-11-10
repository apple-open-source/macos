/*
 * Copyright (c) 1998-2010 Apple Computer, Inc. All rights reserved.
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

#include "IOAudioDebug.h"
#include "IOAudioControl.h"
#include "IOAudioControlUserClient.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#define super IOService

OSDefineMetaClassAndStructors(IOAudioControl, IOService)
OSMetaClassDefineReservedUsed(IOAudioControl, 0);
OSMetaClassDefineReservedUsed(IOAudioControl, 1);
OSMetaClassDefineReservedUsed(IOAudioControl, 2);
OSMetaClassDefineReservedUsed(IOAudioControl, 3);

OSMetaClassDefineReservedUnused(IOAudioControl, 4);
OSMetaClassDefineReservedUnused(IOAudioControl, 5);
OSMetaClassDefineReservedUnused(IOAudioControl, 6);
OSMetaClassDefineReservedUnused(IOAudioControl, 7);
OSMetaClassDefineReservedUnused(IOAudioControl, 8);
OSMetaClassDefineReservedUnused(IOAudioControl, 9);
OSMetaClassDefineReservedUnused(IOAudioControl, 10);
OSMetaClassDefineReservedUnused(IOAudioControl, 11);
OSMetaClassDefineReservedUnused(IOAudioControl, 12);
OSMetaClassDefineReservedUnused(IOAudioControl, 13);
OSMetaClassDefineReservedUnused(IOAudioControl, 14);
OSMetaClassDefineReservedUnused(IOAudioControl, 15);
OSMetaClassDefineReservedUnused(IOAudioControl, 16);
OSMetaClassDefineReservedUnused(IOAudioControl, 17);
OSMetaClassDefineReservedUnused(IOAudioControl, 18);
OSMetaClassDefineReservedUnused(IOAudioControl, 19);
OSMetaClassDefineReservedUnused(IOAudioControl, 20);
OSMetaClassDefineReservedUnused(IOAudioControl, 21);
OSMetaClassDefineReservedUnused(IOAudioControl, 22);
OSMetaClassDefineReservedUnused(IOAudioControl, 23);

// New code

// OSMetaClassDefineReservedUsed(IOAudioControl, 3);
IOReturn IOAudioControl::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioControlUserClient **newUserClient, OSDictionary *properties)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *userClient;
    
    userClient = IOAudioControlUserClient::withAudioControl(this, task, securityID, type, properties);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

void IOAudioControl::sendChangeNotification(UInt32 notificationType)
{
    OSCollectionIterator *iterator;
    IOAudioControlUserClient *client;
    
    if (!userClients || !isStarted) {
        return;
    }

	// If we're doing a config change, just queue the notification for later.
	if (reserved->providerEngine->configurationChangeInProgress) {
		OSNumber *notificationNumber;
		UInt32		i, count;
		bool		dupe = FALSE;

		if (!reserved->notificationQueue) {
			reserved->notificationQueue = OSArray::withCapacity (1);
			if (!reserved->notificationQueue) {
				return;
			}
		}

		notificationNumber = OSNumber::withNumber (notificationType, sizeof (notificationType) * 8);
		if (!notificationNumber)
			return;

		// Check to see if this is a unique notification, there is no need to send dupes.
		count = reserved->notificationQueue->getCount ();
		for (i = 0; i < count; i++) {
			if (notificationNumber->isEqualTo ((OSNumber *)reserved->notificationQueue->getObject (i))) {
				dupe = TRUE;
				break;		// no need to send duplicate notifications
			}
		}
		if (!dupe) {
			reserved->notificationQueue->setObject (notificationNumber);
		}
		notificationNumber->release ();
	} else {
		iterator = OSCollectionIterator::withCollection(userClients);
		if (iterator) {
			while (client = (IOAudioControlUserClient *)iterator->getNextObject()) {
				client->sendChangeNotification(notificationType);
			}
	
			iterator->release();
		}
	}
}

void IOAudioControl::sendQueuedNotifications(void)
{
	UInt32				i;
	UInt32				count;

	// Send our the queued notications and release the queue.
	if (reserved && reserved->notificationQueue) {
		count = reserved->notificationQueue->getCount ();
		for (i = 0; i < count; i++) {
			sendChangeNotification(((OSNumber *)reserved->notificationQueue->getObject(i))->unsigned32BitValue());
		}
		reserved->notificationQueue->release();
		reserved->notificationQueue = NULL;
	}
}

// Original code here...
IOAudioControl *IOAudioControl::withAttributes(UInt32 type,
                                               OSObject *initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID,
                                               UInt32 subType,
                                               UInt32 usage)
{
    IOAudioControl *control;

    control = new IOAudioControl;

    if (control) {
        if (!control->init(type, initialValue, channelID, channelName, cntrlID, subType, usage)) {
            control->release();
            control = 0;
        }
    }

    return control;
}

bool IOAudioControl::init(UInt32 type,
                          OSObject *initialValue,
                          UInt32 newChannelID,
                          const char *channelName,
                          UInt32 cntrlID,
                          UInt32 subType,
                          UInt32 usage,
                          OSDictionary *properties)
{
    if (!super::init(properties)) {
        return false;
    }
    
    if (initialValue == NULL) {
        return false;
    }

    if (type == 0) {
        return false;
    }
    
    setType(type);

    setChannelID(newChannelID);
    setControlID(cntrlID);

	setSubType(subType);
    
    if (channelName) {
        setChannelName(channelName);
    }
    
    if (usage != 0) {
        setUsage(usage);
    }
    
    _setValue(initialValue);

    userClients = OSSet::withCapacity(1);
    if (!userClients) {
        return false;
    }
    
	reserved = (ExpansionData *)IOMalloc (sizeof(struct ExpansionData));
	if (!reserved) {
		return false;
	}

	reserved->providerEngine = NULL;
	reserved->notificationQueue = NULL;
    isStarted = false;

    return true;
}

void IOAudioControl::setType(UInt32 type)
{
    this->type = type;
    setProperty(kIOAudioControlTypeKey, type, sizeof(UInt32)*8);
}

void IOAudioControl::setSubType(UInt32 subType)
{
    this->subType = subType;
    setProperty(kIOAudioControlSubTypeKey, subType, sizeof(UInt32)*8);
}

void IOAudioControl::setChannelName(const char *channelName)
{
    setProperty(kIOAudioControlChannelNameKey, channelName);
}

void IOAudioControl::setUsage(UInt32 usage)
{
    this->usage = usage;
    setProperty(kIOAudioControlUsageKey, usage, sizeof(UInt32)*8);
}

void IOAudioControl::setCoreAudioPropertyID(UInt32 propertyID)
{
    setProperty(kIOAudioControlCoreAudioPropertyIDKey, propertyID, sizeof(UInt32)*8);
    setUsage(kIOAudioControlUsageCoreAudioProperty);
}

void IOAudioControl::setReadOnlyFlag()
{
    setProperty(kIOAudioControlValueIsReadOnlyKey, (bool)true);
}

UInt32 IOAudioControl::getType()
{
    return type;
}

UInt32 IOAudioControl::getSubType()
{
    return subType;
}

UInt32 IOAudioControl::getUsage()
{
    return usage;
}

void IOAudioControl::free()
{
    audioDebugIOLog(3, "+ IOAudioControl[%p]::free()\n", this);

    if (userClients) {
        // should we do some sort of notification here?
        userClients->release();
        userClients = NULL;
    }

    if (valueChangeTarget) {
        valueChangeTarget->release();
        valueChangeTarget = NULL;
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

	if (reserved) {
		if (reserved->notificationQueue) {
			reserved->notificationQueue->release();
			reserved->notificationQueue = NULL;
		}

		IOFree (reserved, sizeof (struct ExpansionData));
		reserved = NULL;
	}

    super::free();
    audioDebugIOLog(3, "- IOAudioControl[%p]::free()\n", this);
}

bool IOAudioControl::start(IOService *provider)
{
    if (!super::start(provider)) {
        return false;
    }

    isStarted = true;
	reserved->providerEngine = OSDynamicCast (IOAudioEngine, provider);

    return true;
}

bool IOAudioControl::attachAndStart(IOService *provider)
{
    bool result = true;
    
    if (attach(provider)) {
        if (!isStarted) {
            result = start(provider);
            if (!result) {
                detach(provider);
            }
        }
    } else {
        result = false;
    }

    return result;
}

void IOAudioControl::stop(IOService *provider)
{
    audioDebugIOLog(3, "+ IOAudioControl[%p]::stop(%p)\n", this, provider);

    if (userClients && (userClients->getCount() > 0)) {
        IOCommandGate *cg;
        
        cg = getCommandGate();
        
		if (cg) {
			cg->runAction(detachUserClientsAction);
		}
    }
    
    if (valueChangeTarget) {
        valueChangeTarget->release();
        valueChangeTarget = NULL;
        valueChangeHandler.intHandler = NULL;
    }
    
	// <rdar://7233118>, <rdar://7029696> Remove the event source here as performing heavy workloop operation in free() could lead
	// to deadlock since the context which free() is called is not known. stop() is called on the workloop, so it is safe to remove 
	// the event source here.
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

    super::stop(provider);

    isStarted = false;
	
    audioDebugIOLog(3, "- IOAudioControl[%p]::stop(%p)\n", this, provider);
}

bool IOAudioControl::getIsStarted()
{
    return isStarted;
}

IOWorkLoop *IOAudioControl::getWorkLoop()
{
    return workLoop;
}

void IOAudioControl::setWorkLoop(IOWorkLoop *wl)
{
	if (!workLoop) {
		workLoop = wl;
	
		if (workLoop) {
			workLoop->retain();
	
			commandGate = IOCommandGate::commandGate(this);
	
			if (commandGate) {
				workLoop->addEventSource(commandGate);
			}
		}
	}
}

IOCommandGate *IOAudioControl::getCommandGate()
{
    return commandGate;
}

// <rdar://7529580>
IOReturn IOAudioControl::_setValueAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
                result = cg->runAction(setValueAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
}

IOReturn IOAudioControl::setValueAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;

    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->setValue((OSObject *)arg1);
        }
    }

    return result;
}

IOReturn IOAudioControl::setValue(OSObject *newValue)
{
    IOReturn result = kIOReturnSuccess;
    
    if (OSDynamicCast(OSNumber, newValue)) {
        audioDebugIOLog(3, "+ IOAudioControl[%p]::setValue(int = %d)\n", this, ((OSNumber *)newValue)->unsigned32BitValue());
    } else {
        audioDebugIOLog(3, "+ IOAudioControl[%p]::setValue(%p)\n", this, newValue);
    }

    if (newValue) {
        if (!value || !value->isEqualTo(newValue)) {
            result = validateValue(newValue);
            if (result == kIOReturnSuccess) {
                result = performValueChange(newValue);
                if (result == kIOReturnSuccess) {
                    result = updateValue(newValue);
                } else {
                    audioDebugIOLog(2, "  Error 0x%x received from driver - value not set!\n", result);
                }
            } else {
                audioDebugIOLog(2, "  Error 0x%x - invalid value.\n", result);
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }

    if (OSDynamicCast(OSNumber, newValue)) {
        audioDebugIOLog(3, "- IOAudioControl[%p]::setValue(int = %d) returns 0x%lX\n", this, ((OSNumber *)newValue)->unsigned32BitValue(), (long unsigned int)result );
    } else {
        audioDebugIOLog(3, "- IOAudioControl[%p]::setValue(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
    }

    return result;
}

IOReturn IOAudioControl::setValue(SInt32 intValue)
{
    IOReturn result = kIOReturnError;
    OSNumber *number;
    
    number = OSNumber::withNumber(intValue, sizeof(SInt32)*8);
    if (number) {
        result = setValue(number);
        number->release();
    }
    
    return result;
}

IOReturn IOAudioControl::validateValue(OSObject *value)
{
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::updateValue(OSObject *newValue)
{
    IOReturn result;
    
    result = _setValue(newValue);
    if (result == kIOReturnSuccess) {
        sendValueChangeNotification();
    }
    
    return result;
}

IOReturn IOAudioControl::_setValue(OSObject *newValue)
{
    if (value != newValue) {
        if (value) {
            value->release();
        }
        value = newValue;
        value->retain();
        
        setProperty(kIOAudioControlValueKey, value);
    }
    
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::hardwareValueChanged(OSObject *newValue)
{
    IOReturn result = kIOReturnSuccess;

    audioDebugIOLog(3, "+ IOAudioControl[%p]::hardwareValueChanged(%p)\n", this, newValue);
    
    if (newValue) {
        if (!value || !value->isEqualTo(newValue)) {
            result = validateValue(newValue);
            if (result == kIOReturnSuccess) {
                result = updateValue(newValue);
            } else {
                IOLog("IOAudioControl[%p]::hardwareValueChanged(%p) - Error 0x%x - invalid value.\n", this, newValue, result);
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }
    
    audioDebugIOLog(3, "- IOAudioControl[%p]::hardwareValueChanged(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
    return result;
}

void IOAudioControl::setValueChangeHandler(IntValueChangeHandler intValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kIntValueChangeHandler;
    valueChangeHandler.intHandler = intValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeHandler(DataValueChangeHandler dataValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kDataValueChangeHandler;
    valueChangeHandler.dataHandler = dataValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeHandler(ObjectValueChangeHandler objectValueChangeHandler, OSObject *target)
{
    valueChangeHandlerType = kObjectValueChangeHandler;
    valueChangeHandler.objectHandler = objectValueChangeHandler;
    setValueChangeTarget(target);
}

void IOAudioControl::setValueChangeTarget(OSObject *target)
{
    if (target) {
        target->retain();
    }
    
    if (valueChangeTarget) {
        valueChangeTarget->release();
    }
    
    valueChangeTarget = target;
}

IOReturn IOAudioControl::performValueChange(OSObject *newValue)
{
    IOReturn result = kIOReturnError;
    
    audioDebugIOLog(3, "+ IOAudioControl[%p]::performValueChange(%p)\n", this, newValue);

    if (valueChangeHandler.intHandler != NULL) {
        switch(valueChangeHandlerType) {
            case kIntValueChangeHandler:
                OSNumber *oldNumber, *newNumber;
                
                if ((oldNumber = OSDynamicCast(OSNumber, getValue())) == NULL) {
                    IOLog("IOAudioControl[%p]::performValueChange(%p) - Error: can't call handler - int handler set and old value is not an OSNumber.\n", this, newValue);
                    break;
                }
                
                if ((newNumber = OSDynamicCast(OSNumber, newValue)) == NULL) {
                    IOLog("IOAudioControl[%p]::performValueChange(%p) - Error: can't call handler - int handler set and new value is not an OSNumber.\n", this, newValue);
                    break;
                }
                
                result = valueChangeHandler.intHandler(valueChangeTarget, this, oldNumber->unsigned32BitValue(), newNumber->unsigned32BitValue());
                
                break;
            case kDataValueChangeHandler:
                OSData *oldData, *newData;
                const void *oldBytes, *newBytes;
                UInt32 oldSize, newSize;
                
                if (getValue()) {
                    if ((oldData = OSDynamicCast(OSData, getValue())) == NULL) {
                        IOLog("IOAudioControl[%p]::performValueChange(%p) - Error: can't call handler - data handler set and old value is not an OSData.\n", this, newValue);
                        break;
                    }
                    
                    oldBytes = oldData->getBytesNoCopy();
                    oldSize = oldData->getLength();
                } else {
                    oldBytes = NULL;
                    oldSize = 0;
                }
                
                if (newValue) {
                    if ((newData = OSDynamicCast(OSData, newValue)) == NULL) {
                        IOLog("IOAudioControl[%p]::performValueChange(%p) - Error: can't call handler - data handler set and new value is not an OSData.\n", this, newValue);
                        break;
                    }
                    
                    newBytes = newData->getBytesNoCopy();
                    newSize = newData->getLength();
                } else {
                    newBytes = NULL;
                    newSize = 0;
                }
                
                result = valueChangeHandler.dataHandler(valueChangeTarget, this, oldBytes, oldSize, newBytes, newSize);
                
                break;
            case kObjectValueChangeHandler:
                result = valueChangeHandler.objectHandler(valueChangeTarget, this, getValue(), newValue);
                break;
        }
    }

    audioDebugIOLog(3, "- IOAudioControl[%p]::performValueChange(%p) returns 0x%lX\n", this, newValue, (long unsigned int)result );
    return result;
}

IOReturn IOAudioControl::flushValue()
{
    return performValueChange(getValue());
}

OSObject *IOAudioControl::getValue()
{
    return value;
}

SInt32 IOAudioControl::getIntValue()
{
    OSNumber *number;
    SInt32 intValue = 0;
    
    number = OSDynamicCast(OSNumber, getValue());
    if (number) {
        intValue = (SInt32)number->unsigned32BitValue();
    }
    
    return intValue;
}

const void *IOAudioControl::getDataBytes()
{
    const void *bytes = NULL;
    OSData *data;
    
    data = OSDynamicCast(OSData, getValue());
    if (data) {
        bytes = data->getBytesNoCopy();
    }
    
    return bytes;
}

UInt32 IOAudioControl::getDataLength()
{
    UInt32 length = 0;
    OSData *data;
    
    data = OSDynamicCast(OSData, getValue());
    if (data) {
        length = data->getLength();
    }
    
    return length;
}

void IOAudioControl::sendValueChangeNotification()
{
    OSCollectionIterator *iterator;
    IOAudioControlUserClient *client;
    
    if (!userClients) {
        return;
    }

    iterator = OSCollectionIterator::withCollection(userClients);
    if (iterator) {
        while (client = (IOAudioControlUserClient *)iterator->getNextObject()) {
            client->sendValueChangeNotification();
        }
        
        iterator->release();
    }
}

void IOAudioControl::setControlID(UInt32 newControlID)
{
    controlID = newControlID;
    setProperty(kIOAudioControlIDKey, newControlID, sizeof(UInt32)*8);
}

UInt32 IOAudioControl::getControlID()
{
    return controlID;
}

void IOAudioControl::setChannelID(UInt32 newChannelID)
{
    channelID = newChannelID;
    setProperty(kIOAudioControlChannelIDKey, newChannelID, sizeof(UInt32)*8);
}

UInt32 IOAudioControl::getChannelID()
{
    return channelID;
}

void IOAudioControl::setChannelNumber(SInt32 channelNumber)
{
    setProperty(kIOAudioControlChannelNumberKey, channelNumber, sizeof(SInt32)*8);
}

IOReturn IOAudioControl::createUserClient(task_t task, void *securityID, UInt32 type, IOAudioControlUserClient **newUserClient)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *userClient;
    
    userClient = IOAudioControlUserClient::withAudioControl(this, task, securityID, type);
    
    if (userClient) {
        *newUserClient = userClient;
    } else {
        result = kIOReturnNoMemory;
    }
    
    return result;
}

IOReturn IOAudioControl::newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler)
{
#if __i386__ || __x86_64__
	return kIOReturnUnsupported;
#else
    IOReturn result = kIOReturnSuccess;
    IOAudioControlUserClient *client = NULL;
    
    audioDebugIOLog(3, "+ IOAudioControl[%p]::newUserClient()\n", this);

    if (!isInactive()) {	// <rdar://7324947>
		result = createUserClient(task, securityID, type, &client);
		
		if ((result == kIOReturnSuccess) && (client != NULL)) {
			if (!client->attach(this)) {
				client->release();
				result = kIOReturnError;
			} else if (!client->start(this) || !userClients) {
				client->detach(this);
				client->release();
				result = kIOReturnError;
			} else {
				if (workLoop) {		// <rdar://7324947>
					result = workLoop->runAction(_addUserClientAction, this, client);	// <rdar://7324947>, <rdar://7529580>
		
					if (result == kIOReturnSuccess) {
						*handler = client;
					}
				} else {
					result = kIOReturnError;
				}
			}
		}
    } else {	// <rdar://7324947>
        result = kIOReturnNoDevice;
    }
	
    audioDebugIOLog(3, "- IOAudioControl[%p]::newUserClient() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
#endif
}

//	<rdar://8121989>	Restructured for single point of entry and single point of exit so that 
//	the indentifier post processing tool can properly insert scope when post processing a log file
//	obtained via fwkpfv.

IOReturn IOAudioControl::newUserClient(task_t task, void *securityID, UInt32 type, OSDictionary *properties, IOUserClient **handler)
{
    IOReturn					result = kIOReturnSuccess;
    IOAudioControlUserClient *	client = NULL;
	
    audioDebugIOLog(3, "+ IOAudioControl[%p]::newUserClient()\n", this);
	
    if ( !isInactive () )	// <rdar://7324947>
	{
		if ( kIOReturnSuccess != newUserClient ( task, securityID, type, handler ) )
		{
			result = createUserClient ( task, securityID, type, &client, properties );
			
			if ( ( kIOReturnSuccess == result ) && ( NULL != client ) )
			{
				if ( !client->attach ( this ) )
				{
					client->release ();
					result = kIOReturnError;
				}
				else if ( !client->start ( this ) || !userClients)
				{
					client->detach ( this );
					client->release ();
					result = kIOReturnError;
				}
				else
				{
					if ( workLoop )	// <rdar://7324947>
					{
						result = workLoop->runAction ( _addUserClientAction, this, client );	// <rdar://7324947>, <rdar://7529580>
			
						if ( result == kIOReturnSuccess )
						{
							*handler = client;
						}
					}
					else
					{
						result = kIOReturnError;
					}
				}
			}
		}
    }
    else	// <rdar://7324947>
    {
        result = kIOReturnNoDevice;
    }
	
    audioDebugIOLog(3, "- IOAudioControl[%p]::newUserClient() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

void IOAudioControl::clientClosed(IOAudioControlUserClient *client)
{
    audioDebugIOLog(3, "+ IOAudioControl[%p]::clientClosed(%p)\n", this, client);

    if (client) {
		if (workLoop) {														// <rdar://7529580>
			workLoop->runAction(_removeUserClientAction, this, client);		// <rdar://7529580>
		}
    }
    audioDebugIOLog(3, "- IOAudioControl[%p]::clientClosed(%p)\n", this, client);
}

// <rdar://7529580>
IOReturn IOAudioControl::_addUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
                result = cg->runAction(addUserClientAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
        }
    }
    
    return result;
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

// <rdar://7529580>
IOReturn IOAudioControl::_removeUserClientAction(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (target) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, target);
        if (audioControl) {
            IOCommandGate *cg;
            
            cg = audioControl->getCommandGate();
            
            if (cg) {
                result = cg->runAction(removeUserClientAction, arg0, arg1, arg2, arg3);
            } else {
                result = kIOReturnError;
            }
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

IOReturn IOAudioControl::detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
    if (owner) {
        IOAudioControl *audioControl = OSDynamicCast(IOAudioControl, owner);
        if (audioControl) {
            result = audioControl->detachUserClients();
        }
    }
    
    return result;
}

IOReturn IOAudioControl::addUserClient(IOAudioControlUserClient *newUserClient)
{
    audioDebugIOLog(3, "+ IOAudioControl[%p]::addUserClient(%p)\n", this, newUserClient);

    assert(userClients);

    userClients->setObject(newUserClient);

    audioDebugIOLog(3, "- IOAudioControl[%p]::addUserClient(%p) returns 0x%lX\n", this, newUserClient, (long unsigned int)kIOReturnSuccess );
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::removeUserClient(IOAudioControlUserClient *userClient)
{
    audioDebugIOLog(3, "+ IOAudioControl[%p]::removeUserClient(%p)\n", this, userClient);

    assert(userClients);

    userClient->retain();
    
    userClients->removeObject(userClient);
    
    if (!isInactive()) {
        userClient->terminate();
    }
    
    userClient->release();

    audioDebugIOLog(3, "- IOAudioControl[%p]::removeUserClient(%p) returns 0x%lX\n", this, userClient, (long unsigned int)kIOReturnSuccess );
    return kIOReturnSuccess;
}

IOReturn IOAudioControl::detachUserClients()
{
    IOReturn result = kIOReturnSuccess;
    
    audioDebugIOLog(3, "+ IOAudioControl[%p]::detachUserClients()\n", this);
    
    assert(userClients);
    
    if (!isInactive()) {
        OSIterator *iterator;
        
        iterator = OSCollectionIterator::withCollection(userClients);
        
        if (iterator) {
            IOAudioControlUserClient *userClient;
            
            while (userClient = (IOAudioControlUserClient *)iterator->getNextObject()) {
                userClient->terminate();
            }
            
            iterator->release();
        }
    }
    
    userClients->flushCollection();
    
    audioDebugIOLog(3, "- IOAudioControl[%p]::detachUserClients() returns 0x%lX\n", this, (long unsigned int)result );
    return result;
}

IOReturn IOAudioControl::setProperties(OSObject *properties)
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSNumber *number = OSDynamicCast(OSNumber, props->getObject(kIOAudioControlValueKey));
        
        if (number) {
        	if (workLoop) {																// <rdar://7529580>
				result = workLoop->runAction(_setValueAction, this, (void *)number);	// <rdar://7529580>
            } else {
                result = kIOReturnError;
            }			
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

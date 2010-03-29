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
#include "IOAudioControlUserClient.h"
#include "IOAudioControl.h"
#include "IOAudioTypes.h"
#include "IOAudioDefines.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOKitKeys.h>

#define super IOUserClient

OSDefineMetaClassAndStructors(IOAudioControlUserClient, IOUserClient)
OSMetaClassDefineReservedUsed(IOAudioControlUserClient, 0);
OSMetaClassDefineReservedUsed(IOAudioControlUserClient, 1);

OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 2);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 3);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 4);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 5);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 6);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 7);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 8);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 9);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 10);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 11);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 12);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 13);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 14);
OSMetaClassDefineReservedUnused(IOAudioControlUserClient, 15);

// New code here

// OSMetaClassDefineReservedUsed(IOAudioControlUserClient, 1);
bool IOAudioControlUserClient::initWithAudioControl(IOAudioControl *control, task_t task, void *securityID, UInt32 type, OSDictionary *properties)
{
	// Declare Rosetta compatibility
	if (properties) {
		properties->setObject(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
	
    if (!initWithTask(task, securityID, type, properties)) {
        return false;
    }
/*
	// For 3019260
	if (clientHasPrivilege(securityID, kIOClientPrivilegeLocalUser)) {
		// You don't have enough privileges to control the audio
		return false;
	}
*/
    if (!control) {
        return false;
    }

    audioControl = control;
	audioControl->retain();
    clientTask = task;
    notificationMessage = 0;

    return true;
}

// OSMetaClassDefineReservedUsed(IOAudioControlUserClient, 0);
void IOAudioControlUserClient::sendChangeNotification(UInt32 notificationType)
{
    if (notificationMessage) {
        kern_return_t kr;

		notificationMessage->type = notificationType;
        kr = mach_msg_send_from_kernel(&notificationMessage->messageHeader, notificationMessage->messageHeader.msgh_size);
        if ((kr != MACH_MSG_SUCCESS) && (kr != MACH_SEND_TIMED_OUT)) {
            IOLog("IOAudioControlUserClient: sendRangeChangeNotification() failed - msg_send returned: %d\n", kr);
        }
    }
}

IOAudioControlUserClient *IOAudioControlUserClient::withAudioControl(IOAudioControl *control, task_t clientTask, void *securityID, UInt32 type, OSDictionary *properties)
{
    IOAudioControlUserClient *client;

    client = new IOAudioControlUserClient;

    if (client) {
        if (!client->initWithAudioControl(control, clientTask, securityID, type, properties)) {
            client->release();
            client = 0;
        }
    }
    
    return client;
}

// Original code here...
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
/*
	// For 3019260
	if (clientHasPrivilege(securityID, kIOClientPrivilegeLocalUser)) {
		// You don't have enough privileges to control the audio
		return false;
	}
*/
    if (!control) {
        return false;
    }

    audioControl = control;
	audioControl->retain();
    clientTask = task;
    notificationMessage = 0;

    return true;
}

void IOAudioControlUserClient::free()
{
    audioDebugIOLog(3, "IOAudioControlUserClient[%p]::free()", this);
    
    if (notificationMessage) {
        IOFreeAligned(notificationMessage, sizeof(IOAudioNotificationMessage));
        notificationMessage = 0;
    }

	if (audioControl) {
		audioControl->release ();
		audioControl = 0;
	}

	if (reserved) {
		IOFree (reserved, sizeof(struct ExpansionData));
	}

    super::free();
}

IOReturn IOAudioControlUserClient::clientClose()
{
    audioDebugIOLog(3, "IOAudioControlUserClient[%p]::clientClose()", this);

    if (audioControl) {
		if (!audioControl->isInactive () && !isInactive()) {
			audioControl->clientClosed(this);
		}
		audioControl->release();
        audioControl = 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn IOAudioControlUserClient::clientDied()
{
    audioDebugIOLog(3, "IOAudioControlUserClient[%p]::clientDied()", this);

    return clientClose();
}

IOReturn IOAudioControlUserClient::registerNotificationPort(mach_port_t port,
                                                            UInt32 type,			// No longer used now that we have the generic sendChangeNotification routine
                                                            UInt32 refCon)
{
    IOReturn result = kIOReturnSuccess;

    if (!isInactive()) {
        if (notificationMessage == 0) {
            notificationMessage = (IOAudioNotificationMessage *)IOMallocAligned(sizeof(IOAudioNotificationMessage), sizeof (IOAudioNotificationMessage *));
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
    
        // notificationMessage->type = type;				// ignored now that we have the generic sendChangeNotification routine
        notificationMessage->ref = refCon;
    } else {
        result = kIOReturnNoDevice;
    }
    
    return result;
}

void IOAudioControlUserClient::sendValueChangeNotification()
{
	return sendChangeNotification(kIOAudioControlValueChangeNotification);
}

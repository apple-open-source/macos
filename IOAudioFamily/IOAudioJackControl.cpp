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

#include <IOKit/audio/IOAudioJackControl.h>
#include <IOKit/audio/IOAudioMuteControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSCollectionIterator.h>

class IOAudioJackControlMuteActionEntry : public OSObject
{
    OSDeclareDefaultStructors(IOAudioJackControlMuteActionEntry)

public:
    IOAudioMuteControl *muteControl;
    IOAudioJackControlAction action;
};

OSDefineMetaClassAndStructors(IOAudioJackControlMuteActionEntry, OSObject)


#define super IOAudioControl

OSDefineMetaClassAndStructors(IOAudioJackControl, IOAudioControl)

IOAudioJackControl *IOAudioJackControl::create(IOAudioJackControlState initialState,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID)
{
    IOAudioJackControl *control;

    control = new IOAudioJackControl;

    if (control) {
        if (!control->init(initialState,
                           channelID,
                           channelName,
                           cntrlID)) {
            control->release();
            control = 0;
        }
    }

    return control;
}

bool IOAudioJackControl::init(IOAudioJackControlState initialState,
                              UInt32 channelID,
                              const char *channelName,
                              UInt32 cntrlID,
                              OSDictionary *properties)
{
    if (!super::init(IOAUDIOCONTROL_TYPE_JACK, (UInt32)initialState, channelID, channelName, cntrlID, properties)) {
        return false;
    }

    return true;
}

void IOAudioJackControl::free()
{
    if (muteActions) {
        muteActions->release();
        muteActions = 0;
    }

    super::free();
}

void IOAudioJackControl::addMuteControl(IOAudioMuteControl *muteControl, IOAudioJackControlAction action)
{
    IOAudioJackControlMuteActionEntry *entry;
    
    if (!muteControl) {
        return;
    }

    if (!muteActions) {
        muteActions = OSArray::withCapacity(1);
    }

    if (!muteActions) {
        return;
    }

    entry = new IOAudioJackControlMuteActionEntry;
    if (!entry) {
        return;
    }

    entry->muteControl = muteControl;
    entry->action = action;

    muteActions->setObject(entry);

    entry->release();
}

void IOAudioJackControl::valueChanged()
{
    if (muteActions) {
        OSCollectionIterator *iterator;
        IOAudioJackControlMuteActionEntry *entry;
        
        iterator = OSCollectionIterator::withCollection(muteActions);
        if (iterator) {
            while (entry = (IOAudioJackControlMuteActionEntry *)iterator->getNextObject()) {
                if (entry->muteControl) {
                    IOAudioJackControlState state = getState();

                    if (((state == kAudioJackInserted) && (entry->action == kMuteOnInsertion))
                     || ((state == kAudioJackRemoved) && (entry->action == kMuteOnRemoval))) {
                        entry->muteControl->setValue(1);
                    } else {
                        entry->muteControl->setValue(0);
                    }
                }
            }
            iterator->release();
        }
    }
}

IOAudioJackControlState IOAudioJackControl::getState()
{
    return (IOAudioJackControlState)getValue();
}

void IOAudioJackControl::setState(IOAudioJackControlState newState)
{
    setValue((UInt32)newState);
}
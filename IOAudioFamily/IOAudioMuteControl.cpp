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

#include <IOKit/audio/IOAudioMuteControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#define super IOAudioControl

OSDefineMetaClassAndStructors(IOAudioMuteControl, IOAudioControl)

IOAudioMuteControl *IOAudioMuteControl::create(bool initialValue,
                                               UInt32 channelID,
                                               const char *channelName,
                                               UInt32 cntrlID)
{
    IOAudioMuteControl *control;

    control = new IOAudioMuteControl;

    if (control) {
        if (!control->init(initialValue, channelID, channelName, cntrlID)) {
             control->release();
             control = 0;
        }
    }

    return control;
}

bool IOAudioMuteControl::init(bool initialValue,
                              UInt32 channelID,
                              const char *channelName,
                              UInt32 cntrlID,
                              OSDictionary *properties)
{
    master = false;
    return super::init(IOAUDIOCONTROL_TYPE_MUTE, initialValue ? 1 : 0, channelID, channelName, cntrlID, properties);
}

void IOAudioMuteControl::setMaster(bool isMaster)
{
    master = isMaster;
    //setProperty(IOAUDIOCONTROL_MASTER_KEY, isMaster, 8);
}

bool IOAudioMuteControl::isMaster()
{
    return master;
}

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

#ifndef _IOKIT_IOAUDIOJACKCONTROL_H
#define _IOKIT_IOAUDIOJACKCONTROL_H

#include <IOKit/audio/IOAudioControl.h>

typedef enum _IOAudioJackControlState {
    kAudioJackRemoved = 0,
    kAudioJackInserted = 1
} IOAudioJackControlState;

typedef enum _IOAudioJackControlAction
{
    kMuteOnInsertion = 0,
    kMuteOnRemoval = 1
} IOAudioJackControlAction;

class IOAudioMuteControl;

/*!
 * @class IOAudioJackControl
 * @abstract
 * @discussion
 */

class IOAudioJackControl : public IOAudioControl
{
    OSDeclareDefaultStructors(IOAudioJackControl)

public:
    /*!
     * @function create
     * @abstract Allocates a new jack control with the given attributes
     * @param initialState The initial state of the jack
     * @param channelID The ID of the channel(s) that the jack contains.  Common IDs are located in IOAudioFTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioTypes.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls.
     * @result Returns the newly allocated IOAudioJackControl
     */
    static IOAudioJackControl *create(IOAudioJackControlState initialState,
                                      UInt32 channelID,
                                      const char *channelName = 0,
                                      UInt32 cntrlID = 0);

    /*!
     * @function init
     * @abstract Initializes a newly allocated IOAudioJackControl with the given attributes
     * @param initialState The initial state of the jack
     * @param channelID The ID of the channel(s) that the jack contains.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioTypes.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls.
     * @param properties Standard property list passed to the init() function of any new IOService.  This dictionary
     *  gets stored in the registry entry for this service.
     * @result Returns true on success
     */
    virtual bool init(IOAudioJackControlState initialState,
                      UInt32 channelID,
                      const char *channelName = 0,
                      UInt32 cntrlID = 0,
                      OSDictionary *properties = 0);

    virtual void free();

    virtual void addMuteControl(IOAudioMuteControl *muteControl, IOAudioJackControlAction action);

    virtual IOAudioJackControlState getState();
    virtual void setState(IOAudioJackControlState newState);

protected:

    OSArray *muteActions;
    
    /*!
     * @function valueChanged
     * @abstract Called when the value has changed for the control
     * @discussion This member function performs the muting or unmuting of the registered IOAudioMuteControls
     */
    virtual void valueChanged();

    
};

#endif /* _IOKIT_IOAUDIOJACKCONTROL_H */
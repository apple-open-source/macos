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

#ifndef _IOKIT_IOAUDIOMUTECONTROL_H
#define _IOKIT_IOAUDIOMUTECONTROL_H

#include <IOKit/audio/IOAudioControl.h>

/*!
 * @class IOAudioMuteControl
 */

class IOAudioMuteControl : public IOAudioControl
{
    OSDeclareDefaultStructors(IOAudioMuteControl)

protected:
    bool master;

public:
    /*!
     * @function create
     * @abstract Allocates a new mute control with the given attributes
     * @param initialValue The initial value of the control
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioPort.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls
     * @result Returns a newly allocated and initialized mute IOAudioControl
     */
    static IOAudioMuteControl *create(bool initialValue,
                                      UInt32 channelID,
                                      const char *channelName = 0,
                                      UInt32 cntrlID = 0);

    /*!
     * @function init
     * @abstract Initializes a newly allocated IOAudioMuteControl with the given attributes
     * @param initialValue The initial value of the control
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioPort.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls
     * @result Returns truen on success
     */
    virtual bool init(bool initialValue,
                      UInt32 channelID, 
                      const char *channelName = 0,
                      UInt32 cntrlID = 0,
                      OSDictionary *properties = 0);

    /*!
     * @function setMaster
     * @abstract Records whether or not this level control should be changed when the system-wide master
     *  volume is changed
     */
    virtual void setMaster(bool isMaster);

    /*!
     * @function isMaster
     * @abstract Reports whether or not this level control should be changed when the system-wide master
     *  volume is changed
     * @result Returns true if the level control is a master level control
     */
    virtual bool isMaster();
};

#endif /* _IOKIT_IOAUDIOMUTECONTROL_H */

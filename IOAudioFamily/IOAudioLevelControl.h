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

#ifndef _IOKIT_IOAUDIOLEVELCONTROL_H
#define _IOKIT_IOAUDIOLEVELCONTROL_H

#include <IOKit/audio/IOAudioControl.h>

/*!
 * @class IOAudioLevelControl
 * @abstract
 * @discussion
 */

class IOAudioLevelControl : public IOAudioControl
{
    OSDeclareDefaultStructors(IOAudioLevelControl)

protected:
    UInt32	minValue;
    UInt32	maxValue;
    IOFixed	minDB;
    IOFixed	maxDB;
    
    bool master;

public:
    /*!
     * @function create
     * @abstract Allocates a new level control with the given attributes
     * @param initialValue The initial value of the control
     * @param minValue The lowest possible value the control may have 
     * @param maxValue The highest possible value the control may have
     * @param minDB A fixed point representation of the db value matching minValue 
     * @param maxDB A fixed point representation of the db value matching maxValue 
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioTypes.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls.
     * @result Returns a newly allocted and initialized level IOAudioControl
     */
    static IOAudioLevelControl *create(UInt32 initialValue,
                                       UInt32 minValue,
                                       UInt32 maxValue,
                                       IOFixed minDB,
                                       IOFixed maxDB,
                                       UInt32 channelID,
                                       const char *channelName = 0,
                                       UInt32 cntrlID = 0);

    /*!
     * @function init
     * @abstract Initializes a newly allocated IOAudioLevelControl with the given attributes
     * @param initialValue The initial value of the control
     * @param minValue The lowest possible value the control may have 
     * @param maxValue The highest possible value the control may have
     * @param minDB A fixed point representation of the db value matching minValue 
     * @param maxDB A fixed point representation of the db value matching maxValue 
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioTypes.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls.
     * @param properties Standard property list passed to the init() function of any new IOService.  This dictionary
     *  gets stored in the registry entry for this service.
     * @result Returns true on success
     */
    virtual bool init(UInt32 initialValue,
                      UInt32 minValue,
                      UInt32 maxValue,
                      IOFixed minDB,
                      IOFixed maxDB,
                      UInt32 channelID,
                      const char *channelName = 0,
                      UInt32 cntrlID = 0,
                      OSDictionary *properties = 0);

    /*!
     * @function setMinValue
     * @abstract Sets the minimum value the control may have
     * @param minValue The minimum value for the control
     */
    virtual void setMinValue(UInt32 minValue);

    virtual UInt32 getMinValue();
     
    /*!
     * @function setMaxValue
     * @abstract Sets the maximum value the control may have
     * @param maxValue The maximum value for the control
     */ 
    virtual void setMaxValue(UInt32 maxValue);

    virtual UInt32 getMaxValue();
     
     /*!
     * @function setMinDB
     * @abstract Sets the minimum value in db that the control may have
     * @discussion This value is represented as an IOFixed value which is a fixed point number.  The IOFixed
     *  type is a 16.16 fixed point value.
     * @param minDB The minimum value in db for the control
     */
    virtual void setMinDB(IOFixed minDB);

    virtual IOFixed getMinDB();
    
    /*!
     * @function setMaxDB
     * @abstract Sets the maximum value in db that the control may have
     * @discussion This value is represented as an IOFixed value which is a fixed point number.  The IOFixed
     *  type is a 16.16 fixed point value.
     * @param maxDB The maximum value in db for the control
     */
    virtual void setMaxDB(IOFixed maxDB);

    virtual IOFixed getMaxDB();

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

#endif /* _IOKIT_IOAUDIOLEVELCONTROL_H */

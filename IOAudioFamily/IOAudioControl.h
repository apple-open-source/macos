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

#ifndef _IOKIT_IOAUDIOCONTROL_H
#define _IOKIT_IOAUDIOCONTROL_H

#include <IOKit/IOService.h>

class IOAudioPort;
class OSDictionary;
class OSSet;
class IOAudioUserClient;
class IOAudioControlUserClient;
class IOWorkLoop;
class IOCommandGate;

/*!
 * @class IOAudioControl
 * @abstract Represents any controllable attribute of an IOAudioPort.
 * @discussion An IOAudioControl instance will belong to one IOAudioPort.
 *
 *  When its value changes, it sends a notification to any user clients/applications that have requested
 *  them.  It also forwards the value change on to the IOAudioPort which in turn forwards it on to the
 *  IOAudioDevice to make the change in the hardware.
 *
 *  A base IOAudioControl contains a type, a minimum and maximum value, an actual value and a channel ID
 *  representing the channel(s) which the control acts on.  It may also contain a readable format for the
 *  name of the channel as well as a control ID that can be used to identify unique controls.  There are
 *  defines for the channel ID and common channel names in IOAudioTypes.h.  The channel ID values are prefixed
 *  with 'IOAUDIOCONTROL_CHANNEL_ID_' and the common channel names are prefixed with
 *  'IOAUDIOCONTROL_CHANNEL_NAME_'.  All of the attributes of the IOAudioControl are stored in the registry.
 *  The key used for each attribute is defined in IOAudioTypes.h with the define matching the following
 *  pattern: 'IOAUDIOCONTROL_<attribute name>_KEY'.  For example: IOAUDIOCONTROL_CHANNEL_ID_KEY.
 *
 *  Currently there are two types of controls: Level and Mute.  However there will be more coming in the future
 *  and it is certaintly reasonable for driver writers to create their own types as needed.  One of the upcoming
 *  types of controls will be a jack control type that can be used to represent the state of a jack on the
 *  hardware (i.e. connector inserted in the jack or not).  It will have the capability to mute or unmute other
 *  controls in response to changes in jack state.
 *
 *  For level controls, the range of integer values must be represented as a linear db scale.  There must also
 *  be a minimum and maximum db value specified so that applications know what the range means.
 *
 *  Note: The extra capabilities needed for level controls may be moved out into a subclass.  It appears that
 *  jack controls also need an extra set of capabilities and it probably makes the most sense to do that in
 *  separate subclasses.
 */
class IOAudioControl : public IOService
{
    friend class IOAudioPort;
    friend class IOAudioDevice;
    
    OSDeclareDefaultStructors(IOAudioControl)

protected:
    /*! @var audioPort The IOAudioPort to which this IOAudioControl belongs. */
    IOAudioPort		*audioPort;

    IOWorkLoop 		*workLoop;
    IOCommandGate	*commandGate;

    /*! @var controlID An optional identifier that can be used to identify the control */
    UInt32 		controlID;
    UInt32		channelID;
    /*! @var value The current value of the control */
    UInt32		value;

    /*! @var clients A list of user clients that have requested value change notifications */ 
    OSSet		*userClients;

public:
    /*!
     * @function withAttributes
     * @abstract Allocates a new IOAudioControl with the given attributes
     * @discussion This is a common static allocator used by both createLevelControl() and createMuteControl().
     * @param type The type of the control.  Common, known types are defined in IOAudioTypes.h.  They currently
     *  consist of IOAUDIOCONTROL_TYPE_LEVEL, IOAUDIOCONTROL_TYPE_MUTE, IOAUDIOCONTROL_TYPE_JACK.
     * @param initialValue The initial value of the control.
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioPort.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls
     * @result Returns a newly allocated and initialized IOAudioControl
     */
    static IOAudioControl *withAttributes(const char *type,
                                          UInt32 initialValue,
                                          UInt32 channelID,
                                          const char *channelName = 0,
                                          UInt32 cntrlID = 0);

    /*!
     * @function init
     * @abstract Initializes a newly allocated IOAudioControl with the given attributes
     * @param type The type of the control.  Common, known types are defined in IOAudioTypes.h.  They currently
     *  consist of IOAUDIOCONTROL_TYPE_LEVEL, IOAUDIOCONTROL_TYPE_MUTE, IOAUDIOCONTROL_TYPE_JACK.
     * @param initialValue The initial value of the control.
     * @param channelID The ID of the channel(s) that the control acts on.  Common IDs are located in IOAudioTypes.h.
     * @param channelName An optional name for the channel.  Common names are located in IOAudioPort.h.
     * @param cntrlID An optional ID for the control that can be used to uniquely identify controls
     * @param properties Standard property list passed to the init() function of any new IOService.  This dictionary
     *  gets stored in the registry entry for this instance.
     * @result Returns true on success
     */
    virtual bool init(const char *type,
                      UInt32 initialValue,
                      UInt32 channelID,
                      const char *channelName = 0,
                      UInt32 cntrlID = 0,
                      OSDictionary *properties = 0);

    /*!
     * @function free
     * @abstract Frees all of the resources allocated by the IOAudioControl.
     * @discussion Do not call this directly.  This is called automatically by the system when the instance's
     *  refcount goes to 0.  To decrement the refcount, call release() on the object.
     */
    virtual void free();

    /*!
     * @function start
     * @abstract Called to start a newly created IOAudioControl
     * @discussion This is called automatically by IOAudioPort when addAudioControl() is called.
     * @param provider The IOAudioPort that owns this control
     * @result Returns true on success
     */
    virtual bool start(IOService *provider);

    /*!
     * @function stop
     * @abstract Called to stop the control when the IOAudioPort is going away
     * @param provider The IOAudioPort that owns this control
     */
    virtual void stop(IOService *provider);
    
    virtual IOWorkLoop *getWorkLoop();
    virtual IOCommandGate *getCommandGate();

    /*!
     * @function newUserClient
     * @abstract Called to create a new user client object for this IOAudioControl instance
     * @discussion This is called automatically by IOKit when a user process opens a connection to this
     *  IOAudioControl.  This is typically done when the user process needs to register for value change
     *  notifications.  This implementation allocates a new IOAudioControlUserClient object.  There is no
     *  need to call this directly.
     * @param task The task requesting the new user client
     * @param securityID Optional security paramater passed in by the client - ignored
     * @param type Optional user client type passed in by the client - ignored
     * @param handler The new IOUserClient * must be stored in this param on a successful completion
     * @result Returns kIOReturnSuccess on success.  May also result kIOReturnError or kIOReturnNoMemory.
     */
    virtual IOReturn newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler);

    /*!
     * @function clientClosed
     * @abstract Called automatically by the IOAudioControlUserClient when a user client closes its
     *  connection to the control
     * @param client The user client object that has disconnected
     */
    virtual void clientClosed(IOAudioControlUserClient *client);

    static IOReturn addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    static IOReturn removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

    virtual IOReturn addUserClient(IOAudioControlUserClient *newUserClient);
    virtual IOReturn removeUserClient(IOAudioControlUserClient *userClient);

    virtual IOReturn setProperties(OSObject *properties);

    /*!
     * @function performValueChange
     * @abstract Makes the call to audioControlValueChanged() and valueChaned() after the control's value changes.
     */
    virtual IOReturn performValueChange(UInt32 newValue);

    virtual IOReturn flushValue();

    static IOReturn setValueAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

    /*!
     * @function setValue
     * @abstract Sets the value for this control
     * @discussion When the control's value is changed, a call is made to valueChanged() which in turn notifies
     *  any user clients of the change and calls audioControlValueChanged() on the provider (IOAudioPort).
     * @param value The new value for this control
     */
    virtual IOReturn setValue(UInt32 value);

    /*!
     * @function getValue Returns the current value of the control
     */
    virtual UInt32 getValue();

    /*!
     * @function setControlID
     * @abstract Sets the controlID for this control
     * @discussion The control ID is an optional attribute that can be used to track IOAudioControls.  A typical
     *  use is for the IOAudioDevice to assign a unique controlID to each control that it creates and then
     *  do a switch statement on the id of the control when it gets an audioControlValueChanged() notification.
     *  Typically the control ID is set when the object is created and doesn't need to be called again.
     * @param cntrlID The control ID for the control
     */
    virtual void setControlID(UInt32 cntrlID);

    /*!
     * @function getControlID Returns the control ID for the control
     */
    virtual UInt32 getControlID();

    virtual void setChannelID(UInt32 newChannelID);
    virtual UInt32 getChannelID();

    virtual void setChannelName(const char *channelName);

protected:
    /*!
     * @function sendValueChangeNotification
     * @abstract Called when the value has changed for the control
     * @discussion This member function sends out the value change notification to the user clients
     */
    virtual void sendValueChangeNotification();
};

#endif /* _IOKIT_IOAUDIOCONTROL_H */


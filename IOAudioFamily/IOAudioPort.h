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

#ifndef _IOKIT_IOAUDIOPORT_H
#define _IOKIT_IOAUDIOPORT_H

#include <IOKit/IOService.h>

class IOAudioDevice;
class IOAudioControl;
class OSDictionary;

/*!
 * @class IOAudioPort
 * @abstract Represents a logical or physical port or functional unit in an audio device.
 * @discussion An IOAudioPort represents an element in the signal chain in the audio device.  It may contain
 *  one or more controls (represented by IOAudioControl) by which different attributes of the port may be
 *  represented and adjusted.
 *
 *  IOAudioPort objects are connected up in the IORegistry in the IOAudioPlane to represent the signal chain of
 *  the device.  They may be connected to other IOAudioPorts as well as IOAudioDMAEngines to indicate they either
 *  feed into or are fed by one of the DMA engines (i.e. they provide input to or take output from the computer).
 */
class IOAudioPort : public IOService
{
    friend class IOAudioDevice;

    OSDeclareDefaultStructors(IOAudioPort)

public:
    /* @var audioDevice The IOAudioDevice that this IOAudioPort belongs to. */
    IOAudioDevice *	audioDevice;
    /* @var audioControls A set containg all of the IOAudioControl instances that belong to the port. */
    OSSet *		audioControls;
    bool		isRegistered;

    /*!
     * @function withAttributes
     * @abstract Allocates a new IOAudioPort instance with the given attributes
     * @discussion This static method allocates a new IOAudioPort and calls initWithAttributes() on it with
     *  the parameters passed in to it.
     * @param portType A readable string representing the type of port.  Common port types are defined in
     *  IOAudioTypes.h and are prefixed with 'IOAUDIOPORT_TYPE'.  Please provide feedback if there are
     *  other common port types that should be included.
     * @param portName A readable string representing the name of the port.  For example: 'Internal Speaker',
     *  'Line Out'.  This field is optional, but useful for providing information to the application/user.
     * @param subType Developer defined readable string representing a subtype for the port. (optional)
     * @param properties Standard property list passed to the init of any new IOService.  This dictionary
     *  gets stored in the registry for this instance. (optional)
     * @result Returns the newly allocated and initialized IOAudioPort instance.
     */
    static IOAudioPort *withAttributes(const char *portType, const char *portName = 0, const char *subType = 0, OSDictionary *properties = 0);

    /*!
     * @function initWithAttributes
     * @abstract Initializes a newly allocated IOAudioPort instance with the given attributes
     * @discussion The properties parameter is passed on the superclass' init().  The portType, subType
     *  and properties parameters are optional, however portType is recommended.
     * @param portType A readable string representing the type of port.  Common port types are defined in
     *  IOAudioTypes.h and are prefixed with 'IOAUDIOPORT_TYPE'.  Please provide feedback if there are
     *  other common port types that should be included.
     * @param portName A readable string representing the name of the port.  For example: 'Internal Speaker',
     *  'Line Out'.  This field is optional, but useful for providing information to the application/user.
     * @param subType Developer defined readable string representing a subtype for the port. (optional)
     * @param properties Standard property list passed to the init of any new IOService.  This dictionary
     *  gets stored in the registry for this instance. (optional)
     * @result Returns true on success.
     */
    virtual bool initWithAttributes(const char *portType, const char *portName = 0, const char *subType = 0, OSDictionary *properties = 0);

    /*!
     * @function free
     * @abstract Frees all of the resources allocated by the IOAudioPort.
     * @discussion Do not call this directly.  This is called automatically by the system when the instance's
     *  refcount goes to 0.  To decrement the refcount, call release() on the object.
     */
    virtual void free();

    /*!
     * @function start
     * @abstract Called to start a newly created IOAudioPort.
     * @discussion This is called automatically by IOAudioDevice when attachAudioPort() is called.
     * @param provider The IOAudioDevice that owns this port
     * @result Returns true on success
     */
    virtual bool start(IOService *provider);

    /*!
     * @function stop
     * @abstract Called when the IOAudioDevice is stopping when it is no longer available.
     * @discussion This method calls deactivateAudioControls() to shut down all of the controls associated with
     *  this port.
     * @param provider The IOAudioDevice that owns this port
     */
    virtual void stop(IOService *provider);

    virtual void registerService(IOOptionBits options = 0);

    virtual IOAudioDevice *getAudioDevice();

    /*!
     * @function addAudioControl
     * @abstract Adds a newly created IOAudioControl instance to the port.
     * @discussion This method is responsible for starting the new IOAudioControl and adding it to the internal
     *  audioControls array.
     * @param control A newly created IOAudioControl instance that should belong to this port.
     * @result Returns true on successfully staring the IOAudioControl.
     */
    virtual bool addAudioControl(IOAudioControl *control);

    /*!
     * @function deactivateAudioControls
     * @abstract Called to shut down all of the audio controls for this port.
     * @discussion This will stop all of the audio controls and release them so that the instances may be
     *  freed.  This is called from the free() method.
     */
    virtual void deactivateAudioControls();

    /*!
     * @function performAudioControlValueChange
     * @abstract Called when one of the port's audio control's value changes.
     * @discussion This implementation simply calls audioControlValueChanged() on the port's IOAudioDevice.
     * @param control The IOAudioControl who's value has changed.
     */
    virtual IOReturn performAudioControlValueChange(IOAudioControl *control, UInt32 newValue);
};

#endif /* _IOKIT_IOAUDIOPORT_H */

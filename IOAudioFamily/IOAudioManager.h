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

#ifndef _IOAUDIOMANAGER_H
#define _IOAUDIOMANAGER_H

/*!
 * @header AudioManager
 * Central location for handling frequently used audio values
 */
#include <IOKit/IOService.h>

#include <IOKit/IONotifier.h>
#include <IOKit/IOLocks.h>

class IOAudioDevice;
class OSSet;

/*! @defined MASTER_VOLUME_MIN */
#define MASTER_VOLUME_MIN	0

/*! @defined MASTER_VOLUME_MAX */
#define MASTER_VOLUME_MAX	65535

/*! @defined kMasterVolumeLeft */
#define kMasterVolumeLeft	"MasterVolumeLeft"

/*! @defined kMasterVolumeRight */
#define kMasterVolumeRight	"MasterVolumeRight"

/*! @defined kMasterMute */
#define kMasterMute		"MasterMute"

/*! @defined kMasterVolumeIncrement */
#define kMasterVolumeIncrement	"MasterVolumeIncrement"

/*!
 * @class IOAudioManager
 * Object for handling getting and setting of common audio properties and registering
 * interest in changes to these.
 */
class IOAudioManager : public IOService
{
    OSDeclareDefaultStructors(IOAudioManager);

private:
    static IOAudioManager	*amInstance;
    UInt16		masterVolumeLeft;
    UInt16		masterVolumeRight;
    bool		masterMute;
    UInt16		masterVolumeIncrement;

    OSSet *		audioDevices;
    IONotifier *	publishNotify;
    IOLock *		driverLock;
    
    int old_mach_trap_arg_count;
    int (*old_mach_trap_function)(void);
    
public:
   /*!
    * @function sharedInstance
    * @result IOAudioManager
    */
    static IOAudioManager *sharedInstance();

    /*!
     * @function init
     * @abstract Set default values and initialize the Audio Manager
     * @param properties Passed to inherited init method
     * @result bool TRUE is successful, false otherwise
     */
    virtual bool init(OSDictionary *properties);

    /*!
     * @function free
     * @abstract Release resources allocated and break notify connections
     * @result void
     */
    virtual void free();

    /*!
     * @function start
     * @param provider
     * @result bool TRUE if successful, FALSE otherwise
     */
    virtual bool start(IOService *provider);

    /*!
     * @function setProperties
     * @param properties
     * @result IOReturn kIOReturnSuccess if successful, kIOReturnBadArgument otherwise
     */
    virtual IOReturn setProperties(OSObject *properties);

    virtual void removeAudioDevice(IOAudioDevice *device);
    
    /*!
     * @function getMasterVolumeLeft
     * @result UInt16 left master volume
     */   
    virtual UInt16 getMasterVolumeLeft();
    
    /*!
     * @function getMasterVolumeRight
     * @result UInt16 right master volume
     */
    virtual UInt16 getMasterVolumeRight();

    /*!
     * @function setMasterVolumeLeft
     * @param newMasterVolumeLeft
     * @result UInt16 previous value
     */
    virtual UInt16 setMasterVolumeLeft(UInt16 newMasterVolumeLeft);

    /*!
     * @function setMasterVolumeRight
     * @param newMasterVolumeRight
     * @result UInt16 previous value
     */
    virtual UInt16 setMasterVolumeRight(UInt16 newMasterVolumeRight);

    /*!
     * @function getMasterMute
     * @result bool master mute
     */
    virtual bool getMasterMute();

    /*!
     * @function setMasterMute
     * @param newMasterMute
     * @result bool previous value
     */
    virtual bool setMasterMute(bool newMasterMute);

    /*!
     * @function getMasterVolumeIncrement
     * @result UInt16 master volume increment
     */
    virtual UInt16 getMasterVolumeIncrement();

    /*!
     * @function setMasterVolumeIncrement
     * @param newMasterVolumeIncrement
     * @result UInt16 previous value
     */
    virtual UInt16 setMasterVolumeIncrement(UInt16 newMasterVolumeIncrement);

    /*!
     * @function incrementMasterVolume
     * @abstract Increment master right and master left volume by master volume increment value
     * @result UInt16 The larger of master volume right and master volume left
     */
    virtual UInt16 incrementMasterVolume();

    /*!
     * @function decrementMasterVolume
     * @abstract Decrement master right and master left volume by master volume increment value
     * @result UInt16 The larger of master volume right and master volume left
     */
    virtual UInt16 decrementMasterVolume();

    /*!
     * @function toggleMasterMute
     * @abstract If muted, unmute, and vice versa
     * @result bool New value of mute
     */
    virtual bool toggleMasterMute();

private:
    static bool audioPublishNotificationHandler(IOAudioManager *self,
                                                void *ref,
                                                IOService *newService);
    virtual bool registerAudioDevice(IOAudioDevice *device);

};

#endif /* _IOAUDIOMANAGER_H */

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

/*!
 * @header IOAudioDevice
 */

#ifndef _IOKIT_IOAUDIODEVICE_H
#define _IOKIT_IOAUDIODEVICE_H

#include <IOKit/IOService.h>

#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>

class IOAudioDMAEngine;
class IOAudioStream;
class IOAudioPort;
class IOAudioControl;
class OSDictionary;
class OSSet;
class IOTimerEventSource;
class IOCommandGate;

/*!
 * @class IOAudioDevice
 * @abstract Abstract base class for a single piece of audio hardware.  The IOAudioDevice provides
 *  the central coordination point for an audio driver.
 * @discussion An audio driver is required to subclass IOAudioDevice in order to provide
 *  working audio to the system.  The subclass is responsible for mapping all hardware device
 *  resources from the service provider nub.  It must control access to the hardware so that the
 *  hardware doesn't get into an inconsistent state.  It is possible that different threads may make
 *  requests of the hardware at the same time.
 *
 *  It must identify and create all IOAudioDMAEngines that are not automatically created by the system
 *  (i.e. those that are not matched and instantiated by IOKit directly).
 *
 *  The IOAudioDevice subclass must enumerate and create all IOAudioPorts and IOAudioControls to match
 *  the device capabilities.  It must also connect up the IOAudioPorts and IOAudioDMAEngines to match the
 *  internal signal chain of the device.
 *
 *  It must also execute control value chages when requested by the system (i.e. volume adjustments).
 *
 *  The IOAudioDevice class provides timer services that allow different elements in the audio driver
 *  to receive timer notifications as needed.  These services were designed with the idea that most
 *  timed events in a typical audio driver need to be done at least as often as a certain interval.
 *  Further, it is designed with the idea that being called more often than the specified interval
 *  doesn't hurt anything - and in fact may help.  With this in mind, the timer services provided
 *  by the IOAudioDevice class allow different targets to register for timer callbacks at least as
 *  often as the specified interval.  The actual interval will be the smallest of the intervals of
 *  all of the callbacks.  This way, we avoid the overhead of having many timers in a single audio
 *  device.  As an example, each output IOAudioDMAEngine has a timer to run the erase head.  It doesn't hurt
 *  to have the erase head run more often.  Also, a typical IOAudioDevice subclass may need to run a timer
 *  to check for device state changes (e.g. jack insertions).
 */

class IOAudioDevice : public IOService
{
    OSDeclareDefaultStructors(IOAudioDevice)

protected:
    IOWorkLoop				*workLoop;
    IOCommandGate			*commandGate;
    IOTimerEventSource		*timerEventSource;

    bool			duringStartup;
    bool			wakingFromSleep;
    bool			familyManagePower;

    /*! @var audioDMAEngines The set of IOAudioDMAEngine objects vended by the IOAudioDevice. */
    OSSet *			audioDMAEngines;
    /*! @var timerEvents The set of timer events in use by the device.  
     *  @discussion The key for the dictionary is the target of the event.  This means that a single target may
     *   have only a single event associated with it.
     */
    OSDictionary *		timerEvents;
    /*! @var audioPorts The set of IOAudioPort objects associated with the IOAudioDevice */
    OSSet *			audioPorts;
    OSSet *			masterControls;

    /*! @var minimumInterval The smallest timer interval requested by all timer event targets. */
    AbsoluteTime		minimumInterval;
    /*! @var previousTimerFire The time of the last timer event.
     *  @discussion This is used to schedule the next timer event.
     */
    AbsoluteTime		previousTimerFire;

public:
    /*! @var gIOAudioPlane A static IORegistryPlane representing the new IOAudioPlane that the IOAudioFamily uses
     *   to represent the signal chain of the device.
     */
    static const IORegistryPlane *gIOAudioPlane;

    /*!
     * @function init
     * @abstract Initialize a newly created instance of IOAudioDevice.
     * @discussion This implementation initializes all of the data structures and locks used by the
     *  IOAudioDevice.  These include: audioDMAEngines, audioPorts, dmaEnginesLock, timerLock and portsLock.
     *  A subclass that overrides this method must call the superclass' implementation.
     * @param properties An OSDictionary of the device properties that gets passed to super::init and set
     *  in the IORegistry.
     * @result true if initialization was successful
     */
    virtual bool init(OSDictionary *properties);

    /*!
     * @function free
     * @abstract free resources used by the IOAudioDevice instance
     * @discussion This method will deactivate all audio DMA engines and release the audioDMAEngines OSSet.
     *  It will also deactivate all of the audio ports and release the audioPorts OSSet.  It will release
     *  the timerEvents OSDictionary as well as cancel any outstanding timer callbacks.  It will free all
     *  of the device locks: dmaEnginesLock, timerLock, portsLock.
     *
     *  Do not call this directly.  This is called automatically by the system when the instance's
     *  refcount goes to 0.  To decrement the refcount, call release() on the object.
     */
     
    virtual void free();

    virtual bool initHardware(IOService *provider);

    /*
     * @function start
     * @abstract This method is responsible for starting the service and starting to vend the device's
     *  functionality to the rest of the system.
     * @discussion The start() implementation in IOAudioDevice simply calls start() on its superclass and
     *  registers itself as a service.  This serves to make the IOAudioDevice visible in the IORegistry.
     *  Typically, a subclass will override this method and perform most of its initialization there.
     *  The subclass implementation must call the superclass' start() method.  The subclass can perform
     *  the resource mapping from the device provider.  It must create (or find) the IOAudioDMAEngines and
     *  register them with the superclass.  It must also create all of the IOAudioPorts and IOAudioControls
     *  and wire them up using attachAudioPort().
     * @param provider This is the service provider nub that provides access to the hardware resources.
     * @result Returns true on success
     */
    virtual bool start(IOService *provider);

    /*!
     * @function stop
     * @abstract This is responsible for stopping the device after the system is done with it (or
        as it the device is removed from the system).
     * @discussion The IOAudioDevice implentation of stop() deactivates all of the audio DMA engines and audio ports.
     *  This deactivation causes all of the DMA engines to get stopped and all of the DMA engine and port resources and
     *  objects to be released.  A subclass' implementation may not need to shut down hardware here, but also
     *  may not need to do anything.
     * @param The service provider nub for the device.
     */
    virtual void stop(IOService *provider);
    
    virtual void setFamilyManagePower(bool manage);
    
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService *device);
    
    static IOReturn setPowerStateSleepAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    static IOReturn setPowerStateWakeAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    
    virtual IOReturn setPowerStateSleep();
    virtual IOReturn setPowerStateWake();
    
    virtual IOReturn performDeviceSleep();
    virtual IOReturn performDeviceWake();
    
    virtual IOWorkLoop *getWorkLoop();
    virtual IOCommandGate *getCommandGate();

    /*!
     * @function activateAudioDMAEngine
     * @abstract This simply calls activateAudioDMAEngine(IOAudioDMAEngine *dmaEngine, bool shouldStartDMAEngine) with
     *  a value of true for shouldStartDMAEngine.
     * @param dmaEngine The IOAudioDMAEngine instance to be activated.  It is treated as a newly allocated instance.
     * @result Returns true if the DMA engine was successfully activated.
     */
    virtual bool activateAudioDMAEngine(IOAudioDMAEngine *dmaEngine);

    /*!
     * @function activateAudioDMAEngine
     * @abstract This is called to add a new IOAudioDMAEngine object to the IOAudioDevice.
     * @discussion Once the IOAudioDMAEngine has been activated by this function, it is ready
     *  to begin moving audio data.  This should be called either during the subclass' start()
     *  implementation for each IOAudioDMAEngine the device creates.  Or it should be called by
     *  the IOAudioDMAEngine itself if the DMA engine was automatically created by IOKit's matching
     *  process.  The system won't be able to properly track and control IOAudioDMAEngines if
     *  they are not activated though this function.
     *  This implementation will retain the IOAudioDMAEngine before it is placed into the audioDMAEngines
     *  OSSet.  When the DMA engine is deactivated, the IOAudioDMAEngine will be released.  If the IOAudioDevice
     *  subclass is passing a newly instantiated IOAudioDMAEngine, it will need to release the DMA engine after
     *  it has been activated.  This will insure that the refCount on the DMA engine is correct when it gets
     *  deactivated when the driver is stopped.
     * @param dmaEngine The IOAudioDMAEngine instance to be activated.
     * @param shouldStartDMAEngine If true, the DMA engien is treated as a newly allocated IOAudioDMAEngine
     *  instance and is appropriately attached and started according to IOKit convention.
     * @result Returns true if the DMA engine was successfully activated.
     */
    virtual bool activateAudioDMAEngine(IOAudioDMAEngine *dmaEngine, bool shouldStartDMAEngine);

    /*!
     * @function deactivateAudioDMAEngine
     * @abstract Stops the IOAudioDMAEngine, removes it from the device and releases it.
     * @discussion If no other retains have been done on the DMA engine, it will be freed when this call finishes.
     * @param dmaEngine The IOAudioDMAEngine instance to be deactivated.
     */
    virtual void deactivateAudioDMAEngine(IOAudioDMAEngine *dmaEngine);

    /*!
     * @function deactivateAudioDMAEngines
     * @abstract Deactivates all of the DMA engines in the device.
     * @discussion This is called by the stop() and free() methods in IOAudioDevice to completely
     *  shut down all DMA engines as the driver is being shut down.
     */
    virtual void deactivateAudioDMAEngines();

    /*!
     * @function attachAudioPort
     * @abstract Adds the port to the IOAudioDevice's list of ports and attaches the port to its parent
     *  and attaches the child to the port.
     * @discussion This method provides the functionality to represent the device's signal chain in the
     *  IOAudioPlane in the IORegistry.  An IOAudioPort's parent(s) are before it in the signal chain
     *  and its children are after it.  This method may be called multiple times for a single IOAudioPort.
     *  This is necessary when there are many children or parents.  Once a relationship is made, it is not
     *  necessary to make the reverse relationship.  A NULL value may be passed in for either the parent
     *  or child or both.
     *  The IOAudioPort passed in should be a newly allocated IOAudioPort instance.  This method will
     *  appropriately attach and start the port object.
     * @param port The newly created IOAudioPort instance to be activated.
     * @param parent A parent IOAudioPort or IOAudioDMAEngine of the given port.
     * @param child A child IOAudioPort or IOAudioDMAEngine of the given port.
     * @result Returns true when the port has been successfully added and attached.
     */
    virtual bool attachAudioPort(IOAudioPort *port, IORegistryEntry *parent, IORegistryEntry *child);

    /*!
     * @function deactivateAudioPorts
     * @abstract Deactivates all of the ports in the device.
     * @discussion This is called by the stop() and free() methods in IOAudioDevice to completely
     *  shut down all ports as the driver is being shut down.
     */
    virtual void deactivateAudioPorts();

    /*!
     * @function flushAudioControls
     * @abstract Forces performAudioControlValueChange() to get called for each audio control in this device.
     * @discussion This can be used to force the hardware to get updated with the current value of each control.
     */
    virtual void flushAudioControls();

    /*!
     * @function performAudioControlValueChange
     * @abstract Called when an IOAudioControl connected to one of the device's IOAudioPorts changes.
     * @discussion A subclass must override this method when it has created IOAudioControls.  The
     *  subclass' implementation must then update the hardware to reflect the new control value.
     * @param control The IOAudioControl instance whose value has changed.
     */
    virtual IOReturn performAudioControlValueChange(IOAudioControl *control, UInt32 value);

    virtual IOReturn performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);

    /*!
     * @typedef TimerEvent
     * @abstract Generic timer event callback for IOAudioDevice timer targets
     * @discussion TimerEvent callback function takes two arguments; the target of
     *  the timer event and the IOAudioDevice sending the event.
     */
    typedef void (*TimerEvent)(OSObject *target, IOAudioDevice *audioDevice);

    /*!
     * @function addTimerEvent
     * @abstract Adds a TimerEvent callback for the given target called at least as often
     *  as specified in interval.
     * @discussion The frequency of the timer event callbacks will be the smallest interval
     *  specified by all targets.  Only one interval and callback may be specified per target.
     *  If a addTimerEvent is called twice with the same target, the second one overrides the
     *  first.  There is currently a bug triggered if the first call had the smallest interval.
     *  In that case, that smallest interval would still be used.
     * @param target This parameter is the target object of the TimerEvent.
     * @param event The callback function called each time the timer fires.
     * @param interval The callback will be called at least this often.
     */
    virtual IOReturn addTimerEvent(OSObject *target, TimerEvent event, AbsoluteTime interval);

    /*!
     * @function removeTimerEvent
     * @abstract Removes the timer event for the given target.
     * @discussion If the interval for the target to be removed is the smallest interval,
     *  the timer interval is recalculated based on the remaining targets.  The next fire
     *  time is readjusted based on the new interval compared to the last fire time.
     * @param target The target whose timer event will be removed.
     */
    virtual void removeTimerEvent(OSObject *target);
    
    virtual void clearTimerEvents();

    /*!
     * @function setMasterVolumeLeft
     * @abstract
     */
    virtual void setMasterVolumeLeft(UInt16 newMasterVolumeLeft);

    /*!
     * @function setMasterVolumeRight
     * @abstract
     */
    virtual void setMasterVolumeRight(UInt16 newMasterVolumeRight);

    /*
     * @function setMasterMute
     * @abstract
     */
    virtual void setMasterMute(bool newMasterMute);

protected:
    /*!
     * @function timerFired
     * @abstract Internal static function called when the timer fires.
     * @discussion This function simply calls dispatchTimerEvents() on the IOAudioDevice to do just that.
     * @param void The IOAudioDevice instance that initiated the timer callback.
     */
    static void timerFired(OSObject *target, IOTimerEventSource *sender);

    /*!
     * @function dispatchTimerEvents
     * @abstract Called by timerFired() to cause the timer event callbacks to be called.
     * @discussion This method iterates through all of the timer event targets and calls
     *  the callback on each.
     */
    virtual void dispatchTimerEvents(bool force);

    /*!
     * @function setDeviceName
     * @abstract Sets the name of the device
     * @discussion This method should be called during initialization or startup.  The device
     *  name is used by the audio device API to identify the particular piece of hardware.
     */
    virtual void setDeviceName(const char *deviceName);

    /*!
     * @function setManufacturerName
     * @abstract Sets the manufacturer name of the device
     * @discussion This method should be called during initialization or startup.
     */
    virtual void setManufacturerName(const char *manufacturerName);

    virtual void addMasterControl(IOAudioControl *masterControl);
};

#endif /* _IOKIT_IOAUDIODEVICE_H */

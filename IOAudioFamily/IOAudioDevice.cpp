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

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioDMAEngine.h>
#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioMuteControl.h>
#include <IOKit/audio/IOAudioManager.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>

#define NUM_POWER_STATES	2

class IOAudioTimerEvent : public OSObject
{
    friend class IOAudioDevice;

    OSDeclareDefaultStructors(IOAudioTimerEvent)

protected:
    OSObject *	target;
    IOAudioDevice::TimerEvent event;
    AbsoluteTime interval;
};

OSDefineMetaClassAndStructors(IOAudioTimerEvent, OSObject)

class IOAudioDMAEngineEntry : public OSObject
{
    friend class IOAudioDevice;

    OSDeclareDefaultStructors(IOAudioDMAEngineEntry);

protected:
    IOAudioDMAEngine *audioDMAEngine;
    bool shouldStopDMAEngine;
};

OSDefineMetaClassAndStructors(IOAudioDMAEngineEntry, OSObject)

#define super IOService
OSDefineMetaClassAndStructors(IOAudioDevice, IOService)

const IORegistryPlane *IOAudioDevice::gIOAudioPlane = 0;

bool IOAudioDevice::init(OSDictionary *properties)
{
    if (!super::init(properties)) {
        return false;
    }

    if (!gIOAudioPlane) {
        gIOAudioPlane = IORegistryEntry::makePlane(kIOAudioPlane);
    }

    audioDMAEngines = OSSet::withCapacity(2);
    if (!audioDMAEngines) {
        return false;
    }

    audioPorts = OSSet::withCapacity(1);
    if (!audioPorts) {
        return false;
    }

    workLoop = IOWorkLoop::workLoop();
    if (!workLoop) {
        return false;
    }
    
    wakingFromSleep = false;
    familyManagePower = true;
    
    return true;
}

void IOAudioDevice::free()
{
    if (audioDMAEngines) {
        deactivateAudioDMAEngines();
        audioDMAEngines->release();
        audioDMAEngines = 0;
    }

    if (audioPorts) {
        deactivateAudioPorts();
        audioPorts->release();
        audioPorts = 0;
    }
    
    if (masterControls) {
        masterControls->release();
        masterControls = 0;
    }

    if (timerEvents) {
        timerEvents->release();
        timerEvents = 0;
    }

    if (timerEventSource) {
        if (workLoop) {
            workLoop->removeEventSource(timerEventSource);
        }
        
        timerEventSource->release();
        timerEventSource = NULL;
    }
    
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }
    
    super::free();
}

bool IOAudioDevice::initHardware(IOService *provider)
{
    return true;
}

bool IOAudioDevice::start(IOService *provider)
{
    static IOPMPowerState powerStates[2] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    
    if (!super::start(provider)) {
        return false;
    }

    if (!initHardware(provider)) {
        return false;
    }

    if (familyManagePower) {
        PMinit();
        provider->joinPMtree(this);
        
        if (pm_vars != NULL) {
            duringStartup = true;
            registerPowerDriver(this, powerStates, NUM_POWER_STATES);
            changePowerStateTo(1);
            duringStartup = false;
        }
    }

    registerService();

    return true;
}

void IOAudioDevice::stop(IOService *provider)
{
    IOAudioManager *manager;
    
    if (timerEventSource) {
        if (workLoop) {
            workLoop->removeEventSource(timerEventSource);
        }
        
        timerEventSource->release();
        timerEventSource = NULL;
    }

    clearTimerEvents();

    deactivateAudioDMAEngines();
    deactivateAudioPorts();

    if (familyManagePower) {
        PMstop();
    }
    
    if (commandGate) {
        if (workLoop) {
            workLoop->removeEventSource(commandGate);
        }
        
        commandGate->release();
        commandGate = NULL;
    }

    manager = IOAudioManager::sharedInstance();
    if (manager) {
        manager->removeAudioDevice(this);
    }
    
    super::stop(provider);
}

void IOAudioDevice::setFamilyManagePower(bool manage)
{
    familyManagePower = manage;
}

IOReturn IOAudioDevice::setPowerState(unsigned long powerStateOrdinal, IOService *device)
{
    IOReturn result = IOPMCannotRaisePower;
    
#ifdef DEBUG_CALLS
    kprintf("IOAudioDevice[%p]::setPowerState(%lu, %p)\n", this, powerStateOrdinal, device);
    IOLog("IOAudioDevice[%p]::setPowerState(%lu, %p)\n", this, powerStateOrdinal, device);
#endif
    
    if (!duringStartup) {
        if (powerStateOrdinal >= NUM_POWER_STATES) {
            result = IOPMNoSuchState;
        } else if (powerStateOrdinal == 0) {
            IOCommandGate *cg;
            
            cg = getCommandGate();
            
            if (cg) {
                result = cg->runAction(setPowerStateSleepAction);
            }
            
            wakingFromSleep = true;
        } else if ((powerStateOrdinal == 1) && (wakingFromSleep)) {
            IOCommandGate *cg;
            
            wakingFromSleep = false;
            
            cg = getCommandGate();
            
            if (cg) {
                result = cg->runAction(setPowerStateWakeAction);
            }
        }
    }
    
    return result;
}

IOReturn IOAudioDevice::setPowerStateSleepAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    kprintf("IOAudioDevice::setPowerStateSleepAction(%p)\n", owner);
    IOLog("IOAudioDevice::setPowerStateSleepAction(%p)\n", owner);
#endif

    if (owner) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, owner);
        
        if (audioDevice) {
            audioDevice->setPowerStateSleep();
        }
    }
    
    return result;
}

IOReturn IOAudioDevice::setPowerStateWakeAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_CALLS
    kprintf("IOAudioDevice::setPowerStateWakeAction(%p)\n", owner);
    IOLog("IOAudioDevice::setPowerStateWakeAction(%p)\n", owner);
#endif

    if (owner) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, owner);
        
        if (audioDevice) {
            audioDevice->setPowerStateWake();
        }
    }
    
    return result;
}

IOReturn IOAudioDevice::setPowerStateSleep()
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_CALLS
    kprintf("IOAudioDevice[%p]::setPowerStateSleep()\n", this);
    IOLog("IOAudioDevice[%p]::setPowerStateSleep()\n", this);
#endif

    if (audioDMAEngines) {
        OSCollectionIterator *dmaEngineIterator;
        
        dmaEngineIterator = OSCollectionIterator::withCollection(audioDMAEngines);
        
        if (dmaEngineIterator) {
            IOAudioDMAEngine *audioDMAEngine;
            
            while (audioDMAEngine = (IOAudioDMAEngine *)dmaEngineIterator->getNextObject()) {
                if (audioDMAEngine->getState() == kAudioDMAEngineRunning) {
                    audioDMAEngine->stopDMAEngine();
                }
            }
            
            dmaEngineIterator->release();
        }
    }
    
    result = performDeviceSleep();
    
    return kIOReturnSuccess;
}

IOReturn IOAudioDevice::setPowerStateWake()
{
    IOReturn result;
    
#ifdef DEBUG_CALLS
    kprintf("IOAudioDevice[%p]::setPowerStateWake()\n", this);
    IOLog("IOAudioDevice[%p]::setPowerStateWake()\n", this);
#endif

    result = performDeviceWake();
    
    if (result == kIOReturnSuccess) {
        clock_get_uptime(&previousTimerFire);
        SUB_ABSOLUTETIME(&previousTimerFire, &minimumInterval);
        
        if (timerEvents && (timerEvents->getCount() > 0)) {
            dispatchTimerEvents(true);
        }
        
        if (audioDMAEngines) {
            OSCollectionIterator *dmaEngineIterator;
            
            dmaEngineIterator = OSCollectionIterator::withCollection(audioDMAEngines);
            
            if (dmaEngineIterator) {
                IOAudioDMAEngine *audioDMAEngine;
                
                while (audioDMAEngine = (IOAudioDMAEngine *)dmaEngineIterator->getNextObject()) {
                    if (audioDMAEngine->getState() == kAudioDMAEngineRunning) {
                        UInt32 loopCount;
                        // We can't reset the loop count because the HAL get's confused
                        // Once we do stop/start notifications we can remove the code to reset the loop count
                        loopCount = audioDMAEngine->getStatus()->fCurrentLoopCount;
                        audioDMAEngine->resetStatusBuffer();
                        ((IOAudioDMAEngineStatus *)audioDMAEngine->getStatus())->fCurrentLoopCount = loopCount + 1;
                        
                        audioDMAEngine->clearAllSampleBuffers();
                        
                        audioDMAEngine->startDMAEngine();
                    }
                }
                
                dmaEngineIterator->release();
            }
        }
    }
    
    return kIOReturnSuccess;
}

IOReturn IOAudioDevice::performDeviceSleep()
{
    return kIOReturnSuccess;
}

IOReturn IOAudioDevice::performDeviceWake()
{
    return kIOReturnSuccess;
}

IOWorkLoop *IOAudioDevice::getWorkLoop()
{
    return workLoop;
}

IOCommandGate *IOAudioDevice::getCommandGate()
{
    if (!commandGate) {
        IOWorkLoop *wl;
        
        wl = getWorkLoop();
        
        if (wl) {
            commandGate = IOCommandGate::commandGate(this);
            
            if (commandGate) {
                wl->addEventSource(commandGate);
            }
        }
    }
    
    return commandGate;
}

void IOAudioDevice::setDeviceName(const char *deviceName)
{
    if (deviceName) {
        setProperty(IOAUDIODEVICE_NAME_KEY, deviceName);
    }
}

void IOAudioDevice::setManufacturerName(const char *manufacturerName)
{
    if (manufacturerName) {
        setProperty(IOAUDIODEVICE_MANUFACTURER_NAME_KEY, manufacturerName);
    }
}

bool IOAudioDevice::activateAudioDMAEngine(IOAudioDMAEngine *dmaEngine)
{
    return activateAudioDMAEngine(dmaEngine, true);
}

bool IOAudioDevice::activateAudioDMAEngine(IOAudioDMAEngine *dmaEngine, bool shouldStartDMAEngine)
{
    if (!dmaEngine || !audioDMAEngines) {
        return false;
    }

    if (!dmaEngine->attach(this)) {
        return false;
    }

    if (shouldStartDMAEngine) {
        if (!dmaEngine->start(this)) {
            dmaEngine->detach(this);
            return false;
        }
    }

    dmaEngine->deviceStartedDMAEngine = shouldStartDMAEngine;

    audioDMAEngines->setObject(dmaEngine);
    
    dmaEngine->registerService();
    //dmaEngine->attachToParent(getRegistryRoot(), gIOAudioPlane);
    
    return true;
}

void IOAudioDevice::deactivateAudioDMAEngine(IOAudioDMAEngine *dmaEngine)
{
    if (!dmaEngine || !audioDMAEngines) {
        return;
    }

    dmaEngine->retain();
    
    audioDMAEngines->removeObject(dmaEngine);

    // Need to do better than this
    dmaEngine->stopDMAEngine();
    
    if (!isInactive()) {
        dmaEngine->terminate();
    }
    
    dmaEngine->detachAll(gIOAudioPlane);
        
    dmaEngine->release();
}

void IOAudioDevice::deactivateAudioDMAEngines()
{
    IOAudioDMAEngine *dmaEngine;
    
    if (!audioDMAEngines) {
        return;
    }

    while (dmaEngine = OSDynamicCast(IOAudioDMAEngine, audioDMAEngines->getAnyObject())) {
        deactivateAudioDMAEngine(dmaEngine);
    }
}

bool IOAudioDevice::attachAudioPort(IOAudioPort *port, IORegistryEntry *parent, IORegistryEntry *child)
{
    if (!port || !audioPorts) {
        return false;
    }

    if (!port->attach(this)) {
        return false;
    }

    if (!port->start(this)) {
        port->detach(this);
        return false;
    }

    audioPorts->setObject(port);

    port->registerService();

    if (!parent) {
        parent = getRegistryRoot();
    }
    port->attachToParent(parent, gIOAudioPlane);

    if (child) {
        child->attachToParent(port, gIOAudioPlane);
    }

    if (port->audioControls) {
        OSCollectionIterator *iterator;

        iterator = OSCollectionIterator::withCollection(port->audioControls);
        if (iterator) {
            OSObject *control;

            while (control = iterator->getNextObject()) {
                if ((OSDynamicCast(IOAudioLevelControl, control) && ((IOAudioLevelControl *)control)->isMaster()) ||
                    (OSDynamicCast(IOAudioMuteControl, control) && ((IOAudioMuteControl *)control)->isMaster())) {
                    addMasterControl((IOAudioControl *)control);
                }
            }

            iterator->release();
        }
    }

    return true;
}

void IOAudioDevice::deactivateAudioPorts()
{
    OSCollectionIterator *iterator;
    
    if (!audioPorts) {
        return;
    }

    iterator = OSCollectionIterator::withCollection(audioPorts);

    if (iterator) {
        IOAudioPort *port;
        
        while (port = (IOAudioPort *)iterator->getNextObject()) {
            if (!isInactive()) {
                port->terminate();
            }
            port->detachAll(gIOAudioPlane);
        }
        
        iterator->release();
    }
    
    audioPorts->flushCollection();
}

void IOAudioDevice::flushAudioControls()
{
    if (audioPorts) {
        OSCollectionIterator *portIterator;

        portIterator = OSCollectionIterator::withCollection(audioPorts);
        if (portIterator) {
            IOAudioPort *audioPort;

            while (audioPort = (IOAudioPort *)portIterator->getNextObject()) {
                if (OSDynamicCast(IOAudioPort, audioPort)) {
                    if (audioPort->audioControls) {
                        OSCollectionIterator *controlIterator;

                        controlIterator = OSCollectionIterator::withCollection(audioPort->audioControls);

                        if (controlIterator) {
                            IOAudioControl *audioControl;

                            while (audioControl = (IOAudioControl *)controlIterator->getNextObject()) {
                                audioControl->flushValue();
                            }
                            controlIterator->release();
                        }
                    }
                }
            }
            portIterator->release();
        }
    }
}

IOReturn IOAudioDevice::performAudioControlValueChange(IOAudioControl *control, UInt32 value)
{
    return kIOReturnSuccess;
}

IOReturn IOAudioDevice::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    return kIOReturnSuccess;
}

IOReturn IOAudioDevice::addTimerEvent(OSObject *target, TimerEvent event, AbsoluteTime interval)
{
    IOReturn result = kIOReturnSuccess;
    IOAudioTimerEvent *newEvent;
    
#ifdef DEBUG_CALLS
    UInt64 newInt;
    absolutetime_to_nanoseconds(interval, &newInt);
    IOLog("IOAudioDevice::addTimerEvent(%p, %p, %lums)\n", target, event, (UInt32)(newInt/1000000));
#endif

    if (!event) {
        return kIOReturnBadArgument;
    }

    newEvent = new IOAudioTimerEvent;
    newEvent->target = target;
    newEvent->event = event;
    newEvent->interval = interval;

    if (!timerEvents) {
        IOWorkLoop *wl;
        
        timerEvents = OSDictionary::withObjects(&(const OSObject *)newEvent, &(const OSSymbol *)target, 1, 1);
        
        timerEventSource = IOTimerEventSource::timerEventSource(this, timerFired);
        wl = getWorkLoop();
        if (!timerEventSource || !wl || (wl->addEventSource(timerEventSource) != kIOReturnSuccess)) {
            return kIOReturnError;
        }
        timerEventSource->enable();
    } else {
        timerEvents->setObject((OSSymbol *)target, newEvent);
    }

    newEvent->release();
    
    assert(timerEvents);

    if (timerEvents->getCount() == 1) {
        AbsoluteTime nextTimerFire;
        
        minimumInterval = interval;

        assert(timerEventSource);

        clock_get_uptime(&previousTimerFire);
        
        nextTimerFire = previousTimerFire;
        ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
        
        result = timerEventSource->wakeAtTime(nextTimerFire);
        
#ifdef DEBUG_TIMER
        {
            UInt64 nanos;
            absolutetime_to_nanoseconds(minimumInterval, &nanos);
            IOLog("IOAudioDevice::addTimerEvent() - scheduling timer to fire in %lums - previousTimerFire = {%ld,%lu}\n", (UInt32) (nanos / 1000000), previousTimerFire.hi, previousTimerFire.lo);
        }
#endif

        if (result != kIOReturnSuccess) {
            IOLog("IOAudioDevice::addTimerEvent() - error 0x%x setting timer wake time - timer events will be disabled.\n", result);
        }
    } else if (CMP_ABSOLUTETIME(&interval, &minimumInterval) < 0) {
        AbsoluteTime currentNextFire, desiredNextFire;
        
        clock_get_uptime(&desiredNextFire);
        ADD_ABSOLUTETIME(&desiredNextFire, &interval);

        currentNextFire = previousTimerFire;
        ADD_ABSOLUTETIME(&currentNextFire, &minimumInterval);
        
        minimumInterval = interval;

        if (CMP_ABSOLUTETIME(&desiredNextFire, &currentNextFire) < 0) {
            assert(timerEventSource);
            
#ifdef DEBUG_TIMER
            {
                UInt64 nanos;
                absolutetime_to_nanoseconds(interval, &nanos);
                IOLog("IOAudioDevice::addTimerEvent() - scheduling timer to fire in %lums at {%ld,%lu} - previousTimerFire = {%ld,%lu}\n", (UInt32) (nanos / 1000000), desiredNextFire.hi, desiredNextFire.lo, previousTimerFire.hi, previousTimerFire.lo);
            }
#endif

            result = timerEventSource->wakeAtTime(desiredNextFire);
            if (result != kIOReturnSuccess) {
                IOLog("IOAudioDevice::addTimerEvent() - error 0x%x setting timer wake time - timer events will be disabled.\n", result);
            }
        }
    }
    
    return result;
}

void IOAudioDevice::removeTimerEvent(OSObject *target)
{
    IOAudioTimerEvent *removedTimerEvent;
    
#ifdef DEBUG_CALLS
    IOLog("IOAudioDevice::removeTimerEvent(%p)\n", target);
#endif
    
    if (!timerEvents) {
        return;
    }

    removedTimerEvent = (IOAudioTimerEvent *)timerEvents->getObject((const OSSymbol *)target);
    if (removedTimerEvent) {
        removedTimerEvent->retain();
        timerEvents->removeObject((const OSSymbol *)target);
        if (timerEvents->getCount() == 0) {
            assert(timerEventSource);
            timerEventSource->cancelTimeout();
        } else if (CMP_ABSOLUTETIME(&removedTimerEvent->interval, &minimumInterval) <= 0) { // Need to find a new minimum interval
            OSCollectionIterator *iterator;
            IOAudioTimerEvent *timerEvent;
            AbsoluteTime nextTimerFire;
            OSSymbol *obj;

            iterator = OSCollectionIterator::withCollection(timerEvents);
            
            obj = (OSSymbol *)iterator->getNextObject();
            timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(obj);

            if (timerEvent) {
                minimumInterval = timerEvent->interval;

                while ((obj = (OSSymbol *)iterator->getNextObject()) && (timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(obj))) {
                    if (CMP_ABSOLUTETIME(&timerEvent->interval, &minimumInterval) < 0) {
                        minimumInterval = timerEvent->interval;
                    }
                }
            }

            iterator->release();

            assert(timerEventSource);

            nextTimerFire = previousTimerFire;
            ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
            
#ifdef DEBUG_TIMER
            {
                AbsoluteTime now, then;
                UInt64 nanos, mi;
                clock_get_uptime(&now);
                then = nextTimerFire;
                absolutetime_to_nanoseconds(minimumInterval, &mi);
                if (CMP_ABSOLUTETIME(&then, &now)) {
                    SUB_ABSOLUTETIME(&then, &now);
                    absolutetime_to_nanoseconds(then, &nanos);
                    IOLog("IOAudioDevice::removeTimerEvent() - scheduling timer to fire in %lums at {%ld,%lu} - previousTimerFire = {%ld,%lu} - interval=%lums\n", (UInt32) (nanos / 1000000), nextTimerFire.hi, nextTimerFire.lo, previousTimerFire.hi, previousTimerFire.lo, (UInt32)(mi/1000000));
                } else {
                    SUB_ABSOLUTETIME(&now, &then);
                    absolutetime_to_nanoseconds(now, &nanos);
                    IOLog("IOAudioDevice::removeTimerEvent() - scheduling timer to fire in -%lums - previousTimerFire = {%ld,%lu}\n", (UInt32) (nanos / 1000000), previousTimerFire.hi, previousTimerFire.lo);
                }
            }
#endif

            timerEventSource->wakeAtTime(nextTimerFire);
        }

        removedTimerEvent->release();
    }
}

void IOAudioDevice::clearTimerEvents()
{
    if (timerEventSource) {
        timerEventSource->cancelTimeout();
    }
    
    if (timerEvents) {
        timerEvents->flushCollection();
    }
}

void IOAudioDevice::timerFired(OSObject *target, IOTimerEventSource *sender)
{
    if (target) {
        IOAudioDevice *audioDevice = OSDynamicCast(IOAudioDevice, target);
        
        if (audioDevice) {
            audioDevice->dispatchTimerEvents(false);
        }
    }
}

void IOAudioDevice::dispatchTimerEvents(bool force)
{
    if (timerEvents) {
#ifdef DEBUG_TIMER
        AbsoluteTime now, delta;
        UInt64 nanos;
        
        clock_get_uptime(&now);
        delta = now;
        SUB_ABSOLUTETIME(&delta, &previousTimerFire);
        absolutetime_to_nanoseconds(delta, &nanos);
        IOLog("IOAudioDevice::dispatchTimerEvents() - woke up %lums after last fire - now = {%ld,%lu} - previousFire = {%ld,%lu}\n", (UInt32)(nanos / 1000000), now.hi, now.lo, previousTimerFire.hi, previousTimerFire.lo);
#endif

        if (force || !wakingFromSleep) {
            OSIterator *iterator;
            OSSymbol *target;
            AbsoluteTime nextTimerFire, currentInterval;
            
            currentInterval = minimumInterval;
        
            assert(timerEvents);
        
            iterator = OSCollectionIterator::withCollection(timerEvents);
        
            if (iterator) {
                while (target = (OSSymbol *)iterator->getNextObject()) {
                    IOAudioTimerEvent *timerEvent;
                    timerEvent = (IOAudioTimerEvent *)timerEvents->getObject(target);
                    
                    if (timerEvent) {
                        (*timerEvent->event)(timerEvent->target, this);
                    }
                }
        
                iterator->release();
            }
        
            if (timerEvents->getCount() > 0) {
                ADD_ABSOLUTETIME(&previousTimerFire, &currentInterval);
                nextTimerFire = previousTimerFire;
                ADD_ABSOLUTETIME(&nextTimerFire, &minimumInterval);
        
                assert(timerEventSource);
                
#ifdef DEBUG_TIMER
                {
                    AbsoluteTime later;
                    UInt64 mi;
                    later = nextTimerFire;
                    absolutetime_to_nanoseconds(minimumInterval, &mi);
                    if (CMP_ABSOLUTETIME(&later, &now)) {
                        SUB_ABSOLUTETIME(&later, &now);
                        absolutetime_to_nanoseconds(later, &nanos);
                        IOLog("IOAudioDevice::dispatchTimerEvents() - scheduling timer to fire in %lums at {%ld,%lu} - previousTimerFire = {%ld,%lu} - interval=%lums\n", (UInt32) (nanos / 1000000), nextTimerFire.hi, nextTimerFire.lo, previousTimerFire.hi, previousTimerFire.lo, (UInt32)(mi/1000000));
                    } else {
                        SUB_ABSOLUTETIME(&now, &later);
                        absolutetime_to_nanoseconds(now, &nanos);
                        IOLog("IOAudioDevice::dispatchTimerEvents() - scheduling timer to fire in -%lums - previousTimerFire = {%ld,%lu}\n", (UInt32) (nanos / 1000000), previousTimerFire.hi, previousTimerFire.lo);
                    }
                }
#endif
    
                timerEventSource->wakeAtTime(nextTimerFire);
            }
        }
    }
}

void IOAudioDevice::addMasterControl(IOAudioControl *masterControl)
{
    if (!masterControls) {
        masterControls = OSSet::withCapacity(1);
    }

    masterControls->setObject(masterControl);
}

void IOAudioDevice::setMasterVolumeLeft(UInt16 newMasterVolumeLeft)
{
    if (masterControls) {
        OSCollectionIterator *iterator;

        iterator = OSCollectionIterator::withCollection(masterControls);
        if (iterator) {
            OSObject *control;
            while (control = iterator->getNextObject()) {
                IOAudioLevelControl *levelControl;

                levelControl = OSDynamicCast(IOAudioLevelControl, control);
                if (levelControl && (levelControl->getChannelID() == IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_LEFT)) {
                    UInt32 minValue, maxValue;
                    minValue = levelControl->getMinValue();
                    maxValue = levelControl->getMaxValue();

                    levelControl->setValue(((newMasterVolumeLeft - MASTER_VOLUME_MIN) * (maxValue - minValue) / (MASTER_VOLUME_MAX - MASTER_VOLUME_MIN)) + minValue);
                }
            }
            iterator->release();
        }
    }
}

void IOAudioDevice::setMasterVolumeRight(UInt16 newMasterVolumeRight)
{
    if (masterControls) {
        OSCollectionIterator *iterator;

        iterator = OSCollectionIterator::withCollection(masterControls);
        if (iterator) {
            OSObject *control;
            while (control = iterator->getNextObject()) {
                IOAudioLevelControl *levelControl;

                levelControl = OSDynamicCast(IOAudioLevelControl, control);
                if (levelControl && (levelControl->getChannelID() == IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_RIGHT)) {
                    UInt32 minValue, maxValue;
                    minValue = levelControl->getMinValue();
                    maxValue = levelControl->getMaxValue();

                    levelControl->setValue(((newMasterVolumeRight - MASTER_VOLUME_MIN) * (maxValue - minValue) / (MASTER_VOLUME_MAX - MASTER_VOLUME_MIN)) + minValue);
                }
            }
            iterator->release();
        }
    }
}

void IOAudioDevice::setMasterMute(bool newMasterMute)
{
    if (masterControls) {
        OSCollectionIterator *iterator;

        iterator = OSCollectionIterator::withCollection(masterControls);
        if (iterator) {
            OSObject *control;
            while (control = iterator->getNextObject()) {
                IOAudioMuteControl *muteControl;

                muteControl = OSDynamicCast(IOAudioMuteControl, control);
                if (muteControl) {
                    muteControl->setValue(newMasterMute ? 1 : 0);
                }
            }
            iterator->release();
        }
    }
}


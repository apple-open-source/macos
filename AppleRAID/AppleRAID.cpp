/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  DRI: Josh de Cesare
 *
 */


#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IODeviceTreeSupport.h>

#include "AppleRAID.h"
#include "AppleRAIDGlobals.h"
#include "AppleRAIDEventSource.h"

#define super IOStorage
OSDefineMetaClassAndStructors(AppleRAID, IOStorage);

void AppleRAID::free(void)
{
    AppleRAIDStorageRequest *storageRequest;
    
    if (_arSetMedia != 0) _arSetMedia->release();
    
    if (_arSetReaders != 0) _arSetReaders->release();
    
    if (arLogicalSliceNumbers != 0) IODelete(arLogicalSliceNumbers, UInt32, arSliceCount);
    if (arSliceMediaStates != 0) IODelete(arSliceMediaStates, UInt32, arSliceCount); 
    if (arSliceRequestTerminates != 0) IODelete(arSliceRequestTerminates, bool, arSliceCount);
    if (arSliceMediaErrors != 0) IODelete(arSliceMediaErrors, IOReturn, arSliceCount);
    if (arSliceMedias != 0) IODelete(arSliceMedias, IOMedia *, arSliceCount);
    if (_arSetDTParents != 0) IODelete(_arSetDTParents, IORegistryEntry *, arSliceCount);
    if (_arSetDTLocations != 0) IODelete(_arSetDTLocations, char, arSliceCount * kAppleRAIDMaxOFPath);
    if (_arSetDTPaths != 0) IOFree(_arSetDTPaths, arSliceCount * kAppleRAIDMaxOFPath);
    
    if (_arSliceCloseThreadCall) thread_call_free(_arSliceCloseThreadCall);
    if (_arUpdateHeadersThreadCall) thread_call_free(_arUpdateHeadersThreadCall);

    if (_arSyncronizeCacheThreadCall != 0) {
	for (UInt32 cnt = 0; cnt < arSliceCount; cnt++) {
	    if (_arSyncronizeCacheThreadCall[cnt]) thread_call_free(_arSyncronizeCacheThreadCall[cnt]);
	}
	IODelete(_arSyncronizeCacheThreadCall, thread_call_t, arSliceCount);
    }
    
    if (_arStorageRequestPool != 0) {
        while (1) {
            storageRequest = (AppleRAIDStorageRequest *)_arStorageRequestPool->getCommand(false);
            if (storageRequest == 0) break;
            storageRequest->release();
        }
        _arStorageRequestPool->release();
    }
    
    if (_arStorageRequestErrorPool != 0) {
        _arStorageRequestErrorPool->release();
    }
    
    if (_arSetTimerEventSource != 0) {
        _arSetTimerEventSource->cancelTimeout();
        _arSetWorkLoop->removeEventSource(_arSetTimerEventSource);
        _arSetTimerEventSource->release();
    }
    
    if (_arSetCommandGate != 0) {
        _arSetWorkLoop->removeEventSource(_arSetCommandGate);
        _arSetCommandGate->release();
    }
    
    if (arSetEventSource != 0) {
        _arSetWorkLoop->removeEventSource(arSetEventSource);
        arSetEventSource->release();
    }
    
    if (_arSetWorkLoop != 0) _arSetWorkLoop->release();
    
    if (_arSetUniqueName != 0) _arSetUniqueName->release();
    if (_arSetName != 0) _arSetName->release();
    
    if (_arHeaderBuffer != 0) _arHeaderBuffer->release();
    
    if (_arSetLock != 0) IOLockFree(_arSetLock);
    
    super::free();
}

bool AppleRAID::init(OSDictionary *properties)
{
    if (!super::init(properties)) return false;
    
    if (!gAppleRAIDGlobals.isValid()) return false;
    
    _arActionChangeSliceMediaState = (IOCommandGate::Action)&AppleRAID::changeSliceMediaState;
    
    _arSetLock = IOLockAlloc();
    if (_arSetLock == 0) return false;
    
    _arSetIsWritable = true;
    _arSetIsEjectable = true;
    
    return true;
}

IOService *AppleRAID::probe(IOService *provider, SInt32 *score)
{
    AppleRAID		*raidSet = 0;
    AppleRAIDHeader	*raidHeader;
    IOMedia 		*media = OSDynamicCast(IOMedia, provider);
    OSNumber		*tmpNumber;
    bool		raidLevelValid = true;
    char		tmpString[65];
    
    if (super::probe(provider, score) == 0) return 0;
    
    // Open the IOMedia so that the AppleRAID Header can be read.
    if (!media->open(this, 0, kIOStorageAccessReader)) return 0;
    
    while (1) {
        // Allocate a buffer to read the AppleRAID Header.
        arHeaderSize = kAppleRAIDHeaderSize;
        _arHeaderBuffer = IOBufferMemoryDescriptor::withCapacity(arHeaderSize, kIODirectionIn);
        if (_arHeaderBuffer == 0) break;
        
        // Read the first block of the IOMedia.
        if (media->IOStorage::read(this, 0, _arHeaderBuffer) != kIOReturnSuccess) break;
        
        raidHeader = (AppleRAIDHeader *)_arHeaderBuffer->getBytesNoCopy();
        
        // Make sure the AppleRAID Header contains the correct signature.
        if (strcmp(raidHeader->raidSignature, kAppleRAIDSignature)) break;
        
        // Make sure the header version is understood.
        if (raidHeader->raidHeaderVersion != kAppleRAIDHeaderV1_0_0) break;
        
        // Make sure the header sequence is valid.
        if (raidHeader->raidHeaderSequence == 0) break;
        
        // Validate the the RAID level.
        switch (raidHeader->raidLevel) {
            case kAppleRAIDStripe : break;
            case kAppleRAIDMirror : break;
            //case kAppleRAIDConcat : break;
            default : raidLevelValid = false; break;
        }
        if (!raidLevelValid) break;
        
        // Get the unique name for the RAID Partition.
        _arSetName = OSSymbol::withCString(raidHeader->raidSetName);
        if (_arSetName == 0) break;
        
        // Construct the unique name for the set.
        sprintf(tmpString , "%s%08lx%08lx%08lx%08lx", raidHeader->raidSetName,
                raidHeader->raidUUID[0], raidHeader->raidUUID[1],
                raidHeader->raidUUID[2], raidHeader->raidUUID[3]);
        _arSetUniqueName = OSSymbol::withCString(tmpString);
        if (_arSetUniqueName == 0) break;
        
        // Save the header sequence in the IOMedia.
        tmpNumber = OSNumber::withNumber(raidHeader->raidHeaderSequence, 32);
        if (tmpNumber == 0) break;
        media->setProperty(kAppleRAIDSequenceNumberKey, tmpNumber);
        tmpNumber->release();
        
        // Save the slice number in the IOMedia.
        tmpNumber = OSNumber::withNumber(raidHeader->raidSliceNumber, 32);
        if (tmpNumber == 0) break;
        media->setProperty(kAppleRAIDSliceNumberKey, tmpNumber);
        tmpNumber->release();
        
        gAppleRAIDGlobals.lock();
        
        // Look up the unique name in gAppleRAIDSets.
        raidSet = gAppleRAIDGlobals.getAppleRAIDSet(_arSetUniqueName);
        
        // If the unique name was not found then this IOMedia is the first one in this AppleRAID Set.
        if (raidSet == 0) {
            // Save the set unique name and name in the raid set.
#if 1
            setProperty(kAppleRAIDSetUniqueNameKey, tmpString);
            setProperty(kAppleRAIDSetNameKey, raidHeader->raidSetName);
#else
            setProperty(kAppleRAIDSetUniqueNameKey, (OSObject *)_arSetUniqueName);
            setProperty(kAppleRAIDSetNameKey, (OSObject *)_arSetName);
#endif
            
            arHeaderSize	= raidHeader->raidHeaderSize;
            arHeaderSequence	= raidHeader->raidHeaderSequence;
            arSetLevel		= raidHeader->raidLevel;
            arSliceCount      	= raidHeader->raidSliceCount;
            arSetBlockSize    	= raidHeader->raidChunkSize;
            arSetBlockCount    	= raidHeader->raidChunkCount;
            arSetMediaSize   	= (UInt64)arSetBlockCount * arSetBlockSize;
            
            _arFirstSlice	= arSliceCount;
            
            _arSetUUID[0] = raidHeader->raidUUID[0];
            _arSetUUID[1] = raidHeader->raidUUID[1];
            _arSetUUID[2] = raidHeader->raidUUID[2];
            _arSetUUID[3] = raidHeader->raidUUID[3];
            
            // Save the name of the raid level in the raid set.
            switch (arSetLevel) {
                case kAppleRAIDStripe	: setProperty(kAppleRAIDLevelName, kAppleRAIDLevelNameStripe); break;
                case kAppleRAIDMirror	: setProperty(kAppleRAIDLevelName, kAppleRAIDLevelNameMirror); break;
                case kAppleRAIDConcat	: setProperty(kAppleRAIDLevelName, kAppleRAIDLevelNameConcat); break;
            }
            
            // Allocate some needed arrays.
            arLogicalSliceNumbers 	= IONew(UInt32, arSliceCount);
            arSliceMediaStates		= IONew(UInt32, arSliceCount);
            arSliceRequestTerminates	= IONew(bool, arSliceCount);
            arSliceMediaErrors	 	= IONew(IOReturn, arSliceCount);
            arSliceMedias		= IONew(IOMedia *, arSliceCount);
            _arSetDTParents		= IONew(IORegistryEntry *, arSliceCount);
            _arSetDTLocations		= IONew(char, arSliceCount * kAppleRAIDMaxOFPath);
            _arSetDTPaths		= (char *)IOMalloc(arSliceCount * kAppleRAIDMaxOFPath);
            
            // Clear the new arrays.
            bzero(arLogicalSliceNumbers, sizeof(UInt32) * arSliceCount);
            bzero(arSliceMediaStates, sizeof(UInt32) * arSliceCount);
            bzero(arSliceRequestTerminates, sizeof(bool) * arSliceCount);
            bzero(arSliceMediaErrors, sizeof(bool) * arSliceCount);
            bzero(arSliceMedias, sizeof(IOMedia *) * arSliceCount);
            bzero(_arSetDTParents, sizeof(IORegistryEntry *) * arSliceCount);
            bzero(_arSetDTLocations, arSliceCount * kAppleRAIDMaxOFPath);
            bzero(_arSetDTPaths, kAppleRAIDMaxOFPath * arSliceCount);
            
            // Create the readers set.
            _arSetReaders = OSSet::withCapacity(1);
            
            // Get the WorkLoop.
            if (getWorkLoop() != 0) {
                _arSetCommandGate = IOCommandGate::commandGate(this);
                if (_arSetCommandGate != 0) {
                    getWorkLoop()->addEventSource(_arSetCommandGate);
                }
                
                _arSetTimerEventSource = IOTimerEventSource::timerEventSource(this,
                                            (IOTimerEventSource::Action)&AppleRAID::raidTimeOut);
                if (_arSetTimerEventSource != 0) {
                    getWorkLoop()->addEventSource(_arSetTimerEventSource);
                }
                
                arSetEventSource = AppleRAIDEventSource::withAppleRAIDSet(this,
                                        (AppleRAIDEventSource::Action)&AppleRAID::completeRAIDRequest);
                if (arSetEventSource != 0) {
                    getWorkLoop()->addEventSource(arSetEventSource);
                }
            }

            _arSliceCloseThreadCall = thread_call_allocate(
                                        (thread_call_func_t)&AppleRAID::closeSliceMedias,
                                        (thread_call_param_t)this);
            
            // Find the AppleRAIDController.
            _arController = gAppleRAIDGlobals.getAppleRAIDController();
            
            if ((arSliceMediaStates != 0) && (arSliceMedias != 0) &&
                (arSliceRequestTerminates != 0) && (arSliceMediaErrors != 0) &&
                (_arSetDTParents != 0) && (_arSetDTLocations != 0) &&
                (_arController != 0) && (_arSliceCloseThreadCall != 0) && (getWorkLoop() != 0) &&
                (_arSetTimerEventSource != 0) && (_arSetCommandGate != 0) && (arSetEventSource != 0)) {
                
                // Set timer for for Mirrors.
                if (arSetLevel == kAppleRAIDMirror) {
                    _arSetTimerEventSource->setTimeout(30, kSecondScale);
                }
                
                gAppleRAIDGlobals.setAppleRAIDSet(_arSetUniqueName, this);
                
                setProperty(kAppleRAIDStatus, kAppleRAIDStatusForming);
                
                attach(_arController);
                
                raidSet = this;
            }
        }
        
        gAppleRAIDGlobals.unlock();
        
        break;
    }
    
    media->close(this);
    
    //  Release the buffer used for probe.
    if (_arHeaderBuffer != 0) {
        _arHeaderBuffer->release();
        _arHeaderBuffer = 0;
    }
    
    return raidSet;
}

bool AppleRAID::start(IOService *provider)
{
    IOMedia *media = OSDynamicCast(IOMedia, provider);
    
    if(!super::start(provider)) return false;
    
    if (_arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::addSliceMedia, media) != kIOReturnSuccess)
        return false;
    
    // Don't really start until all the slices have been counted.
    if (_arSlicesStarted != arSliceCount) return true;
    
    // Cancel the pending timeout.
    _arSetTimerEventSource->cancelTimeout();
    
    // Create the RAID Media.
    if (_arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::publishRAIDMedia) != kIOReturnSuccess)
        return false;
    
    return true;
}

void AppleRAID::stop(IOService *provider)
{
    IOMedia		*media = OSDynamicCast(IOMedia, provider);
    AppleRAIDController	*controller = OSDynamicCast(AppleRAIDController, provider);
    
    // Handle being stopped by the AppleRAIDController.
    if (controller != 0) {
        _arController = 0;
    } else if (media != 0) {
        // Wait for pending slice terminates to complete.
        _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::waitForSliceTerminateRequests);
        
        // Degrade or remove this slice from the raid set.
        if ((arSetLevel == kAppleRAIDMirror) && isOpen()) {
            _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::degradeSliceMedia,
                                          media, 0, (void *)kIOReturnNoMedia);
        } else {
            _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::removeSliceMedia, media);
        }
        
        // Wait for all of the slices to stop before removing the AppleRAID Set.
        if (_arSlicesStarted == 0) {
            // Cancel any pending timers.
            _arSetTimerEventSource->cancelTimeout();
            
            // Mark the set as stopped.
            setProperty(kAppleRAIDStatus, kAppleRAIDStatusStopped);
            
            _arController->statusChanged(this);
            
            // Remove the set from the dictionary.
            gAppleRAIDGlobals.lock();
            gAppleRAIDGlobals.removeAppleRAIDSet(_arSetUniqueName);
            gAppleRAIDGlobals.unlock();
            
            // Self terminate since requestTerminate will always return false.
            terminate();
        }
    }
    
    super::stop(provider);
}

bool AppleRAID::requestTerminate(IOService *provider, IOOptionBits options)
{
    // Always return false to prevent early termination.
    
    _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::requestSliceTerminate, provider);
    
    return false;
}

bool AppleRAID::willTerminate(IOService *provider, IOOptionBits options)
{
    IOMedia *media = OSDynamicCast(IOMedia, provider);
    
    if (media != 0) {
        arSetEventSource->terminateRAIDMedia(media);
    }
    
    return super::willTerminate(provider, options);
}

bool AppleRAID::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    return super::didTerminate(provider, options, defer);
}

IOWorkLoop *AppleRAID::getWorkLoop(void)
{
    // Create a WorkLoop if it has not already been done.
    if (_arSetWorkLoop == 0) {
        _arSetWorkLoop = IOWorkLoop::workLoop();
    }
    
    return _arSetWorkLoop;
}

IOReturn AppleRAID::addSliceMedia(IOMedia *media)
{
    UInt32	cnt, sequenceNumber, sliceNumber;
    OSNumber	*tmpNumber;
    int		length;
    char	*ofPath;
    const char	*location;
    
    // If the set is terminating, abort this slice and re-register.
    if (_arSetIsTerminating) {
        media->registerService(kIOServiceAsynchronous);
        return kIOReturnAborted;
    }
    
    // Don't try to add new slices after the set has already been published.
    if (_arSetMedia != 0) return kIOReturnInternalError;
    
    // Get the sequence number for this slice.
    tmpNumber = OSDynamicCast(OSNumber, media->getProperty(kAppleRAIDSequenceNumberKey));
    sequenceNumber = tmpNumber->unsigned32BitValue();
        
    // Get the number for this slice.
    tmpNumber = OSDynamicCast(OSNumber, media->getProperty(kAppleRAIDSliceNumberKey));
    sliceNumber = tmpNumber->unsigned32BitValue();
    
    // Don't use slices that have sequence numbers older than the raid set.
    if (sequenceNumber < arHeaderSequence) {
        return kIOReturnInternalError;
    }
    
    // If this new slice is newer than the set remove all the other slices.
    if (sequenceNumber > arHeaderSequence) {
        for (cnt = 0; cnt < arSliceCount; cnt++) {
            if (arSliceMedias[cnt] != 0) {
                removeSliceMedia(arSliceMedias[cnt]);
            }
        }
        
        // Update the raid set's sequence number.
        arHeaderSequence = sequenceNumber;
    }
    
    // Making sure this is the only slice in this slot.  This should never happen,
    // but the AppleRAID driver will crash if this is not done.
    if (arSliceMedias[sliceNumber] != 0) {
        return kIOReturnInternalError;
    }
    
    // Save the IOMedia in the raid set.
    arSliceMedias[sliceNumber] = media;
    
    // Set the slice's state to closed.
    arSliceMediaStates[sliceNumber] = kAppleRAIDSliceMediaStateClosed;
    
    arSliceRequestTerminates[sliceNumber] = false;
    
    // Set the first slice number.
    if (sliceNumber < _arFirstSlice) _arFirstSlice = sliceNumber;
    
    // Save the Device Tree parent, location and path of each IOMedia.
    _arSetDTParents[sliceNumber]   = media->getParentEntry(gIODTPlane);
    location = media->getLocation(gIODTPlane);
    if (location != 0) {
        strncpy(_arSetDTLocations + (sliceNumber * kAppleRAIDMaxOFPath), location, kAppleRAIDMaxOFPath);
    }
    length = kAppleRAIDMaxOFPath;
    ofPath = _arSetDTPaths + (sliceNumber * kAppleRAIDMaxOFPath);
    if (!media->getPath(ofPath, &length, gIODTPlane)) *ofPath = '\0';
    
    // Remove each IOMedia from the Device Tree plane.
    media->detachAbove(gIODTPlane);
    
    // Make sure this slice is ejectable and writable.
    if (!media->isEjectable()) _arSetIsEjectable = false;
    if (!media->isWritable())  _arSetIsWritable  = false;
    
    // Count this slice as started.
    _arSlicesStarted++;
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::removeSliceMedia(IOMedia *media)
{
    UInt32		sliceNumber;
    bool		shouldBeClosed;
    
    if (!getSliceNumberForMedia(media, &sliceNumber)) return kIOReturnError;
    
    shouldBeClosed = changeSliceMediaState(sliceNumber, kAppleRAIDSliceMediaStateStopping);
    
    // Clear the OF path to the slice.
    _arSetDTPaths[sliceNumber * kAppleRAIDMaxOFPath] = '\0';
    
    // Mark this slice as stopped.
    _arSlicesStarted--;
    
    // If this slice's media is terminating, mark it's termination as complete.
    if (arSliceRequestTerminates[sliceNumber]) {
        _arSliceTerminatesActive--;
    }
    
    // Attach the media back to the original Device Tree parent.
    if (_arSetDTParents[sliceNumber] != 0) {
        media->attachToParent(_arSetDTParents[sliceNumber], gIODTPlane);
    }
    
    // If the media is still open, close it.
    if (shouldBeClosed) {
        thread_call_enter1(_arSliceCloseThreadCall, (thread_call_param_t)0);
    }
    
    // Find the first slice that has a media.
    if (sliceNumber == _arFirstSlice) {
        for (_arFirstSlice = sliceNumber + 1; _arFirstSlice < arSliceCount; _arFirstSlice++) {
            if (arSliceMedias[_arFirstSlice] != 0) break;
        }
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::degradeSliceMedia(IOMedia *media, AppleRAIDStorageRequest *storageRequest, IOReturn status)
{
    UInt32	sliceNumber;
    IOReturn	result = kIOReturnSuccess;

    if (media != 0) {
        if (!getSliceNumberForMedia(media, &sliceNumber)) return kIOReturnError;
        
        // Pause the raid set.
        _arSetIsDegrading = true;
        _arSetIsPaused = true;
        
        // Mark this slice as having an error.
        if (status == kIOReturnNoMedia) {
            arSliceMediaErrors[sliceNumber] = kIOReturnNoMedia;
        } else if (arSliceMediaErrors[sliceNumber] != kIOReturnNoMedia) {
            arSliceMediaErrors[sliceNumber] = status;
        }
        
        if (storageRequest != 0) {
            // Wait for a pending header update to complete before adding new errors to the pool.
            if (_arSetUpdatePending) {
                _arSetCommandGate->commandSleep(&_arSetUpdatePending, THREAD_UNINT);
            }
            
            // Move this command from pending to the error pool.
            _arStorageRequestsPending--;
            _arStorageRequestsErrorsPending++;
            _arStorageRequestErrorPool->returnCommand(storageRequest);
        }
        
        // If there are more requests pending, return.
        if (_arStorageRequestsPending != 0) {
            return kIOReturnBusy;
        }
    }
    
    // All of the pending requests have completed.
    
    // Wait for a pending header update to complete before degrading the set again.
    if (_arSetUpdatePending) {
        _arSetCommandGate->commandSleep(&_arSetUpdatePending, THREAD_UNINT);
    }
    
    // Count the errors as new pending requests.
    _arStorageRequestsPending = _arStorageRequestsErrorsPending;
    _arStorageRequestsErrorsPending = 0;
    
    for (sliceNumber = 0; sliceNumber < arSliceCount; sliceNumber++) {
        if (arSliceMedias[sliceNumber] != 0) {
            if (arSliceMediaErrors[sliceNumber] == kIOReturnSuccess) continue;
            if ((arSliceMediaErrors[sliceNumber] == kIOReturnNoMedia) || (_arSlicesStarted > 1))
                removeSliceMedia(arSliceMedias[sliceNumber]);
        }
    }
    
    initRAIDSet();
    
    if (_arSlicesStarted != 0) {
        _arSetUpdatePending = true;
        
        // Increment the header sequence number.
        arHeaderSequence++;
        
        // Update the RAID headers.
        thread_call_enter(_arUpdateHeadersThreadCall);
        
        setProperty(kAppleRAIDStatus, kAppleRAIDStatusDegraded);
        
        result = kIOReturnBusy;
    } else {
        setProperty(kAppleRAIDStatus, kAppleRAIDStatusFailed);
    }
    
    _arController->statusChanged(this);
    
    // If not waiting for a syncronizeCache, unpause the raid set and wakeup any sleeping requests.
    _arSetIsDegrading = false;
    if (!_arSetIsSyncing) {
        _arSetIsPaused = false;
        _arSetCommandGate->commandWakeup(&_arSetIsPaused, false);
    }
    
    return result;
}

bool AppleRAID::getSliceNumberForMedia(IOMedia *media, UInt32 *sliceNumber)
{
    OSNumber *tmpNumber;
    UInt32   _sliceNumber;
    
    // Get the number for this slice.
    tmpNumber = OSDynamicCast(OSNumber, media->getProperty(kAppleRAIDSliceNumberKey));
    _sliceNumber = tmpNumber->unsigned32BitValue();
    
    if (_sliceNumber >= arSliceCount) return false;
    
    // Make sure the slice is not in the process of being stopped.
    if (arSliceMediaStates[_sliceNumber] >= kAppleRAIDSliceMediaStateStopping) return false;
    
    // Make sure this media is the same as in the slice slot.
    if (arSliceMedias[_sliceNumber] != media) return false;
    
    *sliceNumber = _sliceNumber;
    
    return true;
}

IOReturn AppleRAID::initRAIDSet(void)
{
    UInt32			cnt, logicalSliceNumber, pagesPerChunk;
    UInt64			maxBlockCount, maxSegmentCount;
    OSNumber			*tmpNumber, *tmpNumber2;
    AppleRAIDStorageRequest	*storageRequest;
    
    if (_arFirstSlice == arSliceCount) return kIOReturnError;
    
    if (!_arSetStartedOnce) {
        // Allocate a buffer for reading and writing RAID headers.
        _arHeaderBuffer = IOBufferMemoryDescriptor::withCapacity(arHeaderSize, kIODirectionNone);
        if (_arHeaderBuffer == 0) return kIOReturnNoMemory;
        _arHeader = (AppleRAIDHeader *)_arHeaderBuffer->getBytesNoCopy();
        
        _arUpdateHeadersThreadCall = thread_call_allocate(
                                    (thread_call_func_t)&AppleRAID::updateRAIDHeaders,
                                    (thread_call_param_t)this);
        if (_arUpdateHeadersThreadCall == 0) return kIOReturnNoMemory;
        
        _arSyncronizeCacheThreadCall = IONew(thread_call_t, arSliceCount);
        if (_arSyncronizeCacheThreadCall == 0) return kIOReturnNoMemory;
	bzero(_arSyncronizeCacheThreadCall, sizeof(thread_call_t) * arSliceCount);
	for (cnt = 0; cnt < arSliceCount; cnt++) {
	    _arSyncronizeCacheThreadCall[cnt] = thread_call_allocate(
		(thread_call_func_t)&AppleRAID::synchronizeCacheSlice,
		(thread_call_param_t)this);
	    if (_arSyncronizeCacheThreadCall[cnt] == 0) return kIOReturnNoMemory;
	}
        
        // Create and populate the storage request pool.
        _arStorageRequestPool = IOCommandPool::withWorkLoop(getWorkLoop());
        if (_arStorageRequestPool == 0) return kIOReturnNoMemory;
        for (cnt = 0; cnt < kAppleRAIDStorageRequestCount; cnt++) {
            storageRequest = AppleRAIDStorageRequest::withAppleRAIDSet(this);
            if (storageRequest == 0) break;
            _arStorageRequestPool->returnCommand(storageRequest);
        }
        
        // Create the storage request error pool.
        _arStorageRequestErrorPool = IOCommandPool::withWorkLoop(getWorkLoop());
        if (_arStorageRequestErrorPool == 0) return kIOReturnNoMemory;
        
        _arSetStartedOnce = true;
    }
    
    // Get the native block size for the raid set.
    _arSetNativeBlockSize = arSliceMedias[_arFirstSlice]->getPreferredBlockSize();
    
    // Publish larger segment sizes for Stripes.
    if (arSetLevel == kAppleRAIDStripe) {
        pagesPerChunk = arSetBlockSize / PAGE_SIZE;
        
        tmpNumber  = OSDynamicCast(OSNumber, arSliceMedias[_arFirstSlice]->getProperty(kIOMaximumBlockCountReadKey, gIOServicePlane));
        tmpNumber2 = OSDynamicCast(OSNumber, arSliceMedias[_arFirstSlice]->getProperty(kIOMaximumSegmentCountReadKey, gIOServicePlane));
        if ((tmpNumber != 0) && (tmpNumber2 != 0)) {
            maxBlockCount   = tmpNumber->unsigned64BitValue();
            maxSegmentCount = tmpNumber2->unsigned64BitValue();
            
            maxBlockCount *= _arSlicesStarted;
            maxSegmentCount *= _arSlicesStarted;
            
            setProperty(kIOMaximumBlockCountReadKey, maxBlockCount, 64);
            setProperty(kIOMaximumSegmentCountReadKey, maxSegmentCount, 64);
        }
        
        tmpNumber  = OSDynamicCast(OSNumber, arSliceMedias[_arFirstSlice]->getProperty(kIOMaximumBlockCountWriteKey, gIOServicePlane));
        tmpNumber2 = OSDynamicCast(OSNumber, arSliceMedias[_arFirstSlice]->getProperty(kIOMaximumSegmentCountWriteKey, gIOServicePlane));
        if ((tmpNumber != 0) && (tmpNumber2 != 0)) {
            maxBlockCount   = tmpNumber->unsigned64BitValue();
            maxSegmentCount = tmpNumber2->unsigned64BitValue();
            
            maxBlockCount *= _arSlicesStarted;
            maxSegmentCount *= _arSlicesStarted;
            
            setProperty(kIOMaximumBlockCountWriteKey, maxBlockCount, 64);
            setProperty(kIOMaximumSegmentCountWriteKey, maxSegmentCount, 64);
        }
    }
    
    // Calculate the logical slice numbers.
    logicalSliceNumber = 0;
    for (cnt = 0; cnt < arSliceCount; cnt++) {
        if (arSliceMediaStates[cnt] >= kAppleRAIDSliceMediaStateStopping) continue;
        if (arSliceMedias[cnt] == 0) continue;
        
        arLogicalSliceNumbers[cnt] = logicalSliceNumber++;
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::publishRAIDMedia(void)
{
    IOReturn 	result;
    
    // Handle first start only stuff.
    result = initRAIDSet();
    if (result != kIOReturnSuccess) return result;
    
    // Don't republish if there is already a raid media.
    if (_arSetMedia != 0) return kIOReturnInternalError;
    
    // Update the RAID headers.
    updateRAIDHeaders();
    
    // Create the media object for the raid set.
    _arSetMedia = new IOMedia;
    if ((_arSetMedia != 0) &&
         _arSetMedia->init(0, arSetMediaSize, _arSetNativeBlockSize,
                           _arSetIsEjectable, true, _arSetIsWritable, 0)) {
        _arSetMedia->setName(_arSetName);
        _arSetMedia->setName("", gIODTPlane);
        _arSetMedia->setLocation(_arSetDTLocations + (_arFirstSlice * kAppleRAIDMaxOFPath), gIODTPlane);
    } else return false;
    
    if (!_arSetMedia->attach(this)) return false;
    
    if ((_arSetDTParents[_arFirstSlice] != 0) &&
        !_arSetMedia->attachToParent(_arSetDTParents[_arFirstSlice], gIODTPlane)) {
            return false;
    }
    
    _arSetMedia->registerService(kIOServiceAsynchronous);
    
    if (_arSlicesStarted == arSliceCount) {
        setProperty(kAppleRAIDStatus, kAppleRAIDStatusRunning);
    } else {
        setProperty(kAppleRAIDStatus, kAppleRAIDStatusDegraded);
    }
    
    _arController->statusChanged(this);
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::terminateRAIDMedia(IOMedia *media)
{
    bool	setMirrorTimer = false;
    UInt32	sliceNumber;
    IOMedia	*setMedia;
    
    // Count this slice as having been actively terminated.
    _arSliceTerminatesPending--;
    _arSliceTerminatesActive++;
    
    // Wakeup ::stop threads that are waiting for pending terminates to become active.
    if (_arSliceTerminatesPending == 0) {
        _arSetCommandGate->commandWakeup(&_arSliceTerminatesPending, false);
    }
    
    if (_arSetMedia == 0) return kIOReturnSuccess;
    
    if (!_arSetIsTerminating) {
        if (!getSliceNumberForMedia(media, &sliceNumber)) return kIOReturnSuccess;
        
        if (arSetLevel == kAppleRAIDMirror) {
            if (isOpen()) return kIOReturnSuccess;
            
            setMirrorTimer = true;
        }
    }
    
    // Tear down the media.
    setMedia = _arSetMedia;
    _arSetMedia = 0;
    setMedia->terminate();
    setMedia->release();
    
    if (!_arSetIsTerminating) {
        // Set timer for for Mirrors if not terminating the set.
        if (setMirrorTimer) {
            _arSetTimerEventSource->setTimeout(30, kSecondScale);
        }
        
        setProperty(kAppleRAIDStatus, kAppleRAIDStatusForming);
        
        _arController->statusChanged(this);
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::updateRAIDHeaders(void)
{
    UInt32	cnt, cnt2;
    char	*setOFPath, *headerOFPath;
    bool	isWrite, changed = false;
    AppleRAIDStorageRequest *storageRequest;
    
    // Open the slices for write.
    if (!open(this, 0, kIOStorageAccessReaderWriter)) return kIOReturnIOError;
    
    for (cnt = 0; cnt < arSliceCount; cnt++) {
        if (arSliceMediaStates[cnt] >= kAppleRAIDSliceMediaStateStopping) continue;
        if (arSliceMedias[cnt] == 0) continue;
        
        _arHeaderBuffer->setDirection(kIODirectionIn);
	if (arSliceMedias[cnt]->IOStorage::read(this, 0, _arHeaderBuffer) != kIOReturnSuccess) continue;
        
        // Update the sequence number if it has changed.
        if (_arHeader->raidHeaderSequence != arHeaderSequence) {
            _arHeader->raidHeaderSequence = arHeaderSequence;
            changed = true;
        }
        
        // Update the OF Device Paths.
        headerOFPath = _arHeader->raidOFPaths;
        for (cnt2 = 0; cnt2 < arSliceCount; cnt2++) {
            setOFPath = strchr(_arSetDTPaths + (cnt2 * kAppleRAIDMaxOFPath), ':');
            if (setOFPath != 0) {
                setOFPath++;
                if (strcmp(setOFPath, headerOFPath)) {
                    strcpy(headerOFPath, setOFPath);
                    changed = true;
                }
                headerOFPath += strlen(setOFPath) + 1;
            } else {
                if (*headerOFPath != '\0') {
                    *(headerOFPath++) = '\0';
                     changed = true;
                }
            }
        }
        
        if (changed) {
            // Clear the rest of the header.
            bzero(headerOFPath, arHeaderSize + (UInt32)_arHeader - (UInt32)headerOFPath);
            
            // Write out the new header for this slice.
            _arHeaderBuffer->setDirection(kIODirectionOut);
            arSliceMedias[cnt]->IOStorage::write(this, 0, _arHeaderBuffer);
        }
    }
    
    // Close the slices.
    close(this, 0);
    
    _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::updateRAIDHeadersDone);
    
    while (1) {
        storageRequest = (AppleRAIDStorageRequest *)_arStorageRequestErrorPool->getCommand(false);
        if (storageRequest == 0) break;
        
        storageRequest->setSliceData(_arSlicesStarted, arLogicalSliceNumbers);
        
        isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
        
        if (isWrite) {
            storageRequest->write(storageRequest->srClient, storageRequest->srByteStart,
                                storageRequest->srMemoryDescriptor, storageRequest->srCompletion);
        } else {
            storageRequest->read(storageRequest->srClient, storageRequest->srByteStart,
                                storageRequest->srMemoryDescriptor, storageRequest->srCompletion);
        }
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::updateRAIDHeadersDone(void)
{
    _arSetUpdatePending = false;
    _arSetCommandGate->commandWakeup(&_arSetUpdatePending, false);
    
    return kIOReturnSuccess;
}

void AppleRAID::raidTimeOut(IOTimerEventSource *sender)
{
    arHeaderSequence++;
    
    publishRAIDMedia();
}

IOReturn AppleRAID::allocateRAIDRequest(AppleRAIDStorageRequest **storageRequest)
{
    while (1) {
        if (_arSetIsTerminating || (_arSlicesStarted == 0)) {
            *storageRequest = 0;
            return kIOReturnNoMedia;
        }
        
        if (_arSetIsPaused) {
            _arSetCommandGate->commandSleep(&_arSetIsPaused, THREAD_UNINT);
            continue;
        }
        
        *storageRequest = (AppleRAIDStorageRequest *)_arStorageRequestPool->getCommand(false);
        if (*storageRequest == 0) {
            _arSetCommandGate->commandSleep(_arStorageRequestPool, THREAD_UNINT);
            continue;
        }
        
        break;
    }
    
    _arStorageRequestsPending++;
    (*storageRequest)->setSliceData(_arSlicesStarted, arLogicalSliceNumbers);
    
    return kIOReturnSuccess;
}

void AppleRAID::returnRAIDRequest(AppleRAIDStorageRequest *storageRequest)
{
    _arStorageRequestsPending--;
    _arStorageRequestPool->returnCommand(storageRequest);
    _arSetCommandGate->commandWakeup(_arStorageRequestPool, true);
}

void AppleRAID::completeRAIDRequest(AppleRAIDStorageRequest *storageRequest)
{
    UInt32		cnt;
    UInt64              byteCount;
    IOReturn            status;
    bool		isMirror, isWrite, shouldCompleteErrors = false;
    IOStorageCompletion storageCompletion;
    
    isMirror = (arSetLevel == kAppleRAIDMirror);
    isWrite = (storageRequest->srMemoryDescriptorDirection == kIODirectionOut);
    
    status = kIOReturnSuccess;
    if (isMirror && isWrite) byteCount = storageRequest->srByteCount;
    else byteCount = 0;
    
    // Collect the status and byte count for each slice.
    for (cnt = 0; cnt < arSliceCount; cnt++) {
        // Ignore missing slices.
        if (arSliceMediaStates[cnt] >= kAppleRAIDSliceMediaStateStopping) continue;
        if (arSliceMedias[cnt] == 0) continue;
        
        // Return any status errors.
        if (storageRequest->srSliceStatus[cnt] != kIOReturnSuccess) {
            status = storageRequest->srSliceStatus[cnt];
            byteCount = 0;
            break;
        }
        
        // Mirrored writes need to all have the same byte count.
        if (isMirror && isWrite) {
            if (byteCount != storageRequest->srSliceByteCounts[cnt]) {
                byteCount = 0;                
                break;
            }
        } else {
            byteCount += storageRequest->srSliceByteCounts[cnt];
        }
    }
    
    // Return an underrun error if the byte count is not complete.
    // This can happen if one or more slices reported a smaller byte count.
    if ((status == kIOReturnSuccess) && (byteCount != storageRequest->srByteCount)) {
        status = kIOReturnUnderrun;
        byteCount = 0;
    }
    
    // If an error has been reported, and the set is a mirror, and the error is reported
    // against an active slice, try to degrade the given slice.
    if ((status != kIOReturnSuccess) && isMirror && (cnt < arSliceCount)) {

	IOLog("AppleRAID::completeRAIDRequest - error detected, status = 0x%x\n", status);

	storageRequest->_srStatus = status;
        storageRequest->_srByteCount = byteCount;
        
        if (degradeSliceMedia(arSliceMedias[cnt], storageRequest, status) == kIOReturnSuccess) shouldCompleteErrors = true;
    } else {
        storageRequest->srMemoryDescriptor->release();
        storageCompletion = storageRequest->srCompletion;
        
        returnRAIDRequest(storageRequest);
        
        // If the set is paused, degrading and there are no pending request, continue degrading the set.
        if (_arSetIsPaused && _arSetIsDegrading && (_arStorageRequestsPending == 0)) {
            if (degradeSliceMedia(0, 0, 0) == kIOReturnSuccess) shouldCompleteErrors = true;
        }
        
        // Call the clients completion routine.
        IOStorage::complete(storageCompletion, status, byteCount);
    }
    
    if (shouldCompleteErrors) {
        while (1) {
            storageRequest = (AppleRAIDStorageRequest *)_arStorageRequestErrorPool->getCommand(false);
            if (storageRequest == 0) break;
            
            storageRequest->srMemoryDescriptor->release();
            storageCompletion	= storageRequest->srCompletion;
            status		= storageRequest->_srStatus;
            byteCount		= storageRequest->_srByteCount;
            
            returnRAIDRequest(storageRequest);
            
            // Call the client's completion routine.
            IOStorage::complete(storageCompletion, status, byteCount);
        }
    }
}

IOReturn AppleRAID::requestSliceTerminate(IOMedia *media)
{
    UInt32	sliceNumber;
    
    if (getSliceNumberForMedia(media, &sliceNumber)) {
        _arSliceTerminatesPending++;
        arSliceRequestTerminates[sliceNumber] = true;
        
        if ((_arSliceTerminatesPending + _arSliceTerminatesActive) == _arSlicesStarted) {
            _arSetIsTerminating = true;
        }
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::waitForSliceTerminateRequests(void)
{
    while (_arSliceTerminatesPending != 0) {
        _arSetCommandGate->commandSleep(&_arSliceTerminatesPending, THREAD_UNINT);
    }
    
    return kIOReturnSuccess;
}

bool AppleRAID::changeSliceMediaState(UInt32 sliceNumber, UInt32 newState)
{
    UInt32	*state = arSliceMediaStates + sliceNumber;
    bool	setState = false;
    
    if (arSliceMedias[sliceNumber] == 0) return false;
    
    switch (newState) {
        case kAppleRAIDSliceMediaStateOpen :
            while (*state == kAppleRAIDSliceMediaStateClosing)
                _arSetCommandGate->commandSleep(state, THREAD_UNINT);
            if ((*state == kAppleRAIDSliceMediaStateClosed) || (*state == kAppleRAIDSliceMediaStateOpen)) {
                setState = true;
            }
            break;
        
        case kAppleRAIDSliceMediaStateClosing :
            if (*state == kAppleRAIDSliceMediaStateOpen) {
                setState = true;
            }
            break;
        
        case kAppleRAIDSliceMediaStateClosed :
            if (*state == kAppleRAIDSliceMediaStateClosing) {
                _arSetCommandGate->commandWakeup(state, false);
                setState = true;
            }
            break;
        
        case kAppleRAIDSliceMediaStateStopping :
            if (*state != kAppleRAIDSliceMediaStateStopped) {
                setState = true;
            }
            break;
        
        case kAppleRAIDSliceMediaStateStopped :
            if (*state == kAppleRAIDSliceMediaStateStopping) {
                setState = true;
                arSliceMedias[sliceNumber] = 0;
            }
            break;
    }
    
    if (setState) *state = newState;
    
    return setState;
}

IOReturn AppleRAID::openSliceMedias(IOOptionBits options, IOStorageAccess access)
{
    UInt32	cnt;
    IOMedia	*sliceMedia;
    bool	shouldOpen, didOpen = true;
    
    for (cnt = 0; cnt < arSliceCount; cnt++) {
        if (arSliceMediaStates[cnt] >= kAppleRAIDSliceMediaStateStopping) continue;
        sliceMedia = arSliceMedias[cnt];
        if (sliceMedia == 0) continue;
        
        shouldOpen = _arSetCommandGate->runAction(_arActionChangeSliceMediaState,
                                                  (void *)cnt,
                                                  (void *)kAppleRAIDSliceMediaStateOpen);
        
        didOpen &= shouldOpen && sliceMedia->open(this, options, access);
    }
    
    return didOpen ? kIOReturnSuccess : kIOReturnNotOpen;
}

IOReturn AppleRAID::closeSliceMedias(IOOptionBits options)
{
    UInt32	cnt, toState;
    IOMedia	*sliceMedia;
    
    for (cnt = 0; cnt < arSliceCount; cnt++) {
        sliceMedia = arSliceMedias[cnt];
        if (sliceMedia == 0) continue;
	
        if ((arSliceMediaStates[cnt] == kAppleRAIDSliceMediaStateClosing) ||
            (arSliceMediaStates[cnt] == kAppleRAIDSliceMediaStateStopping)) {
            sliceMedia->close(this, options);
            
            if (arSliceMediaStates[cnt] == kAppleRAIDSliceMediaStateClosing) {
                toState = kAppleRAIDSliceMediaStateClosed;
            } else {
                toState = kAppleRAIDSliceMediaStateStopped;
            }
            
            _arSetCommandGate->runAction(_arActionChangeSliceMediaState, (void *)cnt, (void *)toState);
        }
    }
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::requestSynchronizeCache(void)
{
    while (_arSetIsSyncing) {
        _arSetCommandGate->commandSleep(&_arSetIsSyncing, THREAD_UNINT);
    }
    
    _arSetIsSyncing = true;
    _arSetIsPaused = true;

    // kick off a thread for each slice
    for (UInt32 cnt = 0; cnt < arSliceCount; cnt++) {
	if (arSliceMediaStates[cnt] >= kAppleRAIDSliceMediaStateStopping) continue;
	if (arSliceMedias[cnt] == 0) continue;
        
	_arSetIsSyncingCount++;
	thread_call_enter1(_arSyncronizeCacheThreadCall[cnt], (thread_call_param_t)arSliceMedias[cnt]);
    }
    
    // wait for slices to complete
    while (_arSetIsSyncingCount) {
	_arSetCommandGate->commandSleep(&_arSetIsSyncingCount, THREAD_UNINT);
    }
    
    // clean up
    _arSetIsSyncing = false;
    if (!_arSetIsDegrading) {
        _arSetIsPaused = false;
        _arSetCommandGate->commandWakeup(&_arSetIsPaused, false);
    }
    
    _arSetCommandGate->commandWakeup(&_arSetIsSyncing, false);
    
    return kIOReturnSuccess;
}

IOReturn AppleRAID::synchronizeCacheSlice(IOMedia * slice)
{
    IOReturn	result = kIOReturnSuccess;
        
    // XXX check the return value somewhere?
    result = slice->synchronizeCache(this);

    _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::completeSynchronizeCacheSlice);
    
    return result;
}

void AppleRAID::completeSynchronizeCacheSlice(void)
{
    _arSetIsSyncingCount--;
    if (_arSetIsSyncingCount == 0) {
	_arSetCommandGate->commandWakeup(&_arSetIsSyncingCount, false);
    }
}

bool AppleRAID::handleOpen(IOService *client, IOOptionBits options, void *argument)
{
    IOReturn		result = kIOReturnSuccess;
    bool		wasReader = _arSetReaders->containsObject(client);
    IOStorageAccess	newAccess, access = (IOStorageAccess)argument;
    
    if (access == kIOStorageAccessReaderWriter) {
        if ((client == _arSetWriter) || (client == _arSetWriterSelf)) return true;
        
        // Only allow one non-self writer.
        if ((client != this) && (_arSetWriter != 0)) return false;
        
        newAccess = kIOStorageAccessReaderWriter;
    } else {
        if (wasReader) return true;
        
        newAccess = kIOStorageAccessReader;
    }
    
    if (newAccess > _arSetOpenLevel) {
        result = openSliceMedias(options, newAccess);
        if (result == kIOReturnSuccess) _arSetOpenLevel = newAccess;
    }
    
    if (result == kIOReturnSuccess) {
        if (access == kIOStorageAccessReaderWriter) {
            if (client == this) _arSetWriterSelf = client;
            else _arSetWriter = client;
        } else {
            _arSetReaders->setObject(client);
        }
        if (wasReader) {
            _arSetReaders->removeObject(client);
        }
    }
    
    return (result == kIOReturnSuccess);
}

bool AppleRAID::handleIsOpen(const IOService *client) const
{
    bool setIsOpen;
    
    if (client == 0) {
        setIsOpen = (_arSetOpenLevel != kIOStorageAccessNone);
    } else {
        setIsOpen = ((client == _arSetWriter) || (client == _arSetWriterSelf) || _arSetReaders->containsObject(client));
    }
    
    return setIsOpen;
}

void AppleRAID::handleClose(IOService *client, IOOptionBits options)
{
    UInt32		readers, cnt;
    IOStorageAccess	newAccess = _arSetOpenLevel;
    
    if (client == _arSetWriter) _arSetWriter = 0;
    else if (client == _arSetWriterSelf) _arSetWriterSelf = 0;
    else {
        _arSetReaders->removeObject(client);
    }
    
    // Set kIOStorageAccessReader if there are no writers.
    if ((_arSetWriter == 0) && (_arSetWriterSelf == 0)) newAccess = kIOStorageAccessReader;
    
    // Set kIOStorageAccessNone if there are no readers and no writers.
    readers = _arSetReaders->getCount();
    if ((newAccess != kIOStorageAccessReaderWriter) && (readers == 0)) {
        newAccess = kIOStorageAccessNone;
    }
    
    if (newAccess != _arSetOpenLevel) {
        if (newAccess == kIOStorageAccessReader) {
            openSliceMedias(options, kIOStorageAccessReader);
        } else {
            for (cnt = 0; cnt < arSliceCount; cnt++) {
                _arSetCommandGate->runAction(_arActionChangeSliceMediaState,
                                             (void *)cnt,
                                             (void *)kAppleRAIDSliceMediaStateClosing);
            }
            thread_call_enter1(_arSliceCloseThreadCall, (thread_call_param_t)0);
        }
        _arSetOpenLevel = newAccess;
    }
}

void AppleRAID::read(IOService *client, UInt64 byteStart,
                     IOMemoryDescriptor *buffer, IOStorageCompletion completion)
{
    AppleRAIDStorageRequest *storageRequest;
    
    _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::allocateRAIDRequest, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->read(client, byteStart, buffer, completion);
    } else {
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

void AppleRAID::write(IOService *client, UInt64 byteStart,
                      IOMemoryDescriptor *buffer, IOStorageCompletion completion)
{
    AppleRAIDStorageRequest *storageRequest;
    
    _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::allocateRAIDRequest, &storageRequest);
    
    if (storageRequest != 0) {
        buffer->retain();
        storageRequest->write(client, byteStart, buffer, completion);
    } else {
        IOStorage::complete(completion, kIOReturnNoMedia, 0);
    }
}

IOReturn AppleRAID::synchronizeCache(IOService *client)
{
    return _arSetCommandGate->runAction((IOCommandGate::Action)&AppleRAID::requestSynchronizeCache);
}


// AppleRAIDStorageRequest

#undef super
#define super IOCommand
OSDefineMetaClassAndStructors(AppleRAIDStorageRequest, IOCommand);

void AppleRAIDStorageRequest::free(void)
{
    UInt32	cnt;
    
    if (_srSliceMemoryDescriptors != 0) {
        for (cnt = 0; cnt < srSliceCount; cnt++)
            if (_srSliceMemoryDescriptors[cnt] != 0)
                _srSliceMemoryDescriptors[cnt]->release();
        
        IODelete(_srSliceMemoryDescriptors, AppleRAIDMemoryDescriptor *, srSliceCount);
    }
    
    if (srSliceStatus != 0) IODelete(srSliceStatus, IOReturn, srSliceCount);
    if (srSliceByteCounts != 0) IODelete(srSliceByteCounts, UInt64, srSliceCount);
    
    super::free();
}

AppleRAIDStorageRequest *AppleRAIDStorageRequest::withAppleRAIDSet(AppleRAID *appleRAID)
{
    AppleRAIDStorageRequest *storageRequest = new AppleRAIDStorageRequest;
    
    if (storageRequest != 0) {
        if (!storageRequest->initWithAppleRAIDSet(appleRAID)) {
            storageRequest->release();
            storageRequest = 0;
        }
    }
    
    return storageRequest;
}

bool AppleRAIDStorageRequest::initWithAppleRAIDSet(AppleRAID *appleRAID)
{
    UInt32			cnt;
    AppleRAIDMemoryDescriptor 	*memoryDescriptor;
    
    if (!super::init()) return false;
    
    _srAppleRAID   = appleRAID;
    
    _srEventSource	= appleRAID->arSetEventSource;
    srSetLevel		= appleRAID->arSetLevel;
    srSetBlockSize   	= appleRAID->arSetBlockSize;
    srSliceCount	= appleRAID->arSliceCount;
    srSliceBaseOffset	= appleRAID->arHeaderSize;
    _srSliceMedias 	= appleRAID->arSliceMedias;
    
    srSliceStatus = IONew(IOReturn, srSliceCount);
    if (srSliceStatus == 0) return false;
    
    srSliceByteCounts = IONew(UInt64, srSliceCount);
    if (srSliceByteCounts == 0) return false;
    
    _srSliceMemoryDescriptors = IONew(AppleRAIDMemoryDescriptor *, srSliceCount);
    if (_srSliceMemoryDescriptors == 0) return false;
    
    for (cnt = 0; cnt < srSliceCount; cnt++) {
        switch (srSetLevel) {
            case kAppleRAIDStripe : memoryDescriptor = AppleRAIDStripeMemoryDescriptor::withStorageRequest(this, cnt); break;
            case kAppleRAIDMirror : memoryDescriptor = AppleRAIDMirrorMemoryDescriptor::withStorageRequest(this, cnt); break;
            case kAppleRAIDConcat : memoryDescriptor = AppleRAIDConcatMemoryDescriptor::withStorageRequest(this, cnt); break;
            default : memoryDescriptor = 0; break;
        }
        if (memoryDescriptor == 0) return false;
        
        _srSliceMemoryDescriptors[cnt] = memoryDescriptor;
    }
    
    return true;
}

void AppleRAIDStorageRequest::setSliceData(UInt32 slicesStarted, UInt32 *logicalSliceNumbers)
{
    UInt32 cnt;
    
    srSlicesStarted = slicesStarted;
    
    for (cnt = 0; cnt < srSliceCount; cnt++) {
        _srSliceMemoryDescriptors[cnt]->setSliceData(logicalSliceNumbers[cnt]);
    }
}

void AppleRAIDStorageRequest::read(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
                                   IOStorageCompletion completion)
{
    UInt32			cnt;
    IOMedia			*media;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srCompletion 		= completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;
    
    storageCompletion.target    = _srEventSource;
    storageCompletion.action    = _srEventSource->getStorageCompletionAction();
    
    for (cnt = 0; cnt < srSliceCount; cnt++) {
        memoryDescriptor = _srSliceMemoryDescriptors[cnt];
        if (_srAppleRAID->arSliceMediaStates[cnt] < kAppleRAIDSliceMediaStateStopping) {
            media = _srSliceMedias[cnt];
        } else {
            media = 0;
        }
        if ((media != 0) && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart)) {
            storageCompletion.parameter = memoryDescriptor;
            media->read(_srAppleRAID, srSliceBaseOffset + memoryDescriptor->mdSliceByteStart,
                        memoryDescriptor, storageCompletion);
        } else {
            _srEventSource->sliceCompleteRequest(memoryDescriptor, kIOReturnSuccess, 0);
        }
    }
}

void AppleRAIDStorageRequest::write(IOService *client, UInt64 byteStart, IOMemoryDescriptor *buffer,
                                    IOStorageCompletion completion)
{
    UInt32			cnt;
    IOMedia			*media;
    IOStorageCompletion		storageCompletion;
    AppleRAIDMemoryDescriptor	*memoryDescriptor;
    
    srClient			= client;
    srCompletion 		= completion;
    srCompletedCount		= 0;
    srMemoryDescriptor 		= buffer;
    srMemoryDescriptorDirection	= buffer->getDirection();
    srByteCount 		= buffer->getLength();
    srByteStart			= byteStart;
    
    storageCompletion.target    = _srEventSource;
    storageCompletion.action    = _srEventSource->getStorageCompletionAction();
    
    for (cnt = 0; cnt < srSliceCount; cnt++) {
        memoryDescriptor = _srSliceMemoryDescriptors[cnt];
        if (_srAppleRAID->arSliceMediaStates[cnt] < kAppleRAIDSliceMediaStateStopping) {
            media = _srSliceMedias[cnt];
        } else {
            media = 0;
        }
        if ((media != 0) && memoryDescriptor->configureForMemoryDescriptor(buffer, byteStart)) {
            storageCompletion.parameter = memoryDescriptor;
            media->write(_srAppleRAID, srSliceBaseOffset + memoryDescriptor->mdSliceByteStart,
                         memoryDescriptor, storageCompletion);
        } else {
            _srEventSource->sliceCompleteRequest(memoryDescriptor, kIOReturnSuccess, 0);
        }
    }
}

// AppleRAIDMemoryDescriptor

#undef super
#define super IOMemoryDescriptor
OSDefineMetaClassAndAbstractStructors(AppleRAIDMemoryDescriptor, IOMemoryDescriptor);

bool AppleRAIDMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    if (!super::init()) return false;
    
    _mdMemoryDescriptorLock = IOLockAlloc();
    if (_mdMemoryDescriptorLock == 0) return false;
    
    mdStorageRequest = storageRequest;
    
    mdSliceNumber = sliceNumber;
    
    return true;
}

void AppleRAIDMemoryDescriptor::setSliceData(UInt32 logicalSliceNumber)
{
    mdLogicalSliceNumber = logicalSliceNumber;
}

IOReturn AppleRAIDMemoryDescriptor::prepare(IODirection forDirection)
{
    IOReturn result;
    
    IOLockLock(_mdMemoryDescriptorLock);
    result = _mdMemoryDescriptor->prepare(forDirection);
    IOLockUnlock(_mdMemoryDescriptorLock);
    
    return result;
}

IOReturn AppleRAIDMemoryDescriptor::complete(IODirection forDirection)
{
    IOReturn result;
    
    IOLockLock(_mdMemoryDescriptorLock);
    result = _mdMemoryDescriptor->complete(forDirection);
    IOLockUnlock(_mdMemoryDescriptorLock);
    
    return result;
}

// AppleRAIDStripeMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDStripeMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDStripeMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDStripeMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, sliceNumber)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDStripeMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    if (!super::initWithStorageRequest(storageRequest, sliceNumber)) return false;
    
    _mdSliceCount = storageRequest->srSliceCount;
    _mdSetBlockSize = storageRequest->srSetBlockSize;
    
    return true;
}

void AppleRAIDStripeMemoryDescriptor::setSliceData(UInt32 logicalSliceNumber)
{
    super::setSliceData(logicalSliceNumber);
    
    _mdSliceCount = mdStorageRequest->srSlicesStarted;
}

bool AppleRAIDStripeMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart)
{
    UInt32 raidBlockStop, raidBlockEndOffset;
    UInt32 startSlice, stopSlice;
    UInt32 blockCount, sliceBlockCount, sliceBlockStart;
    UInt32 byteCount = memoryDescriptor->getLength();
    
    _mdSetBlockStart	= byteStart / _mdSetBlockSize;
    _mdSetBlockOffset	= byteStart % _mdSetBlockSize;
    startSlice		= _mdSetBlockStart % _mdSliceCount;
    raidBlockStop	= (byteStart + byteCount - 1) / _mdSetBlockSize;
    raidBlockEndOffset	= (byteStart + byteCount - 1) % _mdSetBlockSize;
    stopSlice		= raidBlockStop % _mdSliceCount;
    blockCount		= raidBlockStop - _mdSetBlockStart + 1;
    sliceBlockCount	= blockCount / _mdSliceCount;
    sliceBlockStart	= _mdSetBlockStart / _mdSliceCount;
    
    if (((_mdSliceCount + mdLogicalSliceNumber - startSlice) % _mdSliceCount) < (blockCount % _mdSliceCount)) sliceBlockCount++;
    
    if (startSlice > mdLogicalSliceNumber) sliceBlockStart++;
    
    mdSliceByteStart = (UInt64)sliceBlockStart * _mdSetBlockSize;
    _length = sliceBlockCount * _mdSetBlockSize;
    
    if (startSlice == mdLogicalSliceNumber) {
        mdSliceByteStart += _mdSetBlockOffset;
        _length -= _mdSetBlockOffset;
    }
        
    if (stopSlice == mdLogicalSliceNumber) _length -= _mdSetBlockSize - raidBlockEndOffset - 1;
    
    _mdMemoryDescriptor = memoryDescriptor;
    
    _direction = memoryDescriptor->getDirection();
    
    return _length != 0;
}

IOPhysicalAddress AppleRAIDStripeMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length)
{
    UInt32		sliceBlockNumber = (mdSliceByteStart + offset) / _mdSetBlockSize;
    UInt32		sliceBlockOffset = (mdSliceByteStart + offset) % _mdSetBlockSize;
    UInt32		raidBlockNumber = sliceBlockNumber * _mdSliceCount + mdLogicalSliceNumber - _mdSetBlockStart;
    IOByteCount		raidOffset = raidBlockNumber * _mdSetBlockSize + sliceBlockOffset - _mdSetBlockOffset;
    IOPhysicalAddress	physAddress;
    
    physAddress = _mdMemoryDescriptor->getPhysicalSegment(raidOffset, length);
    
    sliceBlockOffset = _mdSetBlockSize - sliceBlockOffset;
    if (*length > sliceBlockOffset) *length = sliceBlockOffset;
    
    return physAddress;
}

// AppleRAIDMirrorMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDMirrorMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDMirrorMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDMirrorMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, sliceNumber)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDMirrorMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    if (!super::initWithStorageRequest(storageRequest, sliceNumber)) return false;
    
    _mdSliceCount = storageRequest->srSliceCount;
    _mdSetBlockSize = storageRequest->srSetBlockSize;
    
    return true;
}

void AppleRAIDMirrorMemoryDescriptor::setSliceData(UInt32 logicalSliceNumber)
{
    super::setSliceData(logicalSliceNumber);
    
    _mdSliceCount = mdStorageRequest->srSlicesStarted;
}

bool AppleRAIDMirrorMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart)
{
    UInt32 raidBlockStop, raidBlockEndOffset;
    UInt32 virtualSliceNumber, startSlice, stopSlice;
    UInt32 blockCount, extraBlocks, sliceBlockCount, sliceBlockStart;
    UInt32 byteCount = memoryDescriptor->getLength();
    
    _direction = memoryDescriptor->getDirection();
    
    if (_direction == kIODirectionOut) {
        mdSliceByteStart = byteStart;
        _length = byteCount;
    } else {
        _mdSetBlockStart	= byteStart / _mdSetBlockSize;
        _mdSetBlockOffset	= byteStart % _mdSetBlockSize;
        startSlice		= _mdSetBlockStart % _mdSliceCount;
        raidBlockStop		= (byteStart + byteCount - 1) / _mdSetBlockSize;
        raidBlockEndOffset	= (byteStart + byteCount - 1) % _mdSetBlockSize;
        stopSlice		= raidBlockStop % _mdSliceCount;
        blockCount		= raidBlockStop - _mdSetBlockStart + 1;
        sliceBlockCount		= blockCount / _mdSliceCount;
        extraBlocks		= blockCount % _mdSliceCount;
        virtualSliceNumber	= (_mdSliceCount + mdLogicalSliceNumber - startSlice) % _mdSliceCount;
	sliceBlockStart		= _mdSetBlockStart + virtualSliceNumber * sliceBlockCount + min(virtualSliceNumber, extraBlocks);
        
	if (virtualSliceNumber < extraBlocks) sliceBlockCount++;
        
        mdSliceByteStart = (UInt64)sliceBlockStart * _mdSetBlockSize;
        _length = sliceBlockCount * _mdSetBlockSize;
        
        if (virtualSliceNumber == 0) {
            mdSliceByteStart += _mdSetBlockOffset;
            _length -= _mdSetBlockOffset;
        }
        
        if (virtualSliceNumber == min(blockCount - 1, _mdSliceCount - 1)) _length -= _mdSetBlockSize - raidBlockEndOffset - 1;
    }
    
    _mdMemoryDescriptor = memoryDescriptor;
    
    return _length != 0;
}

IOPhysicalAddress AppleRAIDMirrorMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length)
{
    UInt32		sliceBlockNumber, sliceBlockOffset, raidBlockNumber;
    IOByteCount		raidOffset = offset;
    IOPhysicalAddress	physAddress;
    
    if (_direction != kIODirectionOut) {
        sliceBlockNumber = (mdSliceByteStart + offset) / _mdSetBlockSize;
        sliceBlockOffset = (mdSliceByteStart + offset) % _mdSetBlockSize;
        raidBlockNumber = sliceBlockNumber - _mdSetBlockStart;
        raidOffset = raidBlockNumber * _mdSetBlockSize + sliceBlockOffset - _mdSetBlockOffset;
    }
    
    physAddress = _mdMemoryDescriptor->getPhysicalSegment(raidOffset, length);
    
    return physAddress;
}

// AppleRAIDConcatMemoryDescriptor

#undef super
#define super AppleRAIDMemoryDescriptor
OSDefineMetaClassAndStructors(AppleRAIDConcatMemoryDescriptor, AppleRAIDMemoryDescriptor);

AppleRAIDMemoryDescriptor *
AppleRAIDConcatMemoryDescriptor::withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    AppleRAIDMemoryDescriptor *memoryDescriptor = new AppleRAIDConcatMemoryDescriptor;
    
    if (memoryDescriptor != 0) {
        if (!memoryDescriptor->initWithStorageRequest(storageRequest, sliceNumber)) {
            memoryDescriptor->release();
            memoryDescriptor = 0;
        }
    }
    
    return memoryDescriptor;
}

bool AppleRAIDConcatMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber)
{
    UInt64 sliceSize;
    
    if (!super::initWithStorageRequest(storageRequest, sliceNumber)) return false;
    
    sliceSize = storageRequest->srSetMediaSize / storageRequest->srSliceCount;
    
    _mdSliceStart = sliceSize * sliceNumber;
    _mdSliceEnd   = (sliceSize + 1) * sliceNumber - 1;
    
    return true;
}

bool AppleRAIDConcatMemoryDescriptor::configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart)
{
    UInt32 byteCount = memoryDescriptor->getLength();
    UInt32 byteEnd = byteStart + byteCount - 1;
    
    if ((byteEnd < _mdSliceStart) || (byteStart > _mdSliceEnd)) return false;
    
    if (byteStart < _mdSliceStart) {
        mdSliceByteStart = 0;
        _mdSliceOffset = _mdSliceStart - byteStart;
        byteCount -= _mdSliceOffset;
    } else {
        mdSliceByteStart = byteStart - _mdSliceStart;
        _mdSliceOffset = 0;
    }
    
    if (byteEnd > _mdSliceEnd) {
        byteCount -= byteEnd - _mdSliceEnd;
    }
    
    _length = byteCount;
    
    _mdMemoryDescriptor = memoryDescriptor;
    
    _direction = memoryDescriptor->getDirection();
    
    return true;

}

IOPhysicalAddress AppleRAIDConcatMemoryDescriptor::getPhysicalSegment(IOByteCount offset, IOByteCount *length)
{
    IOByteCount		raidOffset = offset + _mdSliceOffset;
    IOPhysicalAddress	physAddress;
    
    physAddress = _mdMemoryDescriptor->getPhysicalSegment(raidOffset, length);
    
    return physAddress;
}

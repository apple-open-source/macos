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

#ifndef _APPLERAID_H
#define _APPLERAID_H

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommand.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/storage/IOStorage.h>
#include <IOKit/storage/IOMedia.h>

#include "AppleRAIDHeader.h"

#define kAppleRAIDStorageRequestCount (16)

enum {
    kAppleRAIDSliceMediaStateMissing = 0,
    kAppleRAIDSliceMediaStateOpen,
    kAppleRAIDSliceMediaStateClosing,
    kAppleRAIDSliceMediaStateClosed,
    kAppleRAIDSliceMediaStateStopping,
    kAppleRAIDSliceMediaStateStopped
};

class AppleRAIDController;

class AppleRAIDEventSource;

class AppleRAIDStorageRequest;
class AppleRAIDMemoryDescriptor;
class AppleRAIDStripeMemoryDescriptor;
class AppleRAIDMirrorMemoryDescriptor;
class AppleRAIDConcatMemoryDescriptor;

class AppleRAID : public IOStorage
{
    OSDeclareDefaultStructors(AppleRAID);
    
    friend class AppleRAIDStorageRequest;
    
private:
    IOLock			*_arSetLock;
    const OSSymbol		*_arSetName;
    const OSSymbol		*_arSetUniqueName;
    UInt32			_arSetUUID[4];
    AppleRAIDController		*_arController;
    IOMedia			*_arSetMedia;
    IORegistryEntry		**_arSetDTParents;
    char			*_arSetDTLocations;
    char			*_arSetDTPaths;
    bool			_arSetIsPaused;
    bool			_arSetIsDegrading;
    bool			_arSetIsSyncing;
    int				_arSetIsSyncingCount;
    bool			_arSetIsEjectable;
    bool			_arSetIsWritable;
    bool			_arSetIsTerminating;
    bool			_arSetStartedOnce;
    bool			_arSetUpdatePending;
    UInt32			_arSetNativeBlockSize;
    IOWorkLoop			*_arSetWorkLoop;
    IOCommandGate		*_arSetCommandGate;
    IOTimerEventSource		*_arSetTimerEventSource;
    IOStorageAccess		_arSetOpenLevel;
    OSSet			*_arSetReaders;
    IOService			*_arSetWriter;
    IOService			*_arSetWriterSelf;
    UInt32			_arStorageRequestsPending;
    UInt32			_arStorageRequestsErrorsPending;
    IOCommandPool		*_arStorageRequestPool;
    IOCommandPool		*_arStorageRequestErrorPool;
    UInt32			_arFirstSlice;
    UInt32			_arSlicesStarted;
    UInt32			_arSliceTerminatesPending;
    UInt32			_arSliceTerminatesActive;
    thread_call_t		_arSliceCloseThreadCall;
    thread_call_t		_arUpdateHeadersThreadCall;
    thread_call_t		*_arSyncronizeCacheThreadCall;
    IOCommandGate::Action	_arActionChangeSliceMediaState;
    IOBufferMemoryDescriptor	*_arHeaderBuffer;
    AppleRAIDHeader		*_arHeader;
    
    virtual void free(void);
    virtual IOReturn addSliceMedia(IOMedia *media);
    virtual IOReturn removeSliceMedia(IOMedia *media);
    virtual IOReturn degradeSliceMedia(IOMedia *media, AppleRAIDStorageRequest *storageRequest, IOReturn status);
    virtual bool getSliceNumberForMedia(IOMedia *media, UInt32 *sliceNumber);
    virtual IOReturn initRAIDSet(void);
    virtual IOReturn publishRAIDMedia(void);
    virtual IOReturn updateRAIDHeaders(void);
    virtual IOReturn updateRAIDHeadersDone(void);
    virtual void raidTimeOut(IOTimerEventSource *sender);
    virtual IOReturn allocateRAIDRequest(AppleRAIDStorageRequest **storageRequest);
    virtual void returnRAIDRequest(AppleRAIDStorageRequest *storageRequest);
    virtual void completeRAIDRequest(AppleRAIDStorageRequest *storageRequest);
    virtual IOReturn requestSliceTerminate(IOMedia *media);
    virtual IOReturn waitForSliceTerminateRequests(void);
    virtual bool changeSliceMediaState(UInt32 sliceNumber, UInt32 newState);
    virtual IOReturn openSliceMedias(IOOptionBits options, IOStorageAccess access);
    virtual IOReturn closeSliceMedias(IOOptionBits options);
    virtual IOReturn requestSynchronizeCache(void);
    virtual IOReturn synchronizeCacheSlice(IOMedia * slice);
    virtual void completeSynchronizeCacheSlice(void);
    
protected:
    AppleRAIDEventSource	*arSetEventSource;
    UInt32			arSetLevel;
    UInt32			arSetBlockSize;
    UInt32			arSetBlockCount;
    UInt64			arSetMediaSize;
    UInt32			arSliceCount;
    UInt32                  	arHeaderSize;
    UInt32			arHeaderSequence;
    UInt32			*arLogicalSliceNumbers;
    UInt32			*arSliceMediaStates;
    bool			*arSliceRequestTerminates;
    IOReturn			*arSliceMediaErrors;
    IOMedia			**arSliceMedias;
    
    virtual bool handleOpen(IOService *client, IOOptionBits options, void *argument);
    virtual bool handleIsOpen(const IOService *client) const;
    virtual void handleClose(IOService *client, IOOptionBits options);    
    
public:
    virtual bool init(OSDictionary *properties = 0);
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual bool requestTerminate(IOService *provider, IOOptionBits options);
    virtual bool willTerminate(IOService *provider, IOOptionBits options);
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer);
    virtual IOWorkLoop *getWorkLoop(void);
    virtual void read(IOService *client, UInt64 byteStart,
                      IOMemoryDescriptor *buffer, IOStorageCompletion completion);
    virtual void write(IOService *client, UInt64 byteStart,
                       IOMemoryDescriptor *buffer, IOStorageCompletion completion);
    virtual IOReturn synchronizeCache(IOService *client);
    virtual IOReturn terminateRAIDMedia(IOMedia *media);
};

class AppleRAIDStorageRequest : public IOCommand
{
    OSDeclareDefaultStructors(AppleRAIDStorageRequest);
    
    friend class AppleRAID;
    friend class AppleRAIDEventSource;
    friend class AppleRAIDMemoryDescriptor;
    friend class AppleRAIDStripeMemoryDescriptor;
    friend class AppleRAIDMirrorMemoryDescriptor;
    friend class AppleRAIDConcatMemoryDescriptor;
    
private:
    AppleRAID			*_srAppleRAID;
    AppleRAIDEventSource	*_srEventSource;
    IOMedia			**_srSliceMedias;
    AppleRAIDMemoryDescriptor   **_srSliceMemoryDescriptors;
    IOReturn			_srStatus;
    UInt64			_srByteCount;
    
    virtual void free(void);
    
protected:
    UInt32			srSetLevel;
    UInt32			srSetBlockSize;
    UInt64			srSetMediaSize;
    UInt32			srSlicesStarted;
    UInt32			srSliceBaseOffset;
    UInt32			srSliceCount;
    IOReturn                    *srSliceStatus;
    UInt64			*srSliceByteCounts;
    UInt32			srCompletedCount;
    UInt64			srByteStart;
    UInt64			srByteCount;
    IOService			*srClient;
    IOStorageCompletion		srCompletion;
    IOMemoryDescriptor		*srMemoryDescriptor;
    IODirection			srMemoryDescriptorDirection;
    
    virtual void read(IOService *client, UInt64 byteStart, IOMemoryDescriptor * buffer,
                      IOStorageCompletion completion);
    virtual void write(IOService *client, UInt64 byteStart, IOMemoryDescriptor * buffer,
                       IOStorageCompletion completion);
    
public:
    static AppleRAIDStorageRequest *withAppleRAIDSet(AppleRAID *appleRAID);
    virtual bool initWithAppleRAIDSet(AppleRAID *appleRAID);
    virtual void setSliceData(UInt32 slicesStarted, UInt32 *logicalSliceNumbers);
};

class AppleRAIDMemoryDescriptor : public IOMemoryDescriptor
{
    OSDeclareAbstractStructors(AppleRAIDMemoryDescriptor);
    
    friend class AppleRAIDEventSource;
    friend class AppleRAIDStorageRequest;
    friend class AppleRAIDStripeMemoryDescriptor;
    friend class AppleRAIDMirrorMemoryDescriptor;
    friend class AppleRAIDConcatMemoryDescriptor;
    
private:
    IOMemoryDescriptor		*_mdMemoryDescriptor;
    IOLock			*_mdMemoryDescriptorLock;
    
protected:
    AppleRAIDStorageRequest	*mdStorageRequest;
    UInt32			mdSliceNumber;
    UInt32			mdLogicalSliceNumber;
    UInt64			mdSliceByteStart;
    
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    
    virtual bool initWithAddress(void *address, IOByteCount withLength, IODirection withDirection) { return false; }
    virtual bool initWithAddress(vm_address_t address, IOByteCount withLength, IODirection withDirection, task_t withTask)
                                 { return false; }
    virtual bool initWithPhysicalAddress(IOPhysicalAddress address, IOByteCount withLength, IODirection withDirection)
                                         { return false; }
    virtual bool initWithRanges(IOVirtualRange *ranges, UInt32 withCount, IODirection withDirection, task_t withTask,
                                bool asReference = false) { return false; }
    virtual bool initWithPhysicalRanges(IOPhysicalRange *ranges, UInt32 withCount, IODirection withDirection,
                                        bool asReference = false) { return false; }
    
    virtual void setSliceData(UInt32 logicalSliceNumber);
    
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart) = 0;
    
 public:
    virtual IOPhysicalAddress getPhysicalSegment(IOByteCount offset, IOByteCount *length) = 0;
    virtual void *getVirtualSegment(IOByteCount offset, IOByteCount *length) { return 0; }
    virtual IOReturn prepare(IODirection forDirection = kIODirectionNone);
    virtual IOReturn complete(IODirection forDirection = kIODirectionNone);
};

class AppleRAIDStripeMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleRAIDStripeMemoryDescriptor);
    
    friend class AppleRAIDStorageRequest;
    
private:
    UInt32		_mdSliceCount;
    UInt32		_mdSetBlockSize;
    UInt32		_mdSetBlockStart;
    UInt32		_mdSetBlockOffset;
    
protected:
    static AppleRAIDMemoryDescriptor *withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual void setSliceData(UInt32 logicalSliceNumber);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart);
    
 public:
    virtual IOPhysicalAddress getPhysicalSegment(IOByteCount offset, IOByteCount *length);
};

class AppleRAIDMirrorMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleRAIDMirrorMemoryDescriptor);
    
    friend class AppleRAIDStorageRequest;
    
private:
    UInt32		_mdSliceCount;
    UInt32		_mdSetBlockSize;
    UInt32		_mdSetBlockStart;
    UInt32		_mdSetBlockOffset;
    
protected:
    static AppleRAIDMemoryDescriptor *withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual void setSliceData(UInt32 logicalSliceNumber);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart);
    
public:
    virtual IOPhysicalAddress getPhysicalSegment(IOByteCount offset, IOByteCount *length);
};

class AppleRAIDConcatMemoryDescriptor : public AppleRAIDMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleRAIDConcatMemoryDescriptor);
    
    friend class AppleRAIDStorageRequest;
    
private:
    UInt32		_mdSliceOffset;
    UInt64		_mdSliceStart;
    UInt64		_mdSliceEnd;
    
protected:
    static AppleRAIDMemoryDescriptor *withStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual bool initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 sliceNumber);
    virtual bool configureForMemoryDescriptor(IOMemoryDescriptor *memoryDescriptor, UInt64 byteStart);
    
 public:
    virtual IOPhysicalAddress getPhysicalSegment(IOByteCount offset, IOByteCount *length);
};

#endif /* ! _APPLERAID_H */

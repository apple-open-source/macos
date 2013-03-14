/*
 *  AppleUSBXHCIUIM.cpp
 *
 *  Copyright © 2011-2012 Apple Inc. All Rights Reserved.
 *
 */

//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/IOTimerEventSource.h>

#include "AppleUSBXHCIUIM.h"
#include "AppleUSBXHCI_IsocQueues.h"


//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#ifndef XHCI_USE_KPRINTF 
#define XHCI_USE_KPRINTF 0
#endif

#if XHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBLogKP( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#else
// USBLogKP can only be used for kprintf builds
#define USBLogKP( LEVEL, FORMAT, ARGS... )
#endif

#define super							IOUSBControllerV3
#define _controllerCanSleep				_expansionData->_controllerCanSleep
#define CMD_NOT_COMPLETED				(-1)

#define kSegmentTableEventRingEntries			4
#define kXHCIHardwareEventRingBufferSize		(PAGE_SIZE)

// Make this bigger temporarily while we fix the event ring overflows.
#define kXHCISoftwareEventRingBufferSize		(20 * kXHCIHardwareEventRingBufferSize)

//================================================================================================
//
//   AppleUSBXHCI Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(AppleUSBXHCI, IOUSBControllerV3)


#if DEBUG_COMPLETIONS
// Overriding the controller method
void AppleUSBXHCI::Complete(
                            IOUSBCompletion	completion,
                            IOReturn		status,
                            UInt32		actualByteCount )
{
	if(actualByteCount == 0)
	{
        if(status != 0)
        {
            USBLog(2, "AppleUSBXHCI[%p]::Complete - status: %x", this, status);
        }
	}
	else 
	{
		USBLog(6, "AppleUSBXHCI[%p]::Complete - status: %x, shortfall: %d", this, status, (int)actualByteCount);
	}
    
	
	super::Complete(completion, status, actualByteCount);
}
#endif



#if DEBUG_BUFFER
void AppleUSBXHCI::CheckBuf(IOUSBCommand* command)
{
	if(command->GetUIMScratch(kXHCI_ScratchMark))
	{
		IOMemoryMap *mmap;
		UInt32 *mbuf;
		int count, count0;
		UInt32 pat;
		
		mmap = (IOMemoryMap *)command->GetUIMScratch(kXHCI_ScratchMMap);
		mbuf = (UInt32 *)mmap->getVirtualAddress();
		
		pat = command->GetUIMScratch(kXHCI_ScratchPat);
		count = command->GetReqCount();
		count /= 4;
		if(count == 0)
		{
			return;
		}
		count0 = count;
		mbuf = &mbuf[count-1];	// Point to end of buffer
		do{
			if(*mbuf != pat)
			{
				break;
			}
		}while(--count >0);
		
		if(count < count0)
		{
			USBLog(2, "AppleUSBXHCI[%p]::CheckBuf - potential shortfall: %d", this, (count0-count)*4);
		}
		else
		{
			USBLog(2, "AppleUSBXHCI[%p]::CheckBuf - buff fully used", this);
		}
        
		
		mmap->release();
	}
}
#else
#define CheckBuf(c)

#endif

static void mset_pattern4(UInt32 *p, UInt32 pat, int bytecount)
{
	bytecount /= 4;
	while(bytecount-- >0)
	{
		*(p++) = pat;
	}
	
}

// This should eventually be migrated to IOUSBController_Errata.cpp

// Zero for deviceID will be used a wild card to match all devices from a vendor. It is highly unlikely
// that 0 will be used as a device ID but if someone did, then we have to revisit and replace deviceID
// with a range like revisionID

static ErrataListEntry  errataList[] = {
    
    {0x1033, 0x0194, 0, 0xffff,	kXHCIErrata_NEC},	// NEC XHCI, check firmware
    {0x1b73, 0x1000, 0, 0xffff,	kXHCIErrata_NoMSI},	// Fresco Logic XHCI
    {0x8086, 0x1e31, 0, 0xffff,	kXHCIErrataPPT | kXHCIErrataPPTMux | kXHCIErrata_EnableAutoCompliance | kErrataSWAssistXHCIIdle | kXHCIErrata_ParkRing},	// Intel Panther Point
    {0x1b21, 0, 0, 0xffff,	kXHCIErrata_ASMedia},   // ASMedia XHCI
	{0x1b73, 0,	0, 0xffff, kXHCIErrata_FrescoLogic},// Fresco Logic
	{0x1b73, 0x1100, 0, 16, kXHCIErrata_FL1100_Ax},	// Fresco Logic FL1100-Ax
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))

UInt32 AppleUSBXHCI::GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID)
{
    ErrataListEntry		*entryPtr;
    UInt32				i, errata = 0;
    OSBoolean *			pciTunneled = kOSBooleanFalse;
    
    for(i = 0, entryPtr = errataList; i < errataListLength; i++, entryPtr++)
    {
        if (vendorID == entryPtr->vendID &&
            ((deviceID == entryPtr->deviceID) || (entryPtr->deviceID == 0)) &&
            revisionID >= entryPtr->revisionLo &&
            revisionID <= entryPtr->revisionHi)
        {
            // we match, add this errata to our list
            errata |= entryPtr->errata;
        }
    }
    
    pciTunneled = (OSBoolean *) getProperty(kIOPCITunnelledKey, gIOServicePlane);
    if ( pciTunneled == kOSBooleanTrue)
    {
        _v3ExpansionData->_onThunderbolt = true;
        requireMaxBusStall(kXHCIIsochMaxBusStall);
    }
	
    return errata;
}

UInt8 AppleUSBXHCI::Read8Reg(volatile UInt8 *addr)
{
    UInt8 value = 0xFF;
    
    if (!_lostRegisterAccess)
    {
		value = *addr;
		
		if ( (value == 0xFF) && (_v3ExpansionData->_onThunderbolt) )
		{
            _lostRegisterAccess = true;
            USBLog(3, "AppleUSBXHCI[%p]::Read8Reg got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
		}
	}
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIRead8Reg, (uintptr_t)this, (uintptr_t)addr, value, (uintptr_t)_pXHCICapRegisters);
#endif
	
	return value;
}

UInt16 AppleUSBXHCI::Read16Reg(volatile UInt16 *addr)
{
    UInt16 value = 0xFFFF;

    if (!_lostRegisterAccess)
    {
		value = OSReadLittleInt16(addr, 0);
		
		if ( (value == 0xFFFF) && (_v3ExpansionData->_onThunderbolt) )
		{
            _lostRegisterAccess = true;
            USBLog(3, "AppleUSBXHCI[%p]::Read16Reg got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
		}
    }
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIRead16Reg, (uintptr_t)this, (uintptr_t)addr, value, (uintptr_t)_pXHCICapRegisters);
#endif

    return value;
}

UInt32 AppleUSBXHCI::Read32Reg(volatile UInt32 *addr)
{
    UInt32 value = (UInt32) -1;
    
    if (!_lostRegisterAccess)
    {		
		value = OSReadLittleInt32(addr, 0);

		if ( (value == (UInt32) -1) && (_v3ExpansionData->_onThunderbolt) )
		{
            _lostRegisterAccess = true;
            USBLog(3, "AppleUSBXHCI[%p]::Read32Reg got invalid register base = %p reg addr =%p", this, _pXHCICapRegisters, addr);
		}
	}
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIRead32Reg, (uintptr_t)this, (uintptr_t)addr, value,  (uintptr_t)_pXHCICapRegisters);
#endif
	
    return value;
}

UInt64 AppleUSBXHCI::Read64Reg(volatile UInt64 *addr)
{
    UInt64	value = -1ULL;
    
	do
	{
		if (_lostRegisterAccess)
		{
			break;
		}
		
		if(_AC64)
		{
#if __LP64__
			// If its not Fresco logic proceed as normal
			if ((_errataBits & kXHCIErrata_FrescoLogic) == 0)
			{
				value = *addr;
				if ( (value == (UInt64)-1) && (_v3ExpansionData->_onThunderbolt) )
				{
                    _lostRegisterAccess = true;
                    USBLog(3, "AppleUSBXHCI[%p]::Read64Reg got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
                    break;
				}
			}
			// If its Fresco split up their 64 bit registers into two
			else
			{
				UInt32 dwordLo = *(UInt32 *) addr;
				if ( (dwordLo == (UInt32)-1) && (_v3ExpansionData->_onThunderbolt) )
				{
                    _lostRegisterAccess = true;
                    USBLog(3, "AppleUSBXHCI[%p]::Read64Reg Got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
                    break;
				}
				
				UInt32 dwordHi = *((UInt32 *)addr + 1);
				if ( (dwordHi == (UInt32)-1) && (_v3ExpansionData->_onThunderbolt) )
				{
                    _lostRegisterAccess = true;
                    USBLog(3, "AppleUSBXHCI[%p]::Read64Reg got invalid register base = %p reg addr =%p", this, _pXHCICapRegisters, addr);
                    break;
				}
				
				value = (((UInt64) dwordHi) << 32) | dwordLo;
			}
#else
			UInt32 dwordLo = *(UInt32 *) addr;
			if ( (dwordLo == (UInt32)-1) && (_v3ExpansionData->_onThunderbolt) )
			{
                _lostRegisterAccess = true;
                USBLog(3, "AppleUSBXHCI[%p]::Read64Reg got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
                break;
			}
			
			UInt32 dwordHi = *((UInt32 *)addr + 1);
			if ( (dwordLo == (UInt32)-1) && (_v3ExpansionData->_onThunderbolt) )
			{
                _lostRegisterAccess = true;
                USBLog(3, "AppleUSBXHCI[%p]::Read64Reg got invalid register base = %p reg addr = %p", this, _pXHCICapRegisters, addr);
                break;
			}
			
			value = (((UInt64) dwordHi) << 32) | dwordLo;
#endif
		}
		else
		{
			UInt32 dword = *(UInt32 *)addr;
			if ( (dword == (UInt32)-1) && (_v3ExpansionData->_onThunderbolt) )
			{
				_lostRegisterAccess = true;
				USBLog(3, "AppleUSBXHCI[%p]::Read64Reg got invalid register base = %p reg addr =%p", this, _pXHCICapRegisters, addr);
				break;
			}
			
			value = dword;
		}
	} while (0);
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIRead64Reg, (uintptr_t)this, (uintptr_t)addr, OSSwapLittleToHostInt64(value),  (uintptr_t)_pXHCICapRegisters);
#endif
	
    return OSSwapLittleToHostInt64(value);
}

void AppleUSBXHCI::Write32Reg(volatile UInt32 *addr, UInt32 value)
{
    if(!_lostRegisterAccess)
    {
		OSWriteLittleInt32(addr, 0, value);
    }
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIWrite32Reg, (uintptr_t)this, (uintptr_t)addr, value,  (uintptr_t)_pXHCICapRegisters);
#endif
}

void AppleUSBXHCI::Write64Reg(volatile UInt64 *addr, UInt64 value, bool quiet)
{
	if (_lostRegisterAccess)
	{
		return;
	}

	if(_AC64)
	{
#if __LP64__
		if(!quiet)
		{
			USBLog(7, "AppleUSBXHCI[%p]::Write64Reg - writing 64bits %llx to offset %x", this, value, (int)((UInt8 *)addr - (UInt8 *)_pXHCICapRegisters));
		}
		
		if((_errataBits & kXHCIErrata_FrescoLogic) == 0)
		{
			OSWriteLittleInt64(addr, 0, value);
		}
		else
		{
			value = OSSwapHostToLittleInt64(value);
			
			UInt32 *dwordLo = (UInt32 *) addr;
			UInt32 *dwordHi = dwordLo + 1;
			
			*dwordLo = (UInt32)(value & 0xFFFFFFFF);
			*dwordHi = (UInt32)((value >> 32) & 0xFFFFFFFF);
		}
#else
		if(!quiet)
		{
			USBLog(7, "AppleUSBXHCI[%p]::Write64Reg - writing 2x32bits %llx to offset %x", this, value, (int)((UInt8 *)addr - (UInt8 *)_pXHCICapRegisters));
		}
		
		value = OSSwapHostToLittleInt64(value);
		
		UInt32 *dwordLo = (UInt32 *) addr;
		UInt32 *dwordHi = dwordLo + 1;
		
		*dwordLo = (UInt32)(value & 0xFFFFFFFF);
		*dwordHi = (UInt32)((value >> 32) & 0xFFFFFFFF);
#endif
	}
	else
	{
		if(!quiet)
		{
			USBLog(7, "AppleUSBXHCI[%p]::Write64Reg - writing 32bits %llx to offset %x", this, value, (int)((UInt8 *)addr - (UInt8 *)_pXHCICapRegisters));
		}
		OSWriteLittleInt32(addr, 0, (UInt32)value);
	}
	
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	USBTrace(kUSBTXHCI, kTPXHCIWrite64Reg, (uintptr_t)this, (uintptr_t)addr, value, (uintptr_t)_pXHCICapRegisters);
#endif
}

#if (DEBUG_REGISTER_READS == 1)
#define Read32Reg(registerPtr, ...) Read32RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read32RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read32Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)

#define Read64Reg(registerPtr, ...) Read64RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read64RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read64Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)
#endif


int AppleUSBXHCI::DiffTRBIndex(USBPhysicalAddress64 t1, USBPhysicalAddress64 t2)
{
	return(   ((int)(t1-t2))/(int)sizeof(TRB));
}



// Don't emit log message here ,it may be called at interrupt
XHCIRing *
AppleUSBXHCI::GetRing(int slotID, int endpointID, UInt32 stream)
{
	XHCIRing *ring;
	if(stream > _slots[slotID].maxStream[endpointID])
	{
		return(NULL);
	}
	ring = _slots[slotID].rings[endpointID];
    if(ring == NULL)
    {
        return(NULL);
    }
	ring += stream;
	return(ring);
}



XHCIRing *
AppleUSBXHCI::CreateRing(int slotID, int endpointID, UInt32 maxStream)
{
    XHCIRing *ring;
    XHCIRing *ringX;
    int lenToAlloc;
    lenToAlloc = (int)sizeof(XHCIRing)*(maxStream+1);
    ring = (XHCIRing *)IOMalloc(lenToAlloc);
	if(ring != NULL)
	{
		bzero(ring, lenToAlloc);
        _slots[slotID].potentialStreams[endpointID] = maxStream;
        _slots[slotID].maxStream[endpointID] = 0;
        _slots[slotID].rings[endpointID] = ring;
        ringX = ring;
        for(UInt32 i = 0; i <= maxStream; i++)
        {
            ringX->slotID = slotID;
            ringX->endpointID = endpointID;
            ringX++;
        }
	}
    return(ring);
}

XHCIRing *AppleUSBXHCI::FindStream(int slotID, int endpointID, USBPhysicalAddress64 phys, int *index, bool quiet)
{
	// We have a TRB phys pointer, which belongs to this endpoint
	// Find which stream TRB it belongs to and return the stream ring and TRB index
	
	// This may need to be optimised if we get endpoints with large numbers of streams
    
	UInt32 maxStream;
	XHCIRing *ring;
	int idx;
	
	maxStream = _slots[slotID].maxStream[endpointID];
	ring = _slots[slotID].rings[endpointID];
    
    if(!quiet)
	{	
		USBLog(2, "AppleUSBXHCI[%p]::FindStream - %llx (slot:%d, ep:%d) maxstream:%d", this, phys, (int)slotID, (int)endpointID, (int)maxStream);
	}
	
	for(UInt32 i = 1; i<=maxStream; i++)
	{
		ring++;	// We could do something more fancy here, like have a table of  
        // TRBs sorted by address and then binary search it
		
		if(ring->TRBBuffer == NULL)
		{
			continue;
		}
		idx = DiffTRBIndex(phys, ring->transferRingPhys);
        if(!quiet)
		{
			USBLog(2, "AppleUSBXHCI[%p]::FindStream - ring:%llx, idx:%d (size:%d)", this, ring->transferRingPhys, (int)idx, (int)ring->transferRingSize);
		}
		if( (idx >= 0) && (idx < ring->transferRingSize) )
		{
			*index = idx;
			return(ring);
		}
	}
	*index = 0;
	return(NULL);
}


IOReturn AppleUSBXHCI::MakeBuffer(IOOptionBits options, mach_vm_size_t size, mach_vm_address_t mask, IOBufferMemoryDescriptor **buffer, void **logical, USBPhysicalAddress64 *physical)
{
    IOReturn			err;
	IODMACommand					*dmaCommand = NULL;
	UInt64							offset = 0, maxSegmentSize;
	IODMACommand::Segment64			segments;
	UInt32							numSegments = 1;
    IOPhysicalAddress				physPtr;
	
    if(!_AC64)
    {   // Force allocation into lower 32 bit, controller does not support 64 bit.
        mask &= kXHCIAC32Mask;
    }
    
	*buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, options, size, mask);
	if (*buffer == NULL) 
	{
		USBError(1, "AppleUSBXHCI[%p]::MakeBuffer - IOBufferMemoryDescriptor::inTaskWithPhysicalMask  failed", this);
		return kIOReturnNoMemory;
	}
	err = (*buffer)->prepare();
	if (err)
	{
		USBError(1, "AppleUSBXHCI[%p]::MakeBuffer - could not prepare buffer err(%p)", this, (void*)err);
		(*buffer)->release();
		*buffer = NULL;
		return err;
	}
	
	*logical = (*buffer)->getBytesNoCopy();
	bzero(*logical, (UInt32)size);
	
	// Use IODMACommand to get the physical address
	
	maxSegmentSize = PAGE_SIZE;
	if( ((options & kIOMemoryPhysicallyContiguous) != 0) || (size < PAGE_SIZE) )
	{
		maxSegmentSize = size;
	}
	
	dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost64, 64, size, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
	if (!dmaCommand)
	{
		USBError(1, "AppleUSBXHCI[%p]::MakeBuffer - could not get IODMACommand", this);
		(*buffer)->complete();
		(*buffer)->release();
		(*buffer) = NULL;
        return kIOReturnNoMemory;
	}
	
	//USBLog(6, "AppleUSBXHCI[%p]::MakeBuffer - got IODMACommand %p", this, dmaCommand);
	err = dmaCommand->setMemoryDescriptor((*buffer));
	if (err)
	{
		USBError(1, "AppleUSBXHCI[%p]::MakeBuffer - setMemoryDescriptor returned err (%p)", this, (void*)err);
		dmaCommand->release();
		(*buffer)->complete();
		(*buffer)->release();
		(*buffer) = NULL;
        return err;
	}
	
	err = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);
	if (err || (numSegments != 1) || (segments.fLength != maxSegmentSize))
	{
		USBError(1, "AppleUSBXHCI[%p]::MakeBuffer - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)err, (int)numSegments, (int)segments.fLength);
		err = err ? err : kIOReturnInternalError;
		dmaCommand->clearMemoryDescriptor();
		dmaCommand->release();
		(*buffer)->complete();
		(*buffer)->release();
		(*buffer) = NULL;
        return err;
	}
	
	physPtr = (IOPhysicalAddress)segments.fIOVMAddr;
	
	//USBLog(6, "AppleUSBXHCI[%p]::MakeBuffer - pPhysical[%p] pLogical[%p]", this, (void*)physPtr, *logical);
	
    *physical = HostToUSBLong(physPtr);
	
	dmaCommand->clearMemoryDescriptor();
	dmaCommand->release();
	
	return(kIOReturnSuccess);
}

IOReturn AppleUSBXHCI::AllocStreamsContextArray(XHCIRing *ringX, UInt32 maxStream)
{
	IOReturn err;
	err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, (maxStream+1)*sizeof(StreamContext), kXHCIStreamContextPhysMask,
					 &ringX->TRBBuffer, (void **)&ringX->transferRing, &ringX->transferRingPhys);
	if(err != kIOReturnSuccess)
	{
		return(kIOReturnNoMemory);
	}
	ringX->transferRingSize = maxStream+1;
	return(err);
}



IOReturn 
AppleUSBXHCI::AllocRing(XHCIRing *ringX, int size_in_Pages)
{
	IOReturn err;
	int amount;
	err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, size_in_Pages * PAGE_SIZE, kXHCITransferRingPhysMask,
					 &ringX->TRBBuffer, (void **)&ringX->transferRing, &ringX->transferRingPhys);
	if(err != kIOReturnSuccess)
	{
		return(kIOReturnNoMemory);
	}
	ringX->transferRingSize = size_in_Pages * PAGE_SIZE/sizeof(TRB);
	ringX->transferRingPages = size_in_Pages;
	ringX->transferRingPCS = 1;
	ringX->transferRingEnqueueIdx = 0;
	ringX->transferRingDequeueIdx = 0;
	ringX->lastSeenDequeIdx = 0;
	ringX->lastSeenFrame = 0;
	ringX->nextIsocFrame = 0;
	SetTRBAddr64(&ringX->transferRing[ringX->transferRingSize-1], ringX->transferRingPhys);
	SetTRBType(&ringX->transferRing[ringX->transferRingSize-1], kXHCITRB_Link);
	// Set the toggle cycle bit
	// And the chain bit.
#if 1
	ringX->transferRing[ringX->transferRingSize-1].offsC |= HostToUSBLong(kXHCITRB_TC);
#else
    // Use this one if we want to debug the link. An interrupt will be generated when the link is followed.
	ringX->transferRing[ringX->transferRingSize-1].offsC |= HostToUSBLong(kXHCITRB_TC | kXHCITRB_IOC);
#endif

	return(kIOReturnSuccess);
}
#if 0
IOReturn AppleUSBXHCI::ExpandRing(XHCIRing *ringX)
{
    IOReturn			ret;
    int					slotID, epIdx, stream;
    XHCIRing			oldEp, newEP, *ring0;
	
    slotID = ringX->slotID;
    epIdx = ringX->endpointID;
    ring0 = GetRing(slotID, epIdx, 0);
    stream = (int)(ringX - ring0);
	
    if( (stream < 0) || (stream > (int)_slots[slotID].maxStream[epIdx]) )
    {
        USBLog(1, "AppleUSBXHCI[%p]::ExpandRing - could not find stream", this);
        return(kIOReturnNoMemory);
    }
	
    if(!IsIsocEP(slotID, epIdx))
    {
        USBLog(1, "AppleUSBXHCI[%p]::ExpandRing - only for Isoc endpoints currently", this);
        return(kIOReturnNoMemory);
    }

    ret = AllocRing(&newEP, ringX->transferRingPages+1);
    if(ret != kIOReturnSuccess)
    {
        return(ret);
    }
    
    USBLog(3, "AppleUSBXHCI[%p]::ExpandRing - slot:%d, ep:%d, stream:%d, newSize:%d", this, slotID, epIdx, stream, newEP.transferRingPages);

    StopEndpoint(slotID, epIdx);
    oldEp = *ringX;
    *ringX = newEP;
    USBLog(3, "AppleUSBXHCI[%p]::ExpandRing - ring: %p, phys:%p", this, ringX->transferRing, (void *)ringX->transferRingPhys);
    
    if( (ringX->transferRing == NULL) || (oldEp.transferRing == NULL) )
    {
        USBLog(1, "AppleUSBXHCI[%p]::ExpandRing - NULL Pointer. (%p, %p)", this, ringX->transferRing, oldEp.transferRing);
        return(kIOReturnNoMemory);
    }
    
    for (int i = 0; i<oldEp.transferRingSize-2; i++) // Note -2 because one is link, one is spare, if end==deq, ring is empty
    {
        if(oldEp.transferRingDequeueIdx >= oldEp.transferRingSize-1)
        {
            if(oldEp.transferRingDequeueIdx > oldEp.transferRingSize-1)
            {
                USBLog(1, "AppleUSBXHCI[%p]::ExpandRing - oldEp.transferRingEnqueueIdx > oldEp.transferRingSize-1 (%d > %d)", this, oldEp.transferRingEnqueueIdx, oldEp.transferRingSize-1);
            }
            
            oldEp.transferRingDequeueIdx = 0;
        }
        //USBLog(3, "AppleUSBXHCI[%p]::ExpandRing - i: %d oldEp.transferRingDequeueIdx:%d", this, i, oldEp.transferRingDequeueIdx);
        ringX->transferRing[i] = oldEp.transferRing[oldEp.transferRingDequeueIdx];
        if(GetTRBType(&ringX->transferRing[i]) == kXHCITRB_EventData)
        {
            SetTRBAddr64(&ringX->transferRing[i], ringX->transferRingPhys + i * sizeof(TRB));	// Set user data as phys address of TRB
        }
        SetTRBCycleBit(&ringX->transferRing[i], ringX->transferRingPCS);
        //PrintTRB(&ringX->transferRing[i], "ExpandRing");

        oldEp.transferRingDequeueIdx++;

    }
    ringX->transferRingEnqueueIdx = oldEp.transferRingSize-2;
    
    (void)SetTRDQPtr(ringX, stream, 0, false);

    // Now restart endpoint
    if(IsStreamsEndpoint(slotID, epIdx))
    {
        RestartStreams(slotID, epIdx, 0);
    }
    else
    {
        StartEndpoint(slotID, epIdx);
    }
    
    DeallocRing(&oldEp);
    return(kIOReturnSuccess);
}
#endif

void AppleUSBXHCI::SetTRBAddr64(TRB * trb, USBPhysicalAddress64 addr)
{
	trb->offs0 = HostToUSBLong(addr & 0xffffffff);
	trb->offs4 = HostToUSBLong(addr >> 32);
}

void AppleUSBXHCI::SetStreamCtxAddr64(StreamContext * strc, USBPhysicalAddress64 addr, int sct, UInt32 pcs)
{
	strc->offs0 = HostToUSBLong( (addr & 0xfffffff0) | ( (sct << kXHCIStrCtx_SCT_Shift) & kXHCIStrCtx_SCT_Mask) | (pcs?(int)kXHCITRB_DCS:0));
	strc->offs4 = HostToUSBLong(addr >> 32);
}

void AppleUSBXHCI::SetTRBDCS(TRB * trb, bool DCS)
{
	if(DCS)
	{
		trb->offs0 |= HostToUSBLong(kXHCITRB_DCS);
	}
	else
	{
		trb->offs0 &= ~HostToUSBLong(kXHCITRB_DCS);
	}
}

void AppleUSBXHCI::SetDCBAAAddr64(USBPhysicalAddress64 * el, USBPhysicalAddress64 addr)
{
	UInt32 *p;
	p = (UInt32 *)el;
	p[0] = HostToUSBLong(addr & 0xffffffC0);
	p[1] = HostToUSBLong(addr >> 32);
}

IOReturn AppleUSBXHCI::TestConfiguredEpCount()
{
    SInt16 oldEpCount;
    
    // <rdar://problem/10385765>
    // Software must prevent configuring more endpoints than the controller can handle.  This implies
    // that something bad happens if we try and just let the controller fail the configure endpoint request.
    //
    // test if we are over the endpoint limit and return an error if we are
    oldEpCount = OSIncrementAtomic16(&_configuredEndpointCount);
    USBLog(3, "AppleUSBXHCI[%p]::TestConfiguredEpCount - inc++ _configuredEndpointCount, was:(%d) max=%d ", this, oldEpCount, _maxControllerEndpoints);
    if (oldEpCount >= _maxControllerEndpoints) 
    {
        // bail now if we are over the limit for this XHCI
        oldEpCount= OSDecrementAtomic16(&_configuredEndpointCount);
        USBLog(1, "AppleUSBXHCI[%p]::TestConfiguredEpCount (kIOUSBEndpointCountExceeded) - dec-- _configuredEndpointCount, was:(%d)", this, oldEpCount);
        return(kIOUSBEndpointCountExceeded);
    }
    // rather than populate all the exit paths decrement the counter here and re-increment when the endpoint structure 
    // is allocated (either async or isoc)
    // (note: this works as long as this routine cannot be reentered)
    oldEpCount= OSDecrementAtomic16(&_configuredEndpointCount);
    USBLog(3, "AppleUSBXHCI[%p]::TestConfiguredEpCount - dec-- _configuredEndpointCount, was:(%d)", this, oldEpCount);
    
    return(kIOReturnSuccess);
}

SInt16 AppleUSBXHCI::IncConfiguredEpCount()
{
    SInt16 oldEpCount;
    
    oldEpCount= OSIncrementAtomic16(&_configuredEndpointCount);
    USBLog(3, "AppleUSBXHCI[%p]::IncConfiguredEpCount - count now=%d", this, oldEpCount);
    
    if (oldEpCount >= _maxControllerEndpoints) 
    {
        USBLog(1, "AppleUSBXHCI[%p]::IncConfiguredEpCount oldEpCount >= _maxControllerEndpoints", this);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        panic("AppleUSBXHCI - endpoint count exceeded on allocation %d", oldEpCount);
#endif
    }
    return oldEpCount;
}

SInt16 AppleUSBXHCI::DecConfiguredEpCount()
{
    SInt16 oldEpCount;
    
    oldEpCount= OSDecrementAtomic16(&_configuredEndpointCount);
    USBLog(3, "AppleUSBXHCI[%p]::DecConfiguredEpCount - count now=%d", this, oldEpCount);

    // if we go zero or negative we have a problem somewhere
    if (oldEpCount <= 0) 
    {
        USBLog(1, "AppleUSBXHCI[%p]::DecConfiguredEpCount (oldEpCount <=0), underflow!", this);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    	panic("AppleUSBXHCI - oldEpCount <=0, oldEpCount=%d", oldEpCount);
#endif
        
    }
    return oldEpCount;
}

#pragma mark ------------Endpoint Context Accessors---------------------
void AppleUSBXHCI::SetEPCtxDQpAddr64(Context * ctx, USBPhysicalAddress64 addr)
{	// Must have been set to zero for this to work
	ctx->offs08 |= HostToUSBLong( (addr & 0xffffffff) & kXHCIEpCtx_TRDQpLo_Mask);
	ctx->offs0C = HostToUSBLong(addr >> 32);
}

USBPhysicalAddress64 AppleUSBXHCI::GetEpCtxDQpAddr64(Context * ctx)
{
	return(
		   (((UInt64)USBToHostLong(ctx->offs0C)) << 32) +
		   (USBToHostLong(ctx->offs08) & kXHCIEpCtx_TRDQpLo_Mask)
		   );
}

int AppleUSBXHCI::GetEpCtxEpState(Context * ctx)
{
	return USBToHostLong(ctx->offs00) & kXHCIEpCtx_State_Mask;
}

void AppleUSBXHCI::SetEPCtxEpType(Context * ctx, int t)
{
	ctx->offs04 |= HostToUSBLong((t << kXHCIEpCtx_EPType_Shift) & kXHCIEpCtx_EPType_Mask);
}

UInt8 AppleUSBXHCI::GetEpCtxEpType(Context * ctx)
{
	return(  (USBToHostLong(ctx->offs04) & kXHCIEpCtx_EPType_Mask) >> kXHCIEpCtx_EPType_Shift);
}

UInt8 AppleUSBXHCI::GetEPCtxInterval(Context * ctx)
{
	return(USBToHostLong((ctx->offs00 & kXHCIEpCtx_Interval_Mask) >> kXHCIEpCtx_Interval_Shift));
}

void AppleUSBXHCI::SetEPCtxInterval(Context * ctx, UInt8 interval)
{
	ctx->offs00 |= HostToUSBLong((interval << kXHCIEpCtx_Interval_Shift) & kXHCIEpCtx_Interval_Mask);
}

void AppleUSBXHCI::SetEPCtxMPS(Context * ctx, UInt16 mps)
{
	ctx->offs04 |= HostToUSBLong((mps << kXHCIEpCtx_MPS_Shift) & kXHCIEpCtx_MPS_Mask);
}

UInt16 AppleUSBXHCI::GetEpCtxMPS(Context * ctx)
{
	UInt16 mps;
	
	mps = USBToHostLong((ctx->offs04 & kXHCIEpCtx_MPS_Mask) >> kXHCIEpCtx_MPS_Shift);

	return mps;
}

void AppleUSBXHCI::SetEPCtxMult(Context * ctx, UInt16 mult)
{
	ctx->offs00 |= HostToUSBLong((mult << kXHCIEpCtx_Mult_Shift) & kXHCIEpCtx_Mult_Mask);
}

UInt16 AppleUSBXHCI::GetEPCtxMult(Context * ctx)
{
	return( (HostToUSBLong(ctx->offs00) & kXHCIEpCtx_Mult_Mask) >> kXHCIEpCtx_Mult_Shift);
}

void AppleUSBXHCI::SetEPCtxMaxBurst(Context * ctx, UInt16 maxBurst)
{
	ctx->offs04 |= HostToUSBLong((maxBurst << kXHCIEpCtx_MaxBurst_Shift) & kXHCIEpCtx_MaxBurst_Mask);
}

UInt16 AppleUSBXHCI::GetEPCtxMaxBurst(Context * ctx)
{
	return(USBToHostLong((ctx->offs04 & kXHCIEpCtx_MaxBurst_Mask) >> kXHCIEpCtx_MaxBurst_Shift));
}

void AppleUSBXHCI::SetEPCtxCErr(Context * ctx, UInt32 cerr)
{
	ctx->offs04 |= HostToUSBLong((cerr << kXHCIEpCtx_CErr_Shift) & kXHCIEpCtx_CErr_Mask);
}

void AppleUSBXHCI::SetEPCtxLSA(Context * ctx, int state)
{
	if(state)
	{
		ctx->offs00 |= HostToUSBLong(kXHCIEpCtx_LSA);
	}
	else
	{
		ctx->offs00 &= HostToUSBLong(kXHCIEpCtx_LSA);
	}
}

void AppleUSBXHCI::SetEPCtxMaxPStreams(Context * ctx, int maxPStreams)
{
	ctx->offs00 |= HostToUSBLong((maxPStreams << kXHCIEpCtx_MaxPStreams_Shift) & kXHCIEpCtx_MaxPStreams_Mask);
}

void AppleUSBXHCI::SetEPCtxDCS(Context * ctx, int state)
{
	if(state)
	{
		ctx->offs08 |= HostToUSBLong(kXHCIEpCtx_DCS);
	}
	else
	{
		ctx->offs08 &= HostToUSBLong(~kXHCIEpCtx_DCS);
	}
    
}

void AppleUSBXHCI::SetEPCtxAveTRBLen(Context * ctx, UInt32 len)
{
	ctx->offs10 |= HostToUSBLong(len);
}

void AppleUSBXHCI::SetEPCtxMaxESITPayload(Context * ctx, UInt32 len)
{
	ctx->offs10 |= HostToUSBLong(len << kXHCIEpCtx_MaxESITPayload_Shift);
}


#pragma mark ------------Slot Context Accessors---------------------
void AppleUSBXHCI::SetSlCtxEntries(Context * ctx, UInt32 entries)
{
	ctx->offs00 |= HostToUSBLong((entries << kXHCISlCtx_CtxEnt_Shift) & kXHCISlCtx_CtxEnt_Mask);
}

UInt8 AppleUSBXHCI::GetSlCtxEntries(Context * ctx)
{
	return (USBToHostLong(ctx->offs00) & kXHCISlCtx_CtxEnt_Mask) >> kXHCISlCtx_CtxEnt_Shift;
}

void AppleUSBXHCI::SetSlCtxSpeed(Context * ctx, UInt32 speed)
{
	ctx->offs00 |= HostToUSBLong((speed << kXHCISlCtx_Speed_Shift) & kXHCISlCtx_Speed_Mask);
}

void AppleUSBXHCI::SetSlCtxRootHubPort(Context *ctx, UInt32 rootHubPort)
{
	ctx->offs04 |= HostToUSBLong(rootHubPort << kXHCISlCtx_RHPNum_Shift);
}

UInt32 AppleUSBXHCI::GetSlCtxRootHubPort(Context * ctx)
{
	return (USBToHostLong(ctx->offs04) & kXHCISlCtx_RHPNum_Mask) >> kXHCISlCtx_RHPNum_Shift;
}

int AppleUSBXHCI::GetSlCtxSlotState(Context * ctx)
{
	return (USBToHostLong(ctx->offs0C) & kXHCISlCtx_SlotState_Mask) >> kXHCISlCtx_SlotState_Shift;
}

int AppleUSBXHCI::GetSlCtxUSBAddress(Context * ctx)
{
	return (USBToHostLong(ctx->offs0C) & kXHCISlCtx_USBDeviceAddress_Mask);
}

UInt8 AppleUSBXHCI::GetSlCtxSpeed(Context * ctx)
{
	int xSpeed;
	
	xSpeed = (USBToHostLong(ctx->offs00) & kXHCISlCtx_Speed_Mask) >> kXHCISlCtx_Speed_Shift;
	switch (xSpeed)
	{
		case kXHCISpeed_Full:
			return(kUSBDeviceSpeedFull);
			break;
		case kXHCISpeed_Low:
			return(kUSBDeviceSpeedLow);
			break;
		case kXHCISpeed_High:
			return(kUSBDeviceSpeedHigh);
			break;
		case kXHCISpeed_Super:
			return(kUSBDeviceSpeedSuper);
			break;
		default:
			return(kUSBDeviceSpeedSuper);
			break;
	}
}

void AppleUSBXHCI::SetSlCtxTTPort(Context * ctx, UInt32 port)
{
	ctx->offs08 |= HostToUSBLong((port << kXHCISlCtx_TTPort_Shift) & kXHCISlCtx_TTPort_Mask);
}

int AppleUSBXHCI::GetSlCtxTTPort(Context * ctx)
{
	return (USBToHostLong(ctx->offs08) & kXHCISlCtx_TTPort_Mask) >> kXHCISlCtx_TTPort_Shift;
}

void AppleUSBXHCI::SetSlCtxTTSlot(Context * ctx, UInt32 slot)
{
	ctx->offs08 |= HostToUSBLong(slot & kXHCISlCtx_TTSlot_Mask);
}

int AppleUSBXHCI::GetSlCtxTTSlot(Context * ctx)
{
	return (USBToHostLong(ctx->offs08) & kXHCISlCtx_TTSlot_Mask);
}

void AppleUSBXHCI::SetSlCtxInterrupter(Context * ctx, UInt32 interrupter)
{
	ctx->offs08 |= HostToUSBLong((interrupter << kXHCISlCtx_Interrupter_Shift) & kXHCISlCtx_Interrupter_Mask);
}

UInt32 AppleUSBXHCI::GetSlCtxInterrupter(Context * ctx)
{
	return (USBToHostLong(ctx->offs08) & kXHCISlCtx_Interrupter_Mask) >> kXHCISlCtx_Interrupter_Shift;
}

UInt32 AppleUSBXHCI::GetSlCtxRouteString(Context * ctx)
{
	return (USBToHostLong(ctx->offs00) & kXHCISlCtx_Route_Mask);
}

void AppleUSBXHCI::SetSlCtxRouteString(Context * ctx, UInt32 string)
{
	ctx->offs00 |= HostToUSBLong(string & kXHCISlCtx_Route_Mask);
}

void AppleUSBXHCI::ResetSlCtxNumPorts(Context * ctx, UInt32 num)
{
	ctx->offs04 = (ctx->offs04 & ~HostToUSBLong(kXHCISlCtx_NumPorts_Mask)) |  HostToUSBLong((num << kXHCISlCtx_NumPorts_Shift) & kXHCISlCtx_NumPorts_Mask);
}

void AppleUSBXHCI::ResetSlCtxTTT(Context * ctx, UInt32 TTT)
{
	ctx->offs04 = (ctx->offs04 & ~HostToUSBLong(kXHCISlCtx_TTT_Mask)) |  HostToUSBLong((TTT << kXHCISlCtx_TTT_Shift) & kXHCISlCtx_TTT_Mask);
}

void AppleUSBXHCI::SetSlCtxMTT(Context * ctx, bool multiTT)
{
	if(multiTT)
	{
		ctx->offs00 |= HostToUSBLong(kXHCISlCtx_MTTBit); // MultiTT
	}
	else
	{
		ctx->offs00 &= ~HostToUSBLong(kXHCISlCtx_MTTBit); // Single TT
	}
}

bool AppleUSBXHCI::GetSlCtxMTT(Context * ctx)
{
	return( (ctx->offs00 & HostToUSBLong(kXHCISlCtx_MTTBit)) != 0);
}



#pragma mark ------------TRB Accessors---------------------
void AppleUSBXHCI::SetTRBType(TRB * trb, int t)
{
	trb->offsC |= HostToUSBLong((t << kXHCITRB_Type_Shift) & kXHCITRB_Type_Mask);
}

int AppleUSBXHCI::GetTRBType(TRB * trb)
{
	return((USBToHostLong(trb->offsC) & kXHCITRB_Type_Mask) >> kXHCITRB_Type_Shift);
    
}

int AppleUSBXHCI::GetTRBSlotID(TRB * trb)
{
	return((USBToHostLong(trb->offsC) & kXHCITRB_SlotID_Mask) >> kXHCITRB_SlotID_Shift);
	
}

void AppleUSBXHCI::SetTRBSlotID(TRB * trb, UInt32 slotID)
{
	trb->offsC |= HostToUSBLong((slotID << kXHCITRB_SlotID_Shift) & kXHCITRB_SlotID_Mask);
}

void AppleUSBXHCI::SetTRBEpID(TRB * trb, UInt32 slotID)
{
	trb->offsC |= HostToUSBLong((slotID << kXHCITRB_Ep_Shift) & kXHCITRB_Ep_Mask);
}

void AppleUSBXHCI::SetTRBStreamID(TRB * trb, UInt32 streamID)
{
	trb->offs8 |= HostToUSBLong((streamID << kXHCITRB_Stream_Shift) & kXHCITRB_Stream_Mask);
}



int AppleUSBXHCI::GetTRBCC(TRB * trb)
{
	return((USBToHostLong(trb->offs8) & kXHCITRB_CC_Mask) >> kXHCITRB_CC_Shift);
	
}

bool AppleUSBXHCI::IsIsocEP(int slotID, UInt32 endpointIdx)
{
	int epType;
	Context * epContext = GetEndpointContext(slotID, endpointIdx);
	
    if(epContext == NULL)
    {
        return(false);
    }

	epType = GetEpCtxEpType(epContext);
	return( (epType == kXHCIEpCtx_EPType_IsocIn) || (epType == kXHCIEpCtx_EPType_IsocOut) );
    
}

//
// InitEventRing should be called only once from UIMInitialize 
// 
void AppleUSBXHCI::InitEventRing(int IRQ, bool reinit)
{
	int i;
	
	// Clear all Events in the ring
	
    if(reinit)
    {
        for(i = 0; i<_events[IRQ].numEvents; i++)
        {
            ClearTRB(&_events[IRQ].EventRing[i], true);
        }
    }
    else
    {
        for(i = 0; i<(_events[IRQ].numEvents + kSegmentTableEventRingEntries); i++)
        {
            ClearTRB(&_events[IRQ].EventRing[i], true);
        }
	
        // First entry will make up the event ring segment table
        _events[IRQ].EventRingSegTablePhys = _events[IRQ].EventRingPhys;
        _events[IRQ].EventRingPhys = _events[IRQ].EventRingPhys + (kSegmentTableEventRingEntries * sizeof(TRB));
	}
	
	// Set the 0th entry as segment 
	Write64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP, _events[IRQ].EventRingPhys);
	SetTRBAddr64(&_events[IRQ].EventRing[0], _events[IRQ].EventRingPhys);
    
    
    if(!reinit)
    {
        _events[IRQ].EventRing[0].offs8 = HostToUSBLong(_events[IRQ].numEvents);
        
        // Event ring proper, starts on 64 byte boundary
        _events[IRQ].EventRing = &_events[IRQ].EventRing[kSegmentTableEventRingEntries];
        
    }
	Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTSZ, kEntriesInEventRingSegmentTable);		// 1 entry in event ring segment table
	Write64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTBA, _events[IRQ].EventRingSegTablePhys);	// This starts the state machine, so do it last
	Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMOD, kInterruptModerationInterval);	// 40us moderation
	Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN, kXHCIIRQ_IE);		// Enable the interrupt.
    
	_events[IRQ].EventRingDequeueIdx = 0;
	_events[IRQ].EventRingCCS = 1;
    _events[IRQ].EventRingDPNeedsUpdate = false;
    if(!reinit)
    {
        PrintInterrupter(5, IRQ, "InitEventRing");
    }
}

void AppleUSBXHCI::InitCMDRing(void)
{
	int i;
	
	// Clear all CMDs in the ring
	
	for(i = 0; i<_numCMDs; i++)
	{
		ClearTRB(&_CMDRing[i], true);
	}
	
	// Add the Link TRB at the end
	SetTRBAddr64(&_CMDRing[_numCMDs-1], _CMDRingPhys);
	SetTRBType(&_CMDRing[_numCMDs-1], kXHCITRB_Link);
	//USBLog(2, "AppleUSBXHCI[%p]::InitCMDRing - Link @ %llx", this, _CMDRingPhys+sizeof(TRB)*(_numCMDs-1));
	
	// Set the toggle cycle bit
	_CMDRing[_numCMDs-1].offsC |= HostToUSBLong(kXHCITRB_TC);
	
	_CMDRingPCS = 1;	// Producer cycle will be 1, all C bits have been cleared.
	
	_CMDRingEnqueueIdx = 0;
	_CMDRingDequeueIdx = _CMDRingEnqueueIdx;	// empty ring
	
	Write64Reg(&_pXHCIRegisters->CRCR, (_CMDRingPhys & ~kXHCICRCRFlags_Mask) | kXHCI_RCS);	// Sets the consumer cycyle state to one.
}

void AppleUSBXHCI::SetTRBCycleBit(TRB *trb, int state)
{
	if(state)
	{
		trb->offsC |= HostToUSBLong(kXHCITRB_C);
	}
	else 
	{
		trb->offsC &= ~HostToUSBLong(kXHCITRB_C);
	}	
}

void AppleUSBXHCI::SetTRBChainBit(TRB *trb, int state)
{
	if(state)
	{
		trb->offsC |= HostToUSBLong(kXHCITRB_CH);
	}
	else 
	{
		trb->offsC &= ~HostToUSBLong(kXHCITRB_CH);
	}	
}

bool AppleUSBXHCI::GetTRBChainBit(TRB *trb)
{
    return( (USBToHostLong(trb->offsC) & kXHCITRB_CH) != 0);
}


void AppleUSBXHCI::SetTRBBSRBit(TRB *trb, int state)
{
	if(state)
	{
		trb->offsC |= HostToUSBLong(kXHCITRB_BSR);
	}
	else 
	{
		trb->offsC &= ~HostToUSBLong(kXHCITRB_BSR);
	}	
}


IOReturn AppleUSBXHCI::EnqueCMD(TRB *trb, int type, CMDComplete callBackFn, SInt32 **param)
{
	int nextEnqueueIndex;
	UInt32 offsC;
	
	// First see if CMD ring is full, and advance pointer
	
	if(_CMDRingEnqueueIdx >= _numCMDs-2)
	{	// Next TRB is the link TRB
		if(_CMDRingDequeueIdx == 0)
		{
			// Ring is full
			USBLog(1, "AppleUSBXHCI[%p]::EnqueCMD - Ring full 1, enq:%d, deq:%d", this, _CMDRingEnqueueIdx, _CMDRingDequeueIdx);
			return kIOReturnNoResources;
		}
		nextEnqueueIndex = 0;	
	}
	else
	{
		if(_CMDRingEnqueueIdx+1 == _CMDRingDequeueIdx)
		{
			// Ring is full
			USBLog(1, "AppleUSBXHCI[%p]::EnqueCMD - Ring full 2, enq:%d, deq:%d", this, _CMDRingEnqueueIdx, _CMDRingDequeueIdx);
			return kIOReturnNoResources;
		}
		
		nextEnqueueIndex = _CMDRingEnqueueIdx+1;
	}
	
#if 0
	{
        USBPhysicalAddress64 phys;
		phys = _CMDRingEnqueueIdx*sizeof(TRB)+_CMDRingPhys;
		USBLog(3, "AppleUSBXHCI[%p]::EnqueCMD - TRB phys: %08lx", this, (long unsigned int)phys);
	}
#endif
	
	_CMDRing[_CMDRingEnqueueIdx].offs0 = trb->offs0;
	_CMDRing[_CMDRingEnqueueIdx].offs4 = trb->offs4;
	_CMDRing[_CMDRingEnqueueIdx].offs8 = trb->offs8;
	_CMDCompletions[_CMDRingEnqueueIdx].completionAction = callBackFn;
    _CMDCompletions[_CMDRingEnqueueIdx].parameter = CMD_NOT_COMPLETED;
	*param = &_CMDCompletions[_CMDRingEnqueueIdx].parameter;
	
	offsC = trb->offsC;
	offsC &= ~(kXHCITRB_Type_Mask | kXHCITRB_C);	// Clear type and cycle state fields
	offsC |= type << kXHCITRB_Type_Shift;
	if(_CMDRingPCS)
	{
		offsC |= kXHCITRB_C;
	}
    PrintTRB(7, &_CMDRing[_CMDRingEnqueueIdx], "EnqueCMD", offsC);
	USBLog(7, "AppleUSBXHCI[%p]::EnqueCMD - offsC: %08lx, TRB phys: %llx", this, (long unsigned int)offsC, _CMDRingPhys+_CMDRingEnqueueIdx*sizeof(TRB));
    USBLog(7, "AppleUSBXHCI[%p]::EnqueCMD - _CMDRingEnqueueIdx= %d  %p(%p)", this, (int)_CMDRingEnqueueIdx, callBackFn, param);
	IOSync();
	
	_CMDRing[_CMDRingEnqueueIdx].offsC = offsC;
	IOSync();
	
	//PrintTRB(&_CMDRing[_CMDRingEnqueueIdx], "CMDRing");
	
	// Advance the enqueue pointer
	if(nextEnqueueIndex == 0)
	{
		SetTRBCycleBit(&_CMDRing[_numCMDs-1], _CMDRingPCS);		// Set the bit on the link
		_CMDRingPCS = 1 - _CMDRingPCS;	// Toggle cycle state
	}
	PrintCommandTRB(&_CMDRing[_CMDRingEnqueueIdx]);

	_CMDRingEnqueueIdx = nextEnqueueIndex;
	
	// Ring the doorbell
    IOSync();
    Write32Reg(&_pXHCIDoorbells[0], kXHCIDB_Controller);
    IOSync();

	return(kIOReturnSuccess);
}

void AppleUSBXHCI::ClearTRB(TRB *trb,  bool clearCCS)
{
	trb->offs0 = 0;
	trb->offs4 = 0;
	trb->offs8 = 0;
	if(clearCCS)
	{
		trb->offsC = 0;
	}
	else
	{
		trb->offsC &= kXHCITRB_C;
	}
    
}


void AppleUSBXHCI::PrintInterrupter(int level, int IRQ, const char *s)
{
	USBLog(level, "AppleUSBXHCI[%p]::PrintInterrupter %s IRQ:%d - IMAN: %08lx IMOD: %08lx ERDP: %08lx", this, s, IRQ,
		   (long unsigned int)Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN), (long unsigned int)Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMOD), (long unsigned int)Read64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP));
}


void AppleUSBXHCI::PrintTransferTRB(TRB *trb, XHCIRing *ringX, int indexInRing, UInt32 offsC )
{
    USBTrace_Start(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBTransfer, (uintptr_t)this, (uintptr_t)(ringX->transferRingPhys), trb->offs0, trb->offs4);
    if(offsC == 0)
    {
        USBTrace_End(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBTransfer, trb->offs8, trb->offsC, indexInRing, 0);
    }
    else
    {
        USBTrace_End(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBTransfer, trb->offs8, offsC, indexInRing, 0);
    }
}


void AppleUSBXHCI::PrintEventTRB(TRB *trb, int irq, bool inFilter, XHCIRing* otherRing )
{
    USBTrace_Start(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBEvent, (uintptr_t)this, irq, trb->offs0, trb->offs4);
	USBTrace_End(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBEvent, trb->offs8, trb->offsC, otherRing ? (uintptr_t)(otherRing->transferRingPhys) : 0, inFilter);
}

void AppleUSBXHCI::PrintCommandTRB(TRB *trb)
{
    USBTrace_Start(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBCommand, (uintptr_t)this, trb->offs0, trb->offs4, 0);
	USBTrace_End(kUSBTXHCIPrintTRB, kTPXHCIPrintTRBCommand, trb->offs8, trb->offsC, 0, 0);
}

void AppleUSBXHCI::PrintTRB(int level, TRB *trb, const char *s, UInt32 offsC)
{
    //USBTrace_Start(kUSBTXHCI, kTPXHCIPrintTRB, (uintptr_t)this, 0, trb->offs0, trb->offs4);

    if(offsC == 0)
    {
        USBLog(level, "AppleUSBXHCI[%p]::PrintTRB %s - %08lx %08lx %08lx %08lx", this,s,
		   (long unsigned int)trb->offs0,(long unsigned int) trb->offs4,(long unsigned int) trb->offs8,(long unsigned int) trb->offsC);
    }
    else
    {
        USBLog(level, "AppleUSBXHCI[%p]::PrintTRB %s - %08lx %08lx %08lx %08lx", this,s,
               (long unsigned int)trb->offs0,(long unsigned int) trb->offs4,(long unsigned int) trb->offs8,(long unsigned int) offsC);
    }
    //USBTrace_End(kUSBTXHCI, kTPXHCIPrintTRB, (uintptr_t)this, 0, trb->offs8, trb->offsC);
}

void AppleUSBXHCI::PrintRing(XHCIRing *ring)
{
    USBLog(5, "AppleUSBXHCI[%p]::PrintRing ---------------- slotID:%d endpointID:%d ---------------------", this, ring->slotID , ring->endpointID);
    
    USBLog(5, "AppleUSBXHCI[%p]::PrintRing - transferRingPhys: %llx transferRingSize: %d transferRingPCS: %d", this, ring->transferRingPhys, (int)ring->transferRingSize, (int)ring->transferRingPCS);
    USBLog(5, "AppleUSBXHCI[%p]::PrintRing - transferRingEnqueueIdx: %d transferRingDequeueIdx: %d", this, (int)ring->transferRingEnqueueIdx, (int)ring->transferRingDequeueIdx);
    USBLog(5, "AppleUSBXHCI[%p]::PrintRing - lastSeenDequeIdx: %d lastSeenFrame: %d", this, (int)ring->lastSeenDequeIdx, (int)ring->lastSeenFrame);
    USBLog(5, "AppleUSBXHCI[%p]::PrintRing - beingReturned: %d beingDeleted: %d needsDoorbell: %d", this, (int)ring->beingReturned, (int)ring->beingDeleted, (int)ring->needsDoorbell);

    
    if( (ring->endpointType == kXHCIEpCtx_EPType_IsocIn) || (ring->endpointType == kXHCIEpCtx_EPType_IsocOut) )
    {
        AppleXHCIIsochEndpoint *pIsocEP = OSDynamicCast(AppleXHCIIsochEndpoint, (AppleXHCIIsochEndpoint*)ring->pEndpoint);
        pIsocEP->print(5);
    }
    else
    {
        AppleXHCIAsyncEndpoint* pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);
        pAsyncEP->print(5);
    }
    
    char buf[256];
    int  i = 0;
        
    for (i=0; i < ring->transferRingSize; i++)
    {
        UInt8 trbType = GetTRBType(&ring->transferRing[i]);
        snprintf(buf, 256, "(%s) Index: %4d Phys:%08lx", TRBType(trbType), i, (long unsigned int)(ring->transferRingPhys + i*sizeof(TRB)));
        PrintTRB(5, &ring->transferRing[i], buf);
		PrintTransferTRB(&ring->transferRing[i], ring, i);
    }
}

void AppleUSBXHCI::PrintContext(Context * ctx)
{
	
    if ( ctx == NULL )
    {
        USBLog(1, "AppleUSBXHCI[%p]::PrintContext called with a NULL context", this);
        return;
    }
    
    USBTrace_Start(kUSBTXHCI, kTPXHCIPrintContext, (uintptr_t)this, ctx->offs00, ctx->offs04, ctx->offs08);

    USBTrace(kUSBTXHCI, kTPXHCIPrintContext, (uintptr_t)this, 1, 0, ctx->offs0C);
    
    USBLog(4, "AppleUSBXHCI[%p]::PrintContext 1 - %08lx %08lx %08lx %08lx", this,
		   (long unsigned int)ctx->offs00,(long unsigned int) ctx->offs04,(long unsigned int) ctx->offs08,(long unsigned int) ctx->offs0C);

    USBTrace(kUSBTXHCI, kTPXHCIPrintContext, (uintptr_t)this, 2, 0, ctx->offs10);

    USBLog(4, "AppleUSBXHCI[%p]::PrintContext 2 - %08lx %08lx %08lx %08lx", this,
		   (long unsigned int)ctx->offs10,(long unsigned int) ctx->offs14,(long unsigned int) ctx->offs18,(long unsigned int) ctx->offs1C);
    
    USBTrace_End(kUSBTXHCI, kTPXHCIPrintContext, (uintptr_t)this, ctx->offs14, ctx->offs18, ctx->offs1C);
}

void AppleUSBXHCI::PrintSlotContexts(void)
{
    for(int i = 0; i<_numDeviceSlots; i++)
    {
        Context * ctx = GetSlotContext(i);
		if(ctx != NULL)
        {
            USBLog(4, "AppleUSBXHCI[%p]::slot %d: - %08lx %08lx %08lx %08lx   %08lx %08lx %08lx %08lx", this, i,
                   (long unsigned int)ctx->offs00,(long unsigned int) ctx->offs04,(long unsigned int) ctx->offs08,(long unsigned int) ctx->offs0C,
                   (long unsigned int)ctx->offs10,(long unsigned int) ctx->offs14,(long unsigned int) ctx->offs18,(long unsigned int) ctx->offs1C);
        }
    }
}

Context * AppleUSBXHCI::GetContextFromDeviceContext(int SlotID, int contextIdx)
{
	Context * ctx = NULL;
	
	if (_Contexts64 == false)
	{
		ctx = _slots[SlotID].deviceContext;
		if (ctx != NULL)
		{
			ctx = &_slots[SlotID].deviceContext[contextIdx];
		}
	}
	else
	{
		ctx = (Context *)_slots[SlotID].deviceContext64;
		if (ctx != NULL)
		{
			ctx = (Context *)&_slots[SlotID].deviceContext64[contextIdx];
		}
	}
	
	return ctx;
}

Context * AppleUSBXHCI::GetEndpointContext(int SlotID, int EndpointID)
{
    return GetContextFromDeviceContext(SlotID, EndpointID);
}

Context * AppleUSBXHCI::GetSlotContext(int SlotID)
{
    // The first context in the context array is the slot context
    return GetContextFromDeviceContext(SlotID, 0);
}


#if 0
void AppleUSBXHCI::PrintCCETRB(TRB *trb)
{
	int type;
	
	PrintTRB(trb, "PrintCCETRB");
	
	type = GetTRBType(trb);
	
	if(type != kXHCITRB_CCE)
	{
		USBLog(2, "AppleUSBXHCI[%p]::PrintCCETRB - not a command completion event, type: %d", this,(int) type);
		return;
	}
	
	USBLog(2, "AppleUSBXHCI[%p]::PrintCCETRB - Command completion event, type: %d", this,(int) type);
	USBLog(2, "AppleUSBXHCI[%p]::PrintCCETRB - TRB pointer: %08lx %08lx", this,(long unsigned int) trb->offs4, (long unsigned int)trb->offs0);
	USBLog(2, "AppleUSBXHCI[%p]::PrintCCETRB - Completion code: %02lx ", this,(long unsigned int) (trb->offs8 >> 24));
	
}
#endif

IOReturn AppleUSBXHCI::MungeXHCIStatus(int code, bool in, UInt8 speed, bool silent)
{
	switch(code)
	{
		case kXHCITRB_CC_Success:
		case kXHCITRB_CC_ShortPacket:
			return(kIOReturnSuccess);
            
		case kXHCITRB_CC_Data_Buffer:
            if(in)
            {
                return(kIOUSBBufferOverrunErr);
            }
            else
            {
                return(kIOUSBBufferUnderrunErr);
            }

		case kXHCITRB_CC_Babble_Detected:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_XActErr:
            if (speed < kUSBDeviceSpeedHigh)
            {
                return(kIOUSBHighSpeedSplitError); 
            }
            else
            {
                return(kIOReturnNotResponding);
            }
            
		case kXHCITRB_CC_TRBErr:
            return(kIOReturnInternalError);
            
		case kXHCITRB_CC_STALL:
			return(kIOUSBPipeStalled);
            
		case kXHCITRB_CC_ResourceErr:
			return(kIOReturnNoResources);
        
		case kXHCITRB_CC_Bandwidth:
			return(kIOReturnNoBandwidth);
            
		case kXHCITRB_CC_NoSlots:
			return(kIOUSBDeviceCountExceeded);
            
		case kXHCITRB_CC_Invalid_Stream_Type:
			return(kIOReturnInvalid);
            
		case kXHCITRB_CC_Slot_Not_Enabled:
			return(kIOReturnOffline);
            
		case kXHCITRB_CC_Endpoint_Not_Enabled:
			return(kIOReturnOffline);
            
		case kXHCITRB_CC_RingUnderrun:
			return(kIOReturnUnderrun);
            
		case kXHCITRB_CC_RingOverrun:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_VF_Event_Ring_Full:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_CtxParamErr:
			return(kIOReturnBadArgument);
            
		case kXHCITRB_CC_Bandwidth_Overrun:
			return(kIOReturnNoBandwidth);
            
		case kXHCITRB_CC_CtxStateErr:
			return(kIOReturnBadArgument);
            
		case kXHCITRB_CC_No_Ping_Response:
			return(kIOReturnNotResponding);
            
		case kXHCITRB_CC_Event_Ring_Full:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_Incompatible_Device:
			return(kIOReturnDeviceError);
            
		case kXHCITRB_CC_Missed_Service:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_CMDRingStopped:
			return(kIOReturnOffline);
            
		case kXHCITRB_CC_Command_Aborted:
			return(kIOReturnAborted);
            
		case kXHCITRB_CC_Stopped:
		case kXHCITRB_CC_Stopped_Length_Invalid:
			return(kIOReturnAborted);
            
		case kXHCITRB_CC_Max_Exit_Latency_Too_Large:
			return(kIOReturnBadArgument);
            
		case kXHCITRB_CC_Isoch_Buffer_Overrun:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_Event_Lost:
			return(kIOReturnOverrun);
            
		case kXHCITRB_CC_Undefined:
			return(kIOReturnInternalError);
            
		case kXHCITRB_CC_Invalid_Stream_ID:
			return(kIOReturnBadArgument);
            
		case kXHCITRB_CC_Secondary_Bandwidth:
			return(kIOReturnNoBandwidth);
            
		case kXHCITRB_CC_Split_Transaction:
			return(kIOUSBHighSpeedSplitError);
            
        // Intel specific errors
        kXHCITRB_CC_CNTX_ENTRIES_GTR_MAXEP:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_FORCE_HDR_USB2_NO_SUPPORT:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnUnsupported);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_UNDEFINED_BEHAVIOR:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_CMPL_VEN_DEF_ERR_195:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_NOSTOP:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_HALT_STOP:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_DL_ERR:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnInternalError);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_CMPL_WITH_EMPTY_CONTEXT:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnNotOpen);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		case kXHCITRB_CC_VENDOR_CMD_FAILED:
            if( (_errataBits & kXHCIErrataPPT) == 0)
            {
                return(kIOReturnNoBandwidth);
            }
            else
            {
                return(kIOReturnInternalError);
            }
            
		default:
			if(!silent)
			{
				USBLog(2, "AppleUSBXHCI[%p]::MungeXHCIStatus - unknown code: %d", this, code);
			}
			return(kIOReturnInternalError);
	}
}



IOReturn 
AppleUSBXHCI::MungeCommandCompletion(int code, bool silent)
{
    if(code > MakeXHCIErrCode(0))
    {
        return(kIOReturnSuccess);
    }
	
	switch(code)
	{
		case CMD_NOT_COMPLETED:
			return(kIOReturnTimeout);
            
		case MAKEXHCIERR(kXHCITRB_CC_CtxParamErr):
			return(kIOReturnBadArgument);
			
		case MAKEXHCIERR(kXHCITRB_CC_CtxStateErr):
			return(kIOReturnNotPermitted);
			
            
		default:
			if(!silent)
			{
				USBLog(2, "AppleUSBXHCI[%p]::MungeCommandCompletion - unknown code: %d", this, code);
			}
			return(kIOReturnInternalError);
	}
}

int AppleUSBXHCI::GetEndpointID(int endpointNumber, short direction)
{
	int endpointID;
	endpointID = 2*endpointNumber;
	if(direction != kUSBOut)
	{
		endpointID++;
	}
	return(endpointID);
}


int AppleUSBXHCI::GetSlotID(int functionNumber)
{
	int slotID;
	if( (functionNumber == _rootHubFuncAddressSS) || (functionNumber == _rootHubFuncAddressHS) )
	{
		return(0);
	}
	if( (functionNumber <0) || (functionNumber > 127) )
	{
		USBLog(3, "AppleUSBXHCI[%p]::GetSlotID - functionNumber out of range: %d", this, functionNumber);
		return(0);
	}
	if(!_devEnabled[functionNumber])
	{
		USBLog(3, "AppleUSBXHCI[%p]::GetSlotID - functionNumber disabled: %d", this, functionNumber);
		return(0);
	}
	slotID = _devMapping[functionNumber];
	return(slotID);
}



int AppleUSBXHCI::CountRingToED(XHCIRing *ring, int index, UInt32 *shortfall, bool advance)
{
	int newIndex, type;
    TRB *t;
    t = &ring->transferRing[index];
	
    USBLog(5, "AppleUSBXHCI[%p]::CountRingToED - Initial index: %d", this, index);
    PrintTRB(5, t, "CountRingToED1");
    
	while(GetTRBChainBit(t)){
		newIndex = index;
		
		// Find next index
		newIndex++;
		if(newIndex >= ring->transferRingSize-1)
		{
			newIndex = 0;
		}
		// We used to wait here hoping that more TDs would be added to the ring. But
        // We're all on the same thread, so that's never going to happen.
		if(newIndex == ring->transferRingEnqueueIdx)
		{
			if(advance)
			{
				ring->transferRingDequeueIdx = index;
			}
			USBLog(1, "AppleUSBXHCI[%p]::CountRingToED - No ED found: %d", this, index);
			return(index);
		}
		index = newIndex;
		// Index now points to a new TRB
        t = &ring->transferRing[index];
        PrintTRB(5, t, "CountRingToED2");
		type = GetTRBType(t);
		if(type == kXHCITRB_EventData)
		{
			USBLog(3, "AppleUSBXHCI[%p]::CountRingToED - ED index: %d", this, index);
			if(advance)
			{
				ring->transferRingDequeueIdx = index;
			}
			return(index);
		}
		if(type == kXHCITRB_Normal)
		{
			*shortfall += USBToHostLong(t->offs8) & kXHCITRB_Normal_Len_Mask;
			USBLog(2, "AppleUSBXHCI[%p]::CountRingToED - Normal TD (index:%d) shortfall: %d, running:%d", this, 
				   index, USBToHostLong(t->offs8) & kXHCITRB_Normal_Len_Mask, (int)*shortfall);
		}
        
        
	}
    USBLog(5, "AppleUSBXHCI[%p]::CountRingToED - TRB not chained, returning: %d", this, index);
    // This TRB is not chained to the next, so is the end of the TD.
    
    if(advance)
    {
        ring->transferRingDequeueIdx = index;
    }
    return(index);
	
}

bool
AppleUSBXHCI::CanTDFragmentFit(XHCIRing *ring, UInt32 fragmentTransferSize)
{
    UInt16 maxTRBs = fragmentTransferSize/PAGE_SIZE + 2;
    
    if (ring->transferRingEnqueueIdx < ring->transferRingDequeueIdx)
    {
        return (maxTRBs <= (ring->transferRingDequeueIdx - ring->transferRingEnqueueIdx-1));
    }
    else
    {
        UInt16 spaceInEnd, spaceInStart, space;
        
        spaceInEnd = ring->transferRingSize - ring->transferRingEnqueueIdx - 1;
        
        // Subtract 2 for worst case alignment
        if (spaceInEnd < 2)
        {
            spaceInEnd = 0;
        }
        else
        {
            spaceInEnd -= 2;
        }
        
        spaceInStart = ring->transferRingDequeueIdx;
        // Subtract 2 for worst case alignment
        if (spaceInStart < 2)
        {
            spaceInStart = 0;
        }
        else
        {
            spaceInStart -= 2;
        }
        
        if (spaceInEnd > spaceInStart)
        {
            space = spaceInEnd;
        }
        else
        {
            space = spaceInStart;
        }
        
        return (space >= maxTRBs);
    }
}


int 
AppleUSBXHCI::FreeSlotsOnRing(XHCIRing *ring)
{
	// this method need to look at the ring and decide how many TRB slots there are which are owned by the software
	// it should NOT count slots which the hardware is finished with, but which have not been processed for completion
	
	// Since this method can be called with pre-emption disabled (see AddIsocFramesToSchedule) we should not do any regular USBLog calls
	// USBLogKP will be OK if we are part of a kprintf build, and will be a NOP if we are not
	USBLogKP(7, "AppleUSBXHCI::FreeSlotsOnRing - transferRingEnqueueIdx(%d) transferRingDequeueIdx(%d)\n", ring->transferRingEnqueueIdx, ring->transferRingDequeueIdx);
    if(ring->transferRingEnqueueIdx < ring->transferRingDequeueIdx)
    {
        return(ring->transferRingDequeueIdx - ring->transferRingEnqueueIdx-1);
    }
    else
    {
        // Space spans the link at the end of the ring, sub tract worst case fragment alignment
        UInt8 speed;
        int space;
        int align;
        
        speed = GetSlCtxSpeed(GetSlotContext(ring->slotID));
        space = ring->transferRingSize - (ring->transferRingEnqueueIdx - ring->transferRingDequeueIdx) -1;
        if(speed >= kXHCISpeed_Super)
        {
            UInt32  maxPacketSize, maxBurst, mult;
			Context * epContext = GetEndpointContext(ring->slotID, ring->endpointID);
			
			maxPacketSize = GetEpCtxMPS(epContext);
			maxBurst = GetEPCtxMaxBurst(epContext) + 1;
			mult = GetEPCtxMult(epContext)+1;
			align = (maxPacketSize * maxBurst * mult)/ 4096;
        }
        else
        {
            // FS or HS, each TD may be up to 3 TRBs (2 plus ED),
            // so alignment may use up to 2 TRBs
            align = 2;
        }
        if(space < align)
        {
            space = 0;
        }
        else
        {
            space -= align;
        }

        return(space);

    }
}



#define NSEC_PER_MS	1000000		/* nanosecond per millisecond */

SInt32 AppleUSBXHCI::WaitForCMD(TRB *t, int command, CMDComplete callBackF)
{
    int         innercount = 0;
	UInt32      count = 0;
	SInt32      *ret;
	IOReturn    kr = 0;
    UInt32      timeout = 100;
    SInt32      retval = CMD_NOT_COMPLETED;

    if((Read32Reg(&_pXHCIRegisters->USBSTS) & kXHCIHSEBit) != 0)
    {
        if(!_HSEReported)
        {
            USBError(1, "AppleUSBXHCI[%p]::WaitForCMD - HSE bit set:%x (1)", this, Read32Reg(&_pXHCIRegisters->USBSTS));
        }
        _HSEReported = true;
    }

    if ( isInactive() || _lostRegisterAccess || !_controllerAvailable )
	{
		USBLog(1, "AppleUSBXHCI::WaitForCMD - Returning early inactive: %d lost register access:%d", isInactive(), (int)_lostRegisterAccess);
        return retval;
	}

	USBTrace_Start(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) t, command, 0);

	if ( getWorkLoop()->onThread() )
    {
        USBLog(5, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Called on thread.", this, TRBType(command));
        
        // Don't return early here. It stops things working.
        // We need to work out why this is being called on a thread, and fix that first.
    }
    
	if (!getWorkLoop()->inGate())
	{
        USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Not inGate.", this, TRBType(command));
	}
	
    if ( callBackF == 0 )
    {
        callBackF = OSMemberFunctionCast(CMDComplete, this, &AppleUSBXHCI::CompleteSlotCommand);
    }
    
	USBLog(7, "AppleUSBXHCI[%p]::WaitForCMD (%s) 1 - num interrupts: %d, num primary: %d, inactive: %d, unavailable: %d, is controller available: %d", this, TRBType(command), (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable);
    
	kr = EnqueCMD(t, command, callBackF, &ret);
    
    if (kr != kIOReturnSuccess)
    {
        USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Command Ring full or stopped %x", this, TRBType(command), kr);
		USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) kr, 0, 1);
       	return retval;
    }
    
	while ( *ret == CMD_NOT_COMPLETED )
	{
		if ( count++ > timeout )
		{
			break;
		}
        
            
		//Don't call CommandSleep -- it breaks the UIM serialization model
		// As we're not going to be woken up by the signal, user a smaller time increment
		for(innercount = 0; innercount < 1000; innercount++)
		{
			IODelay(1);    // 1us
			PollForCMDCompletions(kPrimaryInterrupter);
			if(*ret != CMD_NOT_COMPLETED)
			{
				break;
			}
		}

#if 0       
        else if ( getWorkLoop()->inGate() )
		{
			AbsoluteTime	deadline;
			
			clock_interval_to_deadline(1, NSEC_PER_MS, &deadline);
			
			kr = GetCommandGate()->commandSleep(&ret, deadline, THREAD_ABORTSAFE);	// Callback will signal at &ret
			
			if ( !_workLoop->inGate() )
			{
				USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD - No longer have the gate after sleeping. kr == %p", this, (void*)kr );
			}
			else if( (kr != THREAD_TIMED_OUT) && (kr != THREAD_AWAKENED) )
			{
				USBLog(5, "AppleUSBXHCI[%p]::WaitForCMD - unexpected return from commandSleep: %p", this, (void*)kr);
			}
		}
		else 
		{
            USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD - called outside the gate", this );
            
            // Not sure if returning here is causing a problem.
            IOSleep(1);
		}
#endif
        
	}
    if(count > 1)
    {
        if ( count > timeout )
		{
			USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Command not completed in %dms", this, TRBType(command), (int)count);
			USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) count, 0, 2);
        }
        else
        {
            USBLog(6, "AppleUSBXHCI[%p]::WaitForCMD (%s) - polled completed in %d.%dms", this, TRBType(command), (int)(count-1), (int)(innercount*1));
 			USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) count-1, innercount, 3);
       }
    }
    else
    {
        USBLog(7, "AppleUSBXHCI[%p]::WaitForCMD (%s) - polled completed in %dus", this, TRBType(command), (int)(innercount*1));
		USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) innercount, 0, 4);
    }
	
	if ( (*ret == CMD_NOT_COMPLETED) || (*ret <= MakeXHCIErrCode(0)) )
	{
        if(*ret == CMD_NOT_COMPLETED)
        {
            Write64Reg(&_pXHCIRegisters->CRCR, kXHCI_CA, false);    // Note writes to CMD ring pointer are ignored while command ring is running.
            _waitForCommandRingStoppedEvent = true;

			// wait for at least 5 seconds
			for (count = 0; ((count < 5000) && (_waitForCommandRingStoppedEvent)); count++)
			{
				IODelay(1000);    // 1ms
				PollForCMDCompletions(kPrimaryInterrupter);
			}

            if(_waitForCommandRingStoppedEvent)
            {
                USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - abort, command ring did not stop, count = %d.%d, ret: %d", this, TRBType(command), (int)count, (int)(innercount*1), (int)*ret);

				USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) count, innercount, 5);
            }
            else
            {
                USBLog(2, "AppleUSBXHCI[%p]::WaitForCMD (%s) - abort command ring stop, count = %d.%d, ret: %d", this, TRBType(command), (int)count, (int)(innercount*1), (int)*ret);
				USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) count, innercount, 6);
            }
        }
        if ( (*ret == CMD_NOT_COMPLETED) || (*ret <= MakeXHCIErrCode(0)) )
        {
			USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) ret, 0, 7);
			USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Command failed:%d (num interrupts: %d, num primary: %d, inactive:%d, unavailable:%d, is controller available:%d)", this, TRBType(command), (int)*ret, (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable);
            PrintRuntimeRegs();
            PrintInterrupter(1, 0, "WaitForCMD");
        }
        else
        {
            USBLog(1, "AppleUSBXHCI[%p]::WaitForCMD (%s) - Command succeeded after abort:%d", this, TRBType(command), (int)*ret);
			USBTrace(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) ret, 0, 8);
        }
        
	}
    
    retval = *ret;
    *ret = 0;
    
	USBTrace_End(kUSBTXHCI, kTPXHCIWaitForCmd,  (uintptr_t)this, (uintptr_t) t, retval, 0);
	return (retval);
}

void AppleUSBXHCI::ResetEndpoint(int slotID, int EndpointID)
{
	TRB t;
	
    USBTrace_Start(kUSBTXHCI, kTPXHCIResetEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);
    
	ClearTRB(&t, true);
	SetTRBSlotID(&t, slotID);
	SetTRBEpID(&t, EndpointID);
	
	WaitForCMD(&t, kXHCITRB_ResetEndpoint);
    
    USBTrace_End(kUSBTXHCI, kTPXHCIResetEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);
	
}

#if 1
void AppleUSBXHCI::ClearEndpoint(int slotID, int EndpointID)
{
    UInt32 offs00;
	IOReturn ret;
	TRB t;
	Context * inputContext;
	Context * slotContext;
	Context * epContext;
    
    USBTrace_Start(kUSBTXHCI, kTPXHCIClearEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);

	GetInputContext();
	
	inputContext = GetInputContextByIndex(0);
	inputContext->offs00 = HostToUSBLong(1 << EndpointID);  // Drop flag
	inputContext->offs04 = HostToUSBLong((1 << EndpointID) | 1);	// Add flag This endpoint, plus the device context
	
	// Initialise the input device context, from the existing device context
	inputContext = GetInputContextByIndex(1);
	slotContext = GetSlotContext(slotID);
	*inputContext = *slotContext;
	USBLog(3, "AppleUSBXHCI[%p]::ClearEndpoint - before slotCtx, inputctx[1]", this);
	PrintContext(inputContext);
	offs00 = USBToHostLong(inputContext->offs00);
	offs00 &= ~kXHCISlCtx_resZ0Bit;	// Clear reserved bit if it set
	inputContext->offs00 = HostToUSBLong(offs00);
	//  ****** I'm not sure this is right.
	inputContext->offs0C = 0;	// Nothing in here is an input parameter
	inputContext->offs10 = 0;
	inputContext->offs14 = 0;
	inputContext->offs18 = 0;
	inputContext->offs1C = 0;
	
	// Copy the endpoint's output slot context to the input
	inputContext = GetInputContextByIndex(EndpointID+1);
	epContext = GetEndpointContext(slotID, EndpointID);
	*inputContext = *epContext;
	inputContext->offs14 = 0;
	inputContext->offs18 = 0;
	inputContext->offs1C = 0;
	PrintContext(inputContext);
	
	// Point controller to input context
	ClearTRB(&t, true);
	SetTRBAddr64(&t, _inputContextPhys);
	SetTRBSlotID(&t, slotID);
	
	PrintTRB(6, &t, "ClearEndpoint");
    ret = WaitForCMD(&t, kXHCITRB_ConfigureEndpoint);
	ReleaseInputContext();
	if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
	{
		USBLog(1, "AppleUSBXHCI[%p]::ClearEndpoint - configure endpoint failed:%d", this, (int)ret);
		
		if( (ret == MakeXHCIErrCode(kXHCITRB_CC_CtxParamErr))	|	// Context param error
		   (ret == MakeXHCIErrCode(kXHCITRB_CC_TRBErr))  )	// NEC giving TRB error when its objecting to context
		{
			USBLog(1, "AppleUSBXHCI[%p]::ClearEndpoint - Input Context 0", this);
			PrintContext(GetInputContextByIndex(0));
			USBLog(1, "AppleUSBXHCI[%p]::ClearEndpoint - Input Context 1", this);
			PrintContext(GetInputContextByIndex(1));
			USBLog(1, "AppleUSBXHCI[%p]::ClearEndpoint - Input Context X", this);
			PrintContext(GetInputContextByIndex(EndpointID+1));
		}
		
		
		return;
	}
	
	USBLog(3, "AppleUSBXHCI[%p]::ClearEndpoint - enabling endpoint succeeded", this);
	
    USBTrace_End(kUSBTXHCI, kTPXHCIClearEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);
    
    return;

}
#endif

int 
AppleUSBXHCI::StartEndpoint(int slotID, int EndpointID, UInt16 streamID)
{
    USBTrace_Start(kUSBTXHCI, kTPXHCIStartEndpoint,  (uintptr_t)this, slotID, EndpointID, streamID);

    IOSync();
	Write32Reg(&_pXHCIDoorbells[slotID], (EndpointID + (streamID << kXHCIDB_Stream_Shift) ));
	IOSync();

    USBTrace_End(kUSBTXHCI, kTPXHCIStartEndpoint,  (uintptr_t)this, slotID, EndpointID, streamID);
    
    return (0);
}


bool AppleUSBXHCI::IsStreamsEndpoint(int slotID, int EndpointID)
{
    return(_slots[slotID].potentialStreams[EndpointID] > 1);
}

void 
AppleUSBXHCI::ClearStopTDs(int slotID, int EndpointID)
{
	XHCIRing *ring;
	
	if(IsStreamsEndpoint(slotID, EndpointID))
    {
        for(int i = 1; i<(int)_slots[slotID].maxStream[EndpointID]; i++)
        {
            ring = GetRing(slotID, EndpointID, i);
            if(ring == NULL)
            {
                USBLog(1, "AppleUSBXHCI[%p]::ClearStopTDs - ring does not exist (slot:%d, ep:%d, stream:%d) ", this, (int)slotID, (int)EndpointID, i);
            }
            else
            {
            	ClearTRB(&ring->stopTRB, false);
            }
        }
    }
    else
    {
        ring = GetRing(slotID, EndpointID, 0);
        if(ring == NULL)
        {
            USBLog(1, "AppleUSBXHCI[%p]::ClearStopTDs - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
        }
        else
        {
        	ClearTRB(&ring->stopTRB, false);
        }
    }
}

int 
AppleUSBXHCI::StopEndpoint(int slotID, int EndpointID)
{
	TRB             t;
	UInt32          completionCode, shortfall, streams;
	bool            ED;
    XHCIRing        *ring;
    SInt32          ret;
	
    USBTrace_Start(kUSBTXHCI, kTPXHCIStopEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);
    
	ring = GetRing(slotID, EndpointID, 0);
    USBTrace(kUSBTXHCI, kTPXHCIStopEndpoint,  (uintptr_t)this, ring ? ring->transferRingPhys : 0, 0, 0);

    ClearStopTDs(slotID, EndpointID);
    
	ClearTRB(&t, true);
	SetTRBSlotID(&t, slotID);
	SetTRBEpID(&t, EndpointID);
    
	ret = WaitForCMD(&t, kXHCITRB_StopEndpoint);

    // If PPT
    if ((_errataBits & kXHCIErrataPPT) != 0)
    {
        if(ret == kXHCITRB_CC_NOSTOP)
        {
            USBLog(1, "AppleUSBXHCI[%p]::StopEndpoint - Stop endpoint failed with no stop (slot:%d, ep:%d) device needs to be reset", this, (int)slotID, (int)EndpointID);
            _slots[slotID].deviceNeedsReset = true;
        }
    }
    
    USBTrace_End(kUSBTXHCI, kTPXHCIStopEndpoint,  (uintptr_t)this, slotID, EndpointID, 0);
    
	return(0);
}

IOReturn AppleUSBXHCI::ReturnAllTransfersAndReinitRing(int slotID, int EndpointID, UInt32 streamID)
{
	int						index, type;
	UInt32					shortfall = 0;
    UInt32					enqCycleState;
	IOUSBCompletion			completion;
	XHCIRing				*ring;
    int						enqueIndex;
    UInt32					completionCode;
    USBPhysicalAddress64	phys, stopPhys;
	
    bool stopTRBFound= false;
	
    USBTrace_Start(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
    
	ring = GetRing(slotID, EndpointID, streamID);
	if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - ring does not exist (slot:%d, ep:%d, stream:%d) ", this, (int)slotID, (int)EndpointID, (int)streamID);
        USBTrace(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, 0, 0, 0);
        USBTrace_End(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
        return(kIOReturnBadArgument);
		
	}
	if(ring->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - ******** NULL TRBBuffer (slot:%d, ep:%d, stream:%d) ", this, slotID, EndpointID, (int)streamID);
        USBTrace(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, 0, 0, 1);
        USBTrace_End(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
        return(kIOReturnBadArgument);
	}
	
	if(ring->transferRing == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - ******** NULL transfer ring (slot:%d, ep:%d, stream:%d) ", this, slotID, EndpointID, (int)streamID);
        USBTrace(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, 0, 0, 2);
        USBTrace_End(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
		return(kIOReturnNoMemory);
	}

	if (IsIsocEP(slotID, EndpointID))
    {
		// Before we get here, we have already called QuiesceEndpoint and the ring is stopped
		// However, we have to make sure that we have processed the Stop TRB in our own event
		// ring so that we know that we have processed all of the event TRBs which may have been
		// lingering in the ring
		
        USBTrace(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, 0, 0, 3);
		AppleXHCIIsochEndpoint *pEP = (AppleXHCIIsochEndpoint*)ring->pEndpoint;
		if (pEP)
		{
#define kMaxWaitForRingToStop   120
            int     retries = kMaxWaitForRingToStop;
			// Isoc is handled differently because we can have different transactions on different parts of the queue
			// they need to be completed in the correct order
			// TODO: Do we still need to reinitialize the ring once we have returned everything?
            while (retries-- && (pEP->ringRunning == true))
            {
                // wait up to kMaxWaitForRingToStop for the event ring to get completed
                IOSleep(1);
            }
            if (pEP->ringRunning)
            {
				USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - Isoch ring for EP(%p) never stopped!", this, pEP);
                pEP->ringRunning = false;
            }
			else if (retries < (kMaxWaitForRingToStop-10))
			{
				USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - Isoch ring for EP(%p) took %d ms to stop running!", this, pEP, kMaxWaitForRingToStop-retries);
			}
            USBTrace(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, ring->transferRingPhys, kMaxWaitForRingToStop-retries, 4);
			AbortIsochEP(pEP);
		}
        else
        {
            USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - ******** Isoc transfer ring with no isocEP (slot:%d, ep:%d, stream:%d) ", this, slotID, EndpointID, (int)streamID);
        }
        USBTrace_End(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
        return kIOReturnSuccess;
	}
    else
    {
        index = ring->transferRingDequeueIdx;
        
        if(index == ring->transferRingEnqueueIdx)
        {
            USBLog(6, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - Empty ep:%d (slot:%d, ep:%d, stream:%d) ", this, index,  slotID, EndpointID, (int)streamID);
            return (ReinitTransferRing(slotID, EndpointID, streamID));
        }
        
        // Remember the enqueIndex index now, so if transfers are added to the ring, we don't return them
        // enqueIndex = ring->transferRingEnqueueIdx;
        
        // completionCode  = GetTRBCC(&ring->stopTRB);
        // stopPhys    =   ((UInt64)USBToHostLong(ring->stopTRB.offs0)) + (((UInt64)USBToHostLong(ring->stopTRB.offs4)) << 32);
        // shortfall   =   USBToHostLong(ring->stopTRB.offs8) & kXHCITRB_TR_Len_Mask;
        // If there was no stop event, then phys == 0, so the TRB will not be found on the ring
        // If the stop event was kXHCITRB_CC_Stopped_Length_Invalid. phys is set, need to find it on the ring, shortfall is zero
        // if the stop event was kXHCITRB_CC_Stopped, phys is set, shortfall is non zero.
        
        //
        // REVIEW :: We can safely indicate that no data was transferred because we are returning all transfers. Right??
        // 
        AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);
        
        USBLog(2, "AppleUSBXHCI[%p]::ReturnAllTransfers - (slot:%d, ep:%d, stream:%d) ", this, ring->slotID, ring->endpointID, (int)streamID);
        
        if (pAsyncEP)
        {
            pAsyncEP->Abort();
        }
        else
        {
            USBLog(1, "AppleUSBXHCI[%p]::ReturnAllTransfersAndReinitRing - ******** Async transfer ring with no asyncEP (slot:%d, ep:%d, stream:%d) ", this, slotID, EndpointID, (int)streamID);
        }
    }
    

    USBTrace_End(kUSBTXHCI, kTPXHCIReturnAllTransfers,  (uintptr_t)this, slotID, EndpointID, streamID);
    
    return (ReinitTransferRing(slotID, EndpointID, streamID));
}



IOReturn
AppleUSBXHCI::ReinitTransferRing(int slotID, int EndpointID, UInt32 streamID)
{
	XHCIRing *			ring;
	TRB					t;
	SInt32				err;
    
    USBTrace_Start(kUSBTXHCI, kTPXHCIReInitTransferRing,  (uintptr_t)this, slotID, EndpointID, streamID);
    
	USBLog(6, "AppleUSBXHCI[%p]::ReinitTransferRing - Slot:%d, ep:%d, stream:%d ", this, slotID, EndpointID, (int)streamID);
    
	ring = GetRing(slotID, EndpointID, streamID);
	if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::ReinitTransferRing - ring does not exist (slot:%d, ep:%d, stream:%d) ", this, (int)slotID, (int)EndpointID, (int)streamID);
        return(kIOReturnBadArgument);
		
	}
	
	PrintContext(GetEndpointContext(slotID,EndpointID));
	
	if(ring->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::ReinitTransferRing - ******** NULL ring (slot:%d, ep:%d, stream:%d) ", this, slotID, EndpointID, (int)streamID);
		return(kIOReturnNoMemory);
	}
	
	if( (ring->transferRing == NULL) || (ring->transferRingSize < 2) )
	{
        
		USBLog(1, "AppleUSBXHCI[%p]::ReinitTransferRing - Invalid transfer ring %p, %d", this, ring->transferRing, ring->transferRingSize);
		return(kIOReturnNoMemory);
	}
    
	//  need to do a set tr deque pointer to set the ring back to the beginning and work out what the cycle state should be.
	ClearTRB(&t, true);
	SetTRBSlotID(&t, slotID);
	SetTRBEpID(&t, EndpointID);
    
    if(ring->transferRingEnqueueIdx == ring->transferRingDequeueIdx)
    {
        // if the ring is empty
        
        // Clear the TRBs, except the link at the end
        bzero(ring->transferRing, (ring->transferRingSize - 1) * sizeof(TRB));
        // Set the toggle cycle bit
        ring->transferRing[ring->transferRingSize-1].offsC |= HostToUSBLong(kXHCITRB_TC);
    
        
        ring->transferRingPCS = 1;
        ring->transferRingEnqueueIdx = 0;
        ring->transferRingDequeueIdx = 0;
        
        // TODO :: Reset/Reinit Endpoint Queues here.
    }
    else
    {
		USBLog(2, "AppleUSBXHCI[%p]::ReinitTransferRing - Ring not empty, setting deque:%d, DCS:%d", this, ring->transferRingDequeueIdx, (int)ring->transferRingPCS);
    }
    err = SetTRDQPtr(slotID, EndpointID, streamID, ring->transferRingDequeueIdx);
    
    if(ring->needsDoorbell)
    {
        if (!IsIsocEP(slotID, EndpointID))
        {
            AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);
            
            if (pAsyncEP)
            {
                pAsyncEP->ScheduleTDs();
            }
        }
        
        USBLog(2, "AppleUSBXHCI[%p]::ReinitTransferRing - Ringing doorbell(s) (@%d,%d)", this, slotID, EndpointID);
        if(IsStreamsEndpoint(slotID, EndpointID))
        {
            RestartStreams(slotID, EndpointID, 0);
        }
        else
        {
        	StartEndpoint(slotID, EndpointID);
        }
        ring->needsDoorbell = false;
    }
    
    USBTrace_End(kUSBTXHCI, kTPXHCIReInitTransferRing,  (uintptr_t)this, slotID, EndpointID, streamID);
    return (MungeCommandCompletion(err));
}
 

// Note that ParkRing also does a SetTRDQPtr

int AppleUSBXHCI::SetTRDQPtr(int slotID, int EndpointID, UInt32 stream, int dQindex)
{
	TRB t;
	SInt32 err;
	XHCIRing *ring;
	Context *epCtx;
	int epState;
    
	ring = GetRing(slotID, EndpointID, stream);
	
    if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::SetTRDQPtr - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
		return MakeXHCIErrCode(kXHCITRB_CC_NULLRing);
	}
	
	epCtx = GetEndpointContext(slotID, EndpointID);
	epState = GetEpCtxEpState(epCtx);
	
	// Dequeue pointer should not be changed if the endpoint is already running
	if(epState != kXHCIEpCtx_State_Stopped)
	{
		// For streams endpoints setting dequeue pointer is allowed if the enq and dq indexes are the same
		if(IsStreamsEndpoint(slotID, EndpointID))
		{
			if(ring->transferRingDequeueIdx != ring->transferRingEnqueueIdx)
			{
				USBLog(6, "AppleUSBXHCI[%p]::SetTRDQPtr - called while streams endpoint was not stopped (slot:%d, ep:%d,stream:%d epstate:%d) ", this, (int)slotID, (int)EndpointID, (int)stream, epState);
				return MakeXHCIErrCode(kXHCITRB_CC_CtxStateErr);
			}
		}
		else
		{
			USBLog(1, "AppleUSBXHCI[%p]::SetTRDQPtr - called while endpoint was not stopped (slot:%d, ep:%d, epstate:%d) ", this, (int)slotID, (int)EndpointID, epState);
			return MakeXHCIErrCode(kXHCITRB_CC_CtxStateErr);
		}
	}

	ClearTRB(&t, true);
	SetTRBSlotID(&t, slotID);
	SetTRBEpID(&t, EndpointID);

    
    if(stream == 0)
    {
        SetTRBAddr64(&t, ring->transferRingPhys+dQindex*sizeof(TRB));
        SetTRBDCS(&t, ring->transferRingPCS);
    }
    else
    {
        // Set DequePtr in the TRB, with the same format (inc SCT and PCS) as for a stream context.
        SetStreamCtxAddr64((StreamContext *)&t, ring->transferRingPhys+dQindex*sizeof(TRB), kXHCI_SCT_PrimaryTRB, ring->transferRingPCS);
        SetTRBStreamID(&t, stream);
    }

	err = WaitForCMD(&t, kXHCITRB_SetTRDqPtr);
    PrintTRB(7, &t, "SetTRDQPtr CMD");

    if((err == CMD_NOT_COMPLETED) || (err <= MakeXHCIErrCode(0)))
	{
#if 0
		// TRB error
        if(err == MakeXHCIErrCode(kXHCITRB_CC_CtxStateErr))    // Context state error
        {
            USBLog(2, "AppleUSBXHCI[%p]::SetTRDQPtr - slot context error, qiescing endpoint and trying again", this);
            (void)QuiesceEndpoint(slotID, EndpointID);
            err = WaitForCMD(&t, kXHCITRB_SetTRDqPtr);
            PrintContext(GetSlotContext(slotID));
        }
        if((err == CMD_NOT_COMPLETED) || (err <= MakeXHCIErrCode(0)))
        {
            PrintTRB(1, &t, "SetTRDQPtr");
            if(err == MakeXHCIErrCode(kXHCITRB_CC_CtxStateErr))    // Context state error
            {
                USBLog(1, "AppleUSBXHCI[%p]::SetTRDQPtr - slot context error again", this);
				PrintContext(GetSlotContext(slotID));
            }
            
            PrintContext(GetEndpointContext(slotID, EndpointID));
        }
#endif
		USBLog(1, "AppleUSBXHCI[%p]::SetTRDQPtr - slot context error", this);
		PrintContext(GetSlotContext(slotID));
		PrintContext(GetEndpointContext(slotID, EndpointID));
	}
	else
	{
		ring->transferRingDequeueIdx = dQindex;
	}
    
    return(err);
	
}


bool AppleUSBXHCI::FilterEventRing(int IRQ, bool *needsSignal)
{
	TRB                     nextEvent;
	int                     type, index;
	void *                  p;
	volatile UInt32         offsC;
	bool                    copyEvent = true;
	
    USBTrace_Start(kUSBTXHCIInterrupts, kTPXHCIFilterEventRing,  (uintptr_t)this, IRQ, 0, 0);

	// Don't read entire event at once. 
	// Only read it once you find there is a new event (according to C bit)
	// Else bits of it may not have been ready when you read it
	offsC = _events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx].offsC;
	
	
	if( (USBToHostLong(offsC) & kXHCITRB_C) == _events[IRQ].EventRingCCS)
	{	// New event to dequeue
		int				enque2Idx;
		
		nextEvent.offsC = offsC;
		nextEvent.offs8 = _events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx].offs8;
		nextEvent.offs4 = _events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx].offs4;
		nextEvent.offs0 = _events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx].offs0;
                		
		ClearTRB(&_events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx], false);
		
        //		USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - Updating _EventRingDequeueIdx, before: %d", this, _EventRingDequeueIdx);
		_events[IRQ].EventRingDequeueIdx++;
		if(_events[IRQ].EventRingDequeueIdx >= _events[IRQ].numEvents)
		{
			_events[IRQ].EventRingDequeueIdx = 0;
			_events[IRQ].EventRingCCS = 1 - _events[IRQ].EventRingCCS;
		}
        //		USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - Updating _EventRingDequeueIdx, after: %d", this, _EventRingDequeueIdx);
		_events[IRQ].EventRingDPNeedsUpdate = true;		// Don't update the DQ pointer everytime
        
		type = GetTRBType(&nextEvent);
        //		USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - Found event type:%d", this, type);
		if(type == kXHCITRB_TE)
		{
			UInt32						slotID, EndpointID;
			
			slotID = GetTRBSlotID(&nextEvent);
            if(GetSlotContext(slotID) != NULL)    // Hasn't been deleted
            {
                EndpointID =  (USBToHostLong(nextEvent.offsC) & kXHCITRB_Ep_Mask) >> kXHCITRB_Ep_Shift;
                // Look at low latency Isoc here
                if (IsIsocEP(slotID, EndpointID))
                {
                    XHCIRing *		ring = GetRing(slotID, EndpointID, 0);
					
                    if( (ring == NULL) || (ring->TRBBuffer == NULL) )
                    {
                        OSIncrementAtomic(&_IsocProblem);
                    }
                    else
                    {
				
						AppleXHCIIsochEndpoint *	pEP = (AppleXHCIIsochEndpoint*)ring->pEndpoint;
						
						if (pEP && pEP->ringRunning)
						{
							UInt8						condCode = 	((USBToHostLong(nextEvent.offs8) & kXHCITRB_CC_Mask) >> kXHCITRB_CC_Shift);
							
							// kprintf("XHCI::FilterEventRing - Isoc pEP(%p) condCode(%d)\n", pEP, (int)condCode);
							
							PrintEventTRB(&nextEvent, IRQ, true, ring);
							if ((condCode == kXHCITRB_CC_Success) || (condCode == kXHCITRB_CC_ShortPacket) || (condCode == kXHCITRB_CC_XActErr)) 
							{
								// XHCI does not need to worry about Abort, as each endpoint will be stopped before removing trasactions
								if (pEP->outSlot < kNumTDSlots)
								{

									// How Isoc events work
									// When an Isoc endpoint is created, we instantiate an AppleXHCIIsochTransferDescriptor to manage it
									// Encapsulated within that can be from 1 to 8 transfer opportunities, depending on the endpoint interval
									// A FS device will have one opportunity per AppleXHCIIsochTransferDescriptor, a HS or SS device might
									// have 1, 2, 4, or 8 transfer opportunities. Each transfer opportunity gets rendered on to the ring as
									// one or more TRBs. If there is more than one TRB, then the list of TRBs is concluded with an Event Data
									// TRB which aids in determining how many bytes were actually transferred (for IN endpoints).
									// For OUT endpoints, we do not have the controller generate an event on most of the TRBs unless there is some
									// kind of error during the transfer. We have the xHC generate an event only if there is an error, or if it is 
									// the last TRB of a client's request, or some minimum we set to make sure that we get interrupted occasionally
									// so we can refil the transfer ring. For IN endpoints, we do something similar except that we tell the 
									// controller to generate an event for any short packet (so that we can know how many bytes there were) BUT
									// we supress having that event generate an interrupt until the end of the transfer.
									// so when we get here and we are dequeuing AppleXHCIIsochTransferDescriptor objects (which are on a shadow queue)
									// we need to know how many to dequeue, as quite a few of our objects might have been processed when an event comes in
									// The event points to a TRB. We can tell whether the TRB is part of the AppleXHCIIsochTransferDescriptor at the 
									// top of the queue, and if so whether it is actually the final TRB for that object or not. If it is NOT part of the 
									// object at the top of the queue, then we assume that the object at the top of the queue was completed with 
									// no error and no short packet. If it IS a TRB inside of that object, then if it completes that object we dequeue
									// the object and stop processing the queue, and if it does NOT complete the object, we leave the object on the top
									// of the queue and we stop processing the queue

									AppleXHCIIsochTransferDescriptor *	cachedHead;
									UInt32								cachedProducer;
									UInt32								frIndex;
									UInt16								testSlot, nextSlot, stopSlot;
									UInt16								curMicroFrame;
									uint64_t							timeStamp;
									//UInt64								curFrameNumber = GetFrameNumber();

									stopSlot = pEP->inSlot & (kNumTDSlots-1);				// pEP->inSlot is the next place there may be a new pTD placed
									//curMicroFrame = frIndex & 7;
									
									cachedHead = (AppleXHCIIsochTransferDescriptor*)pEP->savedDoneQueueHead;
									cachedProducer = pEP->producerCount;
									testSlot = pEP->outSlot;
									
									timeStamp = mach_absolute_time();

									// kprintf("XHCI:FilterEventRing - XFer event [%08x] [%08x] [%08x] [%08x]\n", (int)nextEvent.offs0, (int)nextEvent.offs4, (int)nextEvent.offs8, (int)nextEvent.offsC);
									while (testSlot != stopSlot)
									{
										AppleXHCIIsochTransferDescriptor		*pTD = NULL;
										
										nextSlot = (testSlot+1) & (kNumTDSlots-1);
										pTD = pEP->tdSlots[testSlot];
										// kprintf("XHCI::FilterEventRing - testSlot(%d) stopSlot(%d) pTD(%p)\n", (int)testSlot, (int)stopSlot, pTD);
										if (pTD != NULL)
										{
											if (pEP->ringRunning)
											{
												int						frameinTDForEvent;
												bool					eventIsForThisTD = false;
												UInt64					phys = ((UInt64)USBToHostLong(nextEvent.offs0) & ~0xf) + (((UInt64)USBToHostLong(nextEvent.offs4)) << 32);
												int						eventIndex = DiffTRBIndex(phys, ring->transferRingPhys);
												int						indexIntoTD = -1;
												
												if ((eventIndex >= 0) && (eventIndex < ring->transferRingSize))
												{
													indexIntoTD = pTD->FrameForEventIndex(eventIndex);
													
													if (indexIntoTD >= 0)
														eventIsForThisTD = true;
												}
												
												pTD->eventTRB = nextEvent;										// copy the TRB for the use of UpdateFrameList
												
												// because we need the event in order to update the frame list, and because
												// we don't want to save off all of the events just to do the updates at 
												// secondary interrupt time, we go ahead and update the frame lists now, whether
												// it is low latency or not..
												
												pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);
												// place this guy on the backward done queue
												// the reason that we do not use the _logicalNext link is that the done queue is not a null terminated list
												// and the element linked "last" in the list might not be a true link - trust me
												// !eventIsForThisTD means a TD which did not require any events (Isoc OUT)
												// if it is for this TD, only do it if it is for the last frame in the TD, otherwise, expect another event
												if (!eventIsForThisTD || ((indexIntoTD == (pTD->_framesInTD-1))))
												{
													USBLogKP(7, "AppleUSBXHCI[%p]FilterEventRing pEP(%p) removing pTD(%p) from slot (%d)", this, pEP, pTD, testSlot);
													pEP->tdSlots[testSlot] = NULL;
													pTD->_doneQueueLink = cachedHead;
													cachedHead = pTD;
													cachedProducer++;
													OSIncrementAtomic( &(pEP->onProducerQ));
													OSDecrementAtomic( &(pEP->scheduledTDs));
													pEP->outSlot = testSlot;
													USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, (uintptr_t)pEP, pEP->outSlot, 3 );
													
												}
												else
												{
													// no need to send the event to the 2ndary handler.. we are dealing with it and there is nothing to process
													// in the secondary ring at this time
													copyEvent = false;

												}
												if (eventIsForThisTD)
												{
													break;								// don't process any more slots (most like this was a short packet)
												}
											}
										}
										else
										{
											// kprintf("XHCI::FilterEventRing - empty slot(%d)\n", testSlot);
										}
											
										testSlot = nextSlot;
									}
									IOSimpleLockLock( pEP->wdhLock );
									
									pEP->savedDoneQueueHead = cachedHead;	// updates the shadow head
									pEP->producerCount = cachedProducer;	// Validates _producerCount;
									
									IOSimpleLockUnlock( pEP->wdhLock );
								}
							}
							else if ((condCode == kXHCITRB_CC_Stopped) || (condCode == kXHCITRB_CC_Stopped_Length_Invalid) || (condCode == kXHCITRB_CC_RingOverrun) || (condCode == kXHCITRB_CC_RingUnderrun) || (condCode == kXHCITRB_CC_Missed_Service))
							{
								// these conditions will cause us to stop processing TRBs in the Filter routine until a new request is sent and turns it back on again
								// these condition codes will all be processed in the secondary handler instead
								USBLogKP(2, "AppleUSBXHCI[%p]::FilterEventRing Isoc pEP(%p) stopping ring because of condCode(%d)", this, pEP, condCode);
								USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, (uintptr_t)pEP, condCode, 4 );
								pEP->ringRunning = false;                        // indicated the end of the line for this ring
							}
							else
							{
								USBLogKP(2, "AppleUSBXHCI[%p]::FilterEventRing Isoc pEP(%p) ignoring condCode(%d)", this, pEP, condCode);
								USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, (uintptr_t)pEP, condCode, 5 );
							}
						}
						else
						{
							USBLogKP(6, "AppleUSBXHCI[%p]FilterEventRing Isoch pEP(%p) RING NOT RUNNING", this, pEP);
							USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, (uintptr_t)pEP, 0, 6 );
						}
                    }
                }
            }
		}
		else if(type == kXHCITRB_MFWE)
        {	// MF Wrap Event
            uint64_t		tempTime;
            UInt32			frindex;
            //USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - MF Wrap Event", this);
            _frameNumber64 += kXHCIFrameNumberIncrement;
			
			PrintEventTRB(&nextEvent, IRQ, true);
			
            // get the frame index (if possible) so that we can stamp our Tracepoint but will also bail if it has gone away
            frindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
			if (_lostRegisterAccess)
			{
				return false;
			}
			USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, (int)_frameNumber64, frindex, 1 );
            
            _tempAnchorFrame = _frameNumber64 + (frindex >> 3);
            tempTime = mach_absolute_time();
            _tempAnchorTime = *(AbsoluteTime*)&tempTime;
        }
		else
		{
			PrintEventTRB(&nextEvent, IRQ, true);
		}
    
		// Put event on secondary queue
		
        if(needsSignal != NULL)
        {
            *needsSignal = true;
        }
		
		if (copyEvent)  
		{
			enque2Idx = _events[IRQ].EventRing2EnqueueIdx + 1;
			if(enque2Idx >= _events[IRQ].numEvents2)
			{
				enque2Idx = 0;
			}
			if(enque2Idx == _events[IRQ].EventRing2DequeueIdx)
			{
				// secondary ring full
				//USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - *********** 2ndary queue overflow", this);
				OSIncrementAtomic(&_events[IRQ].EventRing2Overflows);
				// Fall through, update queue
			}
			else
			{
				//USBLog(2, "AppleUSBXHCI[%p]::FilterEventRing - enqueing at: %d", this, _EventRing2EnqueueIdx);
				_events[IRQ].EventRing2[_events[IRQ].EventRing2EnqueueIdx] = nextEvent;
				_events[IRQ].EventRing2EnqueueIdx = enque2Idx;
				USBTrace_End(kUSBTXHCIInterrupts, kTPXHCIFilterEventRing,  (uintptr_t)this, 1, 1, 0);
				return (true);
			}
		}
        else    // But if copyEvent is not true, we need to return true here or events will not get processed.
        {
			USBTrace_End(kUSBTXHCIInterrupts, kTPXHCIFilterEventRing,  (uintptr_t)this, 1, 0, 0);
            return(true);
        }
	}
    
    if((_events[IRQ].EventRingDPNeedsUpdate) || ((Read64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP) & kXHCIIRQ_EHB) != 0))
    {
        USBPhysicalAddress64 phys;
        
        if( (_errataBits & kXHCIErrata_NoMSI) != 0)
        {	// Not using MSI, so need to clear IP bit
            Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN, Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN));
            if (_lostRegisterAccess)
            {
                return false;
            }
        }
        
        //	PrintInterrupter(5,IRQ, "before");
        phys = _events[IRQ].EventRingPhys;
        phys += _events[IRQ].EventRingDequeueIdx * sizeof(TRB);
        
        phys |= kXHCIIRQ_EHB;	// Write clear the Event handler busy bit
        
        Write64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP, phys, true);
        USBTrace( kUSBTXHCIInterrupts, kTPXHCIFilterEventRing, (uintptr_t)this, 0, 0,  2 );
        //	PrintInterrupter(5,IRQ, "after");
        
        _events[IRQ].EventRingDPNeedsUpdate = false;
    }

    
    USBTrace_End(kUSBTXHCIInterrupts, kTPXHCIFilterEventRing,  (uintptr_t)this, 0, 0, 0);
    
	return (false);
    
}



void AppleUSBXHCI::DoStopCompletion(TRB *nextEvent)
{
    USBPhysicalAddress64		phys;
    XHCIRing					*ring;
	int							index;
    UInt32						slotID, EndpointID;
	
    // Stopped TD completion
    slotID = GetTRBSlotID(nextEvent);
    EndpointID =  (USBToHostLong(nextEvent->offsC) & kXHCITRB_Ep_Mask) >> kXHCITRB_Ep_Shift;
    if(IsStreamsEndpoint(slotID, EndpointID))
    {
        // streams endpoint
        phys = ((UInt64)USBToHostLong(nextEvent->offs0)) + (((UInt64)USBToHostLong(nextEvent->offs4)) << 32);
        if(phys == 0)
        {
            USBLog(2, "AppleUSBXHCI[%p]::DoStopCompletion - Null phys pointer @%d, %d", this, (int)slotID, (int)EndpointID);
            return;   // Nothing we can do.
        }
        else
        {
            ring = FindStream(slotID, EndpointID, phys, &index, true);
            if(ring == NULL)
            {
                USBLog(1, "AppleUSBXHCI[%p]::DoStopCompletion - Stream not found @%d, %d, phys:%p", this, (int)slotID, (int)EndpointID, (void *)phys);
                (void) FindStream(slotID, EndpointID, phys, &index, false);
                PrintTRB(1, nextEvent, "DoStopCompletion Stream not found");
                return;   // Nothing we can do.
            }
        }
     }
    else
    {
         ring = GetRing(slotID, EndpointID, 0);
    }
    if(ring)
    {
        ring->stopTRB = *nextEvent;
    }
    else
    {
        // Found a stopped transfer TRB, but endpoint is invalid, can't do anything here
        USBLog(1, "AppleUSBXHCI[%p]::DoStopCompletion - Found a stopped transfer TRB, but endpoint is invalid EP @%d, %d", this, (int)slotID, (int)EndpointID);
        PrintTRB(6, nextEvent, "DoStopCompletion invalid stop");
    }
}

bool AppleUSBXHCI::DoCMDCompletion(TRB nextEvent, UInt16 eventIndex)
{
    USBPhysicalAddress64 phys;
    CMDComplete c;
	int index;
   
    UInt32 completionCode = GetTRBCC(&nextEvent);
    
    // Physical address of command dequeued
    phys = (USBToHostLong(nextEvent.offs0) & ~0xf) + (((UInt64)USBToHostLong(nextEvent.offs4)) << 32);
    
    //			USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - Updating _CMDRingDequeueIdx, before: %d", this, _CMDRingDequeueIdx);
    
    index = DiffTRBIndex(phys, _CMDRingPhys);
    //USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - index %d", this, index);
    if( (index < 0) || (index > (_numCMDs-2)) )
    {
        // Bad event, is Phys zero?
        if(phys == 0)
        {
            USBError(1, "AppleUSBXHCI[%p]::DoCMDCompletion - Zero pointer in CCE", this);
        }
        USBError(1, "AppleUSBXHCI[%p]::DoCMDCompletion - bad pointer in CCE: %d", this, (int)index);
        PrintTRB(1, &nextEvent, "DoCMDCompletion CCE bad pointer");
    }
    else
    {
        //USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - index(2) %d", this, index);
        if (completionCode == kXHCITRB_CC_CMDRingStopped)
        {
            if(_waitForCommandRingStoppedEvent)
            {
                USBLog(1, "AppleUSBXHCI[%p]::DoCMDCompletion command ring stopped", this);
                _waitForCommandRingStoppedEvent = false;
            }
            
            // Command stop event points to current deque index
            _CMDRingDequeueIdx = index;
        }
        else
        {
            _CMDRingDequeueIdx = 1+index;
            if(_CMDRingDequeueIdx >= (_numCMDs-1))
            {
                _CMDRingDequeueIdx = 0;
            }
            // USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - Updating _CMDRingDequeueIdx, after: %d", this, _CMDRingDequeueIdx);
            c = _CMDCompletions[index].completionAction;
            _CMDCompletions[index].completionAction = NULL;
            
            if(c != 0)
            {
                
                //USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - Calling completion function: %p(%p)", this, c, p);
                //PrintTRB(&nextEvent, "DoCMDCompletion");
                (*c)(this, &nextEvent, &_CMDCompletions[index].parameter);
            }
            else
            {
                USBLog(2, "AppleUSBXHCI[%p]::DoCMDCompletion - Null completion, assume its been polled: %d (%d)", this, eventIndex, index);
            }
        }
        return(true);
    }
    
    return(false);
}



void AppleUSBXHCI::PollForCMDCompletions(int IRQ)
{
    UInt16			dqIndex;
    int				type;
    

    dqIndex = _events[IRQ].EventRing2DequeueIdx;
    
    // USBTrace_Start( kUSBTXHCI, kTPXHCIPollForCMDCompletion,  (uintptr_t)this, IRQ, (dqIndex != _events[IRQ].EventRing2EnqueueIdx) ? true : false, 0 );

	while ((dqIndex != _events[IRQ].EventRing2EnqueueIdx) && (!_lostRegisterAccess))
	{	// Queue not empty
        TRB						nextEvent;
		int						newDQIdx;	
       
        nextEvent = _events[IRQ].EventRing2[dqIndex];

		type = GetTRBType(&nextEvent);
        
        if( (type == kXHCITRB_CCE) || ( (type == kXHCITRB_NECCCE) && ( (_errataBits & kXHCIErrata_NEC) != 0) ) )
        {	
        	ClearTRB(&_events[IRQ].EventRing2[dqIndex], false);
            USBLog(7, "AppleUSBXHCI[%p]::PollForCMDCompletions - Completing: %d", this, dqIndex);
			USBTrace( kUSBTXHCI, kTPXHCIPollForCMDCompletion,  (uintptr_t)this, 0, 0, 1);
			PrintEventTRB(&nextEvent, IRQ, false);
			// Command completion event, update CMD deque pointer
            DoCMDCompletion(nextEvent, dqIndex);
        }
        else if(type == kXHCITRB_TE)
		{
			UInt32 completionCode;
			completionCode = GetTRBCC(&nextEvent);
            if( (completionCode == kXHCITRB_CC_Stopped) || (completionCode == kXHCITRB_CC_Stopped_Length_Invalid) )
            {
		        ClearTRB(&_events[IRQ].EventRing2[dqIndex], false);
                USBLog(7, "AppleUSBXHCI[%p]::PollForCMDCompletions - Completing stop event: %d", this, dqIndex);
                PrintTRB(7, &nextEvent, "PollForCMDCompletions Stop Event");
				USBTrace( kUSBTXHCI, kTPXHCIPollForCMDCompletion,  (uintptr_t)this, 0, 0, 2);
				PrintEventTRB(&nextEvent, IRQ, false);
                DoStopCompletion(&nextEvent);
            }
        }
		//USBLog(2, "AppleUSBXHCI[%p]::PollForCMDCompletions - Updating dqIndex, before: %d", this, dqIndex);
		newDQIdx = dqIndex+1;
		if(newDQIdx >= _events[IRQ].numEvents2)
		{
			newDQIdx = 0;
		}
		dqIndex = newDQIdx;
		//USBLog(2, "AppleUSBXHCI[%p]::PollForCMDCompletions - Updating dqIndex, after: %d", this, dqIndex);
    }
    // USBTrace_End( kUSBTXHCI, kTPXHCIPollForCMDCompletion,  (uintptr_t)this, IRQ, 0, 0 );
}



bool AppleUSBXHCI::PollEventRing2(int IRQ)
{
#pragma unused(IRQ)
	int								type, index;
    bool							isocProb = false;
	void *							p;
	AppleXHCIIsochEndpoint *		pEP = (AppleXHCIIsochEndpoint*)_isochEPList;
	
    USBTrace_Start( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 0, 0, 0 );

    
    if((Read32Reg(&_pXHCIRegisters->USBSTS)& kXHCIHSEBit) != 0)
    {
        if(!_HSEReported)
        {
            USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - HSE bit set:%x (1)", this, Read32Reg(&_pXHCIRegisters->USBSTS));
        }
        _HSEReported = true;
    }

    
	if(_events[IRQ].EventRing2Overflows > 0)
	{
		SInt32 overflows;
		overflows = _events[IRQ].EventRing2Overflows;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - Secondary event queue %d overflowed: %d", this, IRQ, (int)overflows);
		OSAddAtomic(-overflows, &_events[IRQ].EventRing2Overflows);
	}
	if(_DebugFlag > 0)
	{
		SInt32 flags;
		flags = _DebugFlag;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - DebugFlags: %d", this, (int)flags);
		OSAddAtomic(-flags, &_DebugFlag);
	}
#if 0
	if(_CCEPhysZero > 0)
	{
		SInt32 zeros;
		zeros = _CCEPhysZero;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - Zero pointer in CCE: %d", this, (int)zeros);
		OSAddAtomic(-zeros, &_CCEPhysZero);
	}
	if(_CCEBadIndex > 0)
	{
		SInt32 badindexes;
		badindexes = _CCEBadIndex;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - bad pointer in CCE: %d", this, (int)badindexes);
		OSAddAtomic(-badindexes, &_CCEBadIndex);
	}
#endif
	if(_EventChanged > 0)
	{
		SInt32 changes;
		changes = _EventChanged;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - Event changed after reading: %d", this, (int)changes);
		OSAddAtomic(-changes, &_EventChanged);
	}
	if(_IsocProblem > 0)
	{
        // isocProb = true;
		SInt32 problems;
		problems = _IsocProblem;
		USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - Isoc problems: %d", this, (int)problems);
		OSAddAtomic(-problems, &_IsocProblem);
	}
	
	// Isoc events for normal events are not consumed by the FilterEventRing. However, they will cause the 2ndary interrupt to
	// occur, so now I just go through all of my Isoc endpoints to see if there is any work to do
	while (pEP)
	{
        USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)pEP, 9, pEP->producerCount, pEP->consumerCount );
		if (pEP->consumerCount != pEP->producerCount)
		{
			USBLog(7, "AppleUSBXHCI[%p]::PollEventRing2 - Scavenging Isoc Transactions for EP(%p)", this, pEP);
			ScavengeIsocTransactions(pEP, true);
		}
		pEP = (AppleXHCIIsochEndpoint *)pEP->nextEP;
	}
	
	if(_events[IRQ].EventRing2DequeueIdx != _events[IRQ].EventRing2EnqueueIdx)
	{	// Queue not empty
        TRB nextEvent;
		int newDQIdx, index0;	

        index0 = _events[IRQ].EventRing2DequeueIdx;
        nextEvent = _events[IRQ].EventRing2[_events[IRQ].EventRing2DequeueIdx];
        //USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - nextEvent: %d", this, _EventRing2DequeueIdx);
        // PrintTRB(&nextEvent, "pollEventRing21");
        
		ClearTRB(&_events[IRQ].EventRing2[_events[IRQ].EventRing2DequeueIdx], false);
		
		//USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - Updating _EventRing2DequeueIdx, before: %d", this, _EventRing2DequeueIdx);
		newDQIdx = _events[IRQ].EventRing2DequeueIdx+1;
		if(newDQIdx >= _events[IRQ].numEvents2)
		{
			newDQIdx = 0;
		}
		_events[IRQ].EventRing2DequeueIdx = newDQIdx;
		//USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - Updating _EventRing2DequeueIdx, after: %d", this, _EventRing2DequeueIdx);
		
		type = GetTRBType(&nextEvent);
        
        if( (type == kXHCITRB_CCE) || ( (type == kXHCITRB_NECCCE) && ( (_errataBits & kXHCIErrata_NEC) != 0) ) )
        {	// Command completion event, update CMD deque pointer
            if(DoCMDCompletion(nextEvent, index0))
            {
                return(true);
            }
        }
		else if (type == kXHCITRB_TE)
		{
			UInt32					completionCode, shortfall, slotID, EndpointID;
			USBPhysicalAddress64	phys;
			XHCIRing				*ring;
			bool					ED = false;
			
			// Physical address of TRB dequeued
			phys = ((UInt64)USBToHostLong(nextEvent.offs0) & ~0xf) + (((UInt64)USBToHostLong(nextEvent.offs4)) << 32);

			completionCode = GetTRBCC(&nextEvent);
			
            if( (completionCode == kXHCITRB_CC_Stopped) || (completionCode == kXHCITRB_CC_Stopped_Length_Invalid) )
            {
                DoStopCompletion(&nextEvent);
                return(true);	// Finished with this event
            }

			shortfall   = USBToHostLong(nextEvent.offs8) & kXHCITRB_TR_Len_Mask;
			slotID      = GetTRBSlotID(&nextEvent);
			EndpointID  = (USBToHostLong(nextEvent.offsC) & kXHCITRB_Ep_Mask) >> kXHCITRB_Ep_Shift;
			ED          = USBToHostLong(nextEvent.offsC) & kXHCITRB_ED;
			
            USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 1, slotID, EndpointID );
            USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 2, (int)phys, shortfall  );

			if(IsStreamsEndpoint(slotID, EndpointID))
			{
                // streams endpoint				
				ring = FindStream(slotID, EndpointID, phys, &index, true);
				
                if(ring == NULL)
				{
                    PrintTRB(2, &nextEvent, "pollEventRing findstream not found");
                    ring = FindStream(slotID, EndpointID, phys, &index, false);
                    if(ring == NULL)
                    {
                        USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - stream not found (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
                        return(false);
                    }
                    USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - stream found 2nd time around (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
				}
			}
			else
			{
				ring = GetRing(slotID, EndpointID, 0);
				
                if(ring == NULL)
				{
					USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
					return(false);
				}
				if((completionCode == kXHCITRB_CC_RingUnderrun) || (completionCode == kXHCITRB_CC_RingOverrun) || (completionCode == kXHCITRB_CC_Stopped)|| (completionCode == kXHCITRB_CC_Stopped_Length_Invalid) )
				{
					pEP = (AppleXHCIIsochEndpoint*)ring->pEndpoint;
					// we already set the ringRunning to false in the Filter routine, which we need to do for Abort, which holds the gate
					if (pEP)
					{						
						if (!pEP->activeTDs)
						{
							pEP->outSlot = kNumTDSlots + 1;
							pEP->inSlot = kNumTDSlots + 1;
						}
						if (pEP->waitForRingToRunDry)
						{
							USBLog(6, "AppleUSBXHCI[%p]::PollEventRing2 - ring has run dry, adding Isoc frames", this);
							
                            USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 8, (uintptr_t)pEP, 0 );
							// the ring has now run dry, so go ahead and add to the schedule if needed
							pEP->waitForRingToRunDry = false;

							// only restart if we were waiting for the overrun/underrun and got them
							// if we are here for one of the other two, it is because of an intentional stop
							if((completionCode == kXHCITRB_CC_RingUnderrun) || (completionCode == kXHCITRB_CC_RingOverrun))
								AddIsocFramesToSchedule(pEP);
						}
					}
					// there may be more that needs to be done here, but do NOT trust the value in "phys" right now as it 
					// is not valid with these two condition codes (see the XHCI spec)
					// specifically do not update the transferRingDequeueIdx based on this phys
					USBLog(3, "AppleUSBXHCI[%p]::PollEventRing2 - Ring Underrun or Overrun on EP @%d, %d", this, (int)slotID, (int)EndpointID);
					return(true);
				}
				index = DiffTRBIndex(phys, ring->transferRingPhys);
			}
			PrintEventTRB(&nextEvent, IRQ, false, ring);
			
			if(ring->TRBBuffer == NULL)
			{
				USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - ******** NULL ring (slot:%d, ep:%d) ", this, (int)slotID, (int)EndpointID);
				return(false);
			}
			
			//USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - Transfer ring: %p TRB phys ring:%p, Index: %d, slot:%d, ep:%d, code:%d, shortfall:%d", this, 
			//	   (void *)ring->transferRingPhys, (void *)phys, (int)index, (int)slotID, (int)EndpointID, (int)completionCode, (int)shortfall);
			if(index < 0 || index > ring->transferRingSize)
			{
				UInt32 dummy;
				USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - Index out of range: %d (slot:%d, endpointID:%d)", this, index, (int)slotID, (int)EndpointID);
				USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - phys: %p, ringPhys:%p", this, (void *)phys, (void *)ring->transferRingPhys);
				PrintTRB(1, &nextEvent, "pollEventRing2x");
				if(false && ED)
				{
					index = CountRingToED(ring, ring->transferRingDequeueIdx, &dummy, false);
					USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - Faking ED index: %d", this, index);
					PrintTRB(1, &ring->transferRing[index], "pollEventRing2c");
				}
				else
				{
					return(true);
				}
				
            }
            
			if (IsIsocEP(slotID, EndpointID))
			{	
                ring->transferRingDequeueIdx = 1+index;
                if(ring->transferRingDequeueIdx >= (ring->transferRingSize-1) )
                {
                    ring->transferRingDequeueIdx = 0;
                }
                
				switch (completionCode)
				{
					case kXHCITRB_CC_Success:
					case kXHCITRB_CC_ShortPacket:
					case kXHCITRB_CC_XActErr:
						// these were handled in the Filter routine and can be ignored here
						break;
						
					default:					
						USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - Isoc Transfer event with completion code (%d) on pEP (%p)", this, (int)completionCode, ring->pEndpoint);
						break;
				}
				return true;
			}
			else
			{
                UInt8   contextSpeed    = GetSlCtxSpeed(GetSlotContext(slotID));
                int     epState         = GetEpCtxEpState(GetEndpointContext(slotID, EndpointID));
  
				AppleXHCIAsyncTransferDescriptor *pActiveATD = NULL;
                AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);

                if (!pAsyncEP)
                {
					USBLog(5, "AppleUSBXHCI[%p]::PollEventRing2 - NULL AppleXHCIAsyncEndpoint", this);
                    return true;
                }

                pActiveATD = pAsyncEP->GetTDFromActiveQueueWithIndex(index);
                
				if(pActiveATD == NULL)
				{
                    USBLog(5, "AppleUSBXHCI[%p]::PollEventRing2 - NULL asyncTD @ index %d, epState: %s", this, index, EndpointState(epState));
                    PrintTRB(6, &nextEvent, "PollEventRing2-x1");

					if(epState != kXHCIEpCtx_State_Halted)
					{
						USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - NULL asyncTD, not halted (%d): Index: %d, slot:%d, endpointID:%d", this, epState, index, (int)slotID, (int)EndpointID);
                        return (true);
					}
					else
					{
						USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - NULL asyncTD, endpoint halted (%d): Index: %d, slot:%d, endpointID:%d", this, epState, index, (int)slotID, (int)EndpointID);
					}
                    
                    //
                    // rdar://10932033 Looks like controller will halt the endpoint for intermediate TRBs on error like kXHCITRB_CC_XActErr and not process the EDs 
                    // so this code will take care of that case. Or we have to rely on timeouts to take care of this condition which might not be optimal for some 
                    // class drivers.
                    //

					// Used to get the phys DQ pointer from the endpoint context here, not sure why,
                    // it should be the same as the phys pointer in the event.
					index = CountRingToED(ring, index, &shortfall, true);
					USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - Transfer event advanceRingToED shortfall:%d", this, (int)shortfall);
                    pActiveATD = pAsyncEP->GetTDFromActiveQueueWithIndex(index);
                    
					if (pActiveATD == NULL)
					{
						USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - NULL asyncTD, when advanced to ED (%d): Index: %d, slot:%d, endpointID:%d", this, epState, index, (int)slotID, (int)EndpointID);
						PrintTRB(6, &nextEvent, "pollEventRing25");
						return(true);
					}
					USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - index advanced to: %d", this, index);
				}
                
                //
                // rdar://10925594 Only if ring is not empty, we want to increment the transferRingDequeueIdx
                // HW can post an event for retired TD so SW needs to protect itself from
                // getting its state confused
                //
                if (ring->transferRingEnqueueIdx != ring->transferRingDequeueIdx)
                {
                    //
                    // We want the transferRingDequeueIdx to be at 1 + position of last ED
                    ring->transferRingDequeueIdx = 1+index;
                    if(ring->transferRingDequeueIdx >= (ring->transferRingSize-1) )
                    {
                        ring->transferRingDequeueIdx = 0;
                    }
                }
                else
                {
                    USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - ring is empty but got a transfer event, epState: %s Index: %d, slot:%d, endpointID:%d", this, EndpointState(epState), index, (int)slotID, (int)EndpointID);
                    return (true);
                }
                
				if(ED)
				{
					IOByteCount req = pActiveATD->transferSize;
					
   					shortfall = (UInt32)req - shortfall;
					//USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - Transfer event ED shortfall now:%d, req:%d", this, (int)shortfall, (int)req);

                    USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 3, index, EndpointID );

                    USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 4, shortfall, req );

					//PrintTRB(&nextEvent, "pollEventRing28");
				}
				
				
                bool 		flush    	= false;
                bool 		complete	= false;
                IOReturn	status		= kIOReturnSuccess;
                
                if(completionCode != kXHCITRB_CC_Success)
                {
                    //    USBLog(3, "AppleUSBXHCI[%p]::PollEventRing2 - Completing code:%d, short:%d, index: %d (slotID:%d, endpointID:%d)", this, (int)completionCode, (int)shortfall, (int)index, (int)slotID, (int)EndpointID);
					//			USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - completion: %p,%p,%p for ring:%p", this, completion.target, completion.action, completion.parameter , ring);
					//			USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - completing: %p for ring:%p", this, c , ring);
					USBLog(7, "AppleUSBXHCI[%p]::PollEventRing2 - completing: %p, code:%d, short:%d fragments: %d", this, pActiveATD, (int)completionCode, (int)shortfall, (int)pActiveATD->fragmentedTD);
                    
                    if (pActiveATD->fragmentedTD)
                    {
                        flush    = true;
                    }
                    complete = true;
                    
                    status = MungeXHCIStatus(completionCode, EndpointID&1, contextSpeed);
                    
                    USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 5, slotID, EndpointID);
                    
                    USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 6, completionCode, status);
                }
                else
                {
                    // Last TD and completion is Success
                    if (pActiveATD->interruptThisTD)
                    {
                        complete = pActiveATD->interruptThisTD;
                    }
                }
                
                CheckBuf(c);
                
                pActiveATD->shortfall = shortfall;
               
                if(_slots[slotID].deviceNeedsReset)
                {
                    status = kIOReturnNotResponding;
                }

                pAsyncEP->ScavengeTDs(pActiveATD, status, complete, flush);
			}
		}
		else if(type == kXHCITRB_PSCE)
		{
			UInt32 portID;
            if(!CheckNECFirmware())
            {
                USBLog(2, "AppleUSBXHCI[%p]::PollEventRing2 - This NEC XHCI controller needs a firmware upgrade before it will work. (Current:%x, needed:%x)",this, (unsigned)_NECControllerVersion, (unsigned)kXHCI_MIN_NEC);
            }

			portID = nextEvent.offs0 >> kXHCITRB_Port_Shift;
			USBLog(5, "AppleUSBXHCI[%p]::pollEventRing2x - Port status change event port:%d portSC:%x, portPMSC:%x, portLI:%x", this, (int)portID, 
				   Read32Reg(&_pXHCIRegisters->PortReg[portID-1].PortSC), Read32Reg(&_pXHCIRegisters->PortReg[portID-1].PortPMSC), Read32Reg(&_pXHCIRegisters->PortReg[portID-1].PortLI));

			PrintEventTRB(&nextEvent, IRQ, false);
			EnsureUsability();
		}
		else if(type == kXHCITRB_MFWE)
        {	// MF Wrap Event
            // copy the temporary variables over to the real thing
            // we do this because this method is protected by the workloop gate whereas the FilterInterrupt one is not
            _anchorTime = _tempAnchorTime;
            _anchorFrame = _tempAnchorFrame;
			PrintEventTRB(&nextEvent, IRQ, false);
       }
		else if(type == kXHCITRB_DevNot)
		{
			USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - Device notification event", this);
            USBTrace( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 7, 0, 0);
			PrintTRB(6, &nextEvent, "Device notification event");
			PrintEventTRB(&nextEvent, IRQ, false);
   
        }
		else
		{
            if((nextEvent.offsC & ~kXHCITRB_C) != 0)    // If offsC is all zeros (except cycle bit), this was probably handled polled.
            {
                USBLog(1, "AppleUSBXHCI[%p]::PollEventRing2 - Unhandled event type: %d", this, type);
                PrintTRB(1, &nextEvent, "pollEventRing29");
            }
		}
		
        USBTrace_End( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 0, 0, 0 );

		return(true);
	}
	
    if((Read32Reg(&_pXHCIRegisters->USBSTS)& kXHCIHSEBit) != 0)
    {
        if(!_HSEReported)
        {
            USBError(1, "AppleUSBXHCI[%p]::PollEventRing2 - HSE bit set:%x (3)", this, Read32Reg(&_pXHCIRegisters->USBSTS));
        }
        _HSEReported = true;
    }
    USBTrace_End( kUSBTXHCI, kTPXHCIPollEventRing2,  (uintptr_t)this, 1, 0, 0 );

	return(false);
	
}

bool AppleUSBXHCI::CheckNECFirmware(void)
{
	TRB t;
	SInt32 ret;
    bool result;
    CMDComplete callBackF;
    ClearTRB(&t, true);

	return(true);

    if( (_errataBits & kXHCIErrata_NEC) == 0)
    {   // Not an NEC controller
        return(true);
    }
    
    if(_NECControllerVersion > 0)
    {
        return(_NECControllerVersion >= kXHCI_MIN_NEC);
    }
    
    callBackF = OSMemberFunctionCast(CMDComplete, this, &AppleUSBXHCI::CompleteNECVendorCommand);

    USBLog(2, "AppleUSBXHCI[%p]::CheckNECFirmware - NEC controller", this);
  
    ret = WaitForCMD(&t, kXHCITRB_CMDNEC, callBackF);
    
    if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
    {
        USBLog(1, "AppleUSBXHCI[%p]::CheckNECFirmware - NEC XHCI controller firmware probably out of date, %x+ needed, this controller did not repond to command.",this, (unsigned)kXHCI_MIN_NEC);
        //IOLog("XHCIUIM -- NEC XHCI controller firmware probably out of date\n");
        //_NECControllerVersion = 1;  // So we don't chck again
        return false;
    }
    else
    {
        USBLog(2, "AppleUSBXHCI[%p]::CheckNECFirmware - Command suceeded: Version: %04lx", this, (long unsigned int)ret);
        _NECControllerVersion = ret;
        result = (ret >= kXHCI_MIN_NEC);
        if(!result)
        {
            USBLog(1, "AppleUSBXHCI[%p]::CheckNECFirmware - NEC XHCI controller firmware out of date, %x+ needed, this controller has %x.",this, (unsigned)kXHCI_MIN_NEC, (unsigned)ret);
        }
        return(result);
    }

}


void AppleUSBXHCI::TestCommands(void)
{
#if 0
	TRB t, event;
	int tcount=0;
	SInt32 ret;
	
	while(tcount++ < 800)
	{
		ClearTRB(&t, true);
		
		ret = WaitForCMD(&t, kXHCITRB_CMDNoOp);
		
		if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
		{
			USBLog(1, "AppleUSBXHCI[%p]::TestCommands - NoOp failed:%d", this, (int)ret);
		}
		else
		{
			USBLog(2, "AppleUSBXHCI[%p]::TestCommands - NoOp suceeded", this);
		}
        
	};
#endif
}

void
AppleUSBXHCI::InterruptHandler(OSObject *owner, IOInterruptEventSource * /*source*/, int /*count*/)
{
    register 	AppleUSBXHCI		*controller = (AppleUSBXHCI *) owner;
    static 	Boolean 		emitted;
	
    if (!controller || controller->isInactive() || controller->_lostRegisterAccess || !controller->_controllerAvailable)
	{
		if (!controller)
		{
			USBLog(1, "AppleUSBXHCI[%p]::InterruptHandler - Returning early", controller);
		}
		else
		{
			USBLog(1, "AppleUSBXHCI[%p]::InterruptHandler - Returning early (inactive: %d, lost regsister access: %d)", controller, controller->isInactive(),controller->_lostRegisterAccess);
		}
		
        return;
	}
	
    if (!emitted)
    {
        emitted = true;
        // USBLog("XHCIUIM -- InterruptHandler Unimplimented not finishPending\n");
    }
	
	//controller->PrintInterrupter(5,0, "irq");
    controller->PollInterrupts();
}

bool 
AppleUSBXHCI::PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
#pragma unused(source)
    register AppleUSBXHCI	*controller = (AppleUSBXHCI *)owner;
    bool					result = true;
	
    // If our controller has gone away, or it's going away, or if we're on a PC Card and we have been ejected,
    // then don't process this interrupt.

    USBTrace_Start(kUSBTXHCIInterrupts, kTPXHCIInterruptsPrimaryInterruptFilter,  (uintptr_t)controller, 0, 0, 0);
    if (!controller)
	{
        return false;
	}
 	controller->_numPrimaryInterrupts++;
    if (controller->isInactive())
	{
		controller->_numInactiveInterrupts++;
        return false;
	}
    if (controller->_lostRegisterAccess || !controller->_controllerAvailable)
	{
		controller->_numUnavailableInterrupts++;
        //If terminate has been called we dont need to process interrupts anymore
        if (controller->_filterInterruptSource != NULL)
        {
            controller->_filterInterruptSource->disable();
        }
        return false;
	}
	
    // Process this interrupt
    //
	USBTrace( kUSBTXHCIInterrupts, kTPXHCIInterruptsPrimaryInterruptFilter, (uintptr_t)controller, controller ? controller->isInactive() : 2, controller ? controller->_lostRegisterAccess : 3, 2 );
    
    controller->_filterInterruptActive = true;
    (void) controller->FilterInterrupt(kPrimaryInterrupter);
    result = controller->FilterInterrupt(kTransferInterrupter);
    controller->_filterInterruptActive = false;
    USBTrace_End( kUSBTXHCIInterrupts, kTPXHCIInterruptsPrimaryInterruptFilter,  (uintptr_t)controller, 0, 0, 0 );
	
    return result;
}

bool 
AppleUSBXHCI::FilterInterrupt(int index)
{
#pragma unused(index)
	bool interruptPending = false	;
    bool needsSignal = false;
	UInt32 sts = Read32Reg(&_pXHCIRegisters->USBSTS);
    
	_numInterrupts++;
	interruptPending = ((sts & kXHCIEINT) != 0);
	if(_lostRegisterAccess)
    {
		// Controller is not available disable any further spurious interrupts
		_filterInterruptSource->disable();
        return(false);
    }
	
	USBTrace( kUSBTXHCIInterrupts, kTPXHCIInterruptsPrimaryInterruptFilter, (uintptr_t)this, (uintptr_t)interruptPending, (int)Read32Reg(&_pXHCIRuntimeReg->MFINDEX), 1 );
	USBTrace( kUSBTXHCIInterrupts, kTPXHCIInterruptsPrimaryInterruptFilter, (uintptr_t)this, (int)Read32Reg(&_pXHCIRuntimeReg->IR[index].IMOD), (int)Read32Reg(&_pXHCIRuntimeReg->IR[index].IMAN), 4 );    
    
	if(interruptPending)
	{	// Clear the int bit
		Write32Reg(&_pXHCIRegisters->USBSTS, kXHCIEINT);
	}
	while(FilterEventRing(index, &needsSignal)) ;
    if(!needsSignal)
    {
        needsSignal = ((sts & kXHCIHSEBit) != 0);
    }
    // We will return false from this filter routine, but will indicate that there the action routine should be called by 
    // calling _filterInterruptSource->signalInterrupt(). 
    // This is needed because IOKit will disable interrupts for a level interrupt after the filter interrupt is run, until 
    // the action interrupt is called.  We want to be able to have our filter interrupt routine called before the action 
    // routine runs, if needed.  That is what will enable low latency isoch transfers to work, as when the system is under 
    // heavy load, the action routine can be delayed for tens of ms.

    // Since XHCI uses MSI, this is not technically needed, but it doesn't hurt and will work with the controllers which
    // mess up MSI.
    
    if (needsSignal)
		_filterInterruptSource->signalInterrupt();
    
	return false;
	
}

// OSMemberFunctionCast
#if 0
typedef IOReturn (*fn)(AppleUSBXHCI* xxx, int param);

IOReturn AppleUSBXHCI::TestFn(int param)
{
	USBLog(5, "AppleUSBXHCI[%p]::TestFn - %d , _numCMD: %d", this, param, _numCMDs);
	return(kIOReturnBadArgument);
}
#endif

void 
AppleUSBXHCI::SetVendorInfo(void)
{
    OSData		*vendProp, *deviceProp, *revisionProp;
	
    // get this chips vendID, deviceID, revisionID
    vendProp     = OSDynamicCast(OSData, _device->getProperty( "vendor-id" ));
    if (vendProp)
        _vendorID = *((UInt32 *) vendProp->getBytesNoCopy());
    deviceProp   = OSDynamicCast(OSData, _device->getProperty( "device-id" ));
    if (deviceProp)
        _deviceID = *((UInt32 *) deviceProp->getBytesNoCopy());
    revisionProp = OSDynamicCast(OSData, _device->getProperty( "revision-id" ));
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
}

void AppleUSBXHCI::PrintCapRegs(void)
{
	do
	{
		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - HCIVersion:%lx", this, (long unsigned int)Read16Reg(&_pXHCICapRegisters->HCIVersion));
		if (_lostRegisterAccess)
		{
			break;
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - HCSParams1:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->HCSParams1));
		if (_lostRegisterAccess)
		{
			break;
		}

		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - HCSParams2:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->HCSParams2));
		if (_lostRegisterAccess)
		{
			break;
		}

		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - HCSParams3:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->HCSParams3));
		if (_lostRegisterAccess)
		{
			break;
		}

		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - HCCParams:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->HCCParams));
		if (_lostRegisterAccess)
		{
			break;
		}

		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - DBOff:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->DBOff));
		if (_lostRegisterAccess)
		{
			break;
		}

		USBLog(3, "AppleUSBXHCI[%p]::PrintCapRegs - RTSOff:%lx", this, (long unsigned int)Read32Reg(&_pXHCICapRegisters->RTSOff));
		if (_lostRegisterAccess)
		{
			break;
		}
	} while(0);

}

void AppleUSBXHCI::PrintRuntimeRegs(void)
{
#define PORTTOPRINT (2)
	do
	{
		USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - USBCMD:%lx", this, (long unsigned int)Read32Reg(&_pXHCIRegisters->USBCMD));
		USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - USBSTS:%lx", this, (long unsigned int)Read32Reg(&_pXHCIRegisters->USBSTS));		
		USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - PageSize:%lx", this, (long unsigned int)Read32Reg(&_pXHCIRegisters->PageSize));
		USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - DNCtrl:%lx", this, (long unsigned int)Read32Reg(&_pXHCIRegisters->DNCtrl));
        USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - CRCR:%llx", this, (long long unsigned int)Read64Reg(&_pXHCIRegisters->CRCR));
        USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - DCBAAP:%llx", this, (long long unsigned int)Read64Reg(&_pXHCIRegisters->DCBAAP));
        USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - Config:%lx", this, (long unsigned int)Read32Reg(&_pXHCIRegisters->Config));
        //	USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - PortReg[%d].PortSC:%lx", this, PORTTOPRINT, (long unsigned int)_pXHCIRegisters->PortReg[PORTTOPRINT].PortSC);
		USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - PortReg[%d].PortPMSC:%lx", this, PORTTOPRINT, (long unsigned int)Read32Reg(&_pXHCIRegisters->PortReg[PORTTOPRINT].PortPMSC));
        USBLog(3, "AppleUSBXHCI[%p]::PrintRuntimeRegs - PortReg[%d].PortLI:%lx", this, PORTTOPRINT, (long unsigned int)Read32Reg(&_pXHCIRegisters->PortReg[PORTTOPRINT].PortLI));
        for(int i = 0 ; i<_rootHubNumPorts; i++)
		{
			USBLog(3, "AppleUSBXHCI[%p]::printRuntimeRegs - PortReg[%d].PortSC: 0x%08x", this, i, (uint32_t)Read32Reg(&_pXHCIRegisters->PortReg[i].PortSC));
		}
	} while (0);
}

#pragma mark •••••••• UIM Methods ••••••••

IOReturn AppleUSBXHCI::InitAnEventRing(int IRQ)
{
    IOReturn err;
    _events[IRQ].numEvents = (kXHCIHardwareEventRingBufferSize/sizeof(TRB)) - kSegmentTableEventRingEntries;
    err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, (_events[IRQ].numEvents + kSegmentTableEventRingEntries)*sizeof(TRB), kXHCIEventRingPhysMask,
                     &_events[IRQ].EventRingBuffer, (void **)&_events[IRQ].EventRing, &_events[IRQ].EventRingPhys);
    if(err != kIOReturnSuccess)
    {
        return(err);
    }
    USBLog(3, "AppleUSBXHCI[%p]::InitAnEventRing - Event Ring - pPhysical[%p] pLogical[%p], num Events: %d", this, (void*)_events[IRQ].EventRingPhys, _events[IRQ].EventRing, _events[IRQ].numEvents);

    InitEventRing(IRQ);

    // Set this to a reasonable number. Don't have to be stingy
    _events[IRQ].numEvents2 = kXHCISoftwareEventRingBufferSize/(sizeof(TRB));
    _events[IRQ].EventRing2 = (TRB *)IOMalloc(kXHCISoftwareEventRingBufferSize);
    if(_events[IRQ].EventRing2 == NULL)
    {
        return(kIOReturnNoMemory);
    }
    bzero(_events[IRQ].EventRing2,_events[IRQ].numEvents2*sizeof(TRB));

    _events[IRQ].EventRing2DequeueIdx = 0;
    _events[IRQ].EventRing2EnqueueIdx = 0;
    _events[IRQ].EventRing2Overflows = 0;
    
    return(kIOReturnSuccess);
}


IOReturn AppleUSBXHCI::UIMInitialize(IOService * provider)
{
	UInt32				CapLength;
	UInt32				HCCParams;
    IOReturn			err = kIOReturnSuccess;
	int i, count = 0;
	bool				gotThreads = false;
	UInt16				HCIVersion;
	
#if 0
	fn f;

	f = OSMemberFunctionCast(fn, this, &AppleUSBXHCI::TestFn);
	
	(*f)(this, 1234);
#endif
	
    _device = OSDynamicCast(IOPCIDevice, provider);
    if (_device == NULL)
        return kIOReturnBadArgument;
    
    do {
		
        if (!(_deviceBase = _device->mapDeviceMemoryWithIndex(0)))
        {
            USBError(1, "AppleUSBXHCI[%p]::UIMInitialize - unable to get device memory",  this);
            err = kIOReturnNoResources;
            break;
        }
		// Initialise the errata
		// ** Do this properly sometime
		SetVendorInfo();
		_errataBits |= GetErrataBits(_vendorID, _deviceID, _revisionID);
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - PCI Vendor:%x, device: %x, rev: %x, errata: %x", this,_vendorID, _deviceID, _revisionID, (unsigned int)_errataBits);

		if( (_errataBits & (kXHCIErrataPPT | kXHCIErrata_FrescoLogic | kXHCIErrata_ASMedia)) == 0)
		{
			// By default we will not support XHCI controllers other than PantherPoint and Fresco Logic and ASMedia.  However, we can override this with a boot-arg
			// or with a property list in the driver's personality
			OSBoolean * allowAnyXHCIControllerProp = OSDynamicCast(OSBoolean, getProperty("AllowAnyXHCI"));
			bool		allowAnyXHCIController = (gUSBStackDebugFlags & kUSBEnableAllXHCIControllersMask);
			
			if ( allowAnyXHCIController || ( allowAnyXHCIControllerProp && allowAnyXHCIControllerProp->isTrue()) )
			{
				USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - unsupported XHCI controller, but boot-arg set (%d) or plist override (%p)", this, allowAnyXHCIController, allowAnyXHCIControllerProp);
			}
			else
			{
				USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - unsupported XHCI controller", this);
				err = kIOReturnUnsupported;
				break;
			}
		}
        
		if (!_v3ExpansionData->_onThunderbolt)
		{
			_expansionData->_isochMaxBusStall = kXHCIIsochMaxBusStall;
		}

		_pXHCICapRegisters = (XHCICapRegistersPtr) _deviceBase->getVirtualAddress();
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _pXHCICapRegisters:%p phys:%p", this, _pXHCICapRegisters, (void*)_deviceBase->getPhysicalAddress());
		
		
        // enable the card registers
		if(!_lostRegisterAccess)
		{
			_device->configWrite16(kIOPCIConfigCommand, kIOPCICommandMemorySpace);
		}
		else
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		// Since we have wild cards to support all controllers from some vendors
		// make sure we don't operate pre v1.00 controllers
		HCIVersion = Read16Reg(&_pXHCICapRegisters->HCIVersion);
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		if(HCIVersion < kXHCIVersion0100)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - unsupported XHCI version 0x%x", this, HCIVersion);
			err = kIOReturnUnsupported;
			break;
		}

        
#if 1
		_MaxInterrupters = (Read32Reg(&_pXHCICapRegisters->HCSParams1) & kXHCIMaxInterruptersMask) >> kXHCIMaxInterruptersShift;
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		USBLog(2, "AppleUSBXHCI[%p]::UIMInitialize - _MaxInterrupters:%d", this, (int)_MaxInterrupters);
		
		{
			int msi_interrupt_index = 0;
			bool msi_interrupt_found = false;
			int legacy_interrupt_index = 0;
			int interrupt_index_count = 0;
			int interrupt_type = 0;
			
			while( _device->getInterruptType( interrupt_index_count, &interrupt_type) == kIOReturnSuccess )
			{
				USBLog(2, "AppleUSBXHCI[%p]::UIMInitialize - Found interrupt #%d, type: %x", this, interrupt_index_count, interrupt_type);
				// Use MSI if available
				if( interrupt_type & kIOInterruptTypePCIMessaged )
				{
                    if(!msi_interrupt_found)
                    {
                        msi_interrupt_index = interrupt_index_count;
                        msi_interrupt_found = true;
                    }
					//break;
				}
				else
				{
					legacy_interrupt_index = interrupt_index_count;
				}
                
				interrupt_index_count++;
			}
			
			if(msi_interrupt_found && ( (_errataBits & kXHCIErrata_NoMSI) == 0))
			{
				USBLog(2, "AppleUSBXHCI[%p]::UIMInitialize - Using MSI interrupts", this);
				_filterInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
																								  AppleUSBXHCI::InterruptHandler,	
																								  AppleUSBXHCI::PrimaryInterruptFilter,
																								  _device, msi_interrupt_index );
			}
			else
			{
				USBLog(2, "AppleUSBXHCI[%p]::UIMInitialize - Using legacy interrupts", this);
				_filterInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
																								  AppleUSBXHCI::InterruptHandler,	
																								  AppleUSBXHCI::PrimaryInterruptFilter,
																								  _device, legacy_interrupt_index );
			}
			
		}
		
        
		USBLog(2, "AppleUSBXHCI[%p]::UIMInitialize - _filterInterruptSource:%p", this, _filterInterruptSource);
        
        if ( !_filterInterruptSource )
        {
            USBError(1,"AppleUSBXHCI[%p]: unable to get filterInterruptEventSource",  this);
            err = kIOReturnNoResources;
            break;
        }
        
        err = _workLoop->addEventSource(_filterInterruptSource);
        if ( err != kIOReturnSuccess )
        {
            USBError(1,"AppleUSBXHCI[%p]: unable to add filter event source: 0x%x",  this, err);
            err = kIOReturnNoResources;
            break;
        }
		
		// Enable the interrupt delivery.
		_workLoop->enableAllInterrupts();
		
#endif		
		
		
		// default max number of endpoints we allow to be configured
		_maxControllerEndpoints = kMaxXHCIControllerEndpoints;

		// this will switch the mux for panther point
        EnableXHCIPorts();
		
		if( (_errataBits & kXHCIErrataPPT) != 0)
		{	
			// Panther Point - limit endpoints to 64 <rdar://problem/10385765>
			// does this need a separate errata bit?
			_maxControllerEndpoints = 64;
		}
		
		
	   	CapLength  = Read8Reg(&_pXHCICapRegisters->CapLength);
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - CapLength:%lx", this, (long unsigned int)CapLength);
		
		_pXHCIRegisters = (XHCIRegistersPtr) ( ((uintptr_t)_pXHCICapRegisters) + CapLength);
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _pXHCIRegisters:%p", this, _pXHCIRegisters);
		
		_pXHCIRuntimeReg = (XHCIRunTimeRegPtr) ( ((uintptr_t)_pXHCICapRegisters) + Read32Reg(&_pXHCICapRegisters->RTSOff));
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _pXHCIRuntimeReg:%p", this, _pXHCIRuntimeReg);
		
		_pXHCIDoorbells = (UInt32 *) ( ((uintptr_t)_pXHCICapRegisters) + Read32Reg(&_pXHCICapRegisters->DBOff));
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _pXHCIDoorbells:%p", this, _pXHCIDoorbells);
		
        DisableComplianceMode();
		
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - PortReg:%p", this, ((XHCIRegistersPtr)0)->PortReg);
		
        HCCParams = Read32Reg(&_pXHCICapRegisters->HCCParams);
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}

        _maxPrimaryStreams = (HCCParams & kXHCIMaxPSA_Mask) >> kXHCIMaxPSA_Phase;
        
        if (_maxPrimaryStreams > 0)                                         // leave 0 alone as "no streams support"
            _maxPrimaryStreams = 1 << (_maxPrimaryStreams+1);

        _AC64 = ((HCCParams & kXHCIAC64Bit) != 0);
        _Contexts64 = ((HCCParams & kXHCICSZBit) != 0);
        _contextInUse = 0;

        USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - Max primary streams:%d, AC64:%d, Context Size:%d", this, 
               (int)_maxPrimaryStreams, (int)_AC64, (int)_Contexts64);
        
		PrintCapRegs();
		PrintRuntimeRegs();
		
		// Reset the chip
		// this sets r/s to stop
		Write32Reg(&_pXHCIRegisters->USBCMD, 0);
		if (_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		} 							
        IOSync();
		
		for (i=0; (i < 100) && !(Read32Reg(&_pXHCIRegisters->USBSTS) & kXHCIHCHaltedBit) && !(_lostRegisterAccess); i++)
		{
			IOSleep(1);
		}
		
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		if (i >= 100)
		{
            USBError(1, "AppleUSBXHCI[%p]::UIMInitialize - could not get chip to halt within 100 ms",  this);
			err = kIOReturnInternalError;
			break;
		}
		
        IOReturn status = ResetController();
        
        if( status != kIOReturnSuccess )
        {
            break;
        }
 
		_rootHubNumPorts = (Read32Reg(&_pXHCICapRegisters->HCSParams1) & kXHCINumPortsMask) >> kXHCINumPortsShift;
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _rootHubNumPorts:%lx", this, (long unsigned int)_rootHubNumPorts);
		
		// If the WRC is set, clear it
		for (i=1; i <= _rootHubNumPorts; i++ )
		{
			UInt32		portSC = Read32Reg(&_pXHCIRegisters->PortReg[i-1].PortSC);
			if(_lostRegisterAccess)
			{
				err = kIOReturnNoDevice;
				break;
			}
			USBLog(6, "AppleUSBXHCI[%p]::UIMInitialize - PortReg[%d].PortSC: 0x%08x", this, i-1, (uint32_t)portSC);
		
			if (portSC & kXHCIPortSC_WRC)
			{
				portSC |= (UInt32) kXHCIPortSC_WRC;
				Write32Reg(&_pXHCIRegisters->PortReg[i-1].PortSC, portSC);
				IOSync();
			}
			
			if ((portSC & kXHCIPortSC_CCS) && !(portSC & kXHCIPortSC_CSC))
			{
				// Intel Errata (rdar://10403564):  After a HRST, if we have a connection but no connection status change, then we need to fake it
				USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - PortReg[%d].PortSC: 0x%08x, has a CCS but no CSC", this, i-1, (uint32_t)portSC);
				_synthesizeCSC[i-1] = true;
			}
		}
		
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		_rootHubFuncAddressSS = kXHCISSRootHubAddress;
		_rootHubFuncAddressHS = kXHCIUSB2RootHubAddress;
		
		// Set the number of device slots.
		_numDeviceSlots = Read32Reg(&_pXHCICapRegisters->HCSParams1) & kXHCINumDevsMask;
		if (_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _numDeviceSlots:%d", this, _numDeviceSlots);
		
		// If you want less slots, mess with _numDeviceSlots here
		if(_numDeviceSlots > kMaxSlots)
        {
            _numDeviceSlots = kMaxSlots;
        }
		
		UInt32  tmp = Read32Reg(&_pXHCIRegisters->Config);
		if (_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		Write32Reg(&_pXHCIRegisters->Config, ( tmp & ~kXHCINumDevsMask) | _numDeviceSlots);
		if (_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
        // Turn on all device notifications, they'll be logged in PollEventring
		Write32Reg(&_pXHCIRegisters->DNCtrl, 0xffff);
		if (_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		// Allocate the device context base address array.
		err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, kXHCIDCBAElSize * (_numDeviceSlots+1), kXHCIDCBAAPhysMask,
						 &_DCBAABuffer, (void **)&_DCBAA, &_DCBAAPhys);
		
		if(err != kIOReturnSuccess)
		{
			return(err);
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - DCBAA - pPhysical[%p] pLogical[%p]", this, (void*)_DCBAAPhys, _DCBAA);
		
		// Zero out the DCBAA entries
		for(i = 0; i<=_numDeviceSlots; i++)
		{
			_DCBAA[i] = 0;
		}
		
		Write64Reg(&_pXHCIRegisters->DCBAAP, _DCBAAPhys);
		
		// Allocate the command ring
		
		_numCMDs = PAGE_SIZE/sizeof(TRB);
		
		_CMDCompletions = IONew(XHCICommandCompletion, _numCMDs);
		if(_CMDCompletions == NULL)
			break;
		bzero(_CMDCompletions, _numCMDs * sizeof (XHCICommandCompletion));
              
		err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, _numCMDs*sizeof(TRB), kXHCICMDRingPhysMask,
						 &_CMDRingBuffer, (void **)&_CMDRing, &_CMDRingPhys);
		if(err != kIOReturnSuccess)
		{
			break;
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - CMD Ring - pPhysical[%p] pLogical[%p], num CMDs: %d", this, (void*)_CMDRingPhys, _CMDRing, _numCMDs);
		
		InitCMDRing();
		
		_istKeepAwayFrames = (Read32Reg(&_pXHCICapRegisters->HCSParams2) & kXHCIIST_Mask) >> kXHCIIST_Phase;
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		if((_istKeepAwayFrames & 8) != 0)
		{
			// IST in terms of frames
			_istKeepAwayFrames &= 7;
		}
		else
		{
			// IST in MicroFrames
			_istKeepAwayFrames = 1;
		}

        setProperty("ISTKeepAway", _istKeepAwayFrames, 8);
        
		_ERSTMax = (Read32Reg(&_pXHCICapRegisters->HCSParams2) & kXHCIERSTMax_Mask) >> kXHCIERSTMax_Phase;
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - _ERSTMax  %d", this, _ERSTMax);
		
		err = InitAnEventRing(kPrimaryInterrupter);
		if(err != kIOReturnSuccess)
		{
			break;
		}
		err = InitAnEventRing(kTransferInterrupter);
		if(err != kIOReturnSuccess)
		{
			break;
		}
		
		_CCEPhysZero = 0;
		_CCEBadIndex = 0;
		_EventChanged = 0;
		_IsocProblem = 0;
		
		if (_Contexts64 == false)
		{
			err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, (kXHCI_Num_Contexts+1)*sizeof(Context), kXHCIInputContextPhysMask,
						 &_inputContextBuffer, (void **)&_inputContext, &_inputContextPhys);
		}
		else
		{
			err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, (kXHCI_Num_Contexts+1)*sizeof(Context64), kXHCIInputContextPhysMask,
							 &_inputContextBuffer, (void **)&_inputContext64, &_inputContextPhys);
		}
		
		if(err != kIOReturnSuccess)
		{
			break;
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - Input context - pPhysical[%p] pLogical[%p]", this, (void*)_inputContextPhys, (_Contexts64 == true) ? (void *)_inputContext64 : (void *)_inputContext);
        
		_numScratchpadBufs = (Read32Reg(&_pXHCICapRegisters->HCSParams2) & kXHCIMaxScratchpadBufsLo_Mask) >> kXHCIMaxScratchpadBufsLo_Shift;
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		_numScratchpadBufs |= (((Read32Reg(&_pXHCICapRegisters->HCSParams2) & kXHCIMaxScratchpadBufsHi_Mask) >> kXHCIMaxScratchpadBufsHi_Shift) << kXHCIMaxScratchpadBufsLo_Width);
		if(_lostRegisterAccess)
		{
			err = kIOReturnNoDevice;
			break;
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - Scratchpad bufs %d", this, _numScratchpadBufs);

		_SBABuffer = NULL;		
		
		if(_numScratchpadBufs != 0)
		{
			USBPhysicalAddress64 *dummy;
			mach_vm_size_t pgSz, pgSzMask, pgAlignmentMask;
			IOBufferMemoryDescriptor *scratchpadBuf;
			
			_ScratchPadBuffs = OSArray::withCapacity(_numScratchpadBufs);
			if (_ScratchPadBuffs == NULL)
			{
				err = kIOReturnNoMemory;
				break;
			}

			pgSz = Read32Reg(&_pXHCIRegisters->PageSize) << 12;
			if (_lostRegisterAccess)
			{
				err = kIOReturnNoDevice;
				break;
			}
			
			pgSzMask = pgSz - 1;
			pgAlignmentMask = (~pgSzMask) & 0xffffffff;
			USBLog(3, "AppleUSBXHCI[%p]::UIMInitialize - PageSize: %d, page size mask: %llx, alignment mask: %llx", this, (int)pgSz, pgSzMask, pgAlignmentMask);
			
			// Allocate the scratchpad address array.
			err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, kXHCIDCBAElSize * _numScratchpadBufs, kXHCIDCBAAPhysMask,
							 &_SBABuffer, (void **)&_SBA, &_SBAPhys);
			if(err != kIOReturnSuccess)
			{
				break;
			}
			
			for(i = 0; i<_numScratchpadBufs; i++)
			{
				err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, pgSz, pgAlignmentMask,
								 &scratchpadBuf, (void **)&dummy, &_SBA[i]);
				if(err != kIOReturnSuccess)
				{
					break;
				}
				_ScratchPadBuffs->setObject(scratchpadBuf);
				scratchpadBuf->release();
			}
			if(err != kIOReturnSuccess)
			{
				break;
			}
			_DCBAA[0] = _SBAPhys;
		}
		
		// Initialise some state variables
		for(i = 0; i < kMaxDevices; i++)
		{
			_devHub[i] = 0;
			_devPort[i] = 0;
			_devMapping[i] = 0;
			_devEnabled[i] = false;
		}
		for(i = 0 ; i < kMaxPorts; i++)
		{
			_prevSuspend[i] = false;
			_suspendChangeBits[i] = false;
		}
		_stateSaved = false;
		_fakedSetaddress = false;
		
		_filterInterruptActive = false;
		_frameNumber64 = 0;
		_numInterrupts = 0;
		_numPrimaryInterrupts = 0;
		_numInactiveInterrupts = 0;
		_numUnavailableInterrupts = 0;
        
        _HSEReported = false;
        if( (_errataBits & kXHCIErrata_ParkRing) != 0)
        {
            XHCIRing dummyRing;
            if(AllocRing(&dummyRing, 1) == kIOReturnSuccess)
            {
                _DummyBuffer = dummyRing.TRBBuffer;
                _DummyRingPhys = dummyRing.transferRingPhys;
                _DummyRingCycleBit = dummyRing.transferRingPCS;
            }
            else
            {
                USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - couldn't allocate dummy ring", this);
            }
        }
		
        _NECControllerVersion = 0;
        
		_debugCtr = 0;
		_debugPattern = 0xdeadbeef;
		
		bzero(_slots, sizeof(_slots));

		// Process Extended Capability	
		DecodeExtendedCapability();

		gotThreads = true;
		for (i=0; i < _rootHubNumPorts; i++)
		{
			_rhResumePortTimerThread[i] = thread_call_allocate((thread_call_func_t)RHResumePortTimerEntry, (thread_call_param_t)this);
            _rhResetPortThread[i] = thread_call_allocate((thread_call_func_t)RHResetPortEntry, (thread_call_param_t)this);
			if (!_rhResumePortTimerThread[i] || !_rhResetPortThread[i])
			{
				gotThreads = false;
				break;
			}
		}
		if (!gotThreads)
			continue;
		
        
		CheckSleepCapability();
        
        _uimInitialized = true;

		registerService();
				
		return kIOReturnSuccess;
		//return kIOReturnIOError;
		
	} while (false);
	
	// There was an error, clean up
    
	UIMFinalize();
	
    return(err);
}


void AppleUSBXHCI::DecodeSupportedProtocol(XHCIXECPRegistersPtr protocolBase)
{
	XHCIXECPProtoRegistersPtr CurrentProtoBase = (XHCIXECPProtoRegistersPtr)protocolBase;
	
	USBLog(3, "AppleUSBXHCI[%p]::DecodeSupportedProtocol - xEP CapabilityID %d", this, CurrentProtoBase->Header.CapabilityID);

	USBLog(3, "AppleUSBXHCI[%p]::DecodeSupportedProtocol portSpeed: %d startPort: %d endPort: %d nameString: 0x%lx", this, (int)CurrentProtoBase->MajRevision, 
		   (int)CurrentProtoBase->compatiblePortOffset, (int)CurrentProtoBase->compatiblePortOffset+CurrentProtoBase->compatiblePortCount, (long unsigned int)CurrentProtoBase->NameString);
	
	if( CurrentProtoBase->MajRevision == kUSBSSMajversion && CurrentProtoBase->NameString == kUSBNameString )
	{
		// Initialize SS Root Hub Ports
		_v3ExpansionData->_rootHubNumPortsSS = CurrentProtoBase->compatiblePortCount;
		_v3ExpansionData->_rootHubPortsSSStartRange = CurrentProtoBase->compatiblePortOffset;
	}
	else if ( CurrentProtoBase->MajRevision == kUSBHSMajversion && CurrentProtoBase->NameString == kUSBNameString )
	{
		// Initialize HS Root Hub Ports
		_v3ExpansionData->_rootHubNumPortsHS = CurrentProtoBase->compatiblePortCount;
		_v3ExpansionData->_rootHubPortsHSStartRange = CurrentProtoBase->compatiblePortOffset;
	}
}

void AppleUSBXHCI::DecodeExtendedCapability()
{
	UInt32	HCSParams	= Read32Reg(&_pXHCICapRegisters->HCCParams);
	UInt32	xECP		= (HCSParams & kXHCIxECP_Mask) >> kXHCIxECP_Shift;
	XHCIXECPRegistersPtr CurrentXECPBase;
	
	if (_lostRegisterAccess)
	{
		return;
	}
	
	CurrentXECPBase = _pXHCIXECPRegistersBase = (XHCIXECPRegistersPtr) ( ((uintptr_t)_pXHCICapRegisters) + ((xECP) << 2) );
	
	// Decode extended capability list
	do{
		switch (CurrentXECPBase->Header.CapabilityID)
		{
			case kXHCISupportedProtocol:
				DecodeSupportedProtocol(CurrentXECPBase);
				break;
				
			default:
				USBLog(3, "AppleUSBXHCI[%p]::DecodeExtendedCapability - xEP CapabilityID  %d not implemented", this, CurrentXECPBase->Header.CapabilityID);
				break;
		}
		
		if(CurrentXECPBase->Header.NextPtr == 0)
			break;
		
		CurrentXECPBase = (XHCIXECPRegistersPtr) ( ((uintptr_t)CurrentXECPBase) + ((CurrentXECPBase->Header.NextPtr) << 2) );	
	}while(1);
}

void AppleUSBXHCI::FinalizeAnEventRing(int IRQ)
{
    if (_events[IRQ].EventRingBuffer)
    {
        _events[IRQ].EventRingBuffer->complete();
        _events[IRQ].EventRingBuffer->release();
        _events[IRQ].EventRingBuffer = 0;
    }
	if (_events[IRQ].EventRing2)
	{
		IOFree(_events[IRQ].EventRing2, kXHCISoftwareEventRingBufferSize);
	}
    
}

IOReturn AppleUSBXHCI::UIMFinalize()
{
    int     i;
	IOBufferMemoryDescriptor *scratchpadBuf;
    
	USBTrace( kUSBTXHCI, kTPXHCIUIMFinalize , (uintptr_t)this, isInactive(), (uintptr_t)_pXHCIRegisters, (uintptr_t)_device);
	USBLog(1, "AppleUSBXHCI[%p]::UIMFinalize", this);
    
	if (_acpiDevice )
	{
		_acpiDevice->release();
		_acpiDevice = NULL;
	}
	
	// Cleanup some stuff
 	if (_DCBAABuffer)
	{
		_DCBAABuffer->complete();
		_DCBAABuffer->release();
		_DCBAABuffer = 0;
	}
	
	if (_CMDCompletions)
	{
		IODelete(_CMDCompletions, XHCICommandCompletion, _numCMDs);
		_CMDCompletions = 0;
	}
	
	if (_CMDRingBuffer)
	{
		_CMDRingBuffer->complete();
		_CMDRingBuffer->release();
		_CMDRingBuffer = 0;
	}
	
	FinalizeAnEventRing(kPrimaryInterrupter);
	FinalizeAnEventRing(kTransferInterrupter);
	
	if (_inputContextBuffer)
	{
		_inputContextBuffer->complete();
		_inputContextBuffer->release();
		_inputContextBuffer = 0;
	}
	
	if (_ScratchPadBuffs)
	{
		for (i=0; i < (int)_ScratchPadBuffs->getCount(); i++)
		{
			scratchpadBuf = (IOBufferMemoryDescriptor *)_ScratchPadBuffs->getObject(i);
			scratchpadBuf->complete();
		}
		_ScratchPadBuffs->flushCollection();
		_ScratchPadBuffs->release();
		_ScratchPadBuffs = 0;
	}
    
	if (_SBABuffer)
	{
		_SBABuffer->complete();
		_SBABuffer->release();
		_SBABuffer = 0;
	}
	
	
	// Remove the interruptEventSource we created
    //
    if (_filterInterruptSource && _workLoop)
    {
        _workLoop->removeEventSource(_filterInterruptSource);
        _filterInterruptSource->release();
        _filterInterruptSource = NULL;
    }
	
	if (_deviceBase)
	{
		_deviceBase->release();
		_deviceBase = 0;
	}
    
    for (i=0; i < _rootHubNumPorts; i++)
    {
        if (_rhResumePortTimerThread[i])
        {
            thread_call_cancel(_rhResumePortTimerThread[i]);
            thread_call_free(_rhResumePortTimerThread[i]);
            _rhResumePortTimerThread[i] = NULL;
        }
        if (_rhResetPortThread[i])
        {
            thread_call_cancel(_rhResetPortThread[i]);
            thread_call_free(_rhResetPortThread[i]);
            _rhResetPortThread[i] = NULL;
        }
    }
   
    if( (_errataBits & kXHCIErrata_ParkRing) != 0)
    {
        if(_DummyBuffer)
        {
            _DummyBuffer->complete();
            _DummyBuffer->release();
            _DummyBuffer = 0;
        }
    }
    
   _uimInitialized = false;
	
	return kIOReturnSuccess;
}

IOReturn
AppleUSBXHCI::message( UInt32 type, IOService * provider,  void * argument )
{
	IOReturn	returnValue = kIOReturnSuccess;
	UInt32		disableMuxedPorts = ((gUSBStackDebugFlags & kUSBDisableMuxedPortsMask) >> kUSBDisableMuxedPorts);
	UInt8		controller  = kControllerXHCI;
    
    USBLog(3, "AppleUSBXHCI[%p]::message type: %p, argument = %p isInactive = %d ",  this, (void*)type,  argument, isInactive());
	
    // Let our superclass decide handle this method
    // messages
    //
    returnValue = super::message( type, provider, argument );
	
	switch (type)
	{
		case kIOUSBMessageMuxFromXHCIToEHCI:
			controller = kControllerEHCI;
			// intentional fall through..
			
		case kIOUSBMessageMuxFromEHCIToXHCI:
			// Make sure we are ON before we handle mux hand offs from EHCI -> XHCI
			if(_lostRegisterAccess)
			{
				USBLog(3, "AppleUSBXHCI[%p]::message type: %p, argument = %p Controller Not Available",  this, (void*)type,  argument);
				return returnValue;
			}
			
			if ( !disableMuxedPorts && _device && !isInactive() )
			{
				// Panther point, switch the mux.
				if( (_errataBits & kXHCIErrataPPTMux) != 0)
				{	
					HCSelect((UInt8)(uint64_t)argument, controller);

					USBLog(3, "AppleUSBXHCI[%p]::message type: %p port: %d to %s controller [HCSEL: %lx]", this, (void*)type, (UInt8)(uint64_t)argument, (controller == kControllerEHCI) ? "EHCI" : "XHCI", (long unsigned int)_device->configRead32(kXHCI_XUSB2PR));
				}
			}
			break;

        case kIOUSBMessageHubPortDeviceDisconnected:
			// Make sure we are ON before we handle mux hand offs from EHCI -> XHCI
			if(_lostRegisterAccess)
			{
				USBLog(3, "AppleUSBXHCI[%p]::message type: %p, argument = %p Controller Not Available",  this, (void*)type,  argument);
				return returnValue;
			}
			
			if ( !disableMuxedPorts && _device && !isInactive() )
			{
				// Panther point, switch the mux.
				if( (_errataBits & kXHCIErrataPPTMux) != 0)
				{	
                    HCSelectWithMethod((char*)argument);
                    
					USBLog(3, "AppleUSBXHCI[%p]::message type: %p method %s to %s controller [HCSEL: %lx]", this, (void*)type, (char*)argument, "XHCI", (long unsigned int)_device->configRead32(kXHCI_XUSB2PR));
				}
			}
			break;

			
		default:
			returnValue = kIOReturnUnsupported;
			break;
	}
	
	return returnValue;
}



// Control
IOReturn AppleUSBXHCI::UIMCreateControlEndpoint(UInt8				functionNumber,
												UInt8				endpointNumber,
												UInt16			maxPacketSize,
												UInt8				speed)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(maxPacketSize)
#pragma unused(speed)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 1- Obsolete method called", this);
    return(kIOReturnInternalError);
}


void AppleUSBXHCI::CompleteSlotCommand(TRB *t, void *param)
{
	int CC;
	UInt32 res;
	
	CC = GetTRBCC(t);
	
	if(CC == kXHCITRB_CC_Success)
	{
		res = GetTRBSlotID(t);
	}
	else
	{
		res = MakeXHCIErrCode(CC);
	}
    
	*(UInt32 *)param = res;
	
    if ( !getWorkLoop()->onThread() )
    {
        GetCommandGate()->commandWakeup(param);
    }
	
}

void AppleUSBXHCI::CompleteNECVendorCommand(TRB *t, void *param)
{
	int CC;
	UInt32 res;
	
	CC = GetTRBCC(t);
	
	if(CC == kXHCITRB_CC_Success)
	{
		res = (t->offs8 & 0xffff);
	}
	else
	{
		res = 1000+CC;
	}
    
	*(UInt32 *)param = res;
	
	GetCommandGate()->commandWakeup(param);
	
}


// Don't need to use locks here, this is all on the workloop. Just check you're not doing something wrong.
void AppleUSBXHCI::GetInputContext(void)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
	if ( !_workLoop->inGate() )
		panic ( "AppleUSBXHCI::GetInputContext[%p] called without workloop lock held\n", this );
#endif
	
    if(_contextInUse > 0)
    {
        USBLog(1, "AppleUSBXHCI[%p]::GetInputContext - context in use: %d", this, _contextInUse);
    }
	
    _contextInUse++;
	
	if (_Contexts64 == true)
	{
		bzero((void *)_inputContext64, (kXHCI_Num_Contexts+1)*sizeof(Context64));		
	}
	else
	{
		bzero((void *)_inputContext, (kXHCI_Num_Contexts+1)*sizeof(Context));
	}
}

Context * AppleUSBXHCI::GetInputContextByIndex(int index)
{
	Context *	ctx = NULL;
	
	if (_Contexts64 == false)
	{
		ctx = &_inputContext[index];
	}
	else
	{
		ctx = (Context *)&_inputContext64[index];
	}
	
	return ctx;
}


void AppleUSBXHCI::ReleaseInputContext(void)
{
    if(_contextInUse == 0)
    {
        USBLog(1, "AppleUSBXHCI[%p]::ReleaseInputContext - context already released", this);
        return;
    }
    _contextInUse--;
}

IOReturn AppleUSBXHCI::AddressDevice(UInt32 slotID, UInt16 maxPacketSize, bool setAddr, UInt8 speed, int highSpeedHubSlot, int highSpeedPort)
{
	// Expected:
	//   Endpoint zero transfer ring has been allocated.
	//   Output context is allocated and DCBAA points to it.
	
	SInt32		ret=0;
    UInt32		portSpeed, rootHubPort;
	UInt32		routeString = 0;
	TRB			t;
	XHCIRing *	ring0;
    int			hub, port;
	Context *	inputContext;
	Context *	deviceContext;
		
    USBLog(3, "AppleUSBXHCI[%p]::AddressDevice - _devZeroPort: %d, _devZeroHub:%d", this, _devZeroPort, _devZeroHub);
    
    hub = _devZeroHub; 
    port = _devZeroPort;
    for(int i = 0; i < kMaxUSB3HubDepth; i++)
    {
        if( (hub == _rootHubFuncAddressSS) || (hub == _rootHubFuncAddressHS) )
        {
            break;
        }
        // Add port to route string here
        if(port > 15)
        {
            port = 15;
        }
        routeString = (routeString << 4) + port;
        port = _devPort[hub];
        hub = _devHub[hub];
        USBLog(4, "AppleUSBXHCI[%p]::AddressDevice - Next hub: %d, port:%d, routeString: %x", this, hub, port, (unsigned int)routeString);
    }
    if( (hub != _rootHubFuncAddressSS) && (hub != _rootHubFuncAddressHS) )
    {
        USBError(1, "AppleUSBXHCI[%p]::AddressDevice - Root hub port not found in topology: hub:%d, rootHubSS: %d rootHubHS: %d", this, hub, _rootHubFuncAddressSS, _rootHubFuncAddressHS);
        return(kIOReturnInternalError);
    }
    rootHubPort = port;

    
    //USBLog(2, "AppleUSBXHCI[%p]::AddressDevice - SS port:%d, HS port:%d ", this, (int)_v3ExpansionData->_rootHubPortsSSStartRange , (int)_v3ExpansionData->_rootHubPortsHSStartRange);

	ring0 = GetRing(slotID, 1, 0);
	if(ring0 == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - ring zero does not exist (slot:%d) ", this, (int)slotID);
		return(kIOReturnInternalError);
	}
	if(ring0->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - ******** NULL ring (slot:%d, ring:1) ", this, (int)slotID);
		return(kIOReturnInternalError);
	}
	
	GetInputContext();
	
	// Set A0 and A1 of the input control context, we're affecting the slot and the default endpoint
	inputContext = GetInputContextByIndex(0);
	inputContext->offs04 = HostToUSBLong(kXHCIBit0 | kXHCIBit1);
	
	// Initialise the input slot context
	
	// Root hub port number
	inputContext = GetInputContextByIndex(1);
	SetSlCtxRootHubPort(inputContext, rootHubPort);	
	
	// Context Entries = 1, slot and default endpoint
	inputContext->offs00 = HostToUSBLong(1 << kXHCISlCtx_CtxEnt_Shift);
	
	// Set the speed, sec 6.2.2.1 says we need to, 4.3.3 doesn't.
	portSpeed = (Read32Reg(&_pXHCIRegisters->PortReg[rootHubPort-1].PortSC) & kXHCIPortSC_Speed_Mask) >> kXHCIPortSC_Speed_Shift;
	if (_lostRegisterAccess)
	{
		USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - Controller not available ring (slot:%d, ring:1) ", this, (int)slotID);
		return kIOReturnNoDevice;
	}
	
	
	// Note portSpeed is in terms of xHCI PSIDs
	// Speed is passed in from the controller is in terms of USB.h, they're different
	
	if(portSpeed >= kXHCISpeed_Super)
	{
		SetSlCtxSpeed(inputContext, portSpeed);
	}
	else
	{
		// Not a super speed device. Use port speed, or speed passed in as appropriate
		if(speed ==  kUSBDeviceSpeedHigh)
		{
			SetSlCtxSpeed(inputContext, kXHCISpeed_High);
		}
		else
		{
			if(speed == kUSBDeviceSpeedFull)
			{
				SetSlCtxSpeed(inputContext, kXHCISpeed_Full);
			}
			else
			{
				// Low speed.
				SetSlCtxSpeed(inputContext, kXHCISpeed_Low);
			}
			if(highSpeedHubSlot != 0)
			{
				SetSlCtxTTPort(inputContext, highSpeedPort);
				SetSlCtxTTSlot(inputContext, highSpeedHubSlot);
				SetSlCtxMTT(inputContext, GetSlCtxMTT(GetSlotContext(highSpeedHubSlot)));
			}
		}
	}
	USBLog(3, "AppleUSBXHCI[%p]::AddressDevice - Port %d speed is: %d, device speed is: %d", this, (int)rootHubPort, (int)portSpeed, GetSlCtxSpeed(inputContext));
	
	SetSlCtxInterrupter(inputContext, kTransferInterrupter);
	SetSlCtxRouteString(inputContext, routeString);
	
	if(portSpeed >= kXHCISpeed_Super)
	{	// Only one valid MPS for superspeed devices.
		maxPacketSize = 512;
	}
	
	// Initialise the input context for endpoint zero
	
	// These are set because the context is cleared to zero
	// max burst size = 0 
	// interval = 0
	// max primary streams = 0
	// Mult = 0
	
	// Ep type
	inputContext = GetInputContextByIndex(2);
	SetEPCtxEpType(inputContext, kXHCIEpCtx_EPType_Control);
	// max packet size
	SetEPCtxMPS(inputContext,maxPacketSize);
	// TR dequeue pointer
	SetEPCtxDQpAddr64(inputContext, ring0->transferRingPhys+ring0->transferRingDequeueIdx*sizeof(TRB));
	// Dequeue cycle state, same as producer cycle state
	SetEPCtxDCS(inputContext, ring0->transferRingPCS);
	// CErr = 3
	SetEPCtxCErr(inputContext, 3);
	
	// Point controller to input context
	ClearTRB(&t, true);
	
	SetTRBAddr64(&t, _inputContextPhys);
	SetTRBSlotID(&t, slotID);
	
	if(!setAddr)
	{
		SetTRBBSRBit(&t, 1);
	}
	
	PrintTRB(6, &t, "AddressDevice");
	//PrintContext(&_inputContext[0]);
	//PrintContext(&_inputContext[1]);
	//PrintContext(&_inputContext[2]);
	
	ret = WaitForCMD(&t, kXHCITRB_AddressDevice);
	
	ReleaseInputContext();
	if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
	{
		USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - Address device failed:%d", this, (int)ret);
		
		if(ret == MakeXHCIErrCode(kXHCITRB_CC_CtxParamErr))	// Context param error
		{
			USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - Input Context 0", this);
			PrintContext(GetInputContextByIndex(0));
			USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - Input Context 1", this);
			PrintContext(GetInputContextByIndex(1));
			USBLog(1, "AppleUSBXHCI[%p]::AddressDevice - Input Context 2", this);
			PrintContext(GetInputContextByIndex(2));
		}
		
		return(MungeXHCIStatus(ret, 0));
	}
    
	USBLog(6, "AppleUSBXHCI[%p]::AddressDevice - Address device success: SlotState: %d USB Address: %d ", this, 
           GetSlCtxSlotState(GetSlotContext(slotID)), GetSlCtxUSBAddress(GetSlotContext(slotID)) );
    
	
	return(kIOReturnSuccess);
}	



IOReturn 		
AppleUSBXHCI::configureHub(UInt32 address, UInt32 flags)
{
	bool multiTT;
	UInt32 TTThinkTime, NumPorts, slotID;
	TRB t;
	SInt32 ret=0;
	Context *	slotContext;
	Context *	inputContext;
	
    if( (address == _rootHubFuncAddressSS) || (address == _rootHubFuncAddressHS) )
	{
        return kIOReturnSuccess;
    }
	slotID = GetSlotID(address);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::configureHub - unknown address: %d", this, (int)address);
		return(kIOReturnBadArgument);
	}
	
	
	multiTT = ((flags & kUSBHSHubFlagsMultiTTMask) != 0);
	
	if(flags & kUSBHSHubFlagsMoreInfoMask)
	{
		TTThinkTime = ((flags & kUSBHSHubFlagsTTThinkTimeMask) >> kUSBHSHubFlagsTTThinkTimeShift);
		NumPorts = ((flags & kUSBHSHubFlagsNumPortsMask) >> kUSBHSHubFlagsNumPortsShift);
		USBLog(3, "AppleUSBXHCI[%p]::configureHub (HS) - TTThinkTime: %d, NumPorts: %d, MultiTT: %s", this, (int)TTThinkTime, (int)NumPorts, multiTT ? "true" : "false");
	}
	else
	{
		// No info on these, fake them
		TTThinkTime = 3;	// Worst case
		NumPorts = 15;
		USBLog(3, "AppleUSBXHCI[%p]::configureHub (HS) - faking it TTThinkTime: %d, NumPorts: %d, MultiTT: %s", this, (int)TTThinkTime,(int) NumPorts, multiTT ? "true" : "false");
	}
    
	GetInputContext();
	inputContext = GetInputContextByIndex(0);
	
	// Set A0 of the input control context, we're affecting just the slot
	inputContext->offs04 = HostToUSBLong(kXHCIBit0);
	// Initialise the input device context, from the existing device context
	inputContext = GetInputContextByIndex(1);
	slotContext = GetSlotContext(slotID);
	*inputContext = *slotContext;
	
	inputContext->offs00 |= kXHCISlCtx_HubBit; // Its a hub
	SetSlCtxMTT(inputContext, multiTT);
	
	ResetSlCtxNumPorts(inputContext, NumPorts);
	ResetSlCtxTTT(inputContext, TTThinkTime);
	
	//USBLog(2, "AppleUSBXHCI[%p]::configureHub - InputContext[0/1]", this);
	//PrintContext(&_inputContext[0]);
	//PrintContext(&_inputContext[1]);
	
    
	// Point controller to input context
	ClearTRB(&t, true);
	
	SetTRBAddr64(&t, _inputContextPhys);
	SetTRBSlotID(&t, slotID);
    
	// Evaluate context is the obvious command here, but it doesn't work
	// It seems it did in the 0.95 spec. We have to use configure endpoint instead
	//ret = WaitForCMD(&t, kXHCITRB_EvaluateContext);
	ret = WaitForCMD(&t, kXHCITRB_ConfigureEndpoint);
	
	ReleaseInputContext();
	if((ret == CMD_NOT_COMPLETED) || (ret < MakeXHCIErrCode(0)))
	{
		USBLog(1, "AppleUSBXHCI[%p]::configureHub - Configure endpoint failed:%d", this, (int)ret);
		
		if(ret == MakeXHCIErrCode(kXHCITRB_CC_CtxParamErr))	// Context param error
		{
			USBLog(1, "AppleUSBXHCI[%p]::configureHub - Input Context 0", this);
			PrintContext(GetInputContextByIndex(0));
			USBLog(1, "AppleUSBXHCI[%p]::configureHub - Input Context 1", this);
			PrintContext(GetInputContextByIndex(1));
			USBLog(1, "AppleUSBXHCI[%p]::configureHub - Input Context 2", this);
			PrintContext(GetInputContextByIndex(2));
		}
		
		
		return(kIOReturnInternalError);
	}
	else
	{
		//USBLog(2, "AppleUSBXHCI[%p]::configureHub - Sucessfull, output slot context:", this);
		//PrintContext(&_slots[slotID].deviceContext[0]);
	}
    
	
    return kIOReturnSuccess;
}

IOReturn 		
AppleUSBXHCI::UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags)
{
#pragma unused(highSpeedPort)
	
    switch (command)
    {
		case kUSBHSHubCommandAddHub:
			USBLog(3, "AppleUSBXHCI[%p]::UIMHubMaintenance - adding hub %d with flags 0x%x", this, highSpeedHub, (uint32_t)flags);
			return configureHub(highSpeedHub, flags);
			
			break;
			
		case kUSBHSHubCommandRemoveHub:
			USBLog(3, "AppleUSBXHCI[%p]::UIMHubMaintenance - deleting hub %d", this, highSpeedHub);
			break;
			
		default:
			return kIOReturnBadArgument;
    }
    return kIOReturnSuccess;
}



IOReturn AppleUSBXHCI::UIMCreateControlEndpoint(UInt8			functionNumber,
												UInt8			endpointNumber,
												UInt16			maxPacketSize,
												UInt8			speed,
												USBDeviceAddress highSpeedHub,
												int			    highSpeedPort)
{
	IOReturn err;
    
	USBLog(6, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - fn: %d, ep:%d, MaxPkt:%d, speed:%d, hdHub:%d, hsPort:%d", this, 
           functionNumber, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort);

	USBLog(6, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint - _rootHubFuncAddressSS: %d, _rootHubFuncAddressHS: %d", this, _rootHubFuncAddressSS, _rootHubFuncAddressHS);
	if( (functionNumber == _rootHubFuncAddressSS) || (functionNumber == _rootHubFuncAddressHS) )
	{
		USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Faking root hub ring", this);
        return(kIOReturnSuccess);
    }
	
    // make sure we don't exceed the max configured endpoints allowed
    err = TestConfiguredEpCount();
    if(err != kIOReturnSuccess)
    {
        return(err);
    }
	
	if(maxPacketSize == 9)
	{	// Superspeed endpoint
		maxPacketSize = 512;
	}
	
	if(functionNumber == 0)
	{
		XHCIRing *ring0;
		SInt32 slotID = 0;
		
        
        // Dev zero, bring a new slot online
        TRB t;
        
        ClearTRB(&t, true);
        PrintSlotContexts();

        slotID = WaitForCMD(&t, kXHCITRB_EnableSlot);
        PrintSlotContexts();

        if((slotID == CMD_NOT_COMPLETED) || (slotID <= MakeXHCIErrCode(0)))
        {
            if(slotID == MakeXHCIErrCode(kXHCITRB_CC_NoSlots))
            {
                USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Run out of device slots, returning: %x", this, kIOUSBDeviceCountExceeded);
                return(kIOUSBDeviceCountExceeded);
            }
            USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Enable slot failed:%d", this, (int)slotID);
            return(kIOReturnInternalError);
        }
        
        // Device zero is using this slot ID.
        _devMapping[0] = slotID;
        _devEnabled[0] = true;
        
        USBLog(6, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Enable slot succeeded slot: %d (fn:%d, ep:%d)", this, (int)slotID, functionNumber, endpointNumber);
		
		// Transfer ring for endpoint zero
		ring0 = CreateRing(slotID, 1, 0);
		if(ring0 == NULL)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint - ring zero not created (slot:%d) ", this, (int)slotID);
			return(kIOReturnInternalError);
		}
		if(ring0->TRBBuffer != NULL)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint - ******** ring already exists (slot:%d, ring:1) ", this, (int)slotID);
			return(kIOReturnInternalError);
		}
		
		err = AllocRing(ring0);
		USBLog(6, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 (epID: 1) - tr ring: %p size: %d", this, ring0->transferRing, ring0->transferRingSize);
		if(err != kIOReturnSuccess)
		{
			return(kIOReturnNoMemory);
		}
		

        // Make the output context (the actual context the controller will use).
		if (_Contexts64 == false)
		{
			err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, kXHCI_Num_Contexts*sizeof(Context), kXHCIContextPhysMask,
							 &_slots[slotID].buffer, (void **)&_slots[slotID].deviceContext, &_slots[slotID].deviceContextPhys);
		}
		else
		{
			err = MakeBuffer(kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, kXHCI_Num_Contexts*sizeof(Context64), kXHCIContextPhysMask,
							 &_slots[slotID].buffer, (void **)&_slots[slotID].deviceContext64, &_slots[slotID].deviceContextPhys);
		}
        if(err != kIOReturnSuccess)
        {
            DeallocRing(ring0);
            
            return(kIOReturnNoMemory);
        }
        
        if (ring0->pEndpoint == NULL)
        {
            ring0->endpointType = kXHCIEpCtx_EPType_Control;
            ring0->pEndpoint = AllocateAppleXHCIAsyncEndpoint(ring0, maxPacketSize, 0, 0);
            
            if(ring0->pEndpoint == NULL)
            {
                DeallocRing(ring0);
                return kIOReturnNoMemory;
            }
            USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint - inc ConfiguredEpCount (ring0)", this);
            IncConfiguredEpCount();
        }
        
        USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Output context - pPhysical[%p] pLogical[%p]", this, (void*)_slots[slotID].deviceContextPhys, _slots[slotID].deviceContext);
        SetDCBAAAddr64(&_DCBAA[slotID], _slots[slotID].deviceContextPhys);
        err = AddressDevice(slotID, maxPacketSize, false, speed, GetSlotID(highSpeedHub), highSpeedPort);

        if ( err != kIOReturnSuccess)
		{
			USBLog(2, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint  returning 0x%x", this, (uint32_t) err);
		}
		
		return err;
	}
	
	if(endpointNumber == 0)
	{
		// This already exists with the Address device, just change maxPacketSize
		// if necessary
		UInt16 currMPS;
		int slotID;
		TRB t;
		SInt32 ret=0;
		Context * inputContext;
		
		slotID = GetSlotID(functionNumber);
		
		if(slotID == 0)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - unknown function number: %d", this, functionNumber);
			return(kIOReturnInternalError);
		}
		
		currMPS = GetEpCtxMPS(GetEndpointContext(slotID, 1));
			
		if(currMPS == maxPacketSize)
		{
			USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - currMPS == maxPacketSize: %d", this, currMPS );
		
			return(kIOReturnSuccess);
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - need to change max packet size current: %d, wanted: %d", this, currMPS, maxPacketSize );
        
		GetInputContext();
	
		inputContext = GetInputContextByIndex(0);
		// Set A1 of the input control context, we're affecting only the default endpoint
		inputContext->offs04 = HostToUSBLong(kXHCIBit1);
		// max packet size
		inputContext = GetInputContextByIndex(2);
		SetEPCtxMPS(inputContext,maxPacketSize);		
		
		// Point controller to input context
		ClearTRB(&t, true);
		
		SetTRBAddr64(&t, _inputContextPhys);
		SetTRBSlotID(&t, slotID);
		
		USBLog(5, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Evaluate Context TRB:", this);
		PrintTRB(3, &t, "UIMCreateControlEndpoint");
		
		ret = WaitForCMD(&t, kXHCITRB_EvaluateContext);
		
		ReleaseInputContext();
		if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Evaluate Context failed:%d", this, (int)ret);
			
			if(ret == MakeXHCIErrCode(kXHCITRB_CC_CtxParamErr))	// Context param error
			{
				USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Input Context 0", this);
				PrintContext(GetInputContextByIndex(0));
				USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Input Context 1", this);
				PrintContext(GetInputContextByIndex(1));
				USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Input Context 2", this);
				PrintContext(GetInputContextByIndex(2));
			}
			
			
			return(kIOReturnInternalError);
		}		
		
        return(kIOReturnSuccess);
	}
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlEndpoint 2 - Unimplimented method called", this);
    return(kIOReturnInternalError);
}




// method in 1.8 and 1.8.1
IOReturn AppleUSBXHCI::UIMCreateControlTransfer(short				functionNumber,
												short				endpointNumber,
												IOUSBCompletion	completion,
												void				*CBP,
												bool				bufferRounding,
												UInt32			bufferSize,
												short				direction)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(completion)
#pragma unused(CBP)
#pragma unused(bufferRounding)
#pragma unused(bufferSize)
#pragma unused(direction)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 1 - Obsolete method called", this);
    return(kIOReturnInternalError);
}


TRB *
AppleUSBXHCI::GetNextTRB(XHCIRing *ring, void *xhciTD, TRB **StartofFragment, bool firstFragment)
{
	int nextEnqueueIndex;
	TRB *t;
    UInt32 offsC;
    
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    if ( !getWorkLoop()->inGate() )
    {
		USBLog(1, "AppleUSBXHCI[%p]::GetNextTRB - Not inGate", this);
    }
#endif
    if(StartofFragment == NULL)
    {
        USBLog(1, "AppleUSBXHCI[%p]::GetNextTRB - NULL StartofFragment pointer", this);
        return(NULL);
    }

	if(ring->transferRingEnqueueIdx > ring->transferRingSize-2)
	{
		USBLog(1, "AppleUSBXHCI[%p]::GetNextTRB - bad index: %d", this, ring->transferRingEnqueueIdx);
		return(NULL);
	}
    
	if(ring->transferRingEnqueueIdx == ring->transferRingSize-2)
	{	

        if(ring->transferRingDequeueIdx == 0)
		{	// Ring is full
			USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - Ring full 1, not expanding!", this);
#if 0
            if(ExpandRing(ring) != kIOReturnSuccess)
            {
                return(NULL);
            }
			return(GetNextTRB(ring, c, StartofFragment, firstFragment));
#endif
            return (NULL);
		}
		
        if(xhciTD == NULL)
		{
			// Not the last TRB in TD, chain the link
			SetTRBChainBit(&ring->transferRing[ring->transferRingSize-1], true);
			PrintTransferTRB(&ring->transferRing[ring->transferRingSize-1], ring, ring->transferRingSize-1);
			//USBLog(5, "AppleUSBXHCI[%p]::GetNextTRB - Set link chain bit 1 (index:%d)", this, ring->transferRingSize-1);
		}
		else
		{
			// Last TRB of TD, unset the chain on the link
			SetTRBChainBit(&ring->transferRing[ring->transferRingSize-1], false);
			PrintTransferTRB(&ring->transferRing[ring->transferRingSize-1], ring, ring->transferRingSize-1);
			//USBLog(5, "AppleUSBXHCI[%p]::GetNextTRB - Set link chain bit 0 (index:%d)", this, ring->transferRingSize-1);
		}
        
		nextEnqueueIndex = 0;	
		SetTRBCycleBit(&ring->transferRing[ring->transferRingSize-1], ring->transferRingPCS);		// Set the bit on the link
		PrintTransferTRB(&ring->transferRing[ring->transferRingSize-1], ring, ring->transferRingSize-1);
		ring->transferRingPCS =  1- ring->transferRingPCS;
    }
	else
	{
        if( (*StartofFragment != NULL) && (ring->transferRingEnqueueIdx == 0) )
        {
            // This is being enqueued at the start of the ring
            // and it is not the start of a fragment, so move the fragment
            // down to start of ring.
            int startIndex,startIndex0, fragSize;
            startIndex = (int)(*StartofFragment - ring->transferRing);
            fragSize = ring->transferRingSize - 1 - startIndex;
            if(ring->transferRingDequeueIdx <= (fragSize+1))
            {	// Ring is full
                USBLog(4, "AppleUSBXHCI[%p]::GetNextTRB - Ring full 3, not expanding!; Start:%d, fragSize:%d, deq:%d (enq:%d)", this, startIndex, fragSize, ring->transferRingDequeueIdx, ring->transferRingEnqueueIdx);
#if 0
                if(ExpandRing(ring) != kIOReturnSuccess)
                {
                    return(NULL);
                }
                return(GetNextTRB(ring, c, StartofFragment, firstFragment));
#endif
                return (NULL);
            }
            // Need to copy fragment down to start of ring, so it does not contain link.
            
            // startIndex0 = startIndex;
            nextEnqueueIndex = 0;
            while (startIndex < (ring->transferRingSize-1))
            {
                // Copy from startIndex to nextEnqueueIndex, flip the cycle bit on the destination
                
                // Don't flip the cycle bits on the source. That would keep them safe from being
                // executed, but it messes them up the next time you get to this part of the ring.
                // The fragment head with the cycle bit set to not available is enough to keep
                // the controller from executing these TRBs.
                
                ring->transferRing[nextEnqueueIndex] = ring->transferRing[startIndex];
                ring->transferRing[nextEnqueueIndex].offsC ^= USBToHostLong(kXHCITRB_C);
				PrintTransferTRB(&ring->transferRing[nextEnqueueIndex], ring, nextEnqueueIndex);
                //USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - copyed %d->%d", this, startIndex, nextEnqueueIndex);
                //PrintTRB(&ring->transferRing[nextEnqueueIndex], "GetNextTRB");
                nextEnqueueIndex++;
                startIndex++;
            }
            // Copy the link to the old start of fragment, make sure to copy the cycle bit last (startIndex is transferRingSize-1 here)
            (**StartofFragment).offs0 = ring->transferRing[startIndex].offs0;
            (**StartofFragment).offs4 = ring->transferRing[startIndex].offs4; 
            (**StartofFragment).offs8 = ring->transferRing[startIndex].offs8;
            offsC = ring->transferRing[startIndex].offsC;
            
            // USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - index: %d (next:%d @sl:%d, ep:%d)", this, ring->transferRingEnqueueIdx, nextEnqueueIndex, ring->slotID, ring->endpointID);

            if(firstFragment)
            {
                // the first fragment in the TD, unset the chain bit
                //USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - unsetting chain on link", this);
                offsC &= ~HostToUSBLong(kXHCITRB_CH);
            }
            else
            {
                // Between two fragments, set the link
                //USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - setting chain on link", this);
                offsC |= HostToUSBLong(kXHCITRB_CH);
            }
            (**StartofFragment).offsC = offsC;
			PrintTransferTRB(*StartofFragment, ring, (int)(*StartofFragment - ring->transferRing));
            //     PrintTRB(*StartofFragment, "StartofFragment");
            //     PrintTRB(&ring->transferRing[startIndex0], "startIndex0");
            // Now tell the calling code where the start of fragment has moved to.
            *StartofFragment = &ring->transferRing[0];
            
            ring->transferRingEnqueueIdx = nextEnqueueIndex;
            // nextEnqueueIndex now points to current enqueue head, so point to next one 
            nextEnqueueIndex++;
        }
		else 
        {
            if(ring->transferRingEnqueueIdx+1 == ring->transferRingDequeueIdx)
            {
                USBLog(4, "AppleUSBXHCI[%p]::GetNextTRB - Ring full 2, not expanding!", this);
#if 0
                if(ExpandRing(ring) != kIOReturnSuccess)
                {
                    return(NULL);
                }
                return(GetNextTRB(ring, c, StartofFragment, firstFragment));
#endif
                return (NULL);
            }
            nextEnqueueIndex = ring->transferRingEnqueueIdx+1;
        }
	}
	
	//USBLog(2, "AppleUSBXHCI[%p]::GetNextTRB - index: %d (next:%d @sl:%d, ep:%d), command: %p", this, ring->transferRingEnqueueIdx, nextEnqueueIndex, ring->slotID, ring->endpointID, c);
	t = &ring->transferRing[ring->transferRingEnqueueIdx];
	
	ring->transferRingEnqueueIdx = nextEnqueueIndex;
	ClearTRB(t, false);
    
    //PrintTRB(t, "getNextTRB end");
    
    if(*StartofFragment == NULL)
    {
        *StartofFragment = t;
    }
	return(t);
    
	
}

void AppleUSBXHCI::PutBackTRB(XHCIRing *ring, TRB *t)
{
    int putBackIndex;
    
    putBackIndex = (int)(t - ring->transferRing);
    if( (putBackIndex < 0) || (putBackIndex > (ring->transferRingSize-1)) )
    {
        USBLog(1, "AppleUSBXHCI[%p]::PutBackTRB - Index out of range: %d", this, putBackIndex);
        return;
    }
    while(ring->transferRingEnqueueIdx != putBackIndex)
    {
        if(ring->transferRingEnqueueIdx == ring->transferRingDequeueIdx)
        {
            USBLog(1, "AppleUSBXHCI[%p]::PutBackTRB - Bad TRB pointer, Ring empty: %d (%d - %d)", this, putBackIndex, ring->transferRingDequeueIdx, ring->transferRingEnqueueIdx);
            return;
        }
        if(ring->transferRingEnqueueIdx == 0)
        {   // Wrapped back under, jump ovr link TRB
            ring->transferRingEnqueueIdx = ring->transferRingSize - 1;
            // And flip the cycle state back.
            ring->transferRingPCS = 1-ring->transferRingPCS;
        }
        else
        {
            ring->transferRingEnqueueIdx--;
        }
    }
}



IOReturn AppleUSBXHCI::GenerateNextPhysicalSegment(TRB *t, IOByteCount *req, UInt32 bufferOffset, IODMACommand *dmaCommand)
{
    UInt32 bytesThisTRB=0;
    if(*req > 0)
    {
        UInt32						numSegments;
        IODMACommand::Segment64		segments;
        UInt64						offset;
        IOReturn					status;
        
        numSegments = 1;
        offset = bufferOffset;
        status = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);
        if (status || (numSegments != 1))		// Cope with just one segment at a time
        {
            USBLog(1, "AppleUSBXHCI[%p]::GenerateNextPhysicalSegment - Error generating segments, err: %x, numsegments:%d", this, status, (int)numSegments);
            if( (status == kIOReturnSuccess) && (numSegments != 1))
            {
                status = kIOReturnInternalError;
            }
            return status;
        }
        SetTRBAddr64(t, segments.fIOVMAddr);
        
        bytesThisTRB = (64 * 1024) - ((UInt32)segments.fIOVMAddr & 0xffff);	// Can't cross a 64k boundary
        if(segments.fLength < bytesThisTRB)
        {
            bytesThisTRB = (UInt32)segments.fLength;
        }
        if(*req > bytesThisTRB)
        {
            *req = bytesThisTRB;
        }
        //USBLog(2, "AppleUSBXHCI[%p]::GenerateNextPhysicalSegment - offset:%lx, bytesThisTRB:%d, seg.flen:%d, addr:%p", this, (unsigned long)bufferOffset, (int)bytesThisTRB, (int)segments.fLength, (void *)segments.fIOVMAddr);
    }
    else
    {
        // Zero length buffer, address is ignored, and its already zero
    }
    return(kIOReturnSuccess);
}



#define DEBUG_FRAGMENTS (0)

#if DEBUG_FRAGMENTS
void AppleUSBXHCI::PrintTRBs(XHCIRing *ring, const char *s, TRB *StartofFragment, TRB *endofFragment)
{
	int enq, deq, siz, deq1, istart, end;
    int slot, endp;
    UInt32 stream = 0;
    char buf[256];
	deq = ring->transferRingDequeueIdx;
	enq = ring->transferRingEnqueueIdx;
	siz = ring->transferRingSize;
    
    slot = ring->slotID;
    endp = ring->endpointID;
    
    if(IsStreamsEndpoint(slot, endp))
    {
        stream = (int)(ring - _slots[slot].rings[endp]);
    }
    
    istart = (int)(StartofFragment - ring->transferRing);
    if( (istart < 0) || (istart > siz) )
    {
        USBLog(1, "AppleUSBXHCI[%p]::PrintTRBs - start out of range %p (ring %p, %d), index: %d", this, StartofFragment, ring->transferRing, siz, istart);
        return;
    }
    end = (int)(endofFragment - ring->transferRing);
    if( (end < 0) || (end > siz) )
    {
        USBLog(1, "AppleUSBXHCI[%p]::PrintTRBs - end out of range %p (ring %p, %d), index: %d", this, endofFragment, ring->transferRing, siz, end);
        return;
    }
    USBLog(2, "AppleUSBXHCI[%p]::PrintTRBs - %s %d->%d (@%d, %d, st:%d)", this, s, istart, end, slot, endp, (int)stream);
    deq1 = deq-1;
    if(deq1 < 0)
    {
        deq1 = siz-1;
    }
    if(deq1 == enq)
    {
        USBLog(1, "AppleUSBXHCI[%p]::PrintTRBs - ring full, enq:%d deq:%d", this, enq, deq);
    }
    else
    {
        snprintf(buf, 256, "Idx-: %d, phys:%08lx", deq1, (long unsigned int)(ring->transferRingPhys + deq1*sizeof(TRB)));
        PrintTRB(5, &ring->transferRing[deq1], buf);
    }
    while(enq != deq)
    {
        char mark[]={" "};
        int nextDeq;
        nextDeq = deq+1;
        if(nextDeq >= siz)
        {
            nextDeq = 0;
        }
        if(deq == istart)
        {
            mark[0] = 'S';
        }
        else if (deq == end)
        {
            mark[0] = 'E';
        }
        else
        {
            mark[0] = ' ';
        }
        snprintf(buf, 256, "Idx%s: %d, phys:%08lx", mark, deq, (long unsigned int)(ring->transferRingPhys + deq*sizeof(TRB)));
        PrintTRB(5, &ring->transferRing[deq], buf);
        deq = nextDeq;
    }
}
#endif

void 
AppleUSBXHCI::CloseFragment(XHCIRing *ringX, TRB *StartofFragment, UInt32 offsC1)
{
    UInt32			offsC;
	
    if(StartofFragment != NULL)
    {		
		int index = (int)(StartofFragment - ringX->transferRing);

		// Reread the cycle bit for the start of fragment, it may have changed.
        offsC = (USBToHostLong(StartofFragment->offsC) & kXHCITRB_C) ^ kXHCITRB_C;	// ^ is XOR, flip the bit
        offsC1 &= ~kXHCITRB_C;  // strip the cycle bit from the same bits
        offsC1 |= offsC;        // Update the cycle bit.
        
		PrintTransferTRB(StartofFragment, ringX, index, offsC1);
        IOSync();
        StartofFragment->offsC = HostToUSBLong(offsC1);
        IOSync();
    }
    else
    {
        USBLog(1, "AppleUSBXHCI[%p]::CloseFragment - null StartofFragment", this);
    }
    
}



IOReturn 
AppleUSBXHCI::_createTransfer(void			*xhciTD, 
							  bool			isocTransfer, 
							  IOByteCount	transferSize, 
							  UInt32 		offsCOverride, 
							  UInt64		runningOffset, 
							  bool			interruptNeeded,
							  bool			fragmentedTDs,
							  UInt32		*firstTRBIndex,
							  UInt32		*numTRBs, 
							  bool			noLogging,							  
							  SInt16		*completionIndex)
{	
	TRB             *newTRB;
	TRB             *StartofFragmentTRB = NULL;
	Context			*epContext = NULL;
	UInt32			offsC = 0;
    UInt32          offsC1 = 0;
    UInt32          TDSize = 0;
	int				index, TRBsThisFragment = 0, TRBsThisTD=0;
	int				slotID;
	UInt32			endpointID, maxPacketSize, maxBurst, mult, MBP;
	IOByteCount		bytesThisTRB;
	IOReturn		err;
    XHCIRing        *ringX;
    IODMACommand    *pDMACommand;
	IOByteCount		remainingSize = 0;
	IOByteCount		startOffset = 0;

    bool            noOpTransfer = false;
    bool            immediateTransfer = false;
    UInt8           immediateBuffer[kMaxImmediateTRBTransferSize];
    bool            lastInRing = false;
	bool			lastTDFragment = false;
	
    if( isocTransfer )
    {
        AppleXHCIIsochTransferDescriptor *isocTD = (AppleXHCIIsochTransferDescriptor*)xhciTD;
        IOUSBIsocCommand                *pCommand= (IOUSBIsocCommand*)isocTD->command;
        ringX                                    = ((AppleXHCIIsochEndpoint*)isocTD->_pEndpoint)->ring;
        pDMACommand                              = pCommand->GetDMACommand();
    }
    else
    {
        AppleXHCIAsyncTransferDescriptor *asyncTD = (AppleXHCIAsyncTransferDescriptor*)xhciTD;
        IOUSBCommandPtr                  pCommand = (IOUSBCommandPtr)asyncTD->activeCommand;
        ringX                                     = asyncTD->_endpoint->_ring;
        // lastInRing                                = asyncTD->lastInRing;
        pDMACommand                               = pCommand->GetDMACommand();
        lastTDFragment							  = asyncTD->last;
		remainingSize						      = asyncTD->remAfterThisTD;
		startOffset								  = asyncTD->startOffset;

        UInt8 trbType = ((USBToHostLong(offsCOverride) & kXHCITRB_Type_Mask) >> kXHCITRB_Type_Shift);
        if (trbType == kXHCITRB_TrNoOp || trbType == kXHCITRB_Status)
        {
            noOpTransfer = true;
        }
        
        if (asyncTD->immediateTransfer)
        {
            immediateTransfer                     = true;
            bcopy(asyncTD->immediateBuffer, immediateBuffer, kMaxImmediateTRBTransferSize);
        }
    }
	
	//int countSegs = 0;

    slotID			= ringX->slotID;
    endpointID		= ringX->endpointID;
	epContext       = GetEndpointContext(slotID, endpointID);
	maxPacketSize   = GetEpCtxMPS(epContext);
	maxBurst        = GetEPCtxMaxBurst(epContext) + 1;
	mult            = GetEPCtxMult(epContext) + 1;
    
    // Max Burst Payload. Intermediate fragments should be multiple of this
    MBP = maxPacketSize * maxBurst * mult;
	
    bool firstFragment = true;  // If we close out a fragment, set this to false.
#if XHCI_USE_KPRINTF == 0
	if (!noLogging)
#endif
	{
	    // USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - fragmentedTDs: %d immediateTransfer: %d req:%d (@:%d, %d) maxP:%d maxB:%d noOpTransfer:%d", this, fragmentedTDs, immediateTransfer, (int)transferSize, slotID, (int)endpointID, (int)maxPacketSize, (int)maxBurst, (int)noOpTransfer);
	}
#define DEBUG_CDBS (0)
#if DEBUG_CDBS
    if( ((req == 31) || (req == 32)) && ((endpointID & 1) == 0) )
    {
        UInt8		buf[32];
        UInt64		len;
		
        len = dmaCommand->readBytes(0, buf, req);
        if (len == req)
        {
            char data[31*3];
            char hex[]="0123456789ABCDEF";
            for(int i = 0; i<31; i++)
            {
                data[i*3] = hex[buf[i] >> 4];
                data[i*3+1] = hex[buf[i] & 0xf];
                data[i*3+2] = ' ';
            }
            data[(req-1)*3+2] = 0;
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - %d bytes: %s", this, (int)req, data);
			}
        }
        else
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - readbytes from %d byte transfer (%d)", this, (int)req, (int)len);
			}
    }
#endif
#if XHCI_USE_KPRINTF == 0
	if (!noLogging)
#endif
    {
		USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - req:%d (@:%d, %d) maxP:%d maxB:%d", this, (int)transferSize, slotID, (int)endpointID, (int)maxPacketSize, (int)maxBurst);
    }
	
	if ( maxPacketSize == 0 )
	{
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - maxPacketSize is 0, bailing", this);
		}
		return kIOReturnBadArgument;
	}
	
	do 
    {
		newTRB = GetNextTRB(ringX, NULL, &StartofFragmentTRB, (firstFragment && (startOffset == 0)));
        
		if (newTRB == NULL)
		{
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - could not get TRB1", this);
			}
            PutBackTRB(ringX, StartofFragmentTRB);
			return(kIOReturnInternalError);
		}
		
        TRBsThisFragment++;
        TRBsThisTD++;
        
		offsC = (USBToHostLong(newTRB->offsC) & kXHCITRB_C) ^ kXHCITRB_C;	// ^ is XOR, flip the bit
		
		bytesThisTRB = transferSize;
		
        if (!immediateTransfer)
        {
            err = GenerateNextPhysicalSegment(newTRB, &bytesThisTRB, (UInt32)runningOffset, pDMACommand);
            
            if (err != kIOReturnSuccess)
            {
#if XHCI_USE_KPRINTF == 0
				if (!noLogging)
#endif
				{
                	USBLog(2, "AppleUSBXHCI[%p]::_createTransfer - GenerateNextPhysicalSegment returned 0x%08x", this, err);
                }
                PutBackTRB(ringX, newTRB);
                return(err);
            }
        }
        else
        {
            // Immediate Transfer
            bcopy(immediateBuffer, newTRB, transferSize);
        }

        // First 3 DWords all zero
        if (transferSize == 0)
        {
            newTRB->offs0 = 0; newTRB->offs4 = 0; newTRB->offs8 = 0;
        }
                
        runningOffset   += bytesThisTRB;
        transferSize    -= bytesThisTRB;
        
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - TRB:%p, req remain: %d, offset: %d, bytesThisTRB: %d", this, newTRB, (int)transferSize, (int)runningOffset, (int)bytesThisTRB);
		}
		
		// Req is now the bytes to be transferred after this TRB, so calculate TDSize.
		TDSize = (UInt32)((transferSize + remainingSize + maxPacketSize -1) / maxPacketSize);
		if (TDSize > 31)
		{
			TDSize = 31;
		}
		
        // Last TRB of the TD, so setting Evaluate Next TRB
        if ((transferSize == 0) && (!immediateTransfer) && lastTDFragment && (TRBsThisTD > 1))
        {   
            offsC |= kXHCITRB_ENT;  
        }
        // else         // ENT zero
		
        // Set Interruptor target to kTransferInterrupter
        newTRB->offs8 = HostToUSBLong(kTransferInterrupter << kXHCITRB_InterrupterTarget_Shift);

        if (!noOpTransfer)
        {
			newTRB->offs8 |= HostToUSBLong( bytesThisTRB | (TDSize << kXHCITRB_TDSize_Shift) );
        }

        // Set the TRB Type, Format the 4th DWord of the TRB.
        if (offsCOverride == 0)
        {
            // Format the 4th DWord of the TRB.
            offsC |= (kXHCITRB_Normal << kXHCITRB_Type_Shift);
        }
        else
        {
            offsC 			|= offsCOverride;
            offsCOverride	= 0;
        }
		// NS zero
		// IDT zero
		
        if (!immediateTransfer)
        {
            // TRB is chained to next one.
            offsC |= kXHCITRB_CH;	
        }
		
		// ISP zero
		// BEI zero
		// IOC zero
        
        index = (int)(newTRB - ringX->transferRing);

#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - TRB: %p, offsC:%lx", this, newTRB, (unsigned long int)offsC);
		}
		//PrintTRB(t, "_createTransfer1");
		
        if (StartofFragmentTRB == newTRB)
        {
            offsC1 = offsC;  // Remember this for later
        }
        else
        {
			PrintTransferTRB(newTRB, ringX, index, offsC);
			
            newTRB->offsC = HostToUSBLong(offsC);		// HC now owns this TRB
		}
        
        if (!isocTransfer && (runningOffset > 0) && (transferSize > 0) && ((runningOffset % MBP) == 0))
        {
            // current offset into request is a multiple of the max burst payload
            // This is a suitable place to close a fragment
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - closing fragment %p with %x at %d (num TRBs:%d)", this, StartofFragmentTRB, (unsigned)offsC1, (int)runningOffset, TRBsThisFragment);
			}
            CloseFragment(ringX, StartofFragmentTRB, offsC1);
            firstFragment = false;
#if DEBUG_FRAGMENTS
            PrintTRBs(ringX, "mid", StartofFragmentTRB, newTRB);
#endif
            StartofFragmentTRB = NULL;
            TRBsThisFragment = 0;
        }
        
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			//		USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - TRB: %p", this, t);
			//		PrintTRB(t, "_createTransfer1");
			//countSegs++;
		}
		
	} while(transferSize > 0);
	
#if XHCI_USE_KPRINTF == 0
	if (!noLogging)
#endif
	{
		//	USBLog(2, "AppleUSBXHCI[%p]::_createTransfer - segs: %d", this, countSegs);
	}
        
    if (TRBsThisTD == 1)
    {
        XHCIRing *ring0 = GetRing(slotID, endpointID, 0);
        
        if (ring0 != NULL)
        {
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				//USBLog(2, "AppleUSBXHCI[%p]::_createTransfer - setting command on single TRB TD. @%d, %d, St:%d, index:%d C:%p", this, slotID, (int)endpointID, (int)(ringX-ring0), index, command);
			}

            // This TD only has 1 TRB, so close it without an event data TRB
            if (interruptNeeded)
            {
                offsC1 |= kXHCITRB_IOC | kXHCITRB_ISP;	// Need to know about short packets,
            }
            else
            {
                if (endpointID & 1)
                {   // IN endpoint
                    offsC1 |= kXHCITRB_BEI | kXHCITRB_ISP;	// Need to know about short packets, but don't interrupt us
                }
            }
            
            offsC1 &= ~(kXHCITRB_CH | kXHCITRB_ENT);
            
            if (index == (ringX->transferRingSize-2))
            {
                // This is the last TRB on the ring, so the link TRB will have been chained
#if XHCI_USE_KPRINTF == 0
				if (!noLogging)
#endif
				{
					//USBLog(3, "AppleUSBXHCI[%p]::_createTransfer - unsetting chain on link ind:%d (of %d)", this, index, ringX->transferRingSize);
				}
                SetTRBChainBit(&ringX->transferRing[ringX->transferRingSize-1], false);
				PrintTransferTRB(&ringX->transferRing[ringX->transferRingSize-1], ringX, ringX->transferRingSize-1);
            }
        }
        else
        {
#if XHCI_USE_KPRINTF == 0
			if (!noLogging)
#endif
			{
				// USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - couldn't find stream (%p, %p)", this, ringX, ring0);
			}
        }
    }
    else
    {
		// TODO: I don't actually need to do this part (the EventData TRB) for Isoc OUT..

		if (fragmentedTDs && !lastTDFragment)
		{
			newTRB = GetNextTRB(ringX, NULL, &StartofFragmentTRB, (firstFragment && (startOffset == 0)));
		}
		else
		{
			newTRB = GetNextTRB(ringX, xhciTD, &StartofFragmentTRB, (firstFragment && (startOffset == 0)));
		}
        
        if(newTRB == NULL)
        {
            // kprintf("AppleUSBXHCI[%p]::_createTransfer - could not get TRB2\n", this);
            PutBackTRB(ringX, StartofFragmentTRB);
            return(kIOReturnInternalError);
        }
        
        TRBsThisTD++;
        index = (int)(newTRB - ringX->transferRing);
        
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			//	USBLog(1, "AppleUSBXHCI[%p]::_createTransfer - Event Data TRB index is %d @ %p", this, index, t);
		}
        SetTRBAddr64(newTRB, ringX->transferRingPhys + index * sizeof(TRB));	// Set user data as phys address of TRB
        
        USBTrace( kUSBTXHCI, kTPXHCIUIMCreateTransfer,  (uintptr_t)this, 4, index, (int)(ringX->transferRingPhys + index * sizeof(TRB)));
		
        
        newTRB->offs8 = HostToUSBLong(kTransferInterrupter << kXHCITRB_InterrupterTarget_Shift);
        
        // ^ is XOR, flip the bit
        offsC = (USBToHostLong(newTRB->offsC) & kXHCITRB_C) ^ kXHCITRB_C;	
        
        // Format the 4th DWord of the TRB.
        offsC |= (kXHCITRB_EventData << kXHCITRB_Type_Shift);
        
        // ENT zero
        // CH zero

        // Don't set chain bit for Event Data TRBs for fragmented TDs
        offsC &= ~(kXHCITRB_CH | kXHCITRB_ENT);
        
		// When we have multiple fragments, we need to chain the Event Data TRBs except the
		// last one
		if (fragmentedTDs && !lastTDFragment)
		{
			offsC |= kXHCITRB_CH;
		}
		
        if (interruptNeeded)
        {
            offsC |= kXHCITRB_IOC;	// We want an interrupt
            // BEI zero
        }
        else
        {
            if(endpointID & 1)
            {   // IN endpoint
                offsC |= kXHCITRB_BEI | kXHCITRB_IOC;	// Need to know about short packets, but don't interrupt us
            }
        }
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
	        USBLog(7, "AppleUSBXHCI[%p]::_createTransfer - Event Data TRB index is %4d @ %p, TD: %p offsC:%lx", this, index, newTRB, xhciTD, (unsigned long int)offsC);
		}
        //PrintTRB(t, "_createTransfer2");
		
		PrintTransferTRB(newTRB, ringX, index, offsC);
        newTRB->offsC = HostToUSBLong(offsC);		// HC now owns this TRB
        //PrintTRB(t, "_createTransfer2");
    }
	
    if (TRBsThisFragment != 0)
    {
#if XHCI_USE_KPRINTF == 0
		if (!noLogging)
#endif
		{
			// USBLog(2, "AppleUSBXHCI[%p]::_createTransfer - closing last fragment %p with %x at %d (num TRBs:%d)", this, StartofFragment, (unsigned)offsC1, (int)runningOffset, TRBsThisFragment);
		}
		
        CloseFragment(ringX, StartofFragmentTRB, offsC1);
        
        // firstFragment = false;
#if DEBUG_FRAGMENTS
        PrintTRBs(ringX, "end", StartofFragmentTRB, newTRB);
#endif
    }
	
	// these are for Isoc for now
	if (firstTRBIndex)
    {
		*firstTRBIndex = (int)(StartofFragmentTRB - ringX->transferRing);
    }

    if (completionIndex)
    {
        *completionIndex = index;
    }

    if (numTRBs)
    {
		*numTRBs = TRBsThisTD;
    }
	
	return(kIOReturnSuccess);
}


//================================================================================================
//
//   CreateTransfer 
//
//      Called from UIMCreateBulkTransfer and UIMCreateInterruptTransfer
//
//================================================================================================
//
IOReturn 
AppleUSBXHCI::CreateTransfer(IOUSBCommand* command, UInt32 stream)
{
	IOReturn		status;
	IOByteCount		req = command->GetReqCount();
	
	// ********** Check this works with zero length buffer ************
	int                     slotID;
	UInt32                  endpointIdx;
	XHCIRing                *ringX;
    AppleXHCIAsyncEndpoint  *pAsyncEP;
	int 					epState;
   
	IOReturn    err = kIOReturnInternalError;
	short       direction;
	
	slotID = GetSlotID(command->GetAddress());
	
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateTransfer - unallocated slot ID: fn %d, returning kIOUSBEndpointNotFound", this, command->GetAddress());
		return(kIOUSBEndpointNotFound);
	}
	
	direction = command->GetDirection();
	endpointIdx = GetEndpointID(command->GetEndpoint(), direction);
	
    if(stream != 0)
    {
        //USBLog(2, "AppleUSBXHCI[%p]::CreateTransfer - command: %p, slot: %d, epIndex:%d, maxPacketSize %d, req: %d", this,
        //       command, slotID, (int)endpointIdx,(int) maxPacketSize, (int)req);
	}
    
	ringX = GetRing(slotID, endpointIdx, stream);
	if(ringX == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateTransfer - ring does not exist (slot:%d, ep:%d, str:%d) ", this, (int)slotID, (int)endpointIdx, (int)stream);
		return(kIOReturnBadArgument);
	}
	if(ringX->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateTransfer - ******* unallocated ring: slotID: %d, ring %d", this, slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
    if(_slots[slotID].deviceNeedsReset)
    {
        return(AddDummyCommand(ringX, command));
    }
	if(ringX->beingDeleted)
    {
        USBLog(2, "AppleUSBXHCI[%p]::CreateTransfer - Endpoint being deleted while new transfer is queued, returning kIOReturnNoDevice (%x)", this, kIOReturnNoDevice);
		return(kIOReturnNoDevice);
    }
    if(ringX->beingReturned)
    {
        USBLog(2, "AppleUSBXHCI[%p]::CreateTransfer - Endpoint being cleared while new transfer is queued", this);
    }
	
    pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ringX->pEndpoint);
    
    if ( pAsyncEP == NULL )
    {
        USBLog(1, "AppleUSBXHCI[%p]::CreateTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if ( pAsyncEP->_aborting )
    {
        USBLog(3, "AppleUSBXHCI[%p]::CreateTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }
		
	// The UIM should return an immediate error if the endpoint is halted/eror
	epState = GetEpCtxEpState(GetEndpointContext(slotID, endpointIdx));

	if ((epState == kXHCIEpCtx_State_Halted) || (epState == kXHCIEpCtx_State_Error))
	{
		USBLog(3, "AppleUSBXHCI[%p]::CreateTransfer - sent request while endpoint is Halted/Error (%d), returning kIOUSBPipeStalled", this, epState);
		return kIOUSBPipeStalled;        
	}

	USBLog(7, "AppleUSBXHCI[%p]::CreateTransfer - Endpoint %d - TRBBuffer: %p, transferRing: %p, transferRingPhys: %p, transferRingSize: %d", this, (int)endpointIdx, ringX->TRBBuffer, ringX->transferRing, (void *)ringX->transferRingPhys, (int)ringX->transferRingSize);
	USBLog(7, "AppleUSBXHCI[%p]::CreateTransfer - transferRingPCS: %d, transferRingEnqueueIdx: %d, transferRingDequeueIdx:%d req:%d", this, (int)ringX->transferRingPCS, (int)ringX->transferRingEnqueueIdx, (int)ringX->transferRingDequeueIdx, (int)req);
    USBTrace_Start( kUSBTXHCI, kTPXHCIUIMCreateTransfer,  (uintptr_t)this, slotID, endpointIdx, req );

    //
    // CreateTDs will soft queue the command request into fragments and will schedule the fragments to the ring
    //
    status = pAsyncEP->CreateTDs(command, stream);
    
    //
    // Add requests to the schedule
    pAsyncEP->ScheduleTDs();
    
    USBTrace_End( kUSBTXHCI, kTPXHCIUIMCreateTransfer,  (uintptr_t)this, slotID, endpointIdx, 0 );

	return(status);
}

IOReturn AppleUSBXHCI::AddDummyCommand(XHCIRing *ringX, IOUSBCommand *command)
{
    IOReturn status;
    UInt32   offsCOverrride;
    
    offsCOverrride = 0;
    UInt8 immediateTransferSize = 0;
    UInt8 immediateBuffer[kMaxImmediateTRBTransferSize];
    
    bzero(immediateBuffer, kMaxImmediateTRBTransferSize);
    
    USBLog(1, "AppleUSBXHCI[%p]::AddDummyCommand - Device needs to be reset", this);
    
    offsCOverrride |= kXHCITRB_IOC;				// We want an interrupt
    offsCOverrride |= (kXHCITRB_TrNoOp << kXHCITRB_Type_Shift);

    AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ringX->pEndpoint);
    
    if (pAsyncEP == NULL)
    {
        USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if (pAsyncEP->_aborting)
    {
        USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }
    
    //
    // CreateTDs will soft queue the command request into fragments and will schedule the fragments to the ring
    status = pAsyncEP->CreateTDs(command, kNoStreamID, offsCOverrride, immediateTransferSize, immediateBuffer);
    
    //
    // Add requests to the schedule
    pAsyncEP->ScheduleTDs();
    
	return(status);
}

bool AppleUSBXHCI::IsStillConnectedAndEnabled(int SlotID)
{
    int     port;
    UInt32  portSC;
	bool	status = false;
	
	Context *	context = GetSlotContext(SlotID);
	
	if (_lostRegisterAccess)
	{
		return false;
	}
    
	do {
		if (!context)
		{
			break;
		}
		
		port = GetSlCtxRootHubPort(context);
		
		portSC = Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC);
		if (_lostRegisterAccess)
		{
			break;
		}
		//USBLog(7, "AppleUSBXHCI[%p]::IsStillConnectedAndEnabled - Port:%d, PortSC:%x, ?:%d", this, port, (unsigned)portSC, (portSC & (kXHCIPortSC_CCS|kXHCIPortSC_PED) ) == (kXHCIPortSC_CCS|kXHCIPortSC_PED));
		status = ((portSC & (kXHCIPortSC_CCS|kXHCIPortSC_PED)) == (kXHCIPortSC_CCS|kXHCIPortSC_PED));

		
	} while(0);

	return status;
}



#define kUSBSetup			kUSBNone

//same method in 1.8.2
IOReturn 
AppleUSBXHCI::UIMCreateControlTransfer(	short					functionNumber,
										short					endpointNumber,
										IOUSBCommand*			command,
										IOMemoryDescriptor*		CBP,
										bool					bufferRounding,
										UInt32                  bufferSize,
										short					direction)
{
#pragma unused(bufferRounding)
	int      slotID;
	UInt32   endpointIdx;
    UInt32   offsCOverrride;
	XHCIRing *ringX;
	IOReturn status = kIOReturnSuccess;
    
	USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - command: %p (%d, %d)", this, command, functionNumber, endpointNumber);
    
	slotID = GetSlotID(functionNumber);
	
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - unallocated slot ID: fn %d, returning kIOUSBEndpointNotFound", this, functionNumber);
		return(kIOUSBEndpointNotFound);
	}
    
	endpointIdx = 2*endpointNumber+1;
	
    ringX       = GetRing(slotID, endpointIdx, 0);
    
	if  (ringX == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
    
	if (ringX->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - ******* unallocated ring, slot:%d id:%d", this, slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
	
    if(_slots[slotID].deviceNeedsReset)
    {
        return(AddDummyCommand(ringX, command));
    }
    
	if(ringX->beingDeleted)
    {
        USBLog(2, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - Endpoint being deleted while new transfer is queued, returning kIOReturnNoDevice (%x)", this, kIOReturnNoDevice);
		return(kIOReturnNoDevice);
    }
	
	if (functionNumber == 0)
	{
		//PrintContext(&_slots[slotID].deviceContext[0]);
		//PrintContext(&_slots[slotID].deviceContext[endpointIdx]);
	}
	
    AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ringX->pEndpoint);
    
    if (pAsyncEP == NULL)
    {
        USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if (pAsyncEP->_aborting)
    {
        USBLog(3, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }
    
#if 0
	{
		Context *epCtx;
		int epState;
		epCtx = GetEndpointContext(slot, endpointIdx);
		epState = GetEpCtxEpState(epCtx);
		USBLog(6, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - EP State1: %d", this, epState);
		PrintContext(GetSlotContext(slot));
		PrintContext(epCtx);
		//PrintRuntimeRegs();
	}
#endif
	
    IODMACommand *dmaCommand = command->GetDMACommand();
	
	if (CBP && bufferSize)
	{
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - no dmaCommand", this);
			return kIOReturnNoMemory;
		}
		if (dmaCommand->getMemoryDescriptor() != CBP)
		{
			USBError(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - mismatched CBP (%p) and dmaCommand memory descriptor (%p)", this, CBP, dmaCommand->getMemoryDescriptor());
			return kIOReturnInternalError;
		}
	}
	
    offsCOverrride = 0;
    UInt8 immediateTransferSize = 0;
    UInt8 immediateBuffer[kMaxImmediateTRBTransferSize];
    
    bzero(immediateBuffer, kMaxImmediateTRBTransferSize);

	if(direction == kUSBSetup)
	{		
		int    bytesRead;
		UInt32 TRT;
        
        IOUSBDevRequest request;
        
        
		if(bufferSize != kMaxImmediateTRBTransferSize)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - SETUP not 8 bytes : %d", this, (int)bufferSize);
			return(kIOReturnBadArgument);
		}
		
		// Move bytes to immediate buffer, also gets the bmReqType and bRequest, to see if its a Set Address.
		bytesRead = (int)CBP->readBytes(0, &request, kMaxImmediateTRBTransferSize);
		
		if(bytesRead != kMaxImmediateTRBTransferSize)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - could not get 8 bytes of setup : %d", this, bytesRead);
			return(kIOReturnInternalError);
		}

        USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer : bmRequestType = 0x%2x bRequest = 0x%2x wValue = 0x%4x wIndex = 0x%4x wLength = 0x%4x", 
                                                        this, request.bmRequestType, request.bRequest, request.wValue, request.wIndex, request.wLength);
        
        UInt16 theRequest = (request.bRequest << 8) | request.bmRequestType;
        
        if (theRequest == kSetAddress) 
		{	
            // These bytes seem to be swapped, and I don't know why.
			UInt16  maxPacketSize;
			UInt32  newAddress;
			UInt8   speed;
			int     highSpeedHubSlotID;
			int     highSpeedPort;
			Context * epContext;
            Context * slotContext;
			
			newAddress          = request.wValue;
			
			epContext           = GetEndpointContext(slotID, 1);
			maxPacketSize       = GetEpCtxMPS(epContext);
			
			slotContext         = GetSlotContext(slotID);
			speed               = GetSlCtxSpeed(slotContext);
			highSpeedPort       = GetSlCtxTTPort(slotContext);
			highSpeedHubSlotID  = GetSlCtxTTSlot(slotContext);
			
			_fakedSetaddress = true;
			status = AddressDevice(slotID, maxPacketSize, true, speed, highSpeedHubSlotID, highSpeedPort);
			if (status != kIOReturnSuccess)
			{
				USBLog(2, "AppleUSBXHCI[%p]::UIMCreateControlTransfer  AddressDevice returned 0x%x, bailing...", this, (uint32_t) status);
				return status;
			}
			
			USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - kUSBSetup: kUSBRqSetAddress err:%x (maxpacket: %d, newAddress: % d) slotID: %d", this, status, (int) maxPacketSize, (int)newAddress, (int)slotID);
			
			_devHub[newAddress] = _devZeroHub;
			_devPort[newAddress] = _devZeroPort;
			_devMapping[newAddress] = slotID;
			_devEnabled[newAddress] = true;
            
			_devZeroHub = 0;
			_devZeroPort = 0;
			
			// Now insert TRB as no-op, to get out of thread callback
			// ENT zero
			// CH zero
			offsCOverrride |= kXHCITRB_IOC;				// We want an interrupt
			offsCOverrride |= (kXHCITRB_TrNoOp << kXHCITRB_Type_Shift);
		}
		else
		{
            USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - kUSBSetup: %x err:%x slotID: %d", this, request.bRequest, status, (int)slotID);
   
			// Set the transfer len to 8, and the Interruptor target to zero
			// t->offs8 = HostToUSBLong(8);
			
			// Format the 4th DWord of the TRB.
			offsCOverrride |= kXHCITRB_IOC;				// We want an interrupt
			offsCOverrride |= kXHCITRB_IDT;				// All SETUPs are immediate
			offsCOverrride |= (kXHCITRB_Setup << kXHCITRB_Type_Shift);
			
            // TRB neesd to know TRT, transfer type
            // wlength == 0
			if ( request.wLength == 0)	
			{
                // No data stage
				TRT = kXHCI_TRT_NoData;	
			}
			else if ( (request.bmRequestType & 0x80) != 0)  // bmRequestType is In	 	
			{
				// IN	
				TRT = kXHCI_TRT_InData;
			}
			else 
			{
				// OUT
				TRT = kXHCI_TRT_OutData;
			}
            
			offsCOverrride |= TRT << kXHCITRB_TRT_Shift;
            immediateTransferSize = 8;
            bcopy(&request, &immediateBuffer, kMaxImmediateTRBTransferSize);
		}
        
		//USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 2 - setting setup TRB: %08lx", this, (long unsigned int)offsC);
	}
	else if(bufferSize > 0)
	{
		// Assume a data phase
    
        // Format the offsC for the first TRB, Setup/Data TRB type and Dir flags
        // This will be added to the offsC for the first TRB only
		offsCOverrride |= (kXHCITRB_Data << kXHCITRB_Type_Shift);
        
        // Direction IN, else OUT
		if(direction == kUSBIn)
		{
			offsCOverrride |= kXHCITRB_DIR;				
		}
        
        //
        // We do this, so CreateTDs will pull the buffer from the command
        immediateTransferSize = kInvalidImmediateTRBTransferSize;
        USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - Endpoint %d - ring %p TRBBuffer: %p, transferRing: %p, transferRingPhys: %p, transferRingSize: %d", this, (int)endpointIdx, ringX, ringX->TRBBuffer, ringX->transferRing, (void *)ringX->transferRingPhys, (int)ringX->transferRingSize);
        USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - transferRingPCS: %d, transferRingEnqueueIdx: %d, transferRingDequeueIdx:%d req:%d", this, (int)ringX->transferRingPCS, (int)ringX->transferRingEnqueueIdx, (int)ringX->transferRingDequeueIdx, (int)bufferSize);
	}
	else
	{
        offsCOverrride |= kXHCITRB_IOC;				// We want an interrupt

		if(_fakedSetaddress)
		{
            
			USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - Set Address complete status phase (delay)", this);
			
			_devMapping[0] = 0;
			_devEnabled[0] = false;
            
			_fakedSetaddress = false;	
            
			// Now insert TRB as no-op for completion
			// ENT zero
			// CH zero
			offsCOverrride |= (kXHCITRB_TrNoOp << kXHCITRB_Type_Shift);
		}
		else
		{
			// Assume a status
			
			// First 3 DWords all zero
			// ENT zero
			// CH zero
			offsCOverrride |= (kXHCITRB_Status << kXHCITRB_Type_Shift);
			if(direction == kUSBIn)
			{
				offsCOverrride |= kXHCITRB_DIR;				// Direction IN, else OUT
			}
		}
        
		USBLog(7, "AppleUSBXHCI[%p]::UIMCreateControlTransfer - setting status TRB: %08lx", this, (long unsigned int)offsCOverrride);
		//PrintTRB(t, "UIMCreateControlTransfer2");
	}

    if (status == kIOReturnSuccess)
    {
        //
        // CreateTDs will soft queue the command request into fragments and will schedule the fragments to the ring
        status = pAsyncEP->CreateTDs(command, kNoStreamID, offsCOverrride, immediateTransferSize, immediateBuffer);
        
        //
        // Add requests to the schedule
        pAsyncEP->ScheduleTDs();
    }
	
	return (status);
}



// method in 1.8 and 1.8.1
IOReturn AppleUSBXHCI::UIMCreateControlTransfer(short					functionNumber,
												short					endpointNumber,
												IOUSBCompletion		completion,
												IOMemoryDescriptor	*CBP,
												bool					bufferRounding,
												UInt32				bufferSize,
												short					direction)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(completion)
#pragma unused(CBP)
#pragma unused(bufferRounding)
#pragma unused(bufferSize)
#pragma unused(direction)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 3 - Obsolete method called", this);
    return(kIOReturnInternalError);
}




IOReturn AppleUSBXHCI::UIMCreateControlTransfer(short				functionNumber,
												short				endpointNumber,
												IOUSBCommand		*command,
												void				*CBP,
												bool				bufferRounding,
												UInt32			bufferSize,
												short				direction)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(command)
#pragma unused(CBP)
#pragma unused(bufferRounding)
#pragma unused(bufferSize)
#pragma unused(direction)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateControlTransfer 4 - Obsolete method called", this);
    return(kIOReturnInternalError);
}





#pragma mark Bulk
IOReturn AppleUSBXHCI::UIMCreateBulkEndpoint(UInt8				functionNumber,
											 UInt8				endpointNumber,
											 UInt8				direction,
											 UInt8				speed,
											 UInt8				maxPacketSize)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(speed)
#pragma unused(maxPacketSize)
#pragma unused(direction)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateBulkEndpoint 1 - Obsolete method called", this);
    return(kIOReturnInternalError);
}


IOReturn AppleUSBXHCI::CreateBulkEndpoint(UInt8				functionNumber,
                                          UInt8				endpointNumber,
                                          UInt8				direction,
                                          UInt8				speed,
                                          UInt16            maxPacketSize,
                                          USBDeviceAddress  highSpeedHub,
                                          int               highSpeedPort,
                                          UInt32            maxStream,
                                          UInt32            maxBurst)							// this is the 0 based maxBurst
{
#pragma unused(speed)
#pragma unused(highSpeedHub)
#pragma unused(highSpeedPort)
	
    USBLog(3, "AppleUSBXHCI[%p]::UIMCreateBulkEndpoint 2 (%d, %d, %d, %d, %d, %d, %d, %d, %d)", this, functionNumber, endpointNumber, direction, speed, maxPacketSize, (int)highSpeedHub, highSpeedPort, (int)maxStream, (int)maxBurst);

	int slotID, endpointIdx, epType;
	IOReturn err;
	
	slotID = GetSlotID(functionNumber);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateBulkEndpoint 2 - Unused slot ID for functionAddress: %d", this, functionNumber);
		return(kIOReturnInternalError);
	}
	epType = kXHCIEpCtx_EPType_BulkOut;
	endpointIdx = GetEndpointID(endpointNumber, direction);
	if(direction == kUSBIn)
	{
		epType = kXHCIEpCtx_EPType_BulkIN;
	}
	
	err = CreateEndpoint(slotID, endpointIdx, maxPacketSize, 0, epType, maxStream, maxBurst, 0, NULL);
	if(err != kIOReturnSuccess)
	{
		return(err);
	}
    return(err);
}



IOReturn AppleUSBXHCI::UIMCreateStreams(  UInt8				functionNumber,
                                          UInt8				endpointNumber,
                                          UInt8				direction,
                                          UInt32            maxStream)
{
 	USBLog(3, "AppleUSBXHCI[%p]::UIMCreateStreams %d (@%d, %d, %d)", this, (int)maxStream, (int)functionNumber, (int)endpointNumber, (int)direction);
	
	
	int             slotID, endpointIdx;
	IOReturn        err;
    UInt32          maxPossibleStream;
	
    if (_maxPrimaryStreams == 0)
    {
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - HC does not support streams", this);
        return kIOUSBStreamsNotSupported;
    }
    
	slotID = GetSlotID(functionNumber);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - Unused slot ID for functionAddress: %d", this, functionNumber);
		return kIOReturnInternalError;
	}

	endpointIdx = GetEndpointID(endpointNumber, direction);
    if(_slots[slotID].maxStream[endpointIdx] > 0)
    {
        if(maxStream == 0)
        {
            // Deallocate streams
            USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - Deallocate streams not implimented yet", this);
            return kIOReturnInternalError;
        }
 		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - Endpoint already has streams %d", this, (int)_slots[slotID].maxStream[endpointIdx]);
		return kIOReturnBadArgument;
    }

    maxPossibleStream = _slots[slotID].potentialStreams[endpointIdx];
    if(maxPossibleStream <= 1)
    {
 		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - Not a streams XHCIRing (%d, %d)", this, (int)slotID, (int)endpointIdx);
		return kIOReturnBadArgument;
    }
    _slots[slotID].maxStream[endpointIdx] = maxStream;
	if(maxStream > 1)
	{
		for(UInt32 i = 1; i<=maxStream; i++)
		{
			err = CreateStream(slotID, endpointIdx, i);
			if(err != kIOReturnSuccess)
			{
                _slots[slotID].maxStream[endpointIdx] = 0;
				return err;
			}
		}
	}
    else
    {
 		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateStreams - bad max streams %d", this, (int)maxStream);
		return kIOReturnBadArgument;
    }
	return err;
}



IOReturn AppleUSBXHCI::UIMCreateSSBulkEndpoint(
                                               UInt8		functionNumber,
                                               UInt8		endpointNumber,
                                               UInt8		direction,
                                               UInt8		speed,
                                               UInt16		maxPacketSize,
                                               UInt32       maxStream,
                                               UInt32       maxBurst)
{
    if ((maxStream > 0) && (_maxPrimaryStreams == 0))
    {
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateSSBulkEndpoint - Streams not supported: %d", this, (int)maxStream);
		return kIOUSBStreamsNotSupported;
    }
    
	if(maxStream > (kMaxStreamsPerEndpoint-1))
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateSSBulkEndpoint - Bad max streams param: %d", this, (int)maxStream);
		return kIOReturnBadArgument;
	}
	
	if(maxStream >= 65533)
	{
		if(maxStream != 65533)
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateSSBulkEndpoint - Bad Max streams %d (fn:%d, ep:%d) ", this, (int)maxStream, (int)functionNumber, (int)endpointNumber);
			return kIOReturnBadArgument;
		}
		else
		{
			USBLog(3, "AppleUSBXHCI[%p]::UIMCreateSSBulkEndpoint - Maximal streams %d (fn:%d, ep:%d) and we support it!", this, (int)maxStream, (int)functionNumber, (int)endpointNumber);
		}
	}
	if( (maxStream & (maxStream+1)) != 0)	// Test for power of 2 minus 1
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateSSBulkEndpoint - Bad Max streams %d not 2^x-1(fn:%d, ep:%d) ", this, (int)maxStream, (int)functionNumber, (int)endpointNumber);
		return kIOReturnBadArgument;
	}
    
    return  CreateBulkEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize, 0, 0, maxStream, maxBurst);
}



IOReturn AppleUSBXHCI::UIMCreateBulkEndpoint(UInt8				functionNumber,
											 UInt8				endpointNumber,
											 UInt8				direction,
											 UInt8				speed,
											 UInt16				maxPacketSize,
											 USBDeviceAddress    	highSpeedHub,
											 int					highSpeedPort)
{
	USBLog(3, "AppleUSBXHCI[%p]::UIMCreateBulkEndpoint 2", this);
    return CreateBulkEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize, highSpeedHub, highSpeedPort, 0, 0);
}






// method in 1.8 and 1.8.1
IOReturn AppleUSBXHCI::UIMCreateBulkTransfer(short				functionNumber,
											 short				endpointNumber,
											 IOUSBCompletion		completion,
											 IOMemoryDescriptor   *CBP,
											 bool					bufferRounding,
											 UInt32				bufferSize,
											 short				direction)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(completion)
#pragma unused(CBP)
#pragma unused(direction)
#pragma unused(bufferRounding)
#pragma unused(bufferSize)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateBulkTransfer 1- Obsolete method called", this);
    return(kIOReturnInternalError);
}




// same method in 1.8.2
IOReturn AppleUSBXHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
	IOReturn err;
	UInt32 stream;
#if DEBUG_BUFFER
	IOMemoryMap *mmap;
	void *mbuf;
	USBLog(2, "AppleUSBXHCI[%p]::UIMCreateBulkTransfer 2 - Req: %d", this, (int)command->GetReqCount());
    
    if (_lostRegisterAccess)
    {
        return kIOReturnNoDevice;
    }
	
	if(command->GetDirection() == kUSBIn)
	{
		mmap = command->GetBuffer()->map();
		command->SetUIMScratch(kXHCI_ScratchMMap, (UInt64)mmap);	// good for 32bit mode debugging.
		mbuf = (void *)mmap->getVirtualAddress();
		mset_pattern4((UInt32 *)mbuf, _debugPattern, command->GetReqCount());
		command->SetUIMScratch(kXHCI_ScratchPat, _debugPattern);
		_debugPattern = ~_debugPattern;
		
		command->SetUIMScratch(kXHCI_ScratchMark,1);	// Mark as mapped and marked
	}
#endif
	
    USBLog(7, "AppleUSBXHCI[%p]::UIMCreateBulkTransfer - addr=%d:%d(%s) StreamID: 0x%x, Timeouts:(%d,%d) cbp=%p:%x cback=[%p:%p:%p]) dma=%p mem=%p", 
		   this, command->GetAddress(), command->GetEndpoint(), command->GetDirection() == kUSBIn ? "in" : "out", 
		   (uint32_t)command->GetStreamID(), (uint32_t) command->GetNoDataTimeout(), (uint32_t) command->GetCompletionTimeout(),
		   command->GetBuffer(), (int)command->GetReqCount(), command->GetUSLCompletion().action, command->GetUSLCompletion().target,
		   command->GetUSLCompletion().parameter, command->GetDMACommand(), command->GetDMACommand()->getMemoryDescriptor());
	
	stream = command->GetStreamID();
	
	err = CreateTransfer(command, stream);
    
	return(err);
}




// Interrupt
IOReturn AppleUSBXHCI::UIMCreateInterruptEndpoint(short				functionAddress,
												  short				endpointNumber,
												  UInt8				direction,
												  short				speed,
												  UInt16			maxPacketSize,
												  short				pollingRate)
{
#pragma unused(functionAddress)
#pragma unused(endpointNumber)
#pragma unused(speed)
#pragma unused(maxPacketSize)
#pragma unused(direction)
#pragma unused(pollingRate)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 1 - Obsolete method called", this);
    return(kIOReturnInternalError);
}

IOReturn AppleUSBXHCI::CreateStream(int				slotID,
                                    int				endpointIdx,
                                    UInt32			stream)
{
	XHCIRing *ring, *streamEp;
	IOReturn err = kIOReturnSuccess;
	StreamContext *ctx;
	
	ring =  GetRing(slotID, endpointIdx, 0);
	if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateStream - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
	if(stream >= (UInt32)ring->transferRingSize)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateStream - stream out of range (%d > %d) (slot:%d, ep:%d) ", this, (int)stream, (int)ring->transferRingSize, (int)slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
	ctx = (StreamContext *)ring->transferRing;
	if(ctx == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateStream - stream context array does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
	
	streamEp = GetRing(slotID, endpointIdx, stream);
	if(streamEp == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateStream - stream does not exist (slot:%d, ep:%d, stream:%d) ", this, (int)slotID, (int)endpointIdx, (int)stream);
		return(kIOReturnBadArgument);
	}
	streamEp->nextIsocFrame = 0;
	streamEp->beingReturned = false;
	streamEp->beingDeleted = false;
	streamEp->needsDoorbell = false;
	if(streamEp->TRBBuffer != NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateStream - stream already exists", this);
		return(kIOReturnNoMemory);
	}
	else
	{
		err = AllocRing(streamEp);
		if(err != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateStream - couldn't alloc transfer ring(slot:%d, ep:%d, stream:%d)", this, (int)slotID, (int)endpointIdx, (int)stream);
			return(kIOReturnNoMemory);
		}
        
        //
        // Setup endpoint queue for Streams Endpoint
        streamEp->endpointType = ring->endpointType;
        AppleXHCIAsyncEndpoint *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);
        streamEp->pEndpoint = (void*)AllocateAppleXHCIAsyncEndpoint(streamEp, pAsyncEP->_maxPacketSize, pAsyncEP->_maxBurst, pAsyncEP->_mult);
        
        if(streamEp->pEndpoint == NULL)
        {
			USBLog(1, "AppleUSBXHCI[%p]::CreateStream - couldn't alloc endpoint queue for (slot:%d, ep:%d, stream:%d)", this, (int)slotID, (int)endpointIdx, (int)stream);
            return kIOReturnNoMemory;
        }
	}
    
	SetStreamCtxAddr64(&ctx[stream], streamEp->transferRingPhys, kXHCI_SCT_PrimaryTRB, streamEp->transferRingPCS);
	
	return(err);
}



IOReturn 
AppleUSBXHCI::CreateEndpoint(int						slotID,
							 int						endpointIdx,
							 UInt16						maxPacketSize,					// this is the base MPS, without burst or mult
							 short						pollingRate,
							 int						epType,
							 UInt8						maxStream,
							 UInt8						maxBurst,						// this is still 0 based
							 UInt8						mult,							// this is still 0 based
							 void                       *pEP)
{
	
	
	USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - Sl:%d, ep:%d, mps:%d, poll:%d, typ:%d, maxStream:%d, maxBurst:%d, mult:%d", this, slotID, endpointIdx, maxPacketSize, pollingRate, epType, (int)maxStream, (int)maxBurst, (int)mult);
	
	
	SInt32              ret = 0;
    UInt32              offs00, CErr;
    UInt8               speed;
	int                 epState, ctxEntries;
	XHCIRing *          ringX = NULL;
	IOReturn			err = kIOReturnSuccess;
	TRB                 t;
	bool				needToCheckBandwidth = true;
	UInt32				ringSizeInPages = 1;
	Context *			inputContext = NULL;
	Context *			slotContext = NULL;
    
    AppleXHCIIsochEndpoint *pIsochEP = NULL;
    AppleXHCIAsyncEndpoint *pAsyncEP = NULL;
	
    USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - inc++ _configuredEndpointCount", this);
    ret = TestConfiguredEpCount();
    if(err != kIOReturnSuccess)
    {
        return(err);
    }
    
    if( (epType == kXHCIEpCtx_EPType_IsocIn) || (epType == kXHCIEpCtx_EPType_IsocOut) )
    {
        pIsochEP = (AppleXHCIIsochEndpoint*)pEP;
    }
    
	if (pIsochEP)
	{
		pollingRate = pIsochEP->xhciInterval;			// this is already in XHCI format
		ringSizeInPages = pIsochEP->ringSizeInPages;
		USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - Isoc - mult(%d) MPS(%d) rate(%d) ringPages(%d)", this, mult, maxPacketSize, pollingRate, (int)ringSizeInPages);
	}
	else
	{
		if(pollingRate != 0)
		{
			pollingRate -= 1;								// Descriptor 1 is 1 uframe, context 0 is 1 (2^0) uframe
		}
	}
	
	speed = GetSlCtxSpeed(GetSlotContext(slotID));
	
    if (speed == kUSBDeviceSpeedSuper)
    {   
        USBLog(6, "AppleUSBXHCI[%p]::CreateEndpoint - SS endpoint with maxPacketSize(%d) mult(%d) maxBurst(%d)", this, maxPacketSize, mult, maxBurst);
    }
    else if (speed == kUSBDeviceSpeedHigh)
    {
        USBLog(6, "AppleUSBXHCI[%p]::CreateEndpoint - HS endpoint with maxPacketSize(%d) mult(%d) maxBurst(%d)", this, maxPacketSize, mult, maxBurst);
   }
    else
    {
        USBLog(6, "AppleUSBXHCI[%p]::CreateEndpoint - FS/LS endpoint with maxPacketSize(%d) mult(%d) maxBurst(%d)", this, maxPacketSize, mult, maxBurst);
    }
	
	// first check to see if this EP is already running with the correct MPS
	epState = GetEpCtxEpState(GetEndpointContext(slotID, endpointIdx));

	if(epState != kXHCIEpCtx_State_Disabled)
	{
		UInt16		oldMPS = GetEpCtxMPS(GetEndpointContext(slotID, endpointIdx));
		
		if (maxPacketSize <= oldMPS)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - new MPS < old MPS, no need to check bandwidth", this);
			needToCheckBandwidth = false;
		}
		ringX = GetRing(slotID, endpointIdx, maxStream);
	}
	if (needToCheckBandwidth)
	{
		err = CheckPeriodicBandwidth(slotID, endpointIdx, maxPacketSize, pollingRate, epType, maxStream, maxBurst, mult);
		if (err != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - CheckPeriodicBandwidth returned err (0x%x)", this, (int)err);
			return err;
		}
		
	}
	
	if (ringX == NULL)
	{
		ringX = CreateRing(slotID, endpointIdx, maxStream);
		if(ringX == NULL)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointIdx);
			
			return(kIOReturnBadArgument);
		}
	}
	
    // Set the Isoc Endpoint only if it is valid
    if (pIsochEP)
    {
        ringX->pEndpoint = pEP;	
    }
    else
    {
        ringX->pEndpoint     = AllocateAppleXHCIAsyncEndpoint(ringX, maxPacketSize, maxBurst, mult);
        
        if(ringX->pEndpoint == NULL)
        {
            // TODO :: Deallocate the ring
            return kIOReturnNoMemory;
        }
                   
        USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - inc ConfiguredEpCount (async)", this); 
        IncConfiguredEpCount();

    }
	
    ringX->endpointType     = epType;
	ringX->nextIsocFrame	= 0;
	ringX->beingReturned	= false;
	ringX->beingDeleted		= false;
	ringX->needsDoorbell	= false;
	
	GetInputContext();
	inputContext = GetInputContextByIndex(0);
	
	epState = GetEpCtxEpState(GetEndpointContext(slotID, endpointIdx));
	if(epState != kXHCIEpCtx_State_Disabled)
	{
		if(epState == kXHCIEpCtx_State_Running)
		{
			StopEndpoint(slotID, endpointIdx);
		}
		// EP already exists, so disable and enable it
		USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - ring already exists (slot:%d, ep:%d), setting MPS to %d ", this, slotID, endpointIdx, maxPacketSize);
		
		inputContext->offs00 = HostToUSBLong(1 << endpointIdx);
		
	}
	// Activate the endpoint
	inputContext->offs04 = HostToUSBLong((1 << endpointIdx) | 1);	// This endpoint, plus the device context
	
	// Initialise the input device context, from the existing device context
	inputContext = GetInputContextByIndex(1);
	slotContext = GetSlotContext(slotID);
	*inputContext = *slotContext;
	
	//USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - before slotCtx, inputctx[1]", this);
	//PrintContext(&_slots[slotID].deviceContext[0]);
	//PrintContext(&_inputContext[1]);
	
	offs00 = USBToHostLong(inputContext->offs00);
	ctxEntries = (offs00 & kXHCISlCtx_CtxEnt_Mask) >> kXHCISlCtx_CtxEnt_Shift;
	if(endpointIdx > ctxEntries)
	{
		ctxEntries = endpointIdx;
		offs00 = (offs00 & ~kXHCISlCtx_CtxEnt_Mask) | (ctxEntries << kXHCISlCtx_CtxEnt_Shift);
	}
	offs00 &= ~kXHCISlCtx_resZ0Bit;	// Clear reserved bit if it set
	inputContext->offs00 = HostToUSBLong(offs00);
	
	//  ****** I'm not sure this is right.
	inputContext->offs0C = 0;	// Nothing in here is an input parameter
	inputContext->offs10 = 0;
	inputContext->offs14 = 0;
	inputContext->offs18 = 0;
	inputContext->offs1C = 0;
	
	CErr = 3;
	if( (epType == kXHCIEpCtx_EPType_IsocOut) || (epType == kXHCIEpCtx_EPType_IsocIn) )
	{
		CErr = 0;
	}
	
	// EP state zero
	// MaxPStreams zero
	// LSA zero
	inputContext = GetInputContextByIndex(endpointIdx + 1);
	SetEPCtxInterval(inputContext, pollingRate);
	
	// CErr = 3
	SetEPCtxCErr(inputContext, CErr);
	// Ep type
	SetEPCtxEpType(inputContext, epType);
	// Mult 
	SetEPCtxMult(inputContext, mult);
	// Max burst
	SetEPCtxMaxBurst(inputContext, maxBurst);
	// max packet size
	SetEPCtxMPS(inputContext,maxPacketSize);
	
	if(ringX->TRBBuffer != NULL)
	{
		if(maxStream > 1)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - create streams endpoint which already exists", this);
			return(kIOReturnNoMemory);
		}
		ReinitTransferRing(slotID, endpointIdx, 0);
	}
	else
	{
		if(maxStream > 1)
		{
			// Allocate the streams context array
			err = AllocStreamsContextArray(ringX, maxStream);
		}
		else
		{
			// Allocate the transfer ring
			err = AllocRing(ringX, ringSizeInPages);
		}
		if(err != kIOReturnSuccess)
		{
			ReleaseInputContext();
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - couldn't alloc transfer ring", this);
			return(kIOReturnNoMemory);
		}
		
		if (pIsochEP)
			pIsochEP->ring = ringX;
	}
	
	SetEPCtxDQpAddr64(inputContext, ringX->transferRingPhys+ringX->transferRingDequeueIdx*sizeof(TRB));
	// Dequeue cycle state, same as producer cycle state
	if (maxStream > 1)
	{
		UInt32		streams;
		
		streams = (maxStream+1)/2;	// make it the power of 2, width is -1.
		int width = 0;
		while(1)
		{
			streams /= 2;
			if(streams == 0)
				break;
			width++;
		};
		USBLog(2, "AppleUSBXHCI[%p]::CreateEndpoint - x maxStream: %d  maxPStreams: %d", this, (int)maxStream, (int)width);
		//PrintContext(&_inputContext[endpointIdx+1]);
		SetEPCtxLSA(inputContext, 1);
		SetEPCtxMaxPStreams(inputContext, width);
		//PrintContext(&_inputContext[endpointIdx+1]);
	}
	else
	{
		SetEPCtxDCS(inputContext, ringX->transferRingPCS);
	}
	
	SetEPCtxAveTRBLen(inputContext, maxPacketSize /* ave TRB len initial */);
	if(pollingRate != 0)
	{   // Only valid for periodic endpoints
		SetEPCtxMaxESITPayload(inputContext, maxPacketSize * (maxBurst+1));
	}
	
#if 0
    USBLog(2, "AppleUSBXHCI[%p]::CreateEndpoint - Context entries: %d", this, (int)ctxEntries);
    for(int i = 0; i<=ctxEntries+1; i++)
    {
        PrintContext(GetInputContextByIndex(i));
    }
    
#endif
    
	// Point controller to input context
	ClearTRB(&t, true);
	
	SetTRBAddr64(&t, _inputContextPhys);
	SetTRBSlotID(&t, slotID);
	
	//PrintTRB(&t, "CreateEndpoint");
	
	ret = WaitForCMD(&t, kXHCITRB_ConfigureEndpoint);
    //USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - after slotCtx", this);
    //PrintContext(&_slots[slotID].deviceContext[0]);
	
	ReleaseInputContext();
	if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
	{
        if(ret == MakeXHCIErrCode(kXHCITRB_CC_ResourceErr))
        {
            // I think this is what we get if we run out of endpoints
            USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - configure endpoint resource error, run out of rings? Returning: %x", this, kIOUSBEndpointCountExceeded);
            
            return(kIOUSBEndpointCountExceeded);
        }
		USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - configure endpoint failed:%d", this, (int)ret);
		
		if( (ret == MakeXHCIErrCode(kXHCITRB_CC_CtxParamErr))	|	// Context param error
		   (ret == MakeXHCIErrCode(kXHCITRB_CC_TRBErr))  )	// NEC giving TRB error when its objecting to context
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - Input Context 0", this);
			PrintContext(GetInputContextByIndex(0));
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - Input Context 1", this);
			PrintContext(GetInputContextByIndex(1));
			USBLog(1, "AppleUSBXHCI[%p]::CreateEndpoint - Input Context X", this);
			PrintContext(GetInputContextByIndex(endpointIdx+1));
		}
		
		return(kIOReturnInternalError);
	}
	
	USBLog(3, "AppleUSBXHCI[%p]::CreateEndpoint - enabling endpoint succeeded", this);
	
    return(kIOReturnSuccess);
}


#pragma mark Interrupt
IOReturn
AppleUSBXHCI::UIMCreateSSInterruptEndpoint(			short		functionAddress,
													short		endpointNumber,
													UInt8		direction,
													short		speed,
													UInt16		maxPacketSize,
													short		pollingRate,
													UInt32		maxBurst)								// this is the 0 based maxBurst
{
	// SS Interrupt endpoints will have the maxPacketSize passed in here as (basemps * burst)
	// we will go ahead and change the mps to the base mps by dividing by the nburst

	if (maxPacketSize > kUSB_EPDesc_MaxMPS)
	{
		// maxBurst is 0 based
		maxPacketSize = (maxPacketSize + maxBurst) / (maxBurst + 1);
	}
    return CreateInterruptEndpoint(functionAddress, endpointNumber, direction, speed, maxPacketSize, pollingRate, 0, 0, maxBurst);
}



IOReturn
AppleUSBXHCI::UIMCreateInterruptEndpoint(			short				functionAddress,
													short				endpointNumber,
													UInt8				direction,
													short				speed,
													UInt16				maxPacketSize,						// this MPS could have a mult built in we will convert it to a burst
													short				pollingRate,
													USBDeviceAddress	highSpeedHub,
													int					highSpeedPort)
{
	// there are three different ways in which a modern UIM will end up here
	// 1) when a FS Interrupt endpoint is first created. No burst or mult needed
	// 2) when a HS Interrupt endpoint is first created. In this case maxPacketSize will include mult (which we will change to burst for XHCI)
	// 3) when a SS Interrupt endpoint is first created, but the burst is 1 (encoded as 0)
	
	int  maxBurst = 0;
	
	if (maxPacketSize > kUSB_EPDesc_MaxMPS)
	{
		maxBurst = ((maxPacketSize + kUSB_EPDesc_MaxMPS - 1) / kUSB_EPDesc_MaxMPS);				// extract the mult from the maxPacketSize (1 based)
		maxPacketSize = (maxPacketSize + maxBurst - 1) / maxBurst;								// convert MPS to <= kUSB_EPDesc_MaxMPS
		maxBurst--;
	}
	
    return CreateInterruptEndpoint(functionAddress, endpointNumber, direction, speed, maxPacketSize, pollingRate, highSpeedHub, highSpeedPort, maxBurst);
}



IOReturn
AppleUSBXHCI::CreateInterruptEndpoint(short				functionAddress,
									  short				endpointNumber,
									  UInt8				direction,
									  short				speed,
									  UInt16			maxPacketSize,							// this is the base MPS
									  short				pollingRate,
									  USBDeviceAddress  highSpeedHub,
									  int				highSpeedPort,
									  UInt32			maxBurst)								// this is the 0 based maxBurst (which could have been the HS mult)
{
	
	
	if( (functionAddress == _rootHubFuncAddressSS) || (functionAddress == _rootHubFuncAddressHS) )
    {
		USBLog(3, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 - starting root hub timer", this);
		return RootHubStartTimer32(pollingRate);
    }
	
	if (0 == functionAddress)
    {
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 - called for address zero", this);
		return(kIOReturnInternalError);
	}
    
	USBLog(3, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 (%d, %d, %d, %d, %d, %d, %d, %d, %d)", this, functionAddress, endpointNumber, direction, speed, maxPacketSize, pollingRate, highSpeedHub, highSpeedPort, (int)maxBurst);
    
	if (pollingRate == 16 ) 
	{
		USBLog(3, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 ••• HACK:  PollingRate was 16.  Will set to 15", this);
		pollingRate = 15;
	}
    
	int slotID, endpointIdx, epType;
	IOReturn err;
	
	slotID = GetSlotID(functionAddress);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 - Unused slot ID for functionAddress: %d", this, functionAddress);
		return(kIOReturnInternalError);
	}
	
	epType = kXHCIEpCtx_EPType_IntOut;
	endpointIdx = GetEndpointID(endpointNumber, direction);
	if(direction == kUSBIn)
	{
		epType = kXHCIEpCtx_EPType_IntIn;
	}
    
	if(speed < kUSBDeviceSpeedHigh)
	{
		// Low or full speed device. Work out the interval as a log
		if( (pollingRate == 0) || (pollingRate < 0) )
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMCreateInterruptEndpoint 2 - bad polling rate: %d", this, pollingRate);
			return(kIOReturnInternalError);
		}
		short logInterval;
		logInterval = 3;
		while(pollingRate)
		{
			logInterval++;
			pollingRate >>= 1;
		}
		pollingRate = logInterval;
	}
    
    err = CreateEndpoint(slotID, endpointIdx, maxPacketSize, pollingRate, epType, 0, maxBurst, 0, NULL);
    
    return(err);
}




// method in 1.8 and 1.8.1
IOReturn AppleUSBXHCI::UIMCreateInterruptTransfer(short				functionNumber,
												  short				endpointNumber,
												  IOUSBCompletion		completion,
												  IOMemoryDescriptor  *CBP,
												  bool				bufferRounding,
												  UInt32				bufferSize,
												  short				direction)
{
#pragma unused(functionNumber)
#pragma unused(endpointNumber)
#pragma unused(completion)
#pragma unused(CBP)
#pragma unused(direction)
#pragma unused(bufferRounding)
#pragma unused(bufferSize)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateInterruptTransfer 1 - Obsolete method called", this);
    return(kIOReturnInternalError);
}





// method in 1.8.2
IOReturn AppleUSBXHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
	IOReturn					status		= kIOReturnSuccess;
    IOUSBCompletion				completion	= command->GetUSLCompletion();
    IOMemoryDescriptor*			buffer		= command->GetBuffer();
	UInt16						address		= command->GetAddress();
	
	if( (address == _rootHubFuncAddressSS) || (address == _rootHubFuncAddressHS) )
    {
		IODMACommand			*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor		*memDesc = dmaCommand ? (IOMemoryDescriptor*)dmaCommand->getMemoryDescriptor() : NULL;
        		
		if (memDesc)
		{
			USBLog(3, "AppleUSBXHCI[%p]::UIMCreateInterruptTransfer - root hub interrupt transfer - clearing unneeded memDesc (%p) from dmaCommand (%p)", this, memDesc, dmaCommand);
			dmaCommand->clearMemoryDescriptor();
		}
		if (command->GetEndpoint() == 1)
		{
			UInt8 speed		= 0;

			if(_rootHubFuncAddressSS == address) 
			{
				speed = kUSBDeviceSpeedSuper;
			}
			else
			{
				speed = kUSBDeviceSpeedHigh;
			}	
			
			IOOptionBits options = 0;
			options	=	(address << kUSBAddress_Shift) & kUSBAddress_Mask;
			options	|=	(speed << kUSBSpeed_Shift) & kUSBSpeed_Mask;
			
			buffer->setTag(options);
			
			status = RootHubQueueInterruptRead(buffer, (UInt32)command->GetReqCount(), completion);
		}
		else
		{
			Complete(completion, kIOUSBEndpointNotFound, (UInt32)command->GetReqCount());
			status = kIOUSBEndpointNotFound;
		}
        return status;
    }    
    	
	status = CreateTransfer(command, 0);
	
	return(status);
}




#pragma mark Isoch
IOReturn AppleUSBXHCI::UIMCreateIsochEndpoint(short				functionAddress,
											  short				endpointNumber,
											  UInt32			maxPacketSize,
											  UInt8				direction)
{
#pragma unused(functionAddress)
#pragma unused(endpointNumber)
#pragma unused(maxPacketSize)
#pragma unused(direction)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochEndpoint 1- Unimplimented method called", this);
    return(kIOReturnInternalError);
}




IOReturn AppleUSBXHCI::UIMCreateIsochEndpoint(short					functionAddress,
											  short					endpointNumber,
											  UInt32				maxPacketSize,
											  UInt8					direction,
											  USBDeviceAddress		highSpeedHub,
											  int					highSpeedPort)
{
#pragma unused(functionAddress)
#pragma unused(endpointNumber)
#pragma unused(maxPacketSize)
#pragma unused(direction)
#pragma unused(highSpeedHub)
#pragma unused(highSpeedPort)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochEndpoint 2- Unimplimented method called", this);
    return(kIOReturnInternalError);
}


IOReturn AppleUSBXHCI::UIMCreateSSIsochEndpoint(short				functionAddress,
												short				endpointNumber,
												UInt32				maxPacketSize,
												UInt8				direction,
												UInt8				interval,
												UInt32				maxBurstAndMult)						// both are 0 based
{
	// SS Isoc endpoints will have the maxPacketSize passed in here as (basemps * burst * mult)
	// we will go ahead and change the mps to the base mps by dividing by the (burst * mult)
	
	int		maxBurst, mult;
	
	maxBurst = (maxBurstAndMult & 0xFF);
	mult = ((maxBurstAndMult & 0xFF00) >> 8);

	if (maxPacketSize > kUSB_EPDesc_MaxMPS)
	{
		maxPacketSize /= ((maxBurst+1) * (mult+1));
	}
    return CreateIsochEndpoint(functionAddress, endpointNumber, maxPacketSize, direction, interval, maxBurst, mult);
}



IOReturn AppleUSBXHCI::UIMCreateIsochEndpoint(short				functionAddress,
											  short				endpointNumber,
											  UInt32			maxPacketSize,
											  UInt8				direction,
											  USBDeviceAddress  highSpeedHub,
											  int				highSpeedPort,
											  UInt8				interval)
{
#pragma unused(highSpeedHub)
#pragma unused(highSpeedPort)
	
	// there are four different ways in which a modern UIM will end up here
	// 1) when a FS Isoc endpoint is first created. No burst or mult needed
	// 2) when a HS Isoc endpoint is first created. In this case maxPacketSize will include mult (which we will change to burst for XHCI)
	// 3) when a SS Isoc endpoint is first created, but the burst is a 1 (encoded as 0)
	// 4) when a SS Isoc endpoint has its packet size changed because of a call to SetPipePolicy, which does not know about burst and mult.
	//		In that case, maxPacketSize will include burst and mult, but those values will already be included in pEP which will already exist

	int  maxBurst = 0;
	
	if (maxPacketSize > kUSB_EPDesc_MaxMPS)
	{
		maxBurst = ((maxPacketSize + kUSB_EPDesc_MaxMPS - 1) / kUSB_EPDesc_MaxMPS);							// extract the mult from the maxPacketSize (1 based)
		maxPacketSize = (maxPacketSize + maxBurst - 1) / maxBurst;											// convert MPS to <= kUSB_EPDesc_MaxMPS
		maxBurst--;																							// make 0 based
	}	
    return CreateIsochEndpoint(functionAddress, endpointNumber, maxPacketSize, direction, interval, maxBurst, 0);
}



IOReturn 
AppleUSBXHCI::CreateIsochEndpoint(short				functionAddress,
								  short				endpointNumber,
								  UInt32			maxPacketSize,				// this will be the base MPS, without burst and mult
								  UInt8				direction,
								  UInt8				interval,
								  UInt8				maxBurst,					// this is the 0 based burst
								  UInt8				mult)						// this is the 0 based mult
{
	
    AppleXHCIIsochEndpoint *		pEP;
	int								slotID, endpointIdx, epType;
	int								TRBsPerTransaction, TRBsPerPage, TRBsNeededInRing;
	IOReturn						err;
	UInt8							xhciInterval;
	UInt8							deviceSpeed;
	UInt32							desiredFullMPS = maxPacketSize * (maxBurst + 1) * (mult + 1);

    USBTrace_Start(kUSBTXHCI, kTPXHCIUIMCreateIsocEndpoint,  (uintptr_t)this, 0, 0, 0);
	USBLog(3, "AppleUSBXHCI[%p]::CreateIsochEndpoint, interval: %d, maxBurst: %d mult: %d", this, interval, maxBurst, mult);

	slotID = GetSlotID(functionAddress);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateIsochEndpoint - Unused slot ID for functionAddress: %d", this, functionAddress);
		return kIOReturnInternalError;
	}

	endpointIdx = GetEndpointID(endpointNumber, direction);
	if(direction == kUSBIn)
	{
		epType = kXHCIEpCtx_EPType_IsocIn;
	}
	else
	{
		epType = kXHCIEpCtx_EPType_IsocOut;
	}
	
	deviceSpeed = GetSlCtxSpeed(GetSlotContext(slotID));
	
	// the interval that is passed in is the raw interval from the endpoint descriptor except that
	// if it is a FS device the interval is hard coded to 1, which should become a 3
	// HS and SS endpoints should have a range of 1-16, which we will convert to 0-15
	// see IOUSBControllerV2::DoCreateEP
	
	if (deviceSpeed == kUSBDeviceSpeedLow)
	{
		USBLog(1, "AppleUSBXHCI[%p]::CreateIsochEndpoint - can't have LS Isoch", this);
		return kIOReturnInternalError;
	}
	
	if (deviceSpeed == kUSBDeviceSpeedFull)
	{
		if (interval != 1)
		{
			USBLog(1, "AppleUSBXHCI[%p]::CreateIsochEndpoint - unexpected FS Isoch interval of %d", this, interval);
		}
		interval = 8;									// convert to 8 microframes
		xhciInterval = 3;								// this is 2^3 or 8 microframes in xhci land
	}
	else
	{
		xhciInterval = interval - 1;					// from the range 1-16 to the range 0-15
	}
	
	// see if the endpoint already exists - if so, this is a SetPipePolicy
    pEP = OSDynamicCast(AppleXHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
	
    if (pEP) 
    {
		UInt32			curFullMPS;
		UInt8			epState;
		
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        // first check to see if the MPS is exactly the same (this is common) and if so, just return easily
        USBLog(5,"AppleUSBXHCI[%p]::CreateIsochEndpoint endpoint already exists, attempting to change maxPacketSize to %d", this, (uint32_t)maxPacketSize);
		
        curFullMPS = pEP->maxPacketSize * pEP->mult * pEP->maxBurst;
        if (desiredFullMPS == curFullMPS)
		{
            USBLog(4, "AppleUSBXHCI[%p]::CreateIsochEndpoint maxPacketSize (%d) the same, no change", this, (uint32_t)maxPacketSize);
			USBTrace_End(kUSBTXHCI, kTPXHCIUIMCreateIsocEndpoint,  (uintptr_t)this, 0, 0, 0);
            return kIOReturnSuccess;
        }
        

		// since the MPS is changing, we will need to stop the current endpoint if it is running
		epState = GetEpCtxEpState(GetEndpointContext(slotID, endpointIdx));
		
		if (epState == kXHCIEpCtx_State_Running)
		{
			StopEndpoint(slotID, endpointIdx);
		}

	}
	else
	{
		pEP = OSDynamicCast(AppleXHCIIsochEndpoint, CreateIsochronousEndpoint(functionAddress, endpointNumber, direction));
		
		if (pEP == NULL) 
			return kIOReturnNoMemory;
		
        USBLog(3, "AppleUSBXHCI[%p]::CreateIsochEndpoint - inc ConfiguredEpCount (isoc)", this);
        IncConfiguredEpCount();
        
		pEP->speed = deviceSpeed;
	}
	
	pEP->maxBurst = maxBurst + 1;									// store it 1 based in the pEP
	pEP->mult = mult + 1;											// store it 1 based in the pEP
	pEP->maxPacketSize = maxPacketSize;

	pEP->interval = (1 << xhciInterval);							// this is true for all endpoint speeds
	pEP->xhciInterval = xhciInterval;

	if (pEP->interval >= kMaxTransfersPerFrame)
	{
		pEP->transactionsPerFrame = 1;
		pEP->msBetweenTDs = pEP->interval / kMaxTransfersPerFrame;
	}
	else
	{
		pEP->transactionsPerFrame = kMaxTransfersPerFrame / pEP->interval;
		pEP->msBetweenTDs = 1;
	}

	// calculate the size needed for the static TRB Ring
	
	TRBsPerTransaction = ((pEP->maxPacketSize * pEP->mult * pEP->maxBurst) / PAGE_SIZE) + 3;					// MPS < 4096 will be 2 TRBs if it crosses a page
	TRBsPerPage = PAGE_SIZE / sizeof(TRB);
	pEP->maxTRBs = TRBsPerTransaction * pEP->transactionsPerFrame;

	TRBsNeededInRing = pEP->maxTRBs * kIsocRingSizeinMS;
	
	USBLog(5, "AppleUSBXHCI[%p]::CreateIsochEndpoint - TRBsPerTransaction(%d) TRBsPerPage(%d) TRBsNeededInRing(%d)", this, TRBsPerTransaction, TRBsPerPage, TRBsNeededInRing);
	pEP->ringSizeInPages = (TRBsNeededInRing + TRBsPerPage - 1) / TRBsPerPage;
	pEP->inSlot = kNumTDSlots + 1;

	pEP->print(6);
	
	err = CreateEndpoint(slotID, endpointIdx, pEP->maxPacketSize, interval, epType, 0, maxBurst, mult, pEP);
    
    USBTrace_End(kUSBTXHCI, kTPXHCIUIMCreateIsocEndpoint,  (uintptr_t)this, 0, 0, 0);
	return err;
}




// obsolete method
IOReturn AppleUSBXHCI::UIMCreateIsochTransfer(short					functionAddress,
											  short					endpointNumber,
											  IOUSBIsocCompletion		completion,
											  UInt8					direction,
											  UInt64					frameStart,
											  IOMemoryDescriptor		*pBuffer,
											  UInt32					frameCount,
											  IOUSBIsocFrame			*pFrames)
{
#pragma unused(functionAddress)
#pragma unused(endpointNumber)
#pragma unused(completion)
#pragma unused(direction)
#pragma unused(frameStart)
#pragma unused(pBuffer)
#pragma unused(frameCount)
#pragma unused(pFrames)
	
    USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer- Unimplimented method called", this);
    return(kIOReturnInternalError);
}



IOReturn 
AppleUSBXHCI::UIMCreateIsochTransfer(IOUSBIsocCommand *command)
{
	IOReturn							err;
    UInt64								maxOffset;
    UInt64								curFrameNumber = GetFrameNumber();
    UInt64								frameDiff;
    UInt32								diff32;
    UInt32								bufferSize;
    AppleXHCIIsochEndpoint  *			pEP;
    AppleXHCIIsochTransferDescriptor *	pNewITD = NULL;
    IOByteCount							transferOffset;
    UInt32								buffPtrSlot, transfer, frameNumberIncrease, frameCount=0, framesBeforeInterrupt;
    unsigned							i,j;
    USBPhysicalAddress32				*buffP, *buffHighP;
    UInt32								*transactionPtr = 0;
    UInt32								*savedTransactionPtr = 0;
    UInt32								pageOffset=0;
    UInt32								page;
    UInt32								frames;
    UInt32								trLen;
    IOPhysicalAddress					dmaStartAddr;
	UInt32								dmaAddrHighBits;
    IOByteCount							segLen;
	bool								lowLatency = command->GetLowLatency();
	UInt32								updateFrequency = command->GetUpdateFrequency();
	IOUSBIsocFrame *					pFrames = command->GetFrameList();
	IOUSBLowLatencyIsocFrame *			pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
	UInt32								transferCount = command->GetNumFrames();
	UInt64								frameNumberStart = command->GetStartFrame();
	// IODMACommand *						dmaCommand = command->GetDMACommand();
	UInt64								offset;
	IODMACommand::Segment64				segments;
	UInt32								numSegments;
	UInt32								transfersPerTD;
	UInt32								numberOfTDs = 0;
	UInt32								baseTransferIndex;
	UInt32								epInterval;
	IOReturn							status;
	bool								newFrame = false;
	
	USBLog(7, "+AppleUSBXHCI[%p]::UIMCreateIsochTransfer - frameNumberStart(%lld) curFrameNumber(%lld)", this, frameNumberStart, curFrameNumber);
	
    USBTrace_Start(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetNumFrames() << 24) | (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), frameNumberStart, curFrameNumber);
    
    
    if ( (command->GetNumFrames() == 0) || (command->GetNumFrames() > 1000) )
    {
        USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer bad frameCount: %d", this, (uint32_t)command->GetNumFrames());
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, command->GetNumFrames(), 0);
        return kIOReturnBadArgument;
    }
	
    pEP = OSDynamicCast(AppleXHCIIsochEndpoint, FindIsochronousEndpoint(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), NULL));
	
    if (pEP == NULL)
    {
        USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), 0, 1);
        return kIOUSBEndpointNotFound;
    }
    
    if ( pEP->aborting )
    {
        USBLog(3, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), 0, 2);
        return kIOReturnNotPermitted;
    }
	
    if(pEP->ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - ring does not exist (addr:%d, ep:%d, dirn:%d) ", this, (int)command->GetAddress(), command->GetEndpoint(), command->GetDirection());
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), 0, 3);
		return(kIOReturnBadArgument);
	}
	if(pEP->ring->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - ******* unallocated ring, (addr:%d, ep:%d, dirn:%d) ", this, (int)command->GetAddress(), command->GetEndpoint(), command->GetDirection());
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), 0, 4);
		return(kIOReturnBadArgument);
	}
    if(pEP->ring->beingDeleted)
    {
        USBLog(2, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - Endpoint being deleted while new transfer is queued, returning kIOReturnNoDevice (%x)", this, kIOReturnNoDevice);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) |(command->GetAddress() << 8) | command->GetEndpoint(), 0, 5);
		return(kIOReturnNoDevice);
    }

	if (frameNumberStart == kAppleUSBSSIsocContinuousFrame)
	{
		pEP->continuousStream = true;
	}
	else
	{
		if (frameNumberStart < pEP->firstAvailableFrame)
		{
			USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer: no overlapping frames -   EP (%p) frameNumberStart: %qd, pEP->firstAvailableFrame: %qd.  Returning 0x%x", this, pEP, frameNumberStart, pEP->firstAvailableFrame, kIOReturnIsoTooOld);
            USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, pEP->firstAvailableFrame, 6);
			return kIOReturnIsoTooOld;
		}
		if (pEP->continuousStream)
		{
			// once it is a continuous endpoint, it will have to stay continuous
			return kIOReturnBadArgument;
		}
	}

    maxOffset = 1024;
	
	if (!pEP->continuousStream)
	{
		if (frameNumberStart != pEP->firstAvailableFrame)
		{
			newFrame = true;
		}
		
		pEP->firstAvailableFrame = frameNumberStart;
		
		if (frameNumberStart <= curFrameNumber)
		{
			if (frameNumberStart < (curFrameNumber - maxOffset))
			{
				USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %d, curFrameNumber: %d.  Returning 0x%x", this, (uint32_t)frameNumberStart, (uint32_t) curFrameNumber, kIOReturnIsoTooOld);
                USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, (curFrameNumber - maxOffset), 7);
				return kIOReturnIsoTooOld;
			}
			USBLog(5,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %d, curFrameNumber: %d.  USBIsocFrame Ptr: %p, First ITD: %p", this, (uint32_t)frameNumberStart, (uint32_t)curFrameNumber, command->GetFrameList(), pEP->toDoEnd);
		} 
		else
		{					// frameNumberStart > curFrameNumber
			if (frameNumberStart > (curFrameNumber + maxOffset))
			{
				USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer request frame too far ahead!  frameNumberStart: %d, curFrameNumber: %d", this, (uint32_t)frameNumberStart, (uint32_t) curFrameNumber);
                USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, (curFrameNumber + maxOffset), 8);
				return kIOReturnIsoTooNew;
			}
			frameDiff = frameNumberStart - curFrameNumber;
			diff32 = (UInt32)frameDiff;
			if (diff32 < _istKeepAwayFrames)
			{
				USBLog(5,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %d)!  frameNumberStart: %d, curFrameNumber: %d", this, (uint32_t) diff32, (uint32_t)frameNumberStart, (uint32_t) curFrameNumber);
                USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameDiff, _istKeepAwayFrames, 9);
			}
		}
	}
	
    if (!command->GetDMACommand() || !command->GetDMACommand()->getMemoryDescriptor())
	{
		USBError(1, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - no DMA Command or missing memory descriptor", this);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (uintptr_t)command->GetDMACommand(), command->GetDMACommand() ? (uintptr_t)command->GetDMACommand()->getMemoryDescriptor() : 0, 10);
		return kIOReturnBadArgument;
	}
	
	epInterval = pEP->interval;
    transferOffset = 0;
	
    // Each frame in the frameList (either "pFrames" or "pLLFrames",
	// depending on whether you're dealing with low-latency), corresponds
	// to a TRANSFER OPPORTUNITY. This is what this outer loop counts through.
	
	// Our job here is to convert this list of TRANSFERS into a list
	// of TDs. Depending on the value of pEP->interval, there could be
	// anywhere from 1 transfer per TD (in the case of pEP->interval
	// equal to 8 or more), up to 8 transfers per TD (if pEP->interval
	// is equal to 1). Other cases are 2 or 4 transfers per TD (for
	// interval values of 4 and 2, respectively)
	
	// Each transfer will happen on a particular microframe. The TD has entries
	// for 8 transactions (transfers), so Transaction0 will go out in microframe 0, 
	// Transaction7 will go out in microframe 7.
	
	// So we need a variable to express the ratio of transfers per TD
	// for this request: "transfersPerTD". Note how, once you've
	// got an pEP->interval > 8, it doesn't matter if the interval is 16,
	// or 32, or 1024 -- effectively, you've got 1 active transfer in
	// whatever USB frame that transfer happens to "land on".  In that case, we need to 
	// just update the USB Frame # of the TD (the frameNumberStart) as well as the endpoints
	// frameNumberStart to advance by bInterval / 8 frames.  We also need to then set the epInterval to 8.
	
	transfersPerTD = (epInterval >= kMaxTransfersPerFrame ? 1 : (kMaxTransfersPerFrame / epInterval));
	frameNumberIncrease = (epInterval <= kMaxTransfersPerFrame ? 1 : epInterval / kMaxTransfersPerFrame);
	
	USBLog(7,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer  transfersPerTD will be %d, frameNumberIncrease = %d", this, (uint32_t)transfersPerTD, (uint32_t)frameNumberIncrease);
    USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, transfersPerTD, frameNumberIncrease, 11);
	
	if ( frameNumberIncrease > 1 )
	{
		// Now that we have a frameNumberIncrease, set the epInterval to 8 so that our calculations are the same as 
		// if we had an interval of 8
		//
		epInterval = 8;		
	}
	
    if (frameNumberStart < pEP->firstAvailableFrame)
    {
		USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer: no overlapping frames -   EP (%p) frameNumberStart: %qd, pEP->firstAvailableFrame: %qd.  Returning 0x%x", this, pEP, frameNumberStart, pEP->firstAvailableFrame, kIOReturnIsoTooOld);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, pEP->firstAvailableFrame, 12);
		return kIOReturnIsoTooOld;
    }
	
    maxOffset = 1024;
	
    if (!pEP->continuousStream)
    {
        if (frameNumberStart != pEP->firstAvailableFrame)
        {
            newFrame = true;
            if(frameNumberIncrease > 1)
            {
                // if we get here frameNumberStart is the start frame requested for a periodic isoc request.
                
                //
                // The XHCI controller requires the start frame for periodic isoc request to be on an "ESIT boundary" meaning that the frame number is 
                // an integral multiple of the interval.
                //
                // If frameNumberStart is not on an ESIT boundary return an error.  If we attempt to move 
                // the start to an ESIT boundary but the requesting driver's calculations for the next start frame will be off.
                
                USBLog(7,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer for periodic isoc frameNumberStart: %lld frameNumberIncrease: %d", this, frameNumberStart, frameNumberIncrease);
                
                if ((frameNumberStart % frameNumberIncrease) != 0) 
                {
                    USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer frameNumberStart is not on ESIT boundary returning error kIOReturnBadArgument frameNumberStart: %lld ", this, frameNumberStart );
                    USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, frameNumberStart % frameNumberIncrease, 13);
                    return kIOReturnBadArgument;
                }
            }
        }
            
        pEP->firstAvailableFrame = frameNumberStart;
	}
	
	USBLog(7, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - pEP (%p) command (%p)", this, pEP, command);

    // Format all the TDs, attach them to the pseudo endpoint. 
    // let the frame interrupt routine put them in the periodic list
	
	// We will set the IOC bit on the last TD of the transaction, unless this is a low latency request and the
	// updateFrequency is between 1 and 8.  0 and > 8 mean only update at the end of the transaction.
	
	// We must enforce the invariant, "there must always be enough transfers left on the framelist
	// to completely fill the current TD". 99% of the time, this is trivial, since the vast majority
	// of possible values of "pEP->interval" are 8 or greater - which means you only need 1 transfer
	// to "fill" the TD. 
	// It's the cases of pEP->interval equal to 1, 2, or 4 we need to worry about; if "transferCount"
	// is not a multiple of 8, 4, or 2, respectively, the "last" TD won't have its full complement of
	// active transfers.
	
	if (!pEP->continuousStream)
	{
		if (transferCount % transfersPerTD != 0)
		{
			USBLog(3,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer Isoch -- incomplete final TD in transfer, command->GetNumFrames() is %d, pEP->Interval is %d and so transfersPerTD is %d", 
				   this, (int ) command->GetNumFrames(), (int ) epInterval, (int ) transfersPerTD);
            USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, command->GetNumFrames(), transferCount % transfersPerTD, 14);
			return kIOReturnBadArgument;
		}
	}
	
	if (updateFrequency == 0)
		updateFrequency = kMaxFramesWithoutInterrupt;
	
  	// We iterate over the framelist not "transfer by transfer", but
	// rather "TD by TD". At any given point in this process, the variable
	// "baseTransferIndex" contains the index into the framelist of the first
	// transfer that'll go into the TD we're currently working on. Each
	// time through the loop, we increment this base index by the number
	// of active transfers in each TD ("transfersPerTD").

	if(lowLatency && (updateFrequency < kMaxFramesWithoutInterrupt))
    {
        framesBeforeInterrupt = updateFrequency;
    }
    else
    {
        framesBeforeInterrupt = kMaxFramesWithoutInterrupt;
    }
	USBLog(7 , "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - transferCount(%d) transfersPerTD(%d) framesBeforeInterrupt(%d)", this, (int)transferCount, (int)transfersPerTD, (int)framesBeforeInterrupt);
    for (baseTransferIndex = 0;  baseTransferIndex < transferCount; baseTransferIndex += transfersPerTD)
    {
        // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
        //
        pNewITD = AppleXHCIIsochTransferDescriptor::ForEndpoint(pEP);
		
        USBLog(7, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - new iTD(%p)", this, pNewITD);
        if (pNewITD == NULL)
        {
            USBLog(1,"AppleUSBXHCI[%p]::UIMCreateIsochTransfer Could not allocate a new iTD", this);
            USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, 0, 15);
            return kIOReturnNoMemory;
        }
		
		// Every TD has some general info
		//
		pNewITD->_lowLatency = lowLatency;
		pNewITD->_framesInTD = 0;
		pNewITD->newFrame = newFrame;
        pNewITD->interruptThisTD = false;
		
        if(frameCount > framesBeforeInterrupt)
        {
            pNewITD->interruptThisTD = true;
            frameCount -= framesBeforeInterrupt;
        }
		// Now, that we won't bail out because of an error, update our parameter in the endpoint that is used to figure out what 
		// frame # we have scheduled
		//
		pEP->firstAvailableFrame += frameNumberIncrease;

		if (frameNumberIncrease == 1)
			newFrame = false;													// for the future TDs
		
		pNewITD->bufferOffset = transferOffset;
		
		// add to the running offset all of the intermediate frame lengths
		for (transfer = 0; ((transfer < transfersPerTD) && ((baseTransferIndex + transfer) < transferCount)); transfer++)
		{			
            if (lowLatency)
            {
                pLLFrames[baseTransferIndex + transfer].frStatus = kUSBLowLatencyIsochTransferKey;
                trLen = pLLFrames[baseTransferIndex + transfer].frReqCount;
            }
            else
            {
                trLen = pFrames[baseTransferIndex + transfer].frReqCount;
            }

			transferOffset += trLen;
		}
		
		// Finish updating the other fields in the TD
		//
		pNewITD->_framesInTD = transfer;										// this may be short if we are continuous streaming
		pNewITD->_pFrames = pFrames;											// Points to the start of the frameList for this request (Used in the callback)
		pNewITD->_frameNumber = frameNumberStart;								// this may be kAppleUSBSSIsocContinuousFrame
		pNewITD->_frameIndex = baseTransferIndex;
		pNewITD->_completion.action = NULL;										// This gets filled in later for the last one.
		pNewITD->_pEndpoint = pEP;
		pNewITD->command = command;
		
		// Debugging aid
		pNewITD->print(7);
		
		// Finally, put the TD on the ToDo list
		//
		USBLog(7, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - putting pTD(%p) on ToDoList for pEP(%p)", this, pNewITD, pEP);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (uintptr_t)pEP, (uintptr_t)pNewITD, 16);
		PutTDonToDoList(pEP, pNewITD);
		
		// Increase our frameNumberStart each time around the loop;
		//
		frameNumberStart += frameNumberIncrease;
        frameCount += frameNumberIncrease;
		numberOfTDs++;
    }
	
    if ( pNewITD == NULL )
    {
        USBLog(1, "AppleUSBXHCI[%p]::CreateHSIsochTransfer - no pNewITD!  Bailing.)", this);
        USBTrace(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, frameNumberStart, 0, 17);
        return kIOReturnInternalError;
    }
    
	// Add the completion action to the last TD
	//
	USBLog(7, "AppleUSBXHCI[%p]::CreateHSIsochTransfer - in TD (%p) setting _completion action (%p)", this, pNewITD, pNewITD ? pNewITD->_completion.action : NULL);
	pNewITD->_completion = command->GetUSLCompletion();
    pNewITD->interruptThisTD = true;    // Always interrupt on the last TD
	
	// Add the request to the schedule
	//
	USBLog(5, "AppleUSBXHCI[%p]::UIMCreateIsochTransfer - adding Frames to schedule for pEP(%p)", this, pEP);

	AddIsocFramesToSchedule(pEP);
	
	USBLog(7, "-AppleUSBXHCI[%p]::UIMCreateIsochTransfer", this);
    USBTrace_End(kUSBTXHCI, kTPXHCIUIMCreateIsochTransfer,  (uintptr_t)this, (command->GetDirection() << 16) | (command->GetAddress() << 8) | command->GetEndpoint(), 0, 0);
	return kIOReturnSuccess;
}



// Restrict streams to 256 per endpoint to avoid massive primary streams context arrays
// See how this works, if it causes problems, then we may need to revisit this
UInt32 
AppleUSBXHCI::UIMMaxSupportedStream(void)
{
	// Note the -1.
	// If there are X supported streams, the stream IDs run from 0 to X-1
	// zero is a valid stream ID, so you really only get X-1 streams.
    
    if (_maxPrimaryStreams == 0)
        return 0;
    
    if(kMaxStreamsPerEndpoint > _maxPrimaryStreams)
    {
        return (_maxPrimaryStreams-1);
    }
    else
    {
		return (kMaxStreamsPerEndpoint-1);
    }
}



int AppleUSBXHCI::QuiesceEndpoint(int slotID, int endpointID)
{
    // Not sure if this is the right name for this function, but I can't think of a better one.
    
    // Bring the endpoint to the stopped state, from whatever state
    
    // Note: Endpoint state diagram in sec 4.8.3.
    
	Context *           epCtx;
    int                 epState, epState1;
    
	USBTrace_Start(kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, 0, 0, 0);
    ClearStopTDs(slotID, endpointID);
    
	epCtx = GetEndpointContext(slotID, endpointID);
	epState = GetEpCtxEpState(epCtx);

    switch(epState)
    {
        case kXHCIEpCtx_State_Halted:
            USBLog(6, "AppleUSBXHCI[%p]::QuiesceEndpoint - resetting endpoint slotID: %d endpointID: %d", this, slotID, endpointID);
			USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 0);
            ResetEndpoint(slotID, endpointID);
            break;
            
        case kXHCIEpCtx_State_Running:
            USBLog(6, "AppleUSBXHCI[%p]::QuiesceEndpoint - stopping endpoint slotID: %d endpointID: %d", this, slotID, endpointID);
			USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 1);
            StopEndpoint(slotID, endpointID);
            epState1 = GetEpCtxEpState(epCtx);
            if(epState1 != kXHCIEpCtx_State_Stopped)
            {
                USBLog(1, "AppleUSBXHCI[%p]::QuiesceEndpoint - state changed before endpoint stopped. (%d)", this, epState1);
                USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, epState1, 0, 2);
                if(epState1 == kXHCIEpCtx_State_Halted)
                {
                    ResetEndpoint(slotID, endpointID);
                    epState = epState1;
                }
            }
            break;
            
        case kXHCIEpCtx_State_Stopped:
            // Already stopped
			USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 3);
            break;
            
        case kXHCIEpCtx_State_Error:
            // In error state, needs a Set Tr Dq pointer to bring it to stopped state.
            // It is assumed that this will be next see <radr://10390833>
            USBLog(2, "AppleUSBXHCI[%p]::QuiesceEndpoint - slotID: %d endpointID: %d in error state", this, slotID, endpointID);
 			USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 4);
            break;
            
        case kXHCIEpCtx_State_Disabled:
            // Endpoint should never be in the disabled state, that means we haven't configured it
            USBLog(1, "AppleUSBXHCI[%p]::QuiesceEndpoint - slotID: %d endpointID: %d in Disabled state, this is a problem", this, slotID, endpointID);
			USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 5);
            break;
            
        default:
            USBLog(1, "AppleUSBXHCI[%p]::QuiesceEndpoint - slotID: %d endpointID: %d unexpected endpoint state:%d", this, epState, slotID, endpointID);
            USBTrace( kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, slotID, endpointID, 6);

    }
	USBTrace_End(kUSBTXHCI, kTPXHCIQuiesceEndpoint,  (uintptr_t)this, epState, 0, 0);
    return(epState);
}



IOReturn 
AppleUSBXHCI::UIMAbortStream(UInt32	streamID,
									  short		functionNumber,
									  short		endpointNumber,
									  short		direction)
{
	int epState, slotID, endpointID;
    IOReturn err=kIOReturnSuccess;

    USBTrace_Start(kUSBTXHCI, kTPXHCIUIMAbortStream,  (uintptr_t)this, 0, 0, 0);
#if 1
	if( (functionNumber == _rootHubFuncAddressSS) || (functionNumber == _rootHubFuncAddressHS) )
    {
        if(streamID != kUSBAllStreams)
        {
            USBLog(1, "AppleUSBXHCI[%p]::UIMAbortStream on root hub - Unimplimented", this);
            return(kIOReturnInternalError);
        }
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBXHCI[%p]::UIMAbortStream: bad params - endpNumber: %d", this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        USBLog(5, "AppleUSBXHCI[%p]::UIMAbortStream: Attempting operation on root hub", this);
		if (endpointNumber == 1)
		{
			if (direction != kUSBIn)
			{
				USBLog(3, "AppleUSBXHCI[%p]::UIMAbortStream - Root hub wrong direction Int pipe %d",  this, direction);
				return kIOReturnInternalError;
			}
			USBLog(5, "AppleUSBXHCI[%p]::UIMAbortStream Root hub aborting int transactions",  this);
			RootHubAbortInterruptRead();
		}
		else
		{
			USBLog(5, "AppleUSBXHCI[%p]::UIMAbortStream Root hub aborting control pipe (NOP)",  this);
		}
		USBTrace_End(kUSBTXHCI, kTPXHCIUIMAbortStream,  (uintptr_t)this, kIOReturnSuccess, 0, 0);
		return kIOReturnSuccess;
    }
#endif	
	
	slotID = GetSlotID(functionNumber);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMAbortStream - unknown function number: %d", this, functionNumber);
		return(kIOReturnInternalError);
	}
	endpointID = GetEndpointID(endpointNumber, direction);
    
    if(streamID != kUSBAllStreams)
    {
        USBLog(3, "AppleUSBXHCI[%p]::UIMAbortStream - with stream ID %u (@%d, %d)", this, (unsigned)streamID, slotID, endpointID);
        
        if(streamID == 0)
        {
            if(IsStreamsEndpoint(slotID, endpointID))
            {
                USBLog(1, "AppleUSBXHCI[%p]::UIMAbortStream - Trying to abort stream zero of streams endpoint %u (@%d, %d)", this, (unsigned)streamID, slotID, endpointID);
                return(kIOReturnBadArgument);
            }
        }
        else
        {   // StreamID non zero
            if(_slots[slotID].maxStream[endpointID] < streamID)
            {
                USBLog(1, "AppleUSBXHCI[%p]::UIMAbortStream - Trying to abort invalid stream ID %u of %u (@%d, %d)", this, (unsigned)streamID, (unsigned)_slots[slotID].maxStream[endpointID], slotID, endpointID);
                return(kIOReturnBadArgument);
            }
        }
   }
    
	USBLog(3, "AppleUSBXHCI[%p]::UIMAbortStream - stream:%u, fn:%d, ep:%d, dir:%d - Slot:%d,ep:%d", this, (unsigned)streamID, functionNumber, endpointNumber, direction, slotID, endpointID);
    
	epState = QuiesceEndpoint(slotID, endpointID);
    
    if(streamID == kUSBAllStreams)
    {
        if(IsStreamsEndpoint(slotID, endpointID))
        {
            if(_slots[slotID].maxStream[endpointID] != 0)
            {
                for(UInt32 i = 1; i<=_slots[slotID].maxStream[endpointID]; i++)
                {
                    IOReturn err1; 
                    err1 = ReturnAllTransfersAndReinitRing(slotID, endpointID, i);
                    // If there's only 1 error, return it, if there are several, return the last
                    if(err1 != kIOReturnSuccess)
                    {
                        err = err1;
                    }
                }
            }
        }
        else
        {
            err = ReturnAllTransfersAndReinitRing(slotID, endpointID, 0);
        }
    }
    else
    {
        err = ReturnAllTransfersAndReinitRing(slotID, endpointID, streamID);
        if(streamID != 0)
        {
            if(epState == kXHCIEpCtx_State_Running)
				RestartStreams(slotID, endpointID, streamID);
        }
    }
    USBTrace_End(kUSBTXHCI, kTPXHCIUIMAbortStream,  (uintptr_t)this, err, 0, 0);
	return err;
    
}



void AppleUSBXHCI::RestartStreams(int slotID, int EndpointID, UInt32 except)
{
	XHCIRing *ring;
    UInt32 maxStream;
    maxStream = _slots[slotID].maxStream[EndpointID];

    if(maxStream <= 1)
    {
        if(!IsStreamsEndpoint(slotID, EndpointID))
        {
            USBLog(1, "AppleUSBXHCI[%p]::RestartStreams - called on non streams endpoint %d, %d", this, slotID, EndpointID);
        }
        return;
    }
    for(UInt32 i = 1; i<=maxStream; i++)
    {
        if(i != except)
        {
            ring = GetRing(slotID, EndpointID, i);
            if(ring != NULL)
            {
                if(ring->transferRingDequeueIdx != ring->transferRingEnqueueIdx)
                {
                    USBLog(3, "AppleUSBXHCI[%p]::RestartStreams - restarting %d (@%d, %d)", this, (int)i, slotID, EndpointID);
                    StartEndpoint(slotID, EndpointID, i);
                }
            }
        }
    }
}


IOReturn AppleUSBXHCI::UIMAbortEndpoint(short				functionNumber,
										short				endpointNumber,
										short				direction)
{
    return UIMAbortStream(kUSBAllStreams, functionNumber, endpointNumber, direction);
}


void AppleUSBXHCI::DeallocRing(XHCIRing *ring)
{
    if(ring != NULL)
    {
        if(ring->TRBBuffer != NULL)
        {
            USBLog(2, "AppleUSBXHCI[%p]::DeallocRing - completing phys:%llx, siz:%x, TRBBuffer:%p", this, ring->transferRingPhys, (unsigned int)ring->transferRingSize, ring->TRBBuffer);
            ring->TRBBuffer->complete();
            ring->TRBBuffer->release();
            ring->TRBBuffer = 0;
        }
        ring->transferRing = 0;
        ring->transferRingPhys = 0;
        ring->transferRingSize = 0;
    }
}

// Problem with certain XHCI controllers. If a transaction is in flight when the
// device is unplugged, and all other ports are unplugged the transaction goes 
// into suspended animation as the clocks are turned off. This transaction is
// reanimated on the next plug when the device reaches U0 (after a reset).
// With VTd turned on this causes a fault which halts the controller, USB is dead
// Supposedly only happens for FS split transactions. This workaround is benign
// and is applied to all high speed of less endpoints.

// The workaround is to do a Set Tr Dq pointer to the endpoint. The new Tr Dq ptr
// points to a dummy ring with no work to do, and all the TRBs are for zero length.
// To ensure the endpoint is in the correct state, the endpoint is stopped first.

// The set TrDqPtr is important as it flushes the TRB cache. A cached TRB could not
// be controlled like this. There is no work on the fake ring, so that the controller
// will not try to access any buffer memory. The TRB Transfer Len is also zero to 
// again head off any data buffer accesses. (So the only access should be to the 
// dummy TRBs.) The dummy TRBs are always mapped, so will not cause a fault.

// SuperSpeed endpoints are not parked even though the workaround is benign, the
// possibility of a streams endpoint made things a little complicated.

// This is the only place apart from SetTRDQPtr where a SetTRDQPtr command is
// done. SetTrDqPtr was not suitable as it doesn't take a ring paramter 

void AppleUSBXHCI::ParkRing(XHCIRing *ring)
{
	TRB t;
    UInt8 speed;
    speed = GetSlCtxSpeed(GetSlotContext(ring->slotID));
    if(speed <= kXHCISpeed_High)
    {
        SInt32 err;
        QuiesceEndpoint(ring->slotID, ring->endpointID);
        ClearTRB(&t, true);
        SetTRBSlotID(&t, ring->slotID);
        SetTRBEpID(&t, ring->endpointID);
        SetTRBAddr64(&t, _DummyRingPhys);
        SetTRBDCS(&t, _DummyRingCycleBit);
        err = WaitForCMD(&t, kXHCITRB_SetTRDqPtr);
        if((err == CMD_NOT_COMPLETED) || (err <= MakeXHCIErrCode(0)))
        {
            USBLog(1, "AppleUSBXHCI[%p]::ParkRing - couldn't set tr dq pointer: %d", this, err);
            PrintContext(GetSlotContext(ring->slotID));
            PrintContext(GetEndpointContext(ring->slotID, ring->endpointID));
        }
    }
}

IOReturn AppleUSBXHCI::UIMDeleteEndpoint(short				functionNumber,
										 short				endpointNumber,
										 short				direction)
{
	UInt32 offs00;
    SInt32 ret;
	int slotID, endpointIdx;
	XHCIRing *ringX;
    int ctxEntries;
	TRB t;
	Context *inputContext;
	Context *deviceContext;
    
    // Streams fix needed here
    
	USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - fn:%d, ep:%d, dir:%d", this, functionNumber, endpointNumber, direction);
	
	slotID = GetSlotID(functionNumber);
    
	if(slotID == 0)
	{
		if(functionNumber == 0)
		{
			// No need to delete for device zero, it got addressed
			
			return(kIOReturnSuccess);
		}
		USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - Unused slot ID for functionAddress: %d", this, functionNumber);
		return(kIOReturnBadArgument);
	}
    
	endpointIdx = GetEndpointID(endpointNumber, direction);
    
	ringX = GetRing(slotID, endpointIdx, 0);
	if (ringX == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointIdx);
		return(kIOReturnBadArgument);
	}
	if (ringX->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - ******* unallocated ring, slot:%d id:%d", this, slotID, endpointIdx);
		return(kIOReturnBadArgument);
	}

    ringX->beingDeleted = true;
	UIMAbortEndpoint(functionNumber, endpointNumber, direction);
    if( (_errataBits & kXHCIErrata_ParkRing) != 0)
    {
        ParkRing(ringX);
    }
   
	if (endpointNumber == 0)
	{
		ringX = GetRing(slotID, 1, 0);
        
		if(ringX != NULL)
		{
            if (ringX->pEndpoint)
            {
                AppleXHCIAsyncEndpoint* pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ringX->pEndpoint);
                
                if (pAsyncEP)
                {
                    pAsyncEP->Abort();
                    pAsyncEP->release();
                    DecConfiguredEpCount();
                    USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint(async) - dec-- _configuredEndpointCount pEP=%p", this, ringX->pEndpoint);
                    
                    ringX->pEndpoint = NULL;
                }
            }

			// Unallocate ring here
			DeallocRing(ringX);
			IOFree((void *)ringX, sizeof(XHCIRing));
            _slots[slotID].potentialStreams[endpointIdx] = 0;
            _slots[slotID].maxStream[endpointIdx] = 0;
            _slots[slotID].rings[endpointIdx] = NULL;
		}
		
	}
	else
	{
		GetInputContext();
		
		inputContext = GetInputContextByIndex(0);
		
		inputContext->offs00 = HostToUSBLong(1 << endpointIdx);	// This XHCIRing
		inputContext->offs04 = HostToUSBLong(1);	//  device context
		
		// Initialise the input device context, from the existing device context
		inputContext = GetInputContextByIndex(1);
		deviceContext = GetSlotContext(slotID);
		*inputContext = *deviceContext;
		offs00 = USBToHostLong(inputContext->offs00);
		ctxEntries = (offs00 & kXHCISlCtx_CtxEnt_Mask) >> kXHCISlCtx_CtxEnt_Shift;
		if(endpointIdx == ctxEntries)
		{
			do{
				XHCIRing *ring;
				ctxEntries--;	// **** Count the context entries here
				ring = GetRing(slotID, ctxEntries, 0);
				
				if( (ring != NULL) && (ring->TRBBuffer != NULL) )
				{
					break;
				}
			}while(ctxEntries > 0);
			if(ctxEntries == 0)
			{
				USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - All eps deleted, setting Context entries to 1", this);
				ctxEntries = 1;
			}
			else
			{
				USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - Context entries now: %d", this, ctxEntries);
			}
			offs00 = (offs00 & ~kXHCISlCtx_CtxEnt_Mask) | (ctxEntries << kXHCISlCtx_CtxEnt_Shift);
		}
		offs00 &= ~kXHCISlCtx_resZ0Bit;	// Clear reserved bit if it set
		inputContext->offs00 = HostToUSBLong(offs00);
		
		// Point controller to input context
		ClearTRB(&t, true);
		
		SetTRBAddr64(&t, _inputContextPhys);
		SetTRBSlotID(&t, slotID);
		
		PrintTRB(6, &t, "UIMDeleteEndpoint 3");
		
		ret = WaitForCMD(&t, kXHCITRB_ConfigureEndpoint);
		
#if 1
        USBLog(6, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - Output context entries: %d", this, (int)ctxEntries);
        for(int i = 0; i<=ctxEntries+1; i++)
        {
            PrintContext(GetEndpointContext(slotID, i));
        }
        
#endif
        
		ReleaseInputContext();

        //
        // If error, don't return here, we will leak rings and endpoints.
		if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - Configure endpoint command failed.", this);
		}
		
        DeleteStreams(slotID, endpointIdx);

		if (ringX->pEndpoint)
		{
            if( (ringX->endpointType == kXHCIEpCtx_EPType_IsocIn) || (ringX->endpointType == kXHCIEpCtx_EPType_IsocOut) )
            {
                DeleteIsochEP((AppleXHCIIsochEndpoint*)ringX->pEndpoint);
                DecConfiguredEpCount();
                USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint(isoc) - dec-- _configuredEndpointCount pEP=%p", this, ringX->pEndpoint);
            }
            else
            {
                AppleXHCIAsyncEndpoint* pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ringX->pEndpoint);
                pAsyncEP->Abort();
                pAsyncEP->release();
                DecConfiguredEpCount();
                USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint(async2) - dec-- _configuredEndpointCount pEP=%p", this, ringX->pEndpoint);
                
            }
			ringX->pEndpoint = NULL;
		}
        
		DeallocRing(ringX);
		IOFree(ringX, sizeof(XHCIRing)* (_slots[slotID].maxStream[endpointIdx]+1));
        _slots[slotID].potentialStreams[endpointIdx] = 0;
        _slots[slotID].maxStream[endpointIdx] = 0;
        _slots[slotID].rings[endpointIdx] = NULL;
	}
	
	for(int i = 1; i < kXHCI_Num_Contexts; i++)
	{
		ringX = GetRing(slotID, i, 0);
		if( (ringX != NULL) && (ringX->TRBBuffer != NULL) )
		{
			// There's still an endpoint active, so just return.
			return(kIOReturnSuccess);
		}
		
	}
	
	// All rings deleted, now delete or reset the device.
	ClearTRB(&t, true);
	SetTRBSlotID(&t, slotID);
	PrintTRB(6, &t, "UIMDeleteEndpoint 2");
	
    USBLog(3, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - Disabling slot:%d.", this, slotID);
    ret = WaitForCMD(&t, kXHCITRB_DisableSlot);	
    PrintSlotContexts();

    //
    // If error, don't return here, we will leak slot->buffer.
	if((ret == CMD_NOT_COMPLETED) || (ret <= MakeXHCIErrCode(0)))
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMDeleteEndpoint - disable slot/reset command failed.", this);
	}
	
    _slots[slotID].deviceContext = NULL;    // Do this first to mark its deleted.
	_slots[slotID].deviceContext64 = NULL;
    
    SetDCBAAAddr64(&_DCBAA[slotID], NULL);
    
    // Relase the output context
    _slots[slotID].buffer->complete();
    _slots[slotID].buffer->release();
    _slots[slotID].buffer = 0;
    _slots[slotID].deviceContextPhys = 0;
    _slots[slotID].deviceNeedsReset = false;

	_devHub[functionNumber] = 0;
	_devPort[functionNumber] = 0;
	_devMapping[functionNumber] = 0;
	_devEnabled[functionNumber] = false;
	
	return(kIOReturnSuccess);
}

void
AppleUSBXHCI::DeleteStreams(int slotID, int endpointIdx)
{
    for(UInt32 i = 1; i<=_slots[slotID].maxStream[endpointIdx]; i++)
    {
        USBLog(2, "AppleUSBXHCI[%p]::DeleteStreams - delete stream %d (@%d, %d).", this, (int)i, slotID, endpointIdx);
        
        XHCIRing *pStreamRing = GetRing(slotID, endpointIdx, i);
        
        if (pStreamRing)
        {
            AppleXHCIAsyncEndpoint  *pAsyncEP = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)pStreamRing->pEndpoint);
            
            if (pAsyncEP)
            {
                pAsyncEP->Abort();
                
                pAsyncEP->release();
            }
            
            DeallocRing(pStreamRing);
        }
    }
}


#if 0

IOReturn AppleUSBXHCI::UIMClearEndpointStall(short				functionNumber,
											 short				endpointNumber,
											 short				direction)
{
	Context *					epCtx;
	int						epState, slotID, endpointID;
	XHCIRing *				ring;
    IOReturn				err = kIOReturnSuccess;
	slotID =				GetSlotID(functionNumber);
	
	
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - unknown function number: %d", this, functionNumber);
		return(kIOReturnInternalError);
	}

    USBTrace_Start(kUSBTXHCI, kTPXHCIUIMClearEndpointStall,  (uintptr_t)this, 0, 0, 0);

	endpointID = GetEndpointID(endpointNumber, direction);
	USBLog(7, "AppleUSBXHCI[%p]::UIMClearEndpointStall - fn: %d, ep:%d, dir:%d, slot: %d, epID:%d", this, functionNumber, endpointNumber, direction, slotID, endpointID);
    
	ring = GetRing(slotID, endpointID, 0);
	if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointID);
		return(kIOReturnBadArgument);
	}
	if(ring->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - ******* unallocated ring, slot:%d id:%d", this, slotID, endpointID);
		return(kIOReturnBadArgument);
	}
	if(ring->beingReturned)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - reenterred", this);
		return(kIOUSBClearPipeStallNotRecursive);
	}
	
	USBLog(7, "AppleUSBXHCI[%p]::UIMClearEndpointStall - tr ring: %p size: %d", this, ring->transferRing, ring->transferRingSize);
	
	epCtx = GetEndpointContext(slotID, endpointID);
	PrintContext(epCtx);
    epState = USBToHostLong(epCtx->offs00) & kXHCIEpCtx_State_Mask;
	if(epState != kXHCIEpCtx_State_Halted)
	{
		USBLog(4, "AppleUSBXHCI[%p]::UIMClearEndpointStall - not halted, clear endpoint", this);
        // Now do configure endpoint with both add and drop flags set.
        ClearEndpoint(slotID, endpointID);
	}
	epState = QuiesceEndpoint(slotID, endpointID);
	USBLog(7, "AppleUSBXHCI[%p]::UIMClearEndpointStall - EP State1: %d", this, epState);
	
	epState = GetEpCtxEpState(epCtx);
	USBLog(7, "AppleUSBXHCI[%p]::UIMClearEndpointStall - EP State2: %d", this, epState);
	
    if(IsStreamsEndpoint(slotID, endpointID))
    {
        if(_slots[slotID].maxStream[endpointID] != 0)
        {
            for(UInt32 i = 1; i<=_slots[slotID].maxStream[endpointID]; i++)
            {
                IOReturn err1; 
                err1 = ReturnAllTransfersAndReinitRing(slotID, endpointID, i);
                
                // If there's only 1 error, return it, if there are several, return the last
                if(err1 != kIOReturnSuccess)
                {
                    err = err1;
                }
            }
        }
    }
    else
    {
        err = ReturnAllTransfersAndReinitRing(slotID, endpointID, 0);
    }
	
	PrintContext(GetEndpointContext(slotID, endpointID));

    USBTrace_Start(kUSBTXHCI, kTPXHCIUIMClearEndpointStall,  (uintptr_t)this, err, 0, 0);
	return err ;
    
}

#else

IOReturn AppleUSBXHCI::UIMClearEndpointStall(short				functionNumber,
											 short				endpointNumber,
											 short				direction)
{
	int epState, slotID, endpointID;
	XHCIRing *ring;
    IOReturn ret;
	Context *epCtx;
    
	slotID = GetSlotID(functionNumber);
	if(slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - unknown function number: %d", this, functionNumber);
		return(kIOReturnInternalError);
	}
	endpointID = GetEndpointID(endpointNumber, direction);
	USBLog(7, "AppleUSBXHCI[%p]::UIMClearEndpointStall - fn: %d, ep:%d, dir:%d, slot: %d, epID:%d", this, functionNumber, endpointNumber, direction, slotID, endpointID);
    
	ring = GetRing(slotID, endpointID, 0);
	if(ring == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - ring does not exist (slot:%d, ep:%d) ", this, (int)slotID, (int)endpointID);
		return(kIOReturnBadArgument);
	}
	if(ring->TRBBuffer == NULL)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - ******* unallocated ring, slot:%d id:%d", this, slotID, endpointID);
		return(kIOReturnBadArgument);
	}
	if(ring->beingReturned)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - reenterred, returning kIOUSBClearPipeStallNotRecursive", this);
		return(kIOUSBClearPipeStallNotRecursive);
	}
	
	epCtx = GetEndpointContext(slotID, endpointID);
	epState = GetEpCtxEpState(epCtx);
	if(epState != kXHCIEpCtx_State_Halted)
	{
        // State is not halted, so the quiesce endpoint did not clear the toggles. This will clear the toggles.
		USBLog(4, "AppleUSBXHCI[%p]::UIMClearEndpointStall - was not halted, clear endpoint", this);
        // Now do configure endpoint with both add and drop flags set.
        ClearEndpoint(slotID, endpointID);
	}
    
    // Abort stream does QuiesceEndpoint and return/reinit.
	ret = UIMAbortStream(kUSBAllStreams, functionNumber, endpointNumber, direction);
    
    if(ret != kIOReturnSuccess)
    {
		USBLog(1, "AppleUSBXHCI[%p]::UIMClearEndpointStall - UIMAbortStream returned: %x", this, ret);
        return(ret);
    }
    
    
	return(kIOReturnSuccess);
    
}

#endif


void			AppleUSBXHCI::UIMRootHubStatusChange( bool abort )
{
#pragma unused(abort)
	
	USBLog(1, "AppleUSBXHCI[%p]::UIMRootHubStatusChange - calling obsolete method UIMRootHubStatusChange(bool)", this);
}


IOReturn 		AppleUSBXHCI::SetRootHubDescriptor( OSData *buffer )
{
#pragma unused(buffer)
	
    USBLog(1, "AppleUSBXHCI[%p]::SetRootHubDescriptor - Unimplimented method called", this);
    return(kIOReturnInternalError);
}


IOReturn		AppleUSBXHCI::ClearRootHubFeature( UInt16 wValue )
{
#pragma unused(wValue)
	
    USBLog(1, "AppleUSBXHCI[%p]::ClearRootHubFeature - Unimplimented method called", this);
    return(kIOReturnInternalError);
}

IOReturn 		AppleUSBXHCI::GetRootHubPortState( UInt8 *state, UInt16 port )
{
#pragma unused(state)
#pragma unused(port)

    USBLog(1, "AppleUSBXHCI[%p]::GetRootHubPortState - Unimplimented method called", this);
    return(kIOReturnInternalError);
}



UInt64 		AppleUSBXHCI::GetFrameNumber( void )
{
	
    UInt64		temp1, temp2;
    register 	UInt32	frindex;
	UInt32		sts;
    UInt32      count = 0;
    bool        loggedZero = false;
    
	//*******************************************************************************************************
	// ************* WARNING WARNING WARNING ****************************************************************
	// Preemption may be off, which means that we cannot make any calls which may block
	// So don't call USBLog for example (see AddIsocFramesToSchedule)
	//*******************************************************************************************************
    
	sts = Read32Reg(&_pXHCIRegisters->USBSTS);
	if (_lostRegisterAccess)
	{
		return 0;
	}
	
	// If the controller is halted, then we should just bail out
	if ( sts & kXHCIHCHaltedBit)
	{
		// USBLog(1, "AppleUSBXHCI[%p]::GetFrameNumber called but controller is halted",  this);
        USBTrace( kUSBTXHCI, kTPXHCIGetFrameNumber,  (uintptr_t)this, 0, 0, 4 );
		return 0;
	}
	
    // First, get a snapshot but make sure that we haven't wrapped around and not processed the new value.  Note that the
    // rollover bit is processed at primary interrupt time, so we don't need to take that into account in this calculation.
    //
    do
    {
		temp1 = _frameNumber64;
		frindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
		if (_lostRegisterAccess)
		{
			return 0;
		}
		
		temp2 = _frameNumber64;
		
		if ((frindex == 0) && !loggedZero)
        {
			USBTrace( kUSBTXHCI, kTPXHCIGetFrameNumber,  (uintptr_t)this, (int)temp1, (int)temp2, 0 );
            loggedZero = true;
        }

        if((count++ > 100) && (temp1 == temp2) && (frindex == 0) )
        {
			USBTrace( kUSBTXHCI, kTPXHCIGetFrameNumber,  (uintptr_t)this, (int)temp1, (int)temp2, 1 );
            IODelay(126);								// Slightly more than one Microframes worth
			USBTrace( kUSBTXHCI, kTPXHCIGetFrameNumber,  (uintptr_t)this, (int)temp1, (int)temp2, 2 );
			temp1 = _frameNumber64;
            frindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
			if (_lostRegisterAccess)
			{
				return 0;
			}
			
			temp2 = _frameNumber64;
            if(frindex == 0)
            {
                return 0;
            }
        }
    } while ( (temp1 != temp2) || (frindex == 0) );
	
    // Shift out the microframes
    //
    frindex = frindex >> 3;		
    
    //USBLog(2, "AppleUSBXHCI[%p]::GetFrameNumber -- returning %Ld (0x%Lx) FnHi: %d",  this, temp1+frindex, temp1+frindex, (int)temp1);
	
    USBTrace( kUSBTXHCI, kTPXHCIGetFrameNumber,  (uintptr_t)this, (int)((temp1 + frindex) >> 32), (int)(temp1 + frindex), 3 );
    return (temp1 + frindex);
}



UInt64 		
AppleUSBXHCI::GetMicroFrameNumber( void )
{
	
    UInt64		temp1, temp2;
    register 	UInt32	frindex;
    UInt32      count = 0;
	UInt32		sts;
		
	sts = Read32Reg(&_pXHCIRegisters->USBSTS);
	if (_lostRegisterAccess)
	{
		return 0;
	}
	
	// If the controller is halted, then we should just bail out
	if ( sts & kXHCIHCHaltedBit)
	{
		// USBLog(1, "AppleUSBXHCI[%p]::GetFrameNumber called but controller is halted",  this);
		return 0;
	}
	
    // First, get a snapshot but make sure that we haven't wrapped around and not processed the new value.  Note that the
    // rollover bit is processed at primary interrupt time, so we don't need to take that into account in this calculation.
    //
    do
    {
        temp1 = _frameNumber64;
        frindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
		if (_lostRegisterAccess)
		{
			return 0;
		}
		
        temp2 = _frameNumber64;

        if((count++ > 100) && (temp1 == temp2) && (frindex == 0) )
        {
            IODelay(126);								// Slightly more than one Microframes worth
			temp1 = _frameNumber64;
			frindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
			if (_lostRegisterAccess)
			{
				return 0;
			}
			
			temp2 = _frameNumber64;
            if(frindex == 0)
            {
                break;
            }
        }
    } while ( (temp1 != temp2) || (frindex == 0) );
	
    // Since this MFINDEX has the lower 3 bits of the microframe, we need to adjust the upper 61 bits by shifting them up
    temp1 = temp1 << 3;
	
    return (temp1 + frindex);
}

UInt32 		AppleUSBXHCI::GetFrameNumber32( void )
{
    return((UInt32)GetFrameNumber());
}


// Debugger polled mode
void 		AppleUSBXHCI::PollInterrupts( IOUSBCompletionAction safeAction)
{
#pragma unused(safeAction)
    
	UInt32 statusReg = Read32Reg(&_pXHCIRegisters->USBSTS);
	
	if (_lostRegisterAccess)
	{
		return;
	}
		
	//  Port Change Interrupt
	if ( statusReg & kXHCIPCD )
	{
		
		Write32Reg(&_pXHCIRegisters->USBSTS, kXHCIPCD);
		
		USBLog(6,"AppleUSBXHCI[%p]::PollInterrupts -  Port Change Detect Bit on bus 0x%x - ensuring usability", this, (uint32_t)_busNumber );
		EnsureUsability();
		
		if (_myPowerState == kUSBPowerStateOn)
		{
			// Check to see if we are resuming the port
			RHCheckForPortResumes();
		}
		else
		{
			USBLog(2, "AppleUSBXHCI[%p]::PollInterrupts - deferring checking for RHStatus until we are running again", this);
		}
	}
	
	// No need to clear the IMAN IP bit, it is auto cleared and EHB is set.
	// Note: PollEventRing2 no longer clears EHB, the filter interrupt does that.
	
	USBTrace_Start( kUSBTXHCIInterrupts, kTPXHCIInterruptsPollInterrupts, (uintptr_t)this, (uintptr_t)0, 0, 0 );
	
	while(PollEventRing2(kPrimaryInterrupter)) ;
	while(PollEventRing2(kTransferInterrupter)) ;
	
	USBTrace_End( kUSBTXHCIInterrupts, kTPXHCIInterruptsPollInterrupts, (uintptr_t)this, 0, 0, 0 );
}

//================================================================================================
//
//   StopUSBBus
//
//================================================================================================
//

IOReturn
AppleUSBXHCI::StopUSBBus()
{
	UInt32		CMD, count=0;
    
	CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
    
	if( CMD & kXHCICMDRunStop )
	{	
		// Stop the controller
		CMD &= ~kXHCICMDRunStop;
		Write32Reg(&_pXHCIRegisters->USBCMD, CMD);
		
		while( ((Read32Reg(&_pXHCIRegisters->USBSTS) & kXHCIHCHaltedBit) == 0) && !(_lostRegisterAccess) )
		{
			IOSleep(1);
			if(count++ >100)
			{
				USBLog(1, "AppleUSBXHCI[%p]::StopUSBBus - Controller not halted after 100ms", this);
				return kIOReturnInternalError;
			}
		}
		
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
	}
	
    _myBusState = kUSBBusStateReset;
    
    USBLog(5, "AppleUSBXHCI[%p]::StopUSBBus - HC halted",  this);

	return kIOReturnSuccess;
}

//================================================================================================
//
//   RestartUSBBus
//
//================================================================================================
//
void			
AppleUSBXHCI::RestartUSBBus()
{
    UInt32	count=0;
    UInt32	CMD, STS;
	
	
	do {
		// Start the controller running, also turn on interrupts and wrap events.
		CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
		if (_lostRegisterAccess)
		{
			break;
		}
		
		CMD |= kXHCICMDRunStop | kXHCICMDINTE | kXHCICMDEWE;
		Write32Reg(&_pXHCIRegisters->USBCMD, CMD);
		IOSync();
		
		STS = Read32Reg(&_pXHCIRegisters->USBSTS);
		if (_lostRegisterAccess)
		{
			break;
		}
		
		while(STS & kXHCIHCHaltedBit)
		{
			IOSleep(1);
			if(count++ >100)
			{
				USBLog(1, "AppleUSBXHCI[%p]::RestartUSBBus - Controller not running after 100ms", this);
				break;
			}
			
			STS = Read32Reg(&_pXHCIRegisters->USBSTS);
			if (_lostRegisterAccess)
			{
				break;
			}
		}
		
		if (_lostRegisterAccess)
		{
			break;
		}
		
		_myBusState = kUSBBusStateRunning;
		USBLog(5, "AppleUSBXHCI[%p]::RestartUSBBus - HC restarted",  this);
	} while (0);
}

//================================================================================================
//
//   ResetController
//
//================================================================================================
//
IOReturn
AppleUSBXHCI::ResetController()
{
    int i;
	UInt32 CMD;
    IOReturn status = kIOReturnSuccess;
	volatile UInt32 val = 0;
	volatile UInt32 * addr;

	
	// Issue HCRST
	Write32Reg(&_pXHCIRegisters->USBCMD, kXHCICMDHCReset); // set the reset bit
	IOSync();
	
	CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
	
	for (i=0; (i < 1000) && (CMD & kXHCICMDHCReset); i++)
	{
		if (_lostRegisterAccess)
		{
			status = kIOReturnNoDevice;
			break;
		}
		
		IOSleep(1);
		if (i >= 1000)
		{
			USBError(1, "AppleUSBXHCI[%p]::ResetController - could not get chip to come out of reset within %d ms",  this, i);
			status = kIOReturnInternalError;
            break;
		}
		if(i > 100)
		{
			USBLog(3, "AppleUSBXHCI[%p]::ResetController - took: %dms to come out of reset", this, i);
		}
		
		CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
		if (_lostRegisterAccess)
		{
			status = kIOReturnNoDevice;
			break;
		}
	}

	
    return status;
}

// this call is not gated, so we need to gate it ourselves
IOReturn
AppleUSBXHCI::GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime)
{
	if (!_commandGate)
		return kIOReturnUnsupported;
	
	return _commandGate->runAction(GatedGetFrameNumberWithTime, frameNumber, theTime);
}



// here is the gated version
IOReturn
AppleUSBXHCI::GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3)
{
#pragma unused (arg2, arg3)
	AppleUSBXHCI		*me = (AppleUSBXHCI*)owner;
	UInt64				*frameNumber = (UInt64*)arg0;
	AbsoluteTime		*theTime = (AbsoluteTime*)arg1;
	
	*frameNumber = me->_anchorFrame;
	*theTime = me->_anchorTime;
	return kIOReturnSuccess;
}

IOReturn 
AppleUSBXHCI::ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed, USBDeviceAddress hub, int port)
{
    
    if( (hub == _rootHubFuncAddressHS) || (hub == _rootHubFuncAddressSS) )
    {
        UInt8 rhSpeed = (hub == _rootHubFuncAddressHS) ? kUSBDeviceSpeedHigh : kUSBDeviceSpeedSuper;
        
        AdjustRootHubPortNumbers(rhSpeed, (UInt16*)&port);
    }
    
    USBLog(3, "AppleUSBXHCI[%p]::UIM **** - ConfigureDeviceZero maxPacketSize:%d, speed:%d, hub:%d, adj port:%d", this, maxPacketSize, speed, hub, port);
    
	_devZeroPort = port;
	_devZeroHub = hub;
	
	return(super::ConfigureDeviceZero(maxPacketSize, speed, hub, port));
}

bool 
AppleUSBXHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable)) 
		return false;
	
    _controllerSpeed = kUSBDeviceSpeedSuper;	

	_isochScheduleLock = IOSimpleLockAlloc();
    if (!_isochScheduleLock)
	{
		return false;
	}

    _uimInitialized = false;
    _myBusState = kUSBBusStateReset;
    
	return true;
	
}

IOReturn				
AppleUSBXHCI::UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable)
{
	int slotID;
    
	USBLog(3, "AppleUSBXHCI[%p]::UIMEnableAddressEndpoints - address: %d %s", this, address, enable?"enable":"disable");
	
	slotID = GetSlotID(address);
	if(slotID == 0)
	{
		if(enable)
		{
			if( address > 127 )
			{
				USBLog(2, "AppleUSBXHCI[%p]::UIMEnableAddressEndpoints - address out of range: %d", this, address);
				return(kIOReturnBadArgument);
			}
            
			// Must be here because device is disabled.
			_devEnabled[address] = true;
			slotID = GetSlotID(address);
			for(int i = 1; i<kXHCI_Num_Contexts; i++)
			{
				XHCIRing *		ringX;
				
				ringX = GetRing(slotID, i, 0);
				if( (ringX != NULL) && (ringX->TRBBuffer != 0) )
				{
					// Now restart endpoint
					if(IsStreamsEndpoint(slotID, i))
					{
						USBLog(5, "AppleUSBXHCI[%p]::UIMEnableAddressEndpoints - calling RestartStreams(%d, %d)", this, slotID, i);
						RestartStreams(slotID, i, 0);
					}
					else
					{
						USBLog(5, "AppleUSBXHCI[%p]::UIMEnableAddressEndpoints - calling StartEndpoint(%d, %d)", this, slotID, i);
						StartEndpoint(slotID, i);
					}
				}
			}

			return(kIOReturnSuccess);
		}
		else
		{
			USBLog(1, "AppleUSBXHCI[%p]::UIMEnableAddressEndpoints - unknown address2: %d", this, address);
			return(kIOReturnBadArgument);
		}
	}
    
	if(!enable)
	{
		for(int i = 1; i<kXHCI_Num_Contexts; i++)
		{
			XHCIRing *ringX;
			ringX = GetRing(slotID, i, 0);
			if( (ringX != NULL) && (ringX->TRBBuffer != 0) )
			{
				StopEndpoint(slotID, i);
			}
		}
		_devEnabled[address] = false;
	}
	return(kIOReturnSuccess);
}



IOReturn				
AppleUSBXHCI::UIMEnableAllEndpoints(bool enable)
{
#pragma unused(enable)
        
    USBLog(1, "AppleUSBXHCI[%p]::UIMEnableAllEndpoints- Unimplimented method called", this);
    return(kIOReturnInternalError);
}




IOReturn AppleUSBXHCI::UIMDeviceToBeReset(short functionAddress)
{
	int slotID;
	slotID = GetSlotID(functionAddress);
	
    if (slotID == 0)
	{
		USBLog(1, "AppleUSBXHCI[%p]::UIMDeviceToBeReset - unknown address2: %d", this, functionAddress);
		return(kIOReturnBadArgument);
	}
    
    // If PPT
    if ((_errataBits & kXHCIErrataPPT) != 0)
    {
        _slots[slotID].deviceNeedsReset = false;
    }
    
    USBLog(3, "AppleUSBXHCI[%p]::UIMDeviceToBeReset - fn:%d, slot:%d", this, (int)functionAddress, slotID);
    return(kIOReturnSuccess);
}

#pragma mark •••••••• Timeouts ••••••••

// Turn this on to log the outstanding TRBs.
#define PRINT_RINGS (0)

bool 
AppleUSBXHCI::checkEPForTimeOuts(int slot, int endp, UInt32 stream, UInt32 curFrame)
{
	int     enQueueIndex, deQueueIndex, ringSize, lastDeQueueIndex;
	Context	*epCtx;
	int 	epState;
	int 	epType;
	bool	stopped = false;
    bool	abortAll;
	
	XHCIRing *ring;
	
    // Streams fix needed here
    // USBTrace_Start(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, slot, endp, curFrame);
    
	ring = GetRing(slot, endp, stream);
	if(ring == NULL)
	{
		USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 0, ( (slot<<16)  | endp), 0);
		return(false);
	}
    
    abortAll = _slots[slot].deviceNeedsReset || !IsStillConnectedAndEnabled(slot);
	
	deQueueIndex     = ring->transferRingDequeueIdx;
	enQueueIndex     = ring->transferRingEnqueueIdx;
	ringSize         = ring->transferRingSize;
	lastDeQueueIndex = ring->lastSeenDequeIdx;

    USBLog(7, "AppleUSBXHCI[%p]::checkEPForTimeOuts - slot:%d ep:%d - eQIndex:%d, dQIndex:%d, lastDQIndex:%d, ringSize:%d", this, slot, endp, enQueueIndex, deQueueIndex, lastDeQueueIndex, ringSize);
    USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 1, ( (slot<<16)  | endp), (stream<<16) | abortAll);

    //
    // Pointer has not moved, (or has wrapped) since we looked last, and there's a TRB here
    // This check so we're not continually stopping endpoints
    //
    if (abortAll ||  ((lastDeQueueIndex == deQueueIndex) && (enQueueIndex != deQueueIndex)))
	{						
        AppleXHCIAsyncEndpoint *pAsyncEP   = OSDynamicCast(AppleXHCIAsyncEndpoint, (AppleXHCIAsyncEndpoint*)ring->pEndpoint);
        
		USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 2, ( (slot<<16)  | endp), lastDeQueueIndex);
		USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 3, enQueueIndex, deQueueIndex);
        if (pAsyncEP)
        {
			// If we have been disconnected/reset, we always need to abort the EPs
            if (!abortAll && !pAsyncEP->NeedTimeouts())
			{
				USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 4, ( (slot<<16)  | endp), 0);
                return false;
        	}
        }
        else
        {
			USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 5, ( (slot<<16)  | endp), 0);
            return false;
        }
        
		epCtx = GetEndpointContext(slot, endp);
		epType	= GetEpCtxEpType(epCtx);
		
		if (abortAll ||  (epType == kXHCIEpCtx_EPType_Control) || (epType == kXHCIEpCtx_EPType_BulkIN) || (epType == kXHCIEpCtx_EPType_BulkOut))
		{	
			//
			// Only Async endpoints timeout
			// We need to stop the endpoint to work on it safely, and to get intermediate progress information
			//
			// USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 6, ( (slot<<16)  | endp), 0);
            ClearStopTDs(slot, endp);
            
            AppleXHCIAsyncTransferDescriptor *pActiveATD = pAsyncEP->activeQueue;
            
            if (!pActiveATD)
            {
				USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 7, ( (slot<<16)  | endp), 0);
                return false;
            }
            
            UInt32 noDataTimeouts = pActiveATD->activeCommand->GetNoDataTimeout();
            
            if (noDataTimeouts != 0)
            {
                USBPhysicalAddress64 phys;
                epState = GetEpCtxEpState(epCtx);
                
                if (epState != kXHCIEpCtx_State_Running)
                {
                    // Probably stopped by timeout code previously, will start when doorbell rung
                    if( (epState != kXHCIEpCtx_State_Stopped) && (epState != kXHCIEpCtx_State_Disabled) )	
                    {
                        USBLog(2, "AppleUSBXHCI[%p]::checkEPForTimeOuts - EP State: %d, slot:%d, ep:%d (eQIndex:%d, dQIndex:%d, ringSize:%d)", 
                                                                                    this, epState, slot, endp, enQueueIndex, deQueueIndex, ringSize);
						USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 8, ( (slot<<16)  | endp), epState);
                        PrintContext(GetSlotContext(slot));
                        PrintContext((Context *)epCtx);
                    }
                }
                else
                {
                    if (abortAll || (stream == 0) )
                    {
                        USBLog(2, "AppleUSBXHCI[%p]::checkEPForTimeOuts - Stopping endpoint (%d, %d [%d]) (eQIndex:%d, dQIndex:%d, ringSize:%d)", 
                                                                                    this, slot, endp, (int)stream, enQueueIndex, deQueueIndex, ringSize);
 						USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 9, ( (slot<<16)  | endp), epState);
                        StopEndpoint(slot, endp);
                        stopped = true;
                    }
                    else
                    {
                        USBLog(2, "AppleUSBXHCI[%p]::checkEPForTimeOuts - Not stopping stream endpoint (%d, %d [%d]) (eQIndex:%d, dQIndex:%d, ringSize:%d)", 
                                                                                    this, slot, endp, (int)stream, enQueueIndex, deQueueIndex, ringSize);
 						USBTrace(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, 10, ( (slot<<16)  | endp), stream);
                    }
                }
            }
            
#if PRINT_RINGS
            {
                int deq1;
                deq1 = deq-1;
                if(deq1 < 0)
                {
                    deq1 = siz-1;
                }
                char buf[256];
                snprintf(buf, 256, "Index-: %d, c:%p phys:%08lx", deq1, (IOUSBCommandPtr)GetCommand(slot, endp, stream, deq1), (long unsigned int)(ring->transferRingPhys + deq1*sizeof(TRB)));
                PrintTRB(2, &ring->transferRing[deq1], buf);
            }
#endif
            pAsyncEP->UpdateTimeouts(abortAll, curFrame, stopped);
		}
	}
	else
	{
		ring->lastSeenDequeIdx = deQueueIndex;
	}
    
    // USBTrace_End(kUSBTXHCI, kTPXHCICheckEPForTimeOuts,  (uintptr_t)this, slot, endp, stopped);
    
	return (stopped);
}

void AppleUSBXHCI::CheckSlotForTimeouts(int slot, UInt32 curFrame)
{
	int endp;

	if( _slots[slot].buffer != NULL)
	{
		//USBLog(3, "AppleUSBXHCI[%p]::CheckSlotForTimeouts - slot:%d", this, slot);
		for(endp = 1; endp<kXHCI_Num_Contexts; endp++)
		{
			XHCIRing *ring;
			ring = GetRing(slot, endp, 0);
			if ( (ring != NULL) && (ring->TRBBuffer != NULL) )
			{
				// USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 5, slot, endp);
				if (!IsIsocEP(slot, endp))
				{
					if(!IsStreamsEndpoint(slot, endp))
					{
						// USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 6, slot, endp);
						if(checkEPForTimeOuts(slot, endp, 0, curFrame))
						{
							USBLog(2, "AppleUSBXHCI[%p]::CheckSlotForTimeouts - Starting XHCIRing (%d, %d)", this, slot, endp);
							USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 7, slot, endp);
							StartEndpoint(slot, endp);                               
						}
					}
					else
					{
                        UInt32 maxStream;
						bool stopped = false;
						maxStream = _slots[slot].maxStream[endp];
						
						// USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 8, ( (slot<<16)  | endp), maxStream);
#if PRINT_RINGS
                        USBLog(2, "AppleUSBXHCI[%p]::CheckSlotForTimeouts - streams endpoint (slot:%d, endp:%d, maxStream:%d)", this, (int)slot, (int)endp, (int)maxStream);
                        PrintContext(GetEndpointContext(slot, endp));
#endif
						for(UInt32 i = 1; i<=maxStream; i++)
						{
#if PRINT_RINGS
                            USBLog(2, "AppleUSBXHCI[%p]::CheckSlotForTimeouts - stream:%d ctx:%08lx, %08lx [%08lx, %08lx] (@%08lx)", this, (int)i, (long unsigned int)ring->transferRing[i].offs0, (long unsigned int)ring->transferRing[i].offs4, (long unsigned int)ring->transferRing[i].offs8, (long unsigned int)ring->transferRing[i].offsC, (long unsigned int)ring->transferRingPhys+i*sizeof(TRB));
#endif
							if(checkEPForTimeOuts(slot, endp, i, curFrame))
							{
								USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 9, ( (slot<<16)  | endp), i);
								stopped = true;
							}
						}
						
						if(stopped)
						{
							USBLog(7, "AppleUSBXHCI[%p]::CheckSlotForTimeout - restarting @%d, %d", this, slot, endp);
							USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 10, slot, endp);
							RestartStreams(slot, endp, 0);
						}
					}
				}
			}
		}
	}
}

void AppleUSBXHCI::UIMCheckForTimeouts(void)
{
	int slot, endp;
	TRB nextEvent;
	
	UInt32 curFrame = GetFrameNumber32();
    
    if((Read32Reg(&_pXHCIRegisters->USBSTS)& kXHCIHSEBit) != 0)
    {
        if(!_HSEReported)
        {
            USBError(1, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - HSE bit set:%x (1)", this, USBToHostLong(_pXHCIRegisters->USBSTS));
        }
        _HSEReported = true;
    }

	USBTrace_Start(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, _numInterrupts, _numPrimaryInterrupts, _numInactiveInterrupts);

    if(_lostRegisterAccess)
	{
		USBLog(3, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - Controller is not available - complete outstanding requests", this);
		
		for(slot = 0; slot<_numDeviceSlots; slot++)
		{
			CheckSlotForTimeouts(slot, 0);
		}
		
		
		if(_expansionData != NULL)
		{
			_watchdogTimerActive = false;
			if(_watchdogUSBTimer != NULL)
			{
				_watchdogUSBTimer->cancelTimeout();
			}
		}
		
		return;
	}

	// Return any transactions for a disconnected device before we check to see if the power is stable
	for(slot = 0; slot<_numDeviceSlots; slot++)
	{
		if ( !IsStillConnectedAndEnabled(slot) )
		{
			// This tracepoint is too verbose
			// USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 4, slot, curFrame);
			CheckSlotForTimeouts(slot, curFrame);
		}
	}
	
    if (_powerStateChangingTo != kUSBPowerStateStable && _powerStateChangingTo < kUSBPowerStateOn )
    {
        USBLog(6, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - Controller power state is not stable [going to sleep] (_powerStateChangingTo = %d, _myPowerState = %d), returning", this, (int)_powerStateChangingTo, (int)_myPowerState);
		USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 1, _powerStateChangingTo, _myPowerState);
        return;
    }
    
    if (_powerStateChangingTo != kUSBPowerStateStable )
    {
        USBLog(6, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - Controller power state is not stable [waking up] (_powerStateChangingTo = %d, _myPowerState = %d),", this, (int)_powerStateChangingTo, (int)_myPowerState);
		USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 2, _powerStateChangingTo, _myPowerState);
    }

    
	if (Read32Reg(&_pXHCIRuntimeReg->MFINDEX) == 0)
	{   // If controller is off because nothing is connected, MFINDEX is zero
		USBLog(7, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - MFINDEX is 0, not doing anything", this);
		USBTrace(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 3, _powerStateChangingTo, _myPowerState);
		return;
	}
	
	if (_lostRegisterAccess)
	{
		return;
	}
	
	
    
	//USBLog(3, "AppleUSBXHCI[%p]::UIMCheckForTimeouts - num interrupts: %d, num primary: %d, inactive:%d, unavailable:%d, is controller available:%d", this, (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable);
	for(slot = 0; slot<_numDeviceSlots; slot++)
	{
		CheckSlotForTimeouts(slot, curFrame);
	}
	
#if 0
	if(0)
	{
		PrintRuntimeRegs();
		Printinterrupter(5, 0, "UIMCheckForTimeouts");
	}
#endif
	
#if 0
    for(int IRQ = 0; IRQ < 2; IRQ++)
    {
        USBLog(3, "AppleUSBXHCI[%p]::UIMCheckForTimeouts IRQ:%d _EventRingDequeueIdx:%d, _EventRing2DequeueIdx:%d", this, IRQ, _events[IRQ].EventRingDequeueIdx, _events[IRQ].EventRing2DequeueIdx);
        nextEvent = _events[IRQ].EventRing[_events[IRQ].EventRingDequeueIdx];
        if( (USBToHostLong(nextEvent.offsC) & kXHCITRB_C) == _events[IRQ].EventRingCCS)
        {
            PrintTRB(3, &nextEvent, "UIMCheckForTimeouts EventRing");
        }
        nextEvent = _events[IRQ].EventRing2[_events[IRQ].EventRing2DequeueIdx];
        if(_events[IRQ].EventRing2DequeueIdx != _events[IRQ].EventRing2EnqueueIdx)
        {	// Queue not empty
            PrintTRB(3, &nextEvent, "UIMCheckForTimeouts EventRing2");
        }
    }
    
	PrintRuntimeRegs();
	Printinterrupter(3, kPrimaryinterrupter, "UIMCheckForTimeouts");
	Printinterrupter(3, kTransferinterrupter, "UIMCheckForTimeouts");
	// Intel wanted a dump of Config space, so this is here.
#if 0
	USBLog(3, "AppleUSBXHCI[%p]::Config Space:", this);
	for(int i = 0 ; i<256; i+=16)
	{
		UInt32 u1, u2, u3 ,u4;
		u1 = _device->configRead32(i);
		u2 = _device->configRead32(i+4);
		u3 = _device->configRead32(i+8);
		u4 = _device->configRead32(i+12);
		USBLog(3, "%02x: %08lx  %08lx   %08lx  %08lx", i, (long unsigned int)u1, (long unsigned int)u2, (long unsigned int)u3 ,(long unsigned int)u4);
        
	}
#endif
#endif
    // USBTrace_End(kUSBTXHCI, kTPXHCICheckForTimeouts,  (uintptr_t)this, 0, 0, 0);
}

// Get the actual device address the specified device is really using
// also swap your internal address from the current one to the real one

USBDeviceAddress
AppleUSBXHCI::UIMGetActualDeviceAddress(USBDeviceAddress current)
{
	UInt32 slotID;
    USBDeviceAddress actual;
 	slotID = GetSlotID(current);
    if(slotID == 0)
    {
        USBLog(1, "AppleUSBXHCI[%p]::UIMGetActualDeviceAddress - current address not found %d", this, current);
        return(0);
    }
	
	actual = GetSlCtxUSBAddress(GetSlotContext(slotID));

	if(actual != current)
    {
        if(_devEnabled[actual])
        {
            USBLog(1, "AppleUSBXHCI[%p]::UIMGetActualDeviceAddress - Actual mapping already enabled, returning mpped address (actual:%d, current:%d, map[act]:%d)", this, actual, current, _devMapping[actual]);
            return(current);
        }
        USBLog(6, "AppleUSBXHCI[%p]::UIMGetActualDeviceAddress - swapping map[%d] with map[%d]", this, actual, current);
        _devHub[actual] = _devHub[current] ;
        _devPort[actual] = _devPort[current];
        _devMapping[actual] = _devMapping[current];
        _devEnabled[actual] = _devEnabled[current];
        _devHub[current] = 0;
        _devPort[current] = 0;
        _devMapping[current] = 0;
        _devEnabled[current] = false;
        
    }
    else
    {
        USBLog(2, "AppleUSBXHCI[%p]::UIMGetActualDeviceAddress - address not changing %d", this, actual);
    }
	return actual;
}

#pragma mark •••••••• xMux and eMux Control ••••••••

IOReturn	
AppleUSBXHCI::HCSelectWithMethod ( char *muxMethod )
{
	IOReturn				status		= kIOReturnSuccess;
	UInt32					assertVal	= 0;
	
    if ( !_discoveredMuxedPorts )
    {
        DiscoverMuxedPorts();
    }
    
	if( _acpiDevice != NULL )
	{
        
		status = _acpiDevice->evaluateInteger ( muxMethod, &assertVal );
		
    }
	else 
	{
		status = kIOReturnUnsupported;
	}
    
    return status;
}

IOReturn	
AppleUSBXHCI::HCSelect ( UInt8 port, UInt8 controllerType )
{
	IOReturn				status		= kIOReturnNoMemory;
	
    if ( !_discoveredMuxedPorts )
    {
        DiscoverMuxedPorts();
    }

	USBLog(5, "AppleUSBXHCI[%p]::HCSelect for acpIDevice %p, port: %d, hcSelect: %d", this, _acpiDevice, port, controllerType);
    
	// We check to make sure the platform supports muxed ports 
	// port value passed here starts from 0...N 
	if( (_acpiDevice != NULL) && _muxedPorts && (port < kMaxHCPortMethods) )
	{
		if( controllerType == kControllerXHCI )
		{
			status = HCSelectWithMethod( (char*)xhciMuxedPorts[port] );
		}
		else if( controllerType == kControllerEHCI )
		{
			status = HCSelectWithMethod( ehciMuxedPorts[port] );		
        }

	}
    
	USBLog(5, "AppleUSBXHCI[%p]::HCSelect for acpIDevice %p, status 0x%x", this, _acpiDevice, status);
	
	return status;
}

//================================================================================================
//
//   DiscoverMuxedPorts
//
//   We look for EHCA...EHCx methods for each port in the XHC ACPI node and initialize 
//   ehciMuxedPorts array
//
//   xhciMuxedPorts is currently statically initialized. Future we can invent a method
//   to discover them dynamically.
//
//================================================================================================
//
bool 
AppleUSBXHCI::DiscoverMuxedPorts( )
{
	UInt8   port;
    UInt32  locationID;
    UInt8   totalMuxedPorts = 0; 
    
    if (_rootHubDeviceSS && !_muxedPorts && ((_errataBits & kXHCIErrataPPTMux) != 0) )
    {
        OSNumber *locationIDProperty = (OSNumber *)_rootHubDeviceSS->getProperty(kUSBDevicePropertyLocationID);
        if ( locationIDProperty )
        {
            locationID  = locationIDProperty->unsigned32BitValue();
            
            // We release _acpiDevice in UIMFinalize and DiscoverMuxedPorts will be called only once.
            _acpiDevice = CopyACPIDevice(_device);
            
            if (_acpiDevice)
            {
                for(port = 1; port < kMaxHCPortMethods; port++)
                {
                    if( IsPortMuxed(_device, port, locationID, ehciMuxedPorts[port-1]) )
                    {
                        USBLog(6, "AppleUSBXHCI[%p]::DiscoverMuxedPorts port %d method %s", this, port, ehciMuxedPorts[port-1]);
                        totalMuxedPorts++; 
                    }
                }
            }
        }
    }
    
    if( totalMuxedPorts > 0 )
    {
        _muxedPorts = true;
    }
    
    _discoveredMuxedPorts = true;
    
	USBLog(6, "AppleUSBXHCI[%p]::DiscoverMuxedPorts returning %s", this, _muxedPorts ? "TRUE" : "FALSE");
	
	return _muxedPorts;
}

bool 
AppleUSBXHCI::HasMuxedPorts( )
{
	UInt8 port = 0;
	bool  muxedPorts = true;
	
	USBLog(6, "AppleUSBXHCI[%p]::HasMuxedPorts (%p)", this, _acpiDevice);
    
	for(port = 0; (port < kMaxHCPortMethods) && (_muxedPorts == true) ; port++)
	{
		if ( _acpiDevice->validateObject ( xhciMuxedPorts[port] ) != kIOReturnSuccess )
		{
			muxedPorts = false;
		}
        
		if ( _acpiDevice->validateObject ( ehciMuxedPorts[port] ) != kIOReturnSuccess )
		{
			muxedPorts = false;
		}
	}
    
	USBLog(6, "AppleUSBXHCI[%p]::HasMuxedPorts returning %s", this, muxedPorts ? "TRUE" : "FALSE");
	
	return muxedPorts;
}

//================================================================================================
//
//   EnableXHCIPorts
//
//   We enable SS Terminations and make all muxes point to XHCI
//
//   Currently called from UIMInitialize and RestoreControllerStateFromSleep
//   We need to restore these settings coming ON after Hibernate.
//
//
//================================================================================================
//
void
AppleUSBXHCI::EnableXHCIPorts()
{

	if( (_errataBits & kXHCIErrataPPTMux) == 0)
	{
		goto Exit;
	}
	
	// Panther point, switch the mux.
	UInt32 HCSEL, HCSELM, USB3SSEN, USB3PRM;
	HCSEL = _device->configRead32(kXHCI_XUSB2PR);
	if (HCSEL == (UInt32)-1)
	{
		goto ControllerNotAvailable;
	}
	
	HCSELM = _device->configRead32(kXHCI_XUSB2PRM);
	if (HCSELM == (UInt32)-1)
	{
		goto ControllerNotAvailable;
	}
	
	USB3SSEN = _device->configRead32(kXHCI_XUSB3_PSSEN);
	if (USB3SSEN == (UInt32)-1)
	{
		goto ControllerNotAvailable;
	}
	
	USB3PRM	= _device->configRead32(kXHCI_XUSB3PRM);
	if (USB3PRM == (UInt32)-1)
	{
		goto ControllerNotAvailable;
	}
	
	// Now set the HCSEL to point to XHCI & Enable SS termination via USB3SSEN 
	if(HCSEL == 0)
	{
		_device->configWrite32(kXHCI_XUSB2PR, HCSELM);
	}
	
	if(USB3SSEN == 0)
	{
		_device->configWrite32(kXHCI_XUSB3_PSSEN, USB3PRM);
	}
	
	USBLog(3, "AppleUSBXHCI[%p]::EnableXHCIPorts - HCSEL: %lx, HCSELM: %lx USB3SSEN: %lx USB3PRM: %lx", 
		   this, (long unsigned int)HCSEL, (long unsigned int)HCSELM, (long unsigned int)USB3SSEN, (long unsigned int)USB3PRM);
	
Exit:
	
	return;
			
ControllerNotAvailable:
	
	_lostRegisterAccess = false;
}


#pragma mark •••••••• Test Mode ••••••••

enum
{
    kXHCIUSB2TestMode_Off		= 0,
    kXHCIUSB2TestMode_J_State	= 1,
    kXHCIUSB2TestMode_K_State 	= 2,
    kXHCIUSB2TestMode_SE0_NAK	= 3,
    kXHCIUSB2TestMode_Packet	= 4,
    kXHCIUSB2TestMode_ForceEnable	= 5,
    kEHCITestMode_Start		= 10,
    kEHCITestMode_End		= 11,
	kXHCIUSB2TestModeControlError = 15
};


void
AppleUSBXHCI::EnableComplianceMode()
{
    volatile UInt32 bits;
    
    if( ( ( (_errataBits & kXHCIErrataPPT) != 0 ) || ( (_errataBits & kXHCIErrata_FrescoLogic ) != 0 ) ) && ( (_errataBits & kXHCIErrata_EnableAutoCompliance) == 0 ) )
    {
        _pXHCIPPTChickenBits = (UInt32 *) ( ((uintptr_t)_pXHCICapRegisters) + 0x80EC);
        bits = *_pXHCIPPTChickenBits;
        bits &= ~kXHCIBit0;
        *_pXHCIPPTChickenBits = bits;
		
        USBLog(3, "AppleUSBXHCI[%p]::EnableComplianceMode - _pXHCIPPTChickenBits:%p, writing:%x", this, _pXHCIPPTChickenBits, (int)bits);
    }
}

void
AppleUSBXHCI::DisableComplianceMode()
{
    volatile UInt32 bits;
    
    if( ( ( (_errataBits & kXHCIErrataPPT) != 0 ) || ( (_errataBits & kXHCIErrata_FrescoLogic ) != 0 ) ) && ( (_errataBits & kXHCIErrata_EnableAutoCompliance) == 0 ) )
    {
        _pXHCIPPTChickenBits = (UInt32 *) ( ((uintptr_t)_pXHCICapRegisters) + 0x80EC);
        bits = *_pXHCIPPTChickenBits;
        bits |= kXHCIBit0;
        *_pXHCIPPTChickenBits = bits;
        USBLog(3, "AppleUSBXHCI[%p]::DisableComplianceMode - _pXHCIPPTChickenBits:%p, writing:%x", this, _pXHCIPPTChickenBits, (int)bits);
    }
    
}

// TestMode for USB 2 ports in XHCI
IOReturn
AppleUSBXHCI::UIMSetTestMode(UInt32 mode, UInt32 port)
{
    IOReturn ret = kIOReturnInternalError;
    
    USBLog(1, "AppleUSBXHCI[%p]::UIMSetTestMode(%d, %d)",  this, (int)mode, (int)port);
    
    switch (mode)
    {
		case kXHCIUSB2TestMode_Off:
		case kXHCIUSB2TestMode_J_State:
		case kXHCIUSB2TestMode_K_State:
		case kXHCIUSB2TestMode_SE0_NAK:
		case kXHCIUSB2TestMode_Packet:
		case kXHCIUSB2TestMode_ForceEnable:
			if (_testModeEnabled)
				ret = PlacePortInMode(port, mode);
			break;
			
		case kEHCITestMode_Start:
			ret = EnterTestMode();
			break;
			
		case kEHCITestMode_End:
			ret = LeaveTestMode();
			break;
    }
	
    return ret;
}


IOReturn
AppleUSBXHCI::EnterTestMode()
{
    UInt32		usbcmd, usbsts;
    UInt8		numPorts;
    int			port;
	IOReturn	status = kIOReturnSuccess;
	
    USBLog(1, "AppleUSBXHCI[%p]::EnterTestMode",  this);

    EnableComplianceMode();
    
	// TODO:: Disable all device slots
	
    // PP in PortSC registers should be '0' Disabled state
    numPorts = _rootHubNumPorts;
	
    USBLog(1, "AppleUSBXHCI[%p]::EnterTestMode - %d ports",  this, numPorts);
    
	for (port=0; port < numPorts; port++)
    {
		UInt32		portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		USBLog(1, "AppleUSBXHCI[%p]::EnterTestMode - portSC 0x%lx",  this, (long unsigned int)portSC);

		if ( (portSC & kXHCIPortSC_PP) != 0 )
		{
			// Set PP to be '0' = OFF
			XHCIRootHubPowerPort((port+1), false);
		}
    }
    
	// set run/stop
	status = StopUSBBus();
	
	if( status != kIOReturnSuccess )
		return status;
	
    _myBusState = kUSBBusStateReset;

    USBLog(1, "AppleUSBXHCI[%p]::EnterTestMode - HC halted - now in test mode",  this);
    
    _testModeEnabled = true;
    return kIOReturnSuccess;
}

IOReturn
AppleUSBXHCI::PlacePortInMode(UInt32 port, UInt32 mode)
{
    UInt32	portStat;
    UInt8	numPorts;
	UInt32	usbCmdReg;
	IOReturn status = kIOReturnInternalError;
    
    USBLog(1, "AppleUSBXHCI[%p]::PlacePortinMode(port %d, mode %d)",  this, (int)port, (int)mode);
	
    // see section 4.14 of the EHCI spec
    if (!_testModeEnabled)
    {
		USBLog(1, "AppleUSBXHCI[%p]::PlacePortinMode - ERROR test mode not enabled",  this);
		return status;
    }
	
	numPorts = _rootHubNumPorts;

    if (port >= numPorts)
    {
		USBLog(1, "AppleUSBXHCI[%p]::PlacePortinMode - ERROR invalid port %d",  this, (int)port);
		return status;
    }

	// Port adjustment applicable only for USB 2 ports. For SS RH Simulation the ports are mapped
	// to HS.
	AdjustRootHubPortNumbers(kUSBDeviceSpeedHigh, (UInt16*)&port);

	UInt32	portPMSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortPMSC);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}

	if (mode != kXHCIUSB2TestMode_ForceEnable)
	{
		// Confirm that controller is Halted for every other mode.
		status = StopUSBBus();
			
		if( status != kIOReturnSuccess )
			return status;
	}
	
    USBLog(1, "AppleUSBXHCI[%p]::PlacePortinMode - old portPMSC = %x",  this, (int)portPMSC);
    portPMSC &= ~kXHCIPortMSC_PortTestControl_Mask;
    portPMSC |= (mode << kXHCIPortMSC_PortTestControl_Shift);
    USBLog(1, "AppleUSBXHCI[%p]::PlacePortinMode - new portPMSC = %x",  this, (int)portPMSC);
    Write32Reg(&_pXHCIRegisters->PortReg[port].PortPMSC, portPMSC);
	
	// Start the controller running for Force Enable so we get SOFs.
	if (mode == kXHCIUSB2TestMode_ForceEnable)
	{
		usbCmdReg = Read32Reg(&_pXHCIRegisters->USBCMD);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		usbCmdReg |= kXHCICMDRunStop;
		
		Write32Reg(&_pXHCIRegisters->USBCMD, usbCmdReg);
	}

    return kIOReturnSuccess;
}


IOReturn
AppleUSBXHCI::LeaveTestMode()
{
	UInt32		CMD, count=0;
	int			i = 0;
	IOReturn	status = kIOReturnSuccess;
	
    USBLog(1, "AppleUSBXHCI[%p]::LeaveTestMode",  this);
    
    DisableComplianceMode();
    
	// Confirm that controller is Halted
	status = StopUSBBus();
	
	if( status != kIOReturnSuccess )
		return status;
	
    status = ResetController();
    
	if( status != kIOReturnSuccess )
		return status;

    _testModeEnabled = false;
    
    return status;
}

#pragma mark •••••••• IOKit Methods ••••••••

bool            
AppleUSBXHCI::terminate(IOOptionBits options)
{
    USBLog(3, "AppleUSBXHCI[%p]::terminate", this);
    
    _lostRegisterAccess = true;

    return super::terminate(options);
}

bool
AppleUSBXHCI::willTerminate(IOService *provider, IOOptionBits options)
{
	if(_expansionData != NULL)
	{
		if((_watchdogTimerActive == true) && (_watchdogUSBTimer != NULL))
		{
			_watchdogUSBTimer->setTimeoutUS(1);
		}
	}
	
	return super::willTerminate(provider, options);
}

#pragma mark •••••••• Utility Methods ••••••••

bool AppleUSBXHCI::CheckControllerAvailable(bool quiet)
{
    UInt32					CRCRLo;
	
	if (!_pXHCIRegisters)
		return false;
		
	if (_lostRegisterAccess)
		return false;
	
    CRCRLo = *(UInt32 *)&_pXHCIRegisters->CRCR;
    if(CRCRLo == kXHCIInvalidRegisterValue)
    {
        if(!quiet && !_lostRegisterAccess)
        {
            USBLog(2,"AppleUSBXHCI[%p]::CheckControllerAvailable CRCR invalid value: %x",  this, (int)CRCRLo );
        }
        _lostRegisterAccess = true;
		_controllerAvailable = false;
        return(false);
    }
    else
    {
        return(true);
    }
}
//================================================================================================
//   EndpointState
//================================================================================================
const char *	
AppleUSBXHCI::EndpointState(int state)
{
	switch (state)
	{
        case kXHCIEpCtx_State_Disabled:
            return "Disabled";
        case kXHCIEpCtx_State_Running:
            return "Running";
        case kXHCIEpCtx_State_Halted:
            return "Halted";
        case kXHCIEpCtx_State_Stopped:
            return "Stopped";
        case kXHCIEpCtx_State_Error:
            return "Error";

        default:
			return "Unknown EndpointState";
    }
    
}

//================================================================================================
//   TRBType
//================================================================================================
const char *	
AppleUSBXHCI::TRBType(int type)
{
	switch (type)
	{
        case kXHCITRB_Normal:
			return "Normal";
        case kXHCITRB_Setup:
			return "Setup Stage";
        case kXHCITRB_Data:
			return "Data Stage";
        case kXHCITRB_Status:
			return "Status Stage";
        case kXHCITRB_Isoc:
			return "Isoch";
        case kXHCITRB_Link:
			return "Link";
        case kXHCITRB_EventData:
			return "Event Data";
        case kXHCITRB_TrNoOp:
			return "No-Op";
        case kXHCITRB_EnableSlot:
			return "Enable Slot Command";
        case kXHCITRB_DisableSlot:
			return "Disable Slot Command";
        case kXHCITRB_AddressDevice:
			return "Address Device Command";
        case kXHCITRB_ConfigureEndpoint:
			return "Configure Endpoint Command";
        case kXHCITRB_EvaluateContext:
			return "Evaluate Context Command";
        case kXHCITRB_ResetEndpoint:
			return "Reset Endpoint Command";
        case kXHCITRB_StopEndpoint:
			return "Stop Endpoint Command";
        case kXHCITRB_SetTRDqPtr:
			return "Set TR Dequeue Pointer Command";
        case kXHCITRB_ResetDevice:
			return "Reset Device Command";
            
        case kXHCITRB_GetPortBandwidth:
            return "Get Port Bandwidth Command";
            
        case kXHCITRB_CMDNoOp:
			return "No Op Command";
        case kXHCITRB_TE:
			return "Transfer Event";
        case kXHCITRB_CCE:
			return "Command Completion Event";
        case kXHCITRB_PSCE:
			return "Port Status Change Event";
            
        case kXHCITRB_DevNot:
			return "Bandwidth Request Event";
        case kXHCITRB_MFWE:
			return "MFIndex Wrap Event";
        case kXHCITRB_NECCCE:
			return "NEC Vendor Defined Command";
        case kXHCITRB_CMDNEC:
			return "NEC Firmware Req Command";
            
        default:
			return "Unknown TRBType";
	}
}


//
// maxBurst and mult are zero based
//
AppleXHCIAsyncEndpoint*			
AppleUSBXHCI::AllocateAppleXHCIAsyncEndpoint(XHCIRing *ring, UInt32 maxPacketSize, UInt32 maxBurst, UInt32 mult)
{
	AppleXHCIAsyncEndpoint		*pEP;
    
	pEP = new AppleXHCIAsyncEndpoint;
	if (pEP)
	{
		if (!pEP->init(this, ring, maxPacketSize, maxBurst, mult))
		{
			pEP->release();
			pEP = NULL;
		}
    }
    
    USBLog(2,"-AppleUSBXHCI[%p]::AllocateAppleXHCIAsyncEndpoint %p maxPacketSize %d maxBurst %d mult %d",  this, pEP, (int)maxPacketSize, (int)maxBurst, (int)mult);

	return pEP;
}



IODMACommand*
AppleUSBXHCI::GetNewDMACommand()
{
	USBLog(7, "AppleUSBXHCI[%p]::GetNewDMACommand - creating %d bit IODMACommand", this, _AC64 ? 64 : 32);
	return IODMACommand::withSpecification(kIODMACommandOutputHost64, _AC64 ? 64 : 32, 0);
}


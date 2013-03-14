#include "AppleUSBXHCI_AsyncQueues.h"


#ifndef XHCI_USE_KPRINTF 
#define XHCI_USE_KPRINTF 0
#endif

#if XHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#if (DEBUG_REGISTER_READS == 1)
#define Read32Reg(registerPtr, ...) Read32RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
	#define Read32RegWithFileInfo(registerPtr, function, file, line, ...) (														\
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

OSDefineMetaClassAndStructors(AppleXHCIAsyncTransferDescriptor, OSObject)

AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncTransferDescriptor::ForEndpoint(AppleXHCIAsyncEndpoint *epQueue)
{
    AppleXHCIAsyncTransferDescriptor *me = OSTypeAlloc(AppleXHCIAsyncTransferDescriptor);
	
    if (!me || !me->init())
		return NULL;
	
	me->_endpoint = epQueue;
	
    return me;
}

bool
AppleXHCIAsyncTransferDescriptor::init()
{
	bool		ret;
	
	ret = OSObject::init();
    
    bzero(immediateBuffer, kMaxImmediateTRBTransferSize);

	return ret;
}


void 
AppleXHCIAsyncTransferDescriptor::reinit()
{
	activeCommand = NULL;	
	transferSize  = 0;		
	startOffset   = 0;		
	trbIndex      = 0;
	trbCount      = 0;
    
    interruptThisTD = 0;     
    fragmentedTD    = 0;  
    totalTDs        = 0;              
    offCOverride    = 0;
    shortfall       = 0;
    maxTRBs         = 0;  
    last            = false;
    streamID        = 0;
    completionIndex = 0;
    
    immediateTransfer = 0;
    flushed           = false;
    lastFlushedTD     = false;
    lastInRing        = false;
    remAfterThisTD    = 0;

    _logicalNext = NULL;				// the next element in the list
    bzero(immediateBuffer, kMaxImmediateTRBTransferSize);
    
}

void									
AppleXHCIAsyncTransferDescriptor::free(void)
{
	OSObject::free();
}

void
AppleXHCIAsyncTransferDescriptor::print(int level)
{
	USBLog(level, "AppleXHCIAsyncTransferDescriptor[%p]::print - activeCommand(%lx) transferSize(%d) startOffset(%d) trbIndex(%d) trbCount(%d) interruptThisTD(%d) ", 
                        this, (uintptr_t)activeCommand, (int)transferSize,  (int)startOffset, (int)trbIndex, (int)trbCount, (int)interruptThisTD );
    
	USBLog(level, "AppleXHCIAsyncTransferDescriptor[%p]::print - fragmentedTD(%d) totalTDs(%d) offCOverride(0x%08x) shortfall(%d) maxTRBs(%d) _logicalNext(%lx)", 
                        this, (int)fragmentedTD, (int)totalTDs, (uint32_t)offCOverride,  (int)shortfall, (int)maxTRBs, (uintptr_t)_logicalNext);
    
	USBLog(level, "AppleXHCIAsyncTransferDescriptor[%p]::print - last(%d) completionIndex(%d) streamID(%d) immediateTransfer(%d)", 
                        this, (int)last, (int)completionIndex, streamID, immediateTransfer); 
           
    USBLog(level, "AppleXHCIAsyncTransferDescriptor[%p]::print - flushed(%d) lastFlushedTD(%d) lastInRing(%d)", 
                        this, (int)flushed, (int)lastFlushedTD, (int)lastInRing);
}


OSDefineMetaClassAndStructors(AppleXHCIAsyncEndpoint, OSObject)


#pragma mark •••••••• Queue Management ••••••••

void 
AppleXHCIAsyncEndpoint::validateLists(int level)
{
    AppleXHCIAsyncTransferDescriptor *freeATD = freeQueue;
    int count = 0;
    
    while (freeATD)
    {
        count++;
        if (freeATD == freeEnd)
        	break;

        freeATD = (AppleXHCIAsyncTransferDescriptor*)freeATD->_logicalNext;
    }

    USBLog(level, "AppleXHCIAsyncEndpoint[%p]::validateLists freeQueue count: %d ",  this, count );
}

void
AppleXHCIAsyncEndpoint::print(int level)
{
    USBLog(level, "AppleXHCIAsyncEndpoint[%p]::print - ring(%lx) readyQueue(%lx) readyEnd(%lx) onReadyQueue(%d)", 
           this, (uintptr_t)_ring, (uintptr_t)readyQueue, (uintptr_t)readyEnd, (int)onReadyQueue);
    
    USBLog(level, "AppleXHCIAsyncEndpoint[%p]::print - activeQueue(%lx) activeEnd(%lx) onActiveQueue(%d)", 
           this, (uintptr_t)activeQueue, (uintptr_t)activeEnd, (int)onActiveQueue);
    
    USBLog(level, "AppleXHCIAsyncEndpoint[%p]::print - doneQueue(%lx) doneEnd(%lx) onDoneQueue(%d)", 
           this, (uintptr_t)doneQueue, (uintptr_t)doneEnd, (int)onDoneQueue);

    USBLog(level, "AppleXHCIAsyncEndpoint[%p]::print - freeQueue(%lx) freeEnd(%lx) onFreeQueue(%d)", 
           this, (uintptr_t)freeQueue, (uintptr_t)freeEnd, (int)onFreeQueue);
    
	USBLog(level, "AppleXHCIAsyncEndpoint[%p]::print - aborting(%d) maxPacketSize(%d) maxBurst(%d) actualFragmentSize(%d)", 
           this, (int)_aborting, (int)_maxPacketSize, (int)_maxBurst, (int)_actualFragmentSize);
}

bool
AppleXHCIAsyncEndpoint::init()
{
    bool		ret;

	ret = OSObject::init();

    return ret;
}

bool
AppleXHCIAsyncEndpoint::init(AppleUSBXHCI *controller, XHCIRing *pRing, UInt16 maxPacketSize, UInt16 maxBurst, UInt16 mult)
{
	bool		ret = init();
	
    if(!ret)
    {
        return false;
    }
    
    _xhciUIM        = controller;
    _ring           = pRing;
    _maxPacketSize  = maxPacketSize;
    _maxBurst       = maxBurst;
    _mult           = mult;
    
    UInt32 maxBurstPayload      = _maxPacketSize*(_maxBurst+1)*(_mult+1);
    UInt32 numberOfMaxBursts    = kAsyncMaxFragmentSize/maxBurstPayload;
    _actualFragmentSize         = numberOfMaxBursts * maxBurstPayload;

	return ret;
}


void									
AppleXHCIAsyncEndpoint::free(void)
{
    USBLog(7,"+AppleXHCIAsyncEndpoint[%p]::free",  this );

    _aborting            = true;
    _ring->beingReturned = true;

    MoveAllTDsFromReadyQToDoneQ();
    
    // If TDs are in Done Queue then complete only the last fragment
    if (onDoneQueue)
    {
        USBLog(1,"AppleXHCIAsyncEndpoint[%p]::free %d ATDs in doneQueue returning as 0x%08x",  this, (int)onDoneQueue, kIOReturnAborted );
        Complete(kIOReturnAborted);
    }
    
    do
    {
        AppleXHCIAsyncTransferDescriptor *freeATD = GetTDFromFreeQueue(false);
        
        if (freeATD)
        {
            USBLog(7,"+AppleXHCIAsyncEndpoint[%p]::free freeATD: %p ",  this, freeATD );

            freeATD->release();
            freeATD = NULL;
        }
        
    } while (freeQueue != NULL);
    
    print(7);

    USBLog(7,"-AppleXHCIAsyncEndpoint[%p]::free",  this );
    
    _aborting            = false;
    _ring->beingReturned = false;
    
	OSObject::free();
}

void 
AppleXHCIAsyncEndpoint::PutTDAtHead(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, AppleXHCIAsyncTransferDescriptor *pTD, UInt32 *qCount)
{
    // Link TD into queue
    if (*qStart == NULL)
    {
		// as the head of a new queue and new qEnd
		*qEnd = *qStart = pTD;
    }
    else
    {
		// at the head of the old queue
        pTD->_logicalNext = *qStart;
    }
    
    // no matter what we are the new head
    *qStart = pTD;
	(*qCount)++;
}

void 
AppleXHCIAsyncEndpoint::PutTD(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, AppleXHCIAsyncTransferDescriptor *pTD, UInt32 *qCount)
{
    // Link TD into queue
    if (*qStart == NULL)
    {
		// as the head of a new queue
		*qStart = pTD;
    }
    else
    {
		// at the tail of the old queue
		(*qEnd)->_logicalNext = pTD;
    }
    
    // no matter what we are the new tail
    *qEnd = pTD;
	(*qCount)++;
}

AppleXHCIAsyncTransferDescriptor * 
AppleXHCIAsyncEndpoint::GetTD(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, UInt32 *qCount)
{
    AppleXHCIAsyncTransferDescriptor	*pTD;
    
    pTD = *qStart;
    if (pTD)
    {
		if (pTD == *qEnd)
			*qStart = *qEnd = NULL;
		else
			*qStart = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pTD->_logicalNext);
        
        if (*qCount == 0)
        {
		    USBLog(1,"AppleXHCIAsyncEndpoint[%p]::GetTD underflow",  this);
		    print(5);
        }
        
		(*qCount)--;
    }
    
    return pTD;
}

void
AppleXHCIAsyncEndpoint::PutTDonFreeQueue(AppleXHCIAsyncTransferDescriptor *pTD)
{
    PutTD(&freeQueue, &freeEnd, pTD, &onFreeQueue);
}

AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncEndpoint::GetTDFromFreeQueue(bool allocate)
{
    if (freeQueue == NULL && allocate)
    {
        // Allocate TDs and add them to FreeQueue
        for (int i=0; i < kFreeTDs; i++)
        {
            AppleXHCIAsyncTransferDescriptor *newATD = AppleXHCIAsyncTransferDescriptor::ForEndpoint(this);
            if (newATD)
            {
                PutTDonFreeQueue(newATD);
            }
        }
    }

    AppleXHCIAsyncTransferDescriptor *pFreeATD = GetTD(&freeQueue, &freeEnd, &onFreeQueue);
    
    if (pFreeATD)
    {
        pFreeATD->reinit();
    }
    
    return pFreeATD;
}

void 
AppleXHCIAsyncEndpoint::PutTDonReadyQueueAtHead(AppleXHCIAsyncTransferDescriptor *pTD)
{
    PutTDAtHead(&readyQueue, &readyEnd, pTD, &onReadyQueue);
}

void
AppleXHCIAsyncEndpoint::PutTDonReadyQueue(AppleXHCIAsyncTransferDescriptor *pTD)
{
    PutTD(&readyQueue, &readyEnd, pTD, &onReadyQueue);
}

AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncEndpoint::GetTDFromReadyQueue()
{
    return GetTD(&readyQueue, &readyEnd, &onReadyQueue);
}

void
AppleXHCIAsyncEndpoint::PutTDonDoneQueue(AppleXHCIAsyncTransferDescriptor *pTD)
{
	if ((doneQueue != NULL) && (doneEnd == NULL))
	{
		AppleXHCIAsyncTransferDescriptor *lastTD = doneQueue;

		UInt32 count = 0;
		while (lastTD->_logicalNext != NULL)
		{
			if (count++ > onDoneQueue)
			{
			    USBLog(1,"AppleXHCIAsyncEndpoint[%p]::PutTDonDoneQueue doneEnd not found",  this);
			    print(5);
				lastTD = NULL;
				break; 
			}
			lastTD = (AppleXHCIAsyncTransferDescriptor*)lastTD->_logicalNext;
		}

		doneEnd = lastTD;
	}
	
    PutTD(&doneQueue, &doneEnd, pTD, &onDoneQueue);
}

AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncEndpoint::GetTDFromDoneQueue()
{
    return GetTD(&doneQueue, &doneEnd, &onDoneQueue);
}

void
AppleXHCIAsyncEndpoint::PutTDonActiveQueue(AppleXHCIAsyncTransferDescriptor *pTD)
{
    PutTD(&activeQueue, &activeEnd, pTD, &onActiveQueue);
}

AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncEndpoint::GetTDFromActiveQueue()
{
    return GetTD(&activeQueue, &activeEnd, &onActiveQueue);
}

AppleXHCIAsyncTransferDescriptor * 
AppleXHCIAsyncEndpoint::GetTDFromActiveQueueWithIndex(UInt16 completionIndex)
{
    AppleXHCIAsyncTransferDescriptor *pActiveATD, *pPrevActiveATD;

    bool    foundMatchingTD      = false;
    
    pPrevActiveATD = pActiveATD = activeQueue;
    
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::GetTDFromActiveQueueWithIndex trbIndex: %d", this, completionIndex);
    
    while (pActiveATD != NULL)
    {
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::GetTDFromActiveQueueWithIndex ATD: %p USBCommand: %p completionIndex: ( %d , %d )", 
               this, pActiveATD, pActiveATD->activeCommand, (int)pActiveATD->completionIndex, (int)completionIndex);
        
        if (completionIndex == pActiveATD->completionIndex) 
        {                        
            if (pActiveATD == activeEnd)
            {
                // End
                if (activeQueue == activeEnd)
                {
                    activeQueue = activeEnd = NULL;
                }
                else
                {
                    activeEnd = pPrevActiveATD;
                }
            }
            else if (pActiveATD == activeQueue)
            {
                // Start
                activeQueue = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pActiveATD->_logicalNext);
            }
            else
            {
                // Chain previous to the next and disconnect the active one.
                pPrevActiveATD->_logicalNext = pActiveATD->_logicalNext;
            }
            
            foundMatchingTD = true;
            onActiveQueue--;
            break;
        }
        
        pPrevActiveATD  = pActiveATD;
        // next item
        pActiveATD      = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pActiveATD->_logicalNext);
    }
    
    if (!foundMatchingTD)
    {
        pActiveATD = NULL;
	    USBLog(1, "AppleXHCIAsyncEndpoint[%p]::GetTDFromActiveQueueWithIndex not found ActiveTD @index: %d", this, completionIndex);
        print(1);
    }
    
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::GetTDFromActiveQueueWithIndex activeQueue %p pActiveATD %p foundMatchingTD: %d", this, activeQueue, pActiveATD, foundMatchingTD);
    
    return pActiveATD;
}

void 
AppleXHCIAsyncEndpoint::MoveAllTDsFromReadyQToDoneQ()
{
    do
    {
        AppleXHCIAsyncTransferDescriptor *pReadyATD = GetTDFromReadyQueue();
        
        if (pReadyATD)
        {
            PutTDonDoneQueue(pReadyATD);
        }   
        
    } while (readyQueue != NULL);
}

void 
AppleXHCIAsyncEndpoint::MoveAllTDsFromActiveQToDoneQ()
{
    do
    {
        AppleXHCIAsyncTransferDescriptor *pActiveATD = GetTDFromActiveQueue();
        
        if (pActiveATD)
        {
            PutTDonDoneQueue(pActiveATD);
        }   

    } while (activeQueue != NULL);
}

void 
AppleXHCIAsyncEndpoint::MoveTDsFromReadyQToDoneQ(IOUSBCommand *pUSBCommand)
{
    do
    {
        AppleXHCIAsyncTransferDescriptor *pReadyATD = GetTDFromReadyQueue();
        
        if(pReadyATD)
        {
            // If command matches then drain only matching ReadyATDs
            if (pUSBCommand)
            {
                if (pReadyATD->activeCommand == pUSBCommand)
                {
                    PutTDonDoneQueue(pReadyATD);
                }
                else
                {
                    // We have drained all ReadyATDs with matching pUSBCommand
                    // Put back the ReadyTD
                    PutTDonReadyQueueAtHead(pReadyATD);
                    break;
                }
            }
            else
            {
                PutTDonDoneQueue(pReadyATD);
            }
        }   
        
    } while (readyQueue != NULL);
}

#if 0

//
// TODO :: Remove this method
//
AppleXHCIAsyncTransferDescriptor *
AppleXHCIAsyncEndpoint::FindNearByActiveTD(int deQueueIndex)
{       
    AppleXHCIAsyncTransferDescriptor *pActiveATD = activeQueue;
    bool foundNearByATD = false;
    
    while (pActiveATD != NULL)
    {
        int diffIndex    = 0;
        
        if (deQueueIndex > (int)pActiveATD->completionIndex)
        {
            diffIndex = deQueueIndex - pActiveATD->completionIndex;
            diffIndex = (ring->transferRingSize-1) - diffIndex; 
        }
        else if (deQueueIndex < (int)pActiveATD->completionIndex)
        {
            diffIndex = pActiveATD->completionIndex - deQueueIndex;
        }    
        
        if (diffIndex <= (int)pActiveATD->trbCount)
        {
            foundNearByATD = true;
            break;
        }
        
        // next item
        pActiveATD      = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pActiveATD->_logicalNext);
    }
    
    if (!foundNearByATD)
    {
        pActiveATD = NULL;
    }
    
    return pActiveATD;
}

#endif

#define PRINT_RINGS 0

#pragma mark •••••••• ATDs Management ••••••••
//
//  Create ATDs by chunking the IOUSBCommand and add them to the readyQueue
//
IOReturn    
AppleXHCIAsyncEndpoint::CreateTDs(IOUSBCommand *command, UInt16 streamID, UInt32 offsC, UInt8 immediateTransferSize, UInt8 *immediateBuffer)
{
    AppleXHCIAsyncTransferDescriptor    *pNewATD            = NULL;
    IOByteCount                         totalTransferSize   = command->GetReqCount();
    UInt32                              numberOfTDs         = kMinimumTDs;
    IOByteCount                         transferThisTD      = 0;
    bool                                fragmentedTDs       = true;
    bool                                immediateTransfer   = false;
    bool                                interruptNeeded     = false;
    IOByteCount                         fragmentSize        = 0, residual = 0, sizeQueued = 0, transferOffset = 0; 

    if (_aborting)
    {
        USBLog(3, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }

#if PRINT_RINGS
    {
        USBLog(1, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - ring->transferRingPCS %d", this, (int)ring->transferRingPCS);
        xhciUIM->PrintContext(GetEndpointContext(ring->slotID, ring->endpointID));
        // print(5);
    }
#endif
    
    if ((command->GetReqCount() > 0) && (!command->GetDMACommand() || !command->GetDMACommand()->getMemoryDescriptor()))
    {
        USBError(1, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - no DMA Command or missing memory descriptor", this);
        return kIOReturnBadArgument;
    }
    
    USBLog(7, "+AppleXHCIAsyncEndpoint[%p]::CreateTDs - command (%p)", this, command);
    
    USBTrace_Start( kUSBTXHCI, kTPXHCIAsyncEPCreateTD, (uintptr_t)this, (uintptr_t)command, (uintptr_t)totalTransferSize, (uintptr_t)offsC );

    command->SetUIMScratch(kXHCI_ScratchShortfall, 0);

    //
    // immediateTransferSize > 0 or == 0 then we have an immediateTransfer
    if (immediateTransferSize <= 8)
    {
        fragmentSize      = immediateTransferSize;
        totalTransferSize = immediateTransferSize;
        immediateTransfer = true;
        fragmentedTDs     = false;
    }
    else
    {   
        // kAsyncMaxFragmentSize per TD
        fragmentSize = _actualFragmentSize;  
        
        // No fragments
        if (totalTransferSize <= fragmentSize)
        {  
            fragmentSize    = totalTransferSize;
            fragmentedTDs   = false;
        }
    }
    
    // Fragmentation starts here
    transferThisTD	= fragmentSize;
    residual		= totalTransferSize;
    
    //
    // Format all the TDs, attach them to the pseudo endpoint. 
    // We will set the IOC bit on the last TD of the transaction
    //
    while (TRUE)
    {
        interruptNeeded = false;
        
        //
        // Get a TD from the free Q
        pNewATD = GetTDFromFreeQueue();
        
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - pNewATD(%p)", this, pNewATD);
        
        if (pNewATD == NULL)
        {
            USBLog(1,"AppleXHCIAsyncEndpoint[%p]::CreateTDs - Could not allocate a new pNewATD queued: %d", this, (int)numberOfTDs);
            print(5);
            return kIOReturnNoMemory;
        }
        
        //
        // set IOC bit for each fragment
        if ((sizeQueued % _actualFragmentSize) == 0)
        {
            interruptNeeded = true;
        }

        pNewATD->activeCommand      = command;
        pNewATD->interruptThisTD    = interruptNeeded;
        pNewATD->fragmentedTD       = fragmentedTDs;
        pNewATD->startOffset        = transferOffset;
        pNewATD->transferSize       = (UInt32)transferThisTD;
        pNewATD->offCOverride       = offsC;
        pNewATD->maxTRBs            = (transferThisTD/PAGE_SIZE) + kAccountForAlignment;
        pNewATD->streamID           = streamID;
        pNewATD->shortfall          = pNewATD->transferSize;
        
        if (immediateTransfer)
        {
            pNewATD->immediateTransfer  = immediateTransfer;
            bcopy(immediateBuffer, &pNewATD->immediateBuffer, immediateTransferSize);
        }        
                  
        // Finally, put the TD on the Ready Queue
        //
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - putting pNewATD(%p) on ReadyQueue transferThisTD: %d residual: %d transferOffset: %d", 
                                        this, pNewATD, (int)transferThisTD, (int)residual, (int)transferOffset);
        
        PutTDonReadyQueue(pNewATD);

        // print(5);
        
        // Calculate next fragment
        sizeQueued += transferThisTD;
        if (sizeQueued == totalTransferSize)
        {
            USBLog(7, "AppleXHCIAsyncEndpoint[%p]::CreateTDs - (%d, %d) putting pNewATD(%p) on ReadyQueue command: %p streamID: %d sizeQueued: %d totalTransferSize: %d", 
                                        this, _ring->slotID, _ring->endpointID, pNewATD, command, pNewATD->streamID, (int)sizeQueued, (int)totalTransferSize);
            USBTrace(kUSBTXHCI, kTPXHCIAsyncEPCreateTD, (uintptr_t)this, _ring->slotID, _ring->endpointID, 0);
            USBTrace(kUSBTXHCI, kTPXHCIAsyncEPCreateTD, (uintptr_t)this, streamID, fragmentedTDs, 1);
            USBTrace(kUSBTXHCI, kTPXHCIAsyncEPCreateTD, (uintptr_t)this, onReadyQueue, onActiveQueue, 2);
            break;		
        }
        
        transferOffset = transferOffset + transferThisTD;
        
        // Figure out transferThisTD for the next time around the loop
        if (residual > transferThisTD)
        {
            transferThisTD = residual - transferThisTD;
        }
        else
        {
            transferThisTD = transferThisTD - residual;
        }
        
        // Figure out residual
        if (transferThisTD >= fragmentSize)
        {
            residual = transferThisTD;
            transferThisTD = fragmentSize;
        }
        else
        {
            residual = transferThisTD;		
        }
        //
        // count keeps track of total TDs 
        numberOfTDs++;
		pNewATD->remAfterThisTD = (UInt32)residual;
    }

    pNewATD->totalTDs        = numberOfTDs;
    
    // Always interrupt on the last TD but only on fragments
    pNewATD->interruptThisTD = true;
    pNewATD->last            = true;
	pNewATD->remAfterThisTD  = 0;
    
    USBTrace_End( kUSBTXHCI, kTPXHCIAsyncEPCreateTD, (uintptr_t)this, numberOfTDs, sizeQueued, (uintptr_t)0);
 
    USBLog(7, "-AppleXHCIAsyncEndpoint[%p]::CreateTDs", this);

    return kIOReturnSuccess;
}


//
//  Schedule the ATDs from readyQueue to the ring and add them to the HW ring
//  Called from ScavengeTDs & CreatTransfer
//
void    
AppleXHCIAsyncEndpoint::ScheduleTDs()
{
    IOReturn      status = kIOReturnSuccess;

    USBLog(7, "+AppleXHCIAsyncEndpoint[%p]::ScheduleTDs", this);
    
    USBTrace_Start( kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, _aborting, _ring->beingReturned, onReadyQueue );

    if (readyQueue == NULL)
    {
		USBLog(7, "AppleXHCIAsyncEndpoint[%p]::Schedule - readyQueue is empty slot:%d epID:%d", this, _ring->slotID, _ring->endpointID);
		return;
    }

	if (_aborting)
    {
        if(_ring->beingReturned)
        {
            _ring->needsDoorbell = true;
        }
		USBLog(1, "AppleXHCIAsyncEndpoint[%p]::Schedule - aborting - not adding", this);
		return;
    }

    do
    {
        bool	spaceAvailable;
        UInt16  spaceForTD;
        
        spaceAvailable = _xhciUIM->CanTDFragmentFit(_ring, _actualFragmentSize);
        
        if (!spaceAvailable )
        {
            USBLog(7, "AppleXHCIAsyncEndpoint[%p]::Schedule - no more space available on Xfer Ring", this);
            USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, spaceAvailable, onReadyQueue, 0);
            // print(5);
            break;
        }
                
        AppleXHCIAsyncTransferDescriptor *pReadyATD = GetTDFromReadyQueue();
        
        if (pReadyATD)
        {            
            status = _xhciUIM->_createTransfer(pReadyATD, 
                                              false,
                                              pReadyATD->transferSize, 
                                              pReadyATD->offCOverride,
                                              pReadyATD->startOffset, 
                                              pReadyATD->interruptThisTD, 
                                              pReadyATD->fragmentedTD,                                          
                                              &pReadyATD->trbIndex, 
                                              &pReadyATD->trbCount,
                                              false,
                                              &pReadyATD->completionIndex);

            if (status != kIOReturnSuccess)
            {
                USBLog(1, "AppleXHCIAsyncEndpoint[%p]::Schedule - returned 0x%x", this, status);
                USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, onReadyQueue, status, 3);
                PutTDonReadyQueueAtHead(pReadyATD);
                break;
            }
            else
            {
                USBLog(7, "AppleXHCIAsyncEndpoint[%p]::Schedule - (%d, %d) ATD: %p USBCommand: %p transferSize: %5d completionIndex: %d", 
                                this, _ring->slotID, _ring->endpointID, pReadyATD, pReadyATD->activeCommand, (int)pReadyATD->transferSize, (int)pReadyATD->completionIndex);
                
                PutTDonActiveQueue(pReadyATD);
                
                USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, _ring->slotID, _ring->endpointID, 4);
                USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, (uintptr_t)pReadyATD, (uintptr_t)pReadyATD->activeCommand, pReadyATD->completionIndex);
                
                if(_ring->beingReturned)
                {
                    _ring->needsDoorbell = true;
                }
                else
                {
                    _xhciUIM->StartEndpoint(_ring->slotID, _ring->endpointID, pReadyATD->streamID);
                }
            }
        }
        
    } while (readyQueue != NULL);
    
    USBTrace_End( kUSBTXHCI, kTPXHCIAsyncEPScheduleTD, (uintptr_t)this, (uintptr_t)onReadyQueue, (uintptr_t)onActiveQueue, (uintptr_t)onDoneQueue );
    
    USBLog(7, "-AppleXHCIAsyncEndpoint[%p]::ScheduleTDs", this);
    
    return;
}

#if 0
void
AppleXHCIAsyncEndpoint::FlushTDs(IOUSBCommandPtr pUSBCommand)
{
    AppleXHCIAsyncTransferDescriptor *pActiveATD;
    
    pActiveATD = activeQueue;
    
    if (pUSBCommand == NULL)
    {
        USBLog(5, "AppleXHCIAsyncEndpoint[%p]::FlushTDs - pUSBCommand: %p Unexpected", this, pUSBCommand);
        return;
    }
    
    if (pActiveATD == NULL)
    {
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDs - activeQueue empty", this);
        return;
    }
    
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDs - pUSBCommand: %p", this, pUSBCommand);
    
    while (pActiveATD != NULL)
    {
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDs - ATD: %p USBCommand: %p onActiveQueue: %d", this, pActiveATD, pActiveATD->activeCommand, (int)onActiveQueue);
        
        if ((pUSBCommand == pActiveATD->activeCommand) && (pActiveATD->flushed == false))
        {
            pActiveATD->flushed = true;
            
            if (pActiveATD == activeEnd)
            {
                pActiveATD->lastFlushedTD = true;
            }
        }
        pActiveATD      = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pActiveATD->_logicalNext);
    }
    
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDs - Done", this);
}
#endif 

void 
AppleXHCIAsyncEndpoint::FlushTDsWithStatus(IOUSBCommandPtr pUSBCommand, IOReturn status)
{
#pragma unused(status)
    
    int     currentIndex = 0;
    int     flushedDequeueIndex = 0;
    int     dequeueStreamID = 0;
    bool    updateDequeueIndex = false;

    AppleXHCIAsyncTransferDescriptor *pActiveATD;
    
    pActiveATD = activeQueue;
    
    //
    // Remember the enqueIndex index now, transfers should not be added
    int enqueueIndex = _ring->transferRingEnqueueIdx;
    
    if (pUSBCommand == NULL)
    {
        USBLog(5, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - pUSBCommand: %p Unexpected", this, pUSBCommand);
        return;
    }
    
    if (pActiveATD == NULL)
    {
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - activeQueue empty", this);
        return;
    }
    
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - pUSBCommand: %p", this, pUSBCommand);
    
    while (pActiveATD != NULL)
    {
        USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - ATD: %p USBCommand: %p onActiveQueue: %d", this, pActiveATD, pActiveATD->activeCommand, (int)onActiveQueue);
        
        if (pUSBCommand == pActiveATD->activeCommand)
        {
            if (pActiveATD == activeEnd)
            {
                // End
                activeQueue = activeEnd = NULL;
            }
            else if (pActiveATD == activeQueue)
            {
                // Start
                activeQueue = OSDynamicCast(AppleXHCIAsyncTransferDescriptor, pActiveATD->_logicalNext);
            }
            
            flushedDequeueIndex = pActiveATD->completionIndex+1;
            dequeueStreamID     = pActiveATD->streamID;
            updateDequeueIndex  = true;
            
            if (flushedDequeueIndex >= (_ring->transferRingSize-1))
            {
                flushedDequeueIndex    =  0;
            }
            
            PutTDonDoneQueue(pActiveATD);
            if(onActiveQueue == 0)
            {
                USBLog(5, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - activeQueue underflow", this);
                print(5);
            }
            onActiveQueue--;
            
            // Start from head again
            pActiveATD = activeQueue;
            continue;
        }
        else
        {
            USBLog(5, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - TDs still scheduled for other USBCommands enqueueIndex %d flushedDequeueIndex %d", this, enqueueIndex, flushedDequeueIndex);
            break;
        }
    }
    
    if (updateDequeueIndex)
    {
        _xhciUIM->SetTRDQPtr(_ring->slotID, _ring->endpointID, dequeueStreamID, flushedDequeueIndex);
    }
 
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::FlushTDsWithStatus - Done enqueueIndex %d flushedDequeueIndex %d", this, enqueueIndex, flushedDequeueIndex);
}

//
//  Move ATDs from HW ring -> doneQueue & readyQueue -> doneQueue
//
void    
AppleXHCIAsyncEndpoint::ScavengeTDs(AppleXHCIAsyncTransferDescriptor *pActiveATD, IOReturn status, bool complete, bool flush)
{
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::ScavengeTDs - (%d, %d) ATD: %p USBCommand: %p status: %x complete: %d flush: %d", 
                            this, _ring->slotID, _ring->endpointID, pActiveATD, pActiveATD->activeCommand, status, complete, flush);
    
    USBTrace_Start( kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, _ring->slotID, _ring->endpointID, status );
    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, (uintptr_t)pActiveATD, (uintptr_t)pActiveATD->activeCommand, 0);
    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, complete, flush, 1);

    // TDs are off the hardware
    PutTDonDoneQueue(pActiveATD);
    
    //
    // Walk readyQueue and activeQueue, move TDs matching pUSBCommand to doneQueue 
    if (flush)
    {
        bool flushReadyQueue = true;
        
        // Stop the endpoint
        _xhciUIM->QuiesceEndpoint(_ring->slotID, _ring->endpointID);
        
        FlushTDsWithStatus(pActiveATD->activeCommand, status);
        
        MoveTDsFromReadyQToDoneQ(pActiveATD->activeCommand);
        
        // Start the endpoint
        if (activeQueue)
        {
            if(_xhciUIM->IsStreamsEndpoint(_ring->slotID, _ring->endpointID))
            {
                _xhciUIM->StartEndpoint(_ring->slotID, _ring->endpointID); 
            }
            else
            {
                _xhciUIM->RestartStreams(_ring->slotID, _ring->endpointID, 0);
            }
        }
    }
    
    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, onReadyQueue, onActiveQueue, 2);
    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, onDoneQueue, onFreeQueue, 3);

    //
    // Complete with status
    if (complete)
    {
        Complete(status);
    }
    
    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, onDoneQueue, onFreeQueue, 4);
    
    //
    // Schedule new TDs
    ScheduleTDs();
        
    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::ScavengeTDs", this);
    
    USBTrace_End(kUSBTXHCI, kTPXHCIAsyncEPScavengeTD, (uintptr_t)this, onReadyQueue, onActiveQueue, onFreeQueue);
    
    return;
}

//
//  Complete the ATDs that are in the doneQueue
//
void    
AppleXHCIAsyncEndpoint::Complete(IOReturn status)
{
    IOByteCount  shortfall = 0;
    
    do
    {
        AppleXHCIAsyncTransferDescriptor *pDoneATD = GetTDFromDoneQueue();
        
        if (pDoneATD)
        {
            shortfall  = pDoneATD->activeCommand->GetUIMScratch(kXHCI_ScratchShortfall);

            shortfall += pDoneATD->shortfall;
            
            pDoneATD->activeCommand->SetUIMScratch(kXHCI_ScratchShortfall, (UInt32)shortfall);
            
            // pDoneATD->print(5);

            // Last fragment in the TD so call the completion
            if (pDoneATD->interruptThisTD && pDoneATD->last)
            {
                // print(5);
                
                shortfall = pDoneATD->activeCommand->GetUIMScratch(kXHCI_ScratchShortfall);
                
                IOUSBCompletion completion = pDoneATD->activeCommand->GetUSLCompletion();
                
                if (completion.action != NULL)
                {
#if DEBUG_BUFFER
                    xhciUIM->CheckBuf(pDoneATD->activeCommand);
#endif                    
                    USBLog(7, "AppleXHCIAsyncEndpoint[%p]::Complete - (%d, %d) completionIndex: %4d command: %p status: 0x%08x bytes: %d shortfall: %d enQ: %d deQ: %d", 
                                                    this, _ring->slotID, _ring->endpointID, (int)pDoneATD->completionIndex , 
                                                    pDoneATD->activeCommand, status, (int)(pDoneATD->activeCommand->GetReqCount()- shortfall), (int)shortfall, _ring->transferRingEnqueueIdx, _ring->transferRingDequeueIdx);
                    USBTrace_Start(kUSBTXHCI, kTPXHCIAsyncEPComplete,  (uintptr_t)this, _ring->slotID, _ring->endpointID, status );
                    USBTrace(kUSBTXHCI, kTPXHCIAsyncEPComplete,  (uintptr_t)this, (uintptr_t)pDoneATD, (uintptr_t)pDoneATD->activeCommand, (uint32_t)pDoneATD->completionIndex );
                    USBTrace_End(kUSBTXHCI, kTPXHCIAsyncEPComplete,  (uintptr_t)this, (uint32_t)(pDoneATD->activeCommand->GetReqCount()- shortfall), _ring->transferRingEnqueueIdx, _ring->transferRingDequeueIdx);
                    
                    _xhciUIM->Complete(completion, status, (UInt32)shortfall);
                    pDoneATD->shortfall = 0;
                }
                
                pDoneATD->interruptThisTD = false;
            }
            PutTDonFreeQueue(pDoneATD);
        }

    } while (doneQueue != NULL);
    
    return;
}



//
// Abort the ATDs starting from ring -> doneQueue and readyQueue -> doneQueue
// 
// Called from UIMDeleteEndpoint
// Called from ReturnAllTransfers
//
IOReturn    
AppleXHCIAsyncEndpoint::Abort()
{
    USBTrace_Start( kUSBTXHCI, kTPXHCIAsyncEPAbort, (uintptr_t)this, onReadyQueue, onActiveQueue, onDoneQueue );

    USBLog(7, "+AppleXHCIAsyncEndpoint[%p]::Abort", this);
    
    IOReturn status = kIOReturnAborted;
    
    _aborting            = true;
    _ring->beingReturned = true;
    
    if (_xhciUIM->_slots[_ring->slotID].deviceNeedsReset == true)
    {
        status = kIOReturnNotResponding;
    }
    
    //
    // Abort only commands that are in the ring which belong in
    // the activeQueue and readyQueue.
    //
    // Leave the new commands that are in the readyQueue intact.
    // They will be scheduled from Reinittransferring
    //
    AppleXHCIAsyncTransferDescriptor *pActiveATD = activeQueue;
    
    while (pActiveATD != NULL)
    {
        IOUSBCommandPtr  pUSBCommand = pActiveATD->activeCommand;

        USBTrace(kUSBTXHCI, kTPXHCIAsyncEPAbort,  (uintptr_t)this, (uintptr_t)pActiveATD, (uintptr_t)pActiveATD->activeCommand, (uint32_t)pActiveATD->completionIndex );
        
        FlushTDsWithStatus(pUSBCommand, status);

        MoveTDsFromReadyQToDoneQ(pUSBCommand);

        pActiveATD = activeQueue;
    }
	
    _ring->beingReturned = false;
    _aborting = false;
    
    // If TDs are in Done Queue then complete only the last fragment
    if (onDoneQueue)
    {
        Complete(status);
    }

    USBLog(7, "-AppleXHCIAsyncEndpoint[%p]::Abort", this);

    USBTrace_End( kUSBTXHCI, kTPXHCIAsyncEPAbort, (uintptr_t)status, onReadyQueue, onActiveQueue, onFreeQueue );
    
    return kIOReturnSuccess;
}

bool
AppleXHCIAsyncEndpoint::NeedTimeouts()
{
    AppleXHCIAsyncTransferDescriptor *pActiveATD = activeQueue;
    
    if (pActiveATD)
    {
        IOUSBCommandPtr pUSBCommand = pActiveATD->activeCommand;
                
        if (pUSBCommand)
        {
            // Don't have no data timeouts for streams endpoints
            UInt32 noDataTimeout        = pUSBCommand->GetNoDataTimeout();
            UInt32 completionTimeout    = pUSBCommand->GetCompletionTimeout();
    
            
            if ((pActiveATD->streamID != 0) || ((noDataTimeout >= completionTimeout) && (completionTimeout > 0)) )
            {
                // Don't have no data timeouts for streams endpoints
                // or if the no data timeout is the same as (or longer than) the completion timeout
                pActiveATD->activeCommand->SetNoDataTimeout(0);
                noDataTimeout = 0;
            }
            
            if ((completionTimeout == 0) && (noDataTimeout == 0))
            {
                return false;
            }
        }
    }
    
    return true;
}

//
// Walk the activeQueue and Update the timeout for the activeCommands in the TDs
// 
void
AppleXHCIAsyncEndpoint::UpdateTimeouts(bool abortAll, UInt32 curFrame, bool stopped) 
{
	int         enQueueIndex, deQueueIndex, stopDeq = -2;
    bool        returnATransfer = false;
	UInt32      shortFall = 0;
    IOReturn    status = kIOUSBTransactionTimeout;
    
    AppleXHCIAsyncTransferDescriptor *pActiveATD = NULL;
    USBPhysicalAddress64 physAddress;

    USBLog(7, "+AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts", this);

    deQueueIndex = _ring->transferRingDequeueIdx;	// Read again, just in case a TD completed
    enQueueIndex = _ring->transferRingEnqueueIdx;
    
    
    TRB *stopTRB = &_ring->stopTRB;
    
    physAddress = (USBToHostLong(stopTRB->offs0) & ~0xf) + (((UInt64)USBToHostLong(stopTRB->offs4)) << 32);
    shortFall   = USBToHostLong(stopTRB->offs8) & kXHCITRB_TR_Len_Mask;

    if (physAddress == 0)
    {
        stopDeq = deQueueIndex;
        _xhciUIM->PrintTRB(7, stopTRB, "Stop TRBx");
    }
    else
    {
        stopDeq = _xhciUIM->DiffTRBIndex(physAddress, _ring->transferRingPhys);
        _xhciUIM->PrintTRB(7, stopTRB, "Stop TRB");
    }

    USBTrace_Start( kUSBTXHCI, kTPXHCIAsyncEPUpdateTimeout, (uintptr_t)this, enQueueIndex, deQueueIndex, shortFall);
    
#if PRINT_RINGS
    {
        USBLog(2, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts PRINT_RINGS - stopDeq: %d (eQIndex:%d, dQIndex:%d) shortFall %d", this, stopDeq, enQueueIndex, deQueueIndex, (int)shortFall);
        _xhciUIM->PrintRing(ring);
    }
#endif
    
    //
    // We should find one always to update the timeout from the head
    // 
    pActiveATD = activeQueue;
    
    
    if (pActiveATD && abortAll)
    {
        USBLog(2, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - Reset needed or unplugged, aborting transactions stopDeq: %d (eQIndex:%d, dQIndex:%d)", this, stopDeq, enQueueIndex, deQueueIndex);
        pActiveATD->shortfall = pActiveATD->transferSize - shortFall;
        status                = kIOReturnNotResponding;
        returnATransfer       = true;
    }
    else if (pActiveATD)
    {
        USBLog(5, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - stopDeq: %d (eQIndex:%d, dQIndex:%d) shortFall: %d", this, stopDeq, enQueueIndex, deQueueIndex, (int)shortFall);

#if PRINT_RINGS
        {
            xhciUIM->PrintContext(GetEndpointContext(ring->slotID,ring->endpointID);
            pActiveATD->print(5);
            print(5);
        }
#endif
        
        IOUSBCommandPtr pUSBCommand = pActiveATD->activeCommand;
        
#if PRINT_RINGS
        {
            char buf[256];
            snprintf(buf, 256, "Index: %d, usbCommand:%p phys:%08lx", deQueueIndex, pUSBCommand, (long unsigned int)(ring->transferRingPhys + deQueueIndex*sizeof(TRB)));
            xhciUIM->PrintTRB(5, &ring->transferRing[deQueueIndex], buf);
        }
#endif
        
        if (pUSBCommand)
        {
            // Don't have no data timeouts for streams endpoints
            UInt32 noDataTimeout        = pUSBCommand->GetNoDataTimeout();
            UInt32 completionTimeout    = pUSBCommand->GetCompletionTimeout();
            
            UInt32 firstSeen;
            UInt32 bytesTransferred, TRTime;
            int    savedStopDeq;
            
            firstSeen = pUSBCommand->GetUIMScratch(kXHCI_ScratchFirstSeen);
            
            if (firstSeen == 0)
            {
                pUSBCommand->SetUIMScratch(kXHCI_ScratchFirstSeen, curFrame);
                firstSeen = curFrame;
            }
            
            if ((completionTimeout != 0) && ((curFrame - firstSeen) >  completionTimeout))
            {
                // return the transacton here.
                USBLog(2, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - Found transaction past its timeout, returning transaction. %d - %d > %d", this, (int)curFrame, (int)firstSeen, (int)completionTimeout);
                pActiveATD->shortfall   = pActiveATD->transferSize - shortFall;
                returnATransfer         = true;
            }
            
            if (noDataTimeout != 0)
            {
                savedStopDeq        = pUSBCommand->GetUIMScratch(kXHCI_ScratchStopDeq);
                bytesTransferred    = pUSBCommand->GetUIMScratch(kXHCI_ScratchBytes);
                TRTime              = pUSBCommand->GetUIMScratch(kXHCI_ScratchTRTime);
                
                if ((TRTime             == 0) ||        // Unintialised, first note of time
                    (savedStopDeq       != stopDeq) ||  // xHC is making progress since last time we checked
                    (bytesTransferred   != shortFall))  // Some data has transferred since last time we checked
                {	
                    // Make a note of current transfer state, TRB pointer, bytes to do
                    USBLog(5, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - Init no data timeout, stopDeq:%d, SaveStopDeq:%d, bytes:%d, time:%d", this, stopDeq, savedStopDeq, (int)shortFall, (int)curFrame);
                    pUSBCommand->SetUIMScratch(kXHCI_ScratchStopDeq, stopDeq);
                    pUSBCommand->SetUIMScratch(kXHCI_ScratchBytes, shortFall);
                    pUSBCommand->SetUIMScratch(kXHCI_ScratchTRTime, curFrame);
                }
                else
                {	
                    // Has not moved since it was seen last
                    if((curFrame - TRTime) >  noDataTimeout) 
                    {
                        USBLog(2, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - Found transaction past its no data timeout, returning transaction. %d - %d > %d", this, (int)curFrame, (int)TRTime, (int)noDataTimeout);
                        // Lets update the shortfall
                        pActiveATD->shortfall = pActiveATD->transferSize - pUSBCommand->GetUIMScratch(kXHCI_ScratchBytes);
                        returnATransfer       = true;
                    }
                }
            }

            USBTrace(kUSBTXHCI, kTPXHCIAsyncEPUpdateTimeout, (uintptr_t)pActiveATD, (uintptr_t)pActiveATD->activeCommand, (int)noDataTimeout, (int)(curFrame - firstSeen) );
            
            USBLog(3, "AppleXHCIAsyncEndpoint[%p]::UpdateTimeouts - command: %p, completionTimeout: %d, noDataTimeout: %d diffTime: %d", this, pUSBCommand, (int)completionTimeout, (int)noDataTimeout, (int)(curFrame - firstSeen));
        }
    }

    USBTrace_End( kUSBTXHCI, kTPXHCIAsyncEPUpdateTimeout, (uintptr_t)this, (uintptr_t)pActiveATD, (uintptr_t)returnATransfer, (uintptr_t)0 );
    
    if (returnATransfer)
    {
        if (_xhciUIM->_slots[_ring->slotID].deviceNeedsReset == true)
        {
            status = kIOReturnNotResponding;
        }
        
        int  timedOutDequeueIndex = pActiveATD->completionIndex+1;

        if (timedOutDequeueIndex >= (_ring->transferRingSize-1))
        {
            timedOutDequeueIndex    =  0;
        }
        
        //
        // Endpoint will be stopped only if there is valid no data timeout.
        // For completion timeout make sure we stop the endpoint before
        // we call SetTRDQPtr
        // ScavengeTDs will restart the endpoint if there are activeTDs 
        // in the activeQueue so we should be safe.
        if (stopped == false)
        {
            _xhciUIM->QuiesceEndpoint(_ring->slotID, _ring->endpointID);
        }
        
        _xhciUIM->SetTRDQPtr(_ring->slotID, _ring->endpointID, pActiveATD->streamID, timedOutDequeueIndex);
        
        //  Scavenge the readyQueue -> doneQueue and complete
        //
        ScavengeTDs(pActiveATD, status, true, true);
    }
}

#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCIedMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCIedMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCIedMemoryBlock*
AppleEHCIedMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIedMemoryBlock 		*me = new AppleEHCIedMemoryBlock;
    IOByteCount				len;
    
    if (!me)
	USBError(1, "AppleEHCIedMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCIedMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    USBLog(7, "AppleEHCIedMemoryBlock::NewMemoryBlock, sizeof (me) = %d, sizeof (super) = %d", sizeof(AppleEHCIedMemoryBlock), sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (EHCIQueueHeadSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleEHCIedMemoryBlock::NumEDs(void)
{
    return EDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIedMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < EDsPerBlock)
	ret = _sharedPhysical + (index * sizeof(EHCIQueueHeadShared));
    return ret;
}


EHCIQueueHeadSharedPtr
AppleEHCIedMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCIQueueHeadSharedPtr ret = NULL;
    if (index < EDsPerBlock)
	ret = &_sharedLogical[index];
    return ret;
}


AppleEHCIedMemoryBlock*
AppleEHCIedMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIedMemoryBlock::SetNextBlock(AppleEHCIedMemoryBlock* next)
{
    _nextBlock = next;
}
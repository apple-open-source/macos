#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCIitdMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCIitdMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCIitdMemoryBlock*
AppleEHCIitdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIitdMemoryBlock 					*me = new AppleEHCIitdMemoryBlock;
    IOByteCount							len;
    
    if (!me)
	USBError(1, "AppleEHCIitdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCIitdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    me->prepare();
    me->_sharedLogical = (EHCIIsochTransferDescriptorSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleEHCIitdMemoryBlock::NumTDs(void)
{
    return ITDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIitdMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < ITDsPerBlock)
	ret = _sharedPhysical + (index * sizeof(EHCIIsochTransferDescriptorShared));
    return ret;
}


EHCIIsochTransferDescriptorSharedPtr
AppleEHCIitdMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCIIsochTransferDescriptorSharedPtr ret = NULL;
    if (index < ITDsPerBlock)
	ret = &_sharedLogical[index];
    return ret;
}


AppleEHCIitdMemoryBlock*
AppleEHCIitdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIitdMemoryBlock::SetNextBlock(AppleEHCIitdMemoryBlock* next)
{
    _nextBlock = next;
}
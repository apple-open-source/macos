#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCIsitdMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCIsitdMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCIsitdMemoryBlock*
AppleEHCIsitdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIsitdMemoryBlock 					*me = new AppleEHCIsitdMemoryBlock;
    IOByteCount							len;
    
    if (!me)
	USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCIsitdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    me->prepare();
    me->_sharedLogical = (EHCISplitIsochTransferDescriptorSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleEHCIsitdMemoryBlock::NumTDs(void)
{
    return SITDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIsitdMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < SITDsPerBlock)
	ret = _sharedPhysical + (index * sizeof(EHCISplitIsochTransferDescriptorShared));
    return ret;
}


EHCISplitIsochTransferDescriptorSharedPtr
AppleEHCIsitdMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCISplitIsochTransferDescriptorSharedPtr ret = NULL;
    if (index < SITDsPerBlock)
	ret = &_sharedLogical[index];
    return ret;
}


AppleEHCIsitdMemoryBlock*
AppleEHCIsitdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIsitdMemoryBlock::SetNextBlock(AppleEHCIsitdMemoryBlock* next)
{
    _nextBlock = next;
}
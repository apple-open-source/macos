#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCItdMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCItdMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCItdMemoryBlock 			*me = new AppleEHCItdMemoryBlock;
    EHCIGeneralTransferDescriptorSharedPtr	sharedPtr;
    IOByteCount					len;
    IOPhysicalAddress				sharedPhysical;
    UInt32					i;
    
    if (!me)
	USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    me->prepare();
    sharedPtr = (EHCIGeneralTransferDescriptorSharedPtr)me->getBytesNoCopy();
    sharedPhysical = me->getPhysicalSegment(0, &len);
    
    for (i=0; i < TDsPerBlock; i++)
    {
	me->_TDs[i].pPhysical = sharedPhysical+(i * sizeof(EHCIGeneralTransferDescriptorShared));
	me->_TDs[i].pShared = &sharedPtr[i];
    }
    
    return me;
}



UInt32
AppleEHCItdMemoryBlock::NumTDs(void)
{
    return TDsPerBlock;
}



EHCIGeneralTransferDescriptorPtr
AppleEHCItdMemoryBlock::GetTD(UInt32 index)
{
    return (index < TDsPerBlock) ? &_TDs[index] : NULL;
}



AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCItdMemoryBlock::SetNextBlock(AppleEHCItdMemoryBlock* next)
{
    _nextBlock = next;
}
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "AppleUSBEHCI.h"
#include "USBEHCI.h"

class AppleEHCItdMemoryBlock : public IOBufferMemoryDescriptor
{
    OSDeclareDefaultStructors(AppleEHCItdMemoryBlock);
    
#define TDsPerBlock	(kEHCIPageSize / sizeof(EHCIGeneralTransferDescriptorShared))

private:
    EHCIGeneralTransferDescriptor	_TDs[TDsPerBlock];
    IOPhysicalAddress			_sharedMem;
    AppleEHCItdMemoryBlock		*_nextBlock;
    
public:

    static AppleEHCItdMemoryBlock 	*NewMemoryBlock(void);
    UInt32				NumTDs(void);
    EHCIGeneralTransferDescriptorPtr	GetTD(UInt32 index);
    void				SetNextBlock(AppleEHCItdMemoryBlock *next);
    AppleEHCItdMemoryBlock		*GetNextBlock(void);
    
};
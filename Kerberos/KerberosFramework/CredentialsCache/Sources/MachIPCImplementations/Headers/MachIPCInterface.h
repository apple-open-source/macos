#pragma once

#include <mach/message.h>
#include <mach/mach_init.h>

#include "CCache.MachIPC.h"

template <class T>
class CCIMachIPCServerBuffer {
    public:
        CCIMachIPCServerBuffer (
            mach_msg_type_number_t	inCount):
            mData (NULL),
            mSize (0)
        {
            kern_return_t err = vm_allocate (mach_task_self (), (vm_address_t*)&mData, inCount * sizeof (T), true);
            ThrowIfIPCAllocateFailed_ (mData, err);
            mSize = inCount * sizeof (T);
        }
        
        T* Data () { return mData; }
        mach_msg_type_number_t Size () { return mSize; }
        
    private:
        T*			mData;
        mach_msg_type_number_t	mSize;
        
        CCIMachIPCServerBuffer (const CCIMachIPCServerBuffer&);
        CCIMachIPCServerBuffer& operator = (const CCIMachIPCServerBuffer&);
};


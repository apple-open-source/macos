/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*!
 * @header DSTCPEndian
 * Provides routines to byte swap DSProxy buffers.
 */

#ifndef __DSTCPEndian_h__
#define __DSTCPEndian_h__ 1

#include <machine/byte_order.h>

#include "SharedConsts.h"	// for sComProxyData
#include "DirServicesConst.h"


class DSTCPEndian
{
public:
    DSTCPEndian(sComProxyData* message, int direction);
    
    void SwapMessage(void);
    
    enum { kSwapToBig, kSwapToHost };

private:
    void SwapObjectData(uInt32 type, char* data, uInt32 size, bool swapAuth);
    
    void SwapStandardBuf(char* data, uInt32 size);
    void SwapRecordEntry(char* data, uInt32 type);

    uInt32 GetLong(void* ptr)
    {
        uInt32 returnVal = *(uInt32*)(ptr);
        if (!toBig)
            returnVal = NXSwapBigLongToHost(returnVal);
            
        return returnVal;
    }

    uInt16 GetShort(void* ptr)
    {
        uInt16 returnVal = *(uInt16*)(ptr);
        if (!toBig)
            returnVal = NXSwapBigShortToHost(returnVal);
            
        return returnVal;
    }
    
    uInt32 GetAndSwapLong(void* ptr)
    {
        uInt32 returnVal = *(uInt32*)(ptr);
        if (toBig)
            *(uInt32*)(ptr) = NXSwapHostLongToBig(returnVal);
        else
            returnVal = *(uInt32*)(ptr) = NXSwapBigLongToHost(returnVal);
            
        return returnVal;
    }

    uInt16 GetAndSwapShort(void* ptr)
    {
        uInt16 returnVal = *(uInt16*)(ptr);
        if (toBig)
            *(uInt16*)(ptr) = NXSwapHostShortToBig(returnVal);
        else
            returnVal = *(uInt16*)(ptr) = NXSwapBigShortToHost(returnVal);
            
        return returnVal;
    }
    
    void SwapLong(void* ptr) { GetAndSwapLong(ptr); }
    void SwapShort(void* ptr) { GetAndSwapShort(ptr); }

    sComProxyData* fMessage;
    bool toBig;
};

#endif
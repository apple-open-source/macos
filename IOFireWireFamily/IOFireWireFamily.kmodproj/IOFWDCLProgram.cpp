/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFWDCLProgram.h>

OSDefineMetaClass( IODCLProgram, OSObject )
OSDefineAbstractStructors(IODCLProgram, OSObject)
OSMetaClassDefineReservedUnused(IODCLProgram, 0);
OSMetaClassDefineReservedUnused(IODCLProgram, 1);
OSMetaClassDefineReservedUnused(IODCLProgram, 2);
OSMetaClassDefineReservedUnused(IODCLProgram, 3);

bool IODCLProgram::init(IOFireWireBus::DCLTaskInfo *info)
{
    bool ok;
    ok = OSObject::init();
    if(!ok || info == NULL)
	return ok;
    do {
	// Have to map DCL as read/write because timestamp opcode writes
	// into the DCL.
        fDCLDesc = IOMemoryDescriptor::withAddress(info->fDCLBaseAddr,
		info->fDCLSize, kIODirectionOutIn, info->fTask);
        if(!fDCLDesc) {
            ok = false;
            break;
        }
        fDataDesc = IOMemoryDescriptor::withAddress(info->fDataBaseAddr,
                info->fDataSize, kIODirectionOutIn, info->fTask);
        if(!fDataDesc) {
            ok = false;
            break;
        }
	// 6250 is the total bandwidth per frame at 400Mb/sec, seems a reasonable limit!
        fDataCursor = IONaturalMemoryCursor::withSpecification(PAGE_SIZE, 6250);
        if(!fDataCursor) {
            ok = false;
            break;
        }
        vm_address_t kernelDCL;
        IOReturn res;
        IOByteCount len;
        res = fDCLDesc->prepare(kIODirectionOutIn);
        if(res != kIOReturnSuccess) {
            ok = false;
            break;
	}
        kernelDCL = (vm_address_t)fDCLDesc->getVirtualSegment(0, &len);
        assert(len >= info->fDCLSize);
        fDCLTaskToKernel = kernelDCL - info->fDCLBaseAddr;
        res = fDataDesc->prepare(kIODirectionOutIn);
        if(res != kIOReturnSuccess) {
            ok = false;
            break;
        }
        fCallUser = info->fCallUser;
        fCallRefCon = info->fCallRefCon;
        fDataBase = info->fDataBaseAddr;
    } while (false);
    if(!ok) {
	if(fDCLDesc)
            fDCLDesc->release();
        if(fDataDesc)
            fDataDesc->release();
        if(fDataCursor)
            fDataCursor->release();
    }
    return ok;
}

void IODCLProgram::free()
{
    if(fDCLDesc) {
        fDCLDesc->complete(kIODirectionOutIn);
	fDCLDesc->release();
    }
    if(fDataDesc) {
        fDataDesc->complete(kIODirectionOutIn);
	fDataDesc->release();
    }
    if(fDataCursor)
        fDataCursor->release();
    OSObject::free();
}

UInt32 IODCLProgram::getPhysicalSegs(void *addr, IOByteCount len,
	IOMemoryCursor::PhysicalSegment segs[], UInt32 maxSegs)
{
    UInt32 nSegs;
    if(fDataDesc && fDataCursor) {
        nSegs = fDataCursor->genPhysicalSegments(fDataDesc, (IOByteCount)addr - fDataBase, segs, maxSegs, len);
    }
    else {
	UInt32 i;
        vm_address_t pos;
        pos = (vm_address_t)addr;
        nSegs = (round_page(pos+len) - trunc_page(pos))/(PAGE_SIZE);
	if (nSegs > maxSegs) {
            IOLog("IODCLProgram::getPhysicalSegs(): Data descriptor too complex for compiler!\n");
            nSegs = 0;
        }
        for(i = 0; i<nSegs; i++) {
            IOByteCount segLen;
            segs[i].location = pmap_extract(kernel_pmap, pos);
            segLen = PAGE_SIZE - (pos & (PAGE_SIZE - 1));
            if(segLen > len)
                segLen = len;
            segs[i].length = segLen;
            pos += segLen;
            len -= segLen;
	}
    }
    return nSegs;
}

void IODCLProgram::dumpDCL(DCLCommand *op)
{
    while(op) {
        UInt32		opcode;
        IOLog("(0x%p)", op);
        op = convertDCLPtrToKernel(op);
        // Dispatch off of opcode.
        opcode = op->opcode & ~kFWDCLOpFlagMask;
        IOLog("Opcode 0x%p:", op);
        switch(opcode) {
            case kDCLReceivePacketStartOp :
            {
                DCLTransferPacketPtr t = (DCLTransferPacketPtr) op;

                IOLog("ReceivePacketStartDCL to 0x%p, size %ld", t->buffer, t->size);
                break;
            }
            case kDCLReceivePacketOp :
            {
                DCLTransferPacketPtr t = (DCLTransferPacketPtr) op;

                IOLog("ReceivePacketDCL to 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketStartOp :
            {
                DCLTransferPacketPtr t = (DCLTransferPacketPtr) op;

                IOLog("SendPacketStartDCL from 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketWithHeaderStartOp :
            {
                DCLTransferPacketPtr t = (DCLTransferPacketPtr) op;

                IOLog("SendPacketWithHeaderStartDCL from 0x%px, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketOp :
            {
                DCLTransferPacketPtr t = (DCLTransferPacketPtr) op;

                IOLog("SendPacketDCL from 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLCallProcOp :
            {
                DCLCallProcPtr t = (DCLCallProcPtr) op;

                IOLog("CallProcDCL calling 0x%p(0x%lx)", t->proc, t->procData);
                break;
            }
            case kDCLJumpOp :
                IOLog("JumpDCL to 0x%p", ((DCLJumpPtr)op)->pJumpDCLLabel);
                break;

            case kDCLLabelOp :
                IOLog("LabelDCL");
                break;

            case kDCLSetTagSyncBitsOp :
                IOLog("SetTagSyncBitsDCL");
                break;

            case kDCLUpdateDCLListOp :
            {
                unsigned int i;
                DCLUpdateDCLListPtr t = (DCLUpdateDCLListPtr) op;
                DCLCommandPtr *p = t->dclCommandList;
                IOLog("updateDCLListDCL:");
                for(i=0; i<t->numDCLCommands; i++)
                    IOLog("0x%p ", *p++);
                break;
            }

            case kDCLTimeStampOp :
                IOLog("timeStampDCL");
                break;
            default: IOLog("Unknown opcode %ld", opcode);
                break;
        }
        IOLog("\n");
        op = op->pNextDCLCommand;
    }
}

IOReturn IODCLProgram::pause()
{
    return kIOReturnSuccess;
}

IOReturn IODCLProgram::resume()
{
    return kIOReturnSuccess;
}



/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
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
    
	if( !ok )
		return ok;
	
	if( info != NULL )
	{
		do 
		{
			IOReturn res;
			// Have to map DCL as read/write because timestamp opcode writes
			// into the DCL.
			if(info->fDCLBaseAddr) 
			{
				fDCLDesc = IOMemoryDescriptor::withAddress(info->fDCLBaseAddr,
				info->fDCLSize, kIODirectionOutIn, info->fTask);
				if(!fDCLDesc) 
				{
					ok = false;
					break;
				}
				res = fDCLDesc->prepare(kIODirectionOutIn);
				if(res != kIOReturnSuccess) 
				{
					ok = false;
					break;
				}
				fDCLTaskToKernel = NULL;
			}
			
			if(info->fDataBaseAddr) 
			{
				// Horrible hack!!!!
				if(info->fCallRefCon) 
				{
					fDataDesc = (IOMemoryDescriptor *)info->fCallRefCon;
					fDataDesc->retain();
				}
				else
				{
					fDataDesc = IOMemoryDescriptor::withAddress(	info->fDataBaseAddr,
																	info->fDataSize, 
																	kIODirectionOutIn, 
																	info->fTask );
				}
				
				if( !fDataDesc ) 
				{
					ok = false;
					break;
				}
				
				res = fDataDesc->prepare( kIODirectionOutIn );
				if( res != kIOReturnSuccess ) 
				{
					ok = false;
					break;
				}
			
				fDataBase = info->fDataBaseAddr;
			}
		} while( false );
	}
	
	// 6250 is the total bandwidth per frame at 400Mb/sec, seems a reasonable limit!
	fDataCursor = IONaturalMemoryCursor::withSpecification( PAGE_SIZE, 6250 );
	if( !fDataCursor ) 
	{
		ok = false;
	}
    
	if( !ok ) 
	{
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

UInt32 
IODCLProgram::getPhysicalSegs(
	void *								addr, 
	IOMemoryDescriptor * 				memory,
	IOByteCount 						len,
	IOMemoryCursor::PhysicalSegment 	segs[], 
	UInt32 								maxSegs )
{
 	UInt32 	nSegs = 0;
    
    if( fDataDesc )
	{
        nSegs = fDataCursor->genPhysicalSegments( fDataDesc, (IOByteCount)addr - fDataBase, segs, maxSegs, len );
    }
    else 
	{
		nSegs = fDataCursor->genPhysicalSegments( memory, 0, segs, maxSegs, len );
    }
	
    return nSegs;
}

void IODCLProgram::dumpDCL(DCLCommand *op)
{
    while(op) {
        UInt32		opcode;
        IOLog("(0x%p)", op);
//        op = convertDCLPtrToKernel(op);
        // Dispatch off of opcode.
        opcode = op->opcode & ~kFWDCLOpFlagMask;
        IOLog("Opcode 0x%p:", op);
        switch(opcode) {
            case kDCLReceivePacketStartOp :
            {
                DCLTransferPacket* t = (DCLTransferPacket*) op;

                IOLog("ReceivePacketStartDCL to 0x%p, size %ld", t->buffer, t->size);
                break;
            }
            case kDCLReceivePacketOp :
            {
                DCLTransferPacket* t = (DCLTransferPacket*) op;

                IOLog("ReceivePacketDCL to 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketStartOp :
            {
                DCLTransferPacket* t = (DCLTransferPacket*) op;

                IOLog("SendPacketStartDCL from 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketWithHeaderStartOp :
            {
                DCLTransferPacket* t = (DCLTransferPacket*) op;

                IOLog("SendPacketWithHeaderStartDCL from 0x%px, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLSendPacketOp :
            {
                DCLTransferPacket* t = (DCLTransferPacket*) op;

                IOLog("SendPacketDCL from 0x%p, size %ld", t->buffer, t->size);
                break;
            }

            case kDCLCallProcOp :
            {
                DCLCallProc* t = (DCLCallProc*) op;

                IOLog("CallProcDCL calling %p (0x%lx)", t->proc, t->procData);
                break;
            }
            case kDCLJumpOp :
                IOLog("JumpDCL to 0x%p", ((DCLJump*)op)->pJumpDCLLabel);
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
                DCLUpdateDCLList* t = (DCLUpdateDCLList*) op;
                DCLCommand** p = t->dclCommandList;
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



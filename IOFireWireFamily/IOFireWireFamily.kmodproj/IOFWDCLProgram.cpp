/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 *	$Log: not supported by cvs2svn $
 *	Revision 1.32  2008/05/06 03:26:57  collin
 *	more K64
 *
 *	Revision 1.31  2007/03/14 01:01:12  collin
 *	*** empty log message ***
 *	
 *	Revision 1.30  2007/02/15 19:42:07  ayanowit
 *	For 4369537, eliminated support for legacy DCL SendPacketWithHeader, since it didn't work anyway, and NuDCL does support it.
 *	
 *	Revision 1.29  2006/02/09 00:21:50  niels
 *	merge chardonnay branch to tot
 *	
 *	Revision 1.28  2005/03/12 03:27:51  collin
 *	*** empty log message ***
 *	
 *	Revision 1.27  2004/06/19 01:05:50  niels
 *	turn on prebinding for IOFireWireLib.plugin
 *	
 *	Revision 1.26  2004/06/10 20:57:36  niels
 *	*** empty log message ***
 *	
 *	Revision 1.25  2004/04/22 23:34:11  niels
 *	*** empty log message ***
 *	
 *	Revision 1.24  2003/12/19 22:07:46  niels
 *	send force stop when channel dies/system sleeps
 *	
 *	Revision 1.23  2003/08/30 00:16:44  collin
 *	*** empty log message ***
 *	
 *	Revision 1.22  2003/08/25 09:24:24  niels
 *	*** empty log message ***
 *	
 *	Revision 1.20  2003/08/25 08:39:15  niels
 *	*** empty log message ***
 *	
 *	Revision 1.19  2003/08/15 04:36:55  niels
 *	*** empty log message ***
 *	
 *	Revision 1.18  2003/08/12 00:55:03  niels
 *	*** empty log message ***
 *	
 *	Revision 1.17  2003/07/29 23:36:29  niels
 *	*** empty log message ***
 *	
 *	Revision 1.16  2003/07/29 22:49:22  niels
 *	*** empty log message ***
 *	
 *	Revision 1.15  2003/07/21 06:52:58  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.14.4.1  2003/07/01 20:54:06  niels
 *	isoch merge
 *	
 *
 */

#import <IOKit/firewire/IOFWDCLProgram.h>
#import "FWDebugging.h"

OSDefineMetaClass( IODCLProgram, OSObject )
OSDefineAbstractStructors ( IODCLProgram, OSObject )
OSMetaClassDefineReservedUsed ( IODCLProgram, 0 ) ;
OSMetaClassDefineReservedUsed ( IODCLProgram, 1 ) ;
OSMetaClassDefineReservedUnused ( IODCLProgram, 2 ) ;
OSMetaClassDefineReservedUnused ( IODCLProgram, 3 ) ;
OSMetaClassDefineReservedUnused ( IODCLProgram, 4 ) ;

#undef super
#define super OSObject

static bool
getDCLDataBuffer(
	const DCLCommand *		dcl,
	IOVirtualRange &		outRange)
{
	bool	result = false ;

	switch( dcl->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		//case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			outRange.address = (IOVirtualAddress)((DCLTransferPacket*)dcl)->buffer ;
			outRange.length = ((DCLTransferPacket*)dcl)->size ;
			result = true ;
			break ;

		case kDCLPtrTimeStampOp:
			outRange.address = (IOVirtualAddress)((DCLPtrTimeStamp*)dcl)->timeStampPtr ;
			outRange.length = sizeof( *( ((DCLPtrTimeStamp*)dcl)->timeStampPtr) ) ;
			result = true ;
			break ;
		
		default:
			break ;
	}
	
	return result ;
}

void
IODCLProgram::generateBufferMap( DCLCommand * program )
{
    
	IOVirtualAddress lowAddress = (IOVirtualAddress)-1 ;
	IOVirtualAddress highAddress = 0 ;
	
	for( DCLCommand * dcl = program; dcl != NULL; dcl = dcl->pNextDCLCommand )
	{
		IOVirtualRange tempRange ;
		if ( getDCLDataBuffer( dcl, tempRange ) )
		{
//			DebugLog( "see range %p +0x%x\n", (void*)(tempRange.address), (unsigned)(tempRange.length) ) ;
			
			lowAddress = MIN( lowAddress, trunc_page( tempRange.address ) ) ;
			highAddress = MAX( highAddress, round_page( tempRange.address + tempRange.length ) ) ;
		}		
	}

#ifdef __LP64__
	DebugLog("IODCLProgram::generateBufferMap lowAddress=%llx highAddress=%llx\n", lowAddress, highAddress ) ;
#else
	DebugLog("IODCLProgram::generateBufferMap lowAddress=%x highAddress=%x\n", lowAddress, highAddress ) ;
#endif
	
	if ( lowAddress == 0 )
	{
		return ;
	}
	
    IOByteCount length = highAddress - lowAddress;
	IOMemoryDescriptor * desc = IOMemoryDescriptor::withAddress( (void*)lowAddress, length, kIODirectionOutIn ) ;

	DebugLogCond(!desc, "couldn't make memory descriptor!\n") ;

	if ( desc && kIOReturnSuccess != desc->prepare() )
	{
		ErrorLog("couldn't prepare memory descriptor\n") ;
		
		desc->release() ;
		desc = NULL ;
	}
    
    IODMACommand * dma_command = NULL;
    if ( desc )
	{
        IOReturn status = kIOReturnSuccess;

        dma_command =  IODMACommand::withSpecification( kIODMACommandOutputHost32,  // segment function
                                                        32,                         // max address bits
                                                        length,                     // max segment size
                                                        (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly), // IO mapped & don't bounce buffer
                                                        length,                     // max transfer size
                                                        0,                          // page alignment
                                                        NULL,                       // mapper
                                                        NULL );                     // refcon
        if( dma_command == NULL )
        {
            status = kIOReturnError;
        }
        
        if( status == kIOReturnSuccess )
        {
            status = dma_command->setMemoryDescriptor( desc ); 
        }
        
        if( status != kIOReturnSuccess )
        {
            ErrorLog("couldn't prepare memory DMA command\n") ;
            
            if( dma_command )
            {
                dma_command->release();
            }
            
            desc->release() ;
            desc = NULL ; 
        }
    }
    
	if ( desc )
	{
        fExpansionData->fDMACommand = dma_command;
		fBufferMem = desc->map() ;
		desc->release() ;
		
		DebugLogCond(!fBufferMem, "couldn't make mapping\n") ;
	}
}

IOReturn
IODCLProgram::virtualToPhysical( 
	IOVirtualRange						ranges[], 
	unsigned							rangeCount, 
	IOMemoryCursor::IOPhysicalSegment	outSegments[], 
	unsigned &							outPhysicalSegmentCount, 
	unsigned							maxSegments )
{
	// nnn this assumes that subtracting an address of one of the DCL program's buffers in the memory map 
	// from the base address of the map will produce the correct offset into the memory map despite 
	// any gaps in the ranges used to build the memory descriptor
	// should be okay, since memory descriptors from user space have been allocated from a
	// single call to vm_allocate()
	
	outPhysicalSegmentCount = 0 ;
	if ( rangeCount == 0 )
		return kIOReturnSuccess ;
	
	IOVirtualAddress bufferMemBaseAddress = fBufferMem->getVirtualAddress() ;
	
	unsigned rangeIndex=0; 
	do
	{
		if ( outPhysicalSegmentCount >= maxSegments )
			return kIOReturnDMAError ;
			
		IOByteCount transferBytes = ranges[ rangeIndex ].length ;
		IOVirtualAddress offset = ranges[ rangeIndex ].address - bufferMemBaseAddress ;
		
		while( transferBytes > 0 )
		{
			outSegments[ outPhysicalSegmentCount ].location = fBufferMem->getPhysicalSegment( offset, & outSegments[ outPhysicalSegmentCount ].length ) ;
			outSegments[ outPhysicalSegmentCount ].length = min( outSegments[ outPhysicalSegmentCount ].length, transferBytes ) ;

			transferBytes -= outSegments[ outPhysicalSegmentCount ].length ;			
			offset += outSegments[ outPhysicalSegmentCount ].length ;

			++outPhysicalSegmentCount ;
		}

	} while ( ++rangeIndex < rangeCount ) ;
	
	return kIOReturnSuccess ;
}

bool
IODCLProgram::init ( IOFireWireBus::DCLTaskInfo * info)
{
	if ( ! super::init () )
		return false ;

	fExpansionData = new ExpansionData ;
	if ( !fExpansionData )
	{
		return false ;
	}
	
	fExpansionData->resourceFlags = kFWDefaultIsochResourceFlags ;

	bool success = true ;
	
	if ( info )
	{
		// this part sets up fBufferMem is passed in 'info'
		
	
		if ( ( !info->unused0 && !info->unused1 && !info->unused2 && !info->unused3 && !info->unused4
			&& ! info->unused5 ) && info->auxInfo )
		{
			switch( info->auxInfo->version )
			{
				case 0 :
				{
					fBufferMem = info->auxInfo->u.v0.bufferMemoryMap ;
					if ( fBufferMem )
					{
						fBufferMem->retain() ;
					}
					
					break ;
				}

				case 1 :
				case 2 :
				{
					fBufferMem = info->auxInfo->u.v1.bufferMemoryMap ; // handles version 2 also
					if ( fBufferMem )
					{
						fBufferMem->retain() ;
					}
					
					break ;
				}
				default :
					ErrorLog( "unsupported version found in info->auxInfo!\n" ) ;
					success = false ;
					break ;
			} ;
		}
	}

    return success ;
}

void IODCLProgram::free()
{
	if ( fExpansionData )
	{
        if( fExpansionData->fDMACommand )
        {
            fExpansionData->fDMACommand->complete();
            fExpansionData->fDMACommand->release();
            fExpansionData->fDMACommand = NULL;
        }
        
		delete fExpansionData ;
		fExpansionData = NULL ;
	}
    
	if ( fBufferMem )
	{
		fBufferMem->release() ;
		fBufferMem = NULL ;
	}
	
    OSObject::free();
}

IOReturn IODCLProgram::pause()
{
    return kIOReturnSuccess;
}

IOReturn IODCLProgram::resume()
{
    return kIOReturnSuccess;
}

void
IODCLProgram::setForceStopProc ( 
	IOFWIsochChannel::ForceStopNotificationProc proc, 
	void * 						refCon,
	IOFWIsochChannel *			channel )
{
	DebugLog("IODCLProgram::setForceStopProc\n") ;
}

void
IODCLProgram::setIsochResourceFlags ( IOFWIsochResourceFlags flags )
{
	fExpansionData->resourceFlags = flags ;
}

IOFWIsochResourceFlags
IODCLProgram::getIsochResourceFlags () const
{
	return fExpansionData->resourceFlags ;
}

IOMemoryMap *
IODCLProgram::getBufferMap() const
{
	return fBufferMem ;
}

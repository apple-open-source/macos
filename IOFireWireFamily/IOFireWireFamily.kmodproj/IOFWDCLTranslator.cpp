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
 */

#include <IOKit/firewire/IOFWDCLTranslator.h>

////////////////////////////////////////////////////////////////////////////////
//
// FWDCLReceivePacketStart
//
//   This routine runs a DCL receive packet start command.
//

static void	DCLReceivePacketStart(
	DCLCommand**			ppCurrentDCLCommand,
	UInt32					packetHeader,
	UInt8**					pPacketBuffer,
	UInt32*					pPacketSize,
	bool*					pGetNextPacket)
{
	DCLTransferPacket*		pDCLTransferPacket;
	UInt8 *				transferBuffer;
	UInt8 *				packetBuffer;
	SInt32				transferSize, packetSize;

	// Recast DCL command.
	pDCLTransferPacket = (DCLTransferPacket*) *ppCurrentDCLCommand;

	// Get some parameters.
	transferBuffer = (UInt8 *)pDCLTransferPacket->buffer;
	transferSize = pDCLTransferPacket->size - sizeof (UInt32);
	packetSize = *pPacketSize;
	if (transferSize > packetSize)
		transferSize = packetSize;
	packetBuffer = *pPacketBuffer;

	// Transfer the packet data.
	*((UInt32 *) transferBuffer) = packetHeader;
	transferBuffer += sizeof (UInt32);
	if (transferSize > 0) {
		bcopy (packetBuffer, transferBuffer, transferSize);
		packetSize -= transferSize;
		packetBuffer += transferSize;
	}

	// Check if we need another packet.
	*pGetNextPacket = true;
	if (pDCLTransferPacket->pNextDCLCommand != NULL)
	{
		if ((pDCLTransferPacket->pNextDCLCommand->opcode & ~kFWDCLOpFlagMask) ==
			kDCLReceivePacketOp)
		{
			*pGetNextPacket = false;
		}
	}

	// Update parameters.
	*ppCurrentDCLCommand = pDCLTransferPacket->pNextDCLCommand;
	*pPacketBuffer = packetBuffer;
	*pPacketSize = packetSize;
}


////////////////////////////////////////////////////////////////////////////////
//
// DCLReceivePacket
//
//   This routine runs a DCL receive packet command.
//

static void	DCLReceivePacket(
	DCLCommand**			ppCurrentDCLCommand,
	UInt32					packetHeader,
	UInt8 *					*pPacketBuffer,
	UInt32					*pPacketSize,
	bool					*pGetNextPacket)
{
	DCLTransferPacket*		pDCLTransferPacket;
	UInt8 *				transferBuffer;
	UInt8 *				packetBuffer;
	UInt32				transferSize, packetSize;

	// Recast DCL command.
	pDCLTransferPacket = (DCLTransferPacket*) *ppCurrentDCLCommand;

	// Get some parameters.
	transferBuffer = (UInt8 *)pDCLTransferPacket->buffer;
	transferSize = pDCLTransferPacket->size;
	packetSize = *pPacketSize;
	if (transferSize > packetSize)
		transferSize = packetSize;
	packetBuffer = *pPacketBuffer;

	// Transfer the packet data.
	if (transferSize > 0)
	{
		bcopy (packetBuffer, transferBuffer, transferSize);
		packetSize -= transferSize;
		packetBuffer += transferSize;
	}

	// Check if we need another packet.
	*pGetNextPacket = true;
	if (pDCLTransferPacket->pNextDCLCommand != NULL)
	{
		if ((pDCLTransferPacket->pNextDCLCommand->opcode & ~kFWDCLOpFlagMask) ==
			kDCLReceivePacketOp)
		{
			*pGetNextPacket = false;
		}
	}

	// Update parameters.
	*ppCurrentDCLCommand = pDCLTransferPacket->pNextDCLCommand;
	*pPacketBuffer = packetBuffer;
	*pPacketSize = packetSize;
}


////////////////////////////////////////////////////////////////////////////////
//
// FWDCLReceiveBuffer
//
//   This routine runs a DCL receive buffer command.
//zzz should we include packet header?
//zzz should we clip off end of packet when buffer is filled?
//

static void	DCLReceiveBuffer( DCLCommand** ppCurrentDCLCommand, UInt32 packetHeader,
	UInt8** pPacketBuffer, UInt32* pPacketSize, bool* pGetNextPacket )
{
	DCLTransferBuffer*	pDCLTransferBuffer;
	UInt8 *				buffer;
	UInt32				bufferOffset, bufferSize;
	UInt32				bufferSizeLeft;
	UInt8 *				packetBuffer;
	UInt32				packetSize;
	UInt32				transferSize;

	// Recast current DCL command.
	pDCLTransferBuffer = (DCLTransferBuffer*) *ppCurrentDCLCommand;

	// Get some parameters.
	buffer = (UInt8 *)pDCLTransferBuffer->buffer;
	bufferOffset = pDCLTransferBuffer->bufferOffset;
	bufferSize = pDCLTransferBuffer->size;
	packetBuffer = *pPacketBuffer;
	packetSize = *pPacketSize;

	// Compute size of transfer.
	bufferSizeLeft = bufferSize - bufferOffset;
	if (bufferSizeLeft > packetSize)
		transferSize = packetSize;
	else
		transferSize = bufferSizeLeft;

	// Transfer the packet data.
	if (transferSize > 0)
	{
		bcopy (packetBuffer, buffer + bufferOffset, transferSize);

		packetBuffer += transferSize;
		packetSize -= transferSize;
		bufferOffset += transferSize;
	}

	// Check if we're done with this DCL or need another packet.
	if (bufferOffset == bufferSize)
	{
		*ppCurrentDCLCommand = pDCLTransferBuffer->pNextDCLCommand;
		*pGetNextPacket = false;
	}
	else
	{
		*pGetNextPacket = true;
	}

	// Update parameters.
	pDCLTransferBuffer->bufferOffset = bufferOffset;
	*pPacketBuffer = packetBuffer;
	*pPacketSize = packetSize;
}

////////////////////////////////////////////////////////////////////////////////
//
// DCLSendPacket
//
//   This routine runs a DCL send packet command.
//

static void	DCLSendPacket(
	DCLCommand**	ppCurrentDCLCommand,
	UInt8 *			*pPacketBuffer,
	UInt32			*pPacketSize,
	bool			*pGetNextPacket)
{
	DCLTransferPacket*	pDCLTransferPacket;
	UInt8 *			transferBuffer;
	UInt8 *			packetBuffer;
	UInt32			transferSize, packetSize;

	// Recast DCL command.
	pDCLTransferPacket = (DCLTransferPacket*) *ppCurrentDCLCommand;

	// Get some parameters.
	transferBuffer = (UInt8 *)pDCLTransferPacket->buffer;
	transferSize = pDCLTransferPacket->size;
	packetSize = *pPacketSize;
	packetBuffer = *pPacketBuffer + packetSize;

	// Transfer the packet data.
	bcopy (transferBuffer, packetBuffer, transferSize);
	packetSize += transferSize;

	// Check if we need another packet.
	*pGetNextPacket = true;
	if (pDCLTransferPacket->pNextDCLCommand != NULL)
	{
		if ((pDCLTransferPacket->pNextDCLCommand->opcode & ~kFWDCLOpFlagMask) ==
			kDCLSendPacketOp)
		{
			*pGetNextPacket = false;
		}
	}

	// Update parameters.
	*ppCurrentDCLCommand = pDCLTransferPacket->pNextDCLCommand;
	*pPacketSize = packetSize;
}


////////////////////////////////////////////////////////////////////////////////
//
// DCLSendBuffer
//
//   This routine runs a DCL send buffer command.
//zzz should we clip off end of packet when buffer is emptied?
//

static void	DCLSendBuffer( DCLCommand** ppCurrentDCLCommand, UInt8** pPacketBuffer, UInt32* pPacketSize, bool* pGetNextPacket)
{
	DCLTransferBuffer*	pDCLTransferBuffer;
	UInt8 *			buffer;
	UInt32			bufferOffset, bufferSize;
	UInt32			bufferSizeLeft;
	UInt8 *			packetBuffer;
	UInt32			packetSize;
	UInt32			transferPacketSize;
	UInt32			transferSize;

	// Recast current DCL command.
	pDCLTransferBuffer = (DCLTransferBuffer*) *ppCurrentDCLCommand;

	// Get some parameters.
	buffer = (UInt8 *)pDCLTransferBuffer->buffer;
	bufferOffset = pDCLTransferBuffer->bufferOffset;
	bufferSize = pDCLTransferBuffer->size;
	packetSize = *pPacketSize;
	packetBuffer = *pPacketBuffer + packetSize;
	transferPacketSize = pDCLTransferBuffer->packetSize;

	// Compute size of transfer.
	bufferSizeLeft = bufferSize - bufferOffset;
	if (bufferSizeLeft > transferPacketSize)
		transferSize = transferPacketSize;
	else
		transferSize = bufferSizeLeft;

	// Transfer the packet data.
	if (transferSize > 0)
	{
		bcopy (buffer + bufferOffset, packetBuffer, transferSize);

		packetSize += transferSize;
		bufferOffset += transferSize;
	}

	// Check if we're done with this DCL or need another packet.
	//zzz is this the best way to do this?
	//zzz what if the next transfer command is a transfer packet command?
	if (bufferOffset == bufferSize)
	{
		*ppCurrentDCLCommand = pDCLTransferBuffer->pNextDCLCommand;
		*pGetNextPacket = false;
	}
	else
	{
		*pGetNextPacket = true;
	}

	// Update parameters.
	pDCLTransferBuffer->bufferOffset = bufferOffset;
	*pPacketSize = packetSize;
}

////////////////////////////////////////////////////////////////////////////////
//
// FWRunTranslatedDCLEngine
//
//   This routine runs the current DCL command using the given packet.  It will
// update the current DCL command, packet buffer pointer, packet size, and will
// set get next packet to true if it needs another packet.
//zzz maybe a vector table would be nice
//zzz implement rest of DCL commands.
//

static void	RunListeningDCLEngine( DCLCommand** ppCurrentDCLCommand, UInt32 packetHeader, UInt8** pPacketBuffer,
		UInt32* pPacketSize, bool* pGetNextPacket)
{
    DCLCommand*				pCurrentDCLCommand;
    DCLCallProc*			pDCLCallProc;
    DCLJump*				pDCLJump;

    // Run the current DCL command.
    pCurrentDCLCommand = *ppCurrentDCLCommand;
    switch (pCurrentDCLCommand->opcode & ~kFWDCLOpFlagMask)
    {
            case kDCLReceivePacketStartOp :
                    DCLReceivePacketStart (
                                                            &pCurrentDCLCommand,
                                                            packetHeader,
                                                            pPacketBuffer,
                                                            pPacketSize,
                                                            pGetNextPacket);
                    break;

            case kDCLReceivePacketOp :
                    DCLReceivePacket (
                                                            &pCurrentDCLCommand,
                                                            packetHeader,
                                                            pPacketBuffer,
                                                            pPacketSize,
                                                            pGetNextPacket);
                    break;

            case kDCLReceiveBufferOp :
                    DCLReceiveBuffer (
                                                            &pCurrentDCLCommand,
                                                            packetHeader,
                                                            pPacketBuffer,
                                                            pPacketSize,
                                                            pGetNextPacket);
                    break;

            case kDCLCallProcOp :
                    pDCLCallProc = (DCLCallProc*) pCurrentDCLCommand;
                    // Call the handler if there is one.
                    if (pDCLCallProc->proc != NULL)
			(*(pDCLCallProc->proc)) ((DCLCommand*) pDCLCallProc);

                    pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
                    *pGetNextPacket = false;
                    break;

            case kDCLJumpOp :
                    pDCLJump = (DCLJump*) pCurrentDCLCommand;
                    pCurrentDCLCommand = (DCLCommand*) pDCLJump->pJumpDCLLabel;
                    *pGetNextPacket = false;
                    break;

            default :
                    pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
                    *pGetNextPacket = false;
                    break;
    }

    // Update current DCL command.
    *ppCurrentDCLCommand = pCurrentDCLCommand;
}

////////////////////////////////////////////////////////////////////////////////
//
// FWRunTranslatedTalkingDCLEngine
//
//   This routine runs the current DCL command using the given packet.  It will
// update the current DCL command, packet buffer pointer, packet size, and will
// set get next packet to true if it needs another packet.
//zzz maybe a vector table would be nice
//

static void	RunTalkingDCLEngine( UInt32* packetHeader, DCLCommand** ppCurrentDCLCommand, 
		UInt8** pPacketBuffer, UInt32* pPacketSize, bool* pGetNextPacket)
{
	DCLCommand*				pCurrentDCLCommand;
	DCLCallProc*			pDCLCallProc;
	DCLJump*				pDCLJump;
	DCLSetTagSyncBits*		pDCLSetTagSyncBits;
	UInt32					host_header;
	
	// Run the current DCL command.
	pCurrentDCLCommand = *ppCurrentDCLCommand;
	switch (pCurrentDCLCommand->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp :
		case kDCLSendPacketOp :
			DCLSendPacket (
							 &pCurrentDCLCommand,
							 pPacketBuffer,
							 pPacketSize,
							 pGetNextPacket);
			break;

		case kDCLSendBufferOp :
			DCLSendBuffer (
							 &pCurrentDCLCommand,
							 pPacketBuffer,
							 pPacketSize,
							 pGetNextPacket);
			break;

		case kDCLCallProcOp :
                        pDCLCallProc = (DCLCallProc*) pCurrentDCLCommand;
                        // Call the handler if there is one.
                        if (pDCLCallProc->proc != NULL)
                            (*(pDCLCallProc->proc)) ((DCLCommand*) pDCLCallProc);
			pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
			*pGetNextPacket = false;
			break;

		case kDCLJumpOp :
			pDCLJump = (DCLJump*) pCurrentDCLCommand;
			pCurrentDCLCommand = (DCLCommand*) pDCLJump->pJumpDCLLabel;
			*pGetNextPacket = false;
			break;

		case kDCLLabelOp :
			pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
			*pGetNextPacket = false;
			break;

		case kDCLSetTagSyncBitsOp :
			pDCLSetTagSyncBits = (DCLSetTagSyncBits*) pCurrentDCLCommand;
			host_header = OSSwapBigToHostInt32( *packetHeader );
			host_header &= ~(kFWIsochTag | kFWIsochSy);
			host_header |= (pDCLSetTagSyncBits->tagBits << kFWIsochTagPhase);
			host_header |= (pDCLSetTagSyncBits->syncBits << kFWIsochSyPhase);
			*packetHeader = OSSwapHostToBigInt32( host_header );
			pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
			*pGetNextPacket = false;
			break;

		default :
			pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
			*pGetNextPacket = false;
			break;
	}

	// Update current DCL command.
	*ppCurrentDCLCommand = pCurrentDCLCommand;
}

OSDefineMetaClass( IODCLTranslator, IODCLProgram )
OSDefineAbstractStructors(IODCLTranslator, IODCLProgram)

bool IODCLTranslator::init(DCLCommand* toInterpret)
{
    if(!IODCLProgram::init())
	return false;

    // Allocate buffers etc. for programs

    fToInterpret = toInterpret;
    return true;
}

IOReturn 
IODCLTranslator::notify (
	IOFWDCLNotificationType 	notificationType,
	DCLCommand ** 				dclCommandList, 
	UInt32	 					numDCLCommands )
{
    return kIOReturnSuccess;	// Nothing to do, we're interpreting anyway
}

IOReturn IODCLTranslator::allocateHW(IOFWSpeed speed, UInt32 chan)
{
    if(!fHWProgram)
        return kIOReturnInternalError;
    return fHWProgram->allocateHW(speed, chan);
}
IOReturn IODCLTranslator::releaseHW()
{
    if(!fHWProgram)
        return kIOReturnInternalError;
    return fHWProgram->releaseHW();
}

void IODCLTranslator::stop()
{
    fHWProgram->stop();
}

DCLCommand*
IODCLTranslator::getTranslatorOpcodes() 
{
	return (DCLCommand*)&fStartLabel;
}

void
IODCLTranslator::setHWProgram(IODCLProgram *program) 
{
	fHWProgram = program;
}

void
IODCLTranslator::ListeningDCLPingPongProc(DCLCommand* pDCLCommand)
{
    IODCLTranslator *		me;
    DCLCommand*		pCurrentDCLCommand;
    DCLTransferPacket*	pDCLTransferPacket;
    UInt8 *			packetBuffer;
    UInt32			packetHeader;
    UInt32			packetSize;
    UInt32			packetNum;
    bool			getNextPacket;

    me = (IODCLTranslator *)((DCLCallProc*)pDCLCommand)->procData;
    pCurrentDCLCommand = me->fCurrentDCLCommand;
    pDCLTransferPacket = &me->fTransfers[me->fPingCount * kNumPacketsPerPingPong];
    // Run all packets through DCL program.
    for (packetNum = 0;
            ((packetNum < kNumPacketsPerPingPong) && (pCurrentDCLCommand != NULL));
                        packetNum++) {
        // Compute packet size.
        packetBuffer = (UInt8 *)pDCLTransferPacket->buffer;
        packetHeader = *((UInt32 *) packetBuffer);
        packetBuffer += sizeof (UInt32);
        packetSize = (OSSwapBigToHostInt32(packetHeader) & kFWIsochDataLength) >> kFWIsochDataLengthPhase;

        // Run this packet through DCL program.
        getNextPacket = false;
        while ((!getNextPacket) && (pCurrentDCLCommand != NULL)) {
            RunListeningDCLEngine (
				&pCurrentDCLCommand,
                                packetHeader,
                                &packetBuffer,
                                &packetSize,
                                &getNextPacket);
        }

        // Update for next packet.
        pDCLTransferPacket++;
    }

    // Update DCL translation data.
    me->fCurrentDCLCommand = pCurrentDCLCommand;
    me->fPingCount++;
    if(me->fPingCount > kNumPingPongs)
	me->fPingCount = 0;
}

void IODCLTranslator::TalkingDCLPingPongProc(DCLCommand* pDCLCommand)
{
    IODCLTranslator *		me;
    DCLCommand*				pCurrentDCLCommand;
    DCLTransferPacket*		pDCLTransferPacket;
    UInt8 *					packetBuffer;
    UInt32					packetHeader;
    UInt32					packetSize;
    UInt32					packetNum;
    bool					getNextPacket;

    me = (IODCLTranslator *)((DCLCallProc*)pDCLCommand)->procData;
    pCurrentDCLCommand = me->fCurrentDCLCommand;
    pDCLTransferPacket = &me->fTransfers[me->fPingCount * kNumPacketsPerPingPong];
    // Run all packets through DCL program.
    for (packetNum = 0;
            ((packetNum < kNumPacketsPerPingPong) && (pCurrentDCLCommand != NULL));
                        packetNum++) {
        // Compute packet size.
        packetBuffer = (UInt8 *)pDCLTransferPacket->buffer;
        packetSize = sizeof (UInt32);

        // Run this packet through DCL program.
        getNextPacket = false;
        while ((!getNextPacket) && (pCurrentDCLCommand != NULL)) {
            RunTalkingDCLEngine (&(me->fPacketHeader),
				&pCurrentDCLCommand,
                                &packetBuffer,
                                &packetSize,
                                &getNextPacket);
        }
        // Update packet header.
        packetSize -= 4;//zzz not the best way
        packetHeader =
                (packetSize << kFWIsochDataLengthPhase) |
                (OSSwapBigToHostInt32(me->fPacketHeader) & ~(kFWIsochDataLength));
        *((UInt32 *) packetBuffer) = OSSwapHostToBigInt32(packetHeader);

        // Update send packet DCL.
        packetSize += 4;//zzz really, not the best way
	// Change the transfer packet command.
	pDCLTransferPacket->size = packetSize;
	// Send notification to DCL compiler.
	me->fHWProgram->notify(kFWDCLModifyNotification,
				(DCLCommand**) pDCLTransferPacket, 1);

        // Update for next packet.
        pDCLTransferPacket++;
    }

    // Update DCL translation data.
    me->fCurrentDCLCommand = pCurrentDCLCommand;
    me->fPingCount++;
    if(me->fPingCount > kNumPingPongs)
	me->fPingCount = 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IODCLTranslateTalk, IODCLTranslator)

IOReturn IODCLTranslateTalk::compile(IOFWSpeed speed, UInt32 chan)
{
    int pingPongNum;
    int packetNum;
    DCLTransferPacket* pDCLTransferPacket = &fTransfers[0];
    DCLCallProc* pDCLPingPongProc = &fCalls[0];
    UInt8 *pingPongBuffer = fBuffer;

    if(!fHWProgram)
        return kIOReturnInternalError;

    fPacketHeader = OSSwapHostToBigInt32( chan << kFWIsochChanNumPhase );

    // Create label for start of loop.
    fStartLabel.pNextDCLCommand = (DCLCommand*)pDCLTransferPacket;
    fStartLabel.opcode = kDCLLabelOp;

    // Create kNumPingPongs ping pong buffer lists of kNumPacketsPerPingPong
    // packets each.
    for (pingPongNum = 0;  pingPongNum < kNumPingPongs; pingPongNum++) {
        // Create transfer DCL for each packet.
        for (packetNum = 0; packetNum < kNumPacketsPerPingPong; packetNum++) {
            // Receive one packet up to kMaxIsochPacketSize bytes.
            pDCLTransferPacket->pNextDCLCommand = (DCLCommand*)(pDCLTransferPacket+1);
            pDCLTransferPacket->opcode = kDCLSendPacketWithHeaderStartOp | kFWDCLOpDynamicFlag;
            pDCLTransferPacket->buffer = pingPongBuffer;
            pDCLTransferPacket->size = kMaxIsochPacketSize;
            pingPongBuffer += kMaxIsochPacketSize;
            pDCLTransferPacket++;
        }
	// Correct next opcode for last transfer op.
        (pDCLTransferPacket-1)->pNextDCLCommand = (DCLCommand*)pDCLPingPongProc;
        // Call the ping pong proc.
        pDCLPingPongProc->pNextDCLCommand = (DCLCommand*) pDCLTransferPacket;
        pDCLPingPongProc->opcode = kDCLCallProcOp;
        pDCLPingPongProc->proc = TalkingDCLPingPongProc;
        pDCLPingPongProc->procData = (UInt32) this;
        pDCLPingPongProc++;
    }
    // Correct next opcode for last call op.
    (pDCLPingPongProc-1)->pNextDCLCommand = (DCLCommand*)&fJumpToStart;

    // Loop to start of ping pong.
    fJumpToStart.pNextDCLCommand = NULL;
    fJumpToStart.opcode = kDCLJumpOp | kFWDCLOpDynamicFlag;
    fJumpToStart.pJumpDCLLabel = &fStartLabel;

    return fHWProgram->compile(speed, chan);
}

IOReturn IODCLTranslateTalk::start()
{
    int i;
    fPingCount = 0;
    // Prime all buffers
    for(i=0; i<kNumPingPongs; i++) {
        TalkingDCLPingPongProc((DCLCommand*)&fCalls[i]);
    }
    return fHWProgram->start();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IODCLTranslateListen, IODCLTranslator)

IOReturn IODCLTranslateListen::compile(IOFWSpeed speed, UInt32 chan)
{
    int pingPongNum;
    int packetNum;
    DCLTransferPacket* pDCLTransferPacket = &fTransfers[0];
    DCLCallProc* pDCLPingPongProc = &fCalls[0];
    UInt8 *pingPongBuffer = fBuffer;

    if(!fHWProgram)
        return kIOReturnInternalError;

    fPacketHeader = OSSwapHostToBigInt32( chan << kFWIsochChanNumPhase );

    // Create label for start of loop.
    fStartLabel.pNextDCLCommand = (DCLCommand*)pDCLTransferPacket;
    fStartLabel.opcode = kDCLLabelOp;

    // Create kNumPingPongs ping pong buffer lists of kNumPacketsPerPingPong
    // packets each.
    for (pingPongNum = 0;  pingPongNum < kNumPingPongs; pingPongNum++) {
        // Create transfer DCL for each packet.
        for (packetNum = 0; packetNum < kNumPacketsPerPingPong; packetNum++) {
            // Receive one packet up to kMaxIsochPacketSize bytes.
            pDCLTransferPacket->pNextDCLCommand = (DCLCommand*)(pDCLTransferPacket+1);
            pDCLTransferPacket->opcode = kDCLReceivePacketStartOp | kFWDCLOpDynamicFlag;
            pDCLTransferPacket->buffer = pingPongBuffer;
            pDCLTransferPacket->size = kMaxIsochPacketSize;
            pingPongBuffer += kMaxIsochPacketSize;
            pDCLTransferPacket++;
        }
	// Correct next opcode for last transfer op.
        (pDCLTransferPacket-1)->pNextDCLCommand = (DCLCommand*)pDCLPingPongProc;
        // Call the ping pong proc.
        pDCLPingPongProc->pNextDCLCommand = (DCLCommand*) pDCLTransferPacket;
        pDCLPingPongProc->opcode = kDCLCallProcOp;
        pDCLPingPongProc->proc = ListeningDCLPingPongProc;
        pDCLPingPongProc->procData = (UInt32) this;
        pDCLPingPongProc++;
    }
    // Correct next opcode for last call op.
    (pDCLPingPongProc-1)->pNextDCLCommand = (DCLCommand*)&fJumpToStart;

    // Loop to start of ping pong.
    fJumpToStart.pNextDCLCommand = NULL;
    fJumpToStart.opcode = kDCLJumpOp | kFWDCLOpDynamicFlag;
    fJumpToStart.pJumpDCLLabel = &fStartLabel;

    return fHWProgram->compile(speed, chan);
}

IOReturn IODCLTranslateListen::start()
{
    fPingCount = 0;
    return fHWProgram->start();
}

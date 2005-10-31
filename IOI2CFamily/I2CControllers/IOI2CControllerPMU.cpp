/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CControllerPMU.cpp,v 1.3 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CControllerPMU.cpp,v $
 *		Revision 1.3  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.2  2004/12/15 02:21:41  jlehrer
 *		Returns actual bytes transfered in IOI2CCommand.
 *		Removed cancelTransactions.
 *		
 *		Revision 1.1  2004/09/17 22:07:15  jlehrer
 *		Initial checkin.
 *		
 *		
 *
 */

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLocks.h>

#include "IOI2CController.h"
#include "IOI2CDefs.h"


//#define PMUI2C_DEBUG 1

#if (defined(PMUI2C_DEBUG) && PMUI2C_DEBUG)
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define ERRLOG(fmt, args...)  IOLog(fmt, ## args)

class IOI2CControllerPMU : public IOI2CController
{
	OSDeclareDefaultStructors(IOI2CControllerPMU)

public:
	virtual bool start(IOService *provider);
	virtual void free ( void );

protected:
	IOReturn processLockI2CBus(
		UInt32			bus);

	IOReturn processUnlockI2CBus(
		UInt32			bus);

	IOReturn processReadI2CBus(
		IOI2CCommand	*cmd);

	IOReturn processWriteI2CBus(
		IOI2CCommand	*cmd);

private:
	IOReturn ApplePMUSendI2CCommand(
		UInt32			command,
		IOByteCount		sendLength,
		UInt8			*sendBuffer,
		IOByteCount		*readLength,
		UInt8			*readBuffer);

	#define kPMUSleepDelay	15

	/*  PMU99 I2C command equates (command $9A)*/
	enum
	{
		kPMUI2CCmd					= 0x9A,

		kI2CReplyPendingErr			= -4,
		kI2CTransactionErr			= -3,
		kI2CBusyErr					= -2,
		kI2CParameterErr			= -1,
		kI2CNoErr					= 0,
		kI2CReadData				= 1,
		kI2CDataBufSize				= 249
	};

	enum
	{
		kPMUSimpleI2CStream			= 0,	// PMU i2c modes			
		kPMUSubaddressI2CStream		= 1,
		kPMUCombinedI2CStream		= 2
	};

	typedef struct
	{
		UInt8						bus;
		UInt8						xferType;
		UInt8						secondaryBusNum;
		UInt8						address;
		UInt8						subAddr;
		UInt8						combAddr;
		UInt8						dataCount;
		UInt8						data[249];					/* sizeof(PMUI2CPB) = 256*/
	} PMUI2CPB;

	typedef struct
	{
		UInt32			command;  
		IOByteCount		sLength;
		UInt8			*sBuffer;
		IOByteCount		*rLength; 
		UInt8			*rBuffer;
	} SendMiscCommandParameterBlock;

	// I2C bus types for PMU
	enum
	{
		kI2CStatusBus				= 0,	/* pseudo bus used for status retrieval*/
		kSystemClockBus				= 1,	/* (Clocks and GPIOs live here, currenlty) */
		kPowerSupplyBus				= 2,	/* (IVAD is here)*/
	};

#define MAXIICRETRYCOUNT	20
#define STATUS_DATAREAD		1
#define STATUS_OK			0
#define STATUS_BUSY			0xfe

	IOService		*fApplePMU;
	const OSSymbol	*i2cCommandSym;
};




#define super IOI2CController
OSDefineMetaClassAndStructors( IOI2CControllerPMU, IOI2CController )

bool
IOI2CControllerPMU::start(IOService *provider)
{
	mach_timespec_t			timeout;

	timeout.tv_sec = 30;
	timeout.tv_nsec = 0;

	DLOG("+IOI2CControllerPMU::start\n");

	fApplePMU = waitForService(serviceMatching("ApplePMU"), &timeout);
	if (NULL == fApplePMU)
	{
		ERRLOG("IOI2CControllerPMU::start timeout waiting for ApplePMU\n");
		return false;
	}

	i2cCommandSym = OSSymbol::withCStringNoCopy("sendMiscCommand");

	if (false == super::start(provider))
		return false;

	registerService();
	publishChildren();
	DLOG("-IOI2CControllerPMU::start\n");
	return true;
}

void IOI2CControllerPMU::free ( void )
{
	if (i2cCommandSym)		{ i2cCommandSym->release();		i2cCommandSym = 0;	}

	super::free();
}

/****************************************************************************************************
 *	controller specific I2C bus control implementation
 *	Exclusive access to these methods are protected by the client interface methods.
 ****************************************************************************************************/

IOReturn
IOI2CControllerPMU::processLockI2CBus(
	UInt32			bus)
{
	DLOG("\n+IOI2CControllerPMU::processLockI2CBus B:%x\n", bus);
	return kIOReturnSuccess;
}

IOReturn
IOI2CControllerPMU::processUnlockI2CBus(
	UInt32			bus)
{
	DLOG("\n+IOI2CControllerPMU::processUnlockI2CBus B:%x\n", bus);
	return kIOReturnSuccess;
}


#define kMAX_READ_LENGTH		256

IOReturn
IOI2CControllerPMU::processReadI2CBus(
	IOI2CCommand	*cmd)
{
    IOReturn 		status = kIOReturnError;

	UInt32			bus;
	UInt32			address;
	UInt32			subAddress;
	UInt8			*buffer;
	UInt32			count;
	UInt32			mode;

    PMUI2CPB		iicPB = {0};
    IOByteCount		rLength;
    SInt32			retries;		// loop counter for retry attempts
    UInt8			rBuffer[kMAX_READ_LENGTH] = {0};
    IOByteCount		sLength;
	UInt32			subAddr;
	UInt8			xferType;
	UInt8			combAddr;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	bus = cmd->bus;
	address = cmd->address;
	combAddr = address;
	subAddress = cmd->subAddress;
	buffer = cmd->buffer;
	count = cmd->count;
	mode = cmd->mode;
	cmd->bytesTransfered = 0;

	DLOG("\n+IOI2CControllerPMU::processReadI2CBus B:%x A:%x S:%x L:%x M:%x\n",
		bus, address, subAddress, count, mode);

	if (count > fMaxI2CDataLength)
	{
		DLOG("-IOI2CControllerPMU::processReadI2CBus max supported byte count exceeded\n");
		return kIOReturnBadArgument;
	}

	if (isI2C10BitAddress(address))
	{
		DLOG("-IOI2CControllerPMU::processReadI2CBus 10-bit addressing not supported\n");
		return kIOReturnBadArgument;
	}

	switch (mode)	// Convert mode to PMU xferType:
	{
		// Non-subaddress modes...
		case kI2CMode_Unspecified:
		case kI2CMode_Standard:
			xferType = kPMUSimpleI2CStream;
			subAddr = 0;
			break;

		// Subaddress modes need Pmuification...
		case kI2CMode_StandardSub:
		case kI2CMode_Combined:
			if (mode == kI2CMode_StandardSub)
				xferType = kPMUSubaddressI2CStream;
			else
			{
				combAddr |= 1;
				xferType = kPMUCombinedI2CStream;
			}
			switch (subAddress >> 24)
			{
				case 1:
				case 0:	subAddr = (subAddress & 0x000000ff);	break;
				default:
					status = kIOReturnBadArgument;
					DLOG("-IOI2CControllerPMU::processReadI2CBus unsupported subAddress <0x%08x> format error:%x\n", subAddress, status);
					return status;
			}
			break;

		default:
			status = kIOReturnBadArgument;
			DLOG("-IOI2CControllerPMU::processReadI2CBus unsupported mode error:%x\n", status);
			return status;
	}

	for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
	{
		iicPB.bus				= bus;
		iicPB.xferType			= xferType;
		iicPB.secondaryBusNum	= 0;			// not currently supported
		iicPB.address			= address;
		iicPB.subAddr			= subAddr;
		iicPB.combAddr			= combAddr;		// (don't care in kPMUSimpleI2Cstream)
		iicPB.dataCount			= count;        // (count set to count + 3 in clock-spread clients) 
		rLength 				= count;		// added one byte for the leading pmu status byte	
		sLength					= (short) ((UInt8 *)&iicPB.data - (UInt8 *)&iicPB.bus);   

		DLOG("PMUI2C send length=%d\n",sLength);
#ifdef PMUI2C_DEBUG
		{
			UInt32 iii;
			UInt8 *sBuffer = (UInt8 *) &iicPB;
			DLOG("PMUI2C:read1 sL:%d, rL:%d, data:", sLength, rLength);
			for (iii = 0; iii < sLength; iii++)
				DLOG(" %02x", sBuffer[iii]);
			DLOG("\n");
		}
#endif

        if (kIOReturnSuccess == (status = ApplePMUSendI2CCommand( kPMUI2CCmd, sLength, (UInt8 *) &iicPB, &rLength, rBuffer )))
		{
			if (rBuffer[0] == STATUS_OK)
				break;							// if pb accepted, proceed to status/read phase
		}

		IOSleep (15);	// hmmm...
	}

	DLOG("READ PMU MID STATUS:0x%x, retries = %d\n", status, retries);

	// If we exceeded our retries then indicate that the device (or PMU) is not responding.
	if (retries >= MAXIICRETRYCOUNT)
		status = kIOReturnNotResponding;

	if (status == kIOReturnSuccess)
	{
		IOSleep (kPMUSleepDelay);

		for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
		{
			iicPB.bus	= kI2CStatusBus;
			rLength 	= count + 1;		// added one byte for the leading status byte
			rBuffer[0]	= 0xff;
			sLength		= 1;

			if (kIOReturnSuccess == (status = ApplePMUSendI2CCommand( kPMUI2CCmd, sLength, (UInt8 *) &iicPB, &rLength, rBuffer )))
			{
#ifdef PMUI2C_DEBUG
				{
					UInt32 iii;
					DLOG("PMUI2C:read3 sL:%d, rL:%d, data:", sLength, rLength);
					for (iii = 0; iii < rLength; iii++)
						DLOG(" %02x", rBuffer[iii]);
					DLOG("\n");
				}
#endif
				if ((SInt8)rBuffer[0] <= (SInt8)STATUS_BUSY)
				{
					DLOG("PMUI2C:busy retries:%d\n", retries);
					status = kIOReturnBusy;
				}
				else
				if ((SInt8)rBuffer[0] >= STATUS_OK)
				{
					if ((SInt8)rBuffer[0] >= STATUS_DATAREAD)
					{
						DLOG("PMUI2C:rLength:%d, count:%d\n", rLength, count);
						bcopy(1 + rBuffer, buffer, count);
						cmd->bytesTransfered = count;
						status = kIOReturnSuccess;
						break;
					}

					DLOG("PMUI2C:read rBuffer[0] == STATUS_OK (%x) considering this an error:%x!!!\n", rBuffer[0], status);
					status = kIOReturnIOError;
#ifdef PMUI2C_DEBUG
					{
						UInt32 iii;
						DLOG("PMUI2C:read4 sL:%d, rL:%d, data:", sLength, rLength);
						for (iii = 0; iii < rLength; iii++)
							DLOG(" %02x", rBuffer[iii]);
						DLOG("\n");
					}
#endif
					break;
				}
			}
			IOSleep( 15 );
		}
	}

	DLOG("READ I2C PMU END - STATUS:0x%x  retries = %d\n", status, retries);
	DLOG("addr = 0x%02lx, subAd = 0x%02lx, rdBuf[0] = 0x%02x, count = 0x%ld\n", address, subAddress, rBuffer[0], count);

	DLOG("-IOI2CControllerPMU::processReadI2CBus\n\n");
	return status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* writeSmuI2C								 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
IOReturn
IOI2CControllerPMU::processWriteI2CBus(
	IOI2CCommand	*cmd)
{
	IOReturn		status = kIOReturnError;

	UInt32			bus;
	UInt32			address;
	UInt32			subAddress;
	UInt8			*buffer;
	UInt32			count;
	UInt32			mode;

	PMUI2CPB		iicPB = {0};
	IOByteCount		rLength;
	UInt8			rBuffer[8] = {0};
	SInt32			retries;		// loop counter for retry attempts
	IOByteCount		sLength;
	UInt32			subAddr;
	UInt8			xferType;
	UInt8			combAddr;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	bus = cmd->bus;
	address = cmd->address;
	combAddr = address;
	subAddress = cmd->subAddress;
	buffer = cmd->buffer;
	count = cmd->count;
	mode = cmd->mode;
	cmd->bytesTransfered = 0;

	DLOG("\n+IOI2CControllerPMU::processWriteI2CBus B:%x A:%x S:%x L:%x M:%x\n",
		bus, address, subAddress, count, mode);

	if (count > fMaxI2CDataLength)
	{
		DLOG("-IOI2CControllerPMU::processReadI2CBus max supported byte count exceeded\n");
		return kIOReturnBadArgument;
	}

	if (isI2C10BitAddress(address))
	{
		DLOG("-IOI2CControllerPMU::processWriteI2CBus 10-bit addressing not supported\n");
		return kIOReturnBadArgument;
	}

	switch (mode)	// Convert mode to PMU xferType:
	{
		// Non-subaddress modes...
		case kI2CMode_Unspecified:
		case kI2CMode_Standard:
			subAddr = 0;
			xferType = kPMUSimpleI2CStream;
			break;

		// Subaddress modes need Pmuification...
		case kI2CMode_StandardSub:
		case kI2CMode_Combined:
			if (mode == kI2CMode_StandardSub)
				xferType = kPMUSubaddressI2CStream;
			else
			{
				combAddr &= ~1;
				xferType = kPMUCombinedI2CStream;
			}
			switch (subAddress >> 24)
			{
				case 1:
				case 0:	subAddr = (subAddress & 0x000000ff);	break;
				default:
					status = kIOReturnBadArgument;
					DLOG("-IOI2CControllerPMU::processWriteI2CBus unsupported subAddress <0x%08x> format error:%x\n", subAddress, status);
					return status;
			}
			break;

		default:
			status = kIOReturnBadArgument;
			DLOG("-IOI2CControllerPMU::processWriteI2CBus unsupported mode error:%x\n", status);
			return status;
	}

	DLOG("\n+IOI2CControllerPMU::processWriteI2CBus\nA:0x%02lx, S:0x%02lx, wrBuf:0x%02x, L:%ld\n",
		address, subAddress, buffer[0], count);

	for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
	{
        iicPB.bus				= bus;
		iicPB.xferType			= xferType;
		iicPB.secondaryBusNum	= 0;			// not currently supported
        iicPB.address			= address;
		iicPB.subAddr			= subAddr;
        iicPB.combAddr			= combAddr;		// (don't care in kPMUSimpleI2Cstream)
        iicPB.dataCount			= count;        // (count set to count + 3 in clock-spread clients)

		bcopy(buffer, iicPB.data, count);

        rLength 				= 1;			// status return only
		rBuffer[0]				= 0xff;
		sLength					= (short) (&iicPB.data[count] - &iicPB.bus);
#ifdef PMUI2C_DEBUG
		{
			UInt32 iii;
			UInt8 *sBuffer = (UInt8 *)&iicPB;
			DLOG("PMUI2C:write1 sL:%d, rL:%d, data:", sLength, rLength);
			for (iii = 0; iii < sLength; iii++)
				DLOG(" %02x", sBuffer[iii]);
			DLOG("\n");
		}
#endif

		if (kIOReturnSuccess == (status = ApplePMUSendI2CCommand( kPMUI2CCmd, sLength, (UInt8 *) &iicPB, &rLength, rBuffer )))
		{
#ifdef PMUI2C_DEBUG
			{
				UInt32 iii;
				DLOG("PMUI2C:write2 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < rLength; iii++)
					DLOG(" %02x", rBuffer[iii]);
				DLOG("\n");
			}
#endif
			if (rBuffer[0] == STATUS_OK)
				break;			// if pb accepted, proceed to status phase
			else
				status = kIOReturnIOError;
		}

		IOSleep(15);
	}

	DLOG("WRITE PMU MID STATUS:%x, retries:%d\n", status, retries);

	// If we exceeded our retries then indicate that the SMU is not responding.
	if (retries >= MAXIICRETRYCOUNT)
		status = kIOReturnNotResponding;

	if (status == kIOReturnSuccess)
	{
		// SMU has a long round trip time so take a nap
		IOSleep (kPMUSleepDelay);

		for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
		{
			iicPB.bus		= kI2CStatusBus;	// attempt to recover status
			rLength 		= sizeof( rBuffer );
			rBuffer[0]		= 0xff;
			sLength			= 1;

#ifdef PMUI2C_DEBUG
			{
				UInt32 iii;
				UInt8 *sBuffer = (UInt8 *)&iicPB;
				DLOG("PMUI2C:write3 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < sLength; iii++)
					DLOG(" %02x", sBuffer[iii]);
				DLOG("\n");
			}
#endif
			if (kIOReturnSuccess == (status = ApplePMUSendI2CCommand( kPMUI2CCmd, sLength, (UInt8 *) &iicPB, &rLength, rBuffer )))
			{
#ifdef PMUI2C_DEBUG
				{
					UInt32 iii;
					DLOG("PMUI2C:write4 sL:%d, rL:%d, data:", sLength, rLength);
					for (iii = 0; iii < rLength; iii++)
						DLOG(" %02x", rBuffer[iii]);
					DLOG("\n");
				}
#endif
				if (rBuffer[0] == STATUS_OK)
				{
					cmd->bytesTransfered = count;
					break;
				}

				status = kIOReturnIOError;
				DLOG("WRITE PMU STATUS:0x%x, retries:%02lx\n", status, retries);
			}

            IOSleep(15);
		}
	}

	DLOG("-IOI2CControllerPMU::processWriteI2CBus\n\n");
	return status;
}


IOReturn
IOI2CControllerPMU::ApplePMUSendI2CCommand(
	UInt32			command,
	IOByteCount		sendLength,
	UInt8			*sendBuffer,
	IOByteCount		*readLength,
	UInt8			*readBuffer)
{
	IOReturn		ret;

	SendMiscCommandParameterBlock params = { command, sendLength, sendBuffer, readLength, readBuffer };

	ret = fApplePMU->callPlatformFunction( "sendMiscCommand", true, (void*)&params, NULL, NULL, NULL );
    return ret;
}


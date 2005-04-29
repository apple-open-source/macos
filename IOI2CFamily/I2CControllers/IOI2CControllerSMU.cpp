/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CControllerSMU.cpp,v 1.4 2004/12/15 02:21:42 jlehrer Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CControllerSMU.cpp,v $
 *		Revision 1.4  2004/12/15 02:21:42  jlehrer
 *		Returns actual bytes transfered in IOI2CCommand.
 *		Removed cancelTransactions.
 *		
 *		Revision 1.3  2004/09/17 22:03:12  jlehrer
 *		Removed ASPL headers.
 *		Added 10-bit address check.
 *		Minor code cleanup.
 *		
 *		Revision 1.2  2004/07/03 00:09:20  jlehrer
 *		Uses dynamic max-i2c-data-length to limit transactions
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLocks.h>

#include "IOI2CController.h"
#include "IOI2CDefs.h"


//#define SMUI2C_DEBUG 1

#if (defined(SMUI2C_DEBUG) && SMUI2C_DEBUG)
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

class IOI2CControllerSMU : public IOI2CController
{
	OSDeclareDefaultStructors(IOI2CControllerSMU)

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
	IOReturn AppleSMUSendI2CCommand(
		IOByteCount		sendLength,
		UInt8			*sendBuffer,
		IOByteCount		*readLength,
		UInt8			*readBuffer,
		UInt8			command);

	enum
	{
		kSMUSleepDelay				= 150	// milliseconds
	};

	enum
	{
		kSMU_Open					= 1,
		kSMU_I2C_Cmd				= 2,
		kSMU_Close					= 3
	};

	enum
	{
		kSimpleI2CStream            = 0,	// PMU & SMU i2c modes			
		kSubaddressI2CStream        = 1,
		kCombinedI2CStream          = 2
	};

	typedef struct
	{
		UInt8						bus;
		UInt8						xferType;
		UInt8						address;
		UInt8						subAddr[4];
		UInt8						combAddr;
		UInt8						dataCount;
		UInt8						data[246];                  /* sizeof(SMUI2CPB) = 256 (255?)*/
	} SMUI2CPB;

	// I2C bus types for PMU
	enum
	{
		kI2CStatusBus               = 0,                 /* pseudo bus used for status retrieval*/
		kSystemClockBus             = 1,                 /* (Clocks and GPIOs live here, currenlty) */
		kPowerSupplyBus             = 2,                 /* (IVAD is here)*/
	};

#define MAXIICRETRYCOUNT	20
#define STATUS_DATAREAD		1
#define STATUS_OK		0
#define STATUS_BUSY		0xfe

	// Loop timeout in waitForCompletion();  Defaults to 2 sec interrupt mode, defaults
	//  to 15 secs on i2cmacio polling (for audio) and to 3 sec i2cUniN polling
	UInt16			waitTime;

	const OSSymbol	*i2cCommandSym;
	const OSSymbol	*i2cOpenSym;
	const OSSymbol	*i2cCloseSym;
};




#define super IOI2CController
OSDefineMetaClassAndStructors( IOI2CControllerSMU, IOI2CController )

bool
IOI2CControllerSMU::start(IOService *provider)
{
	DLOG("+IOI2CControllerSMU::start\n");
	if (false == super::start(provider))
		return false;

	i2cCommandSym = OSSymbol::withCStringNoCopy("sendSMUI2CCommand");
	i2cOpenSym = OSSymbol::withCStringNoCopy("openSMUI2CCommand");
	i2cCloseSym = OSSymbol::withCStringNoCopy("closeSMUI2CCommand");

	registerService();
	publishChildren();
	DLOG("-IOI2CControllerSMU::start\n");
	return true;
}

void IOI2CControllerSMU::free ( void )
{
	if (i2cCommandSym)		{ i2cCommandSym->release();		i2cCommandSym = 0;	}
	if (i2cOpenSym)			{ i2cOpenSym->release();		i2cOpenSym = 0;		}
	if (i2cCloseSym)		{ i2cCloseSym->release();		i2cCloseSym = 0;	}

	super::free();
}



/****************************************************************************************************
 *	controller specific I2C bus control implementation
 *	Exclusive access to these methods are protected by the client interface methods.
 ****************************************************************************************************/

IOReturn
IOI2CControllerSMU::processLockI2CBus(
	UInt32			bus)
{
	IOReturn		status;
//	DLOG("+IOI2CControllerSMU::processLockI2CBus: %lx\n", bus);

	status = AppleSMUSendI2CCommand( (IOByteCount)bus, NULL, NULL, NULL, kSMU_Open);

//	DLOG("-IOI2CControllerSMU::processLockI2CBus: %lx status: %08x\n", bus, status);
	return status;
}

IOReturn
IOI2CControllerSMU::processUnlockI2CBus(
	UInt32			bus)
{
	IOReturn		status;

//	DLOG("IOI2CControllerSMU::processUnlockI2CBus: %lx\n", bus);

	status = AppleSMUSendI2CCommand( (IOByteCount)bus, NULL, NULL, NULL, kSMU_Close);

//	DLOG("-IOI2CControllerSMU::processUnlockI2CBus: %lx status: %08x\n", bus, status);
	return status;
}

#define kMAX_READ_LENGTH		256

IOReturn
IOI2CControllerSMU::processReadI2CBus(
	IOI2CCommand	*cmd)
{
    IOReturn 		status = kIOReturnError;

	UInt32			bus;
	UInt32			address;
	UInt32			subAddress;
	UInt8			*buffer;
	UInt32			count;
	UInt32			mode;

    SMUI2CPB		iicPB = {0};
    IOByteCount		rLength;
    SInt32			retries;		// loop counter for retry attempts
    UInt8			rBuffer[kMAX_READ_LENGTH] = {0};
    IOByteCount		sLength;
	UInt32			smuSubAddr;
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

	DLOG("\n+IOI2CControllerSMU::processReadI2CBus\n");

	if (count > fMaxI2CDataLength)
	{
		DLOG("-IOI2CControllerSMU::processReadI2CBus max supported byte count exceeded\n");
		return kIOReturnBadArgument;
	}

	if (isI2C10BitAddress(address))
	{
		DLOG("-IOI2CControllerSMU::processReadI2CBus 10-bit addressing not supported\n");
		return kIOReturnBadArgument;
	}

	switch (mode)	// Convert mode to SMU xferType:
	{
		// Non-subaddress modes...
		case kI2CMode_Unspecified:
		case kI2CMode_Standard:
			xferType = kSimpleI2CStream;
			smuSubAddr = 0;
			break;

		// Subaddress modes need Smuification...
		case kI2CMode_StandardSub:
		case kI2CMode_Combined:
			if (mode == kI2CMode_StandardSub)
				xferType = kSubaddressI2CStream;
			else
			{
				combAddr |= 1;
				xferType = kCombinedI2CStream;
			}
			switch (subAddress >> 24)
			{
				case 3:	smuSubAddr = subAddress;	break;
				case 2:	smuSubAddr = (2 << 24) | ((subAddress & 0x0000ffff) << 8);	break;
				case 1:
				case 0:	smuSubAddr = (1 << 24) | ((subAddress & 0x000000ff) << 16);	break;
				default:
					status = kIOReturnBadArgument;
					DLOG("-IOI2CControllerSMU::processReadI2CBus unsupported subAddress <0x%08x> format error:%x\n", subAddress, status);
					return status;
			}
			break;

		default:
			status = kIOReturnBadArgument;
			DLOG("-IOI2CControllerSMU::processReadI2CBus unsupported mode error:%x\n", status);
			return status;
	}

	for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
	{
        iicPB.bus				= bus;
		iicPB.xferType			= xferType;
        iicPB.address			= address;
		iicPB.subAddr[0] = ((UInt8*)&smuSubAddr)[0];
		iicPB.subAddr[1] = ((UInt8*)&smuSubAddr)[1];
		iicPB.subAddr[2] = ((UInt8*)&smuSubAddr)[2];
		iicPB.subAddr[3] = ((UInt8*)&smuSubAddr)[3];
        iicPB.combAddr			= combAddr;		// (don't care in kSimpleI2Cstream)
        iicPB.dataCount			= count;        // (count set to count + 3 in clock-spread clients) 
        rLength 				= count;		// added one byte for the leading pmu status byte	
        sLength					= (short) ((UInt8 *)&iicPB.data - (UInt8 *)&iicPB.bus);   

		DLOG("SMUI2C send length=%d\n",sLength);
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				UInt8 *sBuffer = (UInt8 *) &iicPB;
				DLOG("SMUI2C:read1 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < sLength; iii++)
					DLOG(" %02x", sBuffer[iii]);
				DLOG("\n");
			}
#endif

        if (kIOReturnSuccess == (status = AppleSMUSendI2CCommand( sLength, (UInt8 *) &iicPB, &rLength, rBuffer, kSMU_I2C_Cmd )))
		{
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				DLOG("SMUI2C:read2 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < rLength; iii++)
					DLOG(" %02x", rBuffer[iii]);
				DLOG("\n");
			}
#endif
			if (rBuffer[0] == STATUS_OK)
				break;							// if pb accepted, proceed to status/read phase
		}

		IOSleep (15);	// hmmm...
	}

	DLOG("READ SMU MID STATUS:0x%x, retries = %d\n", status, retries);

	// If we exceeded our retries then indicate that the device (or SMU) is not responding.
	if (retries >= MAXIICRETRYCOUNT)
		status = kIOReturnNotResponding;

	if (status == kIOReturnSuccess)
	{
		// SMU has a long round trip time so take a nap
		IOSleep (kSMUSleepDelay);

		for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
		{
			iicPB.bus	= kI2CStatusBus;
			rLength 	= count + 1;		// added one byte for the leading status byte
			rBuffer[0]	= 0xff;
			sLength		= 1;

#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				UInt8 *sBuffer = (UInt8 *) &iicPB;
				DLOG("SMUI2C:read3 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < sLength; iii++)
					DLOG(" %02x", sBuffer[iii]);
				DLOG("\n");
			}
#endif

			if (kIOReturnSuccess == (status = AppleSMUSendI2CCommand( sLength, (UInt8 *) &iicPB, &rLength, rBuffer, kSMU_I2C_Cmd )))
			{
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				DLOG("SMUI2C:read4 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < rLength; iii++)
					DLOG(" %02x", rBuffer[iii]);
				DLOG("\n");
			}
#endif
				if ((SInt8)rBuffer[0] >= STATUS_OK)
				{
					DLOG("SMUI2C:rLength:%d, count:%d\n", rLength, count);
					bcopy( 1 + rBuffer, buffer, count ); // copy SMU data to client buffer, strip smu I2C status byte.
				}
				else
				{
					status = kIOReturnIOError;
					DLOG("SMUI2C:read rBuffer[0] < STATUS_OK (%x) considering this an error:%x!!!\n", rBuffer[0], status);
				}

				DLOG("READ I2C SMU END - STATUS:0x%x  retries = %d\n", status, retries);
				DLOG("addr = 0x%02lx, subAd = 0x%02lx, rdBuf[0] = 0x%02x, count = 0x%ld\n", address, subAddress, rBuffer[0], count);
				cmd->bytesTransfered = count;
				break;
			}
			IOSleep( 15 );
		}
	}

	DLOG("-IOI2CControllerSMU::processReadI2CBus\n\n");
	return status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* writeSmuI2C								 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
IOReturn
IOI2CControllerSMU::processWriteI2CBus(
	IOI2CCommand	*cmd)
{
	IOReturn		status = kIOReturnError;

	UInt32			bus;
	UInt32			address;
	UInt32			subAddress;
	UInt8			*buffer;
	UInt32			count;
	UInt32			mode;

	SMUI2CPB		iicPB = {0};
	IOByteCount		rLength;
	UInt8			rBuffer[8] = {0};
	SInt32			retries;		// loop counter for retry attempts
	IOByteCount		sLength;
	UInt32			smuSubAddr;
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

	if (count > fMaxI2CDataLength)
	{
		DLOG("-IOI2CControllerSMU::processReadI2CBus max supported byte count exceeded\n");
		return kIOReturnBadArgument;
	}

	if (isI2C10BitAddress(address))
	{
		DLOG("-IOI2CControllerSMU::processWriteI2CBus 10-bit addressing not supported\n");
		return kIOReturnBadArgument;
	}

	switch (mode)	// Convert mode to SMU xferType:
	{
		// Non-subaddress modes...
		case kI2CMode_Unspecified:
		case kI2CMode_Standard:
			smuSubAddr = 0;
			xferType = kSimpleI2CStream;
			break;

		// Subaddress modes need Smuification...
		case kI2CMode_StandardSub:
		case kI2CMode_Combined:
			if (mode == kI2CMode_StandardSub)
				xferType = kSubaddressI2CStream;
			else
			{
				combAddr |= 1;
				xferType = kCombinedI2CStream;
			}
			switch (subAddress >> 24)
			{
				case 3:	smuSubAddr = subAddress;	break;
				case 2:	smuSubAddr = (2 << 24) | ((subAddress & 0x0000ffff) << 8);	break;
				case 1:
				case 0:	smuSubAddr = (1 << 24) | ((subAddress & 0x000000ff) << 16);	break;
				default:
					status = kIOReturnBadArgument;
					DLOG("-IOI2CControllerSMU::processReadI2CBus unsupported subAddress <0x%08x> format error:%x\n", subAddress, status);
					return status;
			}
			break;

		default:
			status = kIOReturnBadArgument;
			DLOG("-IOI2CControllerSMU::processReadI2CBus unsupported mode error:%x\n", status);
			return status;
	}

	DLOG("\n+IOI2CControllerSMU::processWriteI2CBus\nA:0x%02lx, S:0x%02lx, wrBuf:0x%02x, L:%ld\n",
		address, subAddress, buffer[0], count);

	for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
	{
        iicPB.bus				= bus;
		iicPB.xferType			= xferType;
        iicPB.address			= address;
		iicPB.subAddr[0] = ((UInt8*)&smuSubAddr)[0];
		iicPB.subAddr[1] = ((UInt8*)&smuSubAddr)[1];
		iicPB.subAddr[2] = ((UInt8*)&smuSubAddr)[2];
		iicPB.subAddr[3] = ((UInt8*)&smuSubAddr)[3];
        iicPB.combAddr			= combAddr;		// (don't care in kSimpleI2Cstream)
        iicPB.dataCount			= count;        // (count set to count + 3 in clock-spread clients)

		bcopy(buffer, iicPB.data, count);		// copy client's buffer data to SMU command.

        rLength 				= 1;			// status return only
		rBuffer[0]				= 0xff;
		sLength					= (short) (&iicPB.data[count] - &iicPB.bus);  
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				UInt8 *sBuffer = (UInt8 *)&iicPB;
				DLOG("SMUI2C:write1 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < sLength; iii++)
					DLOG(" %02x", sBuffer[iii]);
				DLOG("\n");
			}
#endif

		if (kIOReturnSuccess == (status = AppleSMUSendI2CCommand( sLength, (UInt8 *) &iicPB, &rLength, rBuffer, kSMU_I2C_Cmd )))
		{
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				DLOG("SMUI2C:write2 sL:%d, rL:%d, data:", sLength, rLength);
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

	DLOG("WRITE SMU MID STATUS:%x, retries:%d\n", status, retries);

	// If we exceeded our retries then indicate that the SMU is not responding.
	if (retries >= MAXIICRETRYCOUNT)
		status = kIOReturnNotResponding;

	if (status == kIOReturnSuccess)
	{
		// SMU has a long round trip time so take a nap
		IOSleep (kSMUSleepDelay);

		for (retries = 0; retries < MAXIICRETRYCOUNT; retries++)
		{
			iicPB.bus		= kI2CStatusBus;	// attempt to recover status
			rLength 		= sizeof( rBuffer );
			rBuffer[0]		= 0xff;
			sLength			= 1;

#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				UInt8 *sBuffer = (UInt8 *)&iicPB;
				DLOG("SMUI2C:write3 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < sLength; iii++)
					DLOG(" %02x", sBuffer[iii]);
				DLOG("\n");
			}
#endif
			if (kIOReturnSuccess == (status = AppleSMUSendI2CCommand( sLength, (UInt8 *) &iicPB, &rLength, rBuffer, kSMU_I2C_Cmd )))
			{
#ifdef SMUI2C_DEBUG
			{
				UInt32 iii;
				DLOG("SMUI2C:write4 sL:%d, rL:%d, data:", sLength, rLength);
				for (iii = 0; iii < rLength; iii++)
					DLOG(" %02x", rBuffer[iii]);
				DLOG("\n");
			}
#endif
				if (rBuffer[0] != STATUS_OK)
					status = kIOReturnIOError;
				DLOG("WRITE SMU STATUS:0x%x, retries:%02lx\n", status, retries);
				cmd->bytesTransfered = count;
				break;
			}

            IOSleep(15);
		}
	}

	DLOG("-IOI2CControllerSMU::processWriteI2CBus\n\n");
	return status;
}


IOReturn
IOI2CControllerSMU::AppleSMUSendI2CCommand(
	IOByteCount		sendLength,
	UInt8			*sendBuffer,
	IOByteCount		*readLength,
	UInt8			*readBuffer,
	UInt8			command)
{
	IOReturn		ret = kIOReturnError;
	typedef struct
	{
		IOByteCount		sLength;
		UInt8			*sBuffer;
		IOByteCount		*rLength; 
		UInt8			*rBuffer;
	} SMUSendParams;
	
	SMUSendParams params = { sendLength, sendBuffer, readLength, readBuffer };

	switch (command)
	{
		case kSMU_Open:		ret = fProvider->callPlatformFunction( i2cOpenSym,		true, (void*)&params, NULL, NULL, NULL );	break;
		case kSMU_I2C_Cmd:	ret = fProvider->callPlatformFunction( i2cCommandSym,	true, (void*)&params, NULL, NULL, NULL );	break;
		case kSMU_Close:	ret = fProvider->callPlatformFunction( i2cCloseSym,		true, (void*)&params, NULL, NULL, NULL );	break;
	}
    return( ret );
}


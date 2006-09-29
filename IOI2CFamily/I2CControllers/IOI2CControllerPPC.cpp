/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CControllerPPC.cpp,v 1.13 2006/04/10 23:40:02 hpanther Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CControllerPPC.cpp,v $
 *		Revision 1.13  2006/04/10 23:40:02  hpanther
 *		Include FireLog.h only for debug targets.
 *		
 *		Revision 1.12  2006/02/17 22:17:40  hpanther
 *		Fix
 *		
 *		4436981	IOI2CControllerPPC.kext: Replace semaphore with timeout semaphore block	CPU Software	IOI2C	(2) Analyze	McKenna, Mike	leopard	2	2	Crash or Data Loss	Today 8:00 AM
 *		
 *		Revision 1.11  2006/02/02 22:30:01  hpanther
 *		Repair additional race condition.
 *		
 *		Revision 1.10  2006/02/02 00:24:46  hpanther
 *		Replace flawed IOLock synchronization with semaphores.
 *		A bit of cleanup on the logging side.
 *		
 *		Revision 1.9  2005/09/08 03:17:16  galcher
 *		[rdar://4242471] Remove Kodiak Workarounds (IOI2CControllerPPC).
 *		Removed variables and kodiakSyncWait() inline definition.  Removed setup of
 *		kodiakSyncWait in ::start.  Removed usage of kodiakSyncWait() in ::writeReg()
 *		and ::readReg().
 *		
 *		Revision 1.8  2005/05/31 23:27:37  tmason
 *		Fix for Radar bug ID 4117978, panic with boot-args lcks=1
 *		
 *		Revision 1.7  2005/05/16 23:30:57  galcher
 *		[rdar://problem/4120572] OS changes needed to support Kodiak <= 1.1.
 *		Add kodiakSyncWait() routine and use it in the NorthBridge I2C register
 *		accessors.  NOTE - this will need to be removed before shipping.
 *		
 *		Revision 1.6  2005/02/24 22:07:00  jlehrer
 *		[4022756] Fixed fMODE_CLKDIV enums.
 *		
 *		Revision 1.5  2004/12/17 04:53:15  jlehrer
 *		[3925249] Fix polled mode timeout warning.
 *		
 *		Revision 1.4  2004/12/17 00:40:47  jlehrer
 *		[3915595] Allow at least 5 seconds before timeout for devices with long clock stretching after wake from sleep.
 *		Client driver can specify a longer timeout in IOI2CCommand.timeout_uS param.
 *		Replaced semaphore with an IOLock.
 *		
 *		Revision 1.3  2004/12/15 02:39:41  jlehrer
 *		[3867728] Fix problem with address no-ack on write transactions.
 *		[3893026] Return specific I2C transaction errors.
 *		
 *		Revision 1.2  2004/09/17 21:56:27  jlehrer
 *		Removed ASPL headers.
 *		Added workaround for u3 DP timebase synchronization:
 *		   On wake from sleep MR4CPU timebase sync happens before PM threads start.
 *		   We need to be online and ready to process the i2c transactions.
 *		   This is enabled by the PE by setting the "AAPL,i2c-no-sleep" property in our provider.
 *		Added publish IOResource for uni-n and mac-io instance.
 *		Removed unused test code fragments.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CController.h"

#include <mach/task.h>
#include <mach/semaphore.h>

#ifdef DEBUG
	#include <IOKit/firewire/IOFireLog.h>
	#define PPC_I2C_DEBUG 1
#endif

#if (defined(PPC_I2C_DEBUG) && PPC_I2C_DEBUG)
#define DLOG(fmt, args...)  FireLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#ifdef DEBUG
	#define I2C_ERRLOG 1
#endif

#if (defined(I2C_ERRLOG) && I2C_ERRLOG)
#define ERRLOG(fmt, args...)  FireLog(fmt, ## args)
#else
#define ERRLOG(fmt, args...)
#endif

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

#if (defined(PPC_I2C_DEBUG) && PPC_I2C_DEBUG)
void TLOG(const char *str)
{
	AbsoluteTime abst;
	uint64_t t;
	clock_get_uptime(&abst);
	absolutetime_to_nanoseconds(abst, &t);
	DLOG("%s @%d.%u\n", str?str:"", (int)(t/1000000ULL), (int)(t%1000000ULL));
}
#else
#define TLOG(s)
#endif

class IOI2CControllerPPC : public IOI2CController
{
	OSDeclareDefaultStructors(IOI2CControllerPPC)

public:
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	virtual void free(void);

protected:
	// Subclass required methods...
	virtual IOReturn processLockI2CBus(
		UInt32			bus);

	virtual IOReturn processUnlockI2CBus(
		UInt32			bus);

	virtual IOReturn processReadI2CBus(
		IOI2CCommand	*cmd);

	virtual IOReturn processWriteI2CBus(
		IOI2CCommand	*cmd);

private:
	bool fU3NoSleep;

	virtual IOReturn setPowerState(
		unsigned long	newPowerState,
		IOService		*dontCare);

	enum
	{
		iMODE			= 0x0,	// MODE Read/Write Mode Register
		iCNTRL			= 0x1,	// CNTRL Read/Write Control Register
		iSTATUS			= 0x2,	// STATUS Read Only Status Register
		iISR			= 0x3,	// ISR Read/Write Interrupt Status Register
		iIER			= 0x4,	// IER Read/Write Interrupt Enable Register
		iADDR			= 0x5,	// ADDR Read/Write Address Register
		iSUBADDR		= 0x6,	// SUBADDR Read/Write Sub-address Register
		iDATA			= 0x7,	// DATA Read/Write IIC Data Transmit/Receive Register
		iREVNUM			= 0x8,	// REVNUM Read Only Revision Number Register
		iRISETIMECNT	= 0x9,	// RISETIMECNT Read/Write Clock Rise Time Counter value
		iBITTIMECNT		= 0xa,	// BITTIMECNT Read/Write IIC Bit Time Counter value

		kIIC_REG_COUNT	= 0xb	// Number of IIC cell registers.
	};

	enum
	{
		fMODE_PORTSEL		= (1 << 4),		// PORTSEL IIC Port Select

		fMODE_APMODE		= (3 << 2),		// APMODE Address Phase Mode, bits[2:3]
		fMODE_APMODE_dumb	= (0 << 2),
		fMODE_APMODE_std	= (1 << 2),
		fMODE_APMODE_sub	= (2 << 2),
		fMODE_APMODE_comb	= (3 << 2),

		fMODE_CLKDIV		= (3 << 0),		// N Clock divider N, bits[0:1]
		fMODE_CLKDIV_100kHz	= (0 << 0),
		fMODE_CLKDIV_50kHz	= (1 << 0),
		fMODE_CLKDIV_25kHz	= (2 << 0),
	};

	enum
	{
		fCNTRL_START		= (1 << 3),		// START Send Start Condition
		fCNTRL_STOP			= (1 << 2),		// STOP Send Stop Condition
		fCNTRL_XADDR		= (1 << 1),		// XADDR Send Address Phase
		fCNTRL_AAK			= (1 << 0),		// AAK Assert Acknowledge
		fCNTRL_NoAAK		= (0 << 0),		// NoAAK Assert Acknowledge
	};

	enum
	{
		fSTATUS_ISCL		= (1 << 4),		// ISCL Double rank synchronized ISCL of active port
		fSTATUS_ISDA		= (1 << 3),		// ISDA Double rank synchronized ISDA of active port
		fSTATUS_LASTRW		= (1 << 2),		// LASTRW_ Last R/W_
		fSTATUS_LASTAAK		= (1 << 1),		// LASTAAK Last Acknowledge
		fSTATUS_BUSY		= (1 << 0),		// BUSY Bus Busy Indicator
	};

	enum
	{
		fISR_ISTART			= (1 << 3),		// ISTART Start Condition Sent Interrupt
		fISR_ISTOP			= (1 << 2),		// ISTOP Stop Condition Sent Interrupt
		fISR_IADDR			= (1 << 1),		// IADDR Address Phase Sent Interrupt
		fISR_IDATA			= (1 << 0),		// IDATA Data Byte Sent or Received Interrupt
		fISR_IMASK = (fISR_ISTART | fISR_ISTOP | fISR_IADDR | fISR_IDATA),
	};

	enum
	{
		fIER_ESTART			= (1 << 3),		// ESTART Enable Start Condition Sent Interrupt
		fIER_ESTOP			= (1 << 2),		// ESTOP Enable Stop Condition Sent Interrupt
		fIER_EADDR			= (1 << 1),		// EADDR Enable Address Phase Sent Interrupt
		fIER_EDATA			= (1 << 0),		// EDATA Enable Data Byte Sent or Received Interrupt
	};

	enum
	{
		fADDR_MASK			= (0xFE),		// ADDR[1:7] Master Address bits[0:6]
		fADDR_ReadWrite		= (1 << 0),		// ReadWrite_ R/W_ bit
		fADDR_WRITE			= (0 << 0),
		fADDR_READ			= (1 << 0),
	};

private:
	volatile UInt8	*iic[kIIC_REG_COUNT];
	bool			fInterruptCapable;
	bool			fInterruptRegistered;
	int				cellID;

	void writeReg(
		int reg_index,
		UInt8 value);

	UInt8 readReg(
		int reg_index);

	IOReturn i2cTransaction(
		IOI2CCommand	*cmd,
		bool			isRead);

	static void sProcessInterrupt(
		OSObject	*target,
		void		*refCon,
		IOService	*nub,
		int			source);

	void processInterrupt(void);

	UInt8					*i2c_data;
	int						i2c_index;
	int						i2c_count;
	volatile UInt32			i2c_xfer;
	bool					i2c_readDirection;
	UInt32					i2c_rate;
	IOReturn				i2c_status;
	UInt32					i2c_state;
	semaphore_t i2c_sema;
};





#define super IOI2CController
OSDefineMetaClassAndStructors( IOI2CControllerPPC, IOI2CController )

bool
IOI2CControllerPPC::start(
	IOService	*provider)
{
//	IOReturn	status;
	OSData		*t;
	UInt32		baseAddress;
	UInt32		steps;
	UInt8		*base;
	char		*resourceName = 0;

	DLOG("+IOI2CControllerPPC::start\n");

	if (0 == (t = OSDynamicCast(OSData, provider->getProperty("AAPL,address"))))
		return false;
	if (0 == (baseAddress = *((UInt32*)t->getBytesNoCopy())))
		return false;
	if (0 == (base = (UInt8 *)ml_io_map(baseAddress, 0x1000)))
		return false;
	if (0 == (t = OSDynamicCast(OSData, provider->getProperty("AAPL,address-step"))))
		return false;
	steps = *((UInt32*)t->getBytesNoCopy());

	if (0 == (t = OSDynamicCast(OSData, provider->getProperty("AAPL,i2c-rate"))))
		return false;
	i2c_rate = *((UInt32*)t->getBytesNoCopy());

	if (0 == (t = OSDynamicCast(OSData, provider->getProperty("interrupts"))))
		fInterruptCapable = FALSE;
	else
		fInterruptCapable = TRUE;

	semaphore_create(current_task(), &i2c_sema, SYNC_POLICY_FIFO, 0);
	if(0==i2c_sema) return false;
	
	DLOG("IOI2CControllerPPC::start base:%08lx steps:%ld\n", (UInt32)base, steps);

	iic[iMODE]		= base + (steps * iMODE);		// Configure the transmission mode of the i2c cell and the databit rate.
	iic[iCNTRL]		= base + (steps * iCNTRL);		// Holds the 4 bits used to start the operations on the i2c interface.
	iic[iSTATUS]	= base + (steps * iSTATUS);		// Status bits for the i2 cell and the i2c interface.
	iic[iISR]		= base + (steps * iISR);	 	// Holds the status bits for the interrupt conditions.
	iic[iIER]		= base + (steps * iIER);		// Eneables the bits that allow the four interrupt status conditions.
	iic[iADDR]		= base + (steps * iADDR);		// Holds the 7 bits address and the R/W bit.
	iic[iSUBADDR]	= base + (steps * iSUBADDR);	// the 8bit subaddress..
	iic[iDATA]		= base + (steps * iDATA);		// from where we read and write data

	writeReg(iIER, 0x00);
	writeReg(iISR, 0x00);

	DLOG("IOI2CControllerPPC::start publishResource...\n");
	if (t = OSDynamicCast(OSData, provider->getProperty("compatible")))
	{
		const char *cstr = (const char *)t->getBytesNoCopy();
		if (cstr)
		while (*cstr)
		{
			if (0 == strncmp("uni-n-i2c-control", cstr, strlen("uni-n-i2c-control")))
			{
				if (provider->getProperty("AAPL,i2c-no-sleep"))
					fU3NoSleep = true;

				cellID = 1;
				resourceName = "IOI2CControllerPPC.uni-n";
				break;
			}
			else
					if (0 == strncmp("mac-io-i2c-control", cstr, strlen("mac-io-i2c-control")))
					{
							fDisablePowerManagement = true;
							cellID = 2;
							resourceName = "IOI2CControllerPPC.mac-io";
							break;
					}

			cstr += (strlen(cstr) + 1);
		}

		if (resourceName == 0)
				DLOG("IOI2CControllerPPC::start publishResource NO MATCH\n");
	}
	else
			DLOG("IOI2CControllerPPC::start publishResource compatible property not found\n");

	if (false == super::start(provider))
	{
			DLOG("-IOI2CControllerPPC::start super::start failed\n");
			return false;
	}

	registerService();

	publishChildren();

	if (resourceName)
	{
			publishResource(resourceName, this);
			DLOG("IOI2CControllerPPC::start publishResource %s\n", resourceName);
	}

	DLOG("-IOI2CControllerPPC::start\n");

	return true;
}

void IOI2CControllerPPC::stop(IOService * provider)
{
		DLOG("IOI2CControllerPPC::stop\n");
		if (fInterruptRegistered)
		{
				fInterruptCapable = FALSE;
				provider->disableInterrupt(0);
				provider->unregisterInterrupt(0);
		}

		super::stop(provider);
}

void IOI2CControllerPPC::free( void )
{
	if(i2c_sema) semaphore_destroy(current_task(), i2c_sema);
	
	super::free();
}


// Subclass required methods...
IOReturn
IOI2CControllerPPC::processLockI2CBus(
				UInt32			bus)
{
		return kIOReturnSuccess;	// Just acknowledge. The superclass handles locking.
}

IOReturn
IOI2CControllerPPC::processUnlockI2CBus(
				UInt32			bus)
{
		return kIOReturnSuccess;	// Just acknowledge. The superclass handles unlocking.
}

IOReturn
IOI2CControllerPPC::processReadI2CBus(
				IOI2CCommand	*cmd)
{
		IOReturn		status;

		if (cmd == NULL)
				return kIOReturnBadArgument;

		cmd->bytesTransfered = 0;
		status = i2cTransaction(cmd, true);

		if (status == kIOReturnTimeout)
				ERRLOG("IOI2CControllerPPC::i2cRead timed out\n");
		else
				if (status != kIOReturnSuccess)
						ERRLOG("IOI2CControllerPPC::i2cRead error: %x\n", status);

		return status;
}

IOReturn
IOI2CControllerPPC::processWriteI2CBus(
				IOI2CCommand	*cmd)
{
		IOReturn		status;

		if (cmd == NULL)
				return kIOReturnBadArgument;

		cmd->bytesTransfered = 0;
		status = i2cTransaction(cmd, false);

		if (status == kIOReturnTimeout)
				ERRLOG("IOI2CControllerPPC::i2cWrite timed out\n");
		else
				if (status != kIOReturnSuccess)
						ERRLOG("IOI2CControllerPPC::i2cWrite error: %x\n", status);

		return status;
}


// *******************************************************************
// I2C Read / Write Interface Methods
// *******************************************************************

IOReturn
IOI2CControllerPPC::i2cTransaction(
				IOI2CCommand	*cmd,
				bool			isRead)
{
		IOReturn		status;
		kern_return_t	rval = 0;
		int				retry;
		bool			intMode;
		AbsoluteTime	deadline;

		UInt8	mode = cmd->mode;
		UInt8	address = cmd->address;
		UInt8	subAddress = cmd->subAddress;
		UInt32	timeout_uS = cmd->timeout_uS;

		// By default we allow up to 5 seconds for a transaction to complete.
		if (timeout_uS < 5000000) // that's the minimum timeout the caller can request.
				timeout_uS = 5000000;
		clock_interval_to_deadline(timeout_uS, kMicrosecondScale, &deadline);

		if (isRead)
				address |= fADDR_READ;
		else
				address &= ~fADDR_READ;

		// Translate IOI2C mode to PPCI2C mode...
		switch (mode)
		{
				default:
				case kI2CMode_Unspecified:	return kIOReturnUnsupported; //	mode = fMODE_APMODE_dumb;	break;
				case kI2CMode_Standard:		mode = fMODE_APMODE_std;	break;
				case kI2CMode_StandardSub:	mode = fMODE_APMODE_sub;	break;
				case kI2CMode_Combined:		mode = fMODE_APMODE_comb;	break;
		}

		switch (cmd->bus)	// bus can be either {0 or 1} only!
		{
				case 0: break;
				case 1: mode |= fMODE_PORTSEL; break;
				default:
						ERRLOG("-i2cTransaction invalid bus:%ld\n", cmd->bus);
						return kIOReturnBadArgument;
		}

		if (i2c_rate < 50)
				mode |= fMODE_CLKDIV_25kHz;
		else
				if (i2c_rate < 100)
						mode |= fMODE_CLKDIV_50kHz;
				else
						mode |= fMODE_CLKDIV_100kHz;

		// Wait up to 1 second for busy=0 before writing mode reg...
		for (retry = 1000; (retry > 0) && (readReg(iSTATUS) & fSTATUS_BUSY); retry--)
		{
				if (ml_at_interrupt_context())
						IODelay(1000);
				else
						IOSleep(1);
		}

		if (retry <= 0)
		{
				ERRLOG("-IOI2CControllerPPC::i2cTransaction IIC cell busy.\n");
				return kIOReturnDeviceError; // 0x2e9
		}

		// Ready to go...
		writeReg(iIER, 0);									// disable all interrupts
		writeReg(iISR, fISR_IMASK);							// clear pending interrupts

		i2c_xfer = true;									// set transfer in progress flag
		i2c_state = 0;
		i2c_status = kIOReturnSuccess;
		i2c_data = cmd->buffer;
		i2c_count = cmd->count;
		i2c_index = 0;
		i2c_readDirection = isRead;

		// Check conditions for an interrupt transaction.
		intMode = ( fInterruptCapable && (false == ml_at_interrupt_context()) && (0 == (cmd->options & kI2COption_NoInterrupts)) );

		if (intMode)
		{
				if (fInterruptRegistered == false)
				{
						DLOG("IOI2CControllerPPC::i2cTransaction calling registerInterrupt\n");
						if (kIOReturnSuccess != (status = fProvider->registerInterrupt(0, this, sProcessInterrupt, 0)))
						{
								ERRLOG("-IOI2CControllerPPC::i2cTransaction registerInterrupt returned:0x%lx\n", status);
								return status;
						}
						fInterruptRegistered = true;
				}

				//		DLOG("IOI2CPPC imode\n");
				if (kIOReturnSuccess != (status = fProvider->enableInterrupt(0)))
				{
						ERRLOG("-IOI2CControllerPPC::i2cTransaction enableInterrupt failed: 0x%08x\n", status);
						return status;
				}
		}
		else
				i2c_state |= 0x80000000;

		writeReg(iMODE, mode);
		writeReg(iADDR, address);
		//	if ((mode == fMODE_APMODE_sub) || (mode == fMODE_APMODE_comb))
		writeReg(iSUBADDR, subAddress);

		writeReg(iISR, fISR_IMASK);							// clear pending interrupts
		writeReg(iIER, fISR_IMASK);							// enable all interrupts
		writeReg(iCNTRL, fCNTRL_XADDR);						// start the bus transaction

		if (intMode == false)
		{
				//		DLOG("IOI2CPPC pmode\n");
				while (i2c_xfer)
				{
						if (readReg(iISR))
								processInterrupt();
						else
						{
								if (ml_at_interrupt_context())
										IODelay(1000);
								else
										IOSleep(1);
						}
				}
		}
		else
		{
            mach_timespec_t timeout = { 5, 0 };    // 5 secs
            rval = semaphore_timedwait(i2c_sema, timeout);
            DLOG("[%p] woke from semaphore, i2c_state = %x\n", this, i2c_state);
		}

		// Clear transfer in progress flag to try preventing the interupt context from calling IOLockWake after a timeout.
		// If it does.. no big deal.
		i2c_xfer = false;

		// Wait for IIC cell to clear its busy bit before the next transaction...
		// This indicates the stop bit was sent and the SCL and SDA lines have deasserted.
		UInt8 reg = readReg(iSTATUS);
		if (reg & fSTATUS_BUSY)
		{
				for (retry = 1000; retry > 0; retry--)
				{
						reg = readReg(iSTATUS);
						if ((reg & fSTATUS_BUSY) == 0)
								break;

						if (ml_at_interrupt_context())
								IODelay(1000);
						else
								IOSleep(1);
				}
				if (retry == 0)
						ERRLOG("IOI2CControllerPPC::i2cTransaction IIC cell got no stop and still busy: 0x%02x\n", reg);

				if (retry < 998)
						ERRLOG("IOI2CControllerPPC::i2cTransaction Waited %d ms for IIC Cell to complete:\n", 1000-retry);
		}

		// This is the end of the transaction.
		writeReg(iIER, 0);									// disable all interrupts
		writeReg(iISR, fISR_IMASK);							// clear pending interrupts

		if (intMode)
		{
				if (kIOReturnSuccess != (status = fProvider->disableInterrupt(0)))
				{
						ERRLOG("IOI2CControllerPPC::i2cTransaction disableInterrupt failed: 0x%08x\n", status);
				}
		}

		// Evaluate transaction status
		if (i2c_status == kIOReturnSuccess)
		{
				// If transaction successful and didn't timeout?
				if (rval == KERN_OPERATION_TIMED_OUT)
				{
						ERRLOG("IOI2CControllerPPC::i2c%c timed-out B:0x%02x A:0x%02x %d/%d i2c_state:0x%08x\n", 
							i2c_readDirection?'R':'W', cmd->bus, address, i2c_index, i2c_count, i2c_state);

						// Only indicate a timeout if the stop still has not occured.
						if (0 == (i2c_state & 0x1000100))
								i2c_status = kIOReturnTimeout; // 0x2d6
				}
		}
		else // kIOReturnAborted or kIOReturnNotResponding
		{
				// kIOReturnAborted is returned only for write transactions during the data phase.
				// It means the slave device no-acked a data byte.

				// kIOReturnNotResponding is returned for address phase no-acks.

				ERRLOG("i2c%c B:0x%02x A:0x%02x %s: %d/%d i2c_state:0x%08x\n", i2c_readDirection?'R':'W', cmd->bus, address, (i2c_status == kIOReturnAborted)?"aborted":(i2c_status == kIOReturnNotResponding)?"not responding":"???", i2c_index, i2c_count, i2c_state);
		}

		// Return the actual number of bytes transfered.
		cmd->bytesTransfered = i2c_index;

		//	kprintf("-i2cTransaction **** done\n");
		return i2c_status;
}


// *******************************************************************
// interrupt processing
// *******************************************************************
#define iLOG(fmt, args...)
//#define iLOG DLOG
void
IOI2CControllerPPC::sProcessInterrupt(
				OSObject	*target,
				void		*refCon,
				IOService	*nub,
				int			source)
{
		IOI2CControllerPPC *self = OSDynamicCast(IOI2CControllerPPC, target);
		if (self)
				self->processInterrupt();
}

void
IOI2CControllerPPC::processInterrupt(void)
{
		register UInt8 byte;
		register UInt8 isrReg;
		register UInt8 statusReg;

		isrReg = readReg(iISR);
        DLOG("+ [%p] IOI2CControllerPPC::processInterrupt iISR=%02x\n", this, isrReg);
        
		if (i2c_readDirection)
		{
				if (isrReg & fISR_IADDR)
				{
						ERRLOG("[%p] i2cR Addr\n", this);
						i2c_state |= 0x01;
						statusReg = readReg(iSTATUS);					// read bus status
                        
						if (statusReg & fSTATUS_LASTAAK)				// got an ack?
						{
								if (i2c_count > 1)							// more than one byte?
								{
										iLOG(" ->aak\n");
										i2c_state |= 0x02;
										writeReg(iCNTRL, fCNTRL_AAK);			// ..signal ack control
								}
								else										// zero or one bytes?
								{
										iLOG(" ->nak\n");
										i2c_state |= 0x04;
										writeReg(iCNTRL, fCNTRL_NoAAK);			// ..signal noack control
								}
						}
						else											// got noack?
						{
								iLOG(" got nak\n");
								i2c_status = kIOReturnNotResponding;
								i2c_state |= 0x08;
								//				writeReg(iCNTRL, fCNTRL_STOP);				// stop is automatically sent after nak
						}

						writeReg(iISR, fISR_IADDR);						// clear interrupt
				}

				if (isrReg & fISR_IDATA)
				{
						iLOG("i2cR Data");
						if (i2c_count)
						{
								byte = readReg(iDATA);						// save next data byte
								i2c_data[i2c_index++] = byte;
								iLOG(" %d/%d=0x%02x",i2c_index,i2c_count,byte);
								i2c_state |= 0x10;
						}

						if (i2c_index < i2c_count)						// more bytes?
						{
								if (i2c_index >= (i2c_count - 1))			// is next byte the last byte?
								{
										iLOG(" ->nak\n");
										writeReg(iCNTRL, fCNTRL_NoAAK);			// signal noack control
										i2c_state |= 0x20;
								}
								else
								{
										iLOG(" ->aak\n");
										writeReg(iCNTRL, fCNTRL_AAK);			// signal ack control
										i2c_state |= 0x40;
								}
						}
						else
						{
								iLOG(" ->stop\n");
								i2c_state |= 0x80;
						}
						writeReg(iISR, fISR_IDATA);						// clear interrupt
				}

				if (isrReg & fISR_ISTOP)
				{
						ERRLOG("[%p] i2cR Stop\n", this);
						//			writeReg(iIER, 0);								// disable all interrupts
						writeReg(iISR, fISR_ISTOP);						// clear stop interrupt
						i2c_state |= 0x100;
						if (i2c_xfer)
						{
								i2c_xfer = false;							// clear transfer in progress flag
								i2c_state |= 0x200;
						}
						else
						{
							ERRLOG("i2cR Stop not i2c_xfer\n");
						}
                        
                        if (0 == (i2c_state & 0x80000000))			// wakeup sleeping i2cTransaction thread if not in polled mode.
                        {
                            kern_return_t result=semaphore_signal(i2c_sema);
                            DLOG("  [%p] processInterrupt: signalled semaphore, result %08x\n", this, result);
                        }
                        else
                        {
                            ERRLOG("i2cR Stop - synchronous, not signalling\n");
                        }
						
				}
		}
		else // write direction
		{
				if (isrReg & fISR_IADDR)
				{
                        ERRLOG("[%p] i2cW Addr\n", this);
						i2c_state |= 0x10000;
						statusReg = readReg(iSTATUS);					// read bus status
						if (statusReg & fSTATUS_LASTAAK)				// got an ack?
						{
								byte = i2c_data[i2c_index++];
								writeReg(iDATA, byte);		// load first data byte
								iLOG(" ack -> %d/%d=0x%02x\n",i2c_index,i2c_count,byte);
								i2c_state |= 0x20000;
						}
						else
						{
								iLOG(" got nak\n");
								i2c_status = kIOReturnNotResponding;
								i2c_state |= 0x80000;
						}
						writeReg(iISR, fISR_IADDR);						// clear interrupt
				}

				if (isrReg & fISR_IDATA)
				{
						iLOG("i2cW Data");
						statusReg = readReg(iSTATUS);					// read bus status
						if (statusReg & fSTATUS_LASTAAK)				// got an ack?
						{
								if (i2c_index < i2c_count)					// more bytes?
								{
										byte = i2c_data[i2c_index++];
										writeReg(iDATA, byte);					// load next data byte
										iLOG(" -> %d/%d=0x%02x\n",i2c_index,i2c_count,byte);
										i2c_state |= 0x100000;
								}
								else										// last byte?
								{
										iLOG(" ->stop\n");
										writeReg(iCNTRL, fCNTRL_STOP);			// signal stop control
										i2c_state |= 0x200000;
								}
						}
						else
						{
								i2c_status = kIOReturnAborted;
								iLOG(" got nak\n");
								i2c_state |= 0x400000;
						}
						writeReg(iISR, fISR_IDATA);						// clear data interrupt
				}

				if (isrReg & fISR_ISTOP)
				{
						ERRLOG("[%p] i2cW Stop\n", this);
						writeReg(iISR, fISR_ISTOP);						// clear stop interrupt
						//			writeReg(iIER, 0);								// disable all interrupts
						i2c_state |= 0x1000000;
						if (i2c_xfer)
						{
								i2c_xfer = false;							// clear transfer in progress flag
								i2c_state |= 0x2000000;
						}
						else
						{
							ERRLOG("i2cW Stop NOT i2c_xfer\n");
						}

                        if (0 == (i2c_state & 0x80000000))			// wakeup sleeping i2cTransaction thread if not in polled mode.
                        {
                            kern_return_t result=semaphore_signal(i2c_sema);
                            DLOG("  [%p] processInterrupt: signalled semaphore, result %08x\n", this, result);
                        }
                        else
                        {
                            ERRLOG("i2cW Stop - synchronous, not signalling\n");
                        }
				}
		}

        ERRLOG("- IOI2CControllerPPC::processInterrupt\n");
}


// *******************************************************************
// IIC Cell Hardware I/O
// *******************************************************************
/*
   static const char *vstrs[]=
   {"iMODE","iCNTRL","iSTATUS","iISR","iIER","iADDR","iSUBADDR","iDATA","iREVNUM","iRISETIMECNT","iBITTIMECNT","UNKNOWN"};

   const char *v2s(UInt8 v)
   {
   if (v < 0x0b)
   return vstrs[v];
   return vstrs[0x0b];
   }
 */

void
IOI2CControllerPPC::writeReg( int reg_index, UInt8 value)
{
		//	DLOG("wReg:%s, %02x\n",v2s(reg_index),value);
		*(iic[reg_index]) = value;
		eieio();
}

UInt8
IOI2CControllerPPC::readReg( int reg_index)
{
		UInt8 value = *(iic[reg_index]);
		eieio();
		//	DLOG("rReg:%s, %02x\n",v2s(reg_index),value);
		return value;
}

IOReturn
IOI2CControllerPPC::setPowerState(
				unsigned long	newPowerState,
				IOService		*dontCare)
{
		if (fU3NoSleep)
		{
				if (newPowerState == kIOI2CPowerState_SLEEP)
				{
						DLOG("IOI2CControllerPPC::setPowerState -> sleep (leave it on for PE4CPU)\n");
						return IOPMAckImplied;
				}
		}
		return super::setPowerState(newPowerState, dontCare);
}

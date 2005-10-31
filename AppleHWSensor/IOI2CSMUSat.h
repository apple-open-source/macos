/*
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CSMUSat.h,v 1.9 2005/06/03 00:24:17 tsherman Exp $
 *
 *  DRI: Paul Resch
 *
 *		$Log: IOI2CSMUSat.h,v $
 *		Revision 1.9  2005/06/03 00:24:17  tsherman
 *		Changed sensor cache update code to use locks instead of a IOCommandGate. This
 *		new implementation guarantees the control loop will not get blocked waiting
 *		for i2c reads (which can be up to 7/10th of second waiting on the SAT).
 *		
 *		Revision 1.8  2005/06/01 23:15:57  tsherman
 *		Marco implemented IOI2CSMUSat "lock-sensor" mechanism
 *		
 *		Revision 1.7  2005/05/24 01:01:50  mpontil
 *		Cached data was invalidated much to frequently due to a time scale error.
 *		(.001 second as opposed to 1 second.)
 *		
 *		Revision 1.6  2005/05/19 22:15:33  tsherman
 *		4095546 - SMUSAT sensors - Update driver to read all SAT sensors with one read (raddog)
 *		
 *		Revision 1.5  2005/04/28 20:20:41  raddog
 *		Initial SAT flash support.  Redo Josephs getProperty code and eliminate getPartition call platform function (use getProperty instead)
 *		
 *		Revision 1.4  2005/04/27 17:41:48  mpontil
 *		Checking in changes from Joseph to supprt current sensors. Also this checkin
 *		does a little of cleaning up and adds a getProperty() to read from SDB in
 *		the same way it is already done for the SMU.
 *		
 *		These changes are tagged before and after with BeforeMarco050427 and
 *		AfterMarco050427.
 *		
 *		Revision 1.3  2005/01/13 01:46:37  dirty
 *		Add cvs header.
 *		
 *		
 */

#ifndef _IOI2CSMUSat_H
#define _IOI2CSMUSat_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPlatformFunction.h"
#include <IOI2C/IOI2CDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommandGate.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
//#define SMUSAT_DEBUG 1

#ifdef SMUSAT_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define kNumRetries			10

enum
{
	kTypeUnknown 			= 0,	// <<-- use type 0 as unknown type so LUN array gets pre-initialized.
	kTypeTemperature 		= 1,
	kTypeADC 				= 2,
	kTypeVoltage 			= 3,
	kTypeCurrent 			= 4,

	kLUN_TABLE_COUNT		= 15
};

enum
{
	kSatSDBPartitionIDRegister				= 0x08,
	kSatSDBPartitionIDSize					= 2,
	kSatSDBPartitionLengthRegister			= 0x09,
	kSatSDBPartitionLengthSize				= 2,
	kSatSDBPartitionDataRegister			= 0x0A,
	kSatSDBPartitionDataSize				= 4,
	
	kSatFlashCommandRegister				= 0x40,			// 4 byte register for flash operations
	kSatFlashStatusRegister					= 0x41,			// 2 byte status register with result of last flash command
	kSatFlashBufferRegister					= 0x42,			// 16 byte buffer for flash data
	kSatFlashCmdReset						= 0x00,			// reset, return status
	kSatFlashCmdRead						= 0x10,			// read 16 bytes into flash buffer
	kSatFlashCmdErase						= 0x20,			// erase a flash block
	kSatFlashCmdWrite						= 0x40,			// write 16 bytes in flash buffer to flash memory
	kSatFlashCmdXFer						= 0xE0,			// transfer to address
	kSatFlashCmdSuccess						= 0,			// xxx - what is returned in status, assume zero for now?
	kSATCacheRegBase						= 0x30,
	kSatRegCacheTop							= 0x38,
	kCacheExpirationIntervalMilliseconds	= 1000,			// One second cache timeout
	kSatFlashTransferLength					= 16,
	kSATFlashNoError						= 0,
	
	// address map
	kSatSPUBaseAddress						= 0xFC000,
	kSatSPUUpdateAddress					= 0xF8000,
	kSatSPUSize								= 0x4000,
	kSatSDBBaseAddress						= 0xF000,
	kSatSDBUpdateAddress					= 0xF800,		// xxx - not yet documented?
	kSatSDBSize								= 0x800,
};

// Assemble the flash params word from the flash cmd (high byte) and the address (low 3 bytes)
inline UInt32 FlashParams (UInt32 flashCmd, UInt32 flashAddr) { return ((flashCmd << 24) | (flashAddr & 0xFFFFFF)); };

struct LogicalUnitNumberTable
{
	UInt32	reg;
	UInt8	type;
}; 

/*
 * If the polling period is 0xFFFFFFFF it is treated the same
 * as if it didn't ever exist
 */
enum {
	kHWSensorPollingPeriodNULL = 0xFFFFFFFF
};

struct SatFlashInput {
	UInt8		*buffer;
	UInt32		length;
	UInt32		flashAddress;
};
struct SatFlashOutput {
	UInt32		result;
};

class IOI2CSMUSat : public IOI2CDevice
{
    OSDeclareDefaultStructors(IOI2CSMUSat)

private:

    task_t					fTask;					// Userclient owning task

	const OSSymbol			*fGetSensorValueSym;
	const OSSymbol			*fSymDownloadCommand;
	const OSSymbol			*fSymGetDBPartition;

	UInt16					fCachedSensorData[8];
	AbsoluteTime			fCacheExpirationTime;
	UInt16					fNumberOfControlLoopsReading;

	LogicalUnitNumberTable	LUNtable[kLUN_TABLE_COUNT]; // 15 possible sensors
	UInt8 					LUNtableElement;		// # of elements
	UInt32					fFlashTransferLength;
	
	// Variables for tracking flash progress
	bool					fFlashInProgress;
	UInt32					fFlashAddress, fBytesDone, fBytesTotal;

	IOReturn				publishChildren(IOService *);
        
	// Hardware Access Methods
	IOReturn				updateSensorCache ( void );
	IOReturn				readI2CCached ( UInt32 subAddress, UInt8 *data, UInt32 count );
	IOReturn				getSensor( UInt32 Reg, SInt32 * temp );
	
	// Flashing Methods
	IOReturn				flashReadWithStatus( UInt32 subAddress, UInt8 *data, UInt32 count );
	IOReturn				flashWriteWithStatus( UInt32 subAddress, UInt8 *data, UInt32 count );

	// Declarations for updating the sensor cache
	IOLock					*fCacheLock;
	IOWorkLoop				*fWorkLoop;
	IOTimerEventSource		*fTimerSource;
	static void				timerEventOccurred(OSObject *, IOTimerEventSource *);

protected:
	
public:

    virtual bool			start( IOService * );
    virtual void			stop( IOService * );
	virtual bool			init( OSDictionary * );
	virtual void			free( void );
	
	// getProperty is overriden to allow handling of sdb-partition-xx accessing
	using IOService::getProperty;
	virtual	OSObject		*getProperty( const OSSymbol *) const;

	// setProperty is overriden to allow handling of cache locking
	using IOService::setProperty;
	virtual bool			setProperty( const OSSymbol *, OSObject * );

	using IOService::callPlatformFunction;
    virtual IOReturn		callPlatformFunction( const OSSymbol *, bool, void *, void *, void *, void * );

	virtual IOReturn		callPlatformFunction( const char *, bool, void *, void *, void *, void * );


	// read a datablock partition
	virtual OSData			*getPartition( UInt32, UInt32 dbSelector = 0);

	IOReturn				flash( UInt8 *, UInt32, UInt32 );
	IOReturn				flashDownload( UInt8 *, UInt32, UInt32, UInt32 );
	IOReturn				reportProgress( UInt32 *, UInt32 *, UInt32 * );
	
    // User client creator:
    // -----------------------------------------------------------------
    IOReturn newUserClient(task_t				owningTask,
								void			*security_id,
								UInt32			type,     // Magic number
								OSDictionary	*properties,
								IOUserClient	**handler);
    IOReturn newUserClient(task_t				owningTask, 
								void			*security_id,
								UInt32			type,     // Magic number
								IOUserClient	**handler);
};

#endif	// _IOI2CSMUSat_H


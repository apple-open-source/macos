/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>

#include "AppleFlashNVRAM.h"

#define __FLASH_NVRAM_DEBUG__ 0


// Note: Since the storage for NVRAM is inside the BootROM's high
//       logevity flash section, it should be good for a lifetime
//       of erase and write cycles.  However, all care should be
//       exercised when writing.  Writes should be done only once
//       per boot, and only if the NVRAM has changed.  If periodic
//       syncing is desired it should have a period no less than
//       five minutes.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Notice that we are using OSDefineMetaClassAndAbstractStructors instead of
// OSDefineMetaClassAndStructors.  This is because AppleFlashNVRAM is a
// virtual base class.  The writeBlock and eraseBlock methods must be
// implemented by the subclasses.

OSDefineMetaClassAndAbstractStructors( AppleFlashNVRAM, IONVRAMController );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleFlashNVRAM::start( IOService* provider )
	{
	IOMemoryMap*				nvramMemoryMap;
	unsigned long				gen1;
	unsigned long				gen2;

	// Get the base address for the nvram.

	if ( ( nvramMemoryMap = provider->mapDeviceMemoryWithIndex( 0 ) ) == 0 )
		{
		return( false );
		}

	mNVRAMBaseAddress = ( unsigned char * ) nvramMemoryMap->getVirtualAddress();

	// Allocte the nvram shadow.

	if ( ( mNVRAMShadow = ( unsigned char * ) IOMalloc( kAppleFlashNVRAMSize ) ) == 0 )
		{
		return( false );
		}

	mCommandGate = IOCommandGate::commandGate( this, sDispatchInternal );

	getWorkLoop()->addEventSource( mCommandGate );

	// Find the current nvram partition and set the next.

	gen1 = validateGeneration( mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset );
	gen2 = validateGeneration( mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset );

	if ( gen1 > gen2 )
		{
		mNVRAMCurrent	= mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset;
		mNVRAMNext		= mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset;
		}
	else
		{
		mNVRAMCurrent	= mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset;
		mNVRAMNext		= mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset;
		}

	// Copy the nvram into the shadow.

	bcopy( ( const void * ) mNVRAMCurrent, mNVRAMShadow, kAppleFlashNVRAMSize );

	return( IONVRAMController::start( provider ) );
	}


IOReturn AppleFlashNVRAM::sDispatchInternal( OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3 )
	{
	AppleFlashNVRAM*		self;
	IOReturn				result = kIOReturnSuccess;
	int						selector;

	self = ( AppleFlashNVRAM * ) owner;
	selector = ( int ) arg0;

	switch ( selector )
		{
		case	kNVRAMReadCommand:
			{
			result = self->readInternal( ( IOByteCount ) arg1, ( UInt8 * ) arg2, ( IOByteCount ) arg3 );
			break;
			}

		case	kNVRAMWriteCommand:
			{
			result = self->writeInternal( ( IOByteCount ) arg1, ( UInt8 * ) arg2, ( IOByteCount ) arg3 );
			break;
			}

		case	kNVRAMSyncCommand:
			{
			result = self->syncInternal();
			break;
			}
		}

	return( result );
	}


IOReturn AppleFlashNVRAM::readInternal( IOByteCount offset, UInt8* buffer, IOByteCount length )
	{
	if ( mNVRAMShadow == NULL )
		{
		return( kIOReturnNotReady );
		}

	if ( ( buffer == NULL ) || ( length == 0 ) || ( offset > kAppleFlashNVRAMSize ) || ( offset >= kAppleFlashNVRAMSize ) ||
		( ( offset + length ) > kAppleFlashNVRAMSize ) )
		{
		return( kIOReturnBadArgument );
		}

	// Copy from NVRAM shadow memory.

	bcopy( mNVRAMShadow + offset, buffer, length );

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAM::writeInternal( IOByteCount offset, UInt8* buffer, IOByteCount length )
	{
	if ( mNVRAMShadow == NULL )
		{
		return( kIOReturnNotReady );
		}

	if ( ( buffer == NULL ) || ( length == 0 ) || ( offset > kAppleFlashNVRAMSize ) || ( offset >= kAppleFlashNVRAMSize ) ||
		( ( offset + length ) > kAppleFlashNVRAMSize ) )
		{
		return( kIOReturnBadArgument );
		}

	// Write to NVRAM shadow memory.

	bcopy( buffer, mNVRAMShadow + offset, length );

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAM::syncInternal( void )
	{
	AppleFlashNVRAMHeader*						header;
	IOReturn									result;
	unsigned char*								tmpExchangeBuffer;
	unsigned long								generation;
	unsigned long								gen1;
	unsigned long								gen2;


	gen1 = validateGeneration( mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset );
	gen2 = validateGeneration( mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset );

	if ( gen1 > gen2 )
		{
		mNVRAMCurrent	= mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset;
		mNVRAMNext		= mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset;
		}
	else
		{
		mNVRAMCurrent	= mNVRAMBaseAddress + kAppleFlashNVRAMAreaBOffset;
		mNVRAMNext		= mNVRAMBaseAddress + kAppleFlashNVRAMAreaAOffset;
		}

	generation = max( gen1, gen2 );

	// Don't write the BootROM if nothing has changed.

	if ( !bcmp( mNVRAMShadow, ( const void * ) mNVRAMCurrent, kAppleFlashNVRAMSize ) )
		{
		return( kIOReturnSuccess );
		}

	header = ( AppleFlashNVRAMHeader * ) mNVRAMShadow;

	header->generation	= ++generation;
	header->checksum	= chrpCheckSum( mNVRAMShadow );
	header->adler32		= adler32( mNVRAMShadow + kAppleFlashNVRAMAdlerStart, kAppleFlashNVRAMAdlerSize );

	if ( ( result = eraseBlock() ) != kIOReturnSuccess )
		{
		return( result );
		}

	if ( ( result = writeBlock( mNVRAMShadow ) ) != kIOReturnSuccess )
		{
		return( result );
		}

	tmpExchangeBuffer = ( unsigned char * ) mNVRAMCurrent;
	mNVRAMCurrent = mNVRAMNext;
	mNVRAMNext = tmpExchangeBuffer;

	return( result );
	}


void AppleFlashNVRAM::sync( void )
	{
	mCommandGate->runCommand( ( void * ) kNVRAMSyncCommand, NULL, NULL, NULL );
	}


IOReturn AppleFlashNVRAM::read( IOByteCount offset, UInt8* buffer, IOByteCount length )
	{
	return( mCommandGate->runCommand( ( void * ) kNVRAMReadCommand, ( void * ) offset, ( void * ) buffer, ( void * ) length ) );
	}


IOReturn AppleFlashNVRAM::write( IOByteCount offset, UInt8* buffer, IOByteCount length )
	{
	return( mCommandGate->runCommand( ( void * ) kNVRAMWriteCommand, ( void * ) offset, ( void * ) buffer, ( void * ) length ) );
	}


unsigned long AppleFlashNVRAM::validateGeneration( unsigned char* nvramBuffer )
	{
	AppleFlashNVRAMHeader*				header = ( AppleFlashNVRAMHeader * ) nvramBuffer;

	// First validate the signature.

	if ( header->signature != kAppleFlashNVRAMSignature )
		{
		return( 0 );
		}

	// Next make sure the header's checksum matches.

	if ( header->checksum != chrpCheckSum( nvramBuffer ) )
		{
		return( 0 );
		}

	// Make sure the Adler checksum matches.

	if ( header->adler32 != adler32( nvramBuffer + kAppleFlashNVRAMAdlerStart, kAppleFlashNVRAMAdlerSize ) )
		{
		return( 0 );
		}

	return( header->generation );
	}


unsigned char AppleFlashNVRAM::chrpCheckSum( unsigned char* buffer )
	{
	long						cnt;
	unsigned char				c_sum;

	c_sum = 0;

	for ( cnt = 0; cnt < 16; cnt++ )
		{
		unsigned char				i_sum;

		// Skip the checksum.

		if ( cnt == 1 )
			continue;

		i_sum = c_sum + buffer[ cnt ];

		if ( i_sum < c_sum )
			{
			i_sum++;
			}

		c_sum = i_sum;
		}

	return( c_sum );
	}


unsigned long AppleFlashNVRAM::adler32( unsigned char* buffer, long length )
	{
	unsigned long				result;
	unsigned long				lowHalf;
	unsigned long				highHalf;
	long						cnt;

	lowHalf = 1;
	highHalf = 0;

	for ( cnt = 0; cnt < length; cnt++ )
		{
		if ( ( cnt % 5000 ) == 0 )
			{
			lowHalf		%= 65521L;
			highHalf	%= 65521L;
			}

		lowHalf		+= buffer[ cnt ];
		highHalf	+= lowHalf;
		}

	lowHalf		%= 65521L;
	highHalf	%= 65521L;

	result = ( highHalf << 16 ) | lowHalf;

	return( result );
	}


OSDefineMetaClassAndStructors( AppleFlashNVRAMMicronSharp, AppleFlashNVRAM );

IOReturn AppleFlashNVRAMMicronSharp::eraseBlock( void )
	{
	IOReturn				result;

	// Write the Erase Setup Command.

	*mNVRAMNext = kAppleFlashNVRAMMicronSharpEraseSetupCmd;
	eieio();

	// Write the Erase Confirm Command.

	*mNVRAMNext = kAppleFlashNVRAMMicronSharpEraseConfirmCmd;
	eieio();

	result = waitForCommandDone();

	// Write the Reset Command.

	*mNVRAMNext = kAppleFlashNVRAMMicronSharpResetDeviceCmd;
	eieio();

	if ( result == kIOReturnSuccess )
		{
		result = verifyEraseBlock();
		}

	return( result );
	}


IOReturn AppleFlashNVRAMMicronSharp::verifyEraseBlock( void )
	{
	long					cnt;

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		if ( mNVRAMNext[ cnt ] != 0xFF )
			{
			return( kIOReturnInvalid );
			}
		}

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAMMicronSharp::writeBlock( unsigned char* sourceAddress )
	{
	IOReturn				result;
	long					cnt;

	// Write the data byte by byte.

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		mNVRAMNext[ cnt ] = kAppleFlashNVRAMMicronSharpWriteSetupCmd;
		eieio();

		mNVRAMNext[ cnt ] = sourceAddress[ cnt ];
		eieio();

		if ( ( result = waitForCommandDone() ) != kIOReturnSuccess )
			break;
		}

	// Write the Reset Command.

	*mNVRAMNext = kAppleFlashNVRAMMicronSharpResetDeviceCmd;
	eieio();

	if ( result == kIOReturnSuccess )
		{
		result = verifyWriteBlock( sourceAddress );
		}

	return( result );
	}


IOReturn AppleFlashNVRAMMicronSharp::verifyWriteBlock( unsigned char* sourceAddress )
	{
	long						cnt;

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		if ( mNVRAMNext[ cnt ] != sourceAddress[ cnt ] )
			{
			return( kIOReturnInvalid );
			}
		}

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAMMicronSharp::waitForCommandDone( void )
	{
	unsigned char				status;

	do
		{
		status = *mNVRAMNext;
		eieio();
		}
	while ( ( status & kAppleFlashNVRAMMicronSharpStatusRegCompletionMask ) == 0 );

	// Check for errors.

	if ( status & kAppleFlashNVRAMMicronSharpStatusRegErrorMask )
		{
		return( kIOReturnInvalid );
		}

	return( kIOReturnSuccess );
	}


OSDefineMetaClassAndStructors( AppleFlashNVRAMAMD, AppleFlashNVRAM );

IOReturn AppleFlashNVRAMAMD::eraseBlock( void )
	{
static unsigned short		commandAddrList[] = {
													kAppleFlashNVRAMAMDCmdAddr1,	kAppleFlashNVRAMAMDUnlockBypass1Cmd,
													kAppleFlashNVRAMAMDCmdAddr2,	kAppleFlashNVRAMAMDUnlockBypass2Cmd,
													kAppleFlashNVRAMAMDCmdAddr1,	kAppleFlashNVRAMAMDEraseSetupCmd,
													kAppleFlashNVRAMAMDCmdAddr1,	kAppleFlashNVRAMAMDUnlockBypass1Cmd,
													kAppleFlashNVRAMAMDCmdAddr2,	kAppleFlashNVRAMAMDUnlockBypass2Cmd,
													0x0,							kAppleFlashNVRAMAMDEraseConfirmCmd,
												};
	IOReturn				result;
	unsigned long			i;

	// The commands need to be written to specific offsets within the block.

	for ( i = 0; i < ( sizeof( commandAddrList ) / ( 2 * sizeof( unsigned short ) ) ); i++ )
		{
		*( mNVRAMNext + commandAddrList[ 2 * i ] ) = commandAddrList[ ( 2 * i ) + 1 ];

		eieio();
		}

	// There is some thought that DQ6 will not begin to toggle until the sector erase timeout
	// has expired.  We have no proof of this, but the AMD field engineer suggested delaying.
	// The documentation says the timeout is 50 microseconds.  Add 10% for slop.

	IODelay( 55 );

	result = waitForCommandDone();

	if ( result == kIOReturnSuccess )
		{
		result = verifyEraseBlock();
		}

	return( result );
	}


IOReturn AppleFlashNVRAMAMD::verifyEraseBlock( void )
	{
	long					cnt;

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		if ( mNVRAMNext[ cnt ] != 0xFF )
			{
#if __FLASH_NVRAM_DEBUG__
			IOLog( "AppleFlashNVRAMAMD::verifyEraseBlock - Byte %d is not 0xFF!\n", ( int ) cnt );
#endif

			return( kIOReturnInvalid );
			}
		}

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAMAMD::writeBlock( unsigned char* sourceAddress )
	{
	IOReturn				result = kIOReturnSuccess;
	long					cnt;

	// Enter UnlockBypass mode.  This allows us to write the data faster, as we
	// don't need to unlock each individual byte.  This should save us 4 writes
	// per byte.

	mNVRAMNext[ kAppleFlashNVRAMAMDCmdAddr1 ] = kAppleFlashNVRAMAMDUnlockBypass1Cmd;
	eieio();

	mNVRAMNext[ kAppleFlashNVRAMAMDCmdAddr2 ] = kAppleFlashNVRAMAMDUnlockBypass2Cmd;
	eieio();

	mNVRAMNext[ kAppleFlashNVRAMAMDCmdAddr1 ] = kAppleFlashNVRAMAMDUnlockBypass3Cmd;
	eieio();

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		mNVRAMNext[ kAppleFlashNVRAMAMDCmdAddr1 ] = kAppleFlashNVRAMAMDWriteConfirmCmd;
		eieio();

		mNVRAMNext[ cnt ] = sourceAddress[ cnt ];
		eieio();

		if ( ( result = waitForCommandDone() ) != kIOReturnSuccess )
			break;
		}

	// We've finished.  Exit out of UnlockBypass mode.

	*mNVRAMNext = kAppleFlashNVRAMAMDUnlockBypassReset1Cmd;
	eieio();

	*mNVRAMNext = kAppleFlashNVRAMAMDUnlockBypassReset2Cmd;
	eieio();

	if ( result == kIOReturnSuccess )
		{
		result = verifyWriteBlock( sourceAddress );
		}

	return( result );
	}


IOReturn AppleFlashNVRAMAMD::verifyWriteBlock( unsigned char* sourceAddress )
	{
	long						cnt;

	for ( cnt = 0; cnt < kAppleFlashNVRAMSize; cnt++ )
		{
		if ( mNVRAMNext[ cnt ] != sourceAddress[ cnt ] )
			{
#if __FLASH_NVRAM_DEBUG__
			IOLog( "AppleFlashNVRAMAMD::verifyWriteBlock - Byte %d is not correct!\n", ( int ) cnt );
#endif

			return( kIOReturnInvalid );
			}
		}

	return( kIOReturnSuccess );
	}


IOReturn AppleFlashNVRAMAMD::waitForCommandDone( void )
	{
	// The AMD part will toggle DQ6 after each read while the operation
	// is in progress.  Once the operation is complete, then D6 will remain
	// constant.

	while ( true )
		{
		unsigned char			lastStatus;
		unsigned char			currentStatus;

		lastStatus = *mNVRAMNext;
		eieio();

		currentStatus = *mNVRAMNext;
		eieio();

		// Are we still toggling?  If not, then we have completed the operation.

		if ( ( ( lastStatus ^ currentStatus ) & kAppleFlashNVRAMAMDStatusRegCompletionMask ) == 0 )
			{
			return( kIOReturnSuccess );
			}

		// DQ6 is still toggling.  Check DQ5.  If set, there was an error handling the operation.

		if ( currentStatus & kAppleFlashNVRAMAMDStatusRegErrorMask )
			{
			// DQ5 is set, check again for error case.

			lastStatus = *mNVRAMNext;
			eieio();

			currentStatus = *mNVRAMNext;
			eieio();

			if ( ( ( lastStatus ^ currentStatus ) & kAppleFlashNVRAMAMDStatusRegCompletionMask ) == 0 )
				{
				// DQ6 has stopped toggling.  There was no error.

				return( kIOReturnSuccess );
				}
			else
				{
				// DQ5 is set and DQ6 is still toggling, there was an error.

#if __FLASH_NVRAM_DEBUG__
				IOLog( "AppleFlashNVRAMAMD::waitForCommandDone - Operation has timed out!  RESET-ing chip.\n" );
#endif

				// Write the Reset Command.  According to AMD, the part will come out of reset in
				// 100 to 150 ns.

				*mNVRAMNext = kAppleFlashNVRAMAMDResetDeviceCmd;
				eieio();

				return( kIOReturnInvalid );
				}
			}

		// We can't time out the AMD part because it ignores RESET commands while it is handling an ERASE or WRITE command.
		// Hopefully, the chip will always time out and allow us to recover.
		}

	// Should never get here.

	return( kIOReturnInvalid );
	}

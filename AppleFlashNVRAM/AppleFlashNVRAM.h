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



#ifndef _IOKIT_APPLEFLASHNVRAM_H
#define _IOKIT_APPLEFLASHNVRAM_H

#include <IOKit/nvram/IONVRAMController.h>
#include <IOKit/IOCommandGate.h>

// Commands for the Micron/Sharp BootROM Flash.
#define kAppleFlashNVRAMMicronSharpIdentifyCmd			( 0x90 )
#define kAppleFlashNVRAMMicronSharpManufactureIDAddr	( 0x00 )
#define kAppleFlashNVRAMMicronSharpDeviceIDAddr			( 0x01 )
#define kAppleFlashNVRAMMicronSharpResetDeviceCmd		( 0xFF )
#define kAppleFlashNVRAMMicronSharpEraseSetupCmd		( 0x20 )
#define kAppleFlashNVRAMMicronSharpEraseConfirmCmd		( 0xD0 )
#define kAppleFlashNVRAMMicronSharpWriteSetupCmd		( 0x40 )
#define kAppleFlashNVRAMMicronSharpWriteConfirmCmd		( 0x70 )

// Commands for the AMD BootROM Flash.
#define kAppleFlashNVRAMAMDCmdAddr1						( 0x555 )
#define kAppleFlashNVRAMAMDCmdAddr2						( 0x2AA )
#define kAppleFlashNVRAMAMDResetDeviceCmd				( 0xF0 )
#define kAppleFlashNVRAMAMDUnlockBypass1Cmd				( 0xAA )
#define kAppleFlashNVRAMAMDUnlockBypass2Cmd				( 0x55 )
#define kAppleFlashNVRAMAMDUnlockBypass3Cmd				( 0x20 )
#define kAppleFlashNVRAMAMDEraseSetupCmd				( 0x80 )
#define kAppleFlashNVRAMAMDEraseConfirmCmd				( 0x30 )
#define kAppleFlashNVRAMAMDWriteConfirmCmd				( 0xA0 )
#define kAppleFlashNVRAMAMDUnlockBypassReset1Cmd		( 0x90 )
#define kAppleFlashNVRAMAMDUnlockBypassReset2Cmd		( 0x00 )


// Status for the Micron/Sharp BootROM Flash.
#define kAppleFlashNVRAMMicronSharpStatusRegErrorMask			( 0x38 )
#define kAppleFlashNVRAMMicronSharpStatusRegVoltageErrorMask	( 0x08 )
#define kAppleFlashNVRAMMicronSharpStatusRegWriteErrorMask		( 0x10 )
#define kAppleFlashNVRAMMicronSharpStatusRegEraseErrorMask		( 0x20 )
#define kAppleFlashNVRAMMicronSharpStatusRegSequenceErrorMask	( 0x30 )
#define kAppleFlashNVRAMMicronSharpStatusRegCompletionMask		( 0x80 )

// Status for the AMD BootROM Flash.
#define kAppleFlashNVRAMAMDStatusRegErrorMask					( 0x20 )
#define kAppleFlashNVRAMAMDStatusRegCompletionMask				( 0x40 )

// Manufacturer Specific Information.
#define kAppleFlashNVRAMMicronManufactureID			( 0x89 )
#define kAppleFlashNVRAMMicronTopBootDeviceID		( 0x98 )
#define kAppleFlashNVRAMMicronBottomBootDeviceID	( 0x99 )
#define kAppleFlashNVRAMSharpManufactureID			( 0xB0 )
#define kAppleFlashNVRAMSharpDeviceID				( 0x4B )


#define kAppleFlashNVRAMSize		( 0x2000 ) // 8K for A and B offsets.  16K combined.
#define kAppleFlashNVRAMMapSize		( kAppleFlashNVRAMSize * 4 )
#define kAppleFlashNVRAMAreaAOffset	( kAppleFlashNVRAMSize * 0 )
#define kAppleFlashNVRAMAreaBOffset	( kAppleFlashNVRAMSize * 1 )
#define kAppleFlashNVRAMAdlerStart	( 20 )
#define kAppleFlashNVRAMAdlerSize	( kAppleFlashNVRAMSize - kAppleFlashNVRAMAdlerStart )
#define kAppleFlashNVRAMSignature	( 0x5A )

struct AppleFlashNVRAMHeader {
	uint8_t				signature;
	uint8_t				checksum;
	uint16_t			length;
	char				name[ 12 ];
	uint32_t			adler32;
	uint32_t			generation;
	uint32_t			reserved1;
	uint32_t			reserved2;
};
typedef struct AppleFlashNVRAMHeader AppleFlashNVRAMHeader;


class AppleFlashNVRAM : public IONVRAMController
	{
	OSDeclareDefaultStructors( AppleFlashNVRAM );

protected:
	volatile unsigned char*			mNVRAMNext;

private:
	enum
	{
		kNVRAMReadCommand,
		kNVRAMWriteCommand,
		kNVRAMSyncCommand
	};

	IOCommandGate*					mCommandGate;
	unsigned char*					mNVRAMBaseAddress;
	volatile unsigned char*			mNVRAMCurrent;
	unsigned char*					mNVRAMShadow;

	virtual			unsigned long	validateGeneration( unsigned char* nvramBuffer );
	virtual			unsigned char	chrpCheckSum( unsigned char* buffer );
	virtual			unsigned long	adler32( unsigned char* buffer, long length );

	// HW specific functions.  Need to be implemented by subclasses.

	virtual			IOReturn		eraseBlock(void) = NULL;
	virtual			IOReturn		writeBlock( unsigned char* sourceAddress ) = NULL;

	static			IOReturn		sDispatchInternal( OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3 );

	// Routines protected against re-entry by IOCommandGate.

	virtual			IOReturn		syncInternal( void );

	virtual			IOReturn		readInternal( IOByteCount offset, UInt8* buffer, IOByteCount length );
	virtual			IOReturn		writeInternal( IOByteCount offset, UInt8* buffer, IOByteCount length );

public:
	virtual			bool			start( IOService* provider );

	virtual			void			sync( void );

	virtual			IOReturn		read( IOByteCount offset, UInt8* buffer, IOByteCount length );
	virtual			IOReturn		write( IOByteCount offset, UInt8* buffer, IOByteCount length );
	};


class AppleFlashNVRAMMicronSharp : public AppleFlashNVRAM
	{
	OSDeclareDefaultStructors( AppleFlashNVRAMMicronSharp );

private:
	virtual			IOReturn		eraseBlock(void);
	virtual			IOReturn		verifyEraseBlock( void );
	virtual			IOReturn		writeBlock( unsigned char* sourceAddress );
	virtual			IOReturn		verifyWriteBlock( unsigned char* sourceAddress );
	virtual			IOReturn		waitForCommandDone( void );
	};


class AppleFlashNVRAMAMD : public AppleFlashNVRAM
	{
	OSDeclareDefaultStructors( AppleFlashNVRAMAMD );

private:
	virtual			IOReturn		eraseBlock(void);
	virtual			IOReturn		verifyEraseBlock( void );
	virtual			IOReturn		writeBlock( unsigned char* sourceAddress );
	virtual			IOReturn		verifyWriteBlock( unsigned char* sourceAddress );
	virtual			IOReturn		waitForCommandDone( void );
	};

#endif /* ! _IOKIT_APPLEFLASHNVRAM_H */

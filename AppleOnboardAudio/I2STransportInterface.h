/*
 *	I2STransportInterface.h
 *
 *	Interface class for audio data transport
 *
 *  Created by Ray Montagne on Mon Mar 12 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 *	
 *
 */

#include <IOKit/IOService.h>
#include "PlatformInterface.h"
#include "TransportInterface.h"

#ifndef	__I2S_TRANSPORT_INTERFACE
#define	__I2S_TRANSPORT_INTERFACE

typedef enum ClockSource 
{
	kClock49MHz				= 49152000,		// 49 MHz clock source
	kClock45MHz				= 45158400,		// 45 MHz clock source
	kClock18MHz				= 18432000		 // 18 MHz clock source
} ClockSource;

//	Number of 18.4320 MHz clock cycles per sample subframe and
//	are used to determine the externally clocked sample rate by
//	comparing the least sigificant 12 bits of the serial format
//	register against the limits described here.
typedef enum ExternalSampleRate {
	kSampleRate_32Khz_LowerLimt		=	574,
	kSampleRate_32Khz_UpperLimt		=	580,
	kSampleRate_44Khz_LowerLimt		=	415,
	kSampleRate_44Khz_UpperLimt		=	419,
	kSampleRate_48Khz_LowerLimt		=	382,
	kSampleRate_48Khz_UpperLimt		=	386,
	kSampleRate_88Khz_LowerLimt		=	207,
	kSampleRate_88Khz_UpperLimt		=	210,
	kSampleRate_96Khz_LowerLimt		=	190,
	kSampleRate_96Khz_UpperLimt		=	194,
	kSampleRate_192Khz_LowerLimt	=	 94,
	kSampleRate_192Khz_UpperLimt	=	 98
} ExternalSampleRate;

//	Format encoding within the serial format register.
typedef enum SoundFormat 
{
    kSndIOFormatI2SSony,
    kSndIOFormatI2S64x,
    kSndIOFormatI2S32x,
    kSndIOFormatUnknown
} SoundFormat;

typedef enum TicksPerFrame 
{
    k64TicksPerFrame		= 64,			// 64 ticks per frame
    k32TicksPerFrame		= 32 			// 32 ticks per frame
} TicksPerFrame;

/*
 * interrupt control register definitions
 */
enum {
    kFrameCountEnableShift		=	31,			// enable frame count interrupt
    kFrameCountPendingShift		=	30,			// frame count interrupt pending
    kMsgFlagEnableShift			=	29,			// enable message flag interrupt
    kMsgFlagPendingShift		=	28,			// message flag interrupt pending
    kNewPeakEnableShift			=	27,			// enable new peak interrupt
    kNewPeakPendingShift		=	26,			// new peak interrupt pending
    kClocksStoppedEnableShift	=	25,			// enable clocks stopped interrupt
    kClocksStoppedPendingShift	=	24,			// clocks stopped interrupt pending
    kExtSyncErrorEnableShift	=	23,			// enable external sync error interrupt
    kExtSyncErrorPendingShift	=	22,			// external sync error interrupt pending
    kExtSyncOKEnableShift		=	21,			// enable external sync OK interrupt
    kExtSyncOKPendingShift		=	20,			// external sync OK interrupt pending
    kNewSampleRateEnableShift	=	19,			// enable new sample rate interrupt
    kNewSampleRatePendingShift	=	18,			// new sample rate interrupt pending
    kStatusFlagEnableShift		=	17,			// enable status flag interrupt
    kStatusFlagPendingShift		=	16			// status flag interrupt pending
};

// serial format register definitions
enum {
    kClockSource18MHz		=	0,			// select 18 MHz clock base:		( kClockSource18MHz << kClockSourceShift )
    kClockSource45MHz		=	1,			// select 45 MHz clock base:		( kClockSource45MHz << kClockSourceShift )
    kClockSource49MHz		=	2,			// select 49 MHz clock base:		( kClockSource49MHz << kClockSourceShift )
    kClockSourceShift		=	30,			// shift to position value in MClk divisor field
    kMClkDivisorShift		=	24,			// shift to position value in MClk divisor field
    kSClkDivisorShift		=	20,			// shift to position value in SClk divisor field
    kBClkMasterShift		=	19,			// shift to position value in SClk divisor field
    kSClkMaster				=	1,			// SClk in master mode:				( kSClkMaster << kBClkMasterShift )
    kSClkSlave				=	0,			// SClk in slave mode:				( kSClkSlave << kBClkMasterShift )
    kSerialFormatShift		=	16,			// shift to position value in I2S serial format field
    kSerialFormatSony		=	0,			// Sony mode						( kSerialFormatSony << kSerialFormatShift )
    kSerialFormat64x		=	1,			// I2S 64x mode						( kSerialFormat64x << kSerialFormatShift )
    kSerialFormat32x		=	2,			// I2S 32x mode						( kSerialFormat32x << kSerialFormatShift )
    kSerialFormatDAV		=	4,			// DAV mode							( kSerialFormatDAV << kSerialFormatShift )
    kSerialFormatSiliLabs	=	5,			// Silicon Labs mode				( kSerialFormatSiliLabs << kSerialFormatShift )
    kExtSampleFreqIntShift	=	12,			// shift to position for external sample frequency interrupt
};

// codec mesage in and out registers are not supported
// data word sizes
enum {
    kNumChannelsInShift		=	24,	     	// shift to get to num channels in
    kDataInSizeShift		=	16,			// shift to get to data in size
    kDataIn16				=	0,			// 16 bit audio data in:			USE:	( kDataIn16 << kDataInSizeShift )
    kDataIn24				=	3,			// 24 bit audio data in:			USE:	( kDataIn24 << kDataInSizeShift )
    kNumChannelsOutShift	=	8,			// shift to get to num channels out
    kDataOutSizeShift		=	0,			// shift to get to data out size
    kDataOut16				=	0,			// 16 bit audio data out:			USE:	( kDataOut16 << kDataOutSizeShift )
    kDataOut24				=	3,			// 24 bit audio data out:			USE:	( kDataOut24 << kDataOutSizeShift )
	kI2sStereoChannels		=	2,			// USE: ( kI2sStereoChannels << kNumChannelsOutShift ) or ( kI2sStereoChannels << kNumChannelsInShift )
	kI2sMonoChannels		=	1			// USE: ( kI2sMonoChannels << kNumChannelsOutShift ) or ( kI2sMonoChannels << kNumChannelsInShift )
};

// peak level subframe select register is not supported
// peak level in meter registers
enum {
    kNewPeakInShift			=	31,       	// shift to get to new peak in
    kNewPeakInMask			=	(1<<31),	// mask new peak in bit
    kHoldPeakInShift		=	30,       	// shift to get to peak hold
    kHoldPeakInMask			=	(1<<30),    // mask hold peak value
    kHoldPeakInEnable		=	(0<<30),	// enable the hold peak register
    kHoldPeakInDisable		=	(1<<30),	// disable the hold peak register (from updating)
    kPeakValueMask			=	0x00FFFFFF	// mask to get peak value
};

class I2STransportInterface : public TransportInterface {

    OSDeclareDefaultStructors ( I2STransportInterface );

public:

	virtual bool		init (PlatformInterface * inPlatformInterface);
	virtual void		free ( void );
	
	virtual IOReturn	transportSetSampleRate ( UInt32 sampleRate );
	virtual IOReturn	transportSetSampleWidth ( UInt32 sampleWidth, UInt32 dmaWidth );
	
	virtual IOReturn	performTransportSleep ( void );
	virtual IOReturn	performTransportWake ( void );
	
	virtual IOReturn	transportBreakClockSelect ( UInt32 clockSource );
	virtual	IOReturn	transportMakeClockSelect ( UInt32 clockSource );

	virtual bool		transportCanClockSelect ( UInt32 clockSource ) {return false;}
	
	virtual UInt32		transportGetSampleRate ( void );

	IOReturn			transportSetPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue );
	UInt32				transportGetPeakLevel ( UInt32 channelTarget );
	
	//	------------------------------
	//	USER CLIENT
	//	------------------------------
	virtual	IOReturn		getTransportInterfaceState ( TransportStateStructPtr outState );
	virtual IOReturn		setTransportInterfaceState ( TransportStateStructPtr inState );
	
private:
	UInt32				mMclkMultiplier;
	UInt32				mSclkMultiplier;
	UInt32				mMClkFrequency;
	UInt32				mSClkFrequency;
	UInt32				mClockSourceFrequency;
	UInt32				mMClkDivisor;				//	source clock frequency Ö mclk frequency = mClkDivisor
	UInt32				mSClkDivisor;				//	mclk clock frequency Ö sclk frequency = sClkDivisor
	UInt32				mMClkDivider;				//	divider calculated from divisor as per I2S I/O Module serial format register specification
	UInt32				mSClkDivider;				//	divider calculated from divisor as per I2S I/O Module serial format register specification
	UInt32				mSerialFormat;				//	value to be written to serial format register
	UInt32				mDataWordSize;				//	value to be written to data word size register
	I2SClockFrequency	mClockSelector;

	Boolean				mHave18MHzClock;
	Boolean				mHave45MHzClock;
	Boolean				mHave49MHzClock;

	IOReturn 			requestClockSources ();
	IOReturn 			releaseClockSources ();	
	
	IOReturn			calculateSerialFormatRegisterValue ( UInt32 sampleRate );
	void				waitForClocksStopped ( void );
};

#endif


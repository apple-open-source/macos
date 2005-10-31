/*
 *  AppleTopazPluginCS8416.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */


#ifndef __AppleTopazPluginCS8416
#define __AppleTopazPluginCS8416

#include "AppleTopazPlugin.h"
#include <IOKit/IOService.h>

enum CS8416_REGISTER_ADDRESS {
	mapControl_0							=	0x00,
	mapControl_1							=	0x01,
	mapControl_2							=	0x02,
	mapControl_3							=	0x03,
	mapControl_4							=	0x04,
	mapSerialAudioDataFormat				=	0x05,
	mapReceiverErrorMask					=	0x06,
	mapInterruptMask						=	0x07,
	mapInterruptModeMSB						=	0x08,
	mapInterruptModeLSB						=	0x09,
	mapReceiverChannelStatus				=	0x0A,
	mapAudioFormatDetect					=	0x0B,
	mapReceiverError						=	0x0C,
	mapInterruptStatus						=	0x0D,
	mapQChannelSubcode						=	0x0E,
	mapQChannelSubcode_0_7					=	0x0E,
	mapQChannelSubcode_8_15					=	0x0F,
	mapQChannelSubcode_16_23				=	0x10,
	mapQChannelSubcode_24_31				=	0x11,
	mapQChannelSubcode_32_39				=	0x12,
	mapQChannelSubcode_40_47				=	0x13,
	mapQChannelSubcode_48_55				=	0x14,
	mapQChannelSubcode_56_63				=	0x15,
	mapQChannelSubcode_64_71				=	0x16,
	mapQChannelSubcode_72_79				=	0x17,
	mapOMCK_RMCK_Ratio						=	0x18,
	mapChannelAStatus						=	0x19,
	mapChannelAStatus_0						=	0x19,
	mapChannelAStatus_1						=	0x1A,
	mapChannelAStatus_2						=	0x1B,
	mapChannelAStatus_3						=	0x1C,
	mapChannelAStatus_4						=	0x1D,
	mapChannelBStatus						=	0x1E,
	mapChannelBStatus_0						=	0x1E,
	mapChannelBStatus_1						=	0x1F,
	mapChannelBStatus_2						=	0x20,
	mapChannelBStatus_3						=	0x21,
	mapChannelBStatus_4						=	0x22,
	mapBurstPreamblePC						=	0x23,
	mapBurstPreamblePC_0					=	0x23,
	mapBurstPreamblePC_1					=	0x24,
	mapBurstPreamblePD						=	0x25,
	mapBurstPreamblePD_0					=	0x25,
	mapBurstPreamblePD_1					=	0x26
};

enum CS8416_Control_0 {
	baFSWCLK								=	6,
		bvOMCKonRMCKwhenSWCLK				=	0,
		bvForceOMCKonRMCK					=	1,
	baTRUNC									=	2,
		bvIncomingNotTruncated				=	0,
		bvIncomingTruncated					=	1
};

enum CS8416_Control_1 {
	baSWCLK									=	7,
		bvDisableAutoClockSwitching			=	0,
		bvEnableAutoClockSwitching			=	1,
	baMUTSAO								=	6,
		bvSDOUTnotMuted						=	0,
		bvSDOUTmuted						=	1,
	baINT									=	4,
		bvIntActiveHigh						=	0,
		bvIntActiveLow						=	1,
		bvIntOpenDrain						=	2,
		bvIntReserved						=	3,
	baHOLD									=	2,
		bvHoldLastSampleOnRxError			=	0,
		bvMuteOnRxError						=	1,
		bvNoChangeOnRxError					=	2,
		bvHoldReserved						=	3,
	baRMCKF									=	1,
		bvRMCKis256fs						=	0,
		bvRMCKis128fs						=	1,
	baCHR									=	0,
		bvChannelStatusFromA				=	0,
		bvChannelStatusFromB				=	1
};

enum CS8416_Control_2 {
	baDETCI									=	7,
		bvEnableDtoEtransfer				=	0,
		bvDisableDtoEtransfer				=	1,
	baEMPH_CN								=	4,
		bvDeEmphaisisOFF					=	0,
		bvDeEmphasis32000					=	1,
		bvDeEmphasis44100					=	2,
		bvDeEmphasis48000					=	3,
		bvDeEmphasisAutoSelect				=	4,
		bvDeEmphasisReserved5				=	5,
		bvDeEmphasisReserved6				=	6,
		bvDeEmphasisReserved7				=	7,
	baGPO0_SEL								=	0,
		bvGPO_FixedLowLevel					=	0x00,
		bvGPO_EMPH							=	0x01,
		bvGPO_INT							=	0x02,
		bvGPO_ChannleStatusBit				=	0x03,
		bvGPO_UserDataBit					=	0x04,
		bvGPO_RsError						=	0x05,
		bvGPO_NonValidityRxError			=	0x06,
		bvGPO_RxChannelStatusBlock			=	0x07,
		bvGPO_96kHz							=	0x08,
		bvGPO_NonAudioInputStream			=	0x09,
		bvGPO_VirtualLRCK					=	0x0A,
		bvGPO_TX_SEL						=	0x0B,
		bvGPO_FixedHighLevel				=	0x0C,
		bvGPO_HRMCK_512fs					=	0x0D,
		bvGPO_Reserved14					=	0x0E,
		bvGPO_Reserved15					=	0x0F
};

enum CS8416_Control_3 {
	baGPO1_SEL								=	3,
	baGPO2_SEL								=	0
};

enum CS8416_Control_4 {
	baRun									=	7,
		bvStopped							=	0,
		bvRunning							=	1,
	baRXD									=	6,
		bvRMCKisOutput						=	0,
		bvRMCKhiZ							=	1,
	baRX_SEL								=	3,
		bvRXP0								=	0x00,		//	еее   USED FOR APPLE HARDWARE   еее
		bvRXP1								=	0x01,
		bvRXP2								=	0x02,
		bvRXP3								=	0x03,
		bvRXP4								=	0x04,
		bvRXP5								=	0x05,
		bvRXP6								=	0x06,
		bvRXP7								=	0x07,
	baTX_SEL								=	0
};

enum CS8416_SerialAudioDataFormat {
	baSOMS									=	7,
		bvSerialOutputSlaveMode				=	0,
		bvSerialOutputMasterMode			=	1,
	baSOSF									=	6,
		bvOSCLKis64fs						=	0,
		bvOSCLKis128fs						=	1,
	baSORES									=	4,
		bvSerialOutputResolution24Bit		=	0,
		bvSerialOutputResolution20Bit		=	1,
		bvSerialOutputResoultion16Bit		=	0,
		bvDirectCopyRxNRZ_AES3				=	1,
	baSOJUST								=	3,
		bvSerialOutputLeftJustified			=	0,
		bvSerialOutputRightJustifed			=	1,
	baSODEL									=	2,
		bvMSBonFirstOSCLKafterOLRCK			=	0,
		bvMSBonSecondOSCLKafterOLRCK		=	1,
	baSOSPOL								=	1,
		bvSDOUTsampledRisingOSCLK			=	0,
		bvSDOUTsampledFallingOSCLK			=	1,
	baSOLRPOL								=	0,
		bvSDOUT_leftChWhenOLRCLKisHigh		=	0,
		bvSDOUT_rightChWhenOLRCLKisHigh		=	1
};

enum CS8416_ReceiverErrorMask {
	baQCRC									=	6,
	baCCRC									=	5,
	baUNLOCK								=	4,
	baINVALID								=	3,
	baCONF									=	2,
	baBIP									=	1,
	baPAR									=	0,
		bvInterruptDisabled					=	0,
		bvInterruptEnabled					=	1
};

enum CS8416_Interrupt {
	baPCCH									=	6,
	baOSLIP									=	5,
	baDETC									=	4,
	baCCH									=	3,
	baRERR									=	2,
	baQCH									=	1,
	baFCH									=	0
};

enum CS8416_ReceiverChannelStatus {
	baAuxDataFieldWidth						=	4,
		bvDataNotPresent					=	0x00,
		bvDataLength_1_bit					=	0x01,
		bvDataLength_2_bit					=	0x02,
		bvDataLength_3_bit					=	0x03,
		bvDataLength_4_bit					=	0x04,
		bvDataLength_5_bit					=	0x05,
		bvDataLength_6_bit					=	0x06,
		bvDataLength_7_bit					=	0x07,
		bvDataLength_Reserved8				=	0x08,
		bvDataLength_Reserved9				=	0x09,
		bvDataLength_Reserved10				=	0x0A,
		bvDataLength_Reserved11				=	0x0B,
		bvDataLength_Reserved12				=	0x0C,
		bvDataLength_Reserved13				=	0x0D,
		bvDataLength_Reserved14				=	0x0E,
		bvDataLength_Reserved15				=	0x0F,
	baChannelStatusBlockFormatIndicator		=	3,
		bvConsumerFormat					=	0,
		bvProfessionalFormat				=	1,
	baSCMS_CopyrightIndicator				=	2,
		bvCopyrightAsserted					=	0,
		bvCopyrightNotAsserted				=	1,
	baSCMS_GenerationIndicator				=	1,
		bvFirstGenerationOrHigher			=	0,
		bvOriginalData						=	1,
	baEMPH									=	0,
		bvPreEmphasisIndicated				=	0,
		bvPreEmphasisNotIndicated			=	1
};

enum CS8416_FormatDetectStatus {
	baPCMdataWasDetected					=	6,
	baIEC61937dataWasDetected				=	5,
	baDTS_LDdataWasDetected					=	4,
	baDTS_CDdataWasDetected					=	3,
	baFormatDetectStatusReserved2			=	2,
	baDigitalSilenceWasDetected				=	1,
	ba96kHzdataWasDetected					=	0
};

enum CS8416_OMCK_RMCK_Ratio {
	baInteger								=	6,
	baFraction								=	0
};

enum CS8416_ID_Version {
	baCS8416_ID								=	4,
		bvCS8416_ID							=	2,
	baCS8416_Version						=	0,
		bvCS8416_Version_A					=	1,
		bvCS8416_Version_B					=	2,
		bvCS8416_Version_C					=	3
};

#define	kControl_0_INIT					(	( bvOMCKonRMCKwhenSWCLK << baFSWCLK ) | ( bvIncomingNotTruncated << baTRUNC )	)

#define	kControl_1_INIT					(	( bvDisableAutoClockSwitching << baSWCLK ) | \
											( bvSDOUTnotMuted << baMUTSAO ) | \
											( bvIntActiveLow << baINT ) | \
											( bvMuteOnRxError << baHOLD ) | \
											( bvRMCKis256fs << baRMCKF ) | \
											( bvChannelStatusFromA << baCHR ) )
											
#if 1
#define	kControl_2_INIT					(	( bvEnableDtoEtransfer << baDETCI ) | ( bvDeEmphaisisOFF << baEMPH_CN ) | ( bvGPO_FixedHighLevel << baGPO0_SEL )	)
#else
#define	kControl_2_INIT					(	( bvEnableDtoEtransfer << baDETCI ) | ( bvDeEmphaisisOFF << baEMPH_CN ) | ( bvGPO_RsError << baGPO0_SEL )	)
#endif
											
#define	kControl_3_INIT					(	( bvGPO_FixedLowLevel << baGPO1_SEL ) | ( bvGPO_FixedLowLevel << baGPO2_SEL )	)
											
#define	kControl_4_INIT					(	( bvStopped << baRun ) | ( bvRMCKisOutput << baRXD ) | ( bvRXP0 << baRX_SEL ) | ( bvRXP0 << baTX_SEL )	)
#define	kControl_4_RUN					(	( bvRunning << baRun ) | ( bvRMCKisOutput << baRXD ) | ( bvRXP0 << baRX_SEL ) | ( bvRXP0 << baTX_SEL )	)
											
#define	kSerialAudioDataFormat_INIT		(	( bvSerialOutputSlaveMode << baSOMS ) | \
											( bvOSCLKis64fs << baSOSF ) | \
											( bvSerialOutputResolution24Bit << baSORES ) | \
											( bvSerialOutputLeftJustified << baSOJUST ) | \
											( bvMSBonSecondOSCLKafterOLRCK << baSODEL ) | \
											( bvSDOUTsampledRisingOSCLK << baSOSPOL ) | \
											( bvSDOUT_rightChWhenOLRCLKisHigh << baSOLRPOL )	)
											
#define kReceiverErrorMask_DISABLE		(	( bvInterruptDisabled << baQCRC ) | \
											( bvInterruptDisabled << baCCRC ) | \
											( bvInterruptDisabled << baUNLOCK ) | \
											( bvInterruptDisabled << baINVALID ) | \
											( bvInterruptDisabled << baCONF ) | \
											( bvInterruptDisabled << baBIP ) | \
											( bvInterruptDisabled << baPAR )	)
											
#define kReceiverErrorMask_ENABLE		(	( bvInterruptDisabled << baQCRC ) | \
											( bvInterruptDisabled << baCCRC ) | \
											( bvInterruptEnabled << baUNLOCK ) | \
											( bvInterruptDisabled << baINVALID ) | \
											( bvInterruptDisabled << baCONF ) | \
											( bvInterruptDisabled << baBIP ) | \
											( bvInterruptDisabled << baPAR )	)

#define kInterruptMask_INIT				(	( bvInterruptDisabled << baPCCH ) | \
											( bvInterruptDisabled << baOSLIP ) | \
											( bvInterruptDisabled << baDETC ) | \
											( bvInterruptDisabled << baCCH ) | \
											( bvInterruptDisabled << baRERR ) | \
											( bvInterruptDisabled << baQCH ) | \
											( bvInterruptDisabled << baFCH )	)
											
#define kInterruptModeMSB_INIT			(	( bvInterruptDisabled << baPCCH ) | \
											( bvInterruptDisabled << baOSLIP ) | \
											( bvInterruptDisabled << baDETC ) | \
											( bvInterruptDisabled << baCCH ) | \
											( bvInterruptDisabled << baRERR ) | \
											( bvInterruptDisabled << baQCH ) | \
											( bvInterruptDisabled << baFCH )	)
											
#define kInterruptModeLSB_INIT			(	( bvInterruptDisabled << baPCCH ) | \
											( bvInterruptDisabled << baOSLIP ) | \
											( bvInterruptDisabled << baDETC ) | \
											( bvInterruptDisabled << baCCH ) | \
											( bvInterruptDisabled << baRERR ) | \
											( bvInterruptDisabled << baQCH ) | \
											( bvInterruptDisabled << baFCH )	)
											
											
//  Low pass filter the clock unlock status here as it may require quite a
//  bit of time before the clock lock status is valid.  Polls occur every 0.5
//  seconds.  Dan Freeman expresses concern that the PLL may not lock for up
//  to 4 seconds.  The kClockLockFilterCountSeed is set to ( 4 seconds / 0.5 seconds)
//  to address Dan's concern.  See AppleTopazPluginCS8416::poll for more info.
#define kNumberOfLockPollsPerSecond		2
#define kTenthSecondsToPLLLock			15
#define kTenthSecondsToPLLUnlock		10
#define kClockLockFilterCountSeed		(( kNumberOfLockPollsPerSecond * kTenthSecondsToPLLLock ) / 10 )
#define kClockUnlockFilterCountSeed		(( kNumberOfLockPollsPerSecond * kTenthSecondsToPLLUnlock ) / 10 )

//  3678605 NOTE:   The following constants are not derived from any specification of the CS8416 but
//					are based on observation of actual operation of the CS8416 and tuning of timing 
//					to produce reliable recovery at the expense of perceived performance as observed
//					in user interface elements.  Do not reduce the values of these constants without
//					performing extensive stress testing!  The counter is seeded to kPollsToDelayReportingAfterWake
//					and counts down resulting in restoration of RUN prior to checking the RATIO.
#if 1
#define kPollsToDelayReportingAfterWake  5												/*  [3678605]   */
#else
#define kPollsToDelayReportingAfterWake  16												/*  [3678605]   */
#define kPollsToRestoreRunAfterWake		 ( kPollsToDelayReportingAfterWake - 4 )		/*  [3678605]   */
#define kPollsToCheckRatioAfterWake		 ( kPollsToRestoreRunAfterWake - 2 )			/*  [3678605]   */
#endif

#define kUnlockFilterCounterSeed		 4												/*  [3678605]   */

#define	kIRQ_HARDWARE_ACK_SEED_COUNT	 2

#define kMAX_OMCK_RMCK_RATIO_STEP        2                                              /*  [4189050]   */


class AppleTopazPluginCS8416 : public AppleTopazPlugin {
    OSDeclareDefaultStructors ( AppleTopazPluginCS8416 );

public:

	virtual	bool 			init ( OSDictionary *properties );
	virtual	bool			start ( IOService * provider );
    virtual void			free ( void );
	
    virtual bool            preDMAEngineInit ( UInt32 autoClockSelectionIsBeingUsed );
	virtual IOReturn		initCodecRegisterCache ( void );
	virtual IOReturn		performDeviceSleep ( void );
	virtual IOReturn		performDeviceWake ( void );
	virtual IOReturn		setChannelStatus ( ChanStatusStructPtr channelStatus ) { return kIOReturnSuccess; }
	virtual IOReturn		breakClockSelect ( UInt32 clockSource );
	virtual IOReturn		makeClockSelectPreLock ( UInt32 clockSource );
	virtual IOReturn		makeClockSelectPostLock ( UInt32 clockSource );
	virtual void			setRunMode ( UInt8 mode );
	virtual UInt8			setStopMode ( void );
	virtual UInt32			getClockLock ( void );
	virtual void			disableReceiverError ( void );

	virtual void			useExternalCLK ( void );
	virtual void			useInternalCLK ( void );

	virtual IOReturn		flushControlRegisters ( void );

	virtual	UInt8			CODEC_GetDataMask ( UInt8 regAddr ) { return 0xFF; }
	virtual IOReturn		CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr );
	virtual IOReturn 		CODEC_IsControlRegister ( UInt8 regAddr );
	virtual IOReturn 		CODEC_IsStatusRegister ( UInt8 regAddr );
	
	virtual IOReturn		getPluginState ( HardwarePluginDescriptorPtr outState );
	virtual IOReturn		setPluginState ( HardwarePluginDescriptorPtr inState );

	virtual bool			supportsDigitalInput ( void ) { return TRUE; }
	virtual bool			supportsDigitalOutput ( void ) { return FALSE; }

	virtual void			poll ( void );
	virtual void			notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue );
    virtual void            decrementDelayPollAfterWakeCounter();
	virtual UInt32			getClockLockTerminalCount ();
	virtual bool			canOnlyMasterTheClock () { return TRUE; }

protected:
	bool					mCodecHasLocked;
	
private:

	UInt32					mDelayPollAfterWakeCounter;			//  [3674345]
    UInt32                  mCached_OMCK_RMCK_Ratio;            //  [4189050]
	UInt8					mRatioEnteringSleep;				//  [3678605]
    UInt8                   mCachedReceiverChannelStatus;       //  [4176686]
	bool					mTopazCS8416isSLEEP;				//  [3678605]
    bool                    mOMCK_RMCK_RatioHasBeenCached;      //  [4189050]
    bool                    mAutoClockSelectionIsBeingUsed;     //  [4244167, 4267209]
};


#endif

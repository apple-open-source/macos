/*
 *  AppleOnboardAudio.h
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Mon Jun 04 2001.
 *  Copyright (c) 2001 Apple Computer Inc. All rights reserved.
 *
 */

#ifndef __APPLEONBOARDAUDIO__
#define __APPLEONBOARDAUDIO__

#include <IOKit/IOInterruptEventSource.h>		//	from:	Kernel.framework/Headers/IOKit/
#include <IOKit/IOUserClient.h>					//	from:	Kernel.framework/Headers/IOKit/
#include <IOKit/IOSyncer.h>						//	from:	Kernel.framework/Headers/IOKit/

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AppleDBDMAAudio.h"
#include "AudioHardwareObjectInterface.h"
#include "AppleOnboardAudioUserClient.h"

#include "PlatformInterface.h"
#include "TransportInterface.h"

enum invokeInternalFunctionSelectors {
	kInvokeHeadphoneInterruptHandler,
	kInvokeSpeakerInterruptHandler
};
		
#define kChangeHandlerDownDelayTime		3000000000ULL				/* 3 seconds					*/
#define kBatteryPowerDownDelayTime		30000000000ULL				/* 30 seconds					*/
#define kACPowerDownDelayTime			300000000000ULL				/* 300 seconds == 5 minutes		*/
#define kiSubMaxVolume					60
#define kiSubVolumePercent				92

#define kRelockToExternalClockMaxNumPolls        4                  /* [4189050] */

#define	kAOAPropertyHeadphoneExclusive	'hpex'						/*	Needs to be added to IOAudioTypes.h and then removed from here	*/

typedef struct {
	UInt32			layoutID;			//	identify the target CPU
	UInt32			portID;				//	identify port (see: AudioPortTypes in AudioHardwareConstants.h)
	UInt32			speakerID;			//	dallas ROM ID (concatenates all fields)
} SpeakerIDStruct;
typedef SpeakerIDStruct * SpeakerIDStructPtr;

class IOAudioControl;

enum outputClipRoutineOptions {
	kFloatToIntClip				= (1 << 0),		// basic clipping
	kPhaseInversionClip			= (1 << 1),		// invert the right channel
	kStereoToLeftChanClip		= (1 << 2),		// mix right channel to left and mute right chan
	kStereoToRightChanClip		= (1 << 3),		// mix left channel to right and mute left chan
	kDelayLeftChan1SampleClip	= (1 << 4),		// insert a 1 sample delay in left channel stream
	kDelayRightChan1SampleClip	= (1 << 5)		// insert a 1 sample delay in right channel stream
};

enum connectionPossibilities {
	kMonoSpeaker				= (1 << 0),
	kStereoSpeaker				= (1 << 1),
	kInternalClock				= (1 << 2),
	kExternalClock				= (1 << 3),
	kMux						= (1 << 4)
};

enum deviceConnections {
	kInternalSpeakerOutput		= 0,
	kHeadphoneOutput			= 1, 
	kLineOutOutput				= 2,
	kExternalSpeakerOutput		= 3,
	kDigitalOutOutput			= 4,
	kDigitalInInput				= 5,
	kLineInInput				= 6,
	kInternalMicInput			= 7,
	kExternalMicInput			= 8
};
	
enum jackStates {
	kRemoved					= 0,
	kInserted					= 1,
	kUnknown					= 2
};

enum inputGainControls {
	kNoInputGainControls		= 0,
	kStereoInputGainControls,
	kMonoInputGainControl
};

typedef struct AOAStateUserClientStruct {
	UInt32				ucPramData;
	UInt32				ucPramVolume;
	UInt32				ucPowerState;
	UInt32				ucLayoutID;
	UInt32				uc_fVersion;											//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngineStatus_fCurrentLoopCount;				//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngineStatus_fLastLoopTime_hi;				//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngineStatus_fLastLoopTime_lo;				//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngineStatus_fEraseHeadSampleFrame;			//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngineState;									//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioStreamFormatExtension_fVersion;				//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioStreamFormatExtension_fFlags;					//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioStreamFormatExtension_fFramesPerPacket;		//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioStreamFormatExtension_fBytesPerPacket;		//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_IOAudioEngine_sampleOffset;							//	(for AOAViewer v3.0.1, 18 Aug. 2004 - RBM)
	UInt32				uc_mDoKPrintfPowerState;
	UInt32				uc_sTotalNumAOAEnginesRunning;
	UInt32				uc_numRunningAudioEngines;
	UInt32				uc_currentAggressivenessLevel;
	UInt32				ucReserved_19;
	UInt32				ucReserved_20;
	UInt32				ucReserved_21;
	UInt32				ucReserved_22;
	UInt32				ucReserved_23;
	UInt32				ucReserved_24;
	UInt32				ucReserved_25;
	UInt32				ucReserved_26;
	UInt32				ucReserved_27;
	UInt32				ucReserved_28;
	UInt32				ucReserved_29;
	UInt32				ucReserved_30;
	UInt32				ucReserved_31;
} AOAStateUserClientStruct, *AOAStateUserClientStructPtr;


#define kPluginPListInputLatency		"InputLatency"
#define kPluginPListOutputLatency		"OutputLatency"
#define kPluginPListAOAAttributes		"AOAAttributes"
#define kPluginPListSoftwareInputGain	"SoftwareInputGain"
#define kPluginPListNumHardwareEQBands	"NumHardwareEQBands"

// Neoborg bring up defines, used to force second AOA instance loading
#define kMultipleDevices				"MultipleDevices"
#define kI2SNode						"i2sNode"
#define kSoundNodePath					"soundNodePath"
#define kMatchProperty					"matchProperty"

#define kSoundEntry						"sound"
#define	kLayoutID						"layout-id"
#define	kCompatible						"compatible"
#define kLayouts						"Layouts"
#define kLayoutIDInfoPlist				"LayoutID"
#define kHardwareObjects				"HardwareObjects"
#define kPlatformObject					"PlatformObject"
#define kAmpRecoveryTime				"AmpRecoveryTime"
#define kInputAutoSelect                "InputAutoSelect"
#define kMicrosecsToSleep				"microsecsToSleep"				/*	[3938771]		*/
#define kExternalClockSelect			"ExternalClockSelect"
#define kExternalClockAutoSelect		"ExternalClockAutoSelect"		/*  For S/DIF input with no hardware Sample Rate Converter	*/
#define kTransportObject				"TransportObject"
#define kControls						"Controls"
#define kFormats						"Formats"
#define kPluginID						"PluginID"
#define kSWInterrupts					"SWInterrupts"
#define kClockLockIntMessage			"ClockLock"
#define kClockUnLockIntMessage			"ClockUnLock"
#define kDigitalInInsertIntMessage		"DigitalInDetectInsert"
#define kDigitalInRemoveIntMessage		"DigitalInDetectRemove"
#define kRemoteChildActiveMessage		"RemoteChildActive"				/*	[3938771]	*/
#define kRemoteChildIdleMessage			"RemoteChildIdle"				/*	[3938771]	*/
#define kRemoteChildSleepMessage		"RemoteChildSleep"				/*	[3938771]	*/
#define	kAOAPowerParentMessage			"AOAPowerParent"				/*	[3938771]	*/
#define kRemoteActiveMessage			"RemoteActive"					/*  [3515371]   */
#define kRemoteIdleMessage				"RemoteIdle"					/*  [3515371]   */
#define kRemoteSleepMessage				"RemoteSleep"					/*  [3515371]   */
#define kSignalProcessing				"SignalProcessing"
#define kSoftwareDSP					"SoftwareDSP"
#define kMaxVolumeOffset				"maxVolumeOffset"
#define kSpeakerID						"SpeakerID"
#define kMicrophoneID					"MicrophoneID"
#define kSiliconVersion                 "SiliconVersion"
#define kPluginRecoveryOrder			"RecoveryOrder"
#define kClipRoutines					"ClipRoutines"
#define kEncoding						"Encoding"
#define kIsMixable						"IsMixable"
#define	kComboInObject					"ComboIn"
#define	kComboOutObject					"ComboOut"
#define kUsesAOAPowerManagement			"UsesAOAPowerManagement"		/*  [3515371]   */
#define kSleepsLayoutIDAOAInstance		"SleepsLayoutIDAOAInstance"		/*  [3515371]   */
#define kUIMutesAmps					"UIMutesAmps"
#define kMuteAmpWhenClockInterrupted    "MuteAmpWhenClockInterrupted"
#define kInputsBitmap					"InputsBitmap"
#define kOutputsBitmap					"OutputsBitmap"
#define kSuppressBootChimeLevelCtrl		"suppressBootChimeLevelControl" /*  [3730863]	*/

#define kComboInNoIrq                   "ComboInNoIrq"                  /*  [4073140,4079688] */
#define kComboOutNoIrq                  "ComboOutNoIrq"                 /*  [4073140,4079688] */

#define kTransportIndex					"TransportIndex"				/*  [3648867]   Provides an association between the 'i2s' IO Module and the I2C address of device attached to it.	*/
																		/*				Used as a <key>TransportIndex</key><integer></integer> value pair where values represent:			*/
																		/*				0 = 'i2s-a', 1 = 'i2s-b', 2 = 'i2s-c', etc...			13 May 2004 rbm								*/

#define	kPlatformInterfaceSupport		"PlatformInterfaceSupport"		/*	bit mapped array passed to PlatformInterface::init	*/

#define kLeftVolControlString			"Left"
#define kRightVolControlString			"Right"
#define kMasterVolControlString			"Master"
#define kMuteControlString				"Mute"
#define	kPlaythroughControlString		"Playthrough"

#define kOutputsList					"Outputs"
#define kHeadphones						"Headphones"
#define kInternalSpeakers				"IntSpeakers"
#define kExternalSpeakers				"ExtSpeakers"
#define kLineOut						"LineOut"
#define kDigitalOut						"DigitalOut"

#define kInputsList						"Inputs"
#define kInternalMic					"InternalMic"
#define kExternalMic					"ExternalMic"
#define kLineIn							"LineIn"
#define kDigitalIn						"DigitalIn"
#define kInputDataMux					"InputDataMux"

#define kPluginPListMasterVol			"master-vol"
#define kPluginPListLeftVol				"left-vol"
#define kPluginPListRightVol			"right-vol"
#define kPluginPListLeftGain			"left-gain"
#define kPluginPListRightGain			"right-gain"
#define kPluginPListMasterGain			"master-gain"

#define kInternalClockString			"InternalClock"
#define kExternalClockString			"ExternalClock"

#define kFloatToIntClipString					"FloatToInt"
#define kIntToFloatClipString					"IntToFloat"
#define kPhaseInversionClipString				"PhaseInversion"
#define kStereoToLeftChanClipString				"StereoToLeft"
#define kStereoToRightChanClipString			"StereoToRight"
#define kDelayLeftChan1SampleClipString			"DelayLeft"
#define kDelayRightChan1SampleClipString		"DelayRight"
#define kBalanceAdjustClipString				"BalanceAdjust"
#define kCopyLeftToRight						"LeftToRight"
#define kCopyRightToLeft						"RightToLeft"
#define kLeftBalanceAdjust						"LeftBalanceAdjust"
#define kRightBalanceAdjust						"RightBalanceAdjust"

#define kNoEQID							0xFFFFFFFF

#define kDelayPollAfterWakeFromSleep	8		/*  [3686032]   */

#define	kPlatformDBDMANoIOString						"DBDMA_NoIO"
#define	kPlatformDBDMAMappedString						"DBDMA_Mapped"
#define	kPlatformDBDMAPlatformFunctionString			"DBDMA_PlatformFunction"
#define	kPlatformDBDMAPlatformFunctionK2String			"DBDMA_PlatformFunctionK2"

#define	kPlatformFCRNoIOString							"FCR_NoIO"
#define	kPlatformFCRMappedString						"FCR_Mapped"
#define	kPlatformFCRPlatformFunctionString				"FCR_PlatformFunction"

#define	kPlatformGPIONoIOString							"GPIO_NoIO"
#define	kPlatformGPIOMappedString						"GPIO_Mapped"
#define	kPlatformGPIOPlatformFunctionString				"GPIO_PlatformFunction"

#define	kPlatformI2CNoIOString							"I2C_NoIO"
#define	kPlatformI2CMappedString						"I2C_Mapped"
#define	kPlatformI2CPlatformFunctionString				"I2C_PlatformFunction"

#define	kPlatformI2SNoIOString							"I2S_NoIO"
#define	kPlatformI2SMappedString						"I2S_Mapped"
#define	kPlatformI2SPlatformFunctionString				"I2S_PlatformFunction"


enum {
	kClockSourceSelectionInternal	= 'int ',
	kClockSourceSelectionExternal	= 'ext '
};


enum {
	kLOG_ENTRY_TO_AOA_METHOD		=	0,
	kLOG_EXIT_FROM_AOA_METHOD
};


class AppleOnboardAudio : public IOAudioDevice
{
	friend class PlatformInterface;
	
    OSDeclareDefaultStructors(AppleOnboardAudio);

protected:
	// general controls : these are the default controls attached to a DMA audio engine
    IOAudioToggleControl *				mOutMuteControl;
    IOAudioToggleControl *				mPlaythruToggleControl;
	IOAudioToggleControl *				mHeadphoneConnected;
	IOAudioToggleControl *				mInputConnectionControl;
	IOAudioToggleControl *				mOutHeadLineDigExclusiveControl;
	IOAudioLevelControl *				mPRAMVolumeControl;
	IOAudioLevelControl *				mOutMasterVolumeControl;
    IOAudioLevelControl *				mOutLeftVolumeControl;
    IOAudioLevelControl *				mOutRightVolumeControl;
    IOAudioLevelControl *				mInLeftGainControl;
    IOAudioLevelControl *				mInRightGainControl;
	IOAudioLevelControl *				mInMasterGainControl;
    IOAudioSelectorControl *			mInputSelector;
	IOAudioSelectorControl *			mOutputSelector;
	IOAudioSelectorControl *			mExternalClockSelector;
	
	IONotifier *						mSysPowerDownNotifier;

    OSArray *							mPluginObjects;
	PlatformInterface *					mPlatformInterface;
	TransportInterface *				mTransportInterface;
	AudioHardwareObjectInterface *		mCurrentOutputPlugin;				
	AudioHardwareObjectInterface *		mCurrentInputPlugin;				

#ifdef THREAD_POWER_MANAGEMENT
    thread_call_t						mPowerThread;
#endif

	thread_call_t						mInitHardwareThread;
	bool								mTerminating;
	bool								mClockSelectInProcessSemaphore;
	bool								mSampleRateSelectInProcessSemaphore;
	bool								mPowerOrderHasBeenSet;			//	[3515371]	rbm		19 Dec 2003
	OSString *							mInternalSpeakerOutputString;
	OSString *							mExternalSpeakerOutputString;
	OSString *							mLineOutputString;
	OSString *							mDigitalOutputString;
	OSString *							mHeadphoneOutputString;
    OSString *                          mInternalMicrophoneInputString;
    OSString *                          mExternalMicrophoneInputString;
    OSString *                          mLineInputString;
    OSString *                          mDigitalInputString;
    
	// we keep the engines around to have a cleaner initHardware
    AppleDBDMAAudio *					mDriverDMAEngine;

    Boolean								mVolMuteActive;
    SInt32								mVolLeft;
    SInt32								mVolRight;
    SInt32								mGainLeft;
    SInt32								mGainRight;
	Boolean								mUseMasterVolumeControl;
	Boolean								mUseInputGainControls;
	Boolean								mUsePlaythroughControl;			//	[3281535]
	UInt32								mLayoutID;
	UInt32								mDetectCollection;
	UInt32								mInternalSpeakerID;
	UInt32								mInternalMicrophoneID;
    UInt32                              mSiliconVersion;
	bool								mCurrentPluginNeedsSoftwareInputGain;
	bool								mCurrentPluginNeedsSoftwareOutputVolume;
	UInt32								mAmpRecoveryMuteDuration;
	
	UInt32								mOutputLatency;
	
	IOTimerEventSource *				idleTimer;
	IOTimerEventSource *				pollTimer;
	UInt32								mCurrentOutputSelection;		//	[3581695]
    Boolean								mIsMute;
    Boolean								mAutoUpdatePRAM;
	Boolean								shuttingDown;

    OSArray	*							AudioSoftDSPFeatures;
	OSString *							mCurrentProcessingOutputString;
	OSString *							mCurrentProcessingInputString;

	// Dynamic variable that handle the connected devices
    sndHWDeviceSpec						currentDevices;
    bool 								fCPUNeedsPhaseInversion;	// true if this CPU's channels are out-of-phase
    bool 								mHasHardwareInputGain;		// aml 5.3.02
	bool 								mRangeInChanged;	
	
	bool								mEncodedOutputFormat;
	
	DualMonoModeType					mInternalMicDualMonoMode;	// aml 6.17.02
	
	UInt32								mProcessingParams[kMaxProcessingParamSize/sizeof(UInt32)];
	bool								disableLoadingEQFromFile;
	SInt32								mCurrentClockSelector;

	IOService *							mProvider;
	
	IOAudioSampleRate					mTransportSampleRate;
	IOSyncer *							mSignal;
	
	UInt32								mMatchingIndex;
	
	static UInt32						sInstanceCount;
	UInt32								mInstanceIndex;
	
	bool								mInputDSPMode;
	OSArray	*							mAOAInstanceArray;
	IONotifier *						aoaNotifier;
	
	UInt32								mCodecLockStatus;
	UInt32								mDigitalInsertStatus;
	
	bool								mUIMutesAmps;
    bool                                mAutoSelectInput;
    bool                                mAutoSelectClock;
    bool                                mDisableAutoSelectClock;
	bool								mMuteAmpWhenClockInterrupted;
    
    bool                                mRelockToExternalClockInProgress;   // [4189050]
    UInt32                              mRelockToExternalClockPollCount;    // [4189050]
	
	bool								mHasSPDIFControl;			// [3639956] Remember we have a selection of SPDIF, we need to know that after wakeup
	SInt32								mOutputSelectorLastValue;	// [3639956] Remember the last user-selected value, so we can restore SPDIF after sleep 
	
public:
	IOInterruptEventSource *			mSoftwareInterruptHandler;
	UInt16								mInterruptProduced[kNumberOfActionSelectors];
	UInt16								mInterruptConsumed[kNumberOfActionSelectors];

	static bool 			aoaPublished (AppleOnboardAudio * aoaObject, void * refCon, IOService * newService);
	virtual bool			aoaPublishedAction ( void * refCon, IOService * newService );
	static void				softwareInterruptHandler (OSObject *, IOInterruptEventSource *, int count);
	virtual UInt32			getLayoutID ( void ) { return mLayoutID; }												//	[3515371]	rbm		19 Dec 2003


	// Classical Unix funxtions
	virtual bool			start (IOService * provider);
    virtual bool			init (OSDictionary * properties);
    virtual void			free ();
	virtual void			stop (IOService * provider);

	virtual bool 			willTerminate ( IOService * provider, IOOptionBits options );
	virtual bool 			handleOpen (IOService * forClient, IOOptionBits	options, void *	arg ) ;
	virtual void			handleClose (IOService * forClient, IOOptionBits options );

	// IOAudioDevice subclass
    virtual bool			initHardware (IOService * provider);
	static void				initHardwareThread (AppleOnboardAudio * aoa, void * provider);
	static IOReturn			initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4);
	virtual IOReturn		protectedInitHardware (IOService * provider);
    virtual IOReturn		createDefaultControls ();
	virtual IOReturn		createInputGainControls (void);
	virtual IOReturn		createOutputVolumeControls (void);
	virtual UInt16			getTerminalTypeForCharCode (UInt32 outputSelection);
    virtual UInt32			getCharCodeForString (OSString * inputString);
	virtual IOReturn		createInputSelectorControl (void);
	virtual IOReturn 		createOutputSelectorControl (void);
	void					createPlayThruControl (void);
	void					removePlayThruControl (void);

	virtual OSString *		getStringForCharCode (UInt32 charCode);
	virtual UInt32			getCharCodeForIntCode (UInt32 inCode);

    virtual IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);

    static IOReturn			outputControlChangeHandler (IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
	static IOReturn			inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue);

	virtual IOReturn		volumeMasterChange (SInt32 newValue);
    virtual IOReturn		volumeLeftChange (SInt32 newValue, bool ignoreMuteState = FALSE);
    virtual IOReturn		volumeRightChange (SInt32 newValue, bool ignoreMuteState = FALSE);
	virtual IOReturn		selectCodecOutputWithMuteState (SInt32 newValue);
    virtual IOReturn		gainLeftChanged (SInt32 newValue);
    virtual IOReturn		gainRightChanged (SInt32 newValue);
	virtual IOReturn		gainMasterChanged (SInt32 newValue);

    virtual IOReturn		passThruChanged (SInt32 newValue);

    virtual IOReturn		inputSelectorChanged (SInt32 newValue);
    virtual IOReturn		outputSelectorChanged (SInt32 newValue);
	virtual IOReturn		clockSelectorChanged (SInt32 newValue);

    virtual IOReturn		performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 * microsecondsUntilComplete);
	static void 			performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState);
	static IOReturn			performPowerStateChangeThreadAction (OSObject * owner, void * newPowerState, void * us, void * arg3, void * arg4);
	virtual	IOReturn		performPowerStateAction ( OSObject * owner, void * newPowerState, void * arg2, void * arg3, void * arg4 );
	virtual IOReturn		performPowerStateChangeAction ( void * newPowerState );
	virtual IOReturn		performPowerStateChangeAction_requestActive ( bool allowDetectIRQDispatch );
	virtual IOReturn		performPowerStateChangeAction_requestIdle ( void );
	virtual IOReturn		performPowerStateChangeAction_requestSleep ( void );
	virtual IOReturn		doLocalChangeToActiveState ( bool allowDetectIRQDispatch, Boolean * wasPoweredDown );		//	[3933529]
	virtual IOReturn		doLocalChangeScheduleIdle ( Boolean wasPoweredDown );										//	[3945202]


	virtual IOReturn		newUserClient (task_t 			inOwningTask,
							void *			inSecurityID,
							UInt32 			inType,
							IOUserClient **	outHandler);

	virtual void			registerPlugin (AudioHardwareObjectInterface * thePlugin);
	static	IOReturn		registerPluginAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4);
	virtual void			unRegisterPlugin (AudioHardwareObjectInterface *inPlugin);

	virtual void			interruptEventHandler (UInt32 statusSelector, UInt32 newValue);
	static	IOReturn		interruptEventHandlerAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4);
	virtual void			protectedInterruptEventHandler (UInt32 statusSelector, UInt32 newValue);
	virtual bool			executeProtectedInterruptEventHandlerWhenInactive ( UInt32 selector );

	virtual void			startDetectInterruptService ( void );
	virtual void			endDetectInterruptService ( void );
	virtual bool			broadcastSoftwareInterruptMessage ( UInt32 actionSelector );

	virtual PlatformInterface * getPlatformInterfaceObject (void);
	virtual IOReturn 		AdjustOutputVolumeControls (AudioHardwareObjectInterface * thePluginObject, UInt32 inSelection);
	virtual IOReturn 		AdjustInputGainControls (AudioHardwareObjectInterface * thePluginObject);

	virtual UInt32			getDeviceIndex () {return mInstanceIndex;}
	
	UInt32					getAOAInstanceIndex () { return mInstanceIndex; }

	virtual UInt32			getTransportIndex ( void ) { return mTransportInterfaceIndex; }					//  [3648867]

	// Functions DBDMAAudio calls on AppleOnboardAudio
	virtual IOReturn		validateOutputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate);
	virtual IOReturn		validateInputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate);
	virtual IOReturn		formatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate);
	virtual	UInt32			getCurrentSampleFrame (void);
	virtual void			setCurrentSampleFrame (UInt32 value);
			UInt32 			getNumHardwareEQBandsForCurrentOutput ();
	AudioHardwareObjectInterface * getCurrentOutputPlugin () { return mCurrentOutputPlugin; };
	//	
	//	User Client Support
	//
	IOReturn				getPlatformState ( UInt32 arg2, void * outState );
	IOReturn 				getPluginState ( HardwarePluginType arg2, void * outState );
	IOReturn 				getDMAStateAndFormat ( UInt32 arg2, void * outState );
	IOReturn				setDMAStateAndFormat ( UInt32 arg2, void * inState );
	IOReturn 				getSoftwareProcessingState ( UInt32 arg2, void * outState );
	IOReturn				getAOAState ( UInt32 arg2, void * outState );
	IOReturn				getTransportInterfaceState ( UInt32 arg2, void * outState );
	IOReturn				getRealTimeCPUUsage ( UInt32 arg2, void * outState );

	IOReturn				setPlatformState ( UInt32 arg2, void * inState );
	IOReturn 				setPluginState ( HardwarePluginType arg2, void * inState );
	IOReturn 				setDMAState ( UInt32 arg2, void * inState );
	IOReturn 				setSoftwareProcessingState ( UInt32 arg2, void * inState );
	IOReturn				setAOAState ( UInt32 arg2, void * inState );
	IOReturn				setTransportInterfaceState ( UInt32 arg2, void * inState );
	
	//	Functions invoked directly on the platform interface object
	virtual void			setInputDataMuxForConnection ( char * connectionString );

	AOAStateUserClientStruct	mUCState;
	
	virtual void			notifyStreamFormatsPublished ( void );		//  [3743041]
	
	bool					getDoKPrintfPowerState () { return mDoKPrintfPowerState; }
	
	UInt32					getDetectCollection () { return mDetectCollection; }
	
protected:
	// Do the link to the IOAudioFamily 
	// These will help to create the port config through the OF Device Tree
            
	void				audioEngineStopped ();
	void				audioEngineStarting ();
	
    IOReturn			configureDMAEngines(IOService *provider);
    IOReturn			parseAndActivateInit(IOService *provider);
    IOReturn			configureAudioDetects(IOService *provider);
    IOReturn			configureAudioOutputs(IOService *provider);
    IOReturn			configureAudioInputs(IOService *provider);
    IOReturn			configurePowerObject(IOService *provider);

	IOReturn			outputControlChangeHandlerAction ( IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue );	//	static handlers dispatch here in virtual space
	IOReturn			inputControlChangeHandlerAction ( IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue );			//	static handlers dispatch here in virtual space
	
	AudioHardwareObjectInterface * getIndexedPluginObject (UInt32 index);
	OSObject * 			getLayoutEntry (const char * entryID, AppleOnboardAudio * theAOA);
	bool				hasMasterVolumeControl (const char * outputEntry);
	bool				hasMasterVolumeControl (const UInt32 inCode);
	bool				hasLeftVolumeControl (const char * outputEntry);
	bool				hasLeftVolumeControl (const UInt32 inCode);
	bool				hasRightVolumeControl (const char * outputEntry);
	bool				hasRightVolumeControl (const UInt32 inCode);

	void				setUseInputGainControls (const char * outputEntry);
	void				setUsePlaythroughControl (const char * inputEntry);			//	[3281535]

	OSArray *			getControlsArray (const char * outputEntry);
	AudioHardwareObjectInterface * getPluginObjectForConnection (const char * entry);
	GpioAttributes		getInputDataMuxForConnection (const char * entry);

	void				setSoftwareOutputDSP (const char * inSelectedOutput);
	void				setSoftwareInputDSP (const char * inSelectedInput);

	UInt32				getMaxVolumeOffsetForOutput (const UInt32 inCode);
	UInt32				getMaxVolumeOffsetForOutput (const char * inSelectedOutput);

	UInt32				setClipRoutineForOutput (const char * inSelectedOutput);
	UInt32				setClipRoutineForInput (const char * inSelectedInput);

	static IOReturn		sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize);
	virtual IOReturn	sysPowerDownHandlerAction ( UInt32 messageType );

    void				setCurrentDevices(sndHWDeviceSpec devices);

	IOReturn			setAggressiveness(unsigned long type, unsigned long newLevel);

	void				createLeftVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createRightVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createMasterVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createLeftGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain);
	void				createRightGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain);
	void				createMasterGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain);

	void				cacheOutputVolumeLevels (AudioHardwareObjectInterface * thePluginObject);
	void				cacheInputGainLevels (AudioHardwareObjectInterface * thePluginObject);
	
	void				removeLeftVolumeControl ();
	void				removeRightVolumeControl ();
	void				removeMasterVolumeControl ();
	void				removeLeftGainControl ();
	void				removeRightGainControl ();
	void				removeMasterGainControl ();

	void				initializeDetectCollection ( void );
	UInt32				getValueForDetectCollection ( UInt32 currentDetectCollection );
	UInt32				parseOutputDetectCollection (void);
	void				updateAllDetectCollection (UInt32 statusSelector, UInt32 newValue);
	UInt32				parseInputDetectCollection (void);
	void				selectOutputAmplifiers (const UInt32 inSelection, const bool inMuteState, const bool inUpdateAll = TRUE);
	void				muteAllAmps();
	UInt32				getSelectorCodeForOutputEvent (UInt32 eventSelector);

	char *				getConnectionKeyFromCharCode (const SInt32 inSelection, const UInt32 inDirection);

	AudioHardwareObjectInterface *	getPluginObjectWithName (OSString * inName);
	IOReturn			callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue);
	IOReturn			callPluginsInOrder (UInt32 inSelector, UInt32 newValue);
	AudioHardwareObjectInterface *	findPluginForType ( HardwarePluginType pluginType );

	void				setPollTimer ();
	static void			pollTimerCallback ( OSObject *owner, IOTimerEventSource *device );
	void				runPollTasksEventHandler ( void );

	static IOReturn		runPolledTasks (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4);
	void				protectedRunPolledTasks ( void );
	bool				isTargetForMessage ( UInt32 index, AppleOnboardAudio * theAOA );
	virtual AppleOnboardAudio* findAOAInstanceWithLayoutID ( UInt32 layoutID );				//	[3515371]	rbm		19 Dec 2003
	UInt32				mUsesAOAPowerManagement;											//	[3515371]	rbm		26 May 2004
	UInt32				mSleepsLayoutIDAOAInstance;											//	[3515371]	rbm		26 May 2004
	bool				mSpressBootChimeLevelControl;										//  [3730863]   rbm		 4 Aug 2004
	
    // The PRAM utility
	UInt32				PRAMToVolumeValue (void);
    UInt8				VolumeToPRAMValue (UInt32 leftVol, UInt32 rightVol);
    void				WritePRAMVol (UInt32 volLeft, UInt32 volRight);
	UInt8				ReadPRAMVol (void);
	
	//	Logging utilities
	void				logPerformPowerStateChangeAction ( UInt32 mInstanceIndex, UInt32 newPowerState, UInt32 curPowerState, bool flag, IOReturn resultCode );
	
	IOTimerEventSource *	theTimerEvent;
	
	UInt32				mTransportInterfaceIndex;											//  [3648867]   rbm		12 May 2004
	bool				mNeedsLockStatusUpdateToUnmute;										//  [3678605]
	UInt32				mDelayPollAfterWakeFromSleep;										//  [3686032]
	UInt32				mPlatformInterfaceSupport;
	UInt32				mNumberOfAOAPowerParents;
	
	bool				mDoKPrintfPowerState;					//	used for SQA/SQE verification
	bool				mAllowDetectIrqDispatchesOnWake;
	bool				mJoinAOAPMTree;														//	[3938771]
	
	UInt32				mCurrentAggressivenessLevel;
	UInt32				mMicrosecsToSleep;													//	[3938771]

	static UInt32		sTotalNumAOAEnginesRunning;											//	[3935620],[3942561]
	bool				mRemoteDetectInterruptEnabled;										//	[3935620],[3942561]
	bool				mRemoteNonDetectInterruptEnabled;									//	[3935620],[3942561]

};

class ConfigChangeHelper {

public:
						ConfigChangeHelper (IOAudioEngine * inEngine, UInt32 inSleep = 0);
						~ConfigChangeHelper ();
	
private:

	IOAudioEngine*		mDriverDMAEngine;

};

#endif

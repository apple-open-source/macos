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

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOUserClient.h>

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
		
#define kBatteryPowerDownDelayTime		30000000000ULL				/* 30 seconds					*/
#define kACPowerDownDelayTime			300000000000ULL				/* 300 seconds == 5 minutes		*/
#define kiSubMaxVolume					60
#define kiSubVolumePercent				92

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

typedef struct AOAStateUserClientStruct {
	UInt32				ucPramData;
	UInt32				ucPramVolume;
	UInt32				ucPowerState;
	UInt32				ucReserved_3;
	UInt32				ucReserved_4;
	UInt32				ucReserved_5;
	UInt32				ucReserved_6;
	UInt32				ucReserved_7;
	UInt32				ucReserved_8;
	UInt32				ucReserved_9;
	UInt32				ucReserved_10;
	UInt32				ucReserved_11;
	UInt32				ucReserved_12;
	UInt32				ucReserved_13;
	UInt32				ucReserved_14;
	UInt32				ucReserved_15;
	UInt32				ucReserved_16;
	UInt32				ucReserved_17;
	UInt32				ucReserved_18;
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

#define kSoundEntry						"sound"
#define	kLayoutID						"layout-id"
#define	kCompatible						"compatible"
#define kLayouts						"Layouts"
#define kLayoutIDInfoPlist				"LayoutID"
#define kHardwareObjects				"HardwareObjects"
#define kPlatformObject					"PlatformObject"
#define kAmpRecoveryTime				"AmpRecoveryTime"
#define kExternalClockSelect			"ExternalClockSelect"
#define kTransportObject				"TransportObject"
#define kControls						"Controls"
#define kFormats						"Formats"
#define kPluginID						"PluginID"
#define kSignalProcessing				"SignalProcessing"
#define kHardwareDSPIndex				"HardwareDSPIndex"
#define kEqualization					"Equalization"
#define kDynamicRange					"DynamicRange"
#define kSoftwareDSP					"SoftwareDSP"
#define kMaxVolumeOffset				"maxVolumeOffset"
#define kSpeakerID						"SpeakerID"
#define kPluginRecoveryOrder			"RecoveryOrder"
#define kClipRoutines					"ClipRoutines"
#define kEncoding						"Encoding"
#define kIsMixable						"IsMixable"

#define kFilterType						"FilterType"
#define kFilterFrequency				"Frequency"
#define kFilterQ						"Q"
#define kFilterGain						"Gain"
#define kFilterRunInSoftware			"runInSoftware"
#define kFilterIndex					"index"

#define kLimiter						"Limiter"
#define kLimiterType					"LimiterType"
#define kLimiterBandIndex				"BandIndex"
#define kLimiterAttackTime				"AttackTime"
#define kLimiterReleaseTime				"ReleaseTime"
#define kLimiterThreshold				"Threshold"
#define kLimiterGain					"Gain"
#define kLimiterRatio					"Ratio"
#define kLimiterLookahead				"Lookahead"

#define kCrossover						"Crossover"
#define kCrossoverFrequency				"Frequency"
#define kCrossoverNumberOfBands			"NumberOfBands"

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

enum {
	kClockSourceSelectionInternal	= 'int ',
	kClockSourceSelectionExternal	= 'ext '
};

class AppleOnboardAudio : public IOAudioDevice
{
    
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
    IOAudioSelectorControl *			mInputSelector;
	IOAudioSelectorControl *			mOutputSelector;
	IOAudioSelectorControl *			mExternalClockSelector;
	
	IONotifier *						mSysPowerDownNotifier;

    OSArray *							mPluginObjects;
	PlatformInterface *					mPlatformInterface;
	TransportInterface *				mTransportInterface;
	AudioHardwareObjectInterface *		mCurrentOutputPlugin;				
	AudioHardwareObjectInterface *		mCurrentInputPlugin;				
    thread_call_t						mPowerThread;
	thread_call_t						mInitHardwareThread;
	bool								mTerminating;
	bool								mHeadLineDigExclusive;
	bool								mClockSelectInProcessSemaphore;
	bool								mSampleRateSelectInProcessSemaphore;
	
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
	UInt32								mSpeakerID;
	bool								mCurrentPluginHasSoftwareInputGain;
	UInt32								mAmpRecoveryMuteDuration;
	
	UInt32								mOutputLatency;
	
	unsigned long long					idleSleepDelayTime;
	IOTimerEventSource *				idleTimer;
	IOTimerEventSource *				pollTimer;
    Boolean								mIsMute;
    Boolean								mAutoUpdatePRAM;
	IOAudioDevicePowerState				ourPowerState;
	Boolean								shuttingDown;

    OSArray	*							AudioSoftDSPFeatures;
	OSString *							mCurrentProcessingOutputString;

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
	
public:
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
    virtual IOReturn		volumeLeftChange (SInt32 newValue);
    virtual IOReturn		volumeRightChange (SInt32 newValue);
	virtual IOReturn		outputMuteChange (SInt32 newValue);
    virtual IOReturn		gainLeftChanged (SInt32 newValue);
    virtual IOReturn		gainRightChanged (SInt32 newValue);
    virtual IOReturn		passThruChanged (SInt32 newValue);

    virtual IOReturn		inputSelectorChanged (SInt32 newValue);
    virtual IOReturn		outputSelectorChanged (SInt32 newValue);
	virtual IOReturn		clockSelectorChanged (SInt32 newValue);

    virtual IOReturn		performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 * microsecondsUntilComplete);
	static void 			performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState);
	static IOReturn			performPowerStateChangeThreadAction (OSObject * owner, void * newPowerState, void * us, void * arg3, void * arg4);

	virtual void			setTimerForSleep ();
	static void				sleepHandlerTimer (OSObject * owner, IOTimerEventSource * sender);
    
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

	virtual PlatformInterface * getPlatformInterfaceObject (void);
	virtual IOReturn 		AdjustOutputVolumeControls (AudioHardwareObjectInterface * thePluginObject, UInt32 inSelection);
	virtual IOReturn 		AdjustInputGainControls (AudioHardwareObjectInterface * thePluginObject);

	// Functions DBDMAAudio calls on AppleOnboardAudio
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
	IOReturn 				getSoftwareProcessingState ( UInt32 arg2, void * outState );
	IOReturn				getAOAState ( UInt32 arg2, void * outState );
	IOReturn				getTransportInterfaceState ( UInt32 arg2, void * outState );

	IOReturn				setPlatformState ( UInt32 arg2, void * inState );
	IOReturn 				setPluginState ( HardwarePluginType arg2, void * inState );
	IOReturn 				setDMAState ( UInt32 arg2, void * inState );
	IOReturn 				setSoftwareProcessingState ( UInt32 arg2, void * inState );
	IOReturn				setAOAState ( UInt32 arg2, void * inState );
	IOReturn				setTransportInterfaceState ( UInt32 arg2, void * inState );
	
	//	Functions invoked directly on the platform interface object
	virtual void			setInputDataMuxForConnection ( char * connectionString );

	AOAStateUserClientStruct	mUCState;

protected:
	// Do the link to the IOAudioFamily 
	// These will help to create the port config through the OF Device Tree
            
    IOReturn			configureDMAEngines(IOService *provider);
    IOReturn			parseAndActivateInit(IOService *provider);
    IOReturn			configureAudioDetects(IOService *provider);
    IOReturn			configureAudioOutputs(IOService *provider);
    IOReturn			configureAudioInputs(IOService *provider);
    IOReturn			configurePowerObject(IOService *provider);

	AudioHardwareObjectInterface * getIndexedPluginObject (UInt32 index);
	OSObject * 			getLayoutEntry (const char * entryID);
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

	UInt32				getMaxVolumeOffsetForOutput (const UInt32 inCode);
	UInt32				getMaxVolumeOffsetForOutput (const char * inSelectedOutput);

	UInt32				setClipRoutineForOutput (const char * inSelectedOutput);
	UInt32				setClipRoutineForInput (const char * inSelectedInput);

	static IOReturn		sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize);

    void				setCurrentDevices(sndHWDeviceSpec devices);

	IOReturn			setAggressiveness(unsigned long type, unsigned long newLevel);

	void				createLeftVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createRightVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createMasterVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume);
	void				createLeftGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain);
	void				createRightGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain);

	void				cacheOutputVolumeLevels (AudioHardwareObjectInterface * thePluginObject);
	void				cacheInputGainLevels (AudioHardwareObjectInterface * thePluginObject);
	
	void				removeLeftVolumeControl ();
	void				removeRightVolumeControl ();
	void				removeMasterVolumeControl ();
	void				removeLeftGainControl ();
	void				removeRightGainControl ();

	void				initializeDetectCollection ( void );
	UInt32				parseOutputDetectCollection (void);
	UInt32				parseInputDetectCollection (void);
	void				selectOutput (const UInt32 inSelection, const bool inUpdateAll = TRUE);
	void				muteAnalogOuts ();
	void				setAnalogCodecMute (UInt32 inValue);

	char *				getConnectionKeyFromCharCode (const SInt32 inSelection, const UInt32 inDirection);

	AudioHardwareObjectInterface *	getPluginObjectWithName (OSString * inName);
	IOReturn			callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue);
	IOReturn			callPluginsInOrder (UInt32 inSelector, UInt32 newValue);
	AudioHardwareObjectInterface *	findPluginForType ( HardwarePluginType pluginType );

	void				setPollTimer ();
	static void			pollTimerCallback ( OSObject *owner, IOTimerEventSource *device );
	void				runPolledTasks ( void );
	
protected:
    // The PRAM utility
	UInt32				PRAMToVolumeValue (void);
    UInt8				VolumeToPRAMValue (UInt32 leftVol, UInt32 rightVol);
    void				WritePRAMVol (UInt32 volLeft, UInt32 volRight);
	UInt8				ReadPRAMVol (void);
	
	IOTimerEventSource *	theTimerEvent;
};

#endif

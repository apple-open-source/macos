#ifndef _APPLEDBDMAAUDIODMAENGINE_H
#define _APPLEDBDMAAUDIODMAENGINE_H

#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>

#include "AudioHardwareCommon.h"

#include "Apple02DBDMAAudioFloatLib.h"

//#define _TIME_CLIP_ROUTINE

// aml 2.28.02 adding header to get constants
#include "AppleiSubEngine.h"

class IOInterruptEventSource;
class IOFilterInterruptEventSource;

// reducing the block size makes the problem happen less often
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS		512
#define DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE		128
#define DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE		44100
#define DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH		16
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS	2

// minimum safety offset, 16 when no iSub, and 45 when iSub is attached
//#define kMinimumLatency			16
#define kMinimumLatency			45
#define kMinimumLatencyiSub		45

typedef enum {							
    e_Mode_Disabled = 0,
    e_Mode_CopyLeftToRight,
    e_Mode_CopyRightToLeft
} DualMonoModeType;

class AppleiSubEngine;

class Apple02DBDMAAudioDMAEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors(Apple02DBDMAAudioDMAEngine)

public:


	virtual void 		free();
	virtual UInt32 		getCurrentSampleFrame();
	virtual bool 		init(OSDictionary 			*properties,
								IOService 			*theDeviceProvider,
								bool				hasInput,
								UInt32				numBlocks = DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS,
								UInt32				blockSize = DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE,
								UInt32				rate = DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE,
								UInt16				bitDepth = DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH,
								UInt16				numChannels = DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS);
    virtual bool 		initHardware(IOService *provider);
    virtual IOReturn	performAudioEngineStart();
    virtual IOReturn 	performAudioEngineStop();
    IOReturn     		restartDMA();
	virtual void 		setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency); 
	virtual void 		stop(IOService *provider);
	static void 		requestiSubClose (IOAudioEngine * audioEngine);
	virtual bool		willTerminate (IOService * provider, IOOptionBits options);
	virtual	OSString *	getGlobalUniqueID();

	virtual IOReturn 	clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn 	convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);


	// [3094574] aml, eliminate copied code
	void				resetiSubProcessingState();
			void  		setPhaseInversion(const bool needsPhaseInversion); 

    inline 	bool  		getPhaseInversion() { return fNeedsPhaseInversion; };

			void  		setRightChanMixed(const bool needsRightChanMixed);
						
    inline	bool  		getRightChanMixed() { return fNeedsRightChanMixed; };
	// [3134221] aml, added mode for delaying right channel to compensate for TAS 3004 phase shift
			void  		setRightChanDelay(const bool needsRightChanDelay);
						
    inline	bool  		getRightChanDelay() { return fNeedsRightChanDelay; };

	void 				setDualMonoMode(const DualMonoModeType inDualMonoMode);

    void		 		setUseSoftwareInputGain(bool inUseSoftwareInputGain);
    void	 			setInputGainL(UInt32 inGainL); 
    void		 		setInputGainR(UInt32 inGainR); 
	void  				setRightChanDelayInput(const bool needsRightChanDelay);

	void 				setBalanceAdjust(const bool needsBalanceAdjust);  
	void 				setLeftBalanceAdjust(UInt32 inVolume);
	void 				setRightBalanceAdjust(UInt32 inVolume); 

 	virtual void 		resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);

    virtual IOReturn	performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
	
	virtual	bool		getDmaState (void );
	virtual IOReturn	getAudioStreamFormat( IOAudioStreamFormat * streamFormatPtr );
    
    static const int 	kDBDMADeviceIndex;
    static const int 	kDBDMAOutputIndex;
    static const int 	kDBDMAInputIndex;

protected:
	IOMemoryDescriptor *			dmaCommandBufferInMemDescriptor;
	IOMemoryDescriptor *			dmaCommandBufferOutMemDescriptor;
	IOMemoryDescriptor *			sampleBufferInMemDescriptor;
	IOMemoryDescriptor *			sampleBufferOutMemDescriptor;
	IOMemoryDescriptor *			stopCommandMemDescriptor;
    IODBDMAChannelRegisters *		ioBaseDMAOutput;
    IODBDMAChannelRegisters *		ioBaseDMAInput;
    IODBDMADescriptor *				dmaCommandBufferOut;
    IODBDMADescriptor *				dmaCommandBufferIn;
    UInt32							commandBufferSize;
	IOFilterInterruptEventSource *	interruptEventSource;
    UInt32							numBlocks;
    UInt32							blockSize;
	
	Boolean							mNeedToRestartDMA;

    // Next lines for iSub
	iSubProcessingParams_t			miSubProcessingParams;

    IOMemoryDescriptor *			iSubBufferMemory; 
	IOAudioToggleControl *			iSubAttach;
    UInt32							ourSampleFrameAtiSubLoop;
    UInt32							clipAdjustment;
    IOService *						ourProvider;
    IONotifier *					iSubEngineNotifier;
    AppleiSubEngine *				iSubEngine;


 
    UInt32							previousClippedToFrame;
    UInt32							initialiSubLead;
	Boolean							needToSync;
    Boolean							startiSub;
    Boolean							justResetClipPosition;


    
	Boolean							restartedDMA;
	Boolean							iSubOpen;

    bool							fNeedsPhaseInversion;
	bool							fNeedsRightChanMixed;
	bool							fNeedsRightChanDelay;
	bool							fNeedsRightChanDelayInput;
	bool							fNeedsBalanceAdjust;
	
	float							mLastInputSample;
	float							mLastOutputSample;

    UInt32							mLeftBalanceAdjust;				
    UInt32							mRightBalanceAdjust;				

	DualMonoModeType				mInputDualMonoMode;

    bool 							mUseSoftwareInputGain;
    float *							mInputGainLPtr;				
    float *							mInputGainRPtr;				

	bool							dmaRunState;			//	rbm 7.12.02 added for user client support
	IOAudioStreamFormat				dbdmaFormat;			//	rbm 7.15.02 added for user client support

    IOAudioStreamDirection			direction;

    virtual bool					filterInterrupt(int index);

	// [3094574] aml, added routines to set member function pointers based on state
	void							chooseOutputClippingRoutinePtr();
	void							chooseInputConversionRoutinePtr();

	void	 						iSubSynchronize(UInt32 firstSampleFrame, UInt32 numSampleFrames);
	void							updateiSubPosition(UInt32 firstSampleFrame, UInt32 numSampleFrames);
	
	static bool 					interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static void 					interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	static IOReturn 				iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue);
    static bool						iSubEnginePublished (Apple02DBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService);
	static IOReturn 				iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static IOReturn 				iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

	// [3094574] aml, added member function pointers
	IOReturn 						(Apple02DBDMAAudioDMAEngine::*mClipAppleLegacyDBDMAToOutputStreamRoutine)(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn 						(Apple02DBDMAAudioDMAEngine::*mConvertInputStreamToAppleLegacyDBDMARoutine)(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

	inline	void					startTiming();
	inline 	void					endTiming();
#ifdef _TIME_CLIP_ROUTINE
	UInt32 							mCallCount;	
	AbsoluteTime					mPreviousUptime;
#endif

#pragma mark ---------------------------------------- 
#pragma mark еее Output Conversion Routines
#pragma mark ---------------------------------------- 

	IOReturn clipLegacyMemCopyToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16InvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn clipAppleLegacyDBDMAToOutputStream32(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
#pragma mark ---------------------------------------- 
#pragma mark еее Output Conversion Routines with iSub
#pragma mark ---------------------------------------- 
	
	IOReturn clipAppleLegacyDBDMAToOutputStream16iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream16iSubInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn clipAppleLegacyDBDMAToOutputStream32iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleLegacyDBDMAToOutputStream32iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
#pragma mark ---------------------------------------- 
#pragma mark еее Input Conversion Routines
#pragma mark ---------------------------------------- 
	
	IOReturn convertAppleLegacyDBDMAFromInputStream16(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleLegacyDBDMAFromInputStream16CopyR2L(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleLegacyDBDMAFromInputStream16CopyL2R(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);	// [3306493]
	IOReturn convertAppleLegacyDBDMAFromInputStream16WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleLegacyDBDMAFromInputStream16DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn convertAppleLegacyDBDMAFromInputStream32(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleLegacyDBDMAFromInputStream32WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleLegacyDBDMAFromInputStream32DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

};

#endif /* _APPLEDBDMAAUDIODMAENGINE_H */

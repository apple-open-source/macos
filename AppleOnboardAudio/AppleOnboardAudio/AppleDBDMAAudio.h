#ifndef _APPLEDBDMAAUDIO_H
#define _APPLEDBDMAAUDIO_H

#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/IOKitKeys.h>

#include "AudioHardwareCommon.h"
#include "PlatformInterface.h"
#include "AppleDBDMAFloatLib.h"

#include "DSP_Manager.h"

//#define _TIME_CLIP_ROUTINE

// aml 2.28.02 adding header to get constants
#include "AppleiSubEngine.h"

class AppleOnboardAudio;
class IOInterruptEventSource;
class IOFilterInterruptEventSource;

// reducing the block size makes the problem happen less often
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS		512
#define DBDMAAUDIODMAENGINE_ROOT_BLOCK_SIZE			64
#define DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE		44100
#define DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH		16
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS	2

#define kDBDMAAttemptsToStop						2

// minimum safety offset, 16 when no iSub, and 45 when iSub is attached
//#define kMinimumLatency			16
#define kMinimumLatency			45
#define kMinimumLatencyiSub		97

#define kChannels				"Channels"
#define kBitDepth				"BitDepth"
#define kBitWidth				"BitWidth"
#define kSampleRates			"SampleRates"

//	[3305011]	begin {
#define	kMAXIMUM_NUMBER_OF_FROZEN_DMA_IRQ_COUNTS		3
//	} end	[3305011]

//
// DBDMA get state
//
typedef enum {
	kGetDMAStateAndFormat			= 0,
	kGetDMAInputChannelCommands,
	kGetDMAOutputChannelCommands,
	kGetInputChannelRegisters,
	kGetOutputChannelRegisters,
	kSetInputChannelRegisters,
	kSetOutputChannelRegisters,
	kGetDMAInputCommands_0,
	kGetDMAInputCommands_1,
	kGetDMAOutputCommands_0,
	kGetDMAOutputCommands_1,
	kSetDMAStateAndFormat
} DMA_STATE_SELECTOR;


enum {
	kDMA_FLAG_useSoftwareOutputVolume			=   0			//  bit address of field set to '1' if driver uses software volume controls
};

typedef struct DBDMAUserClientState_t {
	UInt32		dmaRunState;									// is DMA running flag
	UInt32		numChannels;									// IOAudioStreamFormat member structure
	UInt32		sampleFormat;
	UInt32		numericRepresentation;
	UInt32		bitDepth;
	UInt32		bitWidth;
	UInt32		alignment;
	UInt32		byteOrder;
	UInt32		isMixable;
	UInt32		driverTag;
	UInt32		needsPhaseInversion;							// processing for specific hardware flags
	UInt32		needsRightChanMixed;
	UInt32		needsRightChanDelay;
	UInt32		needsBalanceAdjust;
	UInt32		inputDualMonoMode;
	UInt32 		useSoftwareInputGain;
	UInt32		dmaInterruptCount;
	UInt32		dmaFrozenInterruptCount;
	UInt32		dmaRecoveryInProcess;
	UInt32		dmaStalledCount;
	UInt32		dmaHwDiedCount;
	UInt32		interruptActionCount;
	UInt32		hasInput;
	UInt32		hasOutput;
	float		inputGainL;
	float		inputGainR;
	UInt32		dmaFlags;
	float		softwareOutputLeftVolume;
	float		softwareOutputRightVolume;
	SInt32		softwareOutputMinimumVolume;
	SInt32		softwareOutputMaximumVolume;
	UInt32		reserved_31;
} DBDMAUserClientStruct, *DBDMAUserClientStructPtr;

typedef struct UCIODBDMAChannelCommands {
	UInt32					numBlocks;
	IODBDMADescriptor		channelCommands[1];
};
typedef struct UCIODBDMAChannelCommands UCIODBDMAChannelCommands;
typedef UCIODBDMAChannelCommands * UCIODBDMAChannelCommandsPtr;

class AppleiSubEngine;
class AudioHardwareObjectInterface;

class AppleDBDMAAudio : public IOAudioEngine
{
    OSDeclareDefaultStructors(AppleDBDMAAudio)

public:


	virtual void 		free();
	virtual UInt32 		getCurrentSampleFrame();
	virtual bool 		init(OSDictionary 			*properties,
								PlatformInterface *	inPlatformInterface,
								IOService 			*theDeviceProvider,
								bool				hasInput,
								bool				hasOutput,
								OSArray *			formatsArray,
								UInt32				numBlocks = DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS);
    virtual bool 		initHardware(IOService *provider);
    virtual IOReturn	performAudioEngineStart();
    virtual IOReturn 	performAudioEngineStop();
    IOReturn     		restartDMA();
	virtual void 		setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency); 
	virtual void 		stop(IOService *provider);
	virtual bool		willTerminate (IOService * provider, IOOptionBits options);
	static void 		requestiSubClose (IOAudioEngine * audioEngine);
	virtual bool 		requestTerminate ( IOService * provider, IOOptionBits options );

	virtual void		detach(IOService *provider);
	virtual	OSString *	getGlobalUniqueID();

	virtual IOReturn 	clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn 	convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

			void		resetiSubProcessingState();
	
			void		resetOutputClipOptions();
			void		resetInputClipOptions();
	
			void  		setRightChanMixed(const bool needsRightChanMixed);
    inline	bool  		getRightChanMixed() { return fNeedsRightChanMixed; };
			void  		setRightChanDelay(const bool needsRightChanDelay);

	void 				setDualMonoMode(const DualMonoModeType inDualMonoMode);

    void		 		setUseSoftwareInputGain(bool inUseSoftwareInputGain);
    void	 			setInputGainL(UInt32 inGainL); 
    void		 		setInputGainR(UInt32 inGainR); 
    void		 		setRightChanDelayInput(bool inUseSoftwareInputGain);// [3173869]

    void		 		setUseSoftwareOutputVolume(bool inUseSoftwareOutputVolume, UInt32 inMinLinear = 0, UInt32 inMaxLinear = 0, SInt32 inMindB = 0, SInt32 inMaxdB = 0);
    void	 			setOutputVolumeLeft(UInt32 inVolume); 
    void	 			setOutputVolumeRight(UInt32 inVolume); 

	void				enableOutputProcessing ();
	void				disableOutputProcessing ();
	void				enableInputProcessing ();
	void				disableInputProcessing ();

	void				setOutputSignalProcessing (OSDictionary * inDictionary);
	void				setInputSignalProcessing (OSDictionary * inDictionary);

	AudioHardwareObjectInterface* getCurrentOutputPlugin ();
 	
	virtual void 		resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);

    virtual IOReturn	performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
	
	virtual	bool		getDmaState (void);

	void				updateDSPForSampleRate (UInt32 inSampleRate);
	
	IOReturn			copyDMAStateAndFormat (DBDMAUserClientStructPtr outState);
	IOReturn			setDMAStateAndFormat ( DBDMAUserClientStructPtr inState );
	IOReturn			copyInputChannelCommands ( void * inputChannelCommands );
	IOReturn			copyOutputChannelCommands ( void * outputChannelCommands );
	IOReturn			copyInputChannelRegisters (void * outState);
	IOReturn			copyOutputChannelRegisters (void * outState);
	IOReturn			setInputChannelRegisters (void * inState);
	IOReturn			setOutputChannelRegisters (void * inState);


#ifdef _TIME_CLIP_ROUTINE
	UInt64				getTotalNanos () { return mCurrentTotalNanos; }
#endif	
    static const int 	kDBDMADeviceIndex;
    static const int 	kDBDMAOutputIndex;
    static const int 	kDBDMAInputIndex;
#if 0
	virtual bool		getRunEraseHead ( void ) { return ( false ); }
#endif

	//	[3305011]	begin {
	bool				engineDied ( void );
	//	} end	[3305011]

protected:
	IOMemoryDescriptor *			dmaCommandBufferInMemDescriptor;
	IOMemoryDescriptor *			dmaCommandBufferOutMemDescriptor;
	IOMemoryDescriptor *			sampleBufferInMemDescriptor;
	IOMemoryDescriptor *			sampleBufferOutMemDescriptor;
	IOMemoryDescriptor *			stopCommandInMemDescriptor;
	IOMemoryDescriptor *			stopCommandOutMemDescriptor;
    IODBDMAChannelRegisters *		ioBaseDMAOutput;
    IODBDMAChannelRegisters *		ioBaseDMAInput;
    IODBDMADescriptor *				dmaCommandBufferOut;
    IODBDMADescriptor *				dmaCommandBufferIn;
	IOAudioStream *					mOutputStream;
	IOAudioStream *					mInputStream;
	OSArray *						deviceFormats;
	void *							mOutputSampleBuffer;
	void *							mIntermediateSampleBuffer;
	void *							mInputSampleBuffer;
    UInt32							commandBufferSize;
	IOFilterInterruptEventSource *	interruptEventSource;
    UInt32							numBlocks;
    UInt32							blockSize;
	PlatformInterface *				mPlatformObject;
	
	//	[3305011]	begin {
	UInt32							mDmaInterruptCount;
	UInt32							mLastDmaInterruptCount;
	UInt32							mNumberOfFrozenDmaInterruptCounts;
	Boolean							mDmaRecoveryInProcess;
	UInt32							mDmaStalledCount;						//	[3514709]	for user client
	UInt32							mDmaHwDiedCount;						//	[3514709]	for user client
	UInt32							mInterruptActionCount;					//	[3514709]	for user client
	//	} end	[3305011]

	Boolean							mNeedToRestartDMA;
	
	Boolean							mHasInput;
	Boolean							mHasOutput;

    // Next lines for iSub
	iSubProcessingParams_t			miSubProcessingParams;

    IOMemoryDescriptor *			iSubBufferMemory; 
	IOAudioToggleControl *			iSubAttach;
    UInt32							ourSampleFrameAtiSubLoop;
    UInt32							clipAdjustment;
    AppleOnboardAudio *				ourProvider;
    IONotifier *					iSubEngineNotifier;
    AppleiSubEngine *				iSubEngine;
	
	IOService *						mDeviceProvider;
 
    UInt32							previousClippedToFrame;
    UInt32							initialiSubLead;
	Boolean							needToSync;
    Boolean							startiSub;
    Boolean							justResetClipPosition;
   
	Boolean							restartedDMA;
	Boolean							iSubOpen;

	bool							fNeedsRightChanMixed;
	
	DualMonoModeType				mInputDualMonoMode;

	bool							mUseSoftwareOutputVolume;
	float							mLeftVolume[1];
	float							mRightVolume[1];
	float							mPreviousLeftVolume[1];
	float							mPreviousRightVolume[1];
	UInt32							mMinVolumeLinear;
	UInt32							mMaxVolumeLinear;
	SInt32							mMinVolumedB;
	SInt32							mMaxVolumedB;

    bool 							mUseSoftwareInputGain;
    float *							mInputGainLPtr;				
    float *							mInputGainRPtr;				
	bool							fNeedsRightChanDelayInput;// [3173869]

	
	bool							dmaRunState;			//	rbm 7.12.02 added for user client support
	IOAudioStreamFormat				mDBDMAOutputFormat;		//	rbm 7.15.02 added for user client support
	IOAudioStreamFormat				mDBDMAInputFormat;		//	mpc changed to allow for different formats of input and output dma

    IOAudioStreamDirection			direction;

	float							mLastInputSample;
	float							mLastOutputSample;

    virtual bool					filterInterrupt(int index);

	void							chooseOutputClippingRoutinePtr();
	void							chooseInputConversionRoutinePtr();

	bool							publishStreamFormats (void);
	void							allocateDMABuffers (void);
	bool							allocateOutputDMADescriptors (void);
	bool							allocateInputDMADescriptors (void);
	bool							createDMAPrograms ( void );
	void							deallocateDMAMemory ();

	void	 						iSubSynchronize(UInt32 firstSampleFrame, UInt32 numSampleFrames);
	void							updateiSubPosition(UInt32 firstSampleFrame, UInt32 numSampleFrames);

	UInt32							GetEncodingFormat (OSString * theEncoding);
	static bool 					interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static void 					interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	static IOReturn 				iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue);
    static bool						iSubEnginePublished (AppleDBDMAAudio * dbdmaEngineObject, void * refCon, IOService * newService);
	static IOReturn 				iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static IOReturn 				iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

	IOReturn 						(AppleDBDMAAudio::*mClipAppleDBDMAToOutputStreamRoutine)(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn 						(AppleDBDMAAudio::*mConvertInputStreamToAppleDBDMARoutine)(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

	inline	void					startTiming();
	inline 	void					endTiming();
	
	float*							mMixBufferPtr;
	
#ifdef _TIME_CLIP_ROUTINE
	UInt32 							mCallCount;	
	AbsoluteTime					mStartIOProcUptime;
	AbsoluteTime					mEndProcessingUptime;
	UInt64							mCurrentTotalNanos;
	UInt64							mTotalIOProcNanos;
	UInt64							mTotalProcessingNanos;
	float							mCurrentCPUUsagePercent;
	float							mAverageCPUUsagePercent;
#endif

#pragma mark ---------------------------------------- 
#pragma mark еее Output Conversion Routines
#pragma mark ---------------------------------------- 
	inline void outputProcessing (float* inFloatBufferPtr, UInt32 inNumSamples);
	inline void setupOutputBuffer (const void *mixBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

	IOReturn clipMemCopyToOutputStream (const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleDBDMAToOutputStream16(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleDBDMAToOutputStream16MixRightChannel(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn clipAppleDBDMAToOutputStream32(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleDBDMAToOutputStream32MixRightChannel(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

#pragma mark ---------------------------------------- 
#pragma mark еее Output Conversion Routines with iSub
#pragma mark ---------------------------------------- 
	
	IOReturn clipAppleDBDMAToOutputStream16iSub(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleDBDMAToOutputStream16iSubMixRightChannel(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn clipAppleDBDMAToOutputStream32iSub(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn clipAppleDBDMAToOutputStream32iSubMixRightChannel(const void *inFloatBufferPtr, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

#pragma mark ---------------------------------------- 
#pragma mark еее Input Conversion Routines
#pragma mark ---------------------------------------- 
	inline void inputProcessing (float* inFloatBufferPtr, UInt32 inNumSamples);
	
	IOReturn convertAppleDBDMAFromInputStream16(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleDBDMAFromInputStream16CopyR2L(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleDBDMAFromInputStream16WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	
	IOReturn convertAppleDBDMAFromInputStream32(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
	IOReturn convertAppleDBDMAFromInputStream32WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

};

#endif /* _APPLEDBDMAAUDIO_H */

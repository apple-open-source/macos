#ifndef _APPLEDBDMAAUDIODMAENGINE_H
#define _APPLEDBDMAAUDIODMAENGINE_H

#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioStream.h>

#include "AudioHardwareCommon.h"

// aml 2.28.02 adding header to get constants
#include "AppleiSubEngine.h"

class IOInterruptEventSource;
class IOFilterInterruptEventSource;

// reducing the block size makes the problem happen less often
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS	512
#define DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE	128
#define DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE	44100
#define DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH	16
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS	2
// minimum safety offset, 16 when no iSub, and 45 when iSub is attached
#define kMinimumLatency		16
#define kMinimumLatencyiSub	45

// aml 6.17.02
typedef enum {							
    e_Mode_Disabled = 0,
    e_Mode_CopyLeftToRight,
    e_Mode_CopyRightToLeft
} DualMonoModeType;

typedef struct _sPreviousValues {
    float	xl_1;
    float	xr_1;
    float	xl_2;
    float	xr_2;
    float	yl_1;
    float	yr_1;
    float	yl_2;
    float	yr_2;
} PreviousValues;

class AppleiSubEngine;

class AppleDBDMAAudioDMAEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors(AppleDBDMAAudioDMAEngine)

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

    // Next lines for iSub
    UInt32							iSubLoopCount;
    SInt32							iSubBufferOffset;		// Where we are writing to in the iSub buffer
	IOAudioToggleControl *			iSubAttach;
    UInt32							ourSampleFrameAtiSubLoop;
    UInt32							previousClippedToFrame;
    UInt32							initialiSubLead;
    UInt32							clipAdjustment;
    IOService *						ourProvider;
    IONotifier *					iSubEngineNotifier;
    AppleiSubEngine *				iSubEngine;
    float *							lowFreqSamples;
    float *							highFreqSamples;
    PreviousValues					filterState;
    PreviousValues					filterState2;			// aml 2.14.02, added for 4th order filter
    PreviousValues					phaseCompState;			// aml 2.18.02, added for phase compensator
    float							srcPhase;				// aml 3.5.02
    float							srcState;				// aml 3.6.02
    Boolean							needToSync;
    Boolean							startiSub;
    Boolean							justResetClipPosition;

    Boolean							needToRestartDMA;
    Boolean							restartedDMA;
	Boolean							iSubOpen;

    IOFilterInterruptEventSource *	interruptEventSource;

    UInt32							numBlocks;
    UInt32							blockSize;
	
    bool							fNeedsPhaseInversion;
    bool							fNeedsRightChanDelay;   // [3134221] aml
	bool							fNeedsRightChanMixed;

	// aml 6.17.02
	DualMonoModeType				mInputDualMonoMode;

	// aml 5.10.0
    bool 							mUseSoftwareInputGain;
    float *							mInputGainLPtr;				
    float *							mInputGainRPtr;				

	bool							dmaRunState;			//	rbm 7.12.02 added for user client support
	IOAudioStreamFormat				dbdmaFormat;			//	rbm 7.15.02 added for user client support

    IOAudioStreamDirection			direction;

    virtual bool		filterInterrupt(int index);

    static bool 		interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static void 		interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	static IOReturn 	iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue);
    static bool			iSubEnginePublished (AppleDBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService);
	static IOReturn 	iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static IOReturn 	iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

public:
    IOMemoryDescriptor *		iSubBufferMemory;

    virtual bool init(OSDictionary 			*properties,
						IOService 			*theDeviceProvider,
						bool				hasInput,
						UInt32				numBlocks = DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS,
						UInt32				blockSize = DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE,
						UInt32				rate = DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE,
						UInt16				bitDepth = DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH,
						UInt16				numChannels = DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS);
    virtual void free();

    virtual bool 		initHardware(IOService *provider);
    virtual void 		stop(IOService *provider);
	virtual bool		willTerminate (IOService * provider, IOOptionBits options);

    virtual IOReturn	performAudioEngineStart();
    virtual IOReturn 	performAudioEngineStop();
    
    IOReturn     		restartDMA();

	virtual void 		setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency);

	// [3134221] aml
    inline void  		setRightChanDelay(bool inNeedsRightChanDelay ) { fNeedsRightChanDelay = inNeedsRightChanDelay; }; 
    inline bool  		getRightChanDelay() { return fNeedsRightChanDelay; };

    inline void  		setPhaseInversion(bool needsPhaseInversion ) { fNeedsPhaseInversion = needsPhaseInversion; }; 
    inline bool  		getPhaseInversion() { return fNeedsPhaseInversion; };

    inline void  		setRightChanMixed(bool needsRightChanMixed ) { fNeedsRightChanMixed = needsRightChanMixed; }; 
    inline bool  		getRightChanMixed() { return fNeedsRightChanMixed; };

	// aml 6.17.02
	void 				setDualMonoMode(const DualMonoModeType inDualMonoMode) { mInputDualMonoMode = inDualMonoMode; };

    // aml 5.10.02
    void		 		setUseSoftwareInputGain(bool inUseSoftwareInputGain);
    void	 			setInputGainL(UInt32 inGainL); 
    void		 		setInputGainR(UInt32 inGainR); 

    virtual UInt32 		getCurrentSampleFrame();
	virtual void 		resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);
    virtual IOReturn 	clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn 	convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

    virtual IOReturn	performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
	
	virtual	bool		getDmaState (void );
	virtual IOReturn	getAudioStreamFormat( IOAudioStreamFormat * streamFormatPtr );
    
    static const int kDBDMADeviceIndex;
    static const int kDBDMAOutputIndex;
    static const int kDBDMAInputIndex;
};

#endif /* _APPLEDBDMAAUDIODMAENGINE_H */

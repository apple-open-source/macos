#ifndef _APPLEDBDMAAUDIODMAENGINE_H
#define _APPLEDBDMAAUDIODMAENGINE_H

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/audio/IOAudioStream.h>

class IOInterruptEventSource;
class IOFilterInterruptEventSource;

//reducing the block size makes the problem happen less often
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS	512
#define DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE	128
#define DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE	44100
#define DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH	16
#define DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS	2
#define kMinimumLatency (16) // minimum safety offset

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
    IODBDMAChannelRegisters *		ioBaseDMAOutput;
    IODBDMAChannelRegisters *		ioBaseDMAInput;
    IODBDMADescriptor *			dmaCommandBufferOut;
    IODBDMADescriptor *			dmaCommandBufferIn;
    UInt32				commandBufferSize;
        //Next lines for iSub
    IOMemoryDescriptor *		iSubBufferMemory;
	UInt32						iSubLoopCount;
	SInt32						iSubBufferOffset;		// Where we are writing to in the iSub buffer
	UInt32						ourSampleFrameAtiSubLoop;
	UInt32						previousClippedToFrame;
    IOService *					ourProvider;
    IONotifier *				iSubEngineNotifier;
    AppleiSubEngine *			iSubEngine;
    float *						lowFreqSamples;
    float *						highFreqSamples;
    PreviousValues				filterState;
	Boolean						needToSync;
	Boolean						startiSub;

    IOFilterInterruptEventSource *	interruptEventSource;

    UInt32	numBlocks;
    UInt32	blockSize;
	
    UInt32	fBadCmd;
    UInt32	fBadResult;
    bool	fNeedsPhaseInversion;
	bool	fNeedsRightChanMixed;

    IOAudioStreamDirection	direction;

    virtual bool filterInterrupt(int index);

    static bool interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static void interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
    static bool	iSubEnginePublished (AppleDBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService);
	static IOReturn iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static IOReturn iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);

public:
    virtual bool init(OSDictionary 			*properties,
                      IOService 			*theDeviceProvider,
                      bool					hasInput,
                      UInt32				numBlocks = DBDMAAUDIODMAENGINE_DEFAULT_NUM_BLOCKS,
                      UInt32				blockSize = DBDMAAUDIODMAENGINE_DEFAULT_BLOCK_SIZE,
                      UInt32				rate = DBDMAAUDIODMAENGINE_DEFAULT_SAMPLE_RATE,
                      UInt16				bitDepth = DBDMAAUDIODMAENGINE_DEFAULT_BIT_DEPTH,
                      UInt16				numChannels = DBDMAAUDIODMAENGINE_DEFAULT_NUM_CHANNELS);
    virtual void free();

    virtual bool 	initHardware(IOService *provider);
    virtual void 	stop(IOService *provider);
    virtual IOReturn	message(UInt32 type, IOService * provider, void * arg);

    virtual IOReturn performAudioEngineStart();
    virtual IOReturn performAudioEngineStop();
    
    IOReturn     restartOutputIfFailure();

    inline void  setPhaseInversion(bool needsPhaseInversion ) { fNeedsPhaseInversion = needsPhaseInversion; }; 
    inline bool  getPhaseInversion() { return fNeedsPhaseInversion; };

    inline void  setRightChanMixed(bool needsRightChanMixed ) { fNeedsRightChanMixed = needsRightChanMixed; }; 
    inline bool  getRightChanMixed() { return fNeedsRightChanMixed; };

    virtual UInt32 getCurrentSampleFrame();
	virtual void resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame);
    virtual IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

    virtual IOReturn performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
    
    static const int kDBDMADeviceIndex;
    static const int kDBDMAOutputIndex;
    static const int kDBDMAInputIndex;
};

#endif /* _APPLEDBDMAAUDIODMAENGINE_H */

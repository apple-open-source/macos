#define DEBUGTIMESTAMPS		FALSE

#include "AppleDBDMAAudioDMAEngine.h"
#include "AppleOnboardAudio.h"

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioDebug.h>
//#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include "AudioHardwareUtilities.h"
#include "AppleiSubEngine.h"

extern "C" {
extern vm_offset_t phystokv(vm_offset_t pa);
};

#define super IOAudioEngine

OSDefineMetaClassAndStructors(AppleDBDMAAudioDMAEngine, super)

const int AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex	= 0;
const int AppleDBDMAAudioDMAEngine::kDBDMAOutputIndex	= 1;
const int AppleDBDMAAudioDMAEngine::kDBDMAInputIndex	= 2;

bool AppleDBDMAAudioDMAEngine::init(OSDictionary	*properties,
                                 IOService 			*theDeviceProvider,
                                 bool				hasInput,
                                 UInt32				nBlocks,
                                 UInt32				bSize,
                                 UInt32				rate,
                                 UInt16				bitDepth,
                                 UInt16				numChannels)
{
	IOAudioSampleRate initialSampleRate;
	IOMemoryMap *map;
	Boolean					result;

	CLOG("+ AppleDBDMAAudioDMAEngine::init\n");
	result = FALSE;

	// Ususal check
	FailIf (FALSE == super::init (properties), Exit);
	FailIf (NULL == theDeviceProvider, Exit);

	// create the memory places for DMA
	map = theDeviceProvider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMAOutputIndex);
	FailIf (NULL == map, Exit);
	ioBaseDMAOutput = (IODBDMAChannelRegisters *) map->getVirtualAddress();

	if(hasInput) {
		map = theDeviceProvider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMAInputIndex);
		FailIf (NULL == map, Exit);
		ioBaseDMAInput = (IODBDMAChannelRegisters *) map->getVirtualAddress();
	} else {
		ioBaseDMAInput = 0;
	}

	dmaCommandBufferIn = 0;
	dmaCommandBufferOut = 0;
	commandBufferSize = 0;
	interruptEventSource = 0;

	numBlocks = nBlocks;
	blockSize = bSize;
	setSampleOffset(kMinimumLatency);
	setNumSampleFramesPerBuffer(numBlocks * blockSize / 4);

	initialSampleRate.whole = rate;
	initialSampleRate.fraction = 0;

	setSampleRate(&initialSampleRate);

	result = TRUE;

Exit:
	CLOG("- AppleDBDMAAudioDMAEngine::init\n");    
	return result;
}

void AppleDBDMAAudioDMAEngine::free()
{
    if (interruptEventSource) {
        interruptEventSource->release();
        interruptEventSource = 0;
    }
    
    if (dmaCommandBufferOut && (commandBufferSize > 0)) 
    {
        IOFreeAligned(dmaCommandBufferOut, commandBufferSize);
        dmaCommandBufferOut = 0;
    }
    
    if (dmaCommandBufferIn && (commandBufferSize > 0)) 
    {
        IOFreeAligned(dmaCommandBufferIn, commandBufferSize);
        dmaCommandBufferOut = 0;
    }

    if (NULL != iSubEngineNotifier) {
        iSubEngineNotifier->remove ();
    }

    if (NULL != lowFreqSamples) {
        IOFree (lowFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

    if (NULL != highFreqSamples) {
        IOFree (highFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

    super::free();
}

bool AppleDBDMAAudioDMAEngine::initHardware(IOService *provider)
{
    vm_offset_t commandBufferPhys, sampleBufferPhys, stopCommandPhys, offset, sampleBufOut, sampleBufIn;
    UInt32 blockNum, dmaCommand = 0;
    bool doInterrupt = false;
    int interruptIndex;
    IOWorkLoop *workLoop;
    IOAudioStream *stream;
    
	sampleBufIn = NULL;
    IOAudioStreamFormat format = {
            2,
            kIOAudioStreamSampleFormatLinearPCM,
            kIOAudioStreamNumericRepresentationSignedInt,
            16,
            16,
            kIOAudioStreamAlignmentHighByte,
            kIOAudioStreamByteOrderBigEndian,
            true
    };
        
    DEBUG_IOLOG("+ AppleDBDMAAudioDMAEngine::initHardware()\n");
    
    ourProvider = provider;
    fBadCmd = 0;
    fBadResult = 0;

    if (!super::initHardware(provider)) {return false;}
        
        //allocate the memory for the buffer
    sampleBufOut = (vm_offset_t)IOMallocAligned(round_page(numBlocks * blockSize), PAGE_SIZE);
    if(ioBaseDMAInput)
        sampleBufIn = (vm_offset_t)IOMallocAligned(round_page(numBlocks * blockSize), PAGE_SIZE);
    
        //create the streams
    stream = new IOAudioStream;
    if (stream) {
        const IOAudioSampleRate *rate;        
        rate = getSampleRate();
        
        stream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput, 1, 0, 0);
        stream->setSampleBuffer((void *)sampleBufOut, numBlocks * blockSize);
        stream->addAvailableFormat(&format, rate, rate);
        stream->setFormat(&format);

        addAudioStream(stream);
        stream->release();
    }
    
    if(ioBaseDMAInput) {
        stream = new IOAudioStream;
        if (stream) {
            const IOAudioSampleRate *rate;        
            rate = getSampleRate();
        
            stream->initWithAudioEngine(this, kIOAudioStreamDirectionInput, 1, 0, 0);
            stream->setSampleBuffer((void *)sampleBufIn, numBlocks * blockSize);
            stream->addAvailableFormat(&format, rate, rate);
            stream->setFormat(&format);

            addAudioStream(stream);
            stream->release();
        }
    }

    if (!status || !sampleBufOut) {return false;}
    if(ioBaseDMAInput) 
        if(!sampleBufIn) {return false;}

        //create the DMA output part
    commandBufferSize = (numBlocks + 1) * sizeof(IODBDMADescriptor);
    dmaCommandBufferOut = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32); 
                                                            //needs to be more than 4 byte aligned
    if (!dmaCommandBufferOut) {return false;}

    commandBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)dmaCommandBufferOut);
    sampleBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)sampleBufOut);
    stopCommandPhys = pmap_extract(kernel_pmap, (vm_address_t) (&dmaCommandBufferOut[numBlocks]));

    offset = 0;
    dmaCommand = kdbdmaOutputMore;
    interruptIndex = kDBDMAOutputIndex;

        //install an interrupt handler only on the Ouput size of it
    workLoop = getWorkLoop();
    if (!workLoop) {return false;}
    
    interruptEventSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                               AppleDBDMAAudioDMAEngine::interruptHandler,
                                                                               AppleDBDMAAudioDMAEngine::interruptFilter,
                                                                               audioDevice->getProvider(),
                                                                               interruptIndex);
    if (!interruptEventSource) {return false;}
    workLoop->addEventSource(interruptEventSource);

        //create the DMA program
    for (blockNum = 0; blockNum < numBlocks; blockNum++) {
        vm_offset_t cmdDest;

        if (offset >= PAGE_SIZE) {
            sampleBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)(sampleBufOut + (blockNum * blockSize)));
            offset = 0;
        }

        // This code assumes that the size of the IODBDMADescriptor divides evenly into the page size
        // If this is the last block, branch to the first block
        if (blockNum == (numBlocks - 1)) {
            cmdDest = commandBufferPhys;
            doInterrupt = true;
        // Else if the next block starts on a page boundry, branch to it
        } else if ((((blockNum + 1) * sizeof(IODBDMADescriptor)) % PAGE_SIZE) == 0) {
            cmdDest = pmap_extract(kernel_pmap, (vm_address_t) (dmaCommandBufferOut + (blockNum + 1)));
	// No branch in the common case
        } else {
            cmdDest = 0;
        }

        if (cmdDest) {
            IOMakeDBDMADescriptorDep(&dmaCommandBufferOut[blockNum],
                                     dmaCommand,
                                     kdbdmaKeyStream0,
                                     doInterrupt ? kdbdmaIntAlways : kdbdmaIntNever,
                                     kdbdmaBranchAlways,
                                     kdbdmaWaitNever,
                                     blockSize,
                                     sampleBufferPhys + offset,
                                     cmdDest);
        } else {
            IOMakeDBDMADescriptorDep(&dmaCommandBufferOut[blockNum],
                                     dmaCommand,
                                     kdbdmaKeyStream0,
                                     kdbdmaIntNever,
                                     kdbdmaBranchIfTrue,
                                     kdbdmaWaitNever,
                                     blockSize,
                                     sampleBufferPhys + offset,
                                     stopCommandPhys);
        }
        offset += blockSize;
    }

    IOMakeDBDMADescriptor(&dmaCommandBufferOut[blockNum],
                          kdbdmaStop,
                          kdbdmaKeyStream0,
                          kdbdmaIntNever,
                          kdbdmaBranchNever,
                          kdbdmaWaitNever,
                          0,
                          0);
    
        //create the DMA input code
    if(ioBaseDMAInput) {
        dmaCommandBufferIn = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32); 
                                                            //needs to be more than 4 byte aligned
        if (!dmaCommandBufferIn) {return false;}

        commandBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)dmaCommandBufferIn);
        sampleBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)sampleBufIn);
        stopCommandPhys = pmap_extract(kernel_pmap, (vm_address_t) (&dmaCommandBufferIn[numBlocks]));

        doInterrupt = false;
        offset = 0;
        dmaCommand = kdbdmaInputMore;    
        
        for (blockNum = 0; blockNum < numBlocks; blockNum++) {
            vm_offset_t cmdDest;

            if (offset >= PAGE_SIZE) {
                sampleBufferPhys = pmap_extract(kernel_pmap, (vm_address_t)(sampleBufIn + (blockNum * blockSize)));
                offset = 0;
            }

                // This code assumes that the size of the IODBDMADescriptor 
                // divides evenly into the page size
                // If this is the last block, branch to the first block
            if (blockNum == (numBlocks - 1)) {
                cmdDest = commandBufferPhys;
                //doInterrupt = true;
                // Else if the next block starts on a page boundry, branch to it
            } else if ((((blockNum + 1) * sizeof(IODBDMADescriptor)) % PAGE_SIZE) == 0) {
                cmdDest = pmap_extract(kernel_pmap, (vm_address_t) (dmaCommandBufferIn + (blockNum + 1)));
                // No branch in the common case
            } else {
                cmdDest = 0;
            }

            if (cmdDest) {
                IOMakeDBDMADescriptorDep(&dmaCommandBufferIn[blockNum],
                                     dmaCommand,
                                     kdbdmaKeyStream0,
                                     doInterrupt ? kdbdmaIntAlways : kdbdmaIntNever,
                                     kdbdmaBranchAlways,
                                     kdbdmaWaitNever,
                                     blockSize,
                                     sampleBufferPhys + offset,
                                     cmdDest);
            } else {
                IOMakeDBDMADescriptorDep(&dmaCommandBufferIn[blockNum],
                                     dmaCommand,
                                     kdbdmaKeyStream0,
                                     kdbdmaIntNever,
                                     kdbdmaBranchIfTrue,
                                     kdbdmaWaitNever,
                                     blockSize,
                                     sampleBufferPhys + offset,
                                     stopCommandPhys);
            }
            offset += blockSize;
        }

        IOMakeDBDMADescriptor(&dmaCommandBufferIn[blockNum],
                          kdbdmaStop,
                          kdbdmaKeyStream0,
                          kdbdmaIntNever,
                          kdbdmaBranchNever,
                          kdbdmaWaitNever,
                          0,
                          0);

    }

	// Set up notifier to run when iSub shows up
    iSubBufferMemory = NULL;
    iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), 
                        (IOServiceNotificationHandler)&iSubEnginePublished, this);
    if (NULL != iSubBufferMemory) {
		// it looks like the notifier could be called before iSubEngineNotifier is set, 
                //so if it was called, then iSubBufferMemory would no longer be NULL and we can remove the notifier
		iSubEngineNotifier->remove ();
    }

    DEBUG_IOLOG("- AppleDBDMAAudioDMAEngine::initHardware()\n");
    return true;
}


void AppleDBDMAAudioDMAEngine::stop(IOService *provider)
{
    IOWorkLoop *workLoop;
    
    DEBUG3_IOLOG(" + AppleDBDMAAudioDMAEngine[%p]::stop(%p)\n", this, provider);
    
    if (interruptEventSource) {
        workLoop = getWorkLoop();
        if (workLoop) {
            workLoop->removeEventSource(interruptEventSource);
        }
    }
    
    super::stop(provider);
    stopAudioEngine();
    DEBUG3_IOLOG(" - AppleDBDMAAudioDMAEngine[%p]::stop(%p)\n", this, provider);
}


IOReturn AppleDBDMAAudioDMAEngine::message (UInt32 type, IOService * provider, void * arg) {
	bool							resultCode;
    IOCommandGate *					cg;

	debug4IOLog ("+AppleDBDMAAudioDMAEngine[%p]::message (0x%lx, %p)\n", this, type, provider);

	resultCode = kIOReturnSuccess;

	switch (type) {
		case kIOMessageServiceIsTerminated:
			if (iSubEngine == (AppleiSubEngine *)provider) {
				debugIOLog ("iSub requesting termination\n");

//				iSubEngine->close (this);
				cg = getCommandGate ();
				if (NULL != cg) {
					cg->runAction (iSubCloseAction, this);
				}

				detach (iSubEngine);

				// To avoid a possible race with the clip routine being called at the same time we are disposing of the
				// buffers it is using, we will pause the engine and then restart it around disposing of these buffers.
				pauseAudioEngine ();
				iSubEngine = NULL;
				iSubBufferMemory = NULL;

				if (NULL != lowFreqSamples) {
					IOFree (lowFreqSamples, (numBlocks * blockSize) * sizeof (float));
				}

				if (NULL != highFreqSamples) {
					IOFree (highFreqSamples, (numBlocks * blockSize) * sizeof (float));
				}
				resumeAudioEngine ();

				// Set up notifier to run when iSub shows up
				iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&iSubEnginePublished, this);
			}
			break;
		default:
			;
	}

	debug4IOLog ("-AppleDBDMAAudioDMAEngine[%p]::message (0x%lx, %p)\n", this, type, provider);
	return resultCode;
}


IOReturn AppleDBDMAAudioDMAEngine::performAudioEngineStart()
{
    debugIOLog(" + AppleDBDMAAudioDMAEngine::performAudioEngineStart()\n");

    if (!ioBaseDMAOutput || !dmaCommandBufferOut || !status || !interruptEventSource) {
        return kIOReturnError;
    }

    flush_dcache((vm_offset_t)dmaCommandBufferOut, commandBufferSize, false);
    if(ioBaseDMAInput)
        flush_dcache((vm_offset_t)dmaCommandBufferIn, commandBufferSize, false);

    filterState.xl_1 = 0.0;
    filterState.xr_1 = 0.0;
    filterState.xl_2 = 0.0;
    filterState.xr_2 = 0.0;
    filterState.yl_1 = 0.0;
    filterState.yr_1 = 0.0;
    filterState.yl_2 = 0.0;
    filterState.yr_2 = 0.0;

    if (NULL != iSubEngine) {
		startiSub = TRUE;
		needToSync = TRUE;
    }

    interruptEventSource->enable();

        //add the time stamp take to test
    takeTimeStamp(false);

        //start the input DMA first
    if(ioBaseDMAInput) {
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOClearDBDMAChannelControlBits(kdbdmaS0));
        IOSetDBDMABranchSelect(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
        IODBDMAStart(ioBaseDMAInput, (IODBDMADescriptor *)pmap_extract(kernel_pmap, (vm_address_t)(dmaCommandBufferIn)));
    }
    
    IOSetDBDMAChannelControl(ioBaseDMAOutput, IOClearDBDMAChannelControlBits(kdbdmaS0));
    IOSetDBDMABranchSelect(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
    IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)pmap_extract(kernel_pmap, (vm_address_t)(dmaCommandBufferOut)));

    debugIOLog(" - AppleDBDMAAudioDMAEngine::performAudioEngineStart()\n");
    return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudioDMAEngine::restartOutputIfFailure(){
    if (!ioBaseDMAOutput || !dmaCommandBufferOut || !status || !interruptEventSource) {
        return kIOReturnError;
    }

    flush_dcache((vm_offset_t)dmaCommandBufferOut, commandBufferSize, false);

    if (NULL != iSubEngine) {
		startiSub = TRUE;
		needToSync = TRUE;
    }

    interruptEventSource->enable();

	// add the time stamp take to test
    takeTimeStamp(false);
    
    IOSetDBDMAChannelControl(ioBaseDMAOutput, IOClearDBDMAChannelControlBits(kdbdmaS0));
    IOSetDBDMABranchSelect(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
    IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)pmap_extract(kernel_pmap,
                                (vm_address_t)(dmaCommandBufferOut)));

    return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudioDMAEngine::performAudioEngineStop()
{
    UInt16 attemptsToStop = 1000;

    debugIOLog("+ AppleDBDMAAudioDMAEngine::performAudioEngineStop()\n");

    if (NULL != iSubEngine) {
        iSubEngine->StopiSub ();
        needToSync = TRUE;
    }

    if (!interruptEventSource) {
        return kIOReturnError;
    }

    interruptEventSource->disable();
        
	// stop the output
    IOSetDBDMAChannelControl(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
    while ((IOGetDBDMAChannelStatus(ioBaseDMAOutput) & kdbdmaActive) && (attemptsToStop--)) {
        eieio();
        IOSleep(1);
    }

    IODBDMAStop(ioBaseDMAOutput);
    IODBDMAReset(ioBaseDMAOutput);

	// stop the input
    if(ioBaseDMAInput){
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
        while ((IOGetDBDMAChannelStatus(ioBaseDMAInput) & kdbdmaActive) && (attemptsToStop--)) {
            eieio();
            IOSleep(1);
        }

        IODBDMAStop(ioBaseDMAInput);
        IODBDMAReset(ioBaseDMAInput);
    }
    
    interruptEventSource->enable();

    DEBUG_IOLOG("- AppleDBDMAAudioDMAEngine::performAudioEngineStop()\n");
    return kIOReturnSuccess;
}

bool AppleDBDMAAudioDMAEngine::filterInterrupt(int index)
{
	// check to see if this interupt is because the DMA went bad
    UInt32 result = IOGetDBDMAChannelStatus(ioBaseDMAOutput);
    UInt32 cmd = IOGetDBDMACommandPtr(ioBaseDMAOutput);

    if (!(result & kdbdmaActive)) {
        fBadResult = result;
        fBadCmd = cmd;
    }		

    if (status) 
    {
        //test the takeTimeStamp :it will increment the fCurrentLoopCount and time stamp it with the time now
        takeTimeStamp();
    }

    return false;
}

bool AppleDBDMAAudioDMAEngine::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
    register AppleDBDMAAudioDMAEngine *dmaEngine = (AppleDBDMAAudioDMAEngine *)owner;
    bool result = true;

    if (dmaEngine) {
        result = dmaEngine->filterInterrupt(source->getIntIndex());
    }

    return result;
}

void AppleDBDMAAudioDMAEngine::interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
    return;
}

UInt32 AppleDBDMAAudioDMAEngine::getCurrentSampleFrame()
{
  
    UInt32 currentBlock = 0;

    if (ioBaseDMAOutput) {
        vm_offset_t currentDMACommandPhys, currentDMACommand;

        currentDMACommandPhys = (vm_offset_t)IOGetDBDMAChannelRegister(ioBaseDMAOutput, commandPtrLo);
        currentDMACommand = phystokv(currentDMACommandPhys);

        if ((UInt32)currentDMACommand > (UInt32)dmaCommandBufferOut) {
            currentBlock = ((UInt32)currentDMACommand - (UInt32)dmaCommandBufferOut) / sizeof(IODBDMADescriptor);
        }
    }

    return currentBlock * blockSize / 4;	// 4 bytes per frame - 2 per sample * 2 channels - BIG HACK
}

// This gets called when a new audio stream needs to be mixed into an already playing audio stream
void AppleDBDMAAudioDMAEngine::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
    if (NULL != iSubBufferMemory) {
		// start the filter over again since old filter state is invalid
		filterState.xl_1 = 0.0;
		filterState.xr_1 = 0.0;
		filterState.xl_2 = 0.0;
		filterState.xr_2 = 0.0;
		filterState.yl_1 = 0.0;
		filterState.yr_1 = 0.0;
		filterState.yl_2 = 0.0;
		filterState.yr_2 = 0.0;

#if DEBUGLOG
		IOLog ("resetClipPosition, iSubBufferOffset=%ld, previousClippedToFrame=%ld, clipSampleFrame=%ld\n", iSubBufferOffset, previousClippedToFrame, clipSampleFrame);
#endif
		if (previousClippedToFrame < clipSampleFrame) {
			// ((numBlocks * blockSize) / 4) is the number of samples in the buffer
			iSubBufferOffset -= (previousClippedToFrame + (((numBlocks * blockSize) / 4) - clipSampleFrame)) * 2;
		} else {
			iSubBufferOffset -= (previousClippedToFrame - clipSampleFrame) * 2;		// should be * streamFormat->fNumChannels, but that's not readily available
		}

		if (iSubBufferOffset < 0) {
			iSubBufferOffset += (iSubBufferMemory->getLength () / 2);
		}
		previousClippedToFrame = clipSampleFrame;
#if DEBUGLOG
		IOLog ("now: iSubBufferOffset=%ld, previousClippedToFrame=%ld\n", iSubBufferOffset, previousClippedToFrame);
#endif
    }
}

extern "C" {
UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate);
IOReturn clipAppleDBDMAToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
IOReturn clipAppleDBDMAToOutputStreamInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
IOReturn clipAppleDBDMAToOutputStreamMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
IOReturn clipAppleDBDMAToOutputStreamiSub(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen);
IOReturn clipAppleDBDMAToOutputStreamiSubInvertRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen);
IOReturn clipAppleDBDMAToOutputStreamiSubMixRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen);
IOReturn convertAppleDBDMAFromInputStream(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
};

IOReturn AppleDBDMAAudioDMAEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn					result;
	void *						iSubBuffer = NULL;
	SInt32						offsetDelta;
	UInt32						iSubBufferLen = 0;
	UInt32						sampleRate;

#if DEBUGTIMESTAMPS
	AbsoluteTime					currentTime;
	static AbsoluteTime				lastLoopTime = {0, 0};
	AbsoluteTime					diff;
	UInt64						nanos;

	clock_get_uptime (&currentTime);
	diff.hi = currentTime.hi;
	diff.lo = currentTime.lo;
	SUB_ABSOLUTETIME (&diff, &lastLoopTime);
	lastLoopTime.hi = currentTime.hi;
	lastLoopTime.lo = currentTime.lo;
	absolutetime_to_nanoseconds (diff, &nanos);
	IOLog ("delta = %ld\n", (UInt32)(nanos / (1000 * 1000)));
#endif
	// if the DMA went bad restart it
	if (fBadCmd && fBadResult)
	{
		fBadCmd = 0;
		fBadResult = 0;
                restartOutputIfFailure();
	}
        
	if (iSubBufferMemory) {
		iSubBufferLen = iSubBufferMemory->getLength ();
		iSubBuffer = (void*)iSubBufferMemory->getVirtualSegment (0, &iSubBufferLen);
		// (iSubBufferLen / 2) is because iSubBufferOffset is in UInt16s so convert iSubBufferLen to UInt16 length
		iSubBufferLen = iSubBufferLen / 2;

		sampleRate = getSampleRate()->whole;

		if (FALSE == needToSync && previousClippedToFrame != firstSampleFrame && !(previousClippedToFrame == getNumSampleFramesPerBuffer () && firstSampleFrame == 0)) {
#if DEBUGLOG
			IOLog ("iSubBufferOffset was %ld\n", iSubBufferOffset);
#endif
			if (firstSampleFrame < previousClippedToFrame) {
				// We've wrapped around the buffer
				// don't multiply by bit width because iSubBufferOffset is a UInt16 buffer pointer, not a UInt8 buffer pointer
				offsetDelta = (getNumSampleFramesPerBuffer () - previousClippedToFrame + firstSampleFrame) * streamFormat->fNumChannels;
			} else {
				offsetDelta = (firstSampleFrame - previousClippedToFrame) * streamFormat->fNumChannels;
			}
			iSubBufferOffset += offsetDelta;
#if DEBUGLOG
			IOLog ("clip to point was %ld, now %ld (delta = %ld)\n", previousClippedToFrame, firstSampleFrame, offsetDelta);
			IOLog ("iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			if (iSubBufferOffset > (SInt32)iSubBufferLen) {
				// Our calculated spot has actually wrapped around the iSub's buffer.
				iSubLoopCount += iSubBufferOffset / iSubBufferLen;
				iSubBufferOffset = iSubBufferOffset % iSubBufferLen;
#if DEBUGLOG
				IOLog ("iSubBufferOffset > iSubBufferLen, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			} else if (iSubBufferOffset < 0) {
				iSubBufferOffset += iSubBufferLen;
#if DEBUGLOG
				IOLog ("iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			}
		}
		previousClippedToFrame = firstSampleFrame + numSampleFrames;

		// Detect being out of sync with the iSub
		if (needToSync == FALSE && 0xFFFFFFFF != iSubEngine->GetCurrentLoopCount ()) {
		    if (iSubLoopCount > (iSubEngine->GetCurrentLoopCount () + 1) && !(iSubBufferOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2))) {
#if DEBUGLOG
			IOLog ("****looped more than the iSub iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld\n", iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
#endif
			needToSync = TRUE;
			startiSub = TRUE;
		    } else if (iSubLoopCount < iSubEngine->GetCurrentLoopCount ()) {
#if DEBUGLOG
			IOLog ("****iSub is ahead of us iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld\n", iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
#endif
			needToSync = TRUE;
			startiSub = TRUE;
		    } else if (iSubLoopCount == iSubEngine->GetCurrentLoopCount () && iSubBufferOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
#if DEBUGLOG
			IOLog ("****iSub is in front of our write head iSubBufferOffset = %ld, (iSubEngine->GetCurrentByteCount () / 2) = %ld\n", iSubBufferOffset, iSubEngine->GetCurrentByteCount () / 2);
#endif
			needToSync = TRUE;
			startiSub = TRUE;
		    }
		}

		// sync up with iSub
		if (TRUE == needToSync) {
			needToSync = FALSE;
			// start the filter over again since old filter state is invalid
			filterState.xl_1 = 0.0;
			filterState.xr_1 = 0.0;
			filterState.xl_2 = 0.0;
			filterState.xr_2 = 0.0;
			filterState.yl_1 = 0.0;
			filterState.yr_1 = 0.0;
			filterState.yl_2 = 0.0;
			filterState.yr_2 = 0.0;
			iSubLoopCount = iSubEngine->GetCurrentLoopCount ();
			iSubBufferOffset = (firstSampleFrame - getCurrentSampleFrame ()) * streamFormat->fNumChannels;
#if DEBUGLOG
			IOLog ("firstSampleFrame = %ld, getCurrentSampleFrame () = %ld\n", firstSampleFrame, getCurrentSampleFrame ());
#endif
			iSubBufferOffset -= 88 * 5;
#if DEBUGLOG
			IOLog ("starting iSubBufferOffset = %ld, iSubLoopCount = %ld, numSampleFrames = %ld\n", iSubBufferOffset, iSubLoopCount, numSampleFrames);
#endif
		}

		if (iSubBufferOffset > (SInt32)iSubBufferLen) {
			// Our calculated spot has actually wrapped around the iSub's buffer.
			iSubLoopCount += iSubBufferOffset / iSubBufferLen;
			iSubBufferOffset = iSubBufferOffset % iSubBufferLen;
#if DEBUGLOG
			IOLog ("iSubBufferOffset > iSubBufferLen (%ld), iSubBufferOffset is now %ld\n", iSubBufferLen, iSubBufferOffset);
#endif
		} else if (iSubBufferOffset < 0) {
			iSubBufferOffset += iSubBufferLen;
#if DEBUGLOG
			IOLog ("iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
		}

         // this will be true on a slot load iMac that has the built in speakers enabled
		if (TRUE == fNeedsPhaseInversion) {
            result = clipAppleDBDMAToOutputStreamiSubInvertRightChannel(mixBuf, sampleBuf, &filterState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen);
		} else if (TRUE == fNeedsRightChanMixed) {
            result = clipAppleDBDMAToOutputStreamiSubMixRightChannel(mixBuf, sampleBuf, &filterState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen);
		} else {
            result = clipAppleDBDMAToOutputStreamiSub(mixBuf, sampleBuf, &filterState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen);
		}
            
		if (TRUE == startiSub) {
			iSubEngine->StartiSub ();
			startiSub = FALSE;
			iSubLoopCount = 0;
		}
	} else {
		// this will be true on a slot load iMac that has the built in speakers enabled
		if (TRUE == fNeedsPhaseInversion) {
			result = clipAppleDBDMAToOutputStreamInvertRightChannel(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		} else if (TRUE == fNeedsRightChanMixed) {
			result = clipAppleDBDMAToOutputStreamMixRightChannel(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		} else {
			result = clipAppleDBDMAToOutputStream(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		}
    }

	return result;
}

IOReturn AppleDBDMAAudioDMAEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    return convertAppleDBDMAFromInputStream(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat);
}

IOReturn AppleDBDMAAudioDMAEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    return kIOReturnSuccess;
}

bool AppleDBDMAAudioDMAEngine::iSubEnginePublished (AppleDBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService) {
	IOReturn						result;
	bool							resultCode;
	OSCollectionIterator *			collectionIterator;
	IOAudioToggleControl *			ourMuteControl;
	IOAudioLevelControl *			masterVolumeControl;
	IOAudioLevelControl *			volumeControl;
	IOAudioLevelControl *			leftVolumeControl;
	IOAudioLevelControl *			rightVolumeControl;
    IOCommandGate *					cg;
	UInt32							i;
	UInt32							numControls;

	debug4IOLog ("+AppleDBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p)\n", dbdmaEngineObject, (UInt32*)refCon, newService);

	resultCode = false;

	FailIf (NULL == dbdmaEngineObject, Exit);
	FailIf (NULL == newService, Exit);

	dbdmaEngineObject->iSubEngine = (AppleiSubEngine *)newService;
	FailIf (NULL == dbdmaEngineObject->iSubEngine, Exit);

	// Set the initial volume of the iSub
	debugIOLog ("Looking for our volume controls to set the initial iSub volume\n");
	collectionIterator = OSCollectionIterator::withCollection (dbdmaEngineObject->defaultAudioControls);
	FailIf (NULL == collectionIterator, Exit);

	i = 0;
	numControls = dbdmaEngineObject->defaultAudioControls->getCount ();
	volumeControl = NULL;
	masterVolumeControl = NULL;
	leftVolumeControl = NULL;
	rightVolumeControl = NULL;
	while (i <  numControls) {
		volumeControl = OSDynamicCast (IOAudioLevelControl, collectionIterator->getNextObject ());
		if (NULL != volumeControl && volumeControl->getUsage () == kIOAudioControlUsageOutput) {
			if (volumeControl->getChannelID () == 1) {
				leftVolumeControl = volumeControl;
				debugIOLog ("Got our left volume control\n");
			} else if (volumeControl->getChannelID () == 2) {
				rightVolumeControl = volumeControl;
				debugIOLog ("Got our right volume control\n");
			} else if (volumeControl->getChannelID () == 0) {
				masterVolumeControl = volumeControl;
				debugIOLog ("Got our master volume control\n");
			}
		}
		i++;
	}
	collectionIterator->release ();

	// Get the initial mute state of our control so that we can set the iSub's mute state
	debugIOLog ("Looking for our mute control to set the initial iSub mute\n");
	collectionIterator = OSCollectionIterator::withCollection (dbdmaEngineObject->defaultAudioControls);
	i = 0;
	ourMuteControl = NULL;
	while (i <  numControls && NULL == ourMuteControl) {
		ourMuteControl = OSDynamicCast (IOAudioToggleControl, collectionIterator->getNextObject ());
		i++;
	}
	collectionIterator->release ();

	// Create the memory for the high/low samples to go into
    dbdmaEngineObject->lowFreqSamples = (float *)IOMallocAligned (round_page((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float)), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->lowFreqSamples, Exit);
    dbdmaEngineObject->highFreqSamples = (float *)IOMallocAligned (round_page((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float)), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->highFreqSamples, Exit);

	// Open the iSub which will cause it to create mute and volume controls
	dbdmaEngineObject->attach (dbdmaEngineObject->iSubEngine);
	cg = dbdmaEngineObject->getCommandGate ();
	FailIf (NULL == cg, Exit);
	result = cg->runAction (iSubOpenAction, dbdmaEngineObject);
	FailIf (kIOReturnSuccess != result, Exit);
	dbdmaEngineObject->iSubBufferMemory = dbdmaEngineObject->iSubEngine->GetSampleBuffer ();
	debug2IOLog ("iSubBuffer length = %ld\n", dbdmaEngineObject->iSubBufferMemory->getLength ());

	// Set the volume and mute state of the iSub
	// Since the iSub takes its volume from our volume, just set our volume and the iSub will pick it up.
	if (NULL != leftVolumeControl && NULL != rightVolumeControl) {
		debug3IOLog ("setting initial iSub volumes to L:%ld R:%ld\n", leftVolumeControl->getIntValue (), rightVolumeControl->getIntValue ());
		resultCode = leftVolumeControl->flushValue ();
		resultCode = rightVolumeControl->flushValue ();
	} else if (NULL != masterVolumeControl) {
		debug2IOLog ("setting initial iSub volume using master control to %ld\n", masterVolumeControl->getIntValue ());
		resultCode = masterVolumeControl->flushValue ();
	}

	if (NULL != ourMuteControl) {
		debug2IOLog ("setting initial iSub mute state to %ld\n", ourMuteControl->getIntValue ());
		resultCode = ourMuteControl->flushValue ();
	}

	// remove our notifier because we only care about the first iSub
	if (NULL != dbdmaEngineObject->iSubEngineNotifier)
		dbdmaEngineObject->iSubEngineNotifier->remove ();

	resultCode = true;

Exit:
	debug4IOLog ("-AppleDBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p)\n", dbdmaEngineObject, (UInt32 *)refCon, newService);
	return resultCode;
}

IOReturn AppleDBDMAAudioDMAEngine::iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
    if (owner && arg1) {
        AppleDBDMAAudioDMAEngine *audioEngine = OSDynamicCast (AppleDBDMAAudioDMAEngine, owner);

        if (audioEngine) {
            audioEngine->iSubEngine->close ((IOService *)arg1);
        }
    }

	return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudioDMAEngine::iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn					result;
	bool						resultBool;

	result = kIOReturnError;
	resultBool = FALSE;

    if (owner && arg1) {
        AppleDBDMAAudioDMAEngine *audioEngine = OSDynamicCast (AppleDBDMAAudioDMAEngine, owner);

        if (audioEngine) {
            resultBool = audioEngine->iSubEngine->open ((IOService *)arg1);
        }
    }

	if (TRUE == resultBool) {
		result = kIOReturnSuccess;
	}

	return result;
}

/*
#if 0
		if (iSubBufferOffset > (iSubEngine->GetCurrentByteCount () / 2)) {
			iSubLead = iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
		} else {
			iSubLead = iSubBufferLen - (iSubEngine->GetCurrentByteCount () / 2) + iSubBufferOffset;
		}
//		IOLog ("iSubLead=%ld\n", iSubLead);

		if (FALSE == needToSync && (iSubEngine->GetCurrentLoopCount () > iSubLoopCount || iSubLead < (initialiSubLead / 2))) {
#if DEBUGLOG
			IOLog ("iSubLoopCount=%ld, iSubCurrentLoopCount=%ld, iSubCurrentByteCount=%ld, iSubBufferOffset=%ld\n", iSubLoopCount, iSubEngine->GetCurrentLoopCount (), iSubEngine->GetCurrentByteCount (), iSubBufferOffset);
#endif
			needToSync = true;
		}
#endif

	Pulled out of clipOutputSamples
	AbsoluteTime				deltaTime;
	UInt64						nanoseconds;
	UInt32						ourBufferOffset;
	volatile const AbsoluteTime *	iSubTime;
	UInt32						ourSampleFrameAtiSubLoop;
	UInt32						currentSampleFrame;
	UInt32						frameDelta;	*/
/*
	Pulled out of clipOutputSamples
//		if (TRUE == needToSync || iSubLoopCount < iSubEngine->GetCurrentLoopCount ()) {
			ourSampleFrameAtiSubLoop = iSubEngine->GetEngineSampleFrameAtiSubLoop ();
			IOLog ("ourSampleFrameAtiSubLoop = %ld\n", ourSampleFrameAtiSubLoop);
			currentSampleFrame = getCurrentSampleFrame ();
			IOLog ("currentSampleFrame = %ld\n", currentSampleFrame);
			frameDelta = currentSampleFrame - ourSampleFrameAtiSubLoop;
			IOLog ("frameDelta = %ld\n", frameDelta);
			iSubBufferOffset = frameDelta * 4;		// convert frames into bytes for iSub's buffer location

//			IOLog ("iSubCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld\n", iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
			iSubLoopCount = iSubEngine->GetCurrentLoopCount ();

			// We have to figure out the sample that we are currently playing, so that we know the delta between that sample and the first frame we are mixing
			clock_get_uptime (&deltaTime);
			SUB_ABSOLUTETIME (&deltaTime, &status->fLastLoopTime);
			absolutetime_to_nanoseconds (deltaTime, &nanoseconds);
//			IOLog ("our nanoseconds = %ld\n", (UInt32)nanoseconds);
			ourBufferOffset = CalculateOffset (nanoseconds, sampleRate) * 4;
//			IOLog ("ourBufferOffset = %ld\n", ourBufferOffset);
			offsetDelta = (firstSampleFrame * 4) - ourBufferOffset;
//			IOLog ("offsetDelta = %ld\n", offsetDelta);

			// We have to calculate where the iSub is currently playing from so that we can insert our samples right in front of its play head at the correct offset.
			iSubTime = iSubEngine->GetLoopTime ();
			clock_get_uptime (&deltaTime);
			SUB_ABSOLUTETIME (&deltaTime, iSubTime);
			absolutetime_to_nanoseconds (deltaTime, &nanoseconds);
//			IOLog ("iSub nanoseconds = %ld\n", (UInt32)nanoseconds);
			iSubBufferOffset = CalculateOffset (nanoseconds, sampleRate) * 4;
//			IOLog ("calculated iSubBufferOffset = %ld\n", iSubBufferOffset);
//			IOLog ("iSub reported offset = %ld, frame list = %d\n", iSubEngine->GetCurrentByteCount (), iSubEngine->GetCurrentFrameList ());
			iSubBufferOffset += offsetDelta;
*/

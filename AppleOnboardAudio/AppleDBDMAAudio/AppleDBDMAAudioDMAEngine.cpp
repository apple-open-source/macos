#define DEBUGTIMESTAMPS		FALSE

#include "AppleDBDMAAudioDMAEngine.h"
#include "AppleOnboardAudio.h"

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioDebug.h>

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
	setNumSampleFramesPerBuffer(numBlocks * blockSize / sizeof (float));

	initialSampleRate.whole = rate;
	initialSampleRate.fraction = 0;

	setSampleRate(&initialSampleRate);
           
	mInputDualMonoMode = e_Mode_Disabled;	// aml 6.17.02	   
		   
	srcPhase = 1.0;				 		// aml 3.5.02
	srcState = 0.0;			 			// aml 3.6.02
	
	// aml 5.10.02
	mUseSoftwareInputGain = false;	
	mInputGainLPtr = NULL;	
	mInputGainRPtr = NULL;	

	result = TRUE;

Exit:
	CLOG("- AppleDBDMAAudioDMAEngine::init\n");    
	return result;
}

void AppleDBDMAAudioDMAEngine::setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency) {
	setOutputSampleLatency (outputLatency);
	setInputSampleLatency (inputLatency);
}

void AppleDBDMAAudioDMAEngine::free()
{
    if (interruptEventSource) {
        interruptEventSource->release();
        interruptEventSource = 0;
    }
    
	if (NULL != dmaCommandBufferInMemDescriptor) {
		dmaCommandBufferInMemDescriptor->release ();
		dmaCommandBufferInMemDescriptor = NULL;
	}
	if (NULL != dmaCommandBufferOutMemDescriptor) {
		dmaCommandBufferOutMemDescriptor->release ();
		dmaCommandBufferOutMemDescriptor = NULL;
	}
	if (NULL != sampleBufferInMemDescriptor) {
		sampleBufferInMemDescriptor->release ();
		sampleBufferInMemDescriptor = NULL;
	}
	if (NULL != sampleBufferOutMemDescriptor) {
		sampleBufferOutMemDescriptor->release ();
		sampleBufferOutMemDescriptor = NULL;
	}
	if (NULL != stopCommandMemDescriptor) {
		stopCommandMemDescriptor->release ();
		stopCommandMemDescriptor = NULL;
	}

    if (dmaCommandBufferOut && (commandBufferSize > 0)) {
        IOFreeAligned(dmaCommandBufferOut, commandBufferSize);
        dmaCommandBufferOut = 0;
    }
    
    if (dmaCommandBufferIn && (commandBufferSize > 0)) {
        IOFreeAligned(dmaCommandBufferIn, commandBufferSize);
        dmaCommandBufferOut = 0;
    }

    if (NULL != iSubEngineNotifier) {
        iSubEngineNotifier->remove ();
		iSubEngineNotifier = NULL;
    }

	if (NULL != iSubAttach) {
		iSubAttach->release ();
		iSubAttach = NULL;
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
	vm_offset_t					offset;
	vm_offset_t					sampleBufOut;
	vm_offset_t					sampleBufIn;
	IOPhysicalAddress			commandBufferPhys;
	IOPhysicalAddress			sampleBufferPhys;
	IOPhysicalAddress			stopCommandPhys;
    UInt32						blockNum;
	UInt32						dmaCommand = 0;
    Boolean						doInterrupt = false;
    UInt32						interruptIndex;
    IOWorkLoop *				workLoop;
    IOAudioStream *				stream;
	Boolean						result;

	result = FALSE;
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
	
	//	rbm 7.15.2002 keep a copy for user client
	dbdmaFormat.fNumChannels = format.fNumChannels;
	dbdmaFormat.fSampleFormat = format.fSampleFormat;
	dbdmaFormat.fNumericRepresentation = format.fNumericRepresentation;
	dbdmaFormat.fBitDepth = format.fBitDepth;
	dbdmaFormat.fBitWidth = format.fBitWidth;
	dbdmaFormat.fAlignment = format.fAlignment;
	dbdmaFormat.fIsMixable = format.fIsMixable;
	dbdmaFormat.fDriverTag = format.fDriverTag;
        
    DEBUG_IOLOG("+ AppleDBDMAAudioDMAEngine::initHardware()\n");
    
    ourProvider = provider;
	needToRestartDMA = FALSE;

    FailIf (!super::initHardware(provider), Exit);
        
	// allocate the memory for the buffer
    sampleBufOut = (vm_offset_t)IOMallocAligned(round_page(numBlocks * blockSize), PAGE_SIZE);
    if(ioBaseDMAInput)
        sampleBufIn = (vm_offset_t)IOMallocAligned(round_page(numBlocks * blockSize), PAGE_SIZE);
    
	// create the streams
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

    FailIf (!status || !sampleBufOut, Exit);
    if(ioBaseDMAInput) 
        FailIf (!sampleBufIn, Exit);

	// create the DMA output part
    commandBufferSize = (numBlocks + 1) * sizeof(IODBDMADescriptor);
    dmaCommandBufferOut = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32); 
                                                            // needs to be more than 4 byte aligned
    FailIf (!dmaCommandBufferOut, Exit);

	dmaCommandBufferOutMemDescriptor = IOMemoryDescriptor::withAddress (dmaCommandBufferOut, commandBufferSize, kIODirectionOut);
	FailIf (NULL == dmaCommandBufferOutMemDescriptor, Exit);
	sampleBufferOutMemDescriptor = IOMemoryDescriptor::withAddress ((void *)sampleBufOut, round_page (numBlocks * blockSize), kIODirectionOut);
	FailIf (NULL == sampleBufferOutMemDescriptor, Exit);
	stopCommandMemDescriptor = IOMemoryDescriptor::withAddress (&dmaCommandBufferOut[numBlocks], sizeof (IODBDMADescriptor *), kIODirectionOut);
	FailIf (NULL == stopCommandMemDescriptor, Exit);

	commandBufferPhys = dmaCommandBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == commandBufferPhys, Exit);
	sampleBufferPhys = sampleBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == sampleBufferPhys, Exit);
	stopCommandPhys = stopCommandMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == stopCommandPhys, Exit);

    offset = 0;
    dmaCommand = kdbdmaOutputMore;
    interruptIndex = kDBDMAOutputIndex;

	// install an interrupt handler only on the Ouput size of it
    workLoop = getWorkLoop();
    FailIf (!workLoop, Exit);
    
    interruptEventSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                               AppleDBDMAAudioDMAEngine::interruptHandler,
                                                                               AppleDBDMAAudioDMAEngine::interruptFilter,
                                                                               audioDevice->getProvider(),
                                                                               interruptIndex);
    FailIf (!interruptEventSource, Exit);
    workLoop->addEventSource(interruptEventSource);

	// create the DMA program
    for (blockNum = 0; blockNum < numBlocks; blockNum++) {
		IOPhysicalAddress	cmdDest;

        if (offset >= PAGE_SIZE) {
			sampleBufferPhys = sampleBufferOutMemDescriptor->getPhysicalSegment (blockNum * blockSize, 0);
			FailIf (NULL == sampleBufferPhys, Exit);
            offset = 0;
        }

        // This code assumes that the size of the IODBDMADescriptor divides evenly into the page size
        // If this is the last block, branch to the first block
        if (blockNum == (numBlocks - 1)) {
            cmdDest = commandBufferPhys;
            doInterrupt = true;
        // Else if the next block starts on a page boundry, branch to it
        } else if ((((blockNum + 1) * sizeof(IODBDMADescriptor)) % PAGE_SIZE) == 0) {
			cmdDest = dmaCommandBufferOutMemDescriptor->getPhysicalSegment ((blockNum + 1) * sizeof(IODBDMADescriptor), 0);
			FailIf (NULL == cmdDest, Exit);
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
    
	// create the DMA input code
    if(ioBaseDMAInput) {
        dmaCommandBufferIn = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32); 
                                                            // needs to be more than 4 byte aligned
        FailIf (!dmaCommandBufferIn, Exit);

		dmaCommandBufferInMemDescriptor = IOMemoryDescriptor::withAddress (dmaCommandBufferIn, commandBufferSize, kIODirectionOut);
		FailIf (NULL == dmaCommandBufferInMemDescriptor, Exit);
		sampleBufferInMemDescriptor = IOMemoryDescriptor::withAddress ((void *)sampleBufIn, round_page (numBlocks * blockSize), kIODirectionIn);
		FailIf (NULL == sampleBufferInMemDescriptor, Exit);
		stopCommandMemDescriptor = IOMemoryDescriptor::withAddress (&dmaCommandBufferIn[numBlocks], sizeof (IODBDMADescriptor *), kIODirectionOut);
		FailIf (NULL == stopCommandMemDescriptor, Exit);

		commandBufferPhys = dmaCommandBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		sampleBufferPhys = sampleBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == sampleBufferPhys, Exit);
		stopCommandPhys = stopCommandMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == stopCommandPhys, Exit);

        doInterrupt = false;
        offset = 0;
        dmaCommand = kdbdmaInputMore;    
        
        for (blockNum = 0; blockNum < numBlocks; blockNum++) {
			IOPhysicalAddress	cmdDest;

            if (offset >= PAGE_SIZE) {
				sampleBufferPhys = sampleBufferInMemDescriptor->getPhysicalSegment (blockNum * blockSize, 0);
				FailIf (NULL == sampleBufferPhys, Exit);
                offset = 0;
            }

                // This code assumes that the size of the IODBDMADescriptor 
                // divides evenly into the page size
                // If this is the last block, branch to the first block
            if (blockNum == (numBlocks - 1)) {
                cmdDest = commandBufferPhys;
                // doInterrupt = true;
                // Else if the next block starts on a page boundry, branch to it
            } else if ((((blockNum + 1) * sizeof(IODBDMADescriptor)) % PAGE_SIZE) == 0) {
				cmdDest = dmaCommandBufferInMemDescriptor->getPhysicalSegment ((blockNum + 1) * sizeof(IODBDMADescriptor), 0);
				FailIf (NULL == cmdDest, Exit);
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

    iSubBufferMemory = NULL;
	iSubEngine = NULL;

	// Set up a control that sound prefs can set to tell us if we should install our notifier or not
	iSubAttach = IOAudioToggleControl::create (FALSE,
										kIOAudioControlChannelIDAll,
										kIOAudioControlChannelNameAll,
										0,
										kIOAudioToggleControlSubTypeiSubAttach,
										kIOAudioControlUsageOutput);

	if (NULL != iSubAttach) {
		addDefaultAudioControl (iSubAttach);
		iSubAttach->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)iSubAttachChangeHandler, this);
		iSubAttach->release ();
	}

	result = TRUE;

Exit:
    DEBUG_IOLOG("- AppleDBDMAAudioDMAEngine::initHardware()\n");
    return result;
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

IOReturn AppleDBDMAAudioDMAEngine::performAudioEngineStart()
{
	IOPhysicalAddress			commandBufferPhys;
	IOReturn					result;

    debugIOLog(" + AppleDBDMAAudioDMAEngine::performAudioEngineStart()\n");

	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !status || !interruptEventSource, Exit);

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
    
    // aml 2.14.02 added for 4th order filter
    filterState2.xl_1 = 0.0;
    filterState2.xr_1 = 0.0;
    filterState2.xl_2 = 0.0;
    filterState2.xr_2 = 0.0;
    filterState2.yl_1 = 0.0;
    filterState2.yr_1 = 0.0;
    filterState2.yl_2 = 0.0;
    filterState2.yr_2 = 0.0;

    // aml 2.18.02 added for 4th order filter phase compensator
    phaseCompState.xl_1 = 0.0;
    phaseCompState.xr_1 = 0.0;
    phaseCompState.xl_2 = 0.0;
    phaseCompState.xr_2 = 0.0;
    phaseCompState.yl_1 = 0.0;
    phaseCompState.yr_1 = 0.0;
    phaseCompState.yl_2 = 0.0;
    phaseCompState.yr_2 = 0.0;

    if (NULL != iSubEngine) {
		startiSub = TRUE;
		needToSync = TRUE;
    }

    interruptEventSource->enable();

	// add the time stamp take to test
    takeTimeStamp(false);

	// start the input DMA first
    if(ioBaseDMAInput) {
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOClearDBDMAChannelControlBits(kdbdmaS0));
        IOSetDBDMABranchSelect(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
		commandBufferPhys = dmaCommandBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		IODBDMAStart(ioBaseDMAInput, (IODBDMADescriptor *)commandBufferPhys);
    }
    
    IOSetDBDMAChannelControl(ioBaseDMAOutput, IOClearDBDMAChannelControlBits(kdbdmaS0));
    IOSetDBDMABranchSelect(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
	commandBufferPhys = dmaCommandBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == commandBufferPhys, Exit);
	IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)commandBufferPhys);

	dmaRunState = TRUE;				//	rbm 7.12.02	added for user client support
	result = kIOReturnSuccess;

    debugIOLog(" - AppleDBDMAAudioDMAEngine::performAudioEngineStart()\n");

Exit:
    return result;
}

IOReturn AppleDBDMAAudioDMAEngine::restartDMA () {
	IOReturn					result;

	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !interruptEventSource, Exit);

#if DEBUGLOG
	IOLog ("Restarting DMA\n");
#endif
	performAudioEngineStop ();
	performAudioEngineStart ();
	result = kIOReturnSuccess;

Exit:
    return result;
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
    
	dmaRunState = FALSE;				//	rbm 7.12.02	added for user client support
    interruptEventSource->enable();

    DEBUG_IOLOG("- AppleDBDMAAudioDMAEngine::performAudioEngineStop()\n");
    return kIOReturnSuccess;
}

bool AppleDBDMAAudioDMAEngine::filterInterrupt (int index) {
	// check to see if this interupt is because the DMA went bad
    UInt32 resultOut = IOGetDBDMAChannelStatus (ioBaseDMAOutput);
    UInt32 resultIn = 1;

	if (ioBaseDMAInput) {
		resultIn = IOGetDBDMAChannelStatus (ioBaseDMAInput) & kdbdmaActive;
	}

    if (!(resultOut & kdbdmaActive) || !resultIn) {
		needToRestartDMA = TRUE;
	}

	// test the takeTimeStamp :it will increment the fCurrentLoopCount and time stamp it with the time now
	takeTimeStamp ();

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
    if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
				
        srcPhase = 1.0;			// aml 3.5.02
        srcState = 0.0;			// aml 3.6.02

        // start the filter over again since old filter state is invalid
        filterState.xl_1 = 0.0;
        filterState.xr_1 = 0.0;
        filterState.xl_2 = 0.0;
        filterState.xr_2 = 0.0;
        filterState.yl_1 = 0.0;
        filterState.yr_1 = 0.0;
        filterState.yl_2 = 0.0;
        filterState.yr_2 = 0.0;

        // aml 2.14.02 added for 4th order filter
        filterState2.xl_1 = 0.0;
        filterState2.xr_1 = 0.0;
        filterState2.xl_2 = 0.0;
        filterState2.xr_2 = 0.0;
        filterState2.yl_1 = 0.0;
        filterState2.yr_1 = 0.0;
        filterState2.yl_2 = 0.0;
        filterState2.yr_2 = 0.0;

        // aml 2.18.02 added for 4th order filter phase compensator
        phaseCompState.xl_1 = 0.0;
        phaseCompState.xr_1 = 0.0;
        phaseCompState.xl_2 = 0.0;
        phaseCompState.xr_2 = 0.0;
        phaseCompState.yl_1 = 0.0;
        phaseCompState.yr_1 = 0.0;
        phaseCompState.yl_2 = 0.0;
        phaseCompState.yr_2 = 0.0;

#if DEBUGLOG
        IOLog ("+resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld, clipSampleFrame=%ld\n", iSubBufferOffset, previousClippedToFrame, clipSampleFrame);
#endif
        if (previousClippedToFrame < clipSampleFrame) {
			// Resetting the clip point backwards around the end of the buffer
			// aml 3.12.02 changed to iSub num channels
			clipAdjustment = (getNumSampleFramesPerBuffer () - clipSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();
        } else {
			// aml 3.12.02 changed to iSub num channels
			clipAdjustment = (previousClippedToFrame - clipSampleFrame) * iSubEngine->GetNumChannels();
        }
#if DEBUGLOG
        if (clipAdjustment < kMinimumLatency) {
            IOLog ("resetClipPosition: 44.1 clipAdjustment < min, clipAdjustment=%ld\n", clipAdjustment); 
        }                
#endif
        // aml 3.21.02, adjust for new sample rate
        clipAdjustment = (clipAdjustment * 1000) / ((1000 * getSampleRate()->whole) / iSubEngine->GetSampleRate());  
        iSubBufferOffset -= clipAdjustment;

#if DEBUGLOG
        if (clipAdjustment > (iSubBufferMemory->getLength () / 2)) {
            IOLog ("resetClipPosition: clipAdjustment > iSub buffer size, clipAdjustment=%ld\n", clipAdjustment); 
        }                
#endif

        if (iSubBufferOffset < 0) {
			iSubBufferOffset += (iSubBufferMemory->getLength () / 2);	
			iSubLoopCount--;
        }
        previousClippedToFrame = clipSampleFrame;
        justResetClipPosition = TRUE;

#if DEBUGLOG
        IOLog ("-resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld\n", iSubBufferOffset, previousClippedToFrame);
#endif
    }
}

extern "C" {
UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate);
IOReturn clipAppleDBDMAToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
// [3134221] aml
IOReturn clipAppleDBDMAToOutputStreamDelayRight(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
IOReturn clipAppleDBDMAToOutputStreamInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);
IOReturn clipAppleDBDMAToOutputStreamMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat);

IOReturn clipAppleDBDMAToOutputStreamiSub(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, PreviousValues *filterState2, PreviousValues *phaseCompState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, iSubAudioFormatType* iSubFormat, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate);
// [3134221] aml
IOReturn clipAppleDBDMAToOutputStreamiSubDelayRight(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, PreviousValues *filterState2, PreviousValues *phaseCompState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, iSubAudioFormatType* iSubFormat, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate);
IOReturn clipAppleDBDMAToOutputStreamiSubInvertRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, PreviousValues *filterState2, PreviousValues *phaseCompState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, iSubAudioFormatType* iSubFormat, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate);
IOReturn clipAppleDBDMAToOutputStreamiSubMixRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues *filterState, PreviousValues *filterState2, PreviousValues *phaseCompState, float *low, float *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 * iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, iSubAudioFormatType* iSubFormat, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate);

// aml 6.17.02, added dual mono mode parameter
IOReturn convertAppleDBDMAFromInputStream(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, DualMonoModeType inDualMonoMode);

// aml 5.10.02, adding input clip routine with software gain control
// aml 6.17.02, added dual mono mode parameter
IOReturn convertAppleDBDMAFromInputStreamWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, float* inGainL, float* inGainR, DualMonoModeType inDualMonoMode);

// aml 5.10.02 added utility functions
void dBfixed2float(UInt32 indBfixed, float* ioGainPtr);
void inputGainConverter(UInt32 inGainIndex, float* ioGainPtr);
};

// aml 5.10.02
void AppleDBDMAAudioDMAEngine::setUseSoftwareInputGain(bool inUseSoftwareInputGain) { 
    mUseSoftwareInputGain = inUseSoftwareInputGain; 

#ifdef _AML_LOG_INPUT_GAIN // aml XXX testing
    if (mUseSoftwareInputGain)
        IOLog("AppleDBDMAAudioDMAEngine::setUseSoftwareInputGain, use SW input gain = TRUE.\n"); 
    else
        IOLog("AppleDBDMAAudioDMAEngine::setUseSoftwareInputGain, use SW input gain = FALSE.\n"); 
#endif                    
    
    return;   
}

// aml 5.10.02
void AppleDBDMAAudioDMAEngine::setInputGainL(UInt32 inGainL) { 

    if (mInputGainLPtr == NULL) {        
        mInputGainLPtr = (float *)IOMalloc(sizeof(float));
#ifdef _AML_LOG_INPUT_GAIN // aml XXX testing
        IOLog("AppleDBDMAAudioDMAEngine::setInputGainL - allocating mInputGainLPtr (0x%x).\n", mInputGainLPtr);   
#endif                    
    }
    inputGainConverter(inGainL, mInputGainLPtr);
	
    return;   
} 

// aml 5.10.02
void AppleDBDMAAudioDMAEngine::setInputGainR(UInt32 inGainR) { 

    if (mInputGainRPtr == NULL) {        
        mInputGainRPtr = (float *)IOMalloc(sizeof(float));
#ifdef _AML_LOG_INPUT_GAIN // aml XXX testing
        IOLog("AppleDBDMAAudioDMAEngine::setInputGainR - allocating mInputGainRPtr (0x%x).\n", mInputGainRPtr);   
#endif                    
    }
    inputGainConverter(inGainR, mInputGainRPtr);

    return;   
} 

IOReturn AppleDBDMAAudioDMAEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn					result;
	void *						iSubBuffer = NULL;
	SInt32						offsetDelta;
	SInt32						safetyOffset;
	UInt32						iSubBufferLen = 0;
	iSubAudioFormatType			iSubFormat;	// aml 3.1.02
	UInt32						distance;
	static UInt32				oldiSubBufferOffset;
 
	// if the DMA went bad restart it
	if (needToRestartDMA) {
		needToRestartDMA = FALSE;
		restartDMA ();
	}
        
	if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
		UInt32						adaptiveSampleRate;
		UInt32						sampleRate;

 		iSubBufferLen = iSubBufferMemory->getLength ();
		iSubBuffer = (void*)iSubBufferMemory->getVirtualSegment (0, &iSubBufferLen);
		// (iSubBufferLen / 2) is because iSubBufferOffset is in UInt16s so convert iSubBufferLen to UInt16 length
		iSubBufferLen = iSubBufferLen / 2;

		sampleRate = getSampleRate()->whole;
		adaptiveSampleRate = sampleRate;

		// aml 3.1.02
		iSubFormat.altInterface = iSubEngine->GetAltInterface();
		iSubFormat.numChannels = iSubEngine->GetNumChannels();
		iSubFormat.bytesPerSample = iSubEngine->GetBytesPerSample();
		iSubFormat.outputSampleRate = iSubEngine->GetSampleRate();

		if (needToSync == FALSE) {
			UInt32			wrote;
			wrote = iSubBufferOffset - oldiSubBufferOffset;
//			IOLog ("wrote %ld iSub samples\n", wrote);
			if (iSubLoopCount == iSubEngine->GetCurrentLoopCount () && iSubBufferOffset > (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
				distance = iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
			} else if (iSubLoopCount == (iSubEngine->GetCurrentLoopCount () + 1) && iSubBufferOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
				distance = iSubBufferLen - (iSubEngine->GetCurrentByteCount () / 2) + iSubBufferOffset;
			}
			if (distance < (initialiSubLead / 2)) {
				// Write more samples into the iSub's buffer
//				IOLog ("speed up! %ld, %ld, %ld\n", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
				adaptiveSampleRate = sampleRate - (sampleRate >> 4);
			} else if (distance > (initialiSubLead + (initialiSubLead / 2))) {
				// Write fewer samples into the iSub's buffer
//				IOLog ("slow down! %ld, %ld, %ld\n", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
				adaptiveSampleRate = sampleRate + (sampleRate >> 4);
			} else {
				// The sample rate is just right
//				IOLog ("just right %ld, %ld, %ld\n", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
				adaptiveSampleRate = sampleRate;
			}
		}

		// Detect being out of sync with the iSub
#if ABORT_PIPE_ON_START
		if (needToSync == FALSE && previousClippedToFrame == firstSampleFrame && 0x0 != iSubEngine->GetCurrentLoopCount ()) {
#else
		if (needToSync == FALSE && previousClippedToFrame == firstSampleFrame && 0xFFFFFFFF != iSubEngine->GetCurrentLoopCount ()) {
#endif
			// aml - make the reader/writer check more strict - this helps get rid of long term crunchy iSub audio
			// the reader is now not allowed within one frame (one millisecond of audio) of the writer
			safetyOffset = iSubBufferOffset - ((iSubFormat.outputSampleRate) / 1000);		// 6 samples at 6kHz
			if (safetyOffset < 0) {
				safetyOffset += iSubBufferLen;
			}
			if (iSubLoopCount == iSubEngine->GetCurrentLoopCount () && safetyOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
				#if DEBUGLOG
				distance = iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
				IOLog ("****iSub is in front of write head safetyOffset = %ld, iSubEngine->GetCurrentByteCount () / 2 = %ld\n", safetyOffset, iSubEngine->GetCurrentByteCount () / 2);
//				IOLog ("distance = %ld\n", distance);
				#endif
				needToSync = TRUE;
				startiSub = TRUE;
			} else if (iSubLoopCount > (iSubEngine->GetCurrentLoopCount () + 1)) {
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
		    } else if (iSubLoopCount == iSubEngine->GetCurrentLoopCount () && iSubBufferOffset > ((SInt32)( (iSubEngine->GetCurrentByteCount() + (((iSubFormat.outputSampleRate)/1000 * NUM_ISUB_FRAME_LISTS_TO_QUEUE * NUM_ISUB_FRAMES_PER_LIST) * iSubFormat.bytesPerSample * iSubFormat.numChannels) ) / 2))) {		// aml 3.27.02, this is the right number here (buffersize was 2x too large).  This number should come eventually from the iSub engine reporting it's maximum number of queued bytes.
                     
				#if DEBUGLOG
				IOLog ("****iSub is too far behind write head iSubBufferOffset = %ld, (iSubEngine->GetCurrentByteCount () / 2 + max queued data) = %ld\n", iSubBufferOffset, (iSubEngine->GetCurrentByteCount() / 2 + iSubBufferLen/2));					
				#endif
				needToSync = TRUE;
				startiSub = TRUE;
		    }
		}

		if (FALSE == needToSync && previousClippedToFrame != firstSampleFrame && !(previousClippedToFrame == getNumSampleFramesPerBuffer () && firstSampleFrame == 0)) {
#if DEBUGLOG
			IOLog ("clipOutput: no sync: iSubBufferOffset was %ld\n", iSubBufferOffset);
#endif
			if (firstSampleFrame < previousClippedToFrame) {
#if DEBUGLOG
				IOLog ("clipOutput: no sync: firstSampleFrame < previousClippedToFrame (delta = %ld)\n", previousClippedToFrame-firstSampleFrame);
#endif
				// We've wrapped around the buffer
				// aml 2.28.02 replaced input stream num channels with iSub output num channels
				offsetDelta = (getNumSampleFramesPerBuffer () - firstSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();	
			} else {
#if DEBUGLOG
				IOLog ("clipOutput: no sync: previousClippedToFrame < firstSampleFrame (delta = %ld)\n", firstSampleFrame - previousClippedToFrame);
#endif
				// aml 2.28.02 replaced input stream num channels with iSub output num channels
				offsetDelta = (firstSampleFrame - previousClippedToFrame) * iSubEngine->GetNumChannels();
			}
			// aml 3.21.02, adjust for new sample rate
			offsetDelta = (offsetDelta * 1000) / ((sampleRate * 1000) / iSubFormat.outputSampleRate);
			iSubBufferOffset += offsetDelta;
#if DEBUGLOG
			IOLog ("clipOutput: no sync: clip to point was %ld, now %ld (delta = %ld)\n", previousClippedToFrame, firstSampleFrame, offsetDelta);
			IOLog ("clipOutput: no sync: iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			if (iSubBufferOffset > (SInt32)iSubBufferLen) {
#if DEBUGLOG
				IOLog ("clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset = %ld\n", iSubBufferOffset);
#endif
				// Our calculated spot has actually wrapped around the iSub's buffer.
				iSubLoopCount += iSubBufferOffset / iSubBufferLen;
				iSubBufferOffset = iSubBufferOffset % iSubBufferLen;
#if DEBUGLOG
				IOLog ("clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			} else if (iSubBufferOffset < 0) {
				iSubBufferOffset += iSubBufferLen;
#if DEBUGLOG
				IOLog ("clipOutput: no sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
			}
		}

		if (TRUE == justResetClipPosition) {
			justResetClipPosition = FALSE;
			needToSync = FALSE;
			startiSub = FALSE;
		}

		// sync up with iSub only if everything is proceeding normally.
		// aml [3095619] - added check with iSubEngine for sync state.
		if ((TRUE == needToSync) || (iSubEngine->GetNeedToSync())) {
			UInt32				curSampleFrame;
			
			// aml [3095619] reset iSub sync state if we've handled that case.
			iSubEngine->SetNeedToSync(false);
			
			needToSync = FALSE;
                     
			srcPhase =  1.0;	// aml 3.5.02
			srcState =  0.0;	// aml 3.6.02
                        
			// start the filter over again since old filter state is invalid
			filterState.xl_1 = 0.0;
			filterState.xr_1 = 0.0;
			filterState.xl_2 = 0.0;
			filterState.xr_2 = 0.0;
			filterState.yl_1 = 0.0;
			filterState.yr_1 = 0.0;
			filterState.yl_2 = 0.0;
			filterState.yr_2 = 0.0;

			// aml 2.14.02 added for 4th order filter
			filterState2.xl_1 = 0.0;
			filterState2.xr_1 = 0.0;
			filterState2.xl_2 = 0.0;
			filterState2.xr_2 = 0.0;
			filterState2.yl_1 = 0.0;
			filterState2.yr_1 = 0.0;
			filterState2.yl_2 = 0.0;
			filterState2.yr_2 = 0.0;

			// aml 2.18.02 added for 4th order phase compensator
			phaseCompState.xl_1 = 0.0;
			phaseCompState.xr_1 = 0.0;
			phaseCompState.xl_2 = 0.0;
			phaseCompState.xr_2 = 0.0;
			phaseCompState.yl_1 = 0.0;
			phaseCompState.yr_1 = 0.0;
			phaseCompState.yl_2 = 0.0;
			phaseCompState.yr_2 = 0.0;

#if ABORT_PIPE_ON_START
			// aml 4.25.02 wipe out the iSub buffer, changed due to moving zeroing of iSub buffer in AUA write handler when aborting the pipe
			bzero(iSubBuffer, iSubBufferLen);
#endif
			curSampleFrame = getCurrentSampleFrame ();

			if (TRUE == restartedDMA) {
				iSubBufferOffset = initialiSubLead;
				restartedDMA = FALSE;
			} else {
				if (firstSampleFrame < curSampleFrame) {
					// aml 2.28.02 replaced input stream num channels with iSub output num channels
					// aml 3.21.02, moved to temp variable 
					offsetDelta = (getNumSampleFramesPerBuffer () - curSampleFrame + firstSampleFrame) * iSubEngine->GetNumChannels();
				} else {
					// aml 2.28.02 replaced input stream num channels with iSub output num channels
					// aml 3.21.02, moved to temp variable 
					offsetDelta = (firstSampleFrame - curSampleFrame) * iSubEngine->GetNumChannels();
				}
#if DEBUGLOG
				IOLog ("clipOutput: need to sync: 44.1kHz offsetDelta = %ld\n", offsetDelta);
				if (offsetDelta < kMinimumLatency) {
					IOLog ("clipOutput: no sync: 44.1 offsetDelta < min, offsetDelta=%ld\n", offsetDelta); 
				}                
#endif
				// aml 3.21.02, adjust for new sample rate
				offsetDelta = (offsetDelta * 1000) / ((sampleRate * 1000) / iSubFormat.outputSampleRate);
#if DEBUGLOG
				IOLog ("clipOutput: need to sync: iSubBufferOffset = %ld, offsetDelta = %ld\n", iSubBufferOffset, offsetDelta);
#endif

				// aml 4.24.02 this was supposed to set the offset, not add it!  Looks like a typo from case above.
				iSubBufferOffset = offsetDelta;
#if DEBUGLOG
				IOLog ("clipOutput: need to sync: offsetDelta = %ld\n", offsetDelta);
				IOLog ("clipOutput: need to sync: firstSampleFrame = %ld, curSampleFrame = %ld\n", firstSampleFrame, curSampleFrame);
				IOLog ("clipOutput: need to sync: starting iSubBufferOffset = %ld, numSampleFrames = %ld\n", iSubBufferOffset, numSampleFrames);
#endif
				if (iSubBufferOffset > (SInt32)iSubBufferLen) {
			
					needToSync = TRUE;	// aml 4.24.02, requests larger than our buffer size = bad!
#if DEBUGLOG
					IOLog ("clipOutput: need to sync: SubBufferOffset too big (%ld) RESYNC!\n", iSubBufferOffset);
#endif
					
					// Our calculated spot has actually wrapped around the iSub's buffer.
					iSubLoopCount += iSubBufferOffset / iSubBufferLen;
					iSubBufferOffset = iSubBufferOffset % iSubBufferLen;
#if DEBUGLOG
					IOLog ("clipOutput: need to sync: iSubBufferOffset > iSubBufferLen (%ld), iSubBufferOffset is now %ld\n", iSubBufferLen, iSubBufferOffset);
#endif
				} else if (iSubBufferOffset < 0) {
					iSubBufferOffset += iSubBufferLen;
#if DEBUGLOG
					IOLog ("clipOutput: need to sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", iSubBufferOffset);
#endif
				}
				initialiSubLead = iSubBufferOffset;
			}
		}
		// aml 2.14.02 added filterState2 for 4th order filter in all clip calls below
		// aml 2.18.02 added phaseCompState for 4th order filter phase compensator in all clip calls below
		// aml 3.1.02 added iSub format struct to calls below
		// aml 3.5.02 added src phase to calls below
		// aml 3.6.02 added src state to calls below
		// this will be true on a slot load iMac that has the built in speakers enabled
		oldiSubBufferOffset = iSubBufferOffset;
		if (TRUE == fNeedsPhaseInversion) {
            result = clipAppleDBDMAToOutputStreamiSubInvertRightChannel (mixBuf, sampleBuf, &filterState, &filterState2, &phaseCompState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen, &iSubFormat, &srcPhase, &srcState, adaptiveSampleRate);
		} else if (TRUE == fNeedsRightChanMixed) {
            result = clipAppleDBDMAToOutputStreamiSubMixRightChannel (mixBuf, sampleBuf, &filterState, &filterState2, &phaseCompState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen, &iSubFormat, &srcPhase, &srcState, adaptiveSampleRate);
		} else if (TRUE == fNeedsRightChanDelay) {	// [3134221] aml
            result = clipAppleDBDMAToOutputStreamiSubDelayRight (mixBuf, sampleBuf, &filterState, &filterState2, &phaseCompState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen, &iSubFormat, &srcPhase, &srcState, adaptiveSampleRate);
		} else {
            result = clipAppleDBDMAToOutputStreamiSub (mixBuf, sampleBuf, &filterState, &filterState2, &phaseCompState, lowFreqSamples, highFreqSamples, firstSampleFrame, numSampleFrames, sampleRate, streamFormat, (SInt16*)iSubBuffer, &iSubLoopCount, &iSubBufferOffset, iSubBufferLen, &iSubFormat, &srcPhase, &srcState, adaptiveSampleRate);
		}

		if (TRUE == startiSub) {
			iSubEngine->StartiSub ();
			startiSub = FALSE;
			iSubLoopCount = 0;
		}

		previousClippedToFrame = firstSampleFrame + numSampleFrames;
	} else {
		// this will be true on a slot load iMac that has the built in speakers enabled
		if (TRUE == fNeedsPhaseInversion) {
			result = clipAppleDBDMAToOutputStreamInvertRightChannel(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		} else if (TRUE == fNeedsRightChanMixed) {
			result = clipAppleDBDMAToOutputStreamMixRightChannel(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		} else if (TRUE == fNeedsRightChanDelay) { 	// [3134221] aml
			result = clipAppleDBDMAToOutputStreamDelayRight(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		} else {
			result = clipAppleDBDMAToOutputStream(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
		}
    }

	return result;
}

IOReturn AppleDBDMAAudioDMAEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	if (mUseSoftwareInputGain) {
		return convertAppleDBDMAFromInputStreamWithGain(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, mInputGainLPtr, mInputGainRPtr, mInputDualMonoMode);
	} else {
		return convertAppleDBDMAFromInputStream(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat, mInputDualMonoMode);
	}
}

IOReturn AppleDBDMAAudioDMAEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
	if ( NULL != newFormat ) {									//	rbm 7.15.2002 keep a copy for user client
		dbdmaFormat.fNumChannels = newFormat->fNumChannels;
		dbdmaFormat.fSampleFormat = newFormat->fSampleFormat;
		dbdmaFormat.fNumericRepresentation = newFormat->fNumericRepresentation;
		dbdmaFormat.fBitDepth = newFormat->fBitDepth;
		dbdmaFormat.fBitWidth = newFormat->fBitWidth;
		dbdmaFormat.fAlignment = newFormat->fAlignment;
		dbdmaFormat.fByteOrder = newFormat->fByteOrder;
		dbdmaFormat.fIsMixable = newFormat->fIsMixable;
		dbdmaFormat.fDriverTag = newFormat->fDriverTag;
	}
    return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudioDMAEngine::iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn						result;
    AppleDBDMAAudioDMAEngine *		audioDMAEngine;
    IOCommandGate *					cg;

	debug5IOLog ("+ AppleDBDMAAudioDMAEngine::iSubAttachChangeHandler (%p, %p, 0x%lx, 0x%lx)\n", target, attachControl, oldValue, newValue);

	result = kIOReturnSuccess;
	FailIf (oldValue == newValue, Exit);
    audioDMAEngine = OSDynamicCast (AppleDBDMAAudioDMAEngine, target);
	FailIf (NULL == audioDMAEngine, Exit);

	if (newValue) {
		debugIOLog ("try to connect to an iSub\n");
		// Set up notifier to run when iSub shows up
		audioDMAEngine->iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&iSubEnginePublished, audioDMAEngine);
		if (NULL != audioDMAEngine->iSubBufferMemory) {
			// it looks like the notifier could be called before iSubEngineNotifier is set, 
			// so if it was called, then iSubBufferMemory would no longer be NULL and we can remove the notifier
			debugIOLog ("iSub was already attached\n");
			audioDMAEngine->iSubEngineNotifier->remove ();
			audioDMAEngine->iSubEngineNotifier = NULL;
		}
    } else {
		debugIOLog ("do not try to connect to iSub\n");
		if (NULL != audioDMAEngine->iSubBufferMemory) {
			debugIOLog ("disconnect from iSub\n");
			// We're already attached to an iSub, so detach
			cg = audioDMAEngine->getCommandGate ();
			if (NULL != cg) {
				cg->runAction (iSubCloseAction);
			}
		}

		// We're not attached to the iSub, so just remove our notifier
		if (NULL != audioDMAEngine->iSubEngineNotifier) {
			debugIOLog ("remove iSub notifier\n");
			audioDMAEngine->iSubEngineNotifier->remove ();
			audioDMAEngine->iSubEngineNotifier = NULL;
		}
	}

Exit:
    debugIOLog ("- AppleDBDMAAudioDMAEngine::iSubAttachChangeHandler\n");
    return result;
}

bool AppleDBDMAAudioDMAEngine::iSubEnginePublished (AppleDBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService) {
	IOReturn						result;
	bool							resultCode;
    IOCommandGate *					cg;

	debug4IOLog ("+AppleDBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p)\n", dbdmaEngineObject, (UInt32*)refCon, newService);

	resultCode = false;

	FailIf (NULL == dbdmaEngineObject, Exit);
	FailIf (NULL == newService, Exit);

	dbdmaEngineObject->iSubEngine = (AppleiSubEngine *)newService;
	FailIf (NULL == dbdmaEngineObject->iSubEngine, Exit);

	// Create the memory for the high/low samples to go into
    dbdmaEngineObject->lowFreqSamples = (float *)IOMallocAligned (round_page((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float)), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->lowFreqSamples, Exit);
    dbdmaEngineObject->highFreqSamples = (float *)IOMallocAligned (round_page((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float)), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->highFreqSamples, Exit);

	// Open the iSub which will cause it to create mute and volume controls
	dbdmaEngineObject->attach (dbdmaEngineObject->iSubEngine);
	cg = dbdmaEngineObject->getCommandGate ();
	FailWithAction (NULL == cg, dbdmaEngineObject->detach (dbdmaEngineObject->iSubEngine), Exit);
	dbdmaEngineObject->setSampleOffset(kMinimumLatencyiSub);	// HAL should notice this when iSub adds it's controls and sends out update
	IOSleep (102);
	result = cg->runAction (iSubOpenAction);
	FailWithAction (kIOReturnSuccess != result, dbdmaEngineObject->detach (dbdmaEngineObject->iSubEngine), Exit);
	dbdmaEngineObject->iSubBufferMemory = dbdmaEngineObject->iSubEngine->GetSampleBuffer ();
	debug2IOLog ("iSubBuffer length = %ld\n", dbdmaEngineObject->iSubBufferMemory->getLength ());

	// remove our notifier because we only care about the first iSub
	if (NULL != dbdmaEngineObject->iSubEngineNotifier) {
		dbdmaEngineObject->iSubEngineNotifier->remove ();
		dbdmaEngineObject->iSubEngineNotifier = NULL;
	}

	resultCode = true;
	dbdmaEngineObject->iSubOpen = TRUE;

Exit:
	if (FALSE == resultCode) {
		// We didn't actually open the iSub
		dbdmaEngineObject->iSubBufferMemory = NULL;
		dbdmaEngineObject->iSubEngine = NULL;
		dbdmaEngineObject->iSubOpen = FALSE;
		dbdmaEngineObject->setSampleOffset(kMinimumLatency);

		if (NULL != dbdmaEngineObject->lowFreqSamples) {
			IOFree (dbdmaEngineObject->lowFreqSamples, (dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float));
		}

		if (NULL != dbdmaEngineObject->highFreqSamples) {
			IOFree (dbdmaEngineObject->highFreqSamples, (dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float));
		}
	}

	debug5IOLog ("-AppleDBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p), result = %d\n", dbdmaEngineObject, (UInt32 *)refCon, newService, resultCode);
	return resultCode;
}

IOReturn AppleDBDMAAudioDMAEngine::iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
    if (NULL != owner) {
        AppleDBDMAAudioDMAEngine *		audioEngine;

		debugIOLog ("+AppleDBDMAAudioDMAEngine::iSubCloseAction\n");

		audioEngine = OSDynamicCast (AppleDBDMAAudioDMAEngine, owner);

        if (NULL != audioEngine && NULL != audioEngine->iSubEngine && TRUE == audioEngine->iSubOpen) {
			audioEngine->pauseAudioEngine ();
			audioEngine->beginConfigurationChange ();

			audioEngine->iSubEngine->closeiSub (audioEngine);

			audioEngine->completeConfigurationChange ();
			audioEngine->resumeAudioEngine ();

			audioEngine->detach (audioEngine->iSubEngine);

			audioEngine->iSubEngine = NULL;
			audioEngine->iSubBufferMemory = NULL;

			if (NULL != audioEngine->lowFreqSamples) {
				IOFree (audioEngine->lowFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->lowFreqSamples = NULL;
			}

			if (NULL != audioEngine->highFreqSamples) {
				IOFree (audioEngine->highFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->highFreqSamples = NULL;
			}
#if DEBUGLOG
			IOLog ("iSub connections terminated\n");
#endif
        } else {
#if DEBUGLOG
			IOLog ("didn't terminate the iSub connections because we didn't have an audioEngine\n");
#endif
		}
	} else {
#if DEBUGLOG
		IOLog ("didn't terminate the iSub connections owner = %p, arg1 = %p\n", owner, arg1);
#endif
    }

	debugIOLog ("-AppleDBDMAAudioDMAEngine::iSubCloseAction\n");
	return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudioDMAEngine::iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn					result;
	bool						resultBool;

	debugIOLog ("+AppleDBDMAAudioDMAEngine::iSubOpenAction\n");

	result = kIOReturnError;
	resultBool = FALSE;

    if (NULL != owner) {
        AppleDBDMAAudioDMAEngine *		audioEngine;

		audioEngine = OSDynamicCast (AppleDBDMAAudioDMAEngine, owner);
		resultBool = audioEngine->iSubEngine->openiSub (audioEngine);
    }

	if (resultBool) {
		result = kIOReturnSuccess;
	}

	debugIOLog ("-AppleDBDMAAudioDMAEngine::iSubOpenAction\n");
	return result;
}

bool AppleDBDMAAudioDMAEngine::willTerminate (IOService * provider, IOOptionBits options) {
    IOCommandGate *					cg;

	debug3IOLog ("+AppleDBDMAAudioDMAEngine[%p]::willTerminate (%p)\n", this, provider);

	if (iSubEngine == (AppleiSubEngine *)provider) {
		debugIOLog ("iSub requesting termination\n");

		cg = getCommandGate ();
		if (NULL != cg) {
			cg->runAction (iSubCloseAction);
		}

		// Set up notifier to run when iSub shows up again
		if (iSubAttach->getIntValue ()) {
			iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&iSubEnginePublished, this);
		}
	}

	debug2IOLog ("-AppleDBDMAAudioDMAEngine[%p]::willTerminate - about to call super::willTerminate ()\n", this);

	return super::willTerminate (provider, options);
}

bool AppleDBDMAAudioDMAEngine::getDmaState (void )
{
	return dmaRunState;
}

IOReturn AppleDBDMAAudioDMAEngine::getAudioStreamFormat( IOAudioStreamFormat * streamFormatPtr )
{
	if ( NULL != streamFormatPtr ) {
		streamFormatPtr->fNumChannels = dbdmaFormat.fNumChannels;
		streamFormatPtr->fSampleFormat = dbdmaFormat.fSampleFormat;
		streamFormatPtr->fNumericRepresentation = dbdmaFormat.fNumericRepresentation;
		streamFormatPtr->fBitDepth = dbdmaFormat.fBitDepth;
		streamFormatPtr->fBitWidth = dbdmaFormat.fBitWidth;
		streamFormatPtr->fAlignment = dbdmaFormat.fAlignment;
		streamFormatPtr->fByteOrder = dbdmaFormat.fByteOrder;
		streamFormatPtr->fIsMixable = dbdmaFormat.fIsMixable;
		streamFormatPtr->fDriverTag = dbdmaFormat.fDriverTag;
	}
	return kIOReturnSuccess;
}


#define DEBUGTIMESTAMPS		FALSE

#include "Apple02DBDMAAudioDMAEngine.h"
#include "Apple02Audio.h"

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioDebug.h>

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>

#include "AudioHardwareUtilities.h"
#include "AppleiSubEngine.h"

#pragma mark ------------------------ 
#pragma mark еее Constants
#pragma mark ------------------------ 

#ifdef _TIME_CLIP_ROUTINE
#define kCallFrequency 10
#endif 

extern "C" {
extern vm_offset_t phystokv(vm_offset_t pa);
};

#define super IOAudioEngine

OSDefineMetaClassAndStructors(Apple02DBDMAAudioDMAEngine, super)

const int Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex	= 0;
const int Apple02DBDMAAudioDMAEngine::kDBDMAOutputIndex	= 1;
const int Apple02DBDMAAudioDMAEngine::kDBDMAInputIndex	= 2;

#pragma mark ------------------------ 
#pragma mark еее IOAudioEngine Methods
#pragma mark ------------------------ 

bool Apple02DBDMAAudioDMAEngine::filterInterrupt(int index)
{
	// check to see if this interupt is because the DMA went bad
    UInt32 resultOut = IOGetDBDMAChannelStatus (ioBaseDMAOutput);
    UInt32 resultIn = 1;

	if (ioBaseDMAInput) {
		resultIn = IOGetDBDMAChannelStatus (ioBaseDMAInput) & kdbdmaActive;
	}

    if (!(resultOut & kdbdmaActive) || !resultIn) {
		mNeedToRestartDMA = TRUE;
	}

	// test the takeTimeStamp :it will increment the fCurrentLoopCount and time stamp it with the time now
	takeTimeStamp ();

    return false;
}

void Apple02DBDMAAudioDMAEngine::free()
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

    if (NULL != miSubProcessingParams.lowFreqSamples) {
        IOFree (miSubProcessingParams.lowFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

    if (NULL != miSubProcessingParams.highFreqSamples) {
        IOFree (miSubProcessingParams.highFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

    super::free();
}

UInt32 Apple02DBDMAAudioDMAEngine::getCurrentSampleFrame()
{
	SInt32		curFrame;

	curFrame = ((Apple02Audio *)audioDevice)->sndHWGetCurrentSampleFrame () % 16384;

	return curFrame;
}

bool Apple02DBDMAAudioDMAEngine::init(OSDictionary	*properties,
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

	CLOG("+ Apple02DBDMAAudioDMAEngine::init\n");
	result = FALSE;

	// Ususal check
	FailIf (FALSE == super::init (properties), Exit);
	FailIf (NULL == theDeviceProvider, Exit);

	// create the memory places for DMA
	map = theDeviceProvider->mapDeviceMemoryWithIndex(Apple02DBDMAAudioDMAEngine::kDBDMAOutputIndex);
	FailIf (NULL == map, Exit);
	ioBaseDMAOutput = (IODBDMAChannelRegisters *) map->getVirtualAddress();

	if(hasInput) {
		map = theDeviceProvider->mapDeviceMemoryWithIndex(Apple02DBDMAAudioDMAEngine::kDBDMAInputIndex);
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
 
	mInputDualMonoMode = e_Mode_Disabled;		   
		   
	resetiSubProcessingState();
	
	mUseSoftwareInputGain = false;	
	mInputGainLPtr = NULL;	
	mInputGainRPtr = NULL;	

#ifdef _TIME_CLIP_ROUTINE
	mCallCount = 0;
	mPreviousUptime.hi = 0;
	mPreviousUptime.lo = 0;
#endif

	result = TRUE;

Exit:
	CLOG("- Apple02DBDMAAudioDMAEngine::init\n");    
	return result;
}

bool Apple02DBDMAAudioDMAEngine::initHardware(IOService *provider)
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
            true,
			0
		};
	
	//	rbm 7.15.2002 keep a copy for user client
	dbdmaFormat.fNumChannels = format.fNumChannels;
	dbdmaFormat.fSampleFormat = format.fSampleFormat;
	dbdmaFormat.fNumericRepresentation = format.fNumericRepresentation;
	dbdmaFormat.fBitDepth = format.fBitDepth;
	dbdmaFormat.fBitWidth = format.fBitWidth;
	dbdmaFormat.fAlignment = format.fAlignment;
	dbdmaFormat.fByteOrder = format.fByteOrder;
	dbdmaFormat.fIsMixable = format.fIsMixable;
	dbdmaFormat.fDriverTag = format.fDriverTag;

    DEBUG_IOLOG("+ Apple02DBDMAAudioDMAEngine::initHardware()\n");
    
    ourProvider = provider;
	mNeedToRestartDMA = FALSE;

    FailIf (!super::initHardware(provider), Exit);
        
	// allocate the memory for the buffer
    sampleBufOut = (vm_offset_t)IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);
    if(ioBaseDMAInput)
        sampleBufIn = (vm_offset_t)IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);
    
	// create the streams
    stream = new IOAudioStream;
    if (stream) {
        const IOAudioSampleRate *rate;        
        rate = getSampleRate();
        
        stream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput, 1, 0, 0);
        stream->setSampleBuffer((void *)sampleBufOut, numBlocks * blockSize);
        stream->addAvailableFormat(&format, rate, rate);
        stream->setFormat(&format);

		// [3306295]
		format.fIsMixable = false;
        stream->addAvailableFormat(&format, rate, rate);
		format.fIsMixable = true;

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
	sampleBufferOutMemDescriptor = IOMemoryDescriptor::withAddress ((void *)sampleBufOut, numBlocks * blockSize, kIODirectionOut);
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
                                                                               Apple02DBDMAAudioDMAEngine::interruptHandler,
                                                                               Apple02DBDMAAudioDMAEngine::interruptFilter,
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
		sampleBufferInMemDescriptor = IOMemoryDescriptor::withAddress ((void *)sampleBufIn, numBlocks * blockSize, kIODirectionIn);
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

	chooseOutputClippingRoutinePtr();
	chooseInputConversionRoutinePtr();
	
	result = TRUE;

Exit:
    DEBUG_IOLOG("- Apple02DBDMAAudioDMAEngine::initHardware()\n");
    return result;
}


bool Apple02DBDMAAudioDMAEngine::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
    register Apple02DBDMAAudioDMAEngine *dmaEngine = (Apple02DBDMAAudioDMAEngine *)owner;
    bool result = true;

    if (dmaEngine) {
        result = dmaEngine->filterInterrupt(source->getIntIndex());
    }

    return result;
}

void Apple02DBDMAAudioDMAEngine::interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
    return;
}

IOReturn Apple02DBDMAAudioDMAEngine::performAudioEngineStart()
{
	IOPhysicalAddress			commandBufferPhys;
	IOReturn					result;

    debugIOLog(" + Apple02DBDMAAudioDMAEngine::performAudioEngineStart()\n");

	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !status || !interruptEventSource, Exit);

    flush_dcache((vm_offset_t)dmaCommandBufferOut, commandBufferSize, false);
    if(ioBaseDMAInput)
        flush_dcache((vm_offset_t)dmaCommandBufferIn, commandBufferSize, false);

	resetiSubProcessingState();

	*((UInt32 *)&mLastOutputSample) = 0;
	*((UInt32 *)&mLastInputSample) = 0;

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
	((Apple02Audio *)audioDevice)->sndHWSetCurrentSampleFrame (0);
	IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)commandBufferPhys);

	dmaRunState = TRUE;				//	rbm 7.12.02	added for user client support
	result = kIOReturnSuccess;

    debugIOLog(" - Apple02DBDMAAudioDMAEngine::performAudioEngineStart()\n");

Exit:
    return result;
}

IOReturn Apple02DBDMAAudioDMAEngine::performAudioEngineStop()
{
    UInt16 attemptsToStop = 2;

    debugIOLog("+ Apple02DBDMAAudioDMAEngine::performAudioEngineStop()\n");

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

    attemptsToStop = 2;

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

    DEBUG_IOLOG("- Apple02DBDMAAudioDMAEngine::performAudioEngineStop()\n");
    return kIOReturnSuccess;
}

// This gets called when a new audio stream needs to be mixed into an already playing audio stream
void Apple02DBDMAAudioDMAEngine::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
  if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
				
		resetiSubProcessingState();
		
		*((UInt32 *)&mLastOutputSample) = 0;
		*((UInt32 *)&mLastInputSample) = 0;

		#if DEBUGLOG
        IOLog ("+resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld, clipSampleFrame=%ld\n", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame, clipSampleFrame);
		#endif
        if (previousClippedToFrame < clipSampleFrame) {
			// Resetting the clip point backwards around the end of the buffer
			clipAdjustment = (getNumSampleFramesPerBuffer () - clipSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();
        } else {
			clipAdjustment = (previousClippedToFrame - clipSampleFrame) * iSubEngine->GetNumChannels();
        }
		#if DEBUGLOG
        if (clipAdjustment < kMinimumLatency) {
            IOLog ("resetClipPosition: 44.1 clipAdjustment < min, clipAdjustment=%ld\n", clipAdjustment); 
        }                
		#endif
        clipAdjustment = (clipAdjustment * 1000) / ((1000 * getSampleRate()->whole) / iSubEngine->GetSampleRate());  
        miSubProcessingParams.iSubBufferOffset -= clipAdjustment;

		#if DEBUGLOG
        if (clipAdjustment > (iSubBufferMemory->getLength () / 2)) {
            IOLog ("resetClipPosition: clipAdjustment > iSub buffer size, clipAdjustment=%ld\n", clipAdjustment); 
        }                
		#endif

        if (miSubProcessingParams.iSubBufferOffset < 0) {
			miSubProcessingParams.iSubBufferOffset += (iSubBufferMemory->getLength () / 2);	
			miSubProcessingParams.iSubLoopCount--;
        }

        previousClippedToFrame = clipSampleFrame;
        justResetClipPosition = TRUE;

		#if DEBUGLOG
        IOLog ("-resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld\n", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame);
		#endif
    }
}

IOReturn Apple02DBDMAAudioDMAEngine::restartDMA () {
	IOReturn					result;

	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !interruptEventSource, Exit);

	performAudioEngineStop ();
	performAudioEngineStart ();
	result = kIOReturnSuccess;

Exit:
    return result;
}

OSString *Apple02DBDMAAudioDMAEngine::getGlobalUniqueID()
{
    const char *className = NULL;
    const char *location = NULL;
    char *uniqueIDStr;
    OSString *localID = NULL;
    OSString *uniqueID = NULL;
    UInt32 uniqueIDSize;
    
	className = "AppleDBDMAAudioDMAEngine";
    
    location = getLocation();
    
    localID = getLocalUniqueID();
    
    uniqueIDSize = 3;
    
    if (className) {
        uniqueIDSize += strlen(className);
    }
    
    if (location) {
        uniqueIDSize += strlen(location);
    }
    
    if (localID) {
        uniqueIDSize += localID->getLength();
    }
        
    uniqueIDStr = (char *)IOMallocAligned(uniqueIDSize, sizeof (char));
    
    if (uniqueIDStr) {
		bzero(uniqueIDStr, uniqueIDSize);

        if (className) {
            sprintf(uniqueIDStr, "%s:", className);
        }
        
        if (location) {
            strcat(uniqueIDStr, location);
            strcat(uniqueIDStr, ":");
        }
        
        if (localID) {
            strcat(uniqueIDStr, localID->getCStringNoCopy());
            localID->release();
        }
        
        uniqueID = OSString::withCString(uniqueIDStr);
        
        IOFreeAligned(uniqueIDStr, uniqueIDSize);
    }

    return uniqueID;
}

void Apple02DBDMAAudioDMAEngine::setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency) {
	setOutputSampleLatency (outputLatency);
	setInputSampleLatency (inputLatency);
}

void Apple02DBDMAAudioDMAEngine::stop(IOService *provider)
{
    IOWorkLoop *workLoop;
    
    debug3IOLog(" + Apple02DBDMAAudioDMAEngine[%p]::stop(%p)\n", this, provider);
    
	if (provider == iSubEngine) {
		super::stop(provider);
		goto Exit;
	}
		
    if (interruptEventSource) {
        workLoop = getWorkLoop();
        if (workLoop) {
            workLoop->removeEventSource(interruptEventSource);
        }
    }
    
    super::stop(provider);
    stopAudioEngine();
Exit:
    debug3IOLog(" - Apple02DBDMAAudioDMAEngine[%p]::stop(%p)\n", this, provider);
}

#pragma mark ------------------------ 
#pragma mark еее Conversion Routines
#pragma mark ------------------------ 

// [3094574] aml, pick the correct output conversion routine based on our current state
void Apple02DBDMAAudioDMAEngine::chooseOutputClippingRoutinePtr()
{
	if (FALSE == dbdmaFormat.fIsMixable ) { // [3383910] need to do memcpy any time format is non-mixable
		mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipLegacyMemCopyToOutputStream;
	} else {
		if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
			if (32 == dbdmaFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSubMixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSubDelayRightChannel;
				} else {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSub;
				}
			} else if (16 == dbdmaFormat.fBitWidth) {
				if (TRUE == fNeedsPhaseInversion) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubInvertRightChannel;
				} else if (TRUE == fNeedsRightChanMixed) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubMixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubDelayRightChannel;
				} else {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSub;
				}
			} else {
				debugIOLog("Apple02DBDMAAudioDMAEngine::chooseOutputClippingRoutinePtr - iSub attached, non-supported output bit depth!\n");
			}
		} else {
			if (32 == dbdmaFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32MixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32DelayRightChannel;
				} else {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32;
				}
			} else if (16 == dbdmaFormat.fBitWidth) {
				if (TRUE == fNeedsPhaseInversion) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16InvertRightChannel;
				} else if (TRUE == fNeedsRightChanMixed) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16MixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16DelayRightChannel;
				} else {
					mClipAppleLegacyDBDMAToOutputStreamRoutine = &Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16;
				}
			} else {
				debugIOLog("Apple02DBDMAAudioDMAEngine::chooseOutputClippingRoutinePtr - Non-supported output bit depth!\n");
			}
		}
	}
}

// [3094574] aml, pick the correct input conversion routine based on our current state
void Apple02DBDMAAudioDMAEngine::chooseInputConversionRoutinePtr() 
{
	if (32 == dbdmaFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			if (fNeedsRightChanDelayInput) {// [3173869]
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32DelayRightWithGain;
			} else {
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32WithGain;
			}
		} else {
			mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32;
		}
	} else if (16 == dbdmaFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			if (fNeedsRightChanDelayInput) {// [3173869]
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16DelayRightWithGain;
			} else {
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16WithGain;
			}
		} else {
			if (e_Mode_CopyRightToLeft == mInputDualMonoMode) {
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16CopyR2L;
			} else if (e_Mode_CopyLeftToRight == mInputDualMonoMode) {
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16CopyL2R;
			} else {
				mConvertInputStreamToAppleLegacyDBDMARoutine = &Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16;
			}
		}
	} else {
		debugIOLog("Apple02DBDMAAudioDMAEngine::chooseInputConversionRoutinePtr - Non-supported input bit depth!\n");
	}
}

IOReturn Apple02DBDMAAudioDMAEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result = kIOReturnSuccess;
 
 	// if the DMA went bad restart it
	if (mNeedToRestartDMA) {
		mNeedToRestartDMA = FALSE;
		restartDMA ();
	}

	startTiming();

	if (0 != numSampleFrames) {
		// [3094574] aml, use function pointer instead of if/else block - handles both iSub and non-iSub clipping cases.
		result = (*this.*mClipAppleLegacyDBDMAToOutputStreamRoutine)(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
	}

	endTiming();

	return result;
}

// [3094574] aml, use function pointer instead of if/else block
IOReturn Apple02DBDMAAudioDMAEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result;

 	// if the DMA went bad restart it
	if (mNeedToRestartDMA) {
		mNeedToRestartDMA = FALSE;
		restartDMA ();
	}

	result = (*this.*mConvertInputStreamToAppleLegacyDBDMARoutine)(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat);

	return result;
}

#pragma mark ------------------------ 
#pragma mark еее Output Routines
#pragma mark ------------------------ 

// ------------------------------------------------------------------------
// Copy buffer directly to output
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipLegacyMemCopyToOutputStream (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
	UInt32			offset;
	UInt32			streamSize;

	streamSize = streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);
	offset = firstSampleFrame * streamSize;
	
	memcpy ((UInt8 *)sampleBuf + offset, (UInt8 *)mixBuf, numSampleFrames * streamSize);
	
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);

	balanceAdjust( inFloatBufferPtr, numSamples, (float *)&mLeftBalanceAdjust, (float *)&mRightBalanceAdjust);
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, invert phase to correct older iMac hardware
// assumes 2 channels.  Note that there is no 32 bit version of this 
// conversion routine, since the older hardware does not support it.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16InvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;
	
	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	invertRightChannel( inFloatBufferPtr, numSamples );
 
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );
   
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, mix right and left channels and mute right
// assumes 2 channels
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	mixAndMuteRightChannel( inFloatBufferPtr, numSamples );
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples, &mLastOutputSample );

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );
	
    return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Float32 to Native SInt32, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples, &mLastOutputSample );

	balanceAdjust( inFloatBufferPtr, numSamples, (float *)&mLeftBalanceAdjust, (float *)&mRightBalanceAdjust);

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );
	
    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32, mix right and left channels and mute right
// assumes 2 channels
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	mixAndMuteRightChannel( inFloatBufferPtr, numSamples );

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );

    return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее iSub Output Routines
#pragma mark ------------------------ 

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, assumes 2 channel data
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		maxSampleIndex, numSamples;
    float*		floatMixBuf;
    SInt16*	outputBuf16;
	UInt32		sampleIndex;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	Float32ToNativeInt16( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel(  &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, &mLastOutputSample );
	Float32ToNativeInt16( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	delayRightChannel(  &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, &mLastOutputSample );

	balanceAdjust( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, (float *)&mLeftBalanceAdjust, (float *)&mRightBalanceAdjust);

	Float32ToNativeInt16( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, invert right channel - assumes 2 channels 
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	invertRightChannel( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples );
	Float32ToNativeInt16( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream16iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	mixAndMuteRightChannel(  &high[firstSampleFrame * streamFormat->fNumChannels], numSamples );
	Float32ToNativeInt16( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, assumes 2 channel data
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	Float32ToNativeInt32( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf32, numSamples );

    // low side
  	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);

	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, delay right channel one sample
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, &mLastOutputSample );
	Float32ToNativeInt32( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, delay right channel one sample
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;

	delayRightChannel( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, &mLastOutputSample );

	balanceAdjust( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples, (float *)&mLeftBalanceAdjust, (float *)&mRightBalanceAdjust);

	Float32ToNativeInt32( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::clipAppleLegacyDBDMAToOutputStream32iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

    floatMixBuf = (float *)mixBuf;
	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    // Filter audio into low and high buffers using a 24 dB/octave crossover
	StereoFilter4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	mixAndMuteRightChannel( &high[firstSampleFrame * streamFormat->fNumChannels], numSamples );
	Float32ToNativeInt32( &high[firstSampleFrame * streamFormat->fNumChannels], outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее Input Routines
#pragma mark ------------------------ 

// ------------------------------------------------------------------------
// Native SInt16 to Float32
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
	
    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

   	NativeInt16ToFloat32(inputBuf16, floatDestBuf, numSamplesLeft, 16);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32, copy the rigth sample to the left channel for
// older machines only.  Note that there is no 32 bit version of this  
// function because older hardware does not support it.
// ------------------------------------------------------------------------
// [3306493]
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16CopyL2R(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
    
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
 
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt16ToFloat32CopyLeftToRight(inputBuf16, floatDestBuf, numSamplesLeft, 16);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32, copy the rigth sample to the left channel for
// older machines only.  Note that there is no 32 bit version of this  
// function because older hardware does not support it.
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16CopyR2L(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
    
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
 
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt16ToFloat32CopyRightToLeft(inputBuf16, floatDestBuf, numSamplesLeft, 16);

    return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Native SInt16 to Float32 with right channel delay and gain for TAS3004 compensation [3173869]
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
	
    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt16ToFloat32Gain(inputBuf16, floatDestBuf, numSamplesLeft, 16, mInputGainLPtr, mInputGainRPtr);

	delayRightChannel( floatDestBuf, numSamplesLeft , &mLastInputSample);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32, with software input gain
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream16WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
    
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt16ToFloat32Gain(inputBuf16, floatDestBuf, numSamplesLeft, 16, mInputGainLPtr, mInputGainRPtr);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;

    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt32ToFloat32(inputBuf32, floatDestBuf, numSamplesLeft, 32);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32, with software input gain
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;
  
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt32ToFloat32Gain(inputBuf32, floatDestBuf, numSamplesLeft, 32, mInputGainLPtr, mInputGainRPtr);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32 with right channel delay and gain for TAS3004 compensation [3173869]
// ------------------------------------------------------------------------
IOReturn Apple02DBDMAAudioDMAEngine::convertAppleLegacyDBDMAFromInputStream32DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;
  
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt32ToFloat32Gain(inputBuf32, floatDestBuf, numSamplesLeft, 32, mInputGainLPtr, mInputGainRPtr);

	delayRightChannel( floatDestBuf, numSamplesLeft , &mLastInputSample);

    return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее State Routines
#pragma mark ------------------------ 

void Apple02DBDMAAudioDMAEngine::setDualMonoMode(const DualMonoModeType inDualMonoMode) 
{ 
	mInputDualMonoMode = inDualMonoMode; 
	chooseInputConversionRoutinePtr();

	return;   	
}

void Apple02DBDMAAudioDMAEngine::setInputGainL(UInt32 inGainL) 
{ 
    if (mInputGainLPtr == NULL) {        
        mInputGainLPtr = (float *)IOMalloc(sizeof(float));
    }
    inputGainConverter(inGainL, mInputGainLPtr);
	
    return;   
} 

void Apple02DBDMAAudioDMAEngine::setInputGainR(UInt32 inGainR) 
{ 
    if (mInputGainRPtr == NULL) {        
        mInputGainRPtr = (float *)IOMalloc(sizeof(float));
    }
    inputGainConverter(inGainR, mInputGainRPtr);

    return;   
} 

// [3094574] aml, updated routines below to set the proper clipping routine

void Apple02DBDMAAudioDMAEngine::setPhaseInversion(const bool needsPhaseInversion) 
{
	fNeedsPhaseInversion = needsPhaseInversion; 
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void Apple02DBDMAAudioDMAEngine::setRightChanDelay(const bool needsRightChanDelay)  
{
	fNeedsRightChanDelay = needsRightChanDelay;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void Apple02DBDMAAudioDMAEngine::setRightChanMixed(const bool needsRightChanMixed)  
{
	fNeedsRightChanMixed = needsRightChanMixed;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void Apple02DBDMAAudioDMAEngine::setUseSoftwareInputGain(const bool inUseSoftwareInputGain) 
{     
	mUseSoftwareInputGain = inUseSoftwareInputGain;     	
	chooseInputConversionRoutinePtr();
	
	return;   
}

void Apple02DBDMAAudioDMAEngine::setRightChanDelayInput(const bool needsRightChanDelay)  // [3173869]
{
	// Don't call this because it messes up the input stream if two or more applications are recording at once.  [3398910]
/*
	debugIOLog ("setRightChanDelayInput (%d)\n", needsRightChanDelay);
	fNeedsRightChanDelayInput = needsRightChanDelay;  
	chooseInputConversionRoutinePtr();
*/
	return;   
}

void Apple02DBDMAAudioDMAEngine::setBalanceAdjust(const bool needsBalanceAdjust)  
{
	fNeedsBalanceAdjust = needsBalanceAdjust;  
	
	return;   
}

void Apple02DBDMAAudioDMAEngine::setLeftBalanceAdjust(UInt32 inVolume) 
{
	if (NULL != inVolume) {
		mLeftBalanceAdjust = inVolume;
    } else {
		mLeftBalanceAdjust = 0x3F800000;
	}
	return;   
} 

void Apple02DBDMAAudioDMAEngine::setRightBalanceAdjust(UInt32 inVolume) 
{ 
	if (NULL != inVolume) {
		mRightBalanceAdjust = inVolume;
    } else {
		mRightBalanceAdjust = 0x3F800000;
	}	
	return;   
} 

#pragma mark ------------------------ 
#pragma mark еее Format Routines
#pragma mark ------------------------ 

IOReturn Apple02DBDMAAudioDMAEngine::getAudioStreamFormat( IOAudioStreamFormat * streamFormatPtr )
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

bool Apple02DBDMAAudioDMAEngine::getDmaState (void )
{
	return dmaRunState;
}

IOReturn Apple02DBDMAAudioDMAEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
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

		// [3094574] aml, set the proper clipping routine
		chooseOutputClippingRoutinePtr();
		chooseInputConversionRoutinePtr();
	}

    return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее iSub Support
#pragma mark ------------------------ 

IOReturn Apple02DBDMAAudioDMAEngine::iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn						result;
    Apple02DBDMAAudioDMAEngine *		audioDMAEngine;
    IOCommandGate *					cg;

	debug5IOLog ("+ Apple02DBDMAAudioDMAEngine::iSubAttachChangeHandler (%p, %p, 0x%lx, 0x%lx)\n", target, attachControl, oldValue, newValue);

	result = kIOReturnSuccess;
	FailIf (oldValue == newValue, Exit);
    audioDMAEngine = OSDynamicCast (Apple02DBDMAAudioDMAEngine, target);
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
    debugIOLog ("- Apple02DBDMAAudioDMAEngine::iSubAttachChangeHandler\n");
    return result;
}

bool Apple02DBDMAAudioDMAEngine::iSubEnginePublished (Apple02DBDMAAudioDMAEngine * dbdmaEngineObject, void * refCon, IOService * newService) {
	IOReturn						result;
	bool							resultCode;
    IOCommandGate *					cg;

	debug4IOLog ("+Apple02DBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p)\n", dbdmaEngineObject, (UInt32*)refCon, newService);

	resultCode = false;

	FailIf (NULL == dbdmaEngineObject, Exit);
	FailIf (NULL == newService, Exit);

	dbdmaEngineObject->iSubEngine = (AppleiSubEngine *)newService;
	FailIf (NULL == dbdmaEngineObject->iSubEngine, Exit);

	// Create the memory for the high/low samples to go into
    dbdmaEngineObject->miSubProcessingParams.lowFreqSamples = (float *)IOMallocAligned ((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->miSubProcessingParams.lowFreqSamples, Exit);
    dbdmaEngineObject->miSubProcessingParams.highFreqSamples = (float *)IOMallocAligned ((dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float), PAGE_SIZE);
	FailIf (NULL == dbdmaEngineObject->miSubProcessingParams.highFreqSamples, Exit);

	// Open the iSub which will cause it to create mute and volume controls
	dbdmaEngineObject->attach (dbdmaEngineObject->iSubEngine);
//	dbdmaEngineObject->iSubEngine->retain ();
	cg = dbdmaEngineObject->getCommandGate ();
	FailWithAction (NULL == cg, dbdmaEngineObject->detach (dbdmaEngineObject->iSubEngine), Exit);
//	FailWithAction (NULL == cg, dbdmaEngineObject->iSubEngine->release(), Exit);
	dbdmaEngineObject->setSampleOffset(kMinimumLatencyiSub);	// HAL should notice this when iSub adds it's controls and sends out update
	IOSleep (102);
	result = cg->runAction (iSubOpenAction);
	FailWithAction (kIOReturnSuccess != result, dbdmaEngineObject->detach (dbdmaEngineObject->iSubEngine), Exit);
//	FailWithAction (kIOReturnSuccess != result, dbdmaEngineObject->iSubEngine->release (), Exit);
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

		if (NULL != dbdmaEngineObject->miSubProcessingParams.lowFreqSamples) {
			IOFree (dbdmaEngineObject->miSubProcessingParams.lowFreqSamples, (dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float));
		}

		if (NULL != dbdmaEngineObject->miSubProcessingParams.highFreqSamples) {
			IOFree (dbdmaEngineObject->miSubProcessingParams.highFreqSamples, (dbdmaEngineObject->numBlocks * dbdmaEngineObject->blockSize) * sizeof (float));
		}

	} else {
		// [3094574] aml - iSub opened sucessfully, update the clipping routine
		dbdmaEngineObject->chooseOutputClippingRoutinePtr();
	}
	
	debug5IOLog ("-Apple02DBDMAAudioDMAEngine::iSubEnginePublished (%p, %p, %p), result = %d\n", dbdmaEngineObject, (UInt32 *)refCon, newService, resultCode);
	return resultCode;
}

IOReturn Apple02DBDMAAudioDMAEngine::iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
    if (NULL != owner) {
        Apple02DBDMAAudioDMAEngine *		audioEngine;

		debugIOLog ("+Apple02DBDMAAudioDMAEngine::iSubCloseAction\n");

		audioEngine = OSDynamicCast (Apple02DBDMAAudioDMAEngine, owner);

        if (NULL != audioEngine && NULL != audioEngine->iSubEngine && TRUE == audioEngine->iSubOpen) {
			AppleiSubEngine *				oldiSubEngine;
			
			oldiSubEngine = audioEngine->iSubEngine;

			debugIOLog ("about to null iSub pointers\n");

			audioEngine->iSubEngine = NULL;
			audioEngine->iSubBufferMemory = NULL;
			
			audioEngine->pauseAudioEngine ();
			audioEngine->beginConfigurationChange ();

			debugIOLog ("about to close iSub\n");

			oldiSubEngine->closeiSub (audioEngine);

			debugIOLog ("about to choose clip routine\n");

			// [3094574] aml - iSub is gone, update the clipping routine while the engine is paused
			audioEngine->chooseOutputClippingRoutinePtr();

			debugIOLog ("about to choose detach iSub\n");

			audioEngine->detach (oldiSubEngine); //(audioEngine->iSubEngine);

			debugIOLog ("about to choose free crossover memory\n");

			if (NULL != audioEngine->miSubProcessingParams.lowFreqSamples) {
				IOFree (audioEngine->miSubProcessingParams.lowFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->miSubProcessingParams.lowFreqSamples = NULL;
			}

			if (NULL != audioEngine->miSubProcessingParams.highFreqSamples) {
				IOFree (audioEngine->miSubProcessingParams.highFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->miSubProcessingParams.highFreqSamples = NULL;
			}

			debugIOLog ("about to resume audio engine\n");

			audioEngine->completeConfigurationChange ();
			audioEngine->resumeAudioEngine ();

//			audioEngine->detach (oldiSubEngine); //(audioEngine->iSubEngine);
//			oldiSubEngine->release (); //(audioEngine->iSubEngine);

			//audioEngine->iSubEngine = NULL;
			//audioEngine->iSubBufferMemory = NULL;

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

	debugIOLog ("-Apple02DBDMAAudioDMAEngine::iSubCloseAction\n");
	return kIOReturnSuccess;
}

IOReturn Apple02DBDMAAudioDMAEngine::iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn					result;
	bool						resultBool;

	debugIOLog ("+Apple02DBDMAAudioDMAEngine::iSubOpenAction\n");

	result = kIOReturnError;
	resultBool = FALSE;

    if (NULL != owner) {
        Apple02DBDMAAudioDMAEngine *		audioEngine;

		audioEngine = OSDynamicCast (Apple02DBDMAAudioDMAEngine, owner);
		resultBool = audioEngine->iSubEngine->openiSub (audioEngine, &requestiSubClose);
//		resultBool = audioEngine->iSubEngine->openiSub (audioEngine);
    }

	if (resultBool) {
		result = kIOReturnSuccess;
	}

	debugIOLog ("-Apple02DBDMAAudioDMAEngine::iSubOpenAction\n");
	return result;
}

void Apple02DBDMAAudioDMAEngine::iSubSynchronize(UInt32 firstSampleFrame, UInt32 numSampleFrames) 
{
	void *						iSubBuffer = NULL;
	SInt32						offsetDelta;
	SInt32						safetyOffset;
	UInt32						iSubBufferLen = 0;
	iSubAudioFormatType			iSubFormat;	
	UInt32						distance;
	static UInt32				oldiSubBufferOffset;

	UInt32						adaptiveSampleRate;
	UInt32						sampleRate;

	// pass in:
	//
	// еее in the iSubProcessingParams structure, need to set before this method
	// iSubBufferLen		iSubBufferMemory->getLength ()
	// iSubBuffer			(void*)iSubBufferMemory->getVirtualSegment (0, &iSubBufferLen)
	// sampleRate 			getSampleRate()->whole
	// iSubFormat			iSubEngine->Get methods
	//
	// еее in values/pointers
	// iSubEngineLoopCount	iSubEngine->GetCurrentLoopCount ()
	// iSubEngineByteCount	iSubEngine->GetCurrentByteCount ()
	// 
	// еее io pointers							$$$
	// &needToSync				member
	// &startiSub				member
	// &justResetClipPosition	member		
	// &initialiSubLead			member
	// &previousClippedToFrame	member
	// iSubEngineNeedToSync		iSubEngine->GetNeedToSync(), iSubEngine->SetNeedToSync()

	FailIf (NULL == iSubBufferMemory || NULL == iSubEngine, Exit);
	
	iSubBufferLen = iSubBufferMemory->getLength ();		
	iSubBuffer = (void*)iSubBufferMemory->getVirtualSegment (0, &iSubBufferLen); 
	// (iSubBufferLen / 2) is because iSubBufferOffset is in UInt16s so convert iSubBufferLen to UInt16 length
	iSubBufferLen = iSubBufferLen / 2;

	sampleRate = getSampleRate()->whole;		
	adaptiveSampleRate = sampleRate;

	iSubFormat.altInterface = iSubEngine->GetAltInterface();	
	iSubFormat.numChannels = iSubEngine->GetNumChannels();		
	iSubFormat.bytesPerSample = iSubEngine->GetBytesPerSample();		
	iSubFormat.outputSampleRate = iSubEngine->GetSampleRate();		

	if (needToSync == FALSE) {
		UInt32			wrote;
		wrote = miSubProcessingParams.iSubBufferOffset - oldiSubBufferOffset;
//			IOLog ("wrote %ld iSub samples\n", wrote);
		if (miSubProcessingParams.iSubLoopCount == iSubEngine->GetCurrentLoopCount () && miSubProcessingParams.iSubBufferOffset > (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
			distance = miSubProcessingParams.iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
		} else if (miSubProcessingParams.iSubLoopCount == (iSubEngine->GetCurrentLoopCount () + 1) && miSubProcessingParams.iSubBufferOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
			distance = iSubBufferLen - (iSubEngine->GetCurrentByteCount () / 2) + miSubProcessingParams.iSubBufferOffset;
		} else {
			distance = initialiSubLead;
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
	if (needToSync == FALSE && previousClippedToFrame == firstSampleFrame && 0x0 != iSubEngine->GetCurrentLoopCount ()) {
		// aml - make the reader/writer check more strict - this helps get rid of long term crunchy iSub audio
		// the reader is now not allowed within one frame (one millisecond of audio) of the writer
		safetyOffset = miSubProcessingParams.iSubBufferOffset - ((iSubFormat.outputSampleRate) / 1000);		// 6 samples at 6kHz
		if (safetyOffset < 0) {
			safetyOffset += iSubBufferLen;
		}
		if (miSubProcessingParams.iSubLoopCount == iSubEngine->GetCurrentLoopCount () && safetyOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
			#if DEBUGLOG
			distance = miSubProcessingParams.iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
			IOLog ("****iSub is in front of write head safetyOffset = %ld, iSubEngine->GetCurrentByteCount () / 2 = %ld\n", safetyOffset, iSubEngine->GetCurrentByteCount () / 2);
//				IOLog ("distance = %ld\n", distance);
			#endif
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount > (iSubEngine->GetCurrentLoopCount () + 1)) {
			#if DEBUGLOG
			IOLog ("****looped more than the iSub iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld\n", miSubProcessingParams.iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
			#endif
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount < iSubEngine->GetCurrentLoopCount ()) {
			#if DEBUGLOG
			IOLog ("****iSub is ahead of us iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld\n", miSubProcessingParams.iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
			#endif
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount == iSubEngine->GetCurrentLoopCount () && miSubProcessingParams.iSubBufferOffset > ((SInt32)( (iSubEngine->GetCurrentByteCount() + (((iSubFormat.outputSampleRate)/1000 * NUM_ISUB_FRAME_LISTS_TO_QUEUE * NUM_ISUB_FRAMES_PER_LIST) * iSubFormat.bytesPerSample * iSubFormat.numChannels) ) / 2))) {			// aml 3.27.02, this is the right number here (buffersize was 2x too large).
					
			#if DEBUGLOG
			IOLog ("****iSub is too far behind write head iSubBufferOffset = %ld, (iSubEngine->GetCurrentByteCount () / 2 + max queued data) = %ld\n", miSubProcessingParams.iSubBufferOffset, (iSubEngine->GetCurrentByteCount() / 2 + iSubBufferLen/2));					
			#endif
			needToSync = TRUE;
			startiSub = TRUE;
		}
	}
	if (FALSE == needToSync && previousClippedToFrame != firstSampleFrame && !(previousClippedToFrame == getNumSampleFramesPerBuffer () && firstSampleFrame == 0)) {
		#if DEBUGLOG
		IOLog ("clipOutput: no sync: iSubBufferOffset was %ld\n", miSubProcessingParams.iSubBufferOffset);
		#endif
		if (firstSampleFrame < previousClippedToFrame) {
			#if DEBUGLOG
			IOLog ("clipOutput: no sync: firstSampleFrame < previousClippedToFrame (delta = %ld)\n", previousClippedToFrame-firstSampleFrame);
			#endif
			// We've wrapped around the buffer
			offsetDelta = (getNumSampleFramesPerBuffer () - firstSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();	
		} else {
			#if DEBUGLOG
			IOLog ("clipOutput: no sync: previousClippedToFrame < firstSampleFrame (delta = %ld)\n", firstSampleFrame - previousClippedToFrame);
			#endif
			offsetDelta = (firstSampleFrame - previousClippedToFrame) * iSubEngine->GetNumChannels();
		}
		// aml 3.21.02, adjust for new sample rate
		offsetDelta = (offsetDelta * 1000) / ((sampleRate * 1000) / iSubFormat.outputSampleRate);

		miSubProcessingParams.iSubBufferOffset += offsetDelta;
		#if DEBUGLOG
		IOLog ("clipOutput: no sync: clip to point was %ld, now %ld (delta = %ld)\n", previousClippedToFrame, firstSampleFrame, offsetDelta);
		IOLog ("clipOutput: no sync: iSubBufferOffset is now %ld\n", miSubProcessingParams.iSubBufferOffset);
		#endif
		if (miSubProcessingParams.iSubBufferOffset > (SInt32)iSubBufferLen) {
			#if DEBUGLOG
			IOLog ("clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset = %ld\n", miSubProcessingParams.iSubBufferOffset);
			#endif
			// Our calculated spot has actually wrapped around the iSub's buffer.
			miSubProcessingParams.iSubLoopCount += miSubProcessingParams.iSubBufferOffset / iSubBufferLen;
			miSubProcessingParams.iSubBufferOffset = miSubProcessingParams.iSubBufferOffset % iSubBufferLen;

			#if DEBUGLOG
			IOLog ("clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset is now %ld\n", miSubProcessingParams.iSubBufferOffset);
			#endif
		} else if (miSubProcessingParams.iSubBufferOffset < 0) {

			miSubProcessingParams.iSubBufferOffset += iSubBufferLen;

			#if DEBUGLOG
			IOLog ("clipOutput: no sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", miSubProcessingParams.iSubBufferOffset);
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
					
		resetiSubProcessingState();
					
		// aml 4.25.02 wipe out the iSub buffer, changed due to moving zeroing of iSub buffer in AUA write handler when aborting the pipe
		bzero(iSubBuffer, iSubBufferLen);

		curSampleFrame = getCurrentSampleFrame ();

		if (TRUE == restartedDMA) {
			miSubProcessingParams.iSubBufferOffset = initialiSubLead;		
			restartedDMA = FALSE;
		} else {
			if (firstSampleFrame < curSampleFrame) {
				offsetDelta = (getNumSampleFramesPerBuffer () - curSampleFrame + firstSampleFrame) * iSubEngine->GetNumChannels();
			} else {
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
			IOLog ("clipOutput: need to sync: iSubBufferOffset = %ld, offsetDelta = %ld\n", miSubProcessingParams.iSubBufferOffset, offsetDelta);
			#endif

			miSubProcessingParams.iSubBufferOffset = offsetDelta;
			#if DEBUGLOG
			IOLog ("clipOutput: need to sync: offsetDelta = %ld\n", offsetDelta);
			IOLog ("clipOutput: need to sync: firstSampleFrame = %ld, curSampleFrame = %ld\n", firstSampleFrame, curSampleFrame);
			IOLog ("clipOutput: need to sync: starting iSubBufferOffset = %ld, numSampleFrames = %ld\n", miSubProcessingParams.iSubBufferOffset, numSampleFrames);
			#endif
			if (miSubProcessingParams.iSubBufferOffset > (SInt32)iSubBufferLen) {
		
				needToSync = TRUE;	// aml 4.24.02, requests larger than our buffer size = bad!
				#if DEBUGLOG
				IOLog ("clipOutput: need to sync: SubBufferOffset too big (%ld) RESYNC!\n", miSubProcessingParams.iSubBufferOffset);
				#endif
				
				// Our calculated spot has actually wrapped around the iSub's buffer.

				miSubProcessingParams.iSubLoopCount += miSubProcessingParams.iSubBufferOffset / iSubBufferLen;
				miSubProcessingParams.iSubBufferOffset = miSubProcessingParams.iSubBufferOffset % iSubBufferLen;

				#if DEBUGLOG
				IOLog ("clipOutput: need to sync: iSubBufferOffset > iSubBufferLen (%ld), iSubBufferOffset is now %ld\n", iSubBufferLen, miSubProcessingParams.iSubBufferOffset);
				#endif
			} else if (miSubProcessingParams.iSubBufferOffset < 0) {

				miSubProcessingParams.iSubBufferOffset += iSubBufferLen;

				#if DEBUGLOG
				IOLog ("clipOutput: need to sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld\n", miSubProcessingParams.iSubBufferOffset);
				#endif
			}
			initialiSubLead = miSubProcessingParams.iSubBufferOffset;
		}
	}

	// [3094574] aml - updated iSub state, some of this could probably be done once off line, but it isn't any worse than before
	miSubProcessingParams.iSubBufferLen = iSubBufferLen;
	miSubProcessingParams.iSubFormat.altInterface = iSubEngine->GetAltInterface();
	miSubProcessingParams.iSubFormat.numChannels = iSubEngine->GetNumChannels();
	miSubProcessingParams.iSubFormat.bytesPerSample = iSubEngine->GetBytesPerSample();
	miSubProcessingParams.iSubFormat.outputSampleRate = iSubEngine->GetSampleRate();
	miSubProcessingParams.sampleRate = sampleRate;
	miSubProcessingParams.adaptiveSampleRate = adaptiveSampleRate;
	miSubProcessingParams.iSubBuffer = (SInt16*)iSubBuffer;

Exit:
	return;
}

void Apple02DBDMAAudioDMAEngine::resetiSubProcessingState() 
{ 	
	miSubProcessingParams.srcPhase =  1.0;		
	miSubProcessingParams.srcState =  0.0;		
				
	miSubProcessingParams.filterState.xl_1 = 0.0;
	miSubProcessingParams.filterState.xr_1 = 0.0;
	miSubProcessingParams.filterState.xl_2 = 0.0;
	miSubProcessingParams.filterState.xr_2 = 0.0;
	miSubProcessingParams.filterState.yl_1 = 0.0;
	miSubProcessingParams.filterState.yr_1 = 0.0;
	miSubProcessingParams.filterState.yl_2 = 0.0;
	miSubProcessingParams.filterState.yr_2 = 0.0;

	miSubProcessingParams.filterState2.xl_1 = 0.0;
	miSubProcessingParams.filterState2.xr_1 = 0.0;
	miSubProcessingParams.filterState2.xl_2 = 0.0;
	miSubProcessingParams.filterState2.xr_2 = 0.0;
	miSubProcessingParams.filterState2.yl_1 = 0.0;
	miSubProcessingParams.filterState2.yr_1 = 0.0;
	miSubProcessingParams.filterState2.yl_2 = 0.0;
	miSubProcessingParams.filterState2.yr_2 = 0.0;

	miSubProcessingParams.phaseCompState.xl_1 = 0.0;
	miSubProcessingParams.phaseCompState.xr_1 = 0.0;
	miSubProcessingParams.phaseCompState.xl_2 = 0.0;
	miSubProcessingParams.phaseCompState.xr_2 = 0.0;
	miSubProcessingParams.phaseCompState.yl_1 = 0.0;
	miSubProcessingParams.phaseCompState.yr_1 = 0.0;
	miSubProcessingParams.phaseCompState.yl_2 = 0.0;
	miSubProcessingParams.phaseCompState.yr_2 = 0.0;
	
	return;   	
}

void Apple02DBDMAAudioDMAEngine::requestiSubClose (IOAudioEngine * audioEngine) {
/*	Apple02DBDMAAudioDMAEngine *				dbdmaAudioEngine;
    IOCommandGate *								cg;


	dbdmaAudioEngine = OSDynamicCast (Apple02DBDMAAudioDMAEngine, audioEngine);

	cg = dbdmaAudioEngine->getCommandGate ();
	if (NULL != cg) {
		cg->runAction (dbdmaAudioEngine->iSubCloseAction);
	}

	// Set up notifier to run when iSub shows up again
	if (dbdmaAudioEngine->iSubAttach->getIntValue ()) {
		dbdmaAudioEngine->iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&dbdmaAudioEngine->iSubEnginePublished, dbdmaAudioEngine);
	}
*/
}

bool Apple02DBDMAAudioDMAEngine::willTerminate (IOService * provider, IOOptionBits options) {
    IOCommandGate *					cg;

	debug3IOLog ("+Apple02DBDMAAudioDMAEngine[%p]::willTerminate (%p)\n", this, provider);


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

	debug2IOLog ("-Apple02DBDMAAudioDMAEngine[%p]::willTerminate - about to call super::willTerminate ()\n", this);

	return super::willTerminate (provider, options);
}

void Apple02DBDMAAudioDMAEngine::updateiSubPosition(UInt32 firstSampleFrame, UInt32 numSampleFrames)
{
	if (TRUE == startiSub) {
		iSubEngine->StartiSub ();
		startiSub = FALSE;
		miSubProcessingParams.iSubLoopCount = 0;
 	}

	previousClippedToFrame = firstSampleFrame + numSampleFrames;
}

#pragma mark ------------------------ 
#pragma mark еее Utilities
#pragma mark ------------------------ 

inline void Apple02DBDMAAudioDMAEngine::startTiming() {
#ifdef _TIME_CLIP_ROUTINE
	AbsoluteTime				uptime;
	AbsoluteTime				lastuptime;
	AbsoluteTime				tempTime;
	UInt64						nanos;

	mCallCount++;
	clock_get_uptime (&uptime);
	tempTime = uptime;
	if ((mCallCount % kCallFrequency) == 0) {
		SUB_ABSOLUTETIME (&uptime, &mPreviousUptime);
		absolutetime_to_nanoseconds (uptime, &nanos);
		IOLog("clipOutputSamples[%ld]:\t%ld:", mCallCount, uptime.lo);
	}
	mPreviousUptime = tempTime;

	if ((mCallCount % kCallFrequency) == 0) {
		clock_get_uptime (&lastuptime);
	}	
#endif
}

inline void Apple02DBDMAAudioDMAEngine::endTiming() {
#ifdef _TIME_CLIP_ROUTINE
	if ((mCallCount % kCallFrequency) == 0) {
		clock_get_uptime (&uptime);
		SUB_ABSOLUTETIME (&uptime, &lastuptime);
		absolutetime_to_nanoseconds (uptime, &nanos);
		IOLog("%ld\n", uptime.lo);
	}
#endif
}

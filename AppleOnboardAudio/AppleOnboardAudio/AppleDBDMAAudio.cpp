#define DEBUGTIMESTAMPS		FALSE

#include "AppleDBDMAAudio.h"

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioDebug.h>

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>

#include "AudioHardwareUtilities.h"
#include "AppleiSubEngine.h"
#include "AppleOnboardAudio.h"

#pragma mark ------------------------ 
#pragma mark еее Constants
#pragma mark ------------------------ 

#ifdef _TIME_CLIP_ROUTINE
#define kCallFrequency 10
#endif 

#define USE_SOFT_DSP
//#define LOG_SOFT_DSP

extern "C" {
extern vm_offset_t phystokv(vm_offset_t pa);
};

#define super IOAudioEngine

OSDefineMetaClassAndStructors(AppleDBDMAAudio, IOAudioEngine)

const int AppleDBDMAAudio::kDBDMADeviceIndex	= 0;
const int AppleDBDMAAudio::kDBDMAOutputIndex	= 1;
const int AppleDBDMAAudio::kDBDMAInputIndex		= 2;

#pragma mark ------------------------ 
#pragma mark еее IOAudioEngine Methods
#pragma mark ------------------------ 

bool AppleDBDMAAudio::filterInterrupt (int index) {
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
	
	//	[3305011]	begin {
	//	Increment the activity counter that can be viewed with the AOA Viewer to verify DMA operation
	mDmaInterruptCount++;
	mDmaRecoveryInProcess = FALSE;
	//	} end	[3305011]
	
    return false;
}

void AppleDBDMAAudio::free()
{
	debugIOLog("+ AppleDBDMAAudio::free()\n");

    if (interruptEventSource) {
        interruptEventSource->release();
        interruptEventSource = 0;
    }
    
	if (NULL != mOutputStream) {
		mOutputStream->release ();
		mOutputStream = NULL;
	}

	if (NULL != mInputStream) {
		mInputStream->release ();
		mInputStream = NULL;
	}

	deallocateDMAMemory ();
	
    if (NULL != miSubProcessingParams.lowFreqSamples) {
        IOFree (miSubProcessingParams.lowFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

    if (NULL != miSubProcessingParams.highFreqSamples) {
        IOFree (miSubProcessingParams.highFreqSamples, (numBlocks * blockSize) * sizeof (float));
    }

	if (NULL != deviceFormats) {
		deviceFormats->release ();
		deviceFormats = NULL;
	}

    super::free();

	debugIOLog("- AppleDBDMAAudio::free()\n");
}

UInt32 AppleDBDMAAudio::getCurrentSampleFrame()
{
	SInt32		curFrame;

	curFrame = ((AppleOnboardAudio *)audioDevice)->getCurrentSampleFrame () % getNumSampleFramesPerBuffer ();

	return curFrame;
}

bool AppleDBDMAAudio::init (OSDictionary *			properties,
								 PlatformInterface* inPlatformInterface,
                                 IOService *		theDeviceProvider,
                                 bool				hasInput,
								 OSArray *			formatsArray,
                                 UInt32				nBlocks )
{
	Boolean					result;

	debug6IOLog( "+ AppleDBDMAAudio::init ( %X, %X, %d, %X, %X )\n",
			(unsigned int)properties,
			(unsigned int)theDeviceProvider,
			(unsigned int)hasInput,
			(unsigned int)formatsArray,
			(unsigned int)nBlocks);
			
	result = FALSE;
	
	//	[3305011]	begin {
	//	Init the dma activity counter that can be viewed with AOA Viewer
	mDmaInterruptCount = 0;
	mLastDmaInterruptCount = 0;
	mNumberOfFrozenDmaInterruptCounts = 0;
	mDmaRecoveryInProcess = FALSE;
	//	} end	[3305011]

	// Ususal check
	FailIf (FALSE == super::init (NULL), Exit);
	FailIf (NULL == theDeviceProvider, Exit);
	FailIf (NULL == formatsArray, Exit);
	FailIf (NULL == inPlatformInterface, Exit);

	mPlatformObject = inPlatformInterface;
	deviceFormats = formatsArray;
	deviceFormats->retain ();

	mDeviceProvider = theDeviceProvider; // i2s-a

	//	There is a system I/O controller dependency here.  Keylargo systems describe the DMA channel registers
	//	as having separate sets for input versus output while K2 systems describe a block of memory that
	//	encapsulates both the input and output channel registers.  Since there is a system I/O controller
	//	dependency, it would seem prudent to move the acquisition of these addresses to the appropriate
	//	platform interface instance (i.e. K2 Platform Interface or Keylargo Platform Interface).
	ioBaseDMAOutput = mPlatformObject->GetOutputChannelRegistersVirtualAddress ( mDeviceProvider );
	if ( NULL == ioBaseDMAOutput ) {
		debugIOLog ( "ioBaseDMAOutput = NULL\n" );
		IOSleep ( 50000 );
		FailIf ( true, Exit );
	}
	ioBaseDMAInput = hasInput ? mPlatformObject->GetInputChannelRegistersVirtualAddress ( mDeviceProvider ) : NULL ;
	if ( NULL == ioBaseDMAInput && hasInput ) {
		debugIOLog ( "ioBaseDMAInput = NULL\n" );
		IOSleep ( 50000 );
		FailIf ( true, Exit );
	}

	dmaCommandBufferIn = 0;
	dmaCommandBufferOut = 0;
	commandBufferSize = 0;
	interruptEventSource = 0;

	numBlocks = nBlocks;
	// blockSize will be init'ed when publishFormats calls stream->setFormat with the default format, which results in performFormatChange being called
	blockSize = 0;

	mInputDualMonoMode = e_Mode_Disabled;		   
		   
	resetiSubProcessingState();
	
	mUseSoftwareInputGain = false;	
	mInputGainLPtr = NULL;	
	mInputGainRPtr = NULL;	

	// set current processing state pointers
	mCurrentEQStructPtr = &mEQStructA;
	mCurrentLimiterStructPtr = &mLimiterStructA;
	mCurrentCrossoverStructPtr = &mCrossoverStructA;
	mCurrentInputEQStructPtr = &mInputEQStructA;	// [3306305]

	// init current state
	initializeSoftwareEQ ();
	initializeSoftwareLimiter ();
	initializeSoftwareCrossover ();

	mCurrentCrossoverStructPtr->outBufferPtr[0] = (float *)IOMallocAligned(16384, 16);
	mCurrentCrossoverStructPtr->outBufferPtr[1] = (float *)IOMallocAligned(16384, 16);

	// copy state to secondaries
	memcpy (&mEQStructB, &mEQStructA, sizeof (EQStruct));
	memcpy (&mLimiterStructB, &mLimiterStructA, sizeof (LimiterStruct));
	memcpy (&mCrossoverStructB, &mCrossoverStructA, sizeof (CrossoverStruct));
	memcpy (&mInputEQStructB, &mInputEQStructA, sizeof (EQStruct));	// [3306305]

    mOutputSampleBuffer = IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);

#ifdef _TIME_CLIP_ROUTINE
	mCallCount = 0;
	mPreviousUptime.hi = 0;
	mPreviousUptime.lo = 0;
#endif

	result = TRUE;

Exit:
	debug2IOLog( "- AppleDBDMAAudio::init returns %d\n", (unsigned int)result );
			
	return result;
}

bool AppleDBDMAAudio::initHardware (IOService *provider) {
    UInt32						interruptIndex;
    IOWorkLoop *				workLoop;
	Boolean						result;

	debugIOLog ("+ AppleDBDMAAudio::initHardware ()\n");

	result = FALSE;
    FailIf (!super::initHardware (provider), Exit);

    ourProvider = (AppleOnboardAudio *)provider;
	mNeedToRestartDMA = FALSE;

	// create the streams, this will also cause the DMA programs to be created.
	result = publishStreamFormats ();
	FailIf (FALSE == result, Exit);

	setSampleOffset (kMinimumLatency);
	// blockSize was set by the call to publishStreamFormats
	setNumSampleFramesPerBuffer (numBlocks * blockSize / ((mDBDMAOutputFormat.fBitWidth / 8) * mDBDMAOutputFormat.fNumChannels));

	debug4IOLog ("AppleDBDMAAudio:: setNumSampleFramesPerBuffer(%lu)  numBlocks=%lu blockSize=%lu \n",(numBlocks * blockSize / ((mDBDMAOutputFormat.fBitWidth / 8) * mDBDMAOutputFormat.fNumChannels)),numBlocks,blockSize);

	// install an interrupt handler only on the Output size of it !!! input only??
    workLoop = getWorkLoop();
    FailIf (!workLoop, Exit);

	debug2IOLog("AppleDBDMAudio::initHardware() interrupt source's name = %s\n", mDeviceProvider->getName());	

    interruptIndex = kDBDMAOutputIndex;
    interruptEventSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
																					AppleDBDMAAudio::interruptHandler,
																					AppleDBDMAAudio::interruptFilter,
																					mDeviceProvider,
																					interruptIndex);
    FailIf (!interruptEventSource, Exit);
    workLoop->addEventSource(interruptEventSource);
	// don't release interruptEventSource since we enable/disable it later

    iSubBufferMemory = NULL;
	iSubEngine = NULL;

	// Set up a control that sound prefs can set to tell us if we should install our notifier or not
	iSubAttach = IOAudioToggleControl::create (FALSE,
										kIOAudioControlChannelIDAll,
										kIOAudioControlChannelNameAll,
										0,
										kIOAudioToggleControlSubTypeiSubAttach,
										kIOAudioControlUsageOutput);

	// Don't release this control. We reference it when we terminate.
	if (NULL != iSubAttach) {
		addDefaultAudioControl (iSubAttach);
		iSubAttach->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)iSubAttachChangeHandler, this);
	}

//	performAudioEngineStart();
//	performAudioEngineStop();
	result = TRUE;

Exit:
	debug2IOLog("- AppleDBDMAAudio::initHardware() returns %d\n", result);
    return result;
}

UInt32 AppleDBDMAAudio::GetEncodingFormat (OSString * theEncoding) {
	UInt32						sampleFormat;

	sampleFormat = 0x3F3F3F3F;			// because '????' is a trigraph....
	
	if (NULL != theEncoding) {
		if (theEncoding->isEqualTo ("PCM")) {
			sampleFormat = kIOAudioStreamSampleFormatLinearPCM;
		} else if (theEncoding->isEqualTo ("AC3")) {
			sampleFormat = kIOAudioStreamSampleFormat1937AC3;
		}
	}
	
	return sampleFormat;
}

bool AppleDBDMAAudio::publishStreamFormats (void) {
	OSArray *							sampleRatesArray;
	OSDictionary *						formatDict;
	IOAudioSampleRate					sampleRate;
	IOAudioStreamFormat					dbdmaFormat;
    IOAudioStreamFormatExtension		dbdmaFormatExtension;
	UInt32								numFormats;
	UInt32								formatIndex;
	UInt32								rateIndex;
	UInt32								numSampleRates;
	bool								result;

	result = FALSE;

    if (ioBaseDMAInput) {
		mInputStream = new IOAudioStream;
	}

    mOutputStream = new IOAudioStream;
	FailIf (NULL == mOutputStream, Exit);

	mOutputStream->initWithAudioEngine (this, kIOAudioStreamDirectionOutput, 1, 0, 0);
	if (mInputStream) mInputStream->initWithAudioEngine (this, kIOAudioStreamDirectionInput, 1, 0, 0);

	mDBDMAInputFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
	mDBDMAInputFormat.fIsMixable = TRUE;

	dbdmaFormat.fNumericRepresentation = kIOAudioStreamNumericRepresentationSignedInt;
	dbdmaFormat.fAlignment = kIOAudioStreamAlignmentHighByte;
	dbdmaFormat.fByteOrder = kIOAudioStreamByteOrderBigEndian;
	dbdmaFormat.fIsMixable = true;
	dbdmaFormat.fDriverTag = 0;

	dbdmaFormatExtension.fVersion = kFormatExtensionCurrentVersion;
	dbdmaFormatExtension.fFlags = 0;

	sampleRate.fraction = 0;

	numFormats = deviceFormats->getCount ();
	for (formatIndex = 0; formatIndex < numFormats; formatIndex++) {
		formatDict = OSDynamicCast (OSDictionary, deviceFormats->getObject (formatIndex));
		FailIf (NULL == formatDict, Exit);
		dbdmaFormat.fSampleFormat = GetEncodingFormat ((OSString *)formatDict->getObject (kEncoding));
		dbdmaFormat.fNumChannels = ((OSNumber *)formatDict->getObject(kChannels))->unsigned32BitValue ();
		dbdmaFormat.fBitDepth = ((OSNumber *)formatDict->getObject(kBitDepth))->unsigned32BitValue ();
		dbdmaFormat.fBitWidth = ((OSNumber *)formatDict->getObject(kBitWidth))->unsigned32BitValue ();
		dbdmaFormat.fIsMixable = ((OSBoolean *)formatDict->getObject(kIsMixable))->getValue ();
		dbdmaFormatExtension.fFramesPerPacket = 1;
		dbdmaFormatExtension.fBytesPerPacket = dbdmaFormat.fNumChannels * (dbdmaFormat.fBitWidth / 8);

		debug4IOLog("dbdmaFormat: fNumChannels = %d, fBitDepth = %d, fBitWidth = %d\n", (unsigned int)dbdmaFormat.fNumChannels, (unsigned int)dbdmaFormat.fBitDepth, (unsigned int)dbdmaFormat.fBitWidth);
		sampleRatesArray = OSDynamicCast (OSArray, formatDict->getObject (kSampleRates));
		FailIf (NULL == sampleRatesArray, Exit);
		numSampleRates = sampleRatesArray->getCount ();

		if (kIOAudioStreamSampleFormat1937AC3 == dbdmaFormat.fSampleFormat) {
			dbdmaFormatExtension.fFramesPerPacket = 1536;
			dbdmaFormatExtension.fBytesPerPacket = dbdmaFormatExtension.fFramesPerPacket * dbdmaFormat.fNumChannels * (dbdmaFormat.fBitWidth / 8);
		}

		for (rateIndex = 0; rateIndex < numSampleRates; rateIndex++) {
			sampleRate.whole = ((OSNumber *)sampleRatesArray->getObject(rateIndex))->unsigned32BitValue ();
			debug2IOLog("dbdmaFormat: sampleRate.whole = %d\n", (unsigned int)sampleRate.whole);
			mOutputStream->addAvailableFormat (&dbdmaFormat, &dbdmaFormatExtension, &sampleRate, &sampleRate);
			if (mInputStream && kIOAudioStreamSampleFormatLinearPCM == dbdmaFormat.fSampleFormat) {
				mInputStream->addAvailableFormat (&dbdmaFormat, &sampleRate, &sampleRate);
			}
			// XXX Remove hardcoding of default format!
			if (dbdmaFormat.fNumChannels == 2 && dbdmaFormat.fBitDepth == 16 && sampleRate.whole == 44100 && kIOAudioStreamSampleFormatLinearPCM == dbdmaFormat.fSampleFormat) {
				debugIOLog("dbdmaFormat: mOutputStream->setFormat to 2, 16, 44100\n");
				mOutputStream->setFormat (&dbdmaFormat);
				if (mInputStream) mInputStream->setFormat (&dbdmaFormat);
				setSampleRate (&sampleRate);
				ourProvider->formatChangeRequest (NULL, &sampleRate);
			}
			
			// [3306295] all mixable formats get duplicated as non-mixable for hog mode
			if (dbdmaFormat.fIsMixable) {
				dbdmaFormat.fIsMixable = false;
				mOutputStream->addAvailableFormat (&dbdmaFormat, &dbdmaFormatExtension, &sampleRate, &sampleRate);
				dbdmaFormat.fIsMixable = true;
			}
		}
	}

//	allocateDMABuffers ();

//	FailIf (NULL == mOutputSampleBuffer, Exit);
//    if (ioBaseDMAInput) 
//        FailIf (NULL == mInputSampleBuffer, Exit);

//	mOutputStream->setSampleBuffer ((void *)mOutputSampleBuffer, numBlocks * blockSize);
//	if (mInputStream) mInputStream->setSampleBuffer ((void *)mInputSampleBuffer, numBlocks * blockSize);

	addAudioStream (mOutputStream);

	if (mInputStream) {
		addAudioStream (mInputStream);
	}

	result = TRUE;

Exit:
	return result;
}

void AppleDBDMAAudio::deallocateDMAMemory () {
    if (dmaCommandBufferOut && (commandBufferSize > 0)) {
        IOFreeAligned(dmaCommandBufferOut, commandBufferSize);
        dmaCommandBufferOut = NULL;
    }
    
    if (dmaCommandBufferIn && (commandBufferSize > 0)) {
        IOFreeAligned(dmaCommandBufferIn, commandBufferSize);
        dmaCommandBufferOut = NULL;
    }

	if (NULL != dmaCommandBufferOutMemDescriptor) {
		dmaCommandBufferOutMemDescriptor->release ();
		dmaCommandBufferOutMemDescriptor = NULL;
	}
	if (NULL != dmaCommandBufferInMemDescriptor) {
		dmaCommandBufferInMemDescriptor->release ();
		dmaCommandBufferInMemDescriptor = NULL;
	}	
	if (NULL != sampleBufferOutMemDescriptor) {
		sampleBufferOutMemDescriptor->release ();
		sampleBufferOutMemDescriptor = NULL;
	}
	if (NULL != sampleBufferInMemDescriptor) {
		sampleBufferInMemDescriptor->release ();
		sampleBufferInMemDescriptor = NULL;
	}
	if (NULL != stopCommandOutMemDescriptor) {
		stopCommandOutMemDescriptor->release ();
		stopCommandOutMemDescriptor = NULL;
	}
	if (NULL != stopCommandInMemDescriptor) {
		stopCommandInMemDescriptor->release ();
		stopCommandInMemDescriptor = NULL;
	}

	return;
}

void AppleDBDMAAudio::allocateDMABuffers (void) {

	debug2IOLog ("allocateDMABuffers: buffer size = %ld\n", numBlocks * blockSize);

    mOutputSampleBuffer = IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);

	if(ioBaseDMAInput) {
        mInputSampleBuffer = IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);
	}
}

bool AppleDBDMAAudio::allocateOutputDMADescriptors (void) {
	bool						result;

	result = FALSE;

    commandBufferSize = (numBlocks + 1) * sizeof(IODBDMADescriptor);
    dmaCommandBufferOut = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32);	// needs to be more than 4 byte aligned
    FailIf (!dmaCommandBufferOut, Exit);

	dmaCommandBufferOutMemDescriptor = IOMemoryDescriptor::withAddress (dmaCommandBufferOut, commandBufferSize, kIODirectionOutIn);
	FailIf (NULL == dmaCommandBufferOutMemDescriptor, Exit);
	sampleBufferOutMemDescriptor = IOMemoryDescriptor::withAddress (mOutputSampleBuffer, numBlocks * blockSize, kIODirectionOutIn);
	FailIf (NULL == sampleBufferOutMemDescriptor, Exit);
	stopCommandOutMemDescriptor = IOMemoryDescriptor::withAddress (&dmaCommandBufferOut[numBlocks], sizeof (IODBDMADescriptor *), kIODirectionOutIn);
	FailIf (NULL == stopCommandOutMemDescriptor, Exit);

	result = TRUE;

Exit:
	return result;
}

bool AppleDBDMAAudio::allocateInputDMADescriptors (void) {
	bool						result;

	result = FALSE;

	if(ioBaseDMAInput) {
		commandBufferSize = (numBlocks + 1) * sizeof(IODBDMADescriptor);	// needs to be more than 4 byte aligned
		
		dmaCommandBufferIn = (IODBDMADescriptor *)IOMallocAligned(commandBufferSize, 32);	// needs to be more than 4 byte aligned
        FailIf (!dmaCommandBufferIn, Exit);

		dmaCommandBufferInMemDescriptor = IOMemoryDescriptor::withAddress (dmaCommandBufferIn, commandBufferSize, kIODirectionOutIn);
		FailIf (NULL == dmaCommandBufferInMemDescriptor, Exit);
		sampleBufferInMemDescriptor = IOMemoryDescriptor::withAddress (mInputSampleBuffer, numBlocks * blockSize, kIODirectionOutIn);
		FailIf (NULL == sampleBufferInMemDescriptor, Exit);
		stopCommandInMemDescriptor = IOMemoryDescriptor::withAddress (&dmaCommandBufferIn[numBlocks], sizeof (IODBDMADescriptor *), kIODirectionOutIn);
		FailIf (NULL == stopCommandInMemDescriptor, Exit);
	}

	result = TRUE;

Exit:
	return result;
}

bool AppleDBDMAAudio::createDMAPrograms (bool hasInput) {
	vm_offset_t					offset;
	IOPhysicalAddress			commandBufferPhys;
	IOPhysicalAddress			sampleBufferPhys;
	IOPhysicalAddress			stopCommandPhys;
	UInt32						dmaCommand;
	UInt32						blockNum;
	Boolean						doInterrupt;
	bool						result;

	result = FALSE;			// Didn't successfully create DMA program

    offset = 0;
    dmaCommand = kdbdmaOutputMore;
	doInterrupt = false;

	result = allocateOutputDMADescriptors ();
	FailIf (FALSE == result, Exit);

	commandBufferPhys = dmaCommandBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == commandBufferPhys, Exit);
	sampleBufferPhys = sampleBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == sampleBufferPhys, Exit);
	stopCommandPhys = stopCommandOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == stopCommandPhys, Exit);

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
        doInterrupt = false;
        offset = 0;
        dmaCommand = kdbdmaInputMore;    
        
		result = allocateInputDMADescriptors ();
		FailIf (FALSE == result, Exit);

		commandBufferPhys = dmaCommandBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		sampleBufferPhys = sampleBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == sampleBufferPhys, Exit);
		stopCommandPhys = stopCommandInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == stopCommandPhys, Exit);

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

	result = TRUE;

Exit:
	return result;
}

bool AppleDBDMAAudio::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
    register AppleDBDMAAudio *dmaEngine = (AppleDBDMAAudio *)owner;
    bool result = true;

    if (dmaEngine) {
        result = dmaEngine->filterInterrupt(source->getIntIndex());
    }

    return result;
}

void AppleDBDMAAudio::interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
    return;
}

IOReturn AppleDBDMAAudio::performAudioEngineStart()
{
	IOPhysicalAddress			commandBufferPhys;
	IOReturn					result;
#if DEBUGLOG
	IOLog(" + AppleDBDMAAudio::performAudioEngineStart()\n");
#endif
	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !status || !interruptEventSource, Exit);

    flush_dcache((vm_offset_t)dmaCommandBufferOut, commandBufferSize, false);
    if(ioBaseDMAInput)
        flush_dcache((vm_offset_t)dmaCommandBufferIn, commandBufferSize, false);

	resetiSubProcessingState();

	*((UInt32 *)&mLastOutputSample) = 0;
	*((UInt32 *)&mLastInputSample) = 0;

	resetEQ (mCurrentEQStructPtr);
	resetEQ (mCurrentInputEQStructPtr);	// [3306305]
	resetLimiter (mCurrentLimiterStructPtr);
	resetCrossover (mCurrentCrossoverStructPtr);

    if (NULL != iSubEngine) {
		startiSub = TRUE;
		needToSync = TRUE;
    }

    interruptEventSource->enable();

	// add the time stamp take to test
    takeTimeStamp(false);

#if DEBUGLOG
	IOLog ( "getNumSampleFramesPerBuffer %ld\n", getNumSampleFramesPerBuffer() );
#endif
	((AppleOnboardAudio *)audioDevice)->setCurrentSampleFrame (0);
	if ( mPlatformObject ) { mPlatformObject->LogDBDMAChannelRegisters(); }

	// start the input DMA first
    if(ioBaseDMAInput) {
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOClearDBDMAChannelControlBits(kdbdmaS0));
        IOSetDBDMABranchSelect(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
		commandBufferPhys = dmaCommandBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		IODBDMAStart(ioBaseDMAInput, (IODBDMADescriptor *)commandBufferPhys);
#if DEBUGLOG
		IOLog("AppleDBDMAAudio::performAudioEngineStart - starting input DMA\n");
#endif
   }
    
    IOSetDBDMAChannelControl(ioBaseDMAOutput, IOClearDBDMAChannelControlBits(kdbdmaS0));
    IOSetDBDMABranchSelect(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
	commandBufferPhys = dmaCommandBufferOutMemDescriptor->getPhysicalAddress ();
	FailIf (NULL == commandBufferPhys, Exit);
	IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)commandBufferPhys);
	if ( mPlatformObject ) { mPlatformObject->LogDBDMAChannelRegisters(); }

	dmaRunState = TRUE;				//	rbm 7.12.02	added for user client support
	result = kIOReturnSuccess;

#if DEBUGLOG
    IOLog(" - AppleDBDMAAudio::performAudioEngineStart() returns %X\n", result);
#endif

Exit:
    return result;
}

IOReturn AppleDBDMAAudio::performAudioEngineStop()
{
    UInt16 attemptsToStop = kDBDMAAttemptsToStop; // [3282437], was 1000

    debugIOLog("+ AppleDBDMAAudio::performAudioEngineStop()\n");

    if (NULL != iSubEngine) {
        iSubEngine->StopiSub ();
        needToSync = TRUE;
    }

    if (!interruptEventSource) {
        return kIOReturnError;
    }

    interruptEventSource->disable();
        
	// stop the output
	if ( NULL != ioBaseDMAOutput ) {
		IOSetDBDMAChannelControl(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
		while ((IOGetDBDMAChannelStatus(ioBaseDMAOutput) & kdbdmaActive) && (attemptsToStop--)) {
			eieio();
			IOSleep(1);
		}
	
		IODBDMAStop(ioBaseDMAOutput);
		IODBDMAReset(ioBaseDMAOutput);
	} else {
		debugIOLog ( "AppleDBDMAAudio::performAudioEngineStop has no output\n" );
	}

	// stop the input
    if(ioBaseDMAInput){
		attemptsToStop = kDBDMAAttemptsToStop; // [3282437]
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
        while ((IOGetDBDMAChannelStatus(ioBaseDMAInput) & kdbdmaActive) && (attemptsToStop--)) {
            eieio();
            IOSleep(1);
        }

        IODBDMAStop(ioBaseDMAInput);
        IODBDMAReset(ioBaseDMAInput);
	} else {
		debugIOLog ( "AppleDBDMAAudio::performAudioEngineStop has no input\n" );
    }
    
	dmaRunState = FALSE;				//	rbm 7.12.02	added for user client support
    interruptEventSource->enable();

    DEBUG_IOLOG("- AppleDBDMAAudio::performAudioEngineStop()\n");
    return kIOReturnSuccess;
}

// This gets called when a new audio stream needs to be mixed into an already playing audio stream
void AppleDBDMAAudio::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
  if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
				
		resetiSubProcessingState();

		resetEQ (mCurrentEQStructPtr);
		resetEQ (mCurrentInputEQStructPtr);	// [3306305]
		resetLimiter (mCurrentLimiterStructPtr);
		resetCrossover (mCurrentCrossoverStructPtr);

		*((UInt32 *)&mLastOutputSample) = 0;
		*((UInt32 *)&mLastInputSample) = 0;

		#if DEBUGLOG
        debug4IOLog ("+resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld, clipSampleFrame=%ld\n", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame, clipSampleFrame);
		#endif
        if (previousClippedToFrame < clipSampleFrame) {
			// Resetting the clip point backwards around the end of the buffer
			clipAdjustment = (getNumSampleFramesPerBuffer () - clipSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();
        } else {
			clipAdjustment = (previousClippedToFrame - clipSampleFrame) * iSubEngine->GetNumChannels();
        }
		#if DEBUGLOG
        if (clipAdjustment < kMinimumLatency) {
            debug2IOLog ("resetClipPosition: 44.1 clipAdjustment < min, clipAdjustment=%ld\n", clipAdjustment); 
        }                
		#endif
        clipAdjustment = (clipAdjustment * 1000) / ((1000 * getSampleRate()->whole) / iSubEngine->GetSampleRate());  
        miSubProcessingParams.iSubBufferOffset -= clipAdjustment;

		#if DEBUGLOG
        if (clipAdjustment > (iSubBufferMemory->getLength () / 2)) {
            debug2IOLog ("resetClipPosition: clipAdjustment > iSub buffer size, clipAdjustment=%ld\n", clipAdjustment); 
        }                
		#endif

        if (miSubProcessingParams.iSubBufferOffset < 0) {
			miSubProcessingParams.iSubBufferOffset += (iSubBufferMemory->getLength () / 2);	
			miSubProcessingParams.iSubLoopCount--;
        }

        previousClippedToFrame = clipSampleFrame;
        justResetClipPosition = TRUE;

		#if DEBUGLOG
        debug3IOLog ("-resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld\n", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame);
		#endif
    }
}

IOReturn AppleDBDMAAudio::restartDMA () {
	IOReturn					result;

	result = kIOReturnError;
    FailIf (!ioBaseDMAOutput || !dmaCommandBufferOut || !interruptEventSource, Exit);

	performAudioEngineStop ();
	performAudioEngineStart ();
	result = kIOReturnSuccess;

Exit:
    return result;
}

void AppleDBDMAAudio::setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency) {
	setOutputSampleLatency (outputLatency);
	setInputSampleLatency (inputLatency);
}

void AppleDBDMAAudio::stop(IOService *provider)
{
    IOWorkLoop *workLoop;
    
    DEBUG3_IOLOG(" + AppleDBDMAAudio[%p]::stop(%p)\n", this, provider);
    
    if (interruptEventSource) {
        workLoop = getWorkLoop();
        if (workLoop) {
            workLoop->removeEventSource(interruptEventSource);
        }
    }
 	
    if (NULL != iSubEngineNotifier) {
        iSubEngineNotifier->remove ();
		iSubEngineNotifier = NULL;
    }
   
    super::stop(provider);
    stopAudioEngine();
	
    DEBUG3_IOLOG(" - AppleDBDMAAudio[%p]::stop(%p)\n", this, provider);
}


void AppleDBDMAAudio::detach(IOService *provider)
{
	super::detach(provider);
    debug2IOLog("AppleDBDMAAudio::detach(%p)\n", provider);
}

OSString *AppleDBDMAAudio::getGlobalUniqueID()
{
    const char *className = NULL;
    const char *location = NULL;
    char *uniqueIDStr;
    OSString *localID = NULL;
    OSString *uniqueID = NULL;
    UInt32 uniqueIDSize;
    
	className = "Apple03DBDMAAudio";
    
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

#pragma mark ------------------------ 
#pragma mark еее Conversion Routines
#pragma mark ------------------------ 

// [3094574] aml, pick the correct output conversion routine based on our current state
void AppleDBDMAAudio::chooseOutputClippingRoutinePtr()
{
	if (FALSE == mDBDMAOutputFormat.fIsMixable) { // [3281454], no iSub during encoded playback either
		mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipMemCopyToOutputStream;
	} else {
	if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
		if (32 == mDBDMAOutputFormat.fBitWidth) {
			if (TRUE == fNeedsRightChanMixed) {
				mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubMixRightChannel;
			} else if (TRUE == fNeedsRightChanDelay) {
				if (TRUE == fNeedsBalanceAdjust) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubDelayRightChannelBalance;
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubDelayRightChannel;
				}
			} else {
				mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSub;
			}
		} else if (16 == mDBDMAOutputFormat.fBitWidth) {
			if (TRUE == fNeedsPhaseInversion) {
				mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubInvertRightChannel;
			} else if (TRUE == fNeedsRightChanMixed) {
				mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubMixRightChannel;
			} else if (TRUE == fNeedsRightChanDelay) {
				if (TRUE == fNeedsBalanceAdjust) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubDelayRightChannelBalance;
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubDelayRightChannel;
				}
			} else {
				mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSub;
			}
		} else {
				debugIOLog("AppleDBDMAAudio::chooseOutputClippingRoutinePtr - Non-supported output bit depth, iSub attached.\n");
			}	
		} else {
			if (32 == mDBDMAOutputFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32MixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					if (TRUE == fNeedsBalanceAdjust) {
						mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32DelayRightChannelBalance;
					} else {
						mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32DelayRightChannel;
					}
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32;
				}
			} else if (16 == mDBDMAOutputFormat.fBitWidth) {
				if (TRUE == fNeedsPhaseInversion) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16InvertRightChannel;
				} else if (TRUE == fNeedsRightChanMixed) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16MixRightChannel;
				} else if (TRUE == fNeedsRightChanDelay) {
					if (TRUE == fNeedsBalanceAdjust) {
						mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16DelayRightChannelBalance;
					} else {
						mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16DelayRightChannel;
					}
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16;
				}
			} else {
				debugIOLog("AppleDBDMAAudio::chooseOutputClippingRoutinePtr - Non-supported output bit depth.\n");
			}
		}
	}
}

// [3094574] aml, pick the correct input conversion routine based on our current state
void AppleDBDMAAudio::chooseInputConversionRoutinePtr() 
{
	if (32 == mDBDMAInputFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			if (fNeedsRightChanDelayInput) {// [3173869]
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream32DelayRightWithGain;
			} else {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream32WithGain;
			}
		} else {
			mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream32;
		}
	} else if (16 == mDBDMAInputFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			if (fNeedsRightChanDelayInput) {// [3173869]
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16DelayRightWithGain;
			} else {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16WithGain;
			}
		} else {
			if (e_Mode_CopyRightToLeft == mInputDualMonoMode) {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16CopyR2L;
			} else {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16;
			}
		}
	} else {
		debugIOLog("AppleDBDMAAudio::chooseInputConversionRoutinePtr - Non-supported input bit depth!\n");
	}
}

IOReturn AppleDBDMAAudio::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
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
		result = (*this.*mClipAppleDBDMAToOutputStreamRoutine)(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat);
	}

	endTiming();

	return result;
}

// [3094574] aml, use function pointer instead of if/else block
IOReturn AppleDBDMAAudio::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	IOReturn result;

 	// if the DMA went bad restart it
	if (mNeedToRestartDMA) {
		mNeedToRestartDMA = FALSE;
		restartDMA ();
	}

	result = (*this.*mConvertInputStreamToAppleDBDMARoutine)(sampleBuf, destBuf, firstSampleFrame, numSampleFrames, streamFormat);

	return result;
}

#pragma mark ------------------------ 
#pragma mark еее Output Routines
#pragma mark ------------------------ 

inline void AppleDBDMAAudio::outputProcessing (float* inFloatBufferPtr, UInt32 inNumSamples) {
#ifdef USE_SOFT_DSP
	if (mCurrentEQStructPtr->phaseReverse) {
		invertRightChannel(inFloatBufferPtr, inNumSamples);
	}
		
	if (false == mCurrentEQStructPtr->bypassAll) {
		equalizer (inFloatBufferPtr, inNumSamples, mCurrentEQStructPtr);
	}

	if (false == mCurrentLimiterStructPtr->bypassAll) {
		multibandLimiter (inFloatBufferPtr, inNumSamples, mCurrentCrossoverStructPtr, mCurrentLimiterStructPtr);
	}	
#endif
}

IOReturn AppleDBDMAAudio::clipMemCopyToOutputStream (const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
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
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	outputProcessing(inFloatBufferPtr, numSamples);
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);

	outputProcessing(inFloatBufferPtr, numSamples);
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);

	outputProcessing (inFloatBufferPtr, numSamples);

	balanceAdjust (inFloatBufferPtr, numSamples, mCurrentEQStructPtr);
	
	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, invert phase to correct older iMac hardware
// assumes 2 channels.  Note that there is no 32 bit version of this 
// conversion routine, since the older hardware does not support it.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16InvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;
	
	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	invertRightChannel( inFloatBufferPtr, numSamples );

	outputProcessing (inFloatBufferPtr, numSamples);

	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );
   
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16, mix right and left channels and mute right
// assumes 2 channels
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	outputProcessing (inFloatBufferPtr, numSamples);
	
	mixAndMuteRightChannel( inFloatBufferPtr, numSamples );

	Float32ToNativeInt16( inFloatBufferPtr, outSInt16BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;

	outputProcessing (inFloatBufferPtr, numSamples);

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32DelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);

	outputProcessing (inFloatBufferPtr, numSamples);

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );
	
    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32, delay right channel to correct for TAS 3004
// I2S clocking issue which puts left and right samples out of phase.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32DelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	delayRightChannel( inFloatBufferPtr, numSamples , &mLastOutputSample);

	outputProcessing (inFloatBufferPtr, numSamples);

	balanceAdjust (inFloatBufferPtr, numSamples, mCurrentEQStructPtr);

	Float32ToNativeInt32( inFloatBufferPtr, outSInt32BufferPtr, numSamples );
	
    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32, mix right and left channels and mute right
// assumes 2 channels
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32MixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    float	*inFloatBufferPtr;
	SInt32	*outSInt32BufferPtr;
	UInt32	numSamples;

	numSamples = numSampleFrames*streamFormat->fNumChannels;
    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
	outSInt32BufferPtr = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	
	outputProcessing (inFloatBufferPtr, numSamples);

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
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		maxSampleIndex, numSamples;
    float*		floatMixBuf;
    SInt16*	outputBuf16;
	UInt32		sampleIndex;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;

	outputProcessing (high, numSamples);

	Float32ToNativeInt16( high, outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel( high, numSamples , &mLastOutputSample);

	outputProcessing (high, numSamples);

	Float32ToNativeInt16 (high, outputBuf16, numSamples);

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel( high, numSamples , &mLastOutputSample);

	outputProcessing (high, numSamples);

	balanceAdjust (high, numSamples, mCurrentEQStructPtr);

	Float32ToNativeInt16 (high, outputBuf16, numSamples);

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, invert right channel - assumes 2 channels 
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	invertRightChannel( high, numSamples );

	outputProcessing (high, numSamples);

	Float32ToNativeInt16( high, outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt16 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{

    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt16 *	outputBuf16;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf16 = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
	mixAndMuteRightChannel(  high, numSamples );

	outputProcessing (high, numSamples);

	Float32ToNativeInt16( high, outputBuf16, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, assumes 2 channel data
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSub(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;

	outputProcessing (high, numSamples);

	Float32ToNativeInt32( high, outputBuf32, numSamples );

    // low side
  	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);

	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, delay right channel one sample
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubDelayRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel( high, numSamples , &mLastOutputSample);

	outputProcessing (high, numSamples);

	Float32ToNativeInt32( high, outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, delay right channel one sample
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubDelayRightChannelBalance(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	delayRightChannel( high, numSamples , &mLastOutputSample);

	outputProcessing (high, numSamples);

	balanceAdjust (high, numSamples, mCurrentEQStructPtr);

	Float32ToNativeInt32( high, outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	

	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Float32 to Native SInt32 with iSub, mix and mute right channel
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSubMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex, numSamples;
    float *		floatMixBuf;
    SInt32 *	outputBuf32;

	iSubSynchronize(firstSampleFrame, numSampleFrames);

	PreviousValues* filterState = &(miSubProcessingParams.filterState);
	PreviousValues* filterState2 = &(miSubProcessingParams.filterState2);
//	PreviousValues* phaseCompState = &(miSubProcessingParams.phaseCompState);
	UInt32* loopCount = &(miSubProcessingParams.iSubLoopCount);
	SInt32* iSubBufferOffset = &(miSubProcessingParams.iSubBufferOffset);
	float* srcPhase = &(miSubProcessingParams.srcPhase);
	float* srcState = &(miSubProcessingParams.srcState);

    floatMixBuf = (float *)mixBuf;
	float* low = miSubProcessingParams.lowFreqSamples;
	float* high = &floatMixBuf[firstSampleFrame * streamFormat->fNumChannels];//miSubProcessingParams.highFreqSamples;
	UInt32 sampleRate = miSubProcessingParams.sampleRate;
	UInt32 adaptiveSampleRate = miSubProcessingParams.adaptiveSampleRate;
	SInt16* iSubBufferMemory = miSubProcessingParams.iSubBuffer;
	UInt32 iSubBufferLen = miSubProcessingParams.iSubBufferLen;
	UInt32 outputSampleRate = miSubProcessingParams.iSubFormat.outputSampleRate;

	numSamples = numSampleFrames * streamFormat->fNumChannels;
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
//	high = &high[firstSampleFrame * streamFormat->fNumChannels]; 

    // Filter audio into low and high buffers using a 24 dB/octave crossover
//	StereoCrossover4thOrderPhaseComp (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], high, numSampleFrames, sampleRate, filterState, filterState2, phaseCompState);
	StereoLowPass4thOrder (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState, filterState2);

    // high side 
	outputBuf32 = (SInt32 *)sampleBuf + firstSampleFrame * streamFormat->fNumChannels;
	mixAndMuteRightChannel( high, numSamples );

	outputProcessing (high, numSamples);

	Float32ToNativeInt32( high, outputBuf32, numSamples );

    // low side
 	sampleIndex = (firstSampleFrame * streamFormat->fNumChannels);
	iSubDownSampleLinearAndConvert( low, srcPhase, srcState, adaptiveSampleRate, outputSampleRate, sampleIndex, maxSampleIndex, iSubBufferMemory, iSubBufferOffset, iSubBufferLen, loopCount );	
		
	updateiSubPosition(firstSampleFrame, numSampleFrames);
		
	return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее Input Routines
#pragma mark ------------------------ 

inline void AppleDBDMAAudio::inputProcessing (float* inFloatBufferPtr, UInt32 inNumSamples) {
#ifdef USE_SOFT_DSP

	if (false == mCurrentInputEQStructPtr->bypassAll) {
		equalizer (inFloatBufferPtr, inNumSamples, mCurrentInputEQStructPtr);
	}

#endif
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream16(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
	
    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

   	NativeInt16ToFloat32(inputBuf16, floatDestBuf, numSamplesLeft, 16);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}


// ------------------------------------------------------------------------
// Native SInt16 to Float32 with right channel delay and gain for TAS compensation [3173869]
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream16DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
	
    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

   	NativeInt16ToFloat32(inputBuf16, floatDestBuf, numSamplesLeft, 16);

	delayRightChannel( floatDestBuf, numSamplesLeft , &mLastInputSample);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32, copy the rigth sample to the left channel for
// older machines only.  Note that there is no 32 bit version of this  
// function because older hardware does not support it.
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream16CopyR2L(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
    
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
 
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt16ToFloat32CopyRightToLeft(inputBuf16, floatDestBuf, numSamplesLeft, 16);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt16 to Float32, with software input gain
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream16WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf16;
    
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf16 = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt16ToFloat32Gain(inputBuf16, floatDestBuf, numSamplesLeft, 16, mInputGainLPtr, mInputGainRPtr);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream32(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;

    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt32ToFloat32(inputBuf32, floatDestBuf, numSamplesLeft, 32);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32, with software input gain
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream32WithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;
  
    floatDestBuf = (float *)destBuf;    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);

	NativeInt32ToFloat32Gain(inputBuf32, floatDestBuf, numSamplesLeft, 32, mInputGainLPtr, mInputGainRPtr);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

// ------------------------------------------------------------------------
// Native SInt32 to Float32 with right channel delay and gain  for TAS compensation [3173869]
// ------------------------------------------------------------------------
IOReturn AppleDBDMAAudio::convertAppleDBDMAFromInputStream32DelayRightWithGain(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt32 *inputBuf32;

    floatDestBuf = (float *)destBuf;
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
	inputBuf32 = &(((SInt32 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
   
	NativeInt32ToFloat32Gain(inputBuf32, floatDestBuf, numSamplesLeft, 32, mInputGainLPtr, mInputGainRPtr);

	delayRightChannel( floatDestBuf, numSamplesLeft , &mLastInputSample);

	// [3306305]
	inputProcessing (floatDestBuf, numSamplesLeft); 

    return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее State Routines
#pragma mark ------------------------ 

void AppleDBDMAAudio::resetOutputClipOptions() {
	fNeedsPhaseInversion = false;
	fNeedsRightChanDelay = false;
	fNeedsRightChanMixed = false;
	fNeedsBalanceAdjust = false;
	chooseOutputClippingRoutinePtr();
}

void AppleDBDMAAudio::resetInputClipOptions() {
	mInputDualMonoMode = e_Mode_Disabled;
	fNeedsRightChanDelayInput = false;
}

void AppleDBDMAAudio::setEqualizationFromDictionary (OSDictionary * inDictionary) 
{ 
	OSDictionary *					theFilterDict;
	OSCollectionIterator *			collectionIterator;
	OSNumber *						typeNumber;
	OSNumber *						indexNumber;
	OSSymbol *						theSymbol;
	OSBoolean *						theBoolean;
	OSData *						parameterData;
	EQParamStruct 					eqParams;
	UInt32							typeCode;
	UInt32							index;
	Boolean							runInSoftware;
	UInt32							filtersInSoftwareCount;
	
	debug2IOLog ("+ AppleDBDMAAudio::setEqualizationFromDictionary (%p)\n", inDictionary);

	collectionIterator = OSCollectionIterator::withCollection (inDictionary);
	FailIf (NULL == collectionIterator, Exit);
	
	initializeSoftwareEQ ();
	
	eqParams.type = (FilterType)0;
	eqParams.fc = 0;
	eqParams.Q = 0;
	eqParams.gain = 0;
	
	index = 0;
	filtersInSoftwareCount = 0;

	while (theSymbol = OSDynamicCast (OSSymbol, collectionIterator->getNextObject ())) {
		debug2IOLog ("symbol = %s\n", theSymbol->getCStringNoCopy ());
		theFilterDict = OSDynamicCast (OSDictionary, inDictionary->getObject (theSymbol));
		debug2IOLog ("theFilterDict = %p\n", theFilterDict);

		if (NULL != theFilterDict) {
			typeNumber = OSDynamicCast (OSNumber, theFilterDict->getObject (kFilterType));
			debug2IOLog ("typeNumber = %p\n", typeNumber);
			FailIf (NULL == typeNumber, Exit);
			typeCode = typeNumber->unsigned32BitValue ();
			debug2IOLog ("filter type = %4s\n", (char *)&typeCode);
			eqParams.type = (FilterType)typeCode;

			indexNumber = OSDynamicCast (OSNumber, theFilterDict->getObject (kFilterIndex));
			debug2IOLog ("indexNumber = %p\n", indexNumber);
			FailIf (NULL == indexNumber, Exit);
			index = indexNumber->unsigned32BitValue ();
			debug2IOLog ("filter index = %ld\n", index);
			if (index >= kMaxNumFilters) {
				IOLog ("Filter index too high (%ld) for this layout.\n", index);
				continue;
			}
			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterFrequency));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(eqParams.fc), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterQ));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(eqParams.Q), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterGain));
			if (NULL != parameterData) {
				FailIf (NULL == parameterData, Exit);
				debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
				memcpy (&(eqParams.gain), parameterData->getBytesNoCopy (), 4);
			}

			theBoolean = OSDynamicCast (OSBoolean, theFilterDict->getObject (kFilterRunInSoftware));
			if (NULL != theBoolean) {
				runInSoftware = theBoolean->getValue ();
				debug3IOLog ("runInSoftware[%ld] = %d\n", index, runInSoftware);
				mCurrentEQStructPtr->runInSoftware[index] = runInSoftware;
				if (runInSoftware) {
					filtersInSoftwareCount++;
				}
			} else {
				mCurrentEQStructPtr->runInSoftware[index] = FALSE;
			}
			
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mEQParams[index]), &eqParams, sizeof(EQParamStruct));

			setEQCoefficients (&eqParams, mCurrentEQStructPtr, index, sampleRate.whole);
			
			mCurrentEQStructPtr->bypassFilter[index] = FALSE;

			debug3IOLog ("mCurrentEQStructPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b0[index])));
			debug3IOLog ("mCurrentEQStructPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b1[index])));
			debug3IOLog ("mCurrentEQStructPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b2[index])));
			debug3IOLog ("mCurrentEQStructPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a1[index])));
			debug3IOLog ("mCurrentEQStructPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a2[index])));

		}
	}

	mCurrentEQStructPtr->numSoftwareFilters = filtersInSoftwareCount;
	debug2IOLog ("mCurrentEQStructPtr->numSoftwareFilters = %ld\n", mCurrentEQStructPtr->numSoftwareFilters);
	
	mCurrentEQStructPtr->bypassAll = false;

	// set hardware EQ in non-realtime mode
	(ourProvider->getCurrentOutputPlugin ())->setEQProcessing ((void *)mCurrentEQStructPtr, FALSE);
	
	collectionIterator->release ();

Exit:
	debug2IOLog ("- AppleDBDMAAudio::setEqualizationFromDictionary (%p)\n", inDictionary);
	return;   	
}	

void AppleDBDMAAudio::setInputEqualizationFromDictionary (OSDictionary * inDictionary) 
{ 
	OSDictionary *					theFilterDict;
	OSCollectionIterator *			collectionIterator;
	OSNumber *						typeNumber;
	OSNumber *						indexNumber;
	OSSymbol *						theSymbol;
	OSBoolean *						theBoolean;
	OSData *						parameterData;
	EQParamStruct 					eqParams;
	UInt32							typeCode;
	UInt32							index;
	Boolean							runInSoftware;
	UInt32							filtersInSoftwareCount;
	
	debug2IOLog ("AppleDBDMAAudio::setInputEqualizationFromDictionary (%p)\n", inDictionary);

	collectionIterator = OSCollectionIterator::withCollection (inDictionary);
	FailIf (NULL == collectionIterator, Exit);
	
	initializeSoftwareEQ ();
	
	eqParams.type = (FilterType)0;
	eqParams.fc = 0;
	eqParams.Q = 0;
	eqParams.gain = 0;
	
	index = 0;
	filtersInSoftwareCount = 0;

	while (theSymbol = OSDynamicCast (OSSymbol, collectionIterator->getNextObject ())) {
		debug2IOLog ("symbol = %s\n", theSymbol->getCStringNoCopy ());
		theFilterDict = OSDynamicCast (OSDictionary, inDictionary->getObject (theSymbol));
		debug2IOLog ("theFilterDict = %p\n", theFilterDict);

		if (NULL != theFilterDict) {
			typeNumber = OSDynamicCast (OSNumber, theFilterDict->getObject (kFilterType));
			debug2IOLog ("typeNumber = %p\n", typeNumber);
			FailIf (NULL == typeNumber, Exit);
			typeCode = typeNumber->unsigned32BitValue ();
			debug2IOLog ("filter type = %4s\n", (char *)&typeCode);
			eqParams.type = (FilterType)typeCode;

			indexNumber = OSDynamicCast (OSNumber, theFilterDict->getObject (kFilterIndex));
			debug2IOLog ("indexNumber = %p\n", indexNumber);
			FailIf (NULL == indexNumber, Exit);
			index = indexNumber->unsigned32BitValue ();
			debug2IOLog ("filter index = %ld\n", index);
			if (index >= kMaxNumFilters) {
				debug2IOLog ("Filter index too high (%ld) for this layout.\n", index);
				continue;
			}
			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterFrequency));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(eqParams.fc), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterQ));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(eqParams.Q), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theFilterDict->getObject (kFilterGain));
			if (NULL != parameterData) {
				FailIf (NULL == parameterData, Exit);
				debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
				memcpy (&(eqParams.gain), parameterData->getBytesNoCopy (), 4);
			}

			theBoolean = OSDynamicCast (OSBoolean, theFilterDict->getObject (kFilterRunInSoftware));
			if (NULL != theBoolean) {
				runInSoftware = theBoolean->getValue ();
				debug3IOLog ("runInSoftware[%ld] = %d\n", index, runInSoftware);
				mCurrentInputEQStructPtr->runInSoftware[index] = runInSoftware;
				if (runInSoftware) {
					filtersInSoftwareCount++;
				}
			} else {
				mCurrentInputEQStructPtr->runInSoftware[index] = FALSE;
			}
			
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mInputEQParams[index]), &eqParams, sizeof(EQParamStruct));

			setEQCoefficients (&eqParams, mCurrentInputEQStructPtr, index, sampleRate.whole);
			
			mCurrentInputEQStructPtr->bypassFilter[index] = FALSE;

			debug3IOLog ("mCurrentInputEQStructPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentInputEQStructPtr->b0[index])));
			debug3IOLog ("mCurrentInputEQStructPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentInputEQStructPtr->b1[index])));
			debug3IOLog ("mCurrentInputEQStructPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentInputEQStructPtr->b2[index])));
			debug3IOLog ("mCurrentInputEQStructPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentInputEQStructPtr->a1[index])));
			debug3IOLog ("mCurrentInputEQStructPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentInputEQStructPtr->a2[index])));
			
		}
	}
		
	mCurrentInputEQStructPtr->numSoftwareFilters = filtersInSoftwareCount;
	debug2IOLog ("mCurrentInputEQStructPtr->numSoftwareFilters = %ld\n", mCurrentInputEQStructPtr->numSoftwareFilters);
	
	mCurrentInputEQStructPtr->bypassAll = false;
	
	collectionIterator->release ();

Exit:
	return;   	
}
	
void AppleDBDMAAudio::setLimiterFromDictionary (OSDictionary * inDictionary) 
{ 
	OSDictionary *					theLimiterDict;
	OSCollectionIterator *			collectionIterator;
	OSNumber *						typeNumber;
	OSNumber *						bandIndexNumber;
	OSNumber *						parameterNumber;
	OSSymbol *						theSymbol;
	OSData *						parameterData;
	LimiterParamStruct 				limiterParams;
	UInt32							typeCode;
	UInt32							bandIndex;
	UInt32							count;
	OSBoolean *						theBoolean;
	
	debug2IOLog ("AppleDBDMAAudio::setLimiterFromDictionary (%p)\n", inDictionary);

	collectionIterator = OSCollectionIterator::withCollection (inDictionary);
	FailIf (NULL == collectionIterator, Exit);
	
	initializeSoftwareLimiter ();
	
	limiterParams.type = (LimiterType)0;
	limiterParams.threshold = 0;
	limiterParams.ratio = 0;
	limiterParams.attack = 0;
	limiterParams.release = 0;
	limiterParams.lookahead = 0;
	bandIndex = 0;
	
	mCurrentLimiterStructPtr->bypassAll = true;
	
	count = 0;

	while (theSymbol = OSDynamicCast (OSSymbol, collectionIterator->getNextObject ())) {
		debug2IOLog ("symbol = %s\n", theSymbol->getCStringNoCopy ());
		if (theSymbol->isEqualTo (kCrossover))
			continue;
		theLimiterDict = OSDynamicCast (OSDictionary, inDictionary->getObject (theSymbol));
		debug2IOLog ("theLimiterDict = %p\n", theLimiterDict);

		if (NULL != theLimiterDict) {
			typeNumber = OSDynamicCast (OSNumber, theLimiterDict->getObject (kLimiterType));
			debug2IOLog ("kLimiterType typeNumber = %p\n", typeNumber);
			FailIf (NULL == typeNumber, Exit);
			typeCode = typeNumber->unsigned32BitValue ();
			debug2IOLog ("limiter type = %4s\n", (char *)&typeCode);
			limiterParams.type = (LimiterType)typeCode;

			bandIndexNumber = OSDynamicCast (OSNumber, theLimiterDict->getObject (kLimiterBandIndex));
			debug2IOLog ("kLimiterBandIndex bandIndexNumber = %p\n", bandIndexNumber);
			FailIf (NULL == bandIndexNumber, Exit);
			bandIndex = bandIndexNumber->unsigned32BitValue ();
			debug2IOLog ("band index = %ld\n", bandIndex);
			if (bandIndex > kMaxNumLimiters)
				break;

			parameterData = OSDynamicCast (OSData, theLimiterDict->getObject (kLimiterAttackTime));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("kLimiterAttackTime parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(limiterParams.attack), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theLimiterDict->getObject (kLimiterReleaseTime));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("kLimiterReleaseTime parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(limiterParams.release), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theLimiterDict->getObject (kLimiterThreshold));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("kLimiterThreshold parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(limiterParams.threshold), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theLimiterDict->getObject (kLimiterRatio));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("kLimiterRatio parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(limiterParams.ratio), parameterData->getBytesNoCopy (), 4);

			parameterData = OSDynamicCast (OSData, theLimiterDict->getObject (kLimiterRatioBelow));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("kLimiterRatioBelow parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(limiterParams.ratioBelow), parameterData->getBytesNoCopy (), 4);

			parameterNumber = OSDynamicCast (OSNumber, theLimiterDict->getObject (kLimiterLookahead));
			FailIf (NULL == parameterNumber, Exit);
			debug2IOLog ("kLimiterLookahead parameterNumber = %x\n", parameterNumber->unsigned32BitValue ());
			limiterParams.lookahead = parameterNumber->unsigned32BitValue ();

			theBoolean = OSDynamicCast (OSBoolean, theLimiterDict->getObject (kLimiterRunInHardware));
			if (NULL != theBoolean) {
				limiterParams.runInHardware = theBoolean->getValue ();
			} else {
				limiterParams.runInHardware = FALSE;
			}
			
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mLimiterParams[bandIndex]), &limiterParams, sizeof(LimiterParamStruct));

			setLimiterCoefficients (&limiterParams, mCurrentLimiterStructPtr, bandIndex, sampleRate.whole);

			mCurrentLimiterStructPtr->bypass[bandIndex] = FALSE;

			debug3IOLog ("mCurrentLimiterStructPtr->threshold[%ld] = 0x%lx\n", bandIndex, *((UInt32 *)&(mCurrentLimiterStructPtr->threshold[bandIndex])));
			debug3IOLog ("mCurrentLimiterStructPtr->oneMinusOneOverRatio[%ld] = 0x%lx\n", count, *((UInt32 *)&(mCurrentLimiterStructPtr->oneMinusOneOverRatio[bandIndex])));
			debug3IOLog ("mCurrentLimiterStructPtr->attackTc[%ld] = 0x%lx\n", bandIndex, *((UInt32 *)&(mCurrentLimiterStructPtr->attackTc[bandIndex])));
			debug3IOLog ("mCurrentLimiterStructPtr->releaseTc[%ld] = 0x%lx\n", bandIndex, *((UInt32 *)&(mCurrentLimiterStructPtr->releaseTc[bandIndex])));
			debug3IOLog ("mCurrentLimiterStructPtr->lookahead[%ld] = 0x%lx\n", bandIndex, *((UInt32 *)&(mCurrentLimiterStructPtr->lookahead[bandIndex])));
			debug3IOLog ("mCurrentLimiterStructPtr->type[%ld] = 0x%x\n", bandIndex, mCurrentLimiterStructPtr->type[bandIndex]);
			
			count++;

		}
	}

	mCurrentLimiterStructPtr->numLimiters = count;
	debug2IOLog ("mCurrentLimiterStructPtr->numLimiters = %ld\n", mCurrentLimiterStructPtr->numLimiters);

	mCurrentLimiterStructPtr->bypassAll = false;

	if ( mLimiterParams[kHardwareLimiterIndex].runInHardware ) {
		(ourProvider->getCurrentOutputPlugin ())->setDRCProcessing ((void*)&mLimiterParams[kHardwareLimiterIndex], FALSE);
	} else {
		(ourProvider->getCurrentOutputPlugin ())->setDRCProcessing ((void*)NULL, FALSE);
	}

	collectionIterator->release ();

Exit:
	return;   	
}

void AppleDBDMAAudio::setCrossoverFromDictionary (OSDictionary * inDictionary) 
{ 
	OSNumber *							numBandsNumber;
	OSData *							parameterData;
	CrossoverParamStruct 				crossoverParams;
	UInt32								index;
	UInt32								numBands;
	char								frequencyCString[32];

	debug2IOLog ("AppleDBDMAAudio::setCrossoverFromDictionary (%p)\n", inDictionary);

	initializeSoftwareCrossover ();
	
	crossoverParams.numBands = 0;
	crossoverParams.phaseReverseHigh = 0;
	crossoverParams.frequency[0] = 0;
	crossoverParams.frequency[1] = 0;
	
	if (NULL != inDictionary) {
		numBandsNumber = OSDynamicCast (OSNumber, inDictionary->getObject (kCrossoverNumberOfBands));
		debug2IOLog ("numBandsNumber = %p\n", numBandsNumber);
		FailIf (NULL == numBandsNumber, Exit);
		numBands = numBandsNumber->unsigned32BitValue ();
		debug2IOLog ("number of bands = 0x%lx\n", numBands);
		FailIf (kMaxNumCrossoverBands > numBands, Exit);
		crossoverParams.numBands = numBands;

		for (index = 0; index < (crossoverParams.numBands - 1); index++) {
			sprintf (frequencyCString, "%s%ld", kCrossoverFrequency, index); 
			debug2IOLog("frequency string = %s\n", frequencyCString);
			parameterData = OSDynamicCast (OSData, inDictionary->getObject (frequencyCString));
			FailIf (NULL == parameterData, Exit);
			debug2IOLog ("parameterData = %lx\n", *(UInt32 *)(parameterData->getBytesNoCopy ()));
			memcpy (&(crossoverParams.frequency[index]), parameterData->getBytesNoCopy (), 4);
		}

		// FIX: only use member structure, and not this local variable once this works...
		memcpy (&mCrossoverParams, &crossoverParams, sizeof(CrossoverParamStruct));

		setCrossoverCoefficients (&crossoverParams, mCurrentCrossoverStructPtr, sampleRate.whole);

		debug2IOLog ("mCurrentCrossoverStructPtr->numBands = 0x%lx\n", mCurrentCrossoverStructPtr->numBands);
		debug2IOLog ("mCurrentCrossoverStructPtr->c1_1st[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c1_1st[0])));
		debug2IOLog ("mCurrentCrossoverStructPtr->c1_2nd[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c1_2nd[0])));
		debug2IOLog ("mCurrentCrossoverStructPtr->c2_2nd[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c2_2nd[0])));
		debug2IOLog ("mCurrentCrossoverStructPtr->outBufferPtr[0] = %p\n", mCurrentCrossoverStructPtr->outBufferPtr[0]);
		debug2IOLog ("mCurrentCrossoverStructPtr->outBufferPtr[1] = %p\n", mCurrentCrossoverStructPtr->outBufferPtr[1]);
			
	}
#if 0	
	if (mCurrentLimiterStructPtr->numLimiters != crossoverParams.numBands) {
		IOLog ("AppleDBDMAAudio: bad info plist - mismatched number of limiters (%ld) for specified crossover (%ld bands).\n", mCurrentLimiterStructPtr->numLimiters, crossoverParams.numBands);
		crossoverParams.numBands = 0;
	}
#endif
Exit:
	return;   	
}

void AppleDBDMAAudio::initializeSoftwareEQ (void) 
{ 
	UInt32 					index;
	debugIOLog ("AppleDBDMAAudio::initializeSoftwareEQ\n");

	bzero (mCurrentEQStructPtr, sizeof (EQStruct));
	bzero (&mEQParams, sizeof (EQParamStruct));
	
	mCurrentEQStructPtr->bypassAll = true;
	for (index = 0; index < kMaxNumFilters; index++) {
		*(UInt32*)(&mCurrentEQStructPtr->b0[index]) = 0x3F800000;
	}

	*(UInt32*)(&mCurrentEQStructPtr->leftSoftVolume) = 0x3F800000;
	*(UInt32*)(&mCurrentEQStructPtr->rightSoftVolume) = 0x3F800000;

	resetEQ (mCurrentEQStructPtr);

	// [3306305]
	bzero (mCurrentInputEQStructPtr, sizeof (EQStruct));
	bzero (&mInputEQParams, sizeof (EQParamStruct));
	
	mCurrentInputEQStructPtr->bypassAll = true;
	for (index = 0; index < kMaxNumFilters; index++) {
		*(UInt32*)(&mCurrentInputEQStructPtr->b0[index]) = 0x3F800000;
	}

	*(UInt32*)(&mCurrentInputEQStructPtr->leftSoftVolume) = 0x3F800000;
	*(UInt32*)(&mCurrentInputEQStructPtr->rightSoftVolume) = 0x3F800000;

	resetEQ (mCurrentInputEQStructPtr);

	return;   	
}

void AppleDBDMAAudio::initializeSoftwareLimiter (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::initializeSoftwareLimiter\n");

	bzero (mCurrentLimiterStructPtr, sizeof (LimiterStruct));
	bzero (&mLimiterParams, sizeof (LimiterParamStruct));

	for (index = 0; index < kMaxNumLimiters; index++) {
		*(UInt32*)(&mCurrentLimiterStructPtr->gain) = 0x3F800000;
	}
	
	resetLimiter (mCurrentLimiterStructPtr);

	return;   	
}

void AppleDBDMAAudio::initializeSoftwareCrossover (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::initializeSoftwareCrossover\n");

	for (index = 0; index < kMaxNumCrossoverBands; index++) {
		mCurrentCrossoverStructPtr->c1_1st[index] = 0;
		mCurrentCrossoverStructPtr->c1_2nd[index] = 0;
		mCurrentCrossoverStructPtr->c2_2nd[index] = 0;
	}
	mCurrentCrossoverStructPtr->numBands = 0;
	mCurrentCrossoverStructPtr->phaseReverseHigh = 0;

	bzero (&mCrossoverParams, sizeof (CrossoverParamStruct));

	resetCrossover (mCurrentCrossoverStructPtr);

	return;   	
}

void AppleDBDMAAudio::disableSoftwareEQ (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::disableSoftwareEQ\n");

//	mCurrentEQStructPtr->bypassAll = true;
	mEQStructA.bypassAll = true;
	mEQStructB.bypassAll = true;
	
	return;   	
}

// [3306305]
void AppleDBDMAAudio::disableSoftwareInputEQ (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::disableSoftwareInputEQ\n");

	mInputEQStructA.bypassAll = true;
	mInputEQStructB.bypassAll = true;
	
	return;   	
}

void AppleDBDMAAudio::disableSoftwareLimiter() 
{ 
	debugIOLog ("AppleDBDMAAudio::disableSoftwareLimiter\n");

	mCurrentLimiterStructPtr->bypassAll = true;

	return;   	
}

void AppleDBDMAAudio::enableSoftwareEQ (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::enableSoftwareEQ\n");

//	mCurrentEQStructPtr->bypassAll = false;
	mEQStructA.bypassAll = false;
	mEQStructB.bypassAll = false;
	
	return;   	
}

// [3306305]
void AppleDBDMAAudio::enableSoftwareInputEQ (void) 
{ 
	debugIOLog ("AppleDBDMAAudio::enableSoftwareInputEQ\n");
	
	mInputEQStructA.bypassAll = false;
	mInputEQStructB.bypassAll = false;
	
	return;   	
}

void AppleDBDMAAudio::enableSoftwareLimiter() 
{ 
	debugIOLog ("AppleDBDMAAudio::enableSoftwareLimiter\n");

	mCurrentLimiterStructPtr->bypassAll = false;

	return;   	
}
	
void AppleDBDMAAudio::setDualMonoMode(const DualMonoModeType inDualMonoMode) 
{ 
	mInputDualMonoMode = inDualMonoMode; 
	chooseInputConversionRoutinePtr();

	return;   	
}

void AppleDBDMAAudio::setInputGainL(UInt32 inGainL) 
{ 
    if (mInputGainLPtr == NULL) {        
        mInputGainLPtr = (float *)IOMalloc(sizeof(float));
    }
    inputGainConverter(inGainL, mInputGainLPtr);
	
    return;   
} 

void AppleDBDMAAudio::setInputGainR(UInt32 inGainR) 
{ 
    if (mInputGainRPtr == NULL) {        
        mInputGainRPtr = (float *)IOMalloc(sizeof(float));
    }
    inputGainConverter(inGainR, mInputGainRPtr);

    return;   
} 

void AppleDBDMAAudio::setLeftSoftVolume(float * inVolume) 
{
	dB2linear (inVolume, &(mCurrentEQStructPtr->leftSoftVolume));
    return;   
} 

void AppleDBDMAAudio::setRightSoftVolume(float * inVolume) 
{ 
	dB2linear (inVolume, &(mCurrentEQStructPtr->rightSoftVolume));
    return;   
} 

// [3094574] aml, updated routines below to set the proper clipping routine

void AppleDBDMAAudio::setPhaseInversion(const bool needsPhaseInversion) 
{
	fNeedsPhaseInversion = needsPhaseInversion; 
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setRightChanDelay(const bool needsRightChanDelay)  
{
	fNeedsRightChanDelay = needsRightChanDelay;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setBalanceAdjust(const bool needsBalanceAdjust)  
{
	fNeedsBalanceAdjust = needsBalanceAdjust;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setRightChanMixed(const bool needsRightChanMixed)  
{
	fNeedsRightChanMixed = needsRightChanMixed;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setUseSoftwareInputGain(const bool inUseSoftwareInputGain) 
{     
	debug2IOLog ("setUseSoftwareInputGain (%s)\n", inUseSoftwareInputGain ? "true" : "false");
	
	mUseSoftwareInputGain = inUseSoftwareInputGain;     	
	chooseInputConversionRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setRightChanDelayInput(const bool needsRightChanDelay)  // [3173869]
{
	// Don't call this because it messes up the input stream if two or more applications are recording at once.  [3398910]
/*
	debug2IOLog ("setRightChanDelayInput (%s)\n", needsRightChanDelay ? "true" : "false");
	fNeedsRightChanDelayInput = needsRightChanDelay;
	chooseInputConversionRoutinePtr();
*/	
	return;   
}

#pragma mark ------------------------ 
#pragma mark еее USER CLIENT SUPPORT
#pragma mark ------------------------ 

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copyDMAStateAndFormat (DBDMAUserClientStructPtr outState) {
	
	outState->dmaRunState = dmaRunState;
	outState->numChannels = mDBDMAOutputFormat.fNumChannels;
	outState->sampleFormat = mDBDMAOutputFormat.fSampleFormat;
	outState->numericRepresentation = mDBDMAOutputFormat.fNumericRepresentation;
	outState->bitDepth = mDBDMAOutputFormat.fBitDepth;
	outState->bitWidth = mDBDMAOutputFormat.fBitWidth;
	outState->alignment = mDBDMAOutputFormat.fAlignment;
	outState->byteOrder = mDBDMAOutputFormat.fByteOrder;
	outState->isMixable = mDBDMAOutputFormat.fIsMixable;
	outState->driverTag = mDBDMAOutputFormat.fDriverTag;
	outState->needsPhaseInversion = fNeedsPhaseInversion;
	outState->needsRightChanMixed = fNeedsRightChanMixed;
	outState->needsRightChanDelay = fNeedsRightChanDelay;
	outState->needsBalanceAdjust = fNeedsBalanceAdjust;
	outState->inputDualMonoMode = mInputDualMonoMode;
	outState->useSoftwareInputGain = mUseSoftwareInputGain;
	//	[3305011]	begin {
	outState->dmaInterruptCount = mDmaInterruptCount;
	outState->dmaFrozenInterruptCount = mNumberOfFrozenDmaInterruptCounts;
	outState->dmaRecoveryInProcess = mDmaRecoveryInProcess;
	//	} end	[3305011]
	
	return kIOReturnSuccess;
}


//	[3305011]	begin {
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copyInputChannelCommands ( void * inputChannelCommands ) {
	IOReturn						result;
	UInt32							indexLimit;
	UCIODBDMAChannelCommandsPtr		ptr;
	
	result = kIOReturnError;
	if ( NULL != inputChannelCommands ) {
		if ( NULL != dmaCommandBufferIn ) {
			//	Limit size of transfer to user client buffer size
			ptr = (UCIODBDMAChannelCommandsPtr)inputChannelCommands;
			ptr->numBlocks = numBlocks;
			indexLimit = ( kUserClientStateStructSize - sizeof ( UInt32 ) ) / sizeof ( IODBDMADescriptor );
			if ( numBlocks < indexLimit ) { indexLimit = numBlocks; }
			for ( UInt32 index = 0; index < indexLimit; index++ ) {
				ptr->channelCommands[index].operation = dmaCommandBufferIn[index].operation;
				ptr->channelCommands[index].address = dmaCommandBufferIn[index].address;
				ptr->channelCommands[index].cmdDep = dmaCommandBufferIn[index].cmdDep;
				ptr->channelCommands[index].result = dmaCommandBufferIn[index].result;
			}
			result = kIOReturnSuccess;
		}
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copyOutputChannelCommands ( void * outputChannelCommands ) {
	IOReturn			result;
	UInt32				indexLimit;
	UCIODBDMAChannelCommandsPtr		ptr;
	
	result = kIOReturnError;
	if ( NULL != outputChannelCommands ) {
		if ( NULL != dmaCommandBufferOut ) {
			//	Limit size of transfer to user client buffer size
			ptr = (UCIODBDMAChannelCommandsPtr)outputChannelCommands;
			ptr->numBlocks = numBlocks;
			indexLimit = ( kUserClientStateStructSize - sizeof ( UInt32 ) ) / sizeof ( IODBDMADescriptor );
			if ( numBlocks < indexLimit ) { indexLimit = numBlocks; }
			for ( UInt32 index = 0; index < indexLimit; index++ ) {
				ptr->channelCommands[index].operation = dmaCommandBufferOut[index].operation;
				ptr->channelCommands[index].address = dmaCommandBufferOut[index].address;
				ptr->channelCommands[index].cmdDep = dmaCommandBufferOut[index].cmdDep;
				ptr->channelCommands[index].result = dmaCommandBufferOut[index].result;
			}
			result = kIOReturnSuccess;
		}
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copyInputChannelRegisters (void * outState) {
	IOReturn			result;
	
	result = kIOReturnError;
	if ( NULL != outState ) {
		if ( NULL != ioBaseDMAInput ) {
			((IODBDMAChannelRegisters*)outState)->channelControl = OSReadLittleInt32( &ioBaseDMAInput->channelControl, 0 );
			((IODBDMAChannelRegisters*)outState)->channelStatus = OSReadLittleInt32( &ioBaseDMAInput->channelStatus, 0 );
			((IODBDMAChannelRegisters*)outState)->commandPtrHi = OSReadLittleInt32( &ioBaseDMAInput->commandPtrHi, 0 );
			((IODBDMAChannelRegisters*)outState)->commandPtrLo = OSReadLittleInt32( &ioBaseDMAInput->commandPtrLo, 0 );
			((IODBDMAChannelRegisters*)outState)->interruptSelect = OSReadLittleInt32( &ioBaseDMAInput->interruptSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->branchSelect = OSReadLittleInt32( &ioBaseDMAInput->branchSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->waitSelect = OSReadLittleInt32( &ioBaseDMAInput->waitSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->transferModes = OSReadLittleInt32( &ioBaseDMAInput->transferModes, 0 );
			((IODBDMAChannelRegisters*)outState)->data2PtrHi = OSReadLittleInt32( &ioBaseDMAInput->data2PtrHi, 0 );
			((IODBDMAChannelRegisters*)outState)->data2PtrLo = OSReadLittleInt32( &ioBaseDMAInput->data2PtrLo, 0 );
			((IODBDMAChannelRegisters*)outState)->addressHi = OSReadLittleInt32( &ioBaseDMAInput->addressHi, 0 );
			result = kIOReturnSuccess;
		}
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copyOutputChannelRegisters (void * outState) {
	IOReturn			result;
	
	result = kIOReturnError;
	if ( NULL != outState ) {
		if ( NULL != ioBaseDMAOutput ) {
			((IODBDMAChannelRegisters*)outState)->channelControl = OSReadLittleInt32( &ioBaseDMAOutput->channelControl, 0 );
			((IODBDMAChannelRegisters*)outState)->channelStatus = OSReadLittleInt32( &ioBaseDMAOutput->channelStatus, 0 );
			((IODBDMAChannelRegisters*)outState)->commandPtrHi = OSReadLittleInt32( &ioBaseDMAOutput->commandPtrHi, 0 );
			((IODBDMAChannelRegisters*)outState)->commandPtrLo = OSReadLittleInt32( &ioBaseDMAOutput->commandPtrLo, 0 );
			((IODBDMAChannelRegisters*)outState)->interruptSelect = OSReadLittleInt32( &ioBaseDMAOutput->interruptSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->branchSelect = OSReadLittleInt32( &ioBaseDMAOutput->branchSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->waitSelect = OSReadLittleInt32( &ioBaseDMAOutput->waitSelect, 0 );
			((IODBDMAChannelRegisters*)outState)->transferModes = OSReadLittleInt32( &ioBaseDMAOutput->transferModes, 0 );
			((IODBDMAChannelRegisters*)outState)->data2PtrHi = OSReadLittleInt32( &ioBaseDMAOutput->data2PtrHi, 0 );
			((IODBDMAChannelRegisters*)outState)->data2PtrLo = OSReadLittleInt32( &ioBaseDMAOutput->data2PtrLo, 0 );
			((IODBDMAChannelRegisters*)outState)->addressHi = OSReadLittleInt32( &ioBaseDMAOutput->addressHi, 0 );
			result = kIOReturnSuccess;
		}
	}
	return result;
}
//	} end	[3305011]


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleDBDMAAudio::copySoftwareProcessingState (GetSoftProcUserClientStructPtr outState) {
	UInt32 				index;
		
	for (index = 0; index < kMaxNumFilters; index++) {
		outState->b0[index] = mCurrentEQStructPtr->b0[index];
		outState->b1[index] = mCurrentEQStructPtr->b1[index];
		outState->b2[index] = mCurrentEQStructPtr->b2[index];
		outState->a1[index] = mCurrentEQStructPtr->a1[index];
		outState->a2[index] = mCurrentEQStructPtr->a2[index];
		outState->bypassFilter[index] = mCurrentEQStructPtr->bypassFilter[index];	
	}
	outState->bypassAllFilters = mCurrentEQStructPtr->bypassAll;
	 
	for (index = 0; index < kMaxNumLimiters; index++) {
		outState->type[index] = mCurrentLimiterStructPtr->type[index];
		outState->threshold[index] = mCurrentLimiterStructPtr->threshold[index];
		outState->attackTc[index] = mCurrentLimiterStructPtr->attackTc[index];
		outState->releaseTc[index] = mCurrentLimiterStructPtr->releaseTc[index];
		outState->bypassLimiter[index] = mCurrentLimiterStructPtr->bypass[index];
		outState->lookahead[index] = mCurrentLimiterStructPtr->lookahead[index];	
	}
	outState->bypassAllLimiters = mCurrentLimiterStructPtr->bypassAll;
	outState->numLimiters = mCurrentLimiterStructPtr->numLimiters;

	for (index = 0; index < kMaxNumCrossoverBands; index++) {
		outState->c1_1st[index] = mCurrentCrossoverStructPtr->c1_1st[index];
		outState->c1_2nd[index] = mCurrentCrossoverStructPtr->c1_2nd[index];
		outState->c2_2nd[index] = mCurrentCrossoverStructPtr->c2_2nd[index];
	}
	outState->numBands = mCurrentCrossoverStructPtr->numBands;

	// add input EQ here

	return kIOReturnSuccess;
}


IOReturn AppleDBDMAAudio::applySoftwareProcessingState (SetSoftProcUserClientStructPtr inState) {
	EQParamStruct 					eqParams;
	LimiterParamStruct 				limiterParams;
	CrossoverParamStruct			crossoverParams;
	EQStructPtr 					secondaryEQPtr;
	EQStructPtr 					secondaryInputEQPtr;
	LimiterStructPtr				secondaryLimiterPtr;
	CrossoverStructPtr				secondaryCrossoverPtr;
	UInt32							index;
	UInt32							filtersInSoftwareCount;
	
	#ifdef LOG_SOFT_DSP
	IOLog ("AppleDBDMAAudio::applySoftwareProcessingState (0x%p)\n", inState);
	#endif
	
	if (FALSE == inState->processInput) {
		// Set state on structure currently not in use, then switch when finished
		if (mCurrentEQStructPtr == &mEQStructA) {
			secondaryEQPtr = &mEQStructB;
			memcpy (&mEQStructB, &mEQStructA, sizeof (EQStruct));
		} else {
			secondaryEQPtr = &mEQStructA;
			memcpy (&mEQStructA, &mEQStructB, sizeof (EQStruct));
		}
		if (mCurrentLimiterStructPtr == &mLimiterStructA) {
			secondaryLimiterPtr = &mLimiterStructB;
			memcpy (&mLimiterStructB, &mLimiterStructA, sizeof (LimiterStruct));
		} else {
			secondaryLimiterPtr = &mLimiterStructA;
			memcpy (&mLimiterStructA, &mLimiterStructB, sizeof (LimiterStruct));
		}
		if (mCurrentCrossoverStructPtr == &mCrossoverStructA) {
			secondaryCrossoverPtr = &mCrossoverStructB;
			memcpy (&mCrossoverStructB, &mCrossoverStructA, sizeof (CrossoverStruct));
		} else {
			secondaryCrossoverPtr = &mCrossoverStructA;
			memcpy (&mCrossoverStructA, &mCrossoverStructB, sizeof (CrossoverStruct));
		}

		filtersInSoftwareCount = 0;

		secondaryEQPtr->bypassAll = inState->bypassAllFilters;
		#ifdef LOG_SOFT_DSP
		IOLog ("secondaryEQPtr->bypassAll = %d\n", secondaryEQPtr->bypassAll);
		#endif
		for (index = 0; index < kMaxNumFilters; index++) {
			secondaryEQPtr->bypassFilter[index] = inState->bypassFilter[index];
			secondaryEQPtr->runInSoftware[index] = inState->runInSoftware[index];
			if (secondaryEQPtr->runInSoftware[index]) {
				filtersInSoftwareCount++;
			}
			eqParams.type = (FilterType)inState->filterType[index];
			eqParams.fc = inState->fc[index];
			eqParams.Q = inState->Q[index];
			eqParams.gain = inState->gain[index];
			#ifdef LOG_SOFT_DSP
			IOLog ("secondaryEQPtr->runInSoftware[%ld] = %d\n", index, secondaryEQPtr->runInSoftware[index]);
			IOLog ("secondaryEQPtr->bypassFilter[%ld] = %d\n", index, secondaryEQPtr->bypassFilter[index]);
			IOLog ("eqParams.type = %4s\n", (char *)&(eqParams.type));
			IOLog ("eqParams.fc = 0x%lX\n", *(UInt32 *)&(eqParams.fc));
			IOLog ("eqParams.Q = 0x%lX\n", *(UInt32 *)&(eqParams.Q));
			IOLog ("eqParams.gain = 0x%lX\n", *(UInt32 *)&(eqParams.gain));
			#endif
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mEQParams[index]), &eqParams, sizeof(EQParamStruct));
			setEQCoefficients (&eqParams, secondaryEQPtr, index, sampleRate.whole);
			#ifdef LOG_SOFT_DSP
			IOLog ("secondaryEQPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b0[index])));
			IOLog ("secondaryEQPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b1[index])));
			IOLog ("secondaryEQPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b2[index])));
			IOLog ("secondaryEQPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->a1[index])));
			IOLog ("secondaryEQPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->a2[index])));				
			#endif
		}
		secondaryEQPtr->numSoftwareFilters = filtersInSoftwareCount;
		secondaryEQPtr->phaseReverse = inState->phaseReverse;
		secondaryEQPtr->leftSoftVolume = inState->leftSoftVolume;
		secondaryEQPtr->rightSoftVolume = inState->rightSoftVolume;
	
		secondaryLimiterPtr->bypassAll = inState->bypassAllLimiters;
		#ifdef LOG_SOFT_DSP
		IOLog ("secondaryLimiterPtr->bypassAll = %d\n", secondaryLimiterPtr->bypassAll);
		#endif
		for (index = 0; index < kMaxNumLimiters; index++) {
			secondaryLimiterPtr->bypass[index] = inState->bypassLimiter[index];
			limiterParams.type = (LimiterType)inState->limiterType[index];
			limiterParams.threshold = inState->threshold[index];
			limiterParams.gain = inState->limitergain[index];
			limiterParams.ratio = inState->ratio[index];
			limiterParams.attack = inState->attack[index];
			limiterParams.release = inState->release[index];
			limiterParams.lookahead = inState->lookahead[index];
			#ifdef LOG_SOFT_DSP
			IOLog ("limiterParams.type = %4s\n", (char *)&(limiterParams.type));
			IOLog ("limiterParams.threshold = 0x%lX\n", *(UInt32 *)&(limiterParams.threshold));
			IOLog ("limiterParams.ratio = 0x%lX\n", *(UInt32 *)&(limiterParams.ratio));
			IOLog ("limiterParams.attack = 0x%lX\n", *(UInt32 *)&(limiterParams.attack));
			IOLog ("limiterParams.release = 0x%lX\n", *(UInt32 *)&(limiterParams.release));
			IOLog ("limiterParams.lookahead = 0x%lX\n", *(UInt32 *)&(limiterParams.lookahead));
			IOLog ("inState->bandIndex[%ld] = 0x%lX\n", index, *(UInt32 *)&(inState->bandIndex[index]));
			#endif
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mLimiterParams[index]), &limiterParams, sizeof(LimiterParamStruct));
			setLimiterCoefficients (&limiterParams, secondaryLimiterPtr, inState->bandIndex[index], sampleRate.whole);
			#ifdef LOG_SOFT_DSP
			IOLog ("secondaryLimiterPtr->threshold[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryLimiterPtr->threshold[index])));
			IOLog ("secondaryLimiterPtr->oneMinusOneOverRatio[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryLimiterPtr->oneMinusOneOverRatio[index])));
			IOLog ("secondaryLimiterPtr->attackTc[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryLimiterPtr->attackTc[index])));
			IOLog ("secondaryLimiterPtr->releaseTc[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryLimiterPtr->releaseTc[index])));
			#endif
		}
	
		crossoverParams.numBands = inState->numBands;
		crossoverParams.phaseReverseHigh = inState->phaseReverseHigh;
		#ifdef LOG_SOFT_DSP
		IOLog ("crossoverParams.numBands = %ld\n", crossoverParams.numBands);	
		IOLog ("crossoverParams.phaseReverseHigh = %ld\n", crossoverParams.phaseReverseHigh);	
		#endif
		for (index = 0; index < crossoverParams.numBands - 1; index++) {
			crossoverParams.frequency[index] = inState->frequency[index];
			crossoverParams.delay[index] = inState->delay[index];
			#ifdef LOG_SOFT_DSP
			IOLog ("crossoverParams.frequency[%ld] = %ld\n", index, *((UInt32 *)&(crossoverParams.frequency[index])));	
			#endif
		}
		// FIX: only use member structure, and not this local variable once this works...
		memcpy (&mCrossoverParams, &crossoverParams, sizeof(CrossoverParamStruct));
		setCrossoverCoefficients (&crossoverParams, secondaryCrossoverPtr, sampleRate.whole);
		#ifdef LOG_SOFT_DSP
		IOLog ("secondaryCrossoverPtr->c1_1st[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryCrossoverPtr->c1_1st[index])));
		IOLog ("secondaryCrossoverPtr->c1_2nd[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryCrossoverPtr->c1_2nd[index])));
		IOLog ("secondaryCrossoverPtr->c2_2nd[[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryCrossoverPtr->c2_2nd[[index])));
		#endif
		// Update processing state pointer after all state is updated
		mCurrentEQStructPtr = secondaryEQPtr;
		mCurrentLimiterStructPtr = secondaryLimiterPtr;
		mCurrentCrossoverStructPtr = secondaryCrossoverPtr;
	
		// Send latest state to current output hardware in realtime mode
		(ourProvider->getCurrentOutputPlugin ())->setEQProcessing ((void *)secondaryEQPtr, TRUE);
		
	} else {
		debugIOLog ("inputProcessing enabled.\n");
		// [3306305]
		if (mCurrentInputEQStructPtr == &mInputEQStructA) {
			secondaryInputEQPtr = &mInputEQStructB;
			memcpy (&mInputEQStructB, &mInputEQStructA, sizeof (EQStruct));
		} else {
			secondaryInputEQPtr = &mInputEQStructA;
			memcpy (&mInputEQStructA, &mInputEQStructB, sizeof (EQStruct));
		}
		secondaryInputEQPtr->bypassAll = inState->bypassAllFilters;

		for (index = 0; index < kMaxNumFilters; index++) {
			secondaryInputEQPtr->bypassFilter[index] = inState->bypassFilter[index];
			secondaryInputEQPtr->runInSoftware[index] = true;
	
			eqParams.type = (FilterType)inState->filterType[index];
			eqParams.fc = inState->fc[index];
			eqParams.Q = inState->Q[index];
			eqParams.gain = inState->gain[index];
			#ifdef LOG_SOFT_DSP
			IOLog ("secondaryInputEQPtr->runInSoftware[%ld] = %d\n", index, secondaryEQPtr->runInSoftware[index]);
			IOLog ("secondaryInputEQPtr->bypassFilter[%ld] = %d\n", index, secondaryEQPtr->bypassFilter[index]);
			IOLog ("eqParams.type = %4s\n", (char *)&(eqParams.type));
			IOLog ("eqParams.fc = 0x%lX\n", *(UInt32 *)&(eqParams.fc));
			IOLog ("eqParams.Q = 0x%lX\n", *(UInt32 *)&(eqParams.Q));
			IOLog ("eqParams.gain = 0x%lX\n", *(UInt32 *)&(eqParams.gain));
			#endif
			// FIX: only use member structure, and not this local variable once this works...
			memcpy (&(mInputEQParams[index]), &eqParams, sizeof(EQParamStruct));
			setEQCoefficients (&eqParams, secondaryInputEQPtr, index, sampleRate.whole);
			#ifdef LOG_SOFT_DSP
			IOLog ("secondaryInputEQPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b0[index])));
			IOLog ("secondaryInputEQPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b1[index])));
			IOLog ("secondaryInputEQPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->b2[index])));
			IOLog ("secondaryInputEQPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->a1[index])));
			IOLog ("secondaryInputEQPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(secondaryEQPtr->a2[index])));				
			#endif
		}
		secondaryInputEQPtr->numSoftwareFilters = kMaxNumFilters;
		secondaryInputEQPtr->phaseReverse = inState->phaseReverse;
		secondaryInputEQPtr->leftSoftVolume = inState->leftSoftVolume;
		secondaryInputEQPtr->rightSoftVolume = inState->rightSoftVolume;
		mCurrentInputEQStructPtr = secondaryInputEQPtr;
	}

	return kIOReturnSuccess;
}

#pragma mark ------------------------ 
#pragma mark еее Format Routines
#pragma mark ------------------------ 

bool AppleDBDMAAudio::getDmaState (void )
{
	return dmaRunState;
}

IOReturn AppleDBDMAAudio::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
	IOReturn					result;
	
	result = kIOReturnError;
	
	debug4IOLog ("AppleDBDMAAudio::performFormatChange (%p, %p, %p)\n", audioStream, newFormat, newSampleRate);	
	
	if ( NULL != newFormat ) {	
		debug2IOLog ("user commands us to switch to format '%4s'\n", (char *)&newFormat->fSampleFormat);

		if (audioStream == mOutputStream)
			mDBDMAOutputFormat.fSampleFormat = newFormat->fSampleFormat;

		mDBDMAOutputFormat.fNumChannels = newFormat->fNumChannels;
		mDBDMAOutputFormat.fNumericRepresentation = newFormat->fNumericRepresentation;
		mDBDMAOutputFormat.fBitDepth = newFormat->fBitDepth;
		mDBDMAOutputFormat.fAlignment = newFormat->fAlignment;
		mDBDMAOutputFormat.fByteOrder = newFormat->fByteOrder;
		mDBDMAOutputFormat.fIsMixable = newFormat->fIsMixable;
		mDBDMAOutputFormat.fDriverTag = newFormat->fDriverTag;

		mDBDMAInputFormat.fNumChannels = newFormat->fNumChannels;
		mDBDMAInputFormat.fNumericRepresentation = newFormat->fNumericRepresentation;
		mDBDMAInputFormat.fBitDepth = newFormat->fBitDepth;
		mDBDMAInputFormat.fAlignment = newFormat->fAlignment;
		mDBDMAInputFormat.fByteOrder = newFormat->fByteOrder;
		mDBDMAInputFormat.fDriverTag = newFormat->fDriverTag;

		// Always keep the input and output bit depths the same
		if (mDBDMAOutputFormat.fBitWidth != newFormat->fBitWidth) {
			blockSize = ( DBDMAAUDIODMAENGINE_ROOT_BLOCK_SIZE * ( newFormat->fBitWidth / 8 ) );

			mDBDMAOutputFormat.fBitWidth = newFormat->fBitWidth;
			mDBDMAInputFormat.fBitWidth = newFormat->fBitWidth;

			// If you switch to something other than 16 bit, then you can't do AC-3 for output.
			if (mDBDMAOutputFormat.fBitWidth != 16) {
				mDBDMAOutputFormat.fSampleFormat = kIOAudioStreamSampleFormatLinearPCM;
			}

			deallocateDMAMemory ();
			allocateDMABuffers ();

			FailIf (NULL == mOutputSampleBuffer, Exit);
			if (ioBaseDMAInput) 
				FailIf (NULL == mInputSampleBuffer, Exit);

			mOutputStream->setSampleBuffer ((void *)mOutputSampleBuffer, numBlocks * blockSize);
			if (mInputStream) {
				mInputStream->setSampleBuffer ((void *)mInputSampleBuffer, numBlocks * blockSize);
			}

			FailIf (FALSE == createDMAPrograms (NULL != ioBaseDMAInput), Exit);
		}
	}

	if (audioStream != mInputStream && NULL != mInputStream) {
		mInputStream->hardwareFormatChanged (&mDBDMAInputFormat);
	} else if (audioStream != mOutputStream && NULL != mOutputStream) {
		mOutputStream->hardwareFormatChanged (&mDBDMAOutputFormat);
	}

	if (NULL != newSampleRate) {
		if (newSampleRate->whole != sampleRate.whole) {
			updateDSPForSampleRate (newSampleRate->whole);
		}
	}
	
	// Tell AppleOnboardAudio about the format or sample rate change.
	result = ourProvider->formatChangeRequest (newFormat, newSampleRate);

	// in and out have the same format always.
	chooseOutputClippingRoutinePtr();
	chooseInputConversionRoutinePtr();

Exit:
    return result;
}

void AppleDBDMAAudio::updateDSPForSampleRate (UInt32 inSampleRate) {
	UInt32 index;
	
	debug2IOLog ("updateDSPForSampleRate (%ld)\n", inSampleRate);

	#ifdef LOG_SOFT_DSP
	IOLog ("updateDSPForSampleRate (%ld)\n", inSampleRate);
	IOLog ("mCurrentEQStructPtr->numSoftwareFilters = %ld\n", mCurrentEQStructPtr->numSoftwareFilters);
	#endif

	if (!mCurrentEQStructPtr->bypassAll) {
		// Update EQ coefficients
		debug2IOLog ("updateDSPForSampleRate (%ld), filters not bypassed.\n", inSampleRate);
		for (index = 0; index < kMaxNumFilters; index++) {
			#ifdef LOG_SOFT_DSP
			IOLog ("mEQParams[%ld].type = %4s\n", index, (char *)&(mEQParams[index].type));
			IOLog ("mEQParams[%ld].fc = %ld\n", index, *(UInt32 *)&(mEQParams[index].fc));
			IOLog ("mEQParams[%ld].gain = %ld\n", index, *(UInt32 *)&(mEQParams[index].gain));
			IOLog ("mEQParams[%ld].Q = %ld\n", index, *(UInt32 *)&(mEQParams[index].Q));
			#endif
			setEQCoefficients (&mEQParams[index], mCurrentEQStructPtr, index, inSampleRate);
			#ifdef LOG_SOFT_DSP
			IOLog ("mCurrentEQStructPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b0[index])));
			IOLog ("mCurrentEQStructPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b1[index])));
			IOLog ("mCurrentEQStructPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b2[index])));
			IOLog ("mCurrentEQStructPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a1[index])));
			IOLog ("mCurrentEQStructPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a2[index])));
			#endif
		}
		// update hardware EQ in non-realtime mode
		(ourProvider->getCurrentOutputPlugin ())->setEQProcessing ((void *)mCurrentEQStructPtr, FALSE);
	}

	#ifdef LOG_SOFT_DSP
	IOLog ("mCurrentCrossoverStructPtr->numBands = %ld\n", mCurrentCrossoverStructPtr->numBands);
	#endif

	if (!mCurrentLimiterStructPtr->bypassAll && (mCurrentCrossoverStructPtr->numBands > 0)) {
		// Update Limiter attack, release and lookahead times
		for (index = 0; index < mCurrentCrossoverStructPtr->numBands; index++) {
			setLimiterCoefficients (&mLimiterParams[index], mCurrentLimiterStructPtr, index, inSampleRate);
			#ifdef LOG_SOFT_DSP
			IOLog ("mCurrentLimiterStructPtr->threshold[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentLimiterStructPtr->threshold[index])));
			IOLog ("mCurrentLimiterStructPtr->oneMinusOneOverRatio[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentLimiterStructPtr->oneMinusOneOverRatio[index])));
			IOLog ("mCurrentLimiterStructPtr->attackTc[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentLimiterStructPtr->attackTc[index])));
			IOLog ("mCurrentLimiterStructPtr->releaseTc[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentLimiterStructPtr->releaseTc[index])));
			IOLog ("mCurrentLimiterStructPtr->lookahead[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentLimiterStructPtr->lookahead[index])));
			IOLog ("mCurrentLimiterStructPtr->type[%ld] = 0x%x\n", index, mCurrentLimiterStructPtr->type[index]);
			#endif
		}
		// Update crossover coefficients
		setCrossoverCoefficients (&mCrossoverParams, mCurrentCrossoverStructPtr, inSampleRate);
		#ifdef LOG_SOFT_DSP
		IOLog ("mCurrentCrossoverStructPtr->numBands = 0x%lx\n", mCurrentCrossoverStructPtr->numBands);
		IOLog ("mCurrentCrossoverStructPtr->c1_1st[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c1_1st[0])));
		IOLog ("mCurrentCrossoverStructPtr->c1_2nd[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c1_2nd[0])));
		IOLog ("mCurrentCrossoverStructPtr->c2_2nd[0] = 0x%lx\n", *((UInt32 *)&(mCurrentCrossoverStructPtr->c2_2nd[0])));
		IOLog ("mCurrentCrossoverStructPtr->outBufferPtr[0] = %p\n", mCurrentCrossoverStructPtr->outBufferPtr[0]);
		IOLog ("mCurrentCrossoverStructPtr->outBufferPtr[1] = %p\n", mCurrentCrossoverStructPtr->outBufferPtr[1]);
		#endif
	}

	if (!mCurrentInputEQStructPtr->bypassAll) {
		// Update EQ coefficients
		debug2IOLog ("updateDSPForSampleRate (%ld), input filters not bypassed.\n", inSampleRate);
		for (index = 0; index < kMaxNumFilters; index++) {
			#ifdef LOG_SOFT_DSP
			IOLog ("mInputEQParams[%ld].type = %4s\n", index, (char *)&(mEQParams[index].type));
			IOLog ("mInputEQParams[%ld].fc = %ld\n", index, *(UInt32 *)&(mEQParams[index].fc));
			IOLog ("mInputEQParams[%ld].gain = %ld\n", index, *(UInt32 *)&(mEQParams[index].gain));
			IOLog ("mInputEQParams[%ld].Q = %ld\n", index, *(UInt32 *)&(mEQParams[index].Q));
			#endif
			setEQCoefficients (&mInputEQParams[index], mCurrentInputEQStructPtr, index, inSampleRate);
			#ifdef LOG_SOFT_DSP
			IOLog ("mCurrentInputEQStructPtr->b0[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b0[index])));
			IOLog ("mCurrentInputEQStructPtr->b1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b1[index])));
			IOLog ("mCurrentInputEQStructPtr->b2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->b2[index])));
			IOLog ("mCurrentInputEQStructPtr->a1[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a1[index])));
			IOLog ("mCurrentInputEQStructPtr->a2[%ld] = 0x%lx\n", index, *((UInt32 *)&(mCurrentEQStructPtr->a2[index])));
			#endif
		}
	}

}

#pragma mark ------------------------ 
#pragma mark еее iSub Support
#pragma mark ------------------------ 

IOReturn AppleDBDMAAudio::iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn						result;
    AppleDBDMAAudio *		audioDMAEngine;
    IOCommandGate *					cg;

	debug5IOLog ("+ AppleDBDMAAudio::iSubAttachChangeHandler (%p, %p, 0x%lx, 0x%lx)\n", target, attachControl, oldValue, newValue);

	result = kIOReturnSuccess;
	if (oldValue != newValue) {
		audioDMAEngine = OSDynamicCast (AppleDBDMAAudio, target);
		FailIf (NULL == audioDMAEngine, Exit);
	
		if (newValue) {
			debugIOLog ("will try to connect to an iSub, installing notifier.\n");
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
			debugIOLog ("do not try to connect to iSub, removing notifier.\n");
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
	}
	
Exit:
    debugIOLog ("- AppleDBDMAAudio::iSubAttachChangeHandler\n");
    return result;
}

bool AppleDBDMAAudio::iSubEnginePublished (AppleDBDMAAudio * dbdmaEngineObject, void * refCon, IOService * newService) {
	IOReturn						result;
	bool							resultCode;
    IOCommandGate *					cg;

	debug4IOLog ("+AppleDBDMAAudio::iSubEnginePublished (%p, %p, %p)\n", dbdmaEngineObject, (UInt32*)refCon, newService);

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
//	dbdmaEngineObject->attach (dbdmaEngineObject->iSubEngine);
	dbdmaEngineObject->iSubEngine->retain ();
	cg = dbdmaEngineObject->getCommandGate ();
//	FailWithAction (NULL == cg, dbdmaEngineObject->detach (dbdmaEngineObject->iSubEngine), Exit);
	FailWithAction (NULL == cg, dbdmaEngineObject->iSubEngine->release(), Exit);
	dbdmaEngineObject->setSampleOffset(kMinimumLatencyiSub);	// HAL should notice this when iSub adds it's controls and sends out update
	IOSleep (102);
	result = cg->runAction (iSubOpenAction);
	FailWithAction (kIOReturnSuccess != result, dbdmaEngineObject->iSubEngine->release (), Exit);
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
	
	debug5IOLog ("-AppleDBDMAAudio::iSubEnginePublished (%p, %p, %p), result = %d\n", dbdmaEngineObject, (UInt32 *)refCon, newService, resultCode);
	return resultCode;
}

IOReturn AppleDBDMAAudio::iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
    if (NULL != owner) {
        AppleDBDMAAudio *		audioEngine;

		debugIOLog ("+AppleDBDMAAudio::iSubCloseAction\n");

		audioEngine = OSDynamicCast (AppleDBDMAAudio, owner);

        if (NULL != audioEngine && NULL != audioEngine->iSubEngine && TRUE == audioEngine->iSubOpen) {
			AppleiSubEngine *				oldiSubEngine;
			
			oldiSubEngine = audioEngine->iSubEngine;
			
			audioEngine->pauseAudioEngine ();
			audioEngine->beginConfigurationChange ();

			audioEngine->iSubEngine->closeiSub (audioEngine);

			audioEngine->iSubEngine = NULL;
			audioEngine->iSubBufferMemory = NULL;

			// [3094574] aml - iSub is gone, update the clipping routine while the engine is paused
			audioEngine->chooseOutputClippingRoutinePtr ();

			audioEngine->completeConfigurationChange ();
			audioEngine->resumeAudioEngine ();

			oldiSubEngine->release (); //(audioEngine->iSubEngine);

			//audioEngine->iSubEngine = NULL;
			//audioEngine->iSubBufferMemory = NULL;

			if (NULL != audioEngine->miSubProcessingParams.lowFreqSamples) {
				IOFree (audioEngine->miSubProcessingParams.lowFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->miSubProcessingParams.lowFreqSamples = NULL;
			}

			if (NULL != audioEngine->miSubProcessingParams.highFreqSamples) {
				IOFree (audioEngine->miSubProcessingParams.highFreqSamples, (audioEngine->numBlocks * audioEngine->blockSize) * sizeof (float));
				audioEngine->miSubProcessingParams.highFreqSamples = NULL;
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

	debugIOLog ("-AppleDBDMAAudio::iSubCloseAction\n");
	return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudio::iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn					result;
	bool						resultBool;

	debugIOLog ("+AppleDBDMAAudio::iSubOpenAction\n");

	result = kIOReturnError;
	resultBool = FALSE;

    if (NULL != owner) {
        AppleDBDMAAudio *		audioEngine;

		audioEngine = OSDynamicCast (AppleDBDMAAudio, owner);
		resultBool = audioEngine->iSubEngine->openiSub (audioEngine, &requestiSubClose);
    }

	if (resultBool) {
		result = kIOReturnSuccess;
	}

	debugIOLog ("-AppleDBDMAAudio::iSubOpenAction\n");
	return result;
}

void AppleDBDMAAudio::iSubSynchronize(UInt32 firstSampleFrame, UInt32 numSampleFrames) 
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

	return;
}

void AppleDBDMAAudio::resetiSubProcessingState() 
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

bool AppleDBDMAAudio::willTerminate (IOService * provider, IOOptionBits options) {
//	IOCommandGate *					cg;
	Boolean 						result;
	
	debug3IOLog ("+AppleDBDMAAudio[%p]::willTerminate (%p)\n", this, provider);

/*
	if (iSubEngine == (AppleiSubEngine *)provider) {
		debugIOLog ("iSub requesting termination\n");

		cg = getCommandGate ();
		if (NULL != cg) {
			cg->runAction (iSubCloseAction);
		}

		// Set up notifier to run when iSub shows up again
		if (NULL != iSubAttach) {
			if (iSubAttach->getIntValue ()) {
				iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&iSubEnginePublished, this);
			}
		}
	}
*/
	
	debug2IOLog ("AppleDBDMAAudio::willTerminate, before audioDevice retain count = %d\n", audioDevice->getRetainCount());

	result = super::willTerminate (provider, options);
	debug3IOLog ("-AppleDBDMAAudio[%p]::willTerminate, super::willTerminate () returned %d\n", this, result);

	return result;
}


bool AppleDBDMAAudio::requestTerminate (IOService * provider, IOOptionBits options) {
	Boolean 						result;

	result = super::requestTerminate (provider, options);
	debug3IOLog ("AppleDBDMAAudio[%p]::requestTerminate, super::requestTerminate () returned %d\n", this, result);

	return result;
}

void AppleDBDMAAudio::requestiSubClose (IOAudioEngine * audioEngine) {
	AppleDBDMAAudio *				dbdmaAudioEngine;
    IOCommandGate *								cg;

	dbdmaAudioEngine = OSDynamicCast (AppleDBDMAAudio, audioEngine);

	cg = dbdmaAudioEngine->getCommandGate ();
	if (NULL != cg) {
		cg->runAction (dbdmaAudioEngine->iSubCloseAction);
	}

	// Set up notifier to run when iSub shows up again
	if (dbdmaAudioEngine->iSubAttach->getIntValue ()) {
		dbdmaAudioEngine->iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&dbdmaAudioEngine->iSubEnginePublished, dbdmaAudioEngine);
	}
}

void AppleDBDMAAudio::updateiSubPosition(UInt32 firstSampleFrame, UInt32 numSampleFrames)
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

inline void AppleDBDMAAudio::startTiming() {
#ifdef _TIME_CLIP_ROUTINE
	AbsoluteTime				uptime;
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
		clock_get_uptime (&mLastuptime);
	}	
#endif
}

inline void AppleDBDMAAudio::endTiming() {
#ifdef _TIME_CLIP_ROUTINE
	AbsoluteTime				uptime;
	UInt64						nanos;
	if ((mCallCount % kCallFrequency) == 0) {
		clock_get_uptime (&uptime);
		SUB_ABSOLUTETIME (&uptime, &mLastuptime);
		absolutetime_to_nanoseconds (uptime, &nanos);
		IOLog("%ld\n", uptime.lo);
	}
#endif
}


// --------------------------------------------------------------------------
//	When running on the external I2S clock, it is possible to kill the
//	DMA with no indication of an error.  This method detects that the 
//	DMA has fozen.  Recovery is implemented from within AppleOnboardAudio.
//	[3305011]	begin {
bool AppleDBDMAAudio::engineDied ( void ) {
	bool			result = FALSE;
	UInt32			tempInterruptCount;

	tempInterruptCount = 0;
	if ( dmaRunState ) {
		tempInterruptCount = mDmaInterruptCount;
		if ( tempInterruptCount == mLastDmaInterruptCount ) {
			mNumberOfFrozenDmaInterruptCounts++;
			if ( kMAXIMUM_NUMBER_OF_FROZEN_DMA_IRQ_COUNTS <= mNumberOfFrozenDmaInterruptCounts ) {
				result = TRUE;
				mDmaRecoveryInProcess = true;
				mNumberOfFrozenDmaInterruptCounts = 0;
			}
		} else {
			mLastDmaInterruptCount = tempInterruptCount;
			mNumberOfFrozenDmaInterruptCounts = 0;
		}
	} else {
		mLastDmaInterruptCount = tempInterruptCount;
		mNumberOfFrozenDmaInterruptCounts = 0;
	}
	return result;
}
//	} end	[3305011]







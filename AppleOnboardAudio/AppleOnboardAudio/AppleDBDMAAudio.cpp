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
#define kCallFrequency 50
#endif 

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
	UInt32 resultOut = 1;
    UInt32 resultIn = 1;
	
	// check to see if this interupt is because the DMA went bad
	if ( ioBaseDMAOutput ) {
		resultOut = IOGetDBDMAChannelStatus (ioBaseDMAOutput);
	}

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
	debugIOLog (3, "+ AppleDBDMAAudio::free()");

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

	debugIOLog (3, "- AppleDBDMAAudio::free()");
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
                                 bool				hasOutput,
								 OSArray *			formatsArray,
                                 UInt32				nBlocks )
{
	Boolean					result;

	debugIOLog (3,  "+ AppleDBDMAAudio::init ( %X, %X, %d, %d, %X, %X )",
			(unsigned int)properties,
			(unsigned int)theDeviceProvider,
			(unsigned int)hasInput,
			(unsigned int)hasOutput,
			(unsigned int)formatsArray,
			(unsigned int)nBlocks);
			
	result = FALSE;
	
	mHasInput = hasInput;
	mHasOutput = hasOutput;
	
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
	ioBaseDMAOutput = hasOutput ? mPlatformObject->GetOutputChannelRegistersVirtualAddress ( mDeviceProvider ) : NULL ;
	if ( NULL == ioBaseDMAOutput && hasOutput ) {
		debugIOLog (3,  "ioBaseDMAOutput = NULL" );
		IOSleep ( 50000 );
		FailIf ( true, Exit );
	}
	ioBaseDMAInput = hasInput ? mPlatformObject->GetInputChannelRegistersVirtualAddress ( mDeviceProvider ) : NULL ;
	if ( NULL == ioBaseDMAInput && hasInput ) {
		debugIOLog (3,  "ioBaseDMAInput = NULL" );
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

    mOutputSampleBuffer = IOMallocAligned(numBlocks * blockSize, PAGE_SIZE);

#ifdef _TIME_CLIP_ROUTINE
	mCallCount = 0;
	mPreviousUptime.hi = 0;
	mPreviousUptime.lo = 0;
#endif

	result = TRUE;

Exit:
	debugIOLog (3,  "- AppleDBDMAAudio::init returns %d", (unsigned int)result );
			
	return result;
}

bool AppleDBDMAAudio::initHardware (IOService *provider) {
    UInt32						interruptIndex;
    IOWorkLoop *				workLoop;
	Boolean						result;

	debugIOLog (3, "+ AppleDBDMAAudio::initHardware ()");

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

	debugIOLog (3, "AppleDBDMAAudio:: setNumSampleFramesPerBuffer(%lu)  numBlocks=%lu blockSize=%lu ",(numBlocks * blockSize / ((mDBDMAOutputFormat.fBitWidth / 8) * mDBDMAOutputFormat.fNumChannels)),numBlocks,blockSize);

	// install an interrupt handler only on the Output size of it !!! input only??
    workLoop = getWorkLoop();
    FailIf (!workLoop, Exit);

	debugIOLog (3, "AppleDBDMAudio::initHardware() interrupt source's name = %s", mDeviceProvider->getName());	

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
	debugIOLog (3, "- AppleDBDMAAudio::initHardware() returns %d", result);
    return result;
}

AudioHardwareObjectInterface* AppleDBDMAAudio::getCurrentOutputPlugin () {
	return ourProvider->getCurrentOutputPlugin ();
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
	
	if ( ioBaseDMAOutput ) {
		mOutputStream = new IOAudioStream;
	}
	
	FailIf ((mHasInput && !mInputStream) || (mHasOutput && !mOutputStream), Exit);

	if (mOutputStream) { 
		result = mOutputStream->initWithAudioEngine (this, kIOAudioStreamDirectionOutput, 1, 0, 0); 
		FailIf ( !result, Exit );
	}
	
	if (mInputStream) { 
		result = mInputStream->initWithAudioEngine (this, kIOAudioStreamDirectionInput, 1, 0, 0); 
		FailIf ( !result, Exit );
	}

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

	debugIOLog (3,  "numFormats = %ld", numFormats );
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

		debugIOLog (3, "dbdmaFormat: fNumChannels = %d, fBitDepth = %d, fBitWidth = %d", (unsigned int)dbdmaFormat.fNumChannels, (unsigned int)dbdmaFormat.fBitDepth, (unsigned int)dbdmaFormat.fBitWidth);
		sampleRatesArray = OSDynamicCast (OSArray, formatDict->getObject (kSampleRates));
		FailIf (NULL == sampleRatesArray, Exit);
		numSampleRates = sampleRatesArray->getCount ();
		debugIOLog (3,  "numSampleRates = %ld", numSampleRates );

		if (kIOAudioStreamSampleFormat1937AC3 == dbdmaFormat.fSampleFormat) {
			dbdmaFormatExtension.fFramesPerPacket = 1536;
			dbdmaFormatExtension.fBytesPerPacket = dbdmaFormatExtension.fFramesPerPacket * dbdmaFormat.fNumChannels * (dbdmaFormat.fBitWidth / 8);
		}

		for (rateIndex = 0; rateIndex < numSampleRates; rateIndex++) {
			sampleRate.whole = ((OSNumber *)sampleRatesArray->getObject(rateIndex))->unsigned32BitValue ();
			debugIOLog (3, "dbdmaFormat: sampleRate.whole = %d", (unsigned int)sampleRate.whole);
			if (mOutputStream) {
				mOutputStream->addAvailableFormat (&dbdmaFormat, &dbdmaFormatExtension, &sampleRate, &sampleRate);
			}
			if (mInputStream && kIOAudioStreamSampleFormatLinearPCM == dbdmaFormat.fSampleFormat) {
				mInputStream->addAvailableFormat (&dbdmaFormat, &sampleRate, &sampleRate);
			}
			// XXX Remove hardcoding of default format!
			if (dbdmaFormat.fNumChannels == 2 && dbdmaFormat.fBitDepth == 16 && sampleRate.whole == 44100 && kIOAudioStreamSampleFormatLinearPCM == dbdmaFormat.fSampleFormat) {
				debugIOLog (3, "dbdmaFormat: mOutputStream->setFormat to 2, 16, 44100");
				if (mOutputStream) { mOutputStream->setFormat (&dbdmaFormat); }
				if (mInputStream) { mInputStream->setFormat (&dbdmaFormat); }
				setSampleRate (&sampleRate);
				ourProvider->formatChangeRequest (NULL, &sampleRate);
			}
			
			// [3306295] all mixable formats get duplicated as non-mixable for hog mode
			if (dbdmaFormat.fIsMixable) {
				dbdmaFormat.fIsMixable = false;
				if (mOutputStream) { mOutputStream->addAvailableFormat (&dbdmaFormat, &dbdmaFormatExtension, &sampleRate, &sampleRate); }
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

	if (mOutputStream) {
		addAudioStream (mOutputStream);
	}
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

	debugIOLog (3, "allocateDMABuffers: buffer size = %ld", numBlocks * blockSize);

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

bool AppleDBDMAAudio::createDMAPrograms ( void ) {
	vm_offset_t					offset;
	IOPhysicalAddress			commandBufferPhys;
	IOPhysicalAddress			sampleBufferPhys;
	IOPhysicalAddress			stopCommandPhys;
	UInt32						dmaCommand;
	UInt32						blockNum;
	Boolean						doInterrupt;
	bool						result;

	result = FALSE;			// Didn't successfully create DMA program

	if ( NULL != ioBaseDMAOutput ) {
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
	}
    
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

	debugIOLog (3, " + AppleDBDMAAudio::performAudioEngineStart()");
	result = kIOReturnError;
	
    FailIf (!interruptEventSource, Exit);

	if (dmaCommandBufferOut) {
		flush_dcache((vm_offset_t)dmaCommandBufferOut, commandBufferSize, false);
	}
    if (ioBaseDMAInput) {
        flush_dcache((vm_offset_t)dmaCommandBufferIn, commandBufferSize, false);
	}

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

	debugIOLog (3, "getNumSampleFramesPerBuffer %ld", getNumSampleFramesPerBuffer() );
	((AppleOnboardAudio *)audioDevice)->setCurrentSampleFrame (0);
	if ( mPlatformObject ) { mPlatformObject->LogDBDMAChannelRegisters(); }

	// start the input DMA first
    if (ioBaseDMAInput) {
        IOSetDBDMAChannelControl(ioBaseDMAInput, IOClearDBDMAChannelControlBits(kdbdmaS0));
        IOSetDBDMABranchSelect(ioBaseDMAInput, IOSetDBDMAChannelControlBits(kdbdmaS0));
		commandBufferPhys = dmaCommandBufferInMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		IODBDMAStart(ioBaseDMAInput, (IODBDMADescriptor *)commandBufferPhys);
		debugIOLog (3, "AppleDBDMAAudio::performAudioEngineStart - starting input DMA");
	}
    
	if (ioBaseDMAOutput) {
		IOSetDBDMAChannelControl(ioBaseDMAOutput, IOClearDBDMAChannelControlBits(kdbdmaS0));
		IOSetDBDMABranchSelect(ioBaseDMAOutput, IOSetDBDMAChannelControlBits(kdbdmaS0));
		commandBufferPhys = dmaCommandBufferOutMemDescriptor->getPhysicalAddress ();
		FailIf (NULL == commandBufferPhys, Exit);
		IODBDMAStart(ioBaseDMAOutput, (IODBDMADescriptor *)commandBufferPhys);
	}

	if ( mPlatformObject ) { mPlatformObject->LogDBDMAChannelRegisters(); }

	dmaRunState = TRUE;				//	rbm 7.12.02	added for user client support
	result = kIOReturnSuccess;

    debugIOLog (3, " - AppleDBDMAAudio::performAudioEngineStart() returns %X", result);

Exit:
    return result;
}

IOReturn AppleDBDMAAudio::performAudioEngineStop()
{
    UInt16 attemptsToStop = kDBDMAAttemptsToStop; // [3282437], was 1000

    debugIOLog (3, "+ AppleDBDMAAudio::performAudioEngineStop()");

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
		debugIOLog (3,  "AppleDBDMAAudio::performAudioEngineStop has no output" );
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
		debugIOLog (3,  "AppleDBDMAAudio::performAudioEngineStop has no input" );
    }
    
	dmaRunState = FALSE;				//	rbm 7.12.02	added for user client support
    interruptEventSource->enable();

    debugIOLog (3, "- AppleDBDMAAudio::performAudioEngineStop()");
    return kIOReturnSuccess;
}

// This gets called when a new audio stream needs to be mixed into an already playing audio stream
void AppleDBDMAAudio::resetClipPosition (IOAudioStream *audioStream, UInt32 clipSampleFrame) {
  if ((NULL != iSubBufferMemory) && (NULL != iSubEngine)) {
				
		resetiSubProcessingState();

		*((UInt32 *)&mLastOutputSample) = 0;
		*((UInt32 *)&mLastInputSample) = 0;

        debugIOLog (3, "+resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld, clipSampleFrame=%ld", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame, clipSampleFrame);
        if (previousClippedToFrame < clipSampleFrame) {
			// Resetting the clip point backwards around the end of the buffer
			clipAdjustment = (getNumSampleFramesPerBuffer () - clipSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();
        } else {
			clipAdjustment = (previousClippedToFrame - clipSampleFrame) * iSubEngine->GetNumChannels();
        }
		#if DEBUGLOG
        if (clipAdjustment < kMinimumLatency) {
            debugIOLog (3, "resetClipPosition: 44.1 clipAdjustment < min, clipAdjustment=%ld", clipAdjustment); 
        }                
		#endif
        clipAdjustment = (clipAdjustment * 1000) / ((1000 * getSampleRate()->whole) / iSubEngine->GetSampleRate());  
        miSubProcessingParams.iSubBufferOffset -= clipAdjustment;

		#if DEBUGLOG
        if (clipAdjustment > (iSubBufferMemory->getLength () / 2)) {
            debugIOLog (3, "resetClipPosition: clipAdjustment > iSub buffer size, clipAdjustment=%ld", clipAdjustment); 
        }                
		#endif

        if (miSubProcessingParams.iSubBufferOffset < 0) {
			miSubProcessingParams.iSubBufferOffset += (iSubBufferMemory->getLength () / 2);	
			miSubProcessingParams.iSubLoopCount--;
        }

        previousClippedToFrame = clipSampleFrame;
        justResetClipPosition = TRUE;

        debugIOLog (3, "-resetClipPosition: iSubBufferOffset=%ld, previousClippedToFrame=%ld", miSubProcessingParams.iSubBufferOffset, previousClippedToFrame);
    }
}

IOReturn AppleDBDMAAudio::restartDMA () {
	IOReturn					result;
	
	performAudioEngineStop ();
	performAudioEngineStart ();
	result = kIOReturnSuccess;

    return result;
}

void AppleDBDMAAudio::setSampleLatencies (UInt32 outputLatency, UInt32 inputLatency) {
	setOutputSampleLatency (outputLatency);
	setInputSampleLatency (inputLatency);
}

void AppleDBDMAAudio::stop(IOService *provider)
{
    IOWorkLoop *workLoop;
    
    debugIOLog (3, " + AppleDBDMAAudio[%p]::stop(%p)", this, provider);
    
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
	
    debugIOLog (3, " - AppleDBDMAAudio[%p]::stop(%p)", this, provider);
}


void AppleDBDMAAudio::detach(IOService *provider)
{
	super::detach(provider);
    debugIOLog (3, "AppleDBDMAAudio::detach(%p)", provider);
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
    
	location = mDeviceProvider->getName();
    
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
        
		debugIOLog (3, "getGlobalUniqueID = %s", uniqueIDStr);
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
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32iSub;
				}
			} else if (16 == mDBDMAOutputFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSubMixRightChannel;
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16iSub;
				}
			} else {
				debugIOLog (3, "AppleDBDMAAudio::chooseOutputClippingRoutinePtr - Non-supported output bit depth, iSub attached.");
			}	
		} else {
			if (32 == mDBDMAOutputFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32MixRightChannel;
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream32;
				}
			} else if (16 == mDBDMAOutputFormat.fBitWidth) {
				if (TRUE == fNeedsRightChanMixed) {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16MixRightChannel;
				} else {
					mClipAppleDBDMAToOutputStreamRoutine = &AppleDBDMAAudio::clipAppleDBDMAToOutputStream16;
				}
			} else {
				debugIOLog (3, "AppleDBDMAAudio::chooseOutputClippingRoutinePtr - Non-supported output bit depth.");
			}
		}
	}
}

// [3094574] aml, pick the correct input conversion routine based on our current state
void AppleDBDMAAudio::chooseInputConversionRoutinePtr() 
{
	if (32 == mDBDMAInputFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream32WithGain;
		} else {
			mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream32;
		}
	} else if (16 == mDBDMAInputFormat.fBitWidth) {
		if (mUseSoftwareInputGain) {
			mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16WithGain;
		} else {
			if (e_Mode_CopyRightToLeft == mInputDualMonoMode) {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16CopyR2L;
			} else {
				mConvertInputStreamToAppleDBDMARoutine = &AppleDBDMAAudio::convertAppleDBDMAFromInputStream16;
			}
		}
	} else {
		debugIOLog (3, "AppleDBDMAAudio::chooseInputConversionRoutinePtr - Non-supported input bit depth!");
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

#pragma mark ------------------------ 
#pragma mark еее State Routines
#pragma mark ------------------------ 

void AppleDBDMAAudio::resetOutputClipOptions() {
	fNeedsRightChanMixed = false;
	
	chooseOutputClippingRoutinePtr ();
}

void AppleDBDMAAudio::resetInputClipOptions() {
	mInputDualMonoMode = e_Mode_Disabled;
	fNeedsRightChanDelayInput = false;

	chooseInputConversionRoutinePtr ();
}

void AppleDBDMAAudio::setOutputSignalProcessing (OSDictionary * inDictionary) {
}

void AppleDBDMAAudio::setInputSignalProcessing (OSDictionary * inDictionary) {
}

void AppleDBDMAAudio::enableOutputProcessing (void) {
}

void AppleDBDMAAudio::disableOutputProcessing (void) {
}

void AppleDBDMAAudio::enableInputProcessing (void) {
}

void AppleDBDMAAudio::disableInputProcessing (void) {
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

void AppleDBDMAAudio::setRightChanMixed(const bool needsRightChanMixed)  
{
	fNeedsRightChanMixed = needsRightChanMixed;  
	chooseOutputClippingRoutinePtr();
	
	return;   
}

void AppleDBDMAAudio::setUseSoftwareInputGain(const bool inUseSoftwareInputGain) 
{     
	debugIOLog (3, "setUseSoftwareInputGain (%s)", inUseSoftwareInputGain ? "true" : "false");
	
	mUseSoftwareInputGain = inUseSoftwareInputGain;     	
	chooseInputConversionRoutinePtr();
	
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
	outState->needsPhaseInversion = false;
	outState->needsRightChanMixed = fNeedsRightChanMixed;
	outState->needsRightChanDelay = false;
	outState->needsBalanceAdjust = false;
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
	
	debugIOLog (3, "AppleDBDMAAudio::performFormatChange (%p, %p, %p)", audioStream, newFormat, newSampleRate);	
	
	if ( NULL != newFormat ) {	
		debugIOLog (3, "user commands us to switch to format '%4s'", (char *)&newFormat->fSampleFormat);

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

			if ( mOutputStream ) {
				mOutputStream->setSampleBuffer ((void *)mOutputSampleBuffer, numBlocks * blockSize);
			}
			if (mInputStream) {
				mInputStream->setSampleBuffer ((void *)mInputSampleBuffer, numBlocks * blockSize);
			}

			FailIf (FALSE == createDMAPrograms (), Exit);
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
	debugIOLog (3, "updateDSPForSampleRate (%ld)", inSampleRate);
}

#pragma mark ------------------------ 
#pragma mark еее iSub Support
#pragma mark ------------------------ 

IOReturn AppleDBDMAAudio::iSubAttachChangeHandler (IOService *target, IOAudioControl *attachControl, SInt32 oldValue, SInt32 newValue) {
    IOReturn						result;
    AppleDBDMAAudio *		audioDMAEngine;
    IOCommandGate *					cg;

	debugIOLog (3, "+ AppleDBDMAAudio::iSubAttachChangeHandler (%p, %p, 0x%lx, 0x%lx)", target, attachControl, oldValue, newValue);

	result = kIOReturnSuccess;
	if (oldValue != newValue) {
		audioDMAEngine = OSDynamicCast (AppleDBDMAAudio, target);
		FailIf (NULL == audioDMAEngine, Exit);
	
		if (newValue) {
			debugIOLog (3, "will try to connect to an iSub, installing notifier.");
			// Set up notifier to run when iSub shows up
			audioDMAEngine->iSubEngineNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleiSubEngine"), (IOServiceNotificationHandler)&iSubEnginePublished, audioDMAEngine);
			if (NULL != audioDMAEngine->iSubBufferMemory) {
				// it looks like the notifier could be called before iSubEngineNotifier is set, 
				// so if it was called, then iSubBufferMemory would no longer be NULL and we can remove the notifier
				debugIOLog (3, "iSub was already attached");
				audioDMAEngine->iSubEngineNotifier->remove ();
				audioDMAEngine->iSubEngineNotifier = NULL;
			}
		} else {
			debugIOLog (3, "do not try to connect to iSub, removing notifier.");
			if (NULL != audioDMAEngine->iSubBufferMemory) {
				debugIOLog (3, "disconnect from iSub");
				// We're already attached to an iSub, so detach
				cg = audioDMAEngine->getCommandGate ();
				if (NULL != cg) {
					cg->runAction (iSubCloseAction);
				}
			}
	
			// We're not attached to the iSub, so just remove our notifier
			if (NULL != audioDMAEngine->iSubEngineNotifier) {
				debugIOLog (3, "remove iSub notifier");
				audioDMAEngine->iSubEngineNotifier->remove ();
				audioDMAEngine->iSubEngineNotifier = NULL;
			}
		}
	}
	
Exit:
    debugIOLog (3, "- AppleDBDMAAudio::iSubAttachChangeHandler");
    return result;
}

bool AppleDBDMAAudio::iSubEnginePublished (AppleDBDMAAudio * dbdmaEngineObject, void * refCon, IOService * newService) {
	IOReturn						result;
	bool							resultCode;
    IOCommandGate *					cg;

	debugIOLog (3, "+AppleDBDMAAudio::iSubEnginePublished (%p, %p, %p)", dbdmaEngineObject, (UInt32*)refCon, newService);

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
	debugIOLog (3, "iSubBuffer length = %ld", dbdmaEngineObject->iSubBufferMemory->getLength ());

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
	
	debugIOLog (3, "-AppleDBDMAAudio::iSubEnginePublished (%p, %p, %p), result = %d", dbdmaEngineObject, (UInt32 *)refCon, newService, resultCode);
	return resultCode;
}

IOReturn AppleDBDMAAudio::iSubCloseAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
    if (NULL != owner) {
        AppleDBDMAAudio *		audioEngine;

		debugIOLog (3, "+AppleDBDMAAudio::iSubCloseAction");

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

			debugIOLog (3, "iSub connections terminated");
        } else {
			debugIOLog (3, "didn't terminate the iSub connections because we didn't have an audioEngine");
		}
	} else {
		debugIOLog (3, "didn't terminate the iSub connections owner = %p, arg1 = %p", owner, arg1);
    }

	debugIOLog (3, "-AppleDBDMAAudio::iSubCloseAction");
	return kIOReturnSuccess;
}

IOReturn AppleDBDMAAudio::iSubOpenAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn					result;
	bool						resultBool;

	debugIOLog (3, "+AppleDBDMAAudio::iSubOpenAction");

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

	debugIOLog (3, "-AppleDBDMAAudio::iSubOpenAction");
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
//			debugIOLog (3, "wrote %ld iSub samples", wrote);
		if (miSubProcessingParams.iSubLoopCount == iSubEngine->GetCurrentLoopCount () && miSubProcessingParams.iSubBufferOffset > (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
			distance = miSubProcessingParams.iSubBufferOffset - (iSubEngine->GetCurrentByteCount () / 2);
		} else if (miSubProcessingParams.iSubLoopCount == (iSubEngine->GetCurrentLoopCount () + 1) && miSubProcessingParams.iSubBufferOffset < (SInt32)(iSubEngine->GetCurrentByteCount () / 2)) {
			distance = iSubBufferLen - (iSubEngine->GetCurrentByteCount () / 2) + miSubProcessingParams.iSubBufferOffset;
		} else {
			distance = initialiSubLead;
		}

		if (distance < (initialiSubLead / 2)) {			
			// Write more samples into the iSub's buffer
//				debugIOLog (3, "speed up! %ld, %ld, %ld", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
			adaptiveSampleRate = sampleRate - (sampleRate >> 4);
		} else if (distance > (initialiSubLead + (initialiSubLead / 2))) {
			// Write fewer samples into the iSub's buffer
//				debugIOLog (3, "slow down! %ld, %ld, %ld", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
			adaptiveSampleRate = sampleRate + (sampleRate >> 4);
		} else {
			// The sample rate is just right
//				debugIOLog (3, "just right %ld, %ld, %ld", initialiSubLead, distance, iSubEngine->GetCurrentByteCount () / 2);
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
			debugIOLog (3, "****iSub is in front of write head safetyOffset = %ld, iSubEngine->GetCurrentByteCount () / 2 = %ld", safetyOffset, iSubEngine->GetCurrentByteCount () / 2);
//				debugIOLog (3, "distance = %ld", distance);
			#endif
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount > (iSubEngine->GetCurrentLoopCount () + 1)) {
			debugIOLog (3, "****looped more than the iSub iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld", miSubProcessingParams.iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount < iSubEngine->GetCurrentLoopCount ()) {
			debugIOLog (3, "****iSub is ahead of us iSubLoopCount = %ld, iSubEngine->GetCurrentLoopCount () = %ld", miSubProcessingParams.iSubLoopCount, iSubEngine->GetCurrentLoopCount ());
			needToSync = TRUE;
			startiSub = TRUE;
		} else if (miSubProcessingParams.iSubLoopCount == iSubEngine->GetCurrentLoopCount () && miSubProcessingParams.iSubBufferOffset > ((SInt32)( (iSubEngine->GetCurrentByteCount() + (((iSubFormat.outputSampleRate)/1000 * NUM_ISUB_FRAME_LISTS_TO_QUEUE * NUM_ISUB_FRAMES_PER_LIST) * iSubFormat.bytesPerSample * iSubFormat.numChannels) ) / 2))) {			// aml 3.27.02, this is the right number here (buffersize was 2x too large).
			debugIOLog (3, "****iSub is too far behind write head iSubBufferOffset = %ld, (iSubEngine->GetCurrentByteCount () / 2 + max queued data) = %ld", miSubProcessingParams.iSubBufferOffset, (iSubEngine->GetCurrentByteCount() / 2 + iSubBufferLen/2));					
			needToSync = TRUE;
			startiSub = TRUE;
		}
	}
	if (FALSE == needToSync && previousClippedToFrame != firstSampleFrame && !(previousClippedToFrame == getNumSampleFramesPerBuffer () && firstSampleFrame == 0)) {
		debugIOLog (3, "clipOutput: no sync: iSubBufferOffset was %ld", miSubProcessingParams.iSubBufferOffset);
		if (firstSampleFrame < previousClippedToFrame) {
			debugIOLog (3, "clipOutput: no sync: firstSampleFrame < previousClippedToFrame (delta = %ld)", previousClippedToFrame-firstSampleFrame);
			// We've wrapped around the buffer
			offsetDelta = (getNumSampleFramesPerBuffer () - firstSampleFrame + previousClippedToFrame) * iSubEngine->GetNumChannels();	
		} else {
			debugIOLog (3, "clipOutput: no sync: previousClippedToFrame < firstSampleFrame (delta = %ld)", firstSampleFrame - previousClippedToFrame);
			offsetDelta = (firstSampleFrame - previousClippedToFrame) * iSubEngine->GetNumChannels();
		}
		// aml 3.21.02, adjust for new sample rate
		offsetDelta = (offsetDelta * 1000) / ((sampleRate * 1000) / iSubFormat.outputSampleRate);

		miSubProcessingParams.iSubBufferOffset += offsetDelta;
		debugIOLog (3, "clipOutput: no sync: clip to point was %ld, now %ld (delta = %ld)", previousClippedToFrame, firstSampleFrame, offsetDelta);
		debugIOLog (3, "clipOutput: no sync: iSubBufferOffset is now %ld", miSubProcessingParams.iSubBufferOffset);
		if (miSubProcessingParams.iSubBufferOffset > (SInt32)iSubBufferLen) {
			debugIOLog (3, "clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset = %ld", miSubProcessingParams.iSubBufferOffset);
			// Our calculated spot has actually wrapped around the iSub's buffer.
			miSubProcessingParams.iSubLoopCount += miSubProcessingParams.iSubBufferOffset / iSubBufferLen;
			miSubProcessingParams.iSubBufferOffset = miSubProcessingParams.iSubBufferOffset % iSubBufferLen;

			debugIOLog (3, "clipOutput: no sync: iSubBufferOffset > iSubBufferLen, iSubBufferOffset is now %ld", miSubProcessingParams.iSubBufferOffset);
		} else if (miSubProcessingParams.iSubBufferOffset < 0) {

			miSubProcessingParams.iSubBufferOffset += iSubBufferLen;

			debugIOLog (3, "clipOutput: no sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld", miSubProcessingParams.iSubBufferOffset);
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
			debugIOLog (3, "clipOutput: need to sync: 44.1kHz offsetDelta = %ld", offsetDelta);

			if (offsetDelta < kMinimumLatency) {
				debugIOLog (3, "clipOutput: no sync: 44.1 offsetDelta < min, offsetDelta=%ld", offsetDelta); 
			}                
			#endif
			// aml 3.21.02, adjust for new sample rate
			offsetDelta = (offsetDelta * 1000) / ((sampleRate * 1000) / iSubFormat.outputSampleRate);
			debugIOLog (3, "clipOutput: need to sync: iSubBufferOffset = %ld, offsetDelta = %ld", miSubProcessingParams.iSubBufferOffset, offsetDelta);

			miSubProcessingParams.iSubBufferOffset = offsetDelta;
			debugIOLog (3, "clipOutput: need to sync: offsetDelta = %ld", offsetDelta);
			debugIOLog (3, "clipOutput: need to sync: firstSampleFrame = %ld, curSampleFrame = %ld", firstSampleFrame, curSampleFrame);
			debugIOLog (3, "clipOutput: need to sync: starting iSubBufferOffset = %ld, numSampleFrames = %ld", miSubProcessingParams.iSubBufferOffset, numSampleFrames);
			if (miSubProcessingParams.iSubBufferOffset > (SInt32)iSubBufferLen) {
		
				needToSync = TRUE;	// aml 4.24.02, requests larger than our buffer size = bad!
				debugIOLog (3, "clipOutput: need to sync: SubBufferOffset too big (%ld) RESYNC!", miSubProcessingParams.iSubBufferOffset);
				
				// Our calculated spot has actually wrapped around the iSub's buffer.

				miSubProcessingParams.iSubLoopCount += miSubProcessingParams.iSubBufferOffset / iSubBufferLen;
				miSubProcessingParams.iSubBufferOffset = miSubProcessingParams.iSubBufferOffset % iSubBufferLen;

				debugIOLog (3, "clipOutput: need to sync: iSubBufferOffset > iSubBufferLen (%ld), iSubBufferOffset is now %ld", iSubBufferLen, miSubProcessingParams.iSubBufferOffset);
			} else if (miSubProcessingParams.iSubBufferOffset < 0) {

				miSubProcessingParams.iSubBufferOffset += iSubBufferLen;

				debugIOLog (3, "clipOutput: need to sync: iSubBufferOffset < 0, iSubBufferOffset is now %ld", miSubProcessingParams.iSubBufferOffset);
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
	
	debugIOLog (3, "+AppleDBDMAAudio[%p]::willTerminate (%p)", this, provider);

/*
	if (iSubEngine == (AppleiSubEngine *)provider) {
		debugIOLog (3, "iSub requesting termination");

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
	
	debugIOLog (3, "AppleDBDMAAudio::willTerminate, before audioDevice retain count = %d", audioDevice->getRetainCount());

	result = super::willTerminate (provider, options);
	debugIOLog (3, "-AppleDBDMAAudio[%p]::willTerminate, super::willTerminate () returned %d", this, result);

	return result;
}


bool AppleDBDMAAudio::requestTerminate (IOService * provider, IOOptionBits options) {
	Boolean 						result;

	result = super::requestTerminate (provider, options);
	debugIOLog (3, "AppleDBDMAAudio[%p]::requestTerminate, super::requestTerminate () returned %d", this, result);

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

	mCallCount++;
	clock_get_uptime (&uptime);
	tempTime = uptime;
	if ((mCallCount % kCallFrequency) == 0) {
		UInt64						nanos;
		SUB_ABSOLUTETIME (&uptime, &mPreviousUptime);
		absolutetime_to_nanoseconds (uptime, &nanos);
		mTotalNanos = nanos;
		//IOLog (1, "clipOutputSamples[%ld]:\t%ld:", mCallCount, uptime.lo);
	}
	mPreviousUptime = tempTime;

	if ((mCallCount % kCallFrequency) == 0) {
		clock_get_uptime (&mLastuptime);
	}
#endif
}

inline void AppleDBDMAAudio::endTiming() {
#ifdef _TIME_CLIP_ROUTINE
	if ((mCallCount % kCallFrequency) == 0) {
		AbsoluteTime				uptime;
		UInt64						nanos;
		float						percent;
		clock_get_uptime (&uptime);
		SUB_ABSOLUTETIME (&uptime, &mLastuptime);
		absolutetime_to_nanoseconds (uptime, &nanos);
		//IOLog ("%ld", uptime.lo);
		convertNanosToPercent(nanos, mTotalNanos, &percent);
		//IOLog1Float ("\t", &percent, " \%\n");
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







#include "AudioDebug.h"
#include "VirtualAudioEngine.h"

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

#define INITIAL_SAMPLE_RATE	44100
#define BLOCK_SIZE			512		// Sample frames
#define NUM_BLOCKS			32
#define NUM_STREAMS			1

#define super IOAudioEngine
            
OSDefineMetaClassAndStructors(VirtualAudioEngine, IOAudioEngine)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioEngine::init(OSDictionary *properties)
{
    bool result = false;
    OSNumber *number;
    
    debug2IOLog("VirtualAudioEngine[%p]::init()", this);

    FailIf(!super::init(properties),Done);
	
    // Do class-specific initialization here
    // If no non-hardware initialization is needed, this function can be removed
    
    number = OSDynamicCast(OSNumber, getProperty(NUM_BLOCKS_KEY));
    if (number) {
        numBlocks = number->unsigned32BitValue();
    } else {
        numBlocks = NUM_BLOCKS;
    }
    
    number = OSDynamicCast(OSNumber, getProperty(BLOCK_SIZE_KEY));
    if (number) {
        blockSize = number->unsigned32BitValue();
    } else {
        blockSize = BLOCK_SIZE;
    }
    
    result = true;
    
Done:

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioEngine::initHardware(IOService *provider)
{
    bool 				result = false;
    IOAudioSampleRate	initialSampleRate;
    IOWorkLoop*			wl;
    
    debug3IOLog("VirtualAudioEngine[%p]::initHardware(%p)", this, provider);
    
    FailIf (!super::initHardware(provider),Done);
	
    initialSampleRate.whole = 0;
    initialSampleRate.fraction = 0;

    FailIf(!createAudioStreams(&initialSampleRate),Done);
	
    FailIf (0 == initialSampleRate.whole,Done);
	
    blockTimeoutUS = 1000000 * blockSize / initialSampleRate.whole;
    
    setSampleRate(&initialSampleRate);
    
    // Set the number of sample frames in each buffer
    setNumSampleFramesPerBuffer(blockSize * numBlocks);
    
    wl = getWorkLoop();
    FailIf (!wl, Done);
    
    timerEventSource = IOTimerEventSource::timerEventSource(this, timerFired);
    
    FailIf(!timerEventSource,Done);
	
    workLoop->addEventSource(timerEventSource);
        
    result = true;
    
Done:

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioEngine::createAudioStreams(IOAudioSampleRate *initialSampleRate)
{
    bool result = false;
    OSNumber *number;
    UInt32 numStreams, streamNum;
    OSArray *formatArray, *sampleRateArray;
    UInt32 startingChannelID = 1;
    IOAudioControl *control;
    OSString *desc;
    
    desc = OSDynamicCast(OSString, getProperty(DESCRIPTION_KEY));
    if (desc) {
        setDescription(desc->getCStringNoCopy());
    }
    
    number = OSDynamicCast(OSNumber, getProperty(NUM_STREAMS_KEY));
    if (number) {
        numStreams = number->unsigned32BitValue();
    } else {
        numStreams = NUM_STREAMS;
    }
    
    formatArray = OSDynamicCast(OSArray, getProperty(FORMATS_KEY));
    FailIf(formatArray == NULL,Done);
    
    sampleRateArray = OSDynamicCast(OSArray, getProperty(SAMPLE_RATES_KEY));
    FailIf (sampleRateArray == NULL,Done);

    for (streamNum = 0; streamNum < numStreams; streamNum++) {
        IOAudioStream *inputStream = NULL, *outputStream = NULL;
        UInt32 maxBitWidth = 0;
        UInt32 maxNumChannels = 0;
        OSCollectionIterator *formatIterator = NULL, *sampleRateIterator = NULL;
        OSDictionary *formatDict;
        IOAudioSampleRate sampleRate;
        IOAudioStreamFormat initialFormat;
        bool initialFormatSet;
        UInt32 channelID;
        char outputStreamName[20], inputStreamName[20];
        
        initialFormatSet = false;
        
        sampleRate.whole = 0;
        sampleRate.fraction = 0;
                
        inputStream = new IOAudioStream;
        FailIf (inputStream == NULL,Error);
		
        outputStream = new IOAudioStream;
        FailIf (outputStream == NULL,Error);
        
        sprintf(inputStreamName, "Input Stream #%ld", streamNum + 1);
        sprintf(outputStreamName, "Output Stream #%ld", streamNum + 1);

        if (!inputStream->initWithAudioEngine(this, kIOAudioStreamDirectionInput, startingChannelID, inputStreamName) ||
            !outputStream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput, startingChannelID, outputStreamName)) {
            goto Error;
        }
        
        formatIterator = OSCollectionIterator::withCollection(formatArray);
        FailIf (!formatIterator,Error);
        
        sampleRateIterator = OSCollectionIterator::withCollection(sampleRateArray);
        FailIf (!sampleRateIterator,Error);
		
        
        formatIterator->reset();
        while (formatDict = (OSDictionary *)formatIterator->getNextObject()) {
            IOAudioStreamFormat format;
            
            FailIf (OSDynamicCast(OSDictionary, formatDict) == NULL,Error);
            
            FailIf (IOAudioStream::createFormatFromDictionary(formatDict, &format) == NULL,Error);
			
            
            if (!initialFormatSet) {
                initialFormat = format;
            }
            
            sampleRateIterator->reset();
            while (number = (OSNumber *)sampleRateIterator->getNextObject()) {
                FailIf (!OSDynamicCast(OSNumber, number),Error);
                
                sampleRate.whole = number->unsigned32BitValue();
                
                inputStream->addAvailableFormat(&format, &sampleRate, &sampleRate);
                if (format.fBitDepth == 24) {
                    IOAudioStream::AudioIOFunction functions[2];
                    functions[0] = process24BitSamples;
                    functions[1] = clip24BitSamples;
                    outputStream->addAvailableFormat(&format, &sampleRate, &sampleRate, functions, 2);
                    //outputStream->addAvailableFormat(&format, &sampleRate, &sampleRate, (IOAudioStream::AudioIOFunction)clip24BitSamples);
                } else if (format.fBitDepth == 16) {
                    IOAudioStream::AudioIOFunction functions[2];
                    functions[0] = process16BitSamples;
                    functions[1] = clip16BitSamples;
                    outputStream->addAvailableFormat(&format, &sampleRate, &sampleRate, functions, 2);
                    //outputStream->addAvailableFormat(&format, &sampleRate, &sampleRate, (IOAudioStream::AudioIOFunction)clip16BitSamples);
                } else {
                    outputStream->addAvailableFormat(&format, &sampleRate, &sampleRate);
                }
                
                if (format.fNumChannels > maxNumChannels) {
                    maxNumChannels = format.fNumChannels;
                }
                
                if (format.fBitWidth > maxBitWidth) {
                    maxBitWidth = format.fBitWidth;
                }
                
                if (initialSampleRate->whole == 0) {
                    initialSampleRate->whole = sampleRate.whole;
                }
            }
        }
        
        if (commonBuffer == NULL) {
            bufSize = blockSize * numBlocks * maxNumChannels * maxBitWidth / 8;
        
            commonBuffer = (void *)IOMalloc(bufSize);
            if (!commonBuffer) {
                debug2IOLog("Error allocating buffer - %lu bytes.", bufSize);
                goto Error;
            }
        }
        
        inputStream->setFormat(&initialFormat);
        outputStream->setFormat(&initialFormat);
        
        inputStream->setSampleBuffer(commonBuffer, bufSize);
        outputStream->setSampleBuffer(commonBuffer, bufSize);
        
        addAudioStream(inputStream);
		inputStream->setTerminalType (INPUT_NULL);
        inputStream->release();
        
        addAudioStream(outputStream);
		outputStream->setTerminalType (OUTPUT_NULL);
        outputStream->release();
        
        formatIterator->release();
        sampleRateIterator->release();
        
        for (channelID = startingChannelID; channelID < (startingChannelID + maxNumChannels); channelID++) {
            char channelName[20];
            
            sprintf(channelName, "Channel %lu", channelID);
            
            control = IOAudioLevelControl::createVolumeControl(65535, 0, 65535, (-22 << 16) + (32768), 0, channelID,   channelName, 0, kIOAudioControlUsageOutput);
            FailIf (!control,Error);
            
            control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::volumeChangeHandler, audioDevice);
            addDefaultAudioControl(control);
            control->release();
            
            control = IOAudioToggleControl::createMuteControl(false, channelID, channelName, 0,  kIOAudioControlUsageOutput);
			
            FailIf (!control,Error);
            
			control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::outputMuteChangeHandler, audioDevice);
            addDefaultAudioControl(control);
            control->release();
                                                                
            control = IOAudioLevelControl::createVolumeControl(65535, 0,  65535, (-22 << 16) + (32768), 0,  channelID,  channelName, 0, kIOAudioControlUsageInput);
            FailIf (!control,Error);
            
            control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::gainChangeHandler, audioDevice);
            addDefaultAudioControl(control);
            control->release();
            
            control = IOAudioToggleControl::createMuteControl(false, channelID,  channelName, 0, kIOAudioControlUsageInput);
            FailIf (!control,Error);
            
            control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::inputMuteChangeHandler, audioDevice);
            addDefaultAudioControl(control);
            control->release();
            
            control = IOAudioToggleControl::createMuteControl(true, channelID,  channelName,  0, kIOAudioControlUsagePassThru);
            FailIf (!control,Error);
            
            control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::passThruChangeHandler, audioDevice);
            addDefaultAudioControl(control);
            control->release();
        }
        
        startingChannelID += maxNumChannels;
        
        continue;

Error:

        debug2IOLog("VirtualAudioEngine[%p]::createAudioStreams() - ERROR", this);
    
        if (inputStream) {
            inputStream->release();
        }
        
        if (outputStream) {
            outputStream->release();
        }
        
        if (formatIterator) {
            formatIterator->release();
        }
        
        if (sampleRateIterator) {
            sampleRateIterator->release();
        }
        
        goto Done;
    }
    
    control = IOAudioLevelControl::createVolumeControl(65535, 0, 65535, (-22 << 16) + (32768), 0, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll,  0,   kIOAudioControlUsageOutput);
	FailIf (!control,Done);
    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::volumeChangeHandler, audioDevice);
    addDefaultAudioControl(control);
    control->release();
    
    control = IOAudioToggleControl::createMuteControl(false, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
	FailIf (!control,Done);
    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::outputMuteChangeHandler, audioDevice);
    addDefaultAudioControl(control);
    control->release();
                                                        
    control = IOAudioLevelControl::createVolumeControl(65535,
                                                        0,
                                                        65535,
                                                        (-22 << 16) + (32768),
                                                        0,
                                                        kIOAudioControlChannelIDAll,
                                                        kIOAudioControlChannelNameAll,
                                                        0,
                                                        kIOAudioControlUsageInput);
	FailIf (!control,Done);

    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::gainChangeHandler, audioDevice);
    addDefaultAudioControl(control);
    control->release();
    
    control = IOAudioToggleControl::createMuteControl(false,  kIOAudioControlChannelIDAll,   kIOAudioControlChannelNameAll, 0,  kIOAudioControlUsageInput);
	FailIf (!control,Done);

    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::inputMuteChangeHandler, audioDevice);
    addDefaultAudioControl(control);
    control->release();
    
    control = IOAudioToggleControl::createMuteControl(true,
                                                        kIOAudioControlChannelIDAll,
                                                        kIOAudioControlChannelNameAll,
                                                        0,
                                                        kIOAudioControlUsagePassThru);
	FailIf (!control,Done);

    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)VirtualAudioDevice::passThruChangeHandler, audioDevice);
    addDefaultAudioControl(control);
    control->release();

    result = true;
    
Done:

    if (!result) {
        debug2IOLog("VirtualAudioEngine[%p]::createAudioStreams() - failed!", this);
    }

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void VirtualAudioEngine::free()
{
    debug2IOLog("VirtualAudioEngine[%p]::free()", this);
    
    // We need to free our resources when we're going away
    
    if (commonBuffer) {
        IOFree(commonBuffer, bufSize);
        commonBuffer = NULL;
    }
    
    super::free();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void VirtualAudioEngine::stop(IOService *provider)
{
    debug3IOLog("VirtualAudioEngine[%p]::stop(%p)", this, provider);
    
    super::stop(provider);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioEngine::performAudioEngineStart()
{
	debug2IOLog("VirtualAudioEngine[%p]::performAudioEngineStart()", this);
	
	takeTimeStamp(false);
	currentBlock = 0;
	
	timerEventSource->setTimeoutUS(blockTimeoutUS);
	
	return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioEngine::performAudioEngineStop()
{
    debug2IOLog("VirtualAudioEngine[%p]::performAudioEngineStop()", this);
    
    timerEventSource->cancelTimeout();
    
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 VirtualAudioEngine::getCurrentSampleFrame()
{
    return currentBlock * blockSize;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    debug5IOLog("VirtualAudioEngine[%p]::peformFormatChange(%p, %p, %p)", this, audioStream, newFormat, newSampleRate);
    
    // It is possible that this function will be called with only a format or only a sample rate
    // We need to check for NULL for each of the parameters
    if (newFormat) {
        debug2IOLog("  -> %d bits per sample selected.", newFormat->fBitDepth);
    }
    
    if (newSampleRate) {
        debug2IOLog("  -> %ld Hz selected.", newSampleRate->whole);
    }
    
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void VirtualAudioEngine::timerFired(OSObject *target, IOTimerEventSource *sender)
{
    if (target) {
        VirtualAudioEngine *audioEngine = OSDynamicCast(VirtualAudioEngine, target);
        
        if (audioEngine) {
            audioEngine->currentBlock++;
            if (audioEngine->currentBlock >= audioEngine->numBlocks) {
                audioEngine->currentBlock = 0;
                audioEngine->takeTimeStamp();
            }
            
            sender->setTimeoutUS(audioEngine->blockTimeoutUS);
        }
    }
}

 /*
 *  AppleOnboardAudio.h
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Mon Jun 04 2001.
 *  Copyright (c) 2001 Apple Computer Inc. All rights reserved.
 *
 */
 
 
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "AppleOnboardAudio.h"

OSDefineMetaClassAndAbstractStructors(AppleOnboardAudio, IOAudioDevice)

#define super IOAudioDevice

#pragma mark +UNIX LIKE FUNCTIONS

bool AppleOnboardAudio::init(OSDictionary *properties)
{
    OSDictionary *AOAprop;
    DEBUG_IOLOG("+ AppleOnboardAudio::init\n");
    if (!super::init(properties)) return false;

    outMute =0;
    playthruToggle =0;
    outVolLeft =0;
    outVolRight =0;
    inGainLeft =0;
    inGainRight =0;
    inputSelector = 0;
        
        //globals for the driver
    gIsMute = false;
    gIsPlayThroughActive = false;
    gVolLeft =0;
    gVolRight =0;
    gGainLeft = 0;
    gGainRight = 0;
    gIsModemSoundActive = false;
    gHasModemSound = false;
    gLastInputSourceBeforeModem =0;
    gExpertMode = false;			//when off we are in a OS 9 like config. On we 
        
        //Port Handler like info
    AudioDetects  = 0;
    AudioOutputs  = 0;
    AudioInputs = 0;
    AudioSoftDSPFeatures = 0;
    driverDMAEngine = 0;
    
    currentDevices = 0xFFFF;
    theAudioPowerObject = 0;
    theAudioDeviceTreeParser = 0;
    
        //Future for creation
    if ( AOAprop = OSDynamicCast(OSDictionary, properties->getObject("AOAAttributes"))) {
        gHasModemSound = 
                (kOSBooleanTrue == OSDynamicCast(OSBoolean, AOAprop->getObject("analogModem")));
    }
        
    CLOG("- AppleOnboardAudio::init\n");
    return true;
}

void AppleOnboardAudio::free()
{
    DEBUG_IOLOG("+ AppleOnboardAudio::free\n");
    
    if(driverDMAEngine)
        driverDMAEngine->release();
    CLEAN_RELEASE(outMute);
    CLEAN_RELEASE(playthruToggle);
    CLEAN_RELEASE(outVolLeft);
    CLEAN_RELEASE(outVolRight);
    CLEAN_RELEASE(inGainLeft);
    CLEAN_RELEASE(inGainRight);
    CLEAN_RELEASE(inputSelector);
    CLEAN_RELEASE(theAudioPowerObject);
    CLEAN_RELEASE(AudioDetects);
    CLEAN_RELEASE(AudioOutputs);
    CLEAN_RELEASE(AudioInputs);
    CLEAN_RELEASE(theAudioDeviceTreeParser);

	if (NULL != powerMgrIntEventSource)
		workLoop->removeEventSource (powerMgrIntEventSource);

	publishResource ("setModemSound", NULL);
    super::free();
    DEBUG_IOLOG("- AppleOnboardAudio::free, (void)\n");
}

IOService* AppleOnboardAudio::probe(IOService* provider, SInt32* score)
{

    DEBUG_IOLOG("+ AppleOnboardAudio::probe\n");
    super::probe(provider, score);
    DEBUG_IOLOG("- AppleOnboardAudio::probe\n");
    return (0);
}

OSArray *AppleOnboardAudio::getDetectArray(){
    return(AudioDetects);
}

bool AppleOnboardAudio::getMuteState(){
    return(gIsMute);
}

void AppleOnboardAudio::setMuteState(bool newMuteState){
    outMute->setValue(newMuteState);
}

#pragma mark +PORT HANDLER FUNCTIONS

IOReturn AppleOnboardAudio::configureAudioOutputs(IOService *provider) {
    IOReturn result = kIOReturnSuccess;   
    AudioHardwareOutput *theOutput;
    UInt16 idx;

    DEBUG_IOLOG("+ AppleOnboardAudio::configureAudioOutputs\n");
    if(!theAudioDeviceTreeParser) 
        goto BAIL;

    AudioOutputs = theAudioDeviceTreeParser->createOutputsArray();
    if(!AudioOutputs)
        goto BAIL;
    
    for(idx = 0; idx < AudioOutputs->getCount(); idx++) {
        theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(idx));
        if( theOutput) theOutput->attachAudioPluginRef((AppleOnboardAudio *) this);       
    }
    
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::configureAudioOutputs, %d\n", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn AppleOnboardAudio::configureAudioDetects(IOService *provider) {
    IOReturn result = kIOReturnSuccess;   
    DEBUG_IOLOG("+ AppleOnboardAudio::configureAudioDetects\n");
    
    if(!theAudioDeviceTreeParser) 
        goto BAIL;

    AudioDetects = theAudioDeviceTreeParser->createDetectsArray();

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::configureAudioDetects, %d \n", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::configureAudioInputs(IOService *provider) {
    IOReturn result = kIOReturnSuccess; 
    UInt16 idx;  
    AudioHardwareInput *theInput;
    AudioHardwareMux *theMux;
    DEBUG_IOLOG("+ AppleOnboardAudio::configureAudioDetects\n");

    FailIf (NULL == theAudioDeviceTreeParser, BAIL);

    AudioInputs = theAudioDeviceTreeParser->createInputsArrayWithMuxes();

	FailIf (NULL == AudioInputs, BAIL);

    for(idx = 0; idx < AudioInputs->getCount(); idx++) {
        theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
        if (NULL != theInput) {
			theInput->attachAudioPluginRef((AppleOnboardAudio *) this);       
        } else {
			theMux = OSDynamicCast(AudioHardwareMux, AudioInputs->getObject(idx));
			if (NULL != theMux) {
				theMux->attachAudioPluginRef((AppleOnboardAudio *) this);
			} else {
				DEBUG_IOLOG ("!!!It's not an input and it's not a mux!!!\n");
			}
        }
    }

EXIT:
    DEBUG2_IOLOG("- %d = AppleOnboardAudio::configureAudioDetects\n", result);
    return (result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn AppleOnboardAudio::parseAndActivateInit(IOService *provider){
    IOReturn result = kIOReturnSuccess;   
    SInt16 initType = 0;
    DEBUG_IOLOG("+ AppleOnboardAudio::parseAndActivateInit\n");
    
    if(!theAudioDeviceTreeParser) 
        goto BAIL;
    
    initType = theAudioDeviceTreeParser->getInitOperationType();

    if(2 == initType) 
        sndHWSetProgOutput(kSndHWProgOutput0);
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::parseAndActivateInit, %d\n", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


UInt32 AppleOnboardAudio::getCurrentDevices(){
    return(currentDevices);
}

void AppleOnboardAudio::setCurrentDevices(UInt32 devices){
    UInt32 odevice;
    if(devices != currentDevices) {
        odevice = currentDevices;
        currentDevices = devices;
        changedDeviceHandler(odevice);
    }
    
    // if this CPU has a phase inversion feature see if we need to enable phase inversion    
    if (fCPUNeedsPhaseInversion)
    {
        bool state;
        
        if (currentDevices == 0 || currentDevices & kSndHWInternalSpeaker) // may only need the kSndHWInternalSpeaker check
            state = true;
        else
            state = false;
        
         driverDMAEngine->setPhaseInversion(state);
    }
}

void AppleOnboardAudio::changedDeviceHandler(UInt32 olddevices){
    UInt16 i;
    AudioHardwareOutput *theOutput;
//    AudioHardwareInput  *theInput;
    
    if(AudioOutputs) {
        for(i = 0; i< AudioOutputs->getCount(); i++) {
            theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(i));
            if( theOutput) theOutput->deviceIntService(currentDevices);
        }
    } 
       
        //Now that we have an input selector, it works through it
    /*if(AudioInputs) {
        for(i = 0; i< AudioInputs->getCount(); i++) {
            theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(i));
            if( theInput) theInput->deviceSetActive(currentDevices);
        }
    }*/
}

#pragma mark +IOAUDIO INIT
bool AppleOnboardAudio::initHardware(IOService *provider){
	IOWorkLoop				*workLoop;
    bool					result;

    DEBUG_IOLOG("+ AppleOnboardAudio::initHardware\n");

	result = TRUE;
    if (!super::initHardware(provider)) {
        goto BAIL;
    }

	ourPowerState = kIOAudioDeviceActive;
    sndHWInitialize(provider);
    theAudioDeviceTreeParser = AudioDeviceTreeParser::createWithEntryProvider(provider);
 
    setManufacturerName("Apple");
    setDeviceName("Built-in audio controller");

    parseAndActivateInit(provider);
    configureAudioDetects(provider);
    configureAudioOutputs(provider);
    configureAudioInputs(provider);
    configurePowerObject(provider);

    configureDMAEngines(provider);

	// Have to create the audio controls before calling activateAudioEngine
    createDefaultsPorts(); 

    if (kIOReturnSuccess != activateAudioEngine(driverDMAEngine)){
        driverDMAEngine->release();
        goto BAIL;
    }

	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, BAIL);

	// Make an interrupt event source to service Power Manager requests
	powerMgrIntEventSource = IOInterruptEventSource::interruptEventSource (this, PowerMgrInterruptHandler);
	FailIf (NULL == powerMgrIntEventSource, BAIL);
	workLoop->addEventSource (powerMgrIntEventSource);
	powerMgrIntEventSource->enable ();

	// Give drivers a chance to do something after the DMA engine and IOAudioFamily have been created/started
	sndHWPostDMAEngineInit (provider);

	// Set the default volume to that stored in the PRAM in case we don't get a setValue call from the Sound prefs before being activated.
	if (NULL != outVolLeft && NULL != outVolRight) {
		outVolLeft->setValue (PRAMToVolumeValue ());
		outVolRight->setValue (PRAMToVolumeValue ());
	}

    flushAudioControls();

	// Install power change handler so we get notified about shutdown
	registerPrioritySleepWakeInterest (&sysPowerDownHandler, this, 0);

	// Tell the world about us so the User Client can find us.
	registerService();

    publishResource("setModemSound", this);
EXIT:
    DEBUG_IOLOG("- AppleOnboardAudio::initHardware\n"); 
    return(result);
BAIL:
    result = FALSE;
    goto EXIT;
}

IOReturn AppleOnboardAudio::configureDMAEngines(IOService *provider){
    IOReturn result = kIOReturnSuccess;
    bool hasInput;
    
	// All this config should go in a single method
    if(!theAudioDeviceTreeParser)
        goto BAIL;                        

    if (theAudioDeviceTreeParser->getNumberOfInputs() > 0)
        hasInput = true;
    else 
        hasInput = false;
        
    driverDMAEngine = new AppleDBDMAAudioDMAEngine;
    // make sure we get an engine
    FailIf(NULL == driverDMAEngine,BAIL);
    //tell the uber class if it has to worry about reversing the phase of a channel
    fCPUNeedsPhaseInversion = theAudioDeviceTreeParser->getPhaseInversion();
    
    // it will be set when the device polling starts
    driverDMAEngine->setPhaseInversion(false);
    
    if (!driverDMAEngine->init(0, provider, this, hasInput)) 
    {
        driverDMAEngine->release();
        goto BAIL;
    }
    
EXIT:
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::createDefaultsPorts() {

    IOAudioPort *outputPort = 0;
    IOAudioPort *inputPort = 0;
    IOReturn result;
    OSDictionary *AOAprop = 0, *theRange = 0;
    OSNumber *theNumber;
    OSData *theData;
    SInt32 OutminLin, OutmaxLin, InminLin, InmaxLin; 
    IOFixed OutminDB, OutmaxDB, InminDB, InmaxDB;
    UInt32 idx;

    DEBUG_IOLOG("+ AppleOnboardAudio::createDefaultsPorts\n");

	result = kIOReturnSuccess;
	FailIf (NULL == driverDMAEngine, BAIL);
	FailIf (NULL == (AOAprop = OSDynamicCast(OSDictionary, this->getProperty("AOAAttributes"))), BAIL);

    /*
     * Create out part port : 2 level (one for each side and one mute)
     */
	if (NULL != (theRange= OSDynamicCast(OSDictionary, AOAprop->getObject("RangeOut")))) {
		outputPort = IOAudioPort::withAttributes(kIOAudioPortTypeOutput, "Main Output port");
		if (NULL != outputPort) {
			theNumber = OSDynamicCast(OSNumber, theRange->getObject("minLin"));
			OutminLin = (SInt32) theNumber->unsigned32BitValue();
			theNumber = OSDynamicCast(OSNumber, theRange->getObject("maxLin"));
			OutmaxLin = (SInt32) theNumber->unsigned32BitValue();
			theData = OSDynamicCast(OSData, theRange->getObject("minLog"));
			OutminDB = *((IOFixed*) theData->getBytesNoCopy());
			theData = OSDynamicCast(OSData, theRange->getObject("maxLog"));
			OutmaxDB = *((IOFixed*) theData->getBytesNoCopy());
			
			fMaxVolume = OutmaxLin;
			fMinVolume = OutminLin;
			
			outVolLeft = IOAudioLevelControl::createVolumeControl(OutmaxLin, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultLeft,
												kIOAudioControlChannelNameLeft,
												kOutVolLeft, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolLeft) {
				driverDMAEngine->addDefaultAudioControl(outVolLeft);
				outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeLeftChangeHandler, this);
			}
		
			outVolRight = IOAudioLevelControl::createVolumeControl(OutmaxLin, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultRight,
												kIOAudioControlChannelNameRight,
												kOutVolRight, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolRight) {
				driverDMAEngine->addDefaultAudioControl(outVolRight);
				outVolRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeRightChangeHandler, this);
			}
			
			outMute = IOAudioToggleControl::createMuteControl(false,
											kIOAudioControlChannelIDAll,
											kIOAudioControlChannelNameAll,
											kOutMute, 
											kIOAudioControlUsageOutput);
			if (NULL != outMute) {
				driverDMAEngine->addDefaultAudioControl(outMute);
				outMute->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputMuteChangeHandler, this);
			}

			attachAudioPort(outputPort, driverDMAEngine, 0);
			outputPort->release();
		}
	}

    /*
     * Create input port and level controls (if any) associated to it
     */
    if ((theAudioDeviceTreeParser->getNumberOfInputs() > 0)) {
		if (NULL != (theRange= OSDynamicCast(OSDictionary, AOAprop->getObject("RangeIn")))) {
			inputPort = IOAudioPort::withAttributes(kIOAudioPortTypeInput, "Main Input Port");
			if (NULL != inputPort) {
				theNumber = OSDynamicCast(OSNumber, theRange->getObject("minLin"));
				InminLin = (SInt32) theNumber->unsigned32BitValue();
				theNumber = OSDynamicCast(OSNumber, theRange->getObject("maxLin"));
				InmaxLin = (SInt32) theNumber->unsigned32BitValue();
				theData = OSDynamicCast(OSData, theRange->getObject("minLog"));
				InminDB = *((IOFixed*) theData->getBytesNoCopy());
				theData = OSDynamicCast(OSData, theRange->getObject("maxLog"));
				InmaxDB = *((IOFixed*) theData->getBytesNoCopy());
			
				inGainLeft = IOAudioLevelControl::createVolumeControl((InmaxLin-InminLin)/2, InminLin, InmaxLin, InminDB, InmaxDB,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kInGainLeft, 
													kIOAudioControlUsageInput);
				if (NULL != inGainLeft) {
					driverDMAEngine->addDefaultAudioControl(inGainLeft);
					inGainLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainRightChangeHandler, this);
				}
			
				inGainRight = IOAudioLevelControl::createVolumeControl((InmaxLin-InminLin)/2, InminLin, InmaxLin, InminDB, InmaxDB,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kInGainRight, 
													kIOAudioControlUsageInput);
				if (NULL != inGainRight) {
					driverDMAEngine->addDefaultAudioControl(inGainRight);
					inGainRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainRightChangeHandler, this);
				}

				attachAudioPort(inputPort, 0, driverDMAEngine);
				inputPort->release();
			}
		}

		// create the input selectors
		if(AudioInputs) {
			AudioHardwareInput *theInput;
			UInt32				inputType;

			inputSelector = NULL;
			for(idx = 0; idx < AudioInputs->getCount(); idx++) {
				theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
				if(theInput) {
					inputType = theInput->getInputPortType();
					DEBUG2_IOLOG ("Creating input selector of type %4s\n", (char*)&inputType);
					if (NULL == inputSelector && 'none' != inputType) {
						inputSelector = IOAudioSelectorControl::createInputSelector(inputType,
																		kIOAudioControlChannelIDAll,
																		kIOAudioControlChannelNameAll,
																		kInputSelector);
						if (NULL != inputSelector) {
							driverDMAEngine->addDefaultAudioControl(inputSelector);
							inputSelector->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputSelectorChangeHandler, this);	
						}
					}

					if (NULL != inputSelector) {
						DEBUG2_IOLOG ("Calling addAvailableSelection with type %4s\n", (char*)&inputType);
						switch(inputType) {
							case 'imic' :
								inputSelector->addAvailableSelection('imic', "Internal microphone");
								break;
							case 'emic' :
								inputSelector->addAvailableSelection('emic', "External microphone/Line In");
								break;
							case 'sinj' :
								inputSelector->addAvailableSelection('sinj', "Sound Input");
								break;
							case 'line' :
								inputSelector->addAvailableSelection('line', "Line In");
								break;
							case 'zvpc' :
								inputSelector->addAvailableSelection('zvpc', "Zoomed Video");
								break;
							default:
								break;
						}
					}
				}
			}
		}

		if (NULL != outputPort) {
			playthruToggle = IOAudioToggleControl::createMuteControl(true,
												kIOAudioControlChannelIDAll,
												kIOAudioControlChannelNameAll,
												kPassThruToggle, 
												kIOAudioControlUsagePassThru);
		
			if (NULL != playthruToggle) {
				driverDMAEngine->addDefaultAudioControl(playthruToggle);
				playthruToggle->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)passThruChangeHandler, this);
			}
		}
	}

EXIT:    
    DEBUG2_IOLOG("- %d = AppleOnboardAudio::createDefaultsPorts\n", result);
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

#pragma mark +IOAUDIO CONTROL HANDLERS

IORegistryEntry * AppleOnboardAudio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FAIL_IF (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FAIL_IF (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
				theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

IOReturn AppleOnboardAudio::volumeLeftChangeHandler(IOService *target, 
                    IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;

    DEBUG_IOLOG("+ AppleOnboardAudio::volumeLeftChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    result = audioDevice->volumeLeftChanged(volumeControl, oldValue, newValue);

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::volumeLeftChangeHandler, %d\n", 
                                                (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::volumeLeftChanged(IOAudioControl *volumeControl, 
                    SInt32 oldValue, SInt32 newValue){
                    
    IOReturn result = kIOReturnSuccess;
    AudioHardwareOutput *theOutput;
    UInt16 idx;
    SInt32			newiSubVolume;
    IOAudioLevelControl *	iSubLeftVolume;
    IORegistryEntry *		start;

    DEBUG_IOLOG("+ AppleOnboardAudio::volumeLeftChanged\n");
    if(!AudioOutputs)
        goto BAIL;
    
    gVolLeft = newValue;
    for(idx = 0; idx< AudioOutputs->getCount(); idx++) {
        theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(idx));
        if( theOutput) theOutput->setVolume(newValue, gVolRight);
    }

    start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
    FAIL_IF (NULL == start, Exit);

    iSubLeftVolume = (IOAudioLevelControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioLevelControl", kIOAudioControlSubTypeKey, 'subL');
    start->release ();

    FAIL_IF (NULL == iSubLeftVolume, Exit);
    newiSubVolume = (newValue * 60) / ((IOAudioLevelControl *)volumeControl)->getMaxValue ();
    iSubLeftVolume->setValue (newiSubVolume);
    iSubLeftVolume->release();

Exit:
    if(!gExpertMode)				//We do that only if we are on a OS 9 like UI guideline
        WritePRAMVol(gVolLeft,gVolRight);
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::volumeLeftChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;

}

    
IOReturn AppleOnboardAudio::volumeRightChangeHandler(IOService *target, 
            IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;

    DEBUG_IOLOG("+ AppleOnboardAudio::volumeRightChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    result = audioDevice->volumeRightChanged(volumeControl, oldValue, newValue);

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::volumeRightChangeHandler, %d\n", 
                                                    (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::volumeRightChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AudioHardwareOutput *theOutput;
    UInt16 idx;
	SInt32					newiSubVolume;
	IOAudioLevelControl *	iSubRightVolume;
	IORegistryEntry *		start;

    DEBUG_IOLOG("+ AppleOnboardAudio::volumeRightChanged\n");
    if(!AudioOutputs)
        goto BAIL;
    
    gVolRight = newValue;
    for(idx = 0; idx< AudioOutputs->getCount(); idx++) {
        theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(idx));
        if( theOutput) theOutput->setVolume(gVolLeft, newValue);
    }

    start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
    FAIL_IF (NULL == start, Exit);

    iSubRightVolume = (IOAudioLevelControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioLevelControl", kIOAudioControlSubTypeKey, 'subR');
    start->release ();

    FAIL_IF (NULL == iSubRightVolume, Exit);
    newiSubVolume = (newValue * 60) / ((IOAudioLevelControl *)volumeControl)->getMaxValue ();
    iSubRightVolume->setValue (newiSubVolume);
    iSubRightVolume->release();


Exit:
    if(!gExpertMode)				//We do that only if we are on a OS 9 like UI guideline
        WritePRAMVol(gVolLeft,gVolRight);
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::volumeRightChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

    
IOReturn AppleOnboardAudio::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;

    DEBUG_IOLOG("+ AppleOnboardAudio::outputMuteChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::outputMuteChangeHandler, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn AppleOnboardAudio::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    UInt32 idx;
    AudioHardwareOutput * theOutput;
    IOAudioToggleControl *	iSubMute;
    IORegistryEntry *		start;

    DEBUG_IOLOG("+ AppleOnboardAudio::outputMuteChanged\n");
                //pass it to the AudioHardwareOutputObjects
    if(!AudioOutputs)
        goto BAIL;
        
    gIsMute = newValue;
    for(idx = 0; idx< AudioOutputs->getCount(); idx++) {
        theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(idx));
        if( theOutput) theOutput->setMute(newValue);
    }
    
    if(!gExpertMode) {		
        if (newValue)
            WritePRAMVol(0,0);
        else
            WritePRAMVol(gVolLeft,gVolRight);
    }
    
    start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
    FAIL_IF (NULL == start, EXIT);
    iSubMute = (IOAudioToggleControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioMuteControl", kIOAudioControlSubTypeKey, 'subM');
    start->release ();

    FAIL_IF (NULL == iSubMute, EXIT);
    iSubMute->setValue (newValue);
    iSubMute->release();
    
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::outputMuteChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;

}


IOReturn AppleOnboardAudio::gainLeftChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;
    DEBUG_IOLOG("_ AppleOnboardAudio::gainLeftChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    
    result = audioDevice->gainLeftChanged(gainControl, oldValue, newValue);

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::gainLeftChangeHandler, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::gainLeftChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
 IOReturn result = kIOReturnSuccess;
    UInt32 idx;
    AudioHardwareInput *theInput;
    DEBUG_IOLOG("+ AppleOnboardAudio::gainLeftChanged\n");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainLeft = newValue;
    for(idx = 0; idx< AudioInputs->getCount(); idx++) {
        theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
        if( theInput) theInput->setInputGain(newValue, gGainRight);
    }

EXIT:    
    DEBUG2_IOLOG("- AppleOnboardAudio::gainLeftChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn AppleOnboardAudio::gainRightChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;

    DEBUG_IOLOG("+ AppleOnboardAudio::gainRightChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    result = audioDevice->gainRightChanged(gainControl, oldValue, newValue);
    
EXIT:
    DEBUG_IOLOG("- AppleOnboardAudio::gainRightChangeHandler\n");
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::gainRightChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
     IOReturn result = kIOReturnSuccess;
    UInt32 idx;
    AudioHardwareInput *theInput;
    DEBUG_IOLOG("+ AppleOnboardAudio::gainRightChanged\n");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainRight = newValue;
    for(idx = 0; idx< AudioInputs->getCount(); idx++) {
        theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
        if( theInput) theInput->setInputGain(gGainLeft, newValue);
    }

EXIT:    
    DEBUG2_IOLOG("- AppleOnboardAudio::gainRightChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;
    DEBUG_IOLOG("+ AppleOnboardAudio::passThruChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    
    if(!audioDevice) goto BAIL;
    result = audioDevice->passThruChanged(passThruControl, oldValue, newValue);
    
EXIT:
    DEBUG_IOLOG("- AppleOnboardAudio::passThruChangeHandler\n");
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue){
    
    IOReturn result = kIOReturnSuccess;
    DEBUG_IOLOG("+ AppleOnboardAudio::passThruChanged\n");
    gIsPlayThroughActive = newValue;
    sndHWSetPlayThrough(!newValue);
    DEBUG_IOLOG("- AppleOnboardAudio::passThruChanged\n");
    return result;
}

IOReturn AppleOnboardAudio::inputSelectorChangeHandler(IOService *target, IOAudioControl *inputSelector, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;
    DEBUG_IOLOG("+ AppleOnboardAudio::inputSelectorChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    
    if(!audioDevice) goto BAIL;
    result = audioDevice->inputSelectorChanged(inputSelector, oldValue, newValue);
    
EXIT:
    DEBUG_IOLOG("- AppleOnboardAudio::inputSelectorChangeHandler\n");
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn AppleOnboardAudio::inputSelectorChanged(IOAudioControl *inputSelector, SInt32 oldValue, SInt32 newValue){
    
    AudioHardwareInput *theInput;
    UInt32 idx;
    IOReturn result = kIOReturnSuccess;
    
    DEBUG_IOLOG("+ AppleOnboardAudio::inputSelectorChanged\n");
    if(AudioInputs) {
        for(idx = 0; idx< AudioInputs->getCount(); idx++) {
            theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
            if( theInput) theInput->forceActivation(newValue);
        }
    }  
    DEBUG_IOLOG("- AppleOnboardAudio::inputSelectorChanged\n");
    return result;
}


#pragma mark +POWER MANAGEMENT
IOReturn AppleOnboardAudio::configurePowerObject(IOService *provider){
    IOReturn result = kIOReturnSuccess;

    DEBUG_IOLOG("+ AppleOnboardAudio::configurePowerObject\n");
    switch (theAudioDeviceTreeParser->getPowerObjectType()) {
        case kProj6PowerObject:
            theAudioPowerObject = AudioProj6PowerObject::createAudioProj6PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case kProj7PowerObject:
            theAudioPowerObject = AudioProj7PowerObject::createAudioProj7PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case kProj8PowerObject:
            theAudioPowerObject = AudioProj8PowerObject::createAudioProj8PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case kProj10PowerObject:
            theAudioPowerObject = AudioProj10PowerObject::createAudioProj10PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case kProj14PowerObject:
            theAudioPowerObject = AudioProj14PowerObject::createAudioProj14PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case kProj16PowerObject:
            theAudioPowerObject = AudioProj16PowerObject::createAudioProj16PowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
        case  kBasePowerObject:
        default:     //basic mute
            theAudioPowerObject = AudioPowerObject::createAudioPowerObject(this);
            if(!theAudioPowerObject) goto BAIL;
            break;
    }

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::configurePowerObject result = %d\n", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

void AppleOnboardAudio::GoIdlePower (void) {
	if (NULL != theAudioPowerObject) {
		if (TRUE == theAudioPowerObject->wantsIdleCalls () && kIOAudioDeviceActive == ourPowerState) {
			ourPowerState = kIOAudioDeviceIdle;
			theAudioPowerObject->setIdlePowerState ();
		} else if (TRUE == shuttingDown) {
			theAudioPowerObject->setIdlePowerState ();
		}
	}
}

void AppleOnboardAudio::GoFullPower (void) {
	if (NULL != theAudioPowerObject && TRUE == theAudioPowerObject->wantsIdleCalls () && kIOAudioDeviceActive != ourPowerState) {
		ourPowerState = kIOAudioDeviceActive;
		theAudioPowerObject->setFullPowerState ();
	}
}

// This function is called on the workloop when performPowerStateChange calls powerMgrIntEventSource->interruptOccurred
void AppleOnboardAudio::PowerMgrInterruptHandler (OSObject *owner, IOInterruptEventSource *source, int count) {
	AppleOnboardAudio *			appleOnboardAudio;

	debugIOLog ("+ AppleOnboardAudio::PowerMgrInterruptHandler\n");

	appleOnboardAudio = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf (!appleOnboardAudio, EXIT);

	debug3IOLog ("oldPowerState = %d, newPowerState = %d\n", appleOnboardAudio->fOldPowerState, appleOnboardAudio->fNewPowerState);

	if (appleOnboardAudio->fNewPowerState != appleOnboardAudio->fOldPowerState) {
		switch (appleOnboardAudio->fNewPowerState) {
			case kIOAudioDeviceSleep:
				appleOnboardAudio->theAudioPowerObject->setHardwarePowerOff ();
				break;
			case kIOAudioDeviceActive:
				appleOnboardAudio->theAudioPowerObject->setHardwarePowerOn ();
				break;
			default:
				;
		}
	}

EXIT:
    debugIOLog ("- AppleOnboardAudio::PowerMgrInterruptHandler\n");
    return;
}

IOReturn AppleOnboardAudio::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                        IOAudioDevicePowerState newPowerState,
                                                        UInt32 *microsecondsUntilComplete)
{
	IOReturn				result;

    debug3IOLog ("+ AppleOnboardAudio::performPowerStateChange (%d, %d)\n", oldPowerState, newPowerState);

	// We don't deal with idle requests from the family because it sends us one right after it sends an audio stop
	if (ourPowerState != newPowerState && (kIOAudioDeviceActive == newPowerState || kIOAudioDeviceSleep == newPowerState)) {
		fOldPowerState = oldPowerState;
		fNewPowerState = newPowerState;
		ourPowerState = newPowerState;

		*microsecondsUntilComplete = theAudioPowerObject->GetTimeToChangePowerState (oldPowerState, newPowerState);
		result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);

		powerMgrIntEventSource->interruptOccurred (NULL, NULL, NULL);
	} else {
		result = kIOReturnSuccess;
	}

	debugIOLog ("- AppleOnboardAudio::performPowerStateChange\n");

	return result;
}

//********************************************************************************
//
// Receives a notification when the RootDomain changes state. 
//
// Allows us to take action on system sleep, power down, and restart after
// applications have received their power change notifications and replied,
// but before drivers have powered down. We tell the device to go to sleep for a 
// silent shutdown on P80 and DACA.
//*********************************************************************************
IOReturn AppleOnboardAudio::sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize) {
	AppleOnboardAudio *				appleOnboardAudio;
	IOReturn						result;
//	char							message[100];

	result = kIOReturnUnsupported;
	appleOnboardAudio = OSDynamicCast (AppleOnboardAudio, (OSObject *)target);
	FailIf (NULL == appleOnboardAudio, Exit);

	switch (messageType) {
		case kIOMessageSystemWillPowerOff:
		case kIOMessageSystemWillRestart:
			// Interested applications have been notified of an impending power
			// change and have acked (when applicable).
			// This is our chance to save whatever state we can before powering
			// down.
//			Debugger ("about to shut down the hardware");
			appleOnboardAudio->shuttingDown = TRUE;
			appleOnboardAudio->GoIdlePower ();
			result = kIOReturnSuccess;
			break;
		default:
//			sprintf (message, "unknown selector %lx", messageType);
//			Debugger (message);
			break;
	}

Exit:
	return result;
}


#pragma mark +MODEM SOUND
IOReturn AppleOnboardAudio::setModemSound(bool state){
    IOReturn result = kIOReturnSuccess;
    AudioHardwareInput *theInput;
    UInt32 idx;
    
    DEBUG_IOLOG("+ AppleOnboardAudio::setModemSound\n");

    theInput = NULL;
    if(gIsModemSoundActive == state) 
        goto EXIT;
	
    if(state) {
        //we turn the modem on that is : find the active source, switch to modem
        //turn playthrough on
        
        if(AudioInputs) {
            for(idx = 0; idx< AudioInputs->getCount(); idx++) {
                theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
                if (NULL != theInput) {
                    theInput->forceActivation('modm');
                    theInput->setInputGain(0,0);
                }
            }
        }  
        
        sndHWSetPlayThrough(true);
    } else {
        //we turn the modem off : turn playthrough off, switch to saved source;
        sndHWSetPlayThrough(!gIsPlayThroughActive);
        if(AudioInputs) {
            for(idx = 0; idx< AudioInputs->getCount(); idx++) {
                theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
                if (NULL != theInput){
                    theInput->forceActivation(inputSelector->getIntValue());
                    theInput->setInputGain(gGainLeft, gGainRight);
                }
            }
        }
    }

    gIsModemSoundActive = state;
EXIT:
    DEBUG_IOLOG("- AppleOnboardAudio::setModemSound\n");
    return result;
}

IOReturn AppleOnboardAudio::callPlatformFunction( const OSSymbol * functionName, bool
            waitForFunction,void *param1, void *param2, void *param3, void *param4 ){
    
    DEBUG_IOLOG("+ AppleOnboardAudio::callPlatformFunction\n");
    if(functionName->isEqualTo("setModemSound")) {
        return(setModemSound((bool)param1));
    }    
    
    DEBUG_IOLOG("- AppleOnboardAudio::callPlatformFunction\n");
    return(super::callPlatformFunction(functionName,
            waitForFunction,param1, param2, param3, param4));
}

#pragma mark +PRAM VOLUME
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
UInt8 AppleOnboardAudio::VolumeToPRAMValue (UInt32 leftVol, UInt32 rightVol) {
	UInt32			pramVolume;						// Volume level to store in PRAM
	UInt32 			averageVolume;					// summed volume
    const UInt32 	volumeRange = (fMaxVolume - fMinVolume+1);
    UInt32 			volumeSteps;

	averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
    volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume
    pramVolume = averageVolume / volumeSteps;    

	// Since the volume level in PRAM is only worth three bits,
	// we round small values up to 1. This avoids SysBeep from
	// flashing the menu bar when it thinks sound is off and
	// should really just be very quiet.

	if ((pramVolume == 0) && (leftVol != 0 || rightVol !=0 ))
		pramVolume = 1;

	return (pramVolume & 0x07);
}

UInt32 AppleOnboardAudio::PRAMToVolumeValue (void) {
	const UInt32 	volumeRange = (fMaxVolume - fMinVolume + 1);
	UInt32 			volumeSteps;

	volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume

	return (volumeSteps * ReadPRAMVol ());
}

void AppleOnboardAudio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	UInt8						pramVolume;
	UInt8 						curPRAMVol;
	IODTPlatformExpert * 		platform;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
    
    debug3IOLog("AppleOnboardAudio::WritePRAMVol leftVol=%lu, rightVol=%lu\n",leftVol,  rightVol);
    
    if (platform) {
		pramVolume = VolumeToPRAMValue(leftVol,rightVol);
		
		// get the old value to compare it with
		platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		
		// Update only if there is a change
		if (pramVolume != (curPRAMVol & 0x07)) {
			// clear bottom 3 bits of volume control byte from PRAM low memory image
			curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
            debug2IOLog("AppleOnboardAudio::WritePRAMVol curPRAMVol=0x%x\n",curPRAMVol);

			// write out the volume control byte to PRAM
			platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
		}
	}
}

UInt8 AppleOnboardAudio::ReadPRAMVol (void) {
	UInt8						curPRAMVol;
	IODTPlatformExpert * 		platform;

	curPRAMVol = 0;
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());

    if (platform) {
		platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		curPRAMVol &= 0x07;
	}

	return curPRAMVol;
}

#if 0
//===========================================================================================================================
//	newUserClient
//===========================================================================================================================

IOReturn AppleOnboardAudio::newUserClient( task_t 			inOwningTask,
										 void *				inSecurityID,
										 UInt32 			inType,
										 IOUserClient **	outHandler )
{
	#pragma unused( inType )
	
    IOReturn 			err;
    IOUserClient *		userClientPtr;
    bool				result;
	
	debug2IOLog( "creating user client for task 0x%08lX\n", ( UInt32 ) inOwningTask );
	
	// Create the user client object.
	
	err = kIOReturnNoMemory;
	userClientPtr = AOAUserClient::Create( this, inOwningTask );
	FailIf (!userClientPtr, exit);
    
	// Set up the user client.
	
	err = kIOReturnError;
	result = userClientPtr->attach( this );
	FailIf (!result, exit);

	result = userClientPtr->start( this );
	FailIf (!result, exit);
	
	// Success.
	
    *outHandler = userClientPtr;
	err = kIOReturnSuccess;
	
exit:
	debug2IOLog( "newUserClient done (err=%d)\n", err );
	if( err != kIOReturnSuccess )
	{
		if( userClientPtr )
		{
			userClientPtr->detach( this );
			userClientPtr->release();
		}
	}
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	Static member variables
//===========================================================================================================================

///
/// Method Table
///

const IOExternalMethod		AOAUserClient::sMethods[] =
{
	//	Shutdown
	{
		NULL,												// object
		( IOMethod ) &AOAUserClient::Shutdown,				// func
		kIOUCScalarIScalarO,								// flags
		0,													// count of input parameters
		0													// count of output parameters
	}
};

const IOItemCount		AOAUserClient::sMethodCount = sizeof( AOAUserClient::sMethods ) / 
																  sizeof( AOAUserClient::sMethods[ 0 ] );

OSDefineMetaClassAndStructors( AOAUserClient, IOUserClient )

//===========================================================================================================================
//	Create
//===========================================================================================================================

AOAUserClient *	AOAUserClient::Create( AppleOnboardAudio *inDriver, task_t inTask )
{
    AOAUserClient *		userClient;
    
    userClient = new AOAUserClient;
	if( !userClient )
	{
		debugIOLog( "create user client object failed\n" );
		goto exit;
	}
    
    if( !userClient->initWithDriver( inDriver, inTask ) )
	{
		debugIOLog( "initWithDriver failed\n" );
		
		userClient->release();
		userClient = NULL;
		goto exit;
	}
	
	debug2IOLog( "User client created for task 0x%08lX\n", ( UInt32 ) inTask );
	
exit:
	return( userClient );
}

//===========================================================================================================================
//	initWithDriver
//===========================================================================================================================

bool	AOAUserClient::initWithDriver( AppleOnboardAudio *inDriver, task_t inTask )
{
	bool		result;
	
	debugIOLog( "initWithDriver\n" );
	
	result = false;
    if( !initWithTask( inTask, NULL, 0 ) )
	{
		debugIOLog( "initWithTask failed\n" );
		goto exit;
    }
    if( !inDriver )
	{
		debugIOLog( "initWithDriver failed (null input driver)\n" );
        goto exit;
    }
    
    mDriver 	= inDriver;
    mClientTask = inTask;
    result		= true;
	
exit:
	return( result );
}

//===========================================================================================================================
//	free
//===========================================================================================================================

void	AOAUserClient::free( void )
{
	debugIOLog( "free\n" );
	
    IOUserClient::free();
}

//===========================================================================================================================
//	clientClose
//===========================================================================================================================

IOReturn	AOAUserClient::clientClose( void )
{
	debugIOLog( "clientClose\n" );
	
    if( !isInactive() )
	{
        mDriver = NULL;
    }
    return( kIOReturnSuccess );
}

//===========================================================================================================================
//	clientDied
//===========================================================================================================================

IOReturn	AOAUserClient::clientDied( void )
{
	debugIOLog( "clientDied\n" );

    return( clientClose() );
}

//===========================================================================================================================
//	getTargetAndMethodForIndex
//===========================================================================================================================

IOExternalMethod *	AOAUserClient::getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex )
{
	IOExternalMethod *		methodPtr;
	
	methodPtr = NULL;
	if( inIndex <= sMethodCount ) {
        *outTarget = this;
		methodPtr = ( IOExternalMethod * ) &sMethods[ inIndex ];
    } else {
		debug2IOLog( "getTargetAndMethodForIndex - bad index (index=%lu)\n", inIndex );
	}
	return( methodPtr );
}

//===========================================================================================================================
//	Shutdown
//===========================================================================================================================

IOReturn	AOAUserClient::Shutdown( void ) {
	debugIOLog( "Shutdown\n" );

	if (NULL != mDriver) {
		if (kIOAudioDeviceActive == mDriver->ourPowerState) {
			debugIOLog ("turning off the sound hardware\n");
			mDriver->shuttingDown = TRUE;
			mDriver->GoIdlePower ();
		} else {
			debugIOLog ("hardware is already off\n");
		}
	} else {
		debugIOLog ("mDriver is NULL!!!!\n");
	}
	
	return( kIOReturnSuccess );
}
#endif
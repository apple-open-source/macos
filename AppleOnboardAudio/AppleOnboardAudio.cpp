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
#include <IOKit/IOTimerEventSource.h>

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
    UInt32					odevice;

    if(devices != currentDevices) {
        odevice = currentDevices;
        currentDevices = devices;
        changedDeviceHandler(odevice);
    }
    
	debug2IOLog ("currentDevices = %ld\n", currentDevices);
	debug2IOLog ("fCPUNeedsPhaseInversion = %d\n", fCPUNeedsPhaseInversion);
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
bool AppleOnboardAudio::initHardware (IOService *provider){
	IOWorkLoop				*workLoop;
    bool					result;

    DEBUG_IOLOG("+ AppleOnboardAudio::initHardware\n");

	result = FALSE;
    if (!super::initHardware (provider)) {
        goto EXIT;
    }

	ourPowerState = kIOAudioDeviceActive;
	ScheduleIdle ();
    sndHWInitialize (provider);
    theAudioDeviceTreeParser = AudioDeviceTreeParser::createWithEntryProvider (provider);
 
    setManufacturerName ("Apple");
    setDeviceName ("Built-in audio controller");

    parseAndActivateInit (provider);
    configureAudioDetects (provider);
    configureAudioOutputs (provider);
    configureAudioInputs (provider);
    configurePowerObject (provider);

    configureDMAEngines (provider);

	// Have to create the audio controls before calling activateAudioEngine
    createDefaultsPorts (); 

    if (kIOReturnSuccess != activateAudioEngine (driverDMAEngine)){
        driverDMAEngine->release  ();
        goto EXIT;
    }

	workLoop = getWorkLoop ();
	FailIf (NULL == workLoop, EXIT);

	// Create a timer event source for idle power requests
	idleTimer = IOTimerEventSource::timerEventSource (this, IdleSleepHandlerTimer);
	FailIf (NULL == idleTimer, EXIT);
	workLoop->addEventSource (idleTimer);

	// Give drivers a chance to do something after the DMA engine and IOAudioFamily have been created/started
	sndHWPostDMAEngineInit (provider);

	// Set the default volume to that stored in the PRAM in case we don't get a setValue call from the Sound prefs before being activated.
	gExpertMode = TRUE;			// Don't update the PRAM value while we're initing from it
	if (NULL != outVolLeft) {
		outVolLeft->setValue (PRAMToVolumeValue ());
	}
	if (NULL != outVolRight) {
		outVolRight->setValue (PRAMToVolumeValue ());
	}

    flushAudioControls ();
	gExpertMode = FALSE;

    publishResource ("setModemSound", this);

	// Install power change handler so we get notified about shutdown
	registerPrioritySleepWakeInterest (&sysPowerDownHandler, this, 0);

	// Tell the world about us so the User Client can find us.
	registerService ();

	result = TRUE;

EXIT:
    DEBUG_IOLOG ("- AppleOnboardAudio::initHardware\n"); 
    return (result);
}

IOReturn AppleOnboardAudio::configureDMAEngines(IOService *provider){
    IOReturn 			result;
    bool				hasInput;
    
    result = kIOReturnError;

	// All this config should go in a single method
	FailIf (NULL == theAudioDeviceTreeParser, EXIT);

    if (theAudioDeviceTreeParser->getNumberOfInputs () > 0)
        hasInput = true;
    else 
        hasInput = false;
        
    driverDMAEngine = new AppleDBDMAAudioDMAEngine;
    // make sure we get an engine
    FailIf (NULL == driverDMAEngine, EXIT);

    // tell the uber class if it has to worry about reversing the phase of a channel
    fCPUNeedsPhaseInversion = theAudioDeviceTreeParser->getPhaseInversion ();
    // it will be set when the device polling starts
    driverDMAEngine->setPhaseInversion (false);
    
    if (!driverDMAEngine->init (NULL, provider, hasInput)) {
        driverDMAEngine->release ();
        goto EXIT;
    }
    
	result = kIOReturnSuccess;

EXIT:
    return result;
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
				outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
//				outVolLeft->release ();
			}
		
			outVolRight = IOAudioLevelControl::createVolumeControl(OutmaxLin, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultRight,
												kIOAudioControlChannelNameRight,
												kOutVolRight, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolRight) {
				driverDMAEngine->addDefaultAudioControl(outVolRight);
				outVolRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
//				outVolRight->release ();
			}
			
			outMute = IOAudioToggleControl::createMuteControl(false,
											kIOAudioControlChannelIDAll,
											kIOAudioControlChannelNameAll,
											kOutMute, 
											kIOAudioControlUsageOutput);
			if (NULL != outMute) {
				driverDMAEngine->addDefaultAudioControl(outMute);
				outMute->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputMuteChangeHandler, this);
//				outMute->release ();
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
					inGainLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainLeftChangeHandler, this);
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

IOReturn AppleOnboardAudio::volumeChangeHandler (IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue) {
	AppleOnboardAudio *				audioDevice;
    IOCommandGate *					cg;
	IOReturn						result;

	result = kIOReturnError;
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	FailIf (NULL == audioDevice, Exit);

	cg = audioDevice->getCommandGate ();
	if (NULL != cg) {
		result = cg->runAction (volumeChangeAction, (void *)volumeControl, (void *)oldValue, (void *)newValue);
	}

Exit:
	return result;
}

IOReturn AppleOnboardAudio::volumeChangeAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;

	audioDevice = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf (NULL == audioDevice, Exit);
	levelControl = OSDynamicCast (IOAudioLevelControl, (IOAudioControl *)arg1);
	FailIf (NULL == levelControl, Exit);

    switch (levelControl->getSubType ()) {
		case kIOAudioLevelControlSubTypeVolume:
			switch (levelControl->getChannelID ()) {
				case kIOAudioControlChannelIDDefaultLeft:
					result = volumeLeftChangeHandler ((IOService *)owner, (IOAudioControl *)arg1, (SInt32)arg2, (SInt32)arg3);
					break;
				case kIOAudioControlChannelIDDefaultRight:
					result = volumeRightChangeHandler ((IOService *)owner, (IOAudioControl *)arg1, (SInt32)arg2, (SInt32)arg3);
					break;
			}
			break;
		case kIOAudioToggleControlSubTypeMute:
			result = outputMuteChangeHandler ((IOService *)owner, (IOAudioControl *)arg1, (SInt32)arg2, (SInt32)arg3);
			break;
		default:
			result = kIOReturnBadArgument;
	}

Exit:
	return result;
}

IOReturn AppleOnboardAudio::volumeLeftChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
	IOReturn						result = kIOReturnSuccess;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;

	DEBUG_IOLOG("+ AppleOnboardAudio::volumeLeftChangeHandler\n");

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	if(!audioDevice)
		goto BAIL;

	levelControl = OSDynamicCast (IOAudioLevelControl, volumeControl);
	FailIf (NULL == levelControl, EXIT);
	result = audioDevice->volumeLeftChanged(volumeControl, oldValue, newValue);

	if (newValue == levelControl->getMinValue () && (NULL == audioDevice->outVolRight || (NULL != audioDevice->outVolRight && audioDevice->outVolRight->getMinValue () == audioDevice->outVolRight->getIntValue ()))) {
		// If it's set to it's min, then it's mute, so tell the HAL it's muted
		OSNumber *			muteState;
		muteState = OSNumber::withNumber (1, 32);
		audioDevice->outMute->hardwareValueChanged (muteState);
	} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
		OSNumber *			muteState;
		muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
		audioDevice->outMute->hardwareValueChanged (muteState);
	}

EXIT:
	DEBUG2_IOLOG("- AppleOnboardAudio::volumeLeftChangeHandler, %d\n", (result == kIOReturnSuccess));
	return result;
BAIL:
	result = kIOReturnError;
	goto EXIT;
}

IOReturn AppleOnboardAudio::volumeLeftChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
                    
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

	if (NULL != driverDMAEngine->iSubBufferMemory) {
		start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
		FAIL_IF (NULL == start, Exit);

		iSubLeftVolume = (IOAudioLevelControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioLevelControl", kIOAudioControlSubTypeKey, 'subL');
		start->release ();
	
		if (NULL != iSubLeftVolume) {
			newiSubVolume = ((newValue * kiSubMaxVolume) / ((IOAudioLevelControl *)volumeControl)->getMaxValue ()) * kiSubVolumePercent / 100;
			iSubLeftVolume->setValue (newiSubVolume);
			iSubLeftVolume->release();
		}
	}

Exit:
	debug2IOLog ( "AppleOnboardAudio::volumeLeftChanged say's gExpertMode = %d\n", (unsigned int)gExpertMode );
    if(!gExpertMode) {				//We do that only if we are on a OS 9 like UI guideline
		debug3IOLog ( "AppleOnboardAudio::volumeLeftChanged invokes WritePRAMVol( %d, %d)\n", (unsigned int)gVolLeft, (unsigned int)gVolRight );
        WritePRAMVol(gVolLeft,gVolRight);
	}
EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::volumeLeftChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;

}

    
IOReturn AppleOnboardAudio::volumeRightChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
	IOReturn						result = kIOReturnSuccess;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;

	DEBUG_IOLOG("+ AppleOnboardAudio::volumeRightChangeHandler\n");

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	if(!audioDevice)
		goto BAIL;

	levelControl = OSDynamicCast (IOAudioLevelControl, volumeControl);
	FailIf (NULL == levelControl, EXIT);
	result = audioDevice->volumeRightChanged(volumeControl, oldValue, newValue);

	if (newValue == levelControl->getMinValue () && (NULL == audioDevice->outVolLeft || (NULL != audioDevice->outVolLeft && audioDevice->outVolLeft->getMinValue () == audioDevice->outVolLeft->getIntValue ()))) {
		// If it's set to it's min, then it's mute, so tell the HAL it's muted
		OSNumber *			muteState;
		muteState = OSNumber::withNumber (1, 32);
		audioDevice->outMute->hardwareValueChanged (muteState);
	} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
		OSNumber *			muteState;
		muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
		audioDevice->outMute->hardwareValueChanged (muteState);
	}

EXIT:
	DEBUG2_IOLOG("- AppleOnboardAudio::volumeRightChangeHandler, result = %d\n", kIOReturnSuccess);
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

	if (NULL != driverDMAEngine->iSubBufferMemory) {
		start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
		FAIL_IF (NULL == start, Exit);

		iSubRightVolume = (IOAudioLevelControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioLevelControl", kIOAudioControlSubTypeKey, 'subR');
		start->release ();

		if (NULL != iSubRightVolume) {
			newiSubVolume = ((newValue * kiSubMaxVolume) / ((IOAudioLevelControl *)volumeControl)->getMaxValue ()) * kiSubVolumePercent / 100;
			iSubRightVolume->setValue (newiSubVolume);
			iSubRightVolume->release();
		}
	}

Exit:
	debug2IOLog ( "AppleOnboardAudio::volumeRightChanged say's gExpertMode = %d\n", (unsigned int)gExpertMode );
    if(!gExpertMode) {				//We do that only if we are on a OS 9 like UI guideline
		debug3IOLog ( "AppleOnboardAudio::volumeRightChanged invokes WritePRAMVol( %d, %d)\n", (unsigned int)gVolLeft, (unsigned int)gVolRight );
        WritePRAMVol(gVolLeft,gVolRight);
	}
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
    
	debug2IOLog ( "AppleOnboardAudio::outputMuteChanged say's gExpertMode = %d\n", (unsigned int)gExpertMode );
    if(!gExpertMode) {		
        if (newValue) {
			debugIOLog( "AppleOnboardAudio::outputMuteChanged invokes WritePRAMVol(0,0)\n" );
            WritePRAMVol(0,0);
        } else {
			debug3IOLog ( "AppleOnboardAudio::outputMuteChanged invokes WritePRAMVol( %d, %d)\n", (unsigned int)gVolLeft, (unsigned int)gVolRight );
            WritePRAMVol(gVolLeft,gVolRight);
		}
    }
    
    start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
    FAIL_IF (NULL == start, EXIT);
    iSubMute = (IOAudioToggleControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioMuteControl", kIOAudioControlSubTypeKey, 'subM');
    start->release ();

	if ( NULL != iSubMute ) {
		iSubMute->setValue (newValue);
		iSubMute->release();
	}
    
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

void AppleOnboardAudio::IdleSleepHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
	AppleOnboardAudio *				appleOnboardAudio;

	appleOnboardAudio = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf (NULL == appleOnboardAudio, Exit);

	if (kIOAudioDeviceIdle == appleOnboardAudio->fNewPowerState) {
		if (NULL != appleOnboardAudio->theAudioPowerObject) {
			appleOnboardAudio->theAudioPowerObject->setIdlePowerState ();
			appleOnboardAudio->ourPowerState = kIOAudioDeviceIdle;
		}
	}

Exit:
	return;
}

// Set up a timer to power down the hardware if we haven't used it in a while.
void AppleOnboardAudio::ScheduleIdle (void) {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	if (NULL != idleTimer) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kPowerDownDelayTime;

		nanoseconds_to_absolutetime (nanos, &fireTime);
		idleTimer->wakeAtTime (fireTime);		// will call IdleSleepHandlerTimer
	}

	return;
}

IOReturn AppleOnboardAudio::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                        IOAudioDevicePowerState newPowerState,
                                                        UInt32 *microsecondsUntilComplete)
{
	IOReturn				result;

	debug3IOLog ("+ AppleOnboardAudio::performPowerStateChange (%d, %d)\n", oldPowerState, newPowerState);

	result = kIOReturnSuccess;

	if (NULL != theAudioPowerObject) {
		if (kIOAudioDeviceSleep == oldPowerState) {
			// If waking from sleep, we want to go to active mode first, then go into whatever mode is being requested
			*microsecondsUntilComplete = theAudioPowerObject->GetTimeToChangePowerState (ourPowerState, kIOAudioDeviceActive);
		}

		*microsecondsUntilComplete += theAudioPowerObject->GetTimeToChangePowerState (ourPowerState, newPowerState);
	}

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);

	if (NULL != theAudioPowerObject) {
		if (kIOAudioDeviceSleep == oldPowerState) {
			// We are waking up, so put the hardware into run mode first, then whatever mode we're being asked to go into
			ourPowerState = kIOAudioDeviceActive;
			theAudioPowerObject->setHardwarePowerOn ();
		}

		fNewPowerState = newPowerState;
		if (newPowerState != ourPowerState) {
			if (kIOAudioDeviceIdle == newPowerState) {
				// We won't go idle now, we'll go idle in 30 seconds if we don't get told to go active before then.
				if (TRUE == theAudioPowerObject->wantsIdleCalls ()) {
					ScheduleIdle ();
				}
			} else {
				ourPowerState = newPowerState;
				switch (ourPowerState) {
					case kIOAudioDeviceSleep:
						theAudioPowerObject->setHardwarePowerOff ();
						break;
					case kIOAudioDeviceActive:
						theAudioPowerObject->setHardwarePowerOn ();
						break;
					default:
						;
				}
			}
		}
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
			if (NULL != appleOnboardAudio->theAudioPowerObject) {
				appleOnboardAudio->theAudioPowerObject->setIdlePowerState ();
			}
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
IOReturn AppleOnboardAudio::setModemSound (bool state){
    AudioHardwareInput *			theInput;
    UInt32							idx;

    debugIOLog ("+ AppleOnboardAudio::setModemSound\n");

    theInput = NULL;
    if (gIsModemSoundActive == state) 
        goto EXIT;

    if (FALSE != state) {		// do the compare this way since they may pass anything non-0 to mean TRUE, but 0 is always FALSE
        // we turn the modem on: find the active source, switch to modem, and turn playthrough on
        if (NULL != AudioInputs) {
            for (idx = 0; idx < AudioInputs->getCount (); idx++) {
                theInput = OSDynamicCast (AudioHardwareInput, AudioInputs->getObject (idx));
                if (NULL != theInput) {
                    theInput->forceActivation ('modm');
                    theInput->setInputGain (0,0);
                }
            }
        }  

        sndHWSetPlayThrough (true);
    } else {
        // we turn the modem off: turn playthrough off, switch to saved source;
        sndHWSetPlayThrough (!gIsPlayThroughActive);
        if (NULL != AudioInputs) {
            for (idx = 0; idx < AudioInputs->getCount (); idx++) {
                theInput = OSDynamicCast (AudioHardwareInput, AudioInputs->getObject (idx));
                if (NULL != theInput){
                    theInput->forceActivation (inputSelector->getIntValue ());
                    theInput->setInputGain (gGainLeft, gGainRight);
                }
            }
        }
    }

    gIsModemSoundActive = state;
EXIT:
    debugIOLog ("- AppleOnboardAudio::setModemSound\n");
    return kIOReturnSuccess;
}

IOReturn AppleOnboardAudio::callPlatformFunction( const OSSymbol * functionName, bool waitForFunction,void *param1, void *param2, void *param3, void *param4 ) {
    debugIOLog ("+ AppleOnboardAudio::callPlatformFunction\n");
    if (functionName->isEqualTo ("setModemSound")) {
        return (setModemSound ((bool)param1));
    }

    debugIOLog ("- AppleOnboardAudio::callPlatformFunction\n");
    return (super::callPlatformFunction (functionName, waitForFunction,param1, param2, param3, param4));
}

#pragma mark +PRAM VOLUME
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
UInt8 AppleOnboardAudio::VolumeToPRAMValue (UInt32 inLeftVol, UInt32 inRightVol) {
	UInt32			pramVolume;						// Volume level to store in PRAM
	UInt32 			averageVolume;					// summed volume
    UInt32		 	volumeRange;
    UInt32 			volumeSteps;
	UInt32			leftVol;
	UInt32			rightVol;

	debug3IOLog ( "AppleOnboardAudio::VolumeToPRAMValue ( 0x%X, 0x%X )\n", (unsigned int)inLeftVol, (unsigned int)inRightVol );
	pramVolume = 0;											//	[2886446]	Always pass zero as a result when muting!!!
	if ( ( 0 != inLeftVol ) || ( 0 != inRightVol ) ) {		//	[2886446]
		leftVol = inLeftVol;
		rightVol = inRightVol;
		if (NULL != outVolLeft) {
			leftVol -= outVolLeft->getMinValue ();
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue leftVol = 0x%X\n", (unsigned int)leftVol );
		}
	
		if (NULL != outVolRight) {
			rightVol -= outVolRight->getMinValue ();
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue rightVol = 0x%X\n", (unsigned int)rightVol );
		}
	
		if (NULL != outVolLeft) {
			volumeRange = (outVolLeft->getMaxValue () - outVolLeft->getMinValue () + 1);
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue outVolLeft volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else if (NULL != outVolRight) {
			volumeRange = (outVolRight->getMaxValue () - outVolRight->getMinValue () + 1);
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue outVolRight volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue volumeRange = 0x%X\n", (unsigned int)volumeRange );
		}
	
		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue averageVolume = 0x%X\n", (unsigned int)volumeRange );
		volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume
		pramVolume = averageVolume / volumeSteps;    
	
		// Since the volume level in PRAM is only worth three bits,
		// we round small values up to 1. This avoids SysBeep from
		// flashing the menu bar when it thinks sound is off and
		// should really just be very quiet.
	
		if ((pramVolume == 0) && (leftVol != 0 || rightVol !=0 )) {
			pramVolume = 1;
		}
	
	}
	debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue returns 0x%X\n", (unsigned int)pramVolume );
	return (pramVolume & 0x07);
}

UInt32 AppleOnboardAudio::PRAMToVolumeValue (void) {
	UInt32		 	volumeRange;
	UInt32 			volumeSteps;

	if (NULL != outVolLeft) {
		volumeRange = (outVolLeft->getMaxValue () - outVolLeft->getMinValue () + 1);
	} else if (NULL != outVolRight) {
		volumeRange = (outVolRight->getMaxValue () - outVolRight->getMinValue () + 1);
	} else {
		volumeRange = kMaximumPRAMVolume;
	}

	volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume

	return (volumeSteps * ReadPRAMVol ());
}

void AppleOnboardAudio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	UInt8						pramVolume;
	UInt8 						curPRAMVol;
	IODTPlatformExpert * 		platform;
	IOReturn					err;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
    
    debug3IOLog("AppleOnboardAudio::WritePRAMVol leftVol=%lu, rightVol=%lu\n",leftVol,  rightVol);
    
    if (platform) {
		debug2IOLog ( "AppleOnboardAudio::WritePRAMVol platform 0x%X\n", (unsigned int)platform );
		pramVolume = VolumeToPRAMValue(leftVol,rightVol);
		curPRAMVol = pramVolume ^ 0xFF;
		debug3IOLog ( "AppleOnboardAudio::WritePRAMVol target pramVolume = 0x%X, curPRAMVol = 0x%X\n", pramVolume, curPRAMVol );
		
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			debug2IOLog ( "AppleOnboardAudio::WritePRAMVol curPRAMVol = 0x%X before write\n", (curPRAMVol & 0x07) );
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				debug2IOLog("AppleOnboardAudio::WritePRAMVol curPRAMVol = 0x%x\n",curPRAMVol);
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debug5IOLog ( "0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X\n", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
					err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
					if ( kIOReturnSuccess == err ) {
						if ( ( 0x07 & curPRAMVol ) != pramVolume ) {
							debug3IOLog ( "PRAM Read after Write did not compare:  Write = 0x%X, Read = 0x%X\n", (unsigned int)pramVolume, (unsigned int)curPRAMVol );
						} else {
							debugIOLog ( "PRAM verified after write!\n" );
						}
					} else {
						debugIOLog ( "Could not readXPRAM to verify write!\n" );
					}
				}
			} else {
				debugIOLog ( "PRAM write request is to current value: no I/O\n" );
			}
		} else {
			debug2IOLog ( "Could not readXPRAM prior to write! Error 0x%X\n", err );
		}
	} else {
		debugIOLog ( "AppleOnboardAudio::WritePRAMVol say's no platform\n" );
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

/*
		User Client stuff
*/

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
	
	IOLog( "[AppleOnboardAudio] creating user client for task 0x%08lX\n", ( UInt32 ) inOwningTask );
	
	// Create the user client object.
	
	err = kIOReturnNoMemory;
	userClientPtr = AppleOnboardAudioUserClient::Create( this, inOwningTask );
	if( !userClientPtr ) goto exit;
    
	// Set up the user client.
	
	err = kIOReturnError;
	result = userClientPtr->attach( this );
	if( !result ) goto exit;

	result = userClientPtr->start( this );
	if( !result ) goto exit;
	
	// Success.
	
    *outHandler = userClientPtr;
	err = kIOReturnSuccess;
	
exit:
	IOLog( "[AppleOnboardAudio] newUserClient done (err=%d)\n", err );
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

const IOExternalMethod		AppleOnboardAudioUserClient::sMethods[] =
{
	//	Read
	
	{
		NULL,														// object
		( IOMethod ) &AppleOnboardAudioUserClient::gpioRead,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	
	//	Write
	
	{
		NULL,														// object
		( IOMethod ) &AppleOnboardAudioUserClient::gpioWrite,		// func
		kIOUCScalarIScalarO,										// flags
		2,															// count of input parameters
		0															// count of output parameters
	},

	// gpioGetActiveState

	{
		NULL,														// object
		( IOMethod ) &AppleOnboardAudioUserClient::gpioGetActiveState,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
};

const IOItemCount		AppleOnboardAudioUserClient::sMethodCount = sizeof( AppleOnboardAudioUserClient::sMethods ) / 
																  sizeof( AppleOnboardAudioUserClient::sMethods[ 0 ] );

OSDefineMetaClassAndStructors( AppleOnboardAudioUserClient, IOUserClient )

//===========================================================================================================================
//	Create
//===========================================================================================================================

AppleOnboardAudioUserClient *	AppleOnboardAudioUserClient::Create( AppleOnboardAudio *inDriver, task_t inTask )
{
    AppleOnboardAudioUserClient *		userClient;
    
    userClient = new AppleOnboardAudioUserClient;
	if( !userClient )
	{
		IOLog( "[AppleOnboardAudio] create user client object failed\n" );
		goto exit;
	}
    
    if( !userClient->initWithDriver( inDriver, inTask ) )
	{
		IOLog( "[AppleOnboardAudio] initWithDriver failed\n" );
		
		userClient->release();
		userClient = NULL;
		goto exit;
	}
	
	IOLog( "[AppleOnboardAudio] User client created for task 0x%08lX\n", ( UInt32 ) inTask );
	
exit:
	return( userClient );
}

//===========================================================================================================================
//	initWithDriver
//===========================================================================================================================

bool	AppleOnboardAudioUserClient::initWithDriver( AppleOnboardAudio *inDriver, task_t inTask )
{
	bool		result;
	
	IOLog( "[AppleOnboardAudio] initWithDriver\n" );
	
	result = false;
    if( !initWithTask( inTask, NULL, 0 ) )
	{
		IOLog( "[AppleOnboardAudio] initWithTask failed\n" );
		goto exit;
    }
    if( !inDriver )
	{
		IOLog( "[AppleOnboardAudio] initWithDriver failed (null input driver)\n" );
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

void	AppleOnboardAudioUserClient::free( void )
{
	IOLog( "[AppleOnboardAudio] free\n" );
	
    IOUserClient::free();
}

//===========================================================================================================================
//	clientClose
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::clientClose( void )
{
	IOLog( "[AppleOnboardAudio] clientClose\n" );
	
    if( !isInactive() )
	{
        mDriver = NULL;
    }
    return( kIOReturnSuccess );
}

//===========================================================================================================================
//	clientDied
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::clientDied( void )
{
	IOLog( "[AppleOnboardAudio] clientDied\n" );
	
    return( clientClose() );
}

//===========================================================================================================================
//	getTargetAndMethodForIndex
//===========================================================================================================================

IOExternalMethod *	AppleOnboardAudioUserClient::getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex )
{
	IOExternalMethod *		methodPtr;
	
	methodPtr = NULL;
	if( inIndex <= sMethodCount )
    {
        *outTarget = this;
		methodPtr = ( IOExternalMethod * ) &sMethods[ inIndex ];
    }
	else
	{
		IOLog( "[AppleOnboardAudio] getTargetAndMethodForIndex - bad index (index=%lu)\n", inIndex );
	}
	return( methodPtr );
}

//===========================================================================================================================
//	gpioRead
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::gpioRead (UInt32 selector, UInt8 * gpioState)
{
	IOReturn		err;

#ifdef DEBUGMODE
	IOLog ("gpioRead (selector=%4s, gpioState=0x%p)\n", (char *)&selector, gpioState);
#endif

	err = kIOReturnNotReadable;

	FailIf (NULL == mDriver, Exit);
	*gpioState = mDriver->readGPIO (selector);

#ifdef DEBUGMODE
	IOLog ("gpioRead gpioState=0x%x\n", *gpioState);
#endif

	err = kIOReturnSuccess;

Exit:
	return (err);
}

//===========================================================================================================================
//	gpioWrite
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::gpioWrite (UInt32 selector, UInt8 value)
{
	IOReturn		err;

#ifdef DEBUGMODE
	IOLog ( "gpioWrite (selector=%4s, value=0x%x)\n", (char *)&selector, value);
#endif

	err = kIOReturnNotReadable;

	FailIf (NULL == mDriver, Exit);
	mDriver->writeGPIO (selector, value);

	err = kIOReturnSuccess;

Exit:
	return (err);
}

//===========================================================================================================================
//	gpioGetActiveState
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::gpioGetActiveState (UInt32 selector, UInt8 * gpioActiveState)
{
	IOReturn		err;

#ifdef DEBUGMODE
	IOLog ("gpioGetActiveState (selector=%4s, gpioActiveState=0x%p)\n", (char *)&selector, gpioActiveState);
#endif

	err = kIOReturnNotReadable;

	FailIf (NULL == mDriver, Exit);
	*gpioActiveState = mDriver->getGPIOActiveState (selector);

#ifdef DEBUGMODE
	IOLog ("gpioGetActiveState gpioState=0x%x\n", *gpioActiveState);
#endif

	err = kIOReturnSuccess;

Exit:
	return (err);
}

#if 0
/*
		The following code goes into whatever application wants to call into AppleOnboardAudio
*/

//===========================================================================================================================
//	Private constants
//===========================================================================================================================

enum
{
	kgpioReadIndex	 		= 0,
	kgpioWriteIndex 		= 1
};

//===========================================================================================================================
//	Private prototypes
//===========================================================================================================================

static IOReturn	SetupUserClient( void );
static void		TearDownUserClient( void );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static mach_port_t		gMasterPort		= 0;
static io_object_t		gDriverObject	= 0;
static io_connect_t		gDataPort 		= 0;

//===========================================================================================================================
//	SetupUserClient
//===========================================================================================================================

static IOReturn	SetupUserClient( void )
{
	IOReturn			err;
	CFDictionaryRef		matchingDictionary;
	io_iterator_t		serviceIter;
	
	// Initialize variables for easier cleanup.
	
	err					= kIOReturnSuccess;
	matchingDictionary 	= NULL;
	serviceIter			= NULL;
	
	// Exit quickly if we're already set up.
	
	if( gDataPort )
	{
		goto exit;
	}
	
	// Get a port so we can communicate with IOKit.
	
	err = IOMasterPort( NULL, &gMasterPort );
	if( err != kIOReturnSuccess ) goto exit;
	
	// Build a dictionary of all the services matching our service name. Note that we do not release the dictionary
	// if IOServiceGetMatchingServices succeeds because it does the release itself.
	
	err = kIOReturnNotFound;
	matchingDictionary = IOServiceNameMatching( "AppleTexasAudio" );
	if( !matchingDictionary ) goto exit;
	
	err = IOServiceGetMatchingServices( gMasterPort, matchingDictionary, &serviceIter );
	if( err != kIOReturnSuccess ) goto exit;
	matchingDictionary = NULL;
	
	err = kIOReturnNotFound;
	gDriverObject = IOIteratorNext( serviceIter );
	if( !gDriverObject ) goto exit;
	
	// Open a connection to our service so we can talk to it.
	
	err = IOServiceOpen( gDriverObject, mach_task_self(), 0, &gDataPort );
	if( err != kIOReturnSuccess ) goto exit;
	
	// Success. Clean up stuff and we're done.
	
exit:
	if( serviceIter )
	{
		IOObjectRelease( serviceIter );
	}
	if( matchingDictionary )
	{
		CFRelease( matchingDictionary );
	}
	if( err != kIOReturnSuccess )
	{
		TearDownUserClient();
	}
	return( err );
}

//===========================================================================================================================
//	TearDownUserClient
//===========================================================================================================================

static void	TearDownUserClient( void )
{
	if( gDataPort )
	{
		IOServiceClose( gDataPort );
		gDataPort = 0;
	}
	if( gDriverObject )
	{
		IOObjectRelease( gDriverObject );
		gDriverObject = NULL;
	}
	if( gMasterPort )
	{
		mach_port_deallocate( mach_task_self(), gMasterPort );
		gMasterPort = 0;
	}
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	gpioRead
//===========================================================================================================================

OSStatus	gpioRead( UInt32 selector, UInt8 * gpioState )
{	
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIScalarO( gDataPort, kgpioReadIndex, 1, 1, selector, gpioState );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}

//===========================================================================================================================
//	gpioWrite
//===========================================================================================================================

OSStatus	gpioWrite( UInt32 selector, UInt8 value )
{	
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIScalarO( gDataPort, kgpioWriteIndex, 2, 0, selector, gpioState );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}

//===========================================================================================================================
//	getGPIOAddress
//===========================================================================================================================

OSStatus	gpioGetAddress( UInt32 selector, UInt8 ** gpioAddress )
{
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIScalarO( gDataPort, kgpiogetAddressIndex, 2, selector, gpioAddress );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}

#endif

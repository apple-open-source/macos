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
        
    currentDevices = 0xFFFF;
    
	mHasHardwareInputGain = true;	// aml 5.10.02
	ourPowerState = kIOAudioDeviceActive;
	
	mInternalMicDualMonoMode = e_Mode_Disabled;	// aml 6.17.02, turn off by default
	
	// Future for creation
    if (AOAprop = OSDynamicCast(OSDictionary, properties->getObject("AOAAttributes"))) {
        gHasModemSound = (kOSBooleanTrue == OSDynamicCast(OSBoolean, AOAprop->getObject("analogModem")));
    }
        
    CLOG("- AppleOnboardAudio::init\n");
    return true;
}

void AppleOnboardAudio::free()
{
    DEBUG_IOLOG("+ AppleOnboardAudio::free\n");
    
    if (driverDMAEngine) {
        driverDMAEngine->release();
		driverDMAEngine = NULL;
	}
	CLEAN_RELEASE(outMute);
	CLEAN_RELEASE(headphoneConnection);
	CLEAN_RELEASE(outVolLeft);
	CLEAN_RELEASE(outVolRight);
	CLEAN_RELEASE(outVolMaster);
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
    if (fCPUNeedsPhaseInversion) {
        bool state;

        if (currentDevices == 0 || currentDevices & kSndHWInternalSpeaker) {			// may only need the kSndHWInternalSpeaker check
            state = true;
        } else {
            state = false;
		}

         driverDMAEngine->setPhaseInversion(state);
    }

	// For [2829546]
	if (devices & kSndHWInputDevices) {
		if (NULL != inputConnection) {
			OSNumber *			inputState;
			UInt32				active;

			active = devices & kSndHWInputDevices ? 1 : 0;		// If something is plugged in, that's good enough for now.
			inputState = OSNumber::withNumber ((long long unsigned int)active, 32);
			(void)inputConnection->hardwareValueChanged (inputState);
		}
	}
	// end [2829546]
}

void AppleOnboardAudio::changedDeviceHandler(UInt32 olddevices){
    UInt16 i;
    AudioHardwareOutput *theOutput;
//	AudioHardwareInput  *theInput;
    
    if(AudioOutputs) {
        for(i = 0; i< AudioOutputs->getCount(); i++) {
            theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(i));
            if( theOutput) theOutput->deviceIntService(currentDevices);
        }
    } 
       
	// Now that we have an input selector, it works through it
    /*if(AudioInputs) {
        for(i = 0; i< AudioInputs->getCount(); i++) {
            theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(i));
            if( theInput) theInput->deviceSetActive(currentDevices);
        }
    }*/
}

#pragma mark +IOAUDIO INIT
bool AppleOnboardAudio::initHardware (IOService *provider){
	IOWorkLoop *			workLoop;
	IOAudioStream *			inputStream;
	IOAudioStream *			outputStream;
    bool					result;

    DEBUG_IOLOG("+ AppleOnboardAudio::initHardware\n");

	result = FALSE;
    if (!super::initHardware (provider)) {
        goto EXIT;
    }

//	ourPowerState = getPowerState();
    sndHWInitialize (provider);
    theAudioDeviceTreeParser = AudioDeviceTreeParser::createWithEntryProvider (provider);
 
    setManufacturerName ("Apple");
    setDeviceName ("Built-in audio controller");
	setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);

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
		driverDMAEngine = NULL;
        goto EXIT;
    }

	workLoop = getWorkLoop ();
	FailIf (NULL == workLoop, EXIT);

	outputStream = driverDMAEngine->getAudioStream (kIOAudioStreamDirectionOutput, 1);
	if (outputStream) {
		outputStream->setTerminalType (OUTPUT_SPEAKER);
	} else {
		debugIOLog ("didn't get the output stream\n");
	}

	inputStream = driverDMAEngine->getAudioStream (kIOAudioStreamDirectionInput, 1);
	if (inputStream) {
		inputStream->setTerminalType (INPUT_MICROPHONE);
	} else {
		debugIOLog ("didn't get the input stream\n");
	}

	// Set this to a default for desktop machines (portables will get a setAggressiveness call later in the boot sequence).
	setIdleAudioSleepTime (kNoIdleAudioPowerDown);

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


	// aml 5.10.02
    mHasHardwareInputGain = theAudioDeviceTreeParser->getHasHWInputGain();
	if (mHasHardwareInputGain) {
		driverDMAEngine->setUseSoftwareInputGain(false);
	} else {
		driverDMAEngine->setUseSoftwareInputGain(true);
	}

//	driverDMAEngine->setInputGainL(24);	// XXX make const for unity gain index
//	driverDMAEngine->setInputGainR(24);	// XXX make const for unity gain index

#ifdef _AML_LOG_INPUT_GAIN
	if (mHasHardwareInputGain)
		IOLog("AppleOnboardAudio::configureDMAEngines - has hardware input gain = TRUE.\n");
	else
		IOLog("AppleOnboardAudio::configureDMAEngines - has hardware input gain = FALSE.\n");
#endif

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

    // tell the super class if it has to worry about reversing the phase of a channel
    fCPUNeedsPhaseInversion = theAudioDeviceTreeParser->getPhaseInversion ();
    // it will be set when the device polling starts
    driverDMAEngine->setPhaseInversion (false);
    
    if (!driverDMAEngine->init (NULL, provider, hasInput)) {
        driverDMAEngine->release ();
		driverDMAEngine = NULL;
        goto EXIT;
    }
    
	result = kIOReturnSuccess;

EXIT:
    return result;
}

IOReturn AppleOnboardAudio::createDefaultsPorts () {
    IOAudioPort *				outputPort = NULL;
    IOAudioPort *				inputPort = NULL;
    OSDictionary *				AOAprop = NULL;
	OSDictionary *				theRange = NULL;
    OSNumber *					theNumber;
    OSData *					theData;
	AudioHardwareInput *		theInput;
	UInt32						inputType;
    SInt32						OutminLin;
	SInt32						OutmaxLin;
	SInt32						InminLin;
	SInt32						InmaxLin; 
    IOFixed						OutminDB;
	IOFixed						OutmaxDB;
	IOFixed						InminDB;
	IOFixed						InmaxDB;
    UInt32						idx;
    IOReturn					result;
	Boolean						done;
	Boolean					hasPlaythrough;

    DEBUG_IOLOG("+ AppleOnboardAudio::createDefaultsPorts\n");

	hasPlaythrough = FALSE;
	result = kIOReturnSuccess;
	FailIf (NULL == driverDMAEngine, BAIL);
	FailIf (NULL == (AOAprop = OSDynamicCast(OSDictionary, this->getProperty("AOAAttributes"))), BAIL);

	// [2731278] Create output selector that is used to tell the HAL what the current output is (speaker, headphone, etc.)
	outputSelector = IOAudioSelectorControl::createOutputSelector ('ispk', kIOAudioControlChannelIDAll);
	if (NULL != outputSelector) {
		driverDMAEngine->addDefaultAudioControl(outputSelector);
		outputSelector->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		outputSelector->addAvailableSelection(kIOAudioOutputPortSubTypeInternalSpeaker, "Internal speaker");
		outputSelector->addAvailableSelection(kIOAudioOutputPortSubTypeHeadphones, "Headphones");
		// Don't release it because we might use it later.
	}
	// end [2731278]

    /*
     * Create out part port : 2 level (one for each side and one mute)
     */
	if (NULL != (theRange = OSDynamicCast(OSDictionary, AOAprop->getObject("RangeOut")))) {
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
			
			// Master control will be created when it's needed, which isn't normally the case, so don't make one now
			outVolMaster = NULL;

			pramVol = IOAudioLevelControl::create(PRAMToVolumeValue (), 0, 7, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDAll,
												"Boot beep volume",
												kPRAMVol, 
												kIOAudioLevelControlSubTypePRAMVolume,
												kIOAudioControlUsageOutput);
			if (NULL != pramVol) {
				driverDMAEngine->addDefaultAudioControl(pramVol);
				pramVol->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				pramVol->release ();
				pramVol = NULL;
			}

			outVolLeft = IOAudioLevelControl::createVolumeControl(OutmaxLin, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultLeft,
												kIOAudioControlChannelNameLeft,
												kOutVolLeft, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolLeft) {
				driverDMAEngine->addDefaultAudioControl(outVolLeft);
				outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				// Don't release it because we might reference it later
			}
		
			outVolRight = IOAudioLevelControl::createVolumeControl(OutmaxLin, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultRight,
												kIOAudioControlChannelNameRight,
												kOutVolRight, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolRight) {
				driverDMAEngine->addDefaultAudioControl(outVolRight);
				outVolRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				// Don't release it because we might reference it later
			}
			
			outMute = IOAudioToggleControl::createMuteControl(false,
											kIOAudioControlChannelIDAll,
											kIOAudioControlChannelNameAll,
											kOutMute, 
											kIOAudioControlUsageOutput);
			if (NULL != outMute) {
				driverDMAEngine->addDefaultAudioControl(outMute);
				outMute->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				// Don't release it because we might reference it later
			}

			// Create a toggle control for reporting the status of the headphone jack
			headphoneConnection = IOAudioToggleControl::create (FALSE,
												kIOAudioControlChannelIDAll,
												kIOAudioControlChannelNameAll,
												kHeadphoneInsert,
												kIOAudioControlTypeJack,
												kIOAudioControlUsageOutput);

			if (NULL != headphoneConnection) {
				driverDMAEngine->addDefaultAudioControl (headphoneConnection);
				// no value change handler because this isn't a settable control
				// Don't release it because we might reference it later
			}

			attachAudioPort(outputPort, driverDMAEngine, 0);
		}
	}

    /*
     * Create input port and level controls (if any) associated to it
     */
    if ((theAudioDeviceTreeParser->getNumberOfInputs() > 0)) {
		if (NULL != (theRange = OSDynamicCast(OSDictionary, AOAprop->getObject("RangeIn")))) {
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
				hasPlaythrough = (kOSBooleanTrue == OSDynamicCast(OSBoolean, AOAprop->getObject("Playthrough")));

				// aml 5.24.02, save default ranges (range may change when switching to internal mic eg.)
				mDefaultInMinDB = InminDB;
				mDefaultInMaxDB = InmaxDB;

				inGainLeft = IOAudioLevelControl::createVolumeControl((InmaxLin-InminLin)/2, InminLin, InmaxLin, InminDB, InmaxDB,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kInGainLeft, 
													kIOAudioControlUsageInput);
				if (NULL != inGainLeft) {
					driverDMAEngine->addDefaultAudioControl(inGainLeft);
					inGainLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
					// Don't release it because we might reference it later
				}
			
				inGainRight = IOAudioLevelControl::createVolumeControl((InmaxLin-InminLin)/2, InminLin, InmaxLin, InminDB, InmaxDB,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kInGainRight, 
													kIOAudioControlUsageInput);
				if (NULL != inGainRight) {
					driverDMAEngine->addDefaultAudioControl(inGainRight);
					inGainRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
					// Don't release it because we might reference it later
				}

				attachAudioPort(inputPort, 0, driverDMAEngine);
				inputPort->release();
				inputPort = NULL;
			}
		}

		// create the input selectors
		if(AudioInputs) {
			inputSelector = NULL;
			for (idx = 0; idx < AudioInputs->getCount(); idx++) {
				theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
				if (theInput) {
					inputType = theInput->getInputPortType();
					DEBUG2_IOLOG ("Creating input selector of type %4s\n", (char*)&inputType);
					if (NULL == inputSelector && 'none' != inputType) {
						inputSelector = IOAudioSelectorControl::createInputSelector(inputType,
																		kIOAudioControlChannelIDAll,
																		kIOAudioControlChannelNameAll,
																		kInputSelector);
						if (NULL != inputSelector) {
							driverDMAEngine->addDefaultAudioControl(inputSelector);
							inputSelector->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);	
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
						// Don't release inputSelector because we might use it later in setModemSound
					}
				}
			}
		}

		done = FALSE;
		for (idx = 0; idx < AudioInputs->getCount () && !done; idx++) {
			theInput = OSDynamicCast (AudioHardwareInput, AudioInputs->getObject(idx));
			if (theInput) {
				inputType = theInput->getInputPortType ();

				switch (inputType) {
					case 'emic':
					case 'sinj':
					case 'line':
						// Create a jack control to let the HAL/Sound Manager know that some input source is plugged in
						inputConnection = IOAudioToggleControl::create (FALSE,
															kIOAudioControlChannelIDAll,
															kIOAudioControlChannelNameAll,
															kInputInsert,
															kIOAudioControlTypeJack,
															kIOAudioControlUsageInput);

						if (NULL != inputConnection) {
							driverDMAEngine->addDefaultAudioControl (inputConnection);
							// no value change handler because this isn't a settable control
							// Don't release it because we might reference it later
						}
						done = TRUE;
						break;
					default:
						break;
				}
			}
		}

		if (NULL != outputPort && TRUE == hasPlaythrough) {
			playthruToggle = IOAudioToggleControl::createMuteControl (TRUE,
												kIOAudioControlChannelIDAll,
												kIOAudioControlChannelNameAll,
												kPassThruToggle, 
												kIOAudioControlUsagePassThru);

			if (NULL != playthruToggle) {
				driverDMAEngine->addDefaultAudioControl (playthruToggle);
				playthruToggle->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
				playthruToggle->release ();
				playthruToggle = NULL;
			}

			outputPort->release();
			outputPort = NULL;
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

IOReturn AppleOnboardAudio::outputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	IODTPlatformExpert * 			platform;
	UInt32							leftVol;
	UInt32							rightVol;
	Boolean							wasPoweredDown;

//	IOLog ("AppleOnboardAudio::outputControlChangeHandler (%p, %p, %ld, %ld)\n", target, control, oldValue, newValue);

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	FailIf (NULL == audioDevice, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState && NULL != audioDevice->theAudioPowerObject) {
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->theAudioPowerObject->setHardwarePowerOn ();
		if (NULL != audioDevice->outMute) {
			audioDevice->outMute->flushValue ();					// Restore hardware to the user's selected state
		}
		wasPoweredDown = TRUE;
	} else {
		wasPoweredDown = FALSE;
	}

    switch (control->getSubType ()) {
		case kIOAudioLevelControlSubTypeVolume:
			levelControl = OSDynamicCast (IOAudioLevelControl, control);

			switch (control->getChannelID ()) {
				case kIOAudioControlChannelIDAll:
					result = audioDevice->volumeMasterChange (newValue);
					if (newValue == levelControl->getMinValue ()) {
						// If it's set to it's min, then it's mute, so tell the HAL it's muted
						OSNumber *			muteState;
						muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
						if (NULL != audioDevice->outMute) {
							audioDevice->outMute->hardwareValueChanged (muteState);
						}
					} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
						OSNumber *			muteState;
						muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
						if (NULL != audioDevice->outMute) {
							audioDevice->outMute->hardwareValueChanged (muteState);
						}
					}
					break;
				case kIOAudioControlChannelIDDefaultLeft:
//					IOLog ("left control\n");
					result = audioDevice->volumeLeftChange (newValue);
//					(void)audioDevice->setiSubVolume (kIOAudioLevelControlSubTypeLFEVolume, ((newValue * kiSubMaxVolume) / levelControl->getMaxValue ()) * kiSubVolumePercent / 100);
					break;
				case kIOAudioControlChannelIDDefaultRight:
//					IOLog ("right control\n");
					result = audioDevice->volumeRightChange (newValue);
//					(void)audioDevice->setiSubVolume (kIOAudioLevelControlSubTypeLFEVolume, ((newValue * kiSubMaxVolume) / levelControl->getMaxValue ()) * kiSubVolumePercent / 100);
					break;
			}
			break;
		case kIOAudioToggleControlSubTypeMute:
//			IOLog ("mute control toggled\n");
			result = audioDevice->outputMuteChange (newValue);
//			(void)audioDevice->setiSubMute (newValue);
			break;
		case kIOAudioSelectorControlSubTypeOutput:
			result = kIOReturnUnsupported;
			break;
		case kIOAudioLevelControlSubTypePRAMVolume:
			platform = OSDynamicCast (IODTPlatformExpert, getPlatform());
			if (platform) {
				UInt8 							curPRAMVol;

				result = platform->readXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
				curPRAMVol = (curPRAMVol & 0xF8) | newValue;
				result = platform->writeXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount) 1);
			}
			break;
		default:
			result = kIOReturnBadArgument;
	}

	if (control->getSubType () == kIOAudioLevelControlSubTypeVolume) {
		levelControl = OSDynamicCast (IOAudioLevelControl, control);
		if (audioDevice->outVolRight && audioDevice->outVolLeft) {
			if (audioDevice->outVolRight->getMinValue () == audioDevice->gVolRight &&
				audioDevice->outVolLeft->getMinValue () == audioDevice->gVolLeft) {
				// If it's set to it's min, then it's mute, so tell the HAL it's muted
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
				if (NULL != audioDevice->outMute) {
					audioDevice->outMute->hardwareValueChanged (muteState);
				}
//				IOLog ("turning on hardware mute flag\n");
			} else if (newValue != levelControl->getMinValue () && oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
				if (NULL != audioDevice->outMute) {
					audioDevice->outMute->hardwareValueChanged (muteState);
				}
//				IOLog ("turning off hardware mute flag\n");
			}
		}
	}

	if (audioDevice->gIsMute) {
		leftVol = 0;
		rightVol = 0;
	} else {
		leftVol = audioDevice->gVolLeft;
		rightVol = audioDevice->gVolRight;
	}

	if (FALSE == audioDevice->gExpertMode) {				// We do that only if we are on a OS 9 like UI guideline
		audioDevice->WritePRAMVol (leftVol, rightVol);
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->scheduleIdleAudioSleep ();
	}

	return result;
}
#if 0
IOReturn AppleOnboardAudio::setiSubVolume (UInt32 iSubVolumeControl, SInt32 iSubVolumeLevel) {
    IOReturn				result;
    IOAudioLevelControl *	iSubVolume;
    IORegistryEntry *		start;

	result = kIOReturnSuccess;
	if (NULL != driverDMAEngine->iSubBufferMemory) {
		start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
		FAIL_IF (NULL == start, Exit);

		iSubVolume = (IOAudioLevelControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioLevelControl", kIOAudioControlSubTypeKey, iSubVolumeControl);
		start->release ();

		if (NULL != iSubVolume) {
			iSubVolume->setValue (iSubVolumeLevel);
			iSubVolume->release();
		}
	}

Exit:
	return result;
}

IOReturn AppleOnboardAudio::setiSubMute (UInt32 setMute) {
    IOReturn					result;
    IOAudioToggleControl *		iSubMute;
    IORegistryEntry *			start;

	result = kIOReturnSuccess;
    start = childFromPath ("AppleDBDMAAudioDMAEngine", gIOServicePlane);
    FailIf (NULL == start, Exit);

    iSubMute = (IOAudioToggleControl *)FindEntryByNameAndProperty (start, "AppleUSBAudioMuteControl", kIOAudioControlSubTypeKey, kIOAudioToggleControlSubTypeLFEMute);
    start->release ();

	if ( NULL != iSubMute ) {
		iSubMute->setValue (setMute);
		iSubMute->release ();
	}

Exit:
	return result;
}
#endif
// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn AppleOnboardAudio::volumeMasterChange(SInt32 newValue){
	IOReturn						result = kIOReturnSuccess;

	DEBUG_IOLOG("+ AppleOnboardAudio::volumeMasterChange\n");

	result = kIOReturnError;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange(newValue);
	result = volumeRightChange(newValue);

//	(void)setiSubVolume (kIOAudioLevelControlSubTypeLFEVolume, ((newValue * kiSubMaxVolume) / volumeControl->getMaxValue ()) * kiSubVolumePercent / 100);

	result = kIOReturnSuccess;

	DEBUG2_IOLOG("- AppleOnboardAudio::volumeMasterChange, 0x%x\n", result);
	return result;
}

IOReturn AppleOnboardAudio::volumeLeftChange(SInt32 newValue){
	IOReturn						result;
    AudioHardwareOutput *			theOutput;
	UInt32							idx;

	DEBUG2_IOLOG("+ AppleOnboardAudio::volumeLeftChange (%ld)\n", newValue);

	result = kIOReturnError;
	FailIf (NULL == AudioOutputs, Exit);
    
	if (!gIsMute) {
		for (idx = 0; idx < AudioOutputs->getCount(); idx++) {
			theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
			if (theOutput) {
				theOutput->setVolume (newValue, gVolRight);
			}
		}
	}
    gVolLeft = newValue;

	result = kIOReturnSuccess;
Exit:
	DEBUG2_IOLOG("- AppleOnboardAudio::volumeLeftChange, 0x%x\n", result);
	return result;
}

IOReturn AppleOnboardAudio::volumeRightChange(SInt32 newValue){
	IOReturn						result;
    AudioHardwareOutput *			theOutput;
	UInt32							idx;

	DEBUG2_IOLOG("+ AppleOnboardAudio::volumeRightChange (%ld)\n", newValue);

	result = kIOReturnError;
	FailIf (NULL == AudioOutputs, Exit);

	if (!gIsMute) {
		for (idx = 0; idx < AudioOutputs->getCount(); idx++) {
			theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
			if (theOutput) {
				theOutput->setVolume (gVolLeft, newValue);
			}
		}
	}
    gVolRight = newValue;

	result = kIOReturnSuccess;
Exit:
	DEBUG2_IOLOG("- AppleOnboardAudio::volumeRightChange, result = 0x%x\n", result);
	return result;
}

IOReturn AppleOnboardAudio::outputMuteChange(SInt32 newValue){
    IOReturn						result;
	UInt32							idx;
	AudioHardwareOutput *			theOutput;

    DEBUG2_IOLOG("+ AppleOnboardAudio::outputMuteChange (%ld)\n", newValue);

    result = kIOReturnError;

	// pass it to the AudioHardwareOutputObjects
	FailIf (NULL == AudioOutputs, Exit);
        
    for (idx = 0; idx < AudioOutputs->getCount(); idx++) {
        theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
        if (theOutput) {
			theOutput->setMute (newValue);
		}
    }
    gIsMute = newValue;
    
	result = kIOReturnSuccess;
Exit:
    DEBUG2_IOLOG("- AppleOnboardAudio::outputMuteChange, 0x%x\n", result);
    return result;
}

IOReturn AppleOnboardAudio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	FailIf (NULL == audioDevice, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState && NULL != audioDevice->theAudioPowerObject) {
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->theAudioPowerObject->setHardwarePowerOn ();
		if (NULL != audioDevice->outMute) {
			audioDevice->outMute->flushValue ();					// Restore hardware to the user's selected state
		}
		wasPoweredDown = TRUE;
	} else {
		wasPoweredDown = FALSE;
	}

	switch (control->getType ()) {
		case kIOAudioControlTypeLevel:
			switch (control->getSubType ()) {
				case kIOAudioLevelControlSubTypeVolume:
					levelControl = OSDynamicCast (IOAudioLevelControl, control);
		
					switch (control->getChannelID ()) {
						case kIOAudioControlChannelIDDefaultLeft:
							result = audioDevice->gainLeftChanged (newValue);
							break;
						case kIOAudioControlChannelIDDefaultRight:
							result = audioDevice->gainRightChanged (newValue);
							break;
					}
					break;
			}
			break;
		case kIOAudioControlTypeToggle:
			switch (control->getSubType ()) {
				case kIOAudioToggleControlSubTypeMute:
					result = audioDevice->passThruChanged (newValue);
					break;
			}
			break;
		case kIOAudioControlTypeSelector:
			result = audioDevice->inputSelectorChanged (newValue);
			break;
		default:
			;
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->scheduleIdleAudioSleep ();
	}

	return result;
}
#if 0
IOReturn AppleOnboardAudio::gainLeftChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    
    IOReturn result = kIOReturnSuccess;
    AppleOnboardAudio *audioDevice;
    DEBUG_IOLOG("_ AppleOnboardAudio::gainLeftChangeHandler\n");

    audioDevice = (AppleOnboardAudio *)target;
    if(!audioDevice)
        goto BAIL;
    
    result = audioDevice->gainLeftChanged(gainControl, oldValue, newValue);

EXIT:
    DEBUG2_IOLOG("- AppleOnboardAudio::gainLeftChangeHandler, 0x%x\n", result);
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}
#endif
IOReturn AppleOnboardAudio::gainLeftChanged(SInt32 newValue){
	IOReturn result = kIOReturnSuccess;
    UInt32 idx;
    AudioHardwareInput *theInput;
    DEBUG_IOLOG("+ AppleOnboardAudio::gainLeftChanged\n");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainLeft = newValue;
	if (!mHasHardwareInputGain) {
#ifdef _AML_LOG_INPUT_GAIN
		IOLog("AppleOnboardAudio::gainLeftChanged - using software gain (0x%X).\n", gGainLeft); 
#endif
		driverDMAEngine->setInputGainL(gGainLeft);
	} else {
		for(idx = 0; idx< AudioInputs->getCount(); idx++) {
			theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
			if(theInput) 
				theInput->setInputGain(newValue, gGainRight);
		}
	}

EXIT:    
    DEBUG2_IOLOG("- AppleOnboardAudio::gainLeftChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}
#if 0
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
#endif
IOReturn AppleOnboardAudio::gainRightChanged(SInt32 newValue){
     IOReturn result = kIOReturnSuccess;
    UInt32 idx;
    AudioHardwareInput *theInput;
    DEBUG_IOLOG("+ AppleOnboardAudio::gainRightChanged\n");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainRight = newValue;
	if (!mHasHardwareInputGain) {
#ifdef _AML_LOG_INPUT_GAIN
		IOLog("AppleOnboardAudio::gainRightChanged - using software gain (0x%X).\n", gGainRight); 
#endif
		driverDMAEngine->setInputGainR(gGainRight);
	} else {
		for(idx = 0; idx< AudioInputs->getCount(); idx++) {
			theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
			if( theInput) theInput->setInputGain(gGainLeft, newValue);
		}
	}

EXIT:    
    DEBUG2_IOLOG("- AppleOnboardAudio::gainRightChanged, %d\n", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}
#if 0
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
#endif
IOReturn AppleOnboardAudio::passThruChanged(SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    DEBUG_IOLOG("+ AppleOnboardAudio::passThruChanged\n");
    gIsPlayThroughActive = newValue;
    sndHWSetPlayThrough(!newValue);
    DEBUG_IOLOG("- AppleOnboardAudio::passThruChanged\n");
    return result;
}
#if 0
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
#endif
IOReturn AppleOnboardAudio::inputSelectorChanged(SInt32 newValue){
    AudioHardwareInput *theInput;
    UInt32 idx;
	IOAudioEngine*		audioEngine;
	IOFixed				mindBVol;
	IOFixed				maxdBVol;
	IOFixed				dBOffset;
    IOReturn result = kIOReturnSuccess;
    
    DEBUG_IOLOG("+ AppleOnboardAudio::inputSelectorChanged\n");
    if(AudioInputs) {
        for(idx = 0; idx< AudioInputs->getCount(); idx++) {
            theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
            if( theInput) theInput->forceActivation(newValue);
        }
        // aml 5.28.02 change control range when using internal mic, reset to default on other switches
        if (newValue == kIntMicSource) {
		//
		// Internal mic selected
		//
			dBOffset = theAudioDeviceTreeParser->getInternalMicGainOffset();
			if (dBOffset != 0x0) {
				if (dBOffset > mDefaultInMaxDB) {
					dBOffset = mDefaultInMaxDB;
				}	
				mindBVol = mDefaultInMinDB - dBOffset;
				maxdBVol = mDefaultInMaxDB - dBOffset;
	
				audioEngine = OSDynamicCast (IOAudioEngine, audioEngines->getObject(0));
			
				audioEngine->pauseAudioEngine ();
				audioEngine->beginConfigurationChange ();
				if (NULL != inGainLeft) {
					inGainLeft->setMinDB (mindBVol);
					inGainLeft->setMaxDB (maxdBVol);
					mRangeInChanged = true;
				}

				if (NULL != inGainRight) {
					inGainRight->setMinDB (mindBVol);
					inGainRight->setMaxDB (maxdBVol);
					mRangeInChanged = true;
				}				
				audioEngine->completeConfigurationChange ();
				audioEngine->resumeAudioEngine ();
			}
	
			// aml 6.17.02
			if (mInternalMicDualMonoMode != e_Mode_Disabled) {
				driverDMAEngine->setDualMonoMode(mInternalMicDualMonoMode);
			}

        } else {
		//
		// Non-internal mic source
		//
			// change the input range back if needed
			if (mRangeInChanged) {
				mRangeInChanged = false;
				audioEngine = OSDynamicCast (IOAudioEngine, audioEngines->getObject(0));
			
				audioEngine->pauseAudioEngine ();
				audioEngine->beginConfigurationChange ();
				if (NULL != inGainLeft) {
					inGainLeft->setMinDB (mDefaultInMinDB);
					inGainLeft->setMaxDB (mDefaultInMaxDB);
				}

				if (NULL != inGainRight) {
					inGainRight->setMinDB (mDefaultInMinDB);
					inGainRight->setMaxDB (mDefaultInMaxDB);
				}
				audioEngine->completeConfigurationChange ();
				audioEngine->resumeAudioEngine ();
			}
				
			// aml 6.17.02
			if (mInternalMicDualMonoMode != e_Mode_Disabled) {
				driverDMAEngine->setDualMonoMode(e_Mode_Disabled);
			}
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
        default:     // basic mute
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

// Have to call super::setAggressiveness to complete the function call
IOReturn AppleOnboardAudio::setAggressiveness(unsigned long type, unsigned long newLevel)
{
	if (type == kPMPowerSource) {
		switch (newLevel) {
			case kIOPMInternalPower:								// Running on battery only
				setIdleAudioSleepTime (kBatteryPowerDownDelayTime);
				break;
			case kIOPMExternalPower:								// Running on AC power
				// setIdleAudioSleepTime (kACPowerDownDelayTime);	// idle power down after 5 minutes
				setIdleAudioSleepTime (kNoIdleAudioPowerDown);		// don't tell us about going to the idle state
				break;
			default:
				break;
	}
	}

	return super::setAggressiveness(type, newLevel);
}

IOReturn AppleOnboardAudio::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                        IOAudioDevicePowerState newPowerState,
                                                        UInt32 *microsecondsUntilComplete)
{
	IOReturn				result;

	debug4IOLog ("+ AppleOnboardAudio::performPowerStateChange (%d, %d) -- ourPowerState = %d\n", oldPowerState, newPowerState, ourPowerState);

	if (NULL != theAudioPowerObject) {
		*microsecondsUntilComplete = theAudioPowerObject->GetTimeToChangePowerState (ourPowerState, newPowerState);
	}

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);

	if (NULL != theAudioPowerObject) {
		switch (newPowerState) {
			case kIOAudioDeviceSleep:
				if (ourPowerState == kIOAudioDeviceActive) {
					outputMuteChange (TRUE);			// Mute before turning off power
					theAudioPowerObject->setHardwarePowerOff ();
					ourPowerState = newPowerState;
				}
				break;
			case kIOAudioDeviceIdle:
				if (ourPowerState == kIOAudioDeviceActive) {
					outputMuteChange (TRUE);			// Mute before turning off power
					theAudioPowerObject->setHardwarePowerOff ();
					ourPowerState = kIOAudioDeviceSleep;
				}
				break;
			case kIOAudioDeviceActive:
				theAudioPowerObject->setHardwarePowerOn ();
				if (NULL != outMute) {
					outMute->flushValue ();					// Restore hardware to the user's selected state
				}
				ourPowerState = newPowerState;
				break;
			default:
				;
		}
	}

	debug2IOLog ("- AppleOnboardAudio::performPowerStateChange -- ourPowerState = %d\n", ourPowerState);

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
	Boolean							wasPoweredDown;

    debugIOLog ("+ AppleOnboardAudio::setModemSound\n");

	// We have to make sure the hardware is on before we can send it any control changes [3000358]
	if (kIOAudioDeviceSleep == ourPowerState && NULL != theAudioPowerObject) {
		ourPowerState = kIOAudioDeviceActive;
		theAudioPowerObject->setHardwarePowerOn ();
		if (NULL != outMute) {
			outMute->flushValue ();					// Restore hardware to the user's selected state
		}
		wasPoweredDown = TRUE;
	} else {
		wasPoweredDown = FALSE;
	}

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
	// Second half of [3000358], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		scheduleIdleAudioSleep ();
	}

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
	
		if (NULL != outVolMaster) {
			volumeRange = (outVolMaster->getMaxValue () - outVolMaster->getMinValue () + 1);
			debug2IOLog ( "AppleOnboardAudio::VolumeToPRAMValue outVolMaster volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else if (NULL != outVolLeft) {
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
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
//		curPRAMVol = pramVolume ^ 0xFF;
//		debug3IOLog ( "AppleOnboardAudio::WritePRAMVol target pramVolume = 0x%X, curPRAMVol = 0x%X\n", pramVolume, curPRAMVol );
		
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
//					err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
//					if ( kIOReturnSuccess == err ) {
//						if ( ( 0x07 & curPRAMVol ) != pramVolume ) {
//							debug3IOLog ( "PRAM Read after Write did not compare:  Write = 0x%X, Read = 0x%X\n", (unsigned int)pramVolume, (unsigned int)curPRAMVol );
//						} else {
//							debugIOLog ( "PRAM verified after write!\n" );
//						}
//					} else {
//						debugIOLog ( "Could not readXPRAM to verify write!\n" );
//					}
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

 /*
 *  Apple02Audio.h
 *  Apple02Audio
 *
 *  Created by cerveau on Mon Jun 04 2001.
 *  Copyright (c) 2001 Apple Computer Inc. All rights reserved.
 *
 */
 
 
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>

#include "Apple02Audio.h"

OSDefineMetaClassAndAbstractStructors(Apple02Audio, IOAudioDevice)

#define super IOAudioDevice

#define LOCALIZABLE 1

#pragma mark +UNIX LIKE FUNCTIONS

bool Apple02Audio::init(OSDictionary *properties)
{
    OSDictionary *AOAprop;
    debugIOLog (3, "+ Apple02Audio::init");
    if (!super::init(properties)) return false;
        
    currentDevices = 0xFFFF;
    
	mHasHardwareInputGain = true;	// aml 5.10.02
	
	mInternalMicDualMonoMode = e_Mode_Disabled;	// aml 6.17.02, turn off by default
	
	// Future for creation
    if (AOAprop = OSDynamicCast(OSDictionary, properties->getObject("AOAAttributes"))) {
        gHasModemSound = (kOSBooleanTrue == OSDynamicCast(OSBoolean, AOAprop->getObject("analogModem")));
    }

    debugIOLog (3, "- Apple02Audio::init");
    return true;
}

void Apple02Audio::free()
{
    debugIOLog (3, "+ Apple02Audio::free");
    
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

	if (idleTimer) {
		if (workLoop) {
			workLoop->removeEventSource (idleTimer);
		}

		idleTimer->release ();
		idleTimer = NULL;
	}
	if (NULL != mPowerThread) {
		thread_call_free (mPowerThread);
	}

    super::free();
    debugIOLog (3, "- Apple02Audio::free, (void)");
}

IOService* Apple02Audio::probe(IOService* provider, SInt32* score)
{
    debugIOLog (3, "+ Apple02Audio::probe");
    super::probe(provider, score);
    debugIOLog (3, "- Apple02Audio::probe");
    return (0);
}

void Apple02Audio::stop (IOService *provider) {
	mTerminating = TRUE;
	
	publishResource ("setModemSound", NULL);

	if (mPowerThread) {
		thread_call_cancel (mPowerThread);
	}

	super::stop (provider);

	return;
}

OSArray *Apple02Audio::getDetectArray(){
    return(AudioDetects);
}

#pragma mark +PORT HANDLER FUNCTIONS

IOReturn Apple02Audio::configureAudioOutputs(IOService *provider) {
    IOReturn				result = kIOReturnSuccess;   
    AudioHardwareOutput *	theOutput;
	UInt32					numOutputs;
    UInt16					idx;

    debugIOLog (3, "+ Apple02Audio::configureAudioOutputs");
    if(!theAudioDeviceTreeParser) 
        goto BAIL;

    AudioOutputs = theAudioDeviceTreeParser->createOutputsArray();
    if(!AudioOutputs)
        goto BAIL;
    
	numOutputs = AudioOutputs->getCount();
    for(idx = 0; idx < numOutputs; idx++) {
        theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(idx));
        if( theOutput) theOutput->attachAudioPluginRef((Apple02Audio *) this);       
    }
    
EXIT:
    debugIOLog (3, "- Apple02Audio::configureAudioOutputs, %d", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn Apple02Audio::configureAudioDetects(IOService *provider) {
    IOReturn result = kIOReturnSuccess;   
    debugIOLog (3, "+ Apple02Audio::configureAudioDetects");
    
    if(!theAudioDeviceTreeParser) 
        goto BAIL;

    AudioDetects = theAudioDeviceTreeParser->createDetectsArray();

EXIT:
    debugIOLog (3, "- Apple02Audio::configureAudioDetects, %d ", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn Apple02Audio::configureAudioInputs(IOService *provider) {
    IOReturn				result = kIOReturnSuccess; 
    UInt16					idx;
    AudioHardwareInput *	theInput;
    AudioHardwareMux *		theMux;
	UInt32					numInputs;

    debugIOLog (3, "+ Apple02Audio::configureAudioDetects");

    FailIf (NULL == theAudioDeviceTreeParser, BAIL);

    AudioInputs = theAudioDeviceTreeParser->createInputsArrayWithMuxes();

	if (NULL != AudioInputs) {
		numInputs = AudioInputs->getCount();
		for(idx = 0; idx < numInputs; idx++) {
			theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
			if (NULL != theInput) {
				theInput->attachAudioPluginRef((Apple02Audio *) this);       
			} else {
				theMux = OSDynamicCast(AudioHardwareMux, AudioInputs->getObject(idx));
				if (NULL != theMux) {
					theMux->attachAudioPluginRef((Apple02Audio *) this);
				} else {
					debugIOLog (3, "!!!It's not an input and it's not a mux!!!");
				}
			}
		}
	}

EXIT:
    debugIOLog (3, "- %d = Apple02Audio::configureAudioDetects", result);
    return (result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


IOReturn Apple02Audio::parseAndActivateInit(IOService *provider){
    IOReturn result = kIOReturnSuccess;   
    SInt16 initType = 0;
    debugIOLog (3, "+ Apple02Audio::parseAndActivateInit");
    
    if(!theAudioDeviceTreeParser) 
        goto BAIL;
    
    initType = theAudioDeviceTreeParser->getInitOperationType();

    if(2 == initType) 
        sndHWSetProgOutput(kSndHWProgOutput0);
EXIT:
    debugIOLog (3, "- Apple02Audio::parseAndActivateInit, %d", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}


UInt32 Apple02Audio::getCurrentDevices(){
    return(currentDevices);
}

void Apple02Audio::setCurrentDevices(UInt32 devices){
    UInt32					odevice;

	odevice = 0;
    if (devices != currentDevices) {
        odevice = currentDevices;
        currentDevices = devices;
        changedDeviceHandler(odevice);
    }
    
	debugIOLog (3, "currentDevices = %ld", currentDevices);
	debugIOLog (3, "fCPUNeedsPhaseInversion = %d", fCPUNeedsPhaseInversion);
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
	if (devices & kSndHWInputDevices || odevice & kSndHWInputDevices) {
		if (NULL != inputConnection) {
			OSNumber *			inputState;
			UInt32				active;

			active = devices & kSndHWInputDevices ? 1 : 0;		// If something is plugged in, that's good enough for now.
			inputState = OSNumber::withNumber ((long long unsigned int)active, 32);
			(void)inputConnection->hardwareValueChanged (inputState);
			inputState->release ();
		}
	}
	// end [2829546]
}

void Apple02Audio::changedDeviceHandler(UInt32 olddevices){
    UInt16					i;
    AudioHardwareOutput *	theOutput;
	UInt32					numOutputs;

    if(AudioOutputs) {
		numOutputs = AudioOutputs->getCount();
        for(i = 0; i < numOutputs; i++) {
            theOutput = OSDynamicCast(AudioHardwareOutput, AudioOutputs->getObject(i));
            if( theOutput) theOutput->deviceIntService(currentDevices);
        }
    }
}

#pragma mark +IOAUDIO INIT
bool Apple02Audio::initHardware (IOService * provider) {
	bool								result;

	result = FALSE;

	mInitHardwareThread = thread_call_allocate ((thread_call_func_t)Apple02Audio::initHardwareThread, (thread_call_param_t)this);
	FailIf (NULL == mInitHardwareThread, Exit);

	thread_call_enter1 (mInitHardwareThread, (void *)provider);

	result = TRUE;

Exit:
	return result;
}

void Apple02Audio::initHardwareThread (Apple02Audio * aoa, void * provider) {
	IOCommandGate *						cg;
	IOReturn							result;

	FailIf (NULL == aoa, Exit);
	FailIf (TRUE == aoa->mTerminating, Exit);	

	cg = aoa->getCommandGate ();
	if (cg) {
		result = cg->runAction (aoa->initHardwareThreadAction, provider);
	}

Exit:
	return;
}

IOReturn Apple02Audio::initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4) {
	Apple02Audio *					aoa;
	IOReturn							result;

	result = kIOReturnError;

	aoa = (Apple02Audio *)owner;
	FailIf (NULL == aoa, Exit);

	result = aoa->protectedInitHardware ((IOService *)provider);

Exit:
	return result;
}

IOReturn Apple02Audio::protectedInitHardware (IOService * provider) {
	IOWorkLoop *			workLoop;
	IOAudioStream *			inputStream;
	IOAudioStream *			outputStream;
    bool					result;

    debugIOLog (3, "+ Apple02Audio::initHardware");

	result = FALSE;
    if (!super::initHardware (provider)) {
        goto EXIT;
    }

	mPowerThread = thread_call_allocate((thread_call_func_t)Apple02Audio::performPowerStateChangeThread, (thread_call_param_t)this);
	FailIf (NULL == mPowerThread, EXIT);

    sndHWInitialize (provider);
    theAudioDeviceTreeParser = AudioDeviceTreeParser::createWithEntryProvider (provider);
 
#if LOCALIZABLE
    setDeviceName ("DeviceName");
    setDeviceShortName ("DeviceShortName");
    setManufacturerName ("ManufacturerName");
    setProperty (kIOAudioDeviceLocalizedBundleKey, "Apple02Audio.kext");
#else
    setDeviceName ("Built-in Audio");
    setDeviceShortName ("Built-in");
    setManufacturerName ("Apple");
#endif
	setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);

	setProperty (kIOAudioEngineCoreAudioPlugInKey, "IOAudioFamily.kext/Contents/PlugIns/AOAHALPlugin.bundle");

    parseAndActivateInit (provider);
    configureAudioDetects (provider);
    configureAudioOutputs (provider);
    configureAudioInputs (provider);
    configurePowerObject (provider);

    configureDMAEngines (provider);

	// Have to create the audio controls before calling activateAudioEngine
	gExpertMode = TRUE;			// Don't update the PRAM value while we're initing from it
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
		debugIOLog (3, "didn't get the output stream");
	}

	inputStream = driverDMAEngine->getAudioStream (kIOAudioStreamDirectionInput, 1);
	if (inputStream) {
		inputStream->setTerminalType (INPUT_MICROPHONE);
	} else {
		debugIOLog (3, "didn't get the input stream");
	}

	idleTimer = IOTimerEventSource::timerEventSource (this, sleepHandlerTimer);
	if (!idleTimer) {
		goto EXIT;
	}
	workLoop->addEventSource (idleTimer);

	// Set this to a default for desktop machines (portables will get a setAggressiveness call later in the boot sequence).
	ourPowerState = kIOAudioDeviceIdle;
	setProperty ("IOAudioPowerState", ourPowerState, 32);
	idleSleepDelayTime = kNoIdleAudioPowerDown;
	// [3107909] Turn the hardware off because IOAudioFamily defaults to the off state, so make sure the hardware is off or we get out of synch with the family.
	setIdleAudioSleepTime (idleSleepDelayTime);
	if (NULL != theAudioPowerObject) {
		theAudioPowerObject->setIdlePowerState ();
	}

	// Give drivers a chance to do something after the DMA engine and IOAudioFamily have been created/started
	sndHWPostDMAEngineInit (provider);

	// Set the default volume to that stored in the PRAM in case we don't get a setValue call from the Sound prefs before being activated.
//	if (NULL != outVolLeft) {
//		outVolLeft->setValue (PRAMToVolumeValue ());
//	}
//	if (NULL != outVolRight) {
//		outVolRight->setValue (PRAMToVolumeValue ());
//	}

    flushAudioControls ();

    publishResource ("setModemSound", this);

	// Install power change handler so we get notified about shutdown
	registerPrioritySleepWakeInterest (&sysPowerDownHandler, this, 0);

	// Tell the world about us so the User Client can find us.
	registerService ();

    mHasHardwareInputGain = theAudioDeviceTreeParser->getHasHWInputGain();
	if (mHasHardwareInputGain) {
		driverDMAEngine->setUseSoftwareInputGain(false);
	} else {
		driverDMAEngine->setUseSoftwareInputGain(true);
	}

	sndHWPostThreadedInit (provider); // [3284411]
	gExpertMode = FALSE;

	result = TRUE;

EXIT:
    debugIOLog (3, "- Apple02Audio::initHardware"); 
    return (result);
}

IOReturn Apple02Audio::configureDMAEngines(IOService *provider){
    IOReturn 			result;
    bool				hasInput;
    
    result = kIOReturnError;

	// All this config should go in a single method
	FailIf (NULL == theAudioDeviceTreeParser, EXIT);

    if (theAudioDeviceTreeParser->getNumberOfInputs () > 0)
        hasInput = true;
    else 
        hasInput = false;
        
    driverDMAEngine = new Apple02DBDMAAudioDMAEngine;
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

IOReturn Apple02Audio::createDefaultsPorts () {
    IOAudioPort *				outputPort = NULL;
    IOAudioPort *				inputPort = NULL;
    OSDictionary *				AOAprop = NULL;
	OSDictionary *				theRange = NULL;
    OSNumber *					theNumber;
    OSData *					theData;
	AudioHardwareInput *		theInput;
	UInt32						numInputs;
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
	UInt32						pramVolValue;
    IOReturn					result;
	Boolean						done;
	Boolean						hasPlaythrough;

    debugIOLog (3, "+ Apple02Audio::createDefaultsPorts");

	hasPlaythrough = FALSE;
	result = kIOReturnSuccess;
	FailIf (NULL == driverDMAEngine, BAIL);
	FailIf (NULL == (AOAprop = OSDynamicCast (OSDictionary, this->getProperty("AOAAttributes"))), BAIL);

	// [2731278] Create output selector that is used to tell the HAL what the current output is (speaker, headphone, etc.)
	outputSelector = IOAudioSelectorControl::createOutputSelector ('ispk', kIOAudioControlChannelIDAll);
	if (NULL != outputSelector) {
		driverDMAEngine->addDefaultAudioControl (outputSelector);
		outputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		outputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, "IntSpeakers");
		outputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeHeadphones, "Headphones");
		outputSelector->setReadOnlyFlag ();		// 3292105
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

			pramVolValue = PRAMToVolumeValue ();
#if 1
			pramVol = IOAudioLevelControl::create(pramVolValue, 0, 7, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDAll,
												"BootBeepVolume",
												kPRAMVol, 
												kIOAudioLevelControlSubTypePRAMVolume,
												kIOAudioControlUsageOutput);
			if (NULL != pramVol) {
				driverDMAEngine->addDefaultAudioControl(pramVol);
				pramVol->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				// Don't release it because we might reference it later
			}
#endif
			outVolLeft = IOAudioLevelControl::createVolumeControl((OutmaxLin - OutminLin + 1) * 75 / 100, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
												kIOAudioControlChannelIDDefaultLeft,
												kIOAudioControlChannelNameLeft,
												kOutVolLeft, 
												kIOAudioControlUsageOutput);
			if (NULL != outVolLeft) {
				driverDMAEngine->addDefaultAudioControl(outVolLeft);
				outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				// Don't release it because we might reference it later
			}

			outVolRight = IOAudioLevelControl::createVolumeControl((OutmaxLin - OutminLin + 1) * 75 / 100, OutminLin, OutmaxLin, OutminDB, OutmaxDB,
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
				headphoneConnection->setReadOnlyFlag ();		// 3292105
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
			numInputs = AudioInputs->getCount();
			for (idx = 0; idx < numInputs; idx++) {
				theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
				if (theInput) {
					inputType = theInput->getInputPortType();
					debugIOLog (3, "Creating input selector of type %4s", (char*)&inputType);
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
						debugIOLog (3, "Calling addAvailableSelection with type %4s", (char*)&inputType);
						switch(inputType) {
							case 'imic' :
								inputSelector->addAvailableSelection('imic', "InternalMic");
								break;
							case 'emic' :
								inputSelector->addAvailableSelection('emic', "ExternalMic");
								break;
							case 'sinj' :
								inputSelector->addAvailableSelection('sinj', "SoundIn");
								break;
							case 'line' :
								inputSelector->addAvailableSelection('line', "LineIn");
								break;
							case 'zvpc' :
								inputSelector->addAvailableSelection('zvpc', "ZoomedVideo");
								break;
							default:
								break;
						}
						// Don't release inputSelector because we might use it later in setModemSound
					}
				}
			}

			done = FALSE;
			for (idx = 0; idx < numInputs && !done; idx++) {
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
								inputConnection->setReadOnlyFlag ();		// 3292105
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
    debugIOLog (3, "- %d = Apple02Audio::createDefaultsPorts", result);
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

#pragma mark +IOAUDIO CONTROL HANDLERS

IORegistryEntry * Apple02Audio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

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

IOReturn Apple02Audio::outputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	Apple02Audio *					audioDevice;
	IOAudioLevelControl *			levelControl;
	IODTPlatformExpert * 			platform;
	UInt32							leftVol;
	UInt32							rightVol;
	Boolean							wasPoweredDown;

	debugIOLog (3, "+ Apple02Audio::outputControlChangeHandler (%p, %p, %ld, %ld)", target, control, oldValue, newValue);

	audioDevice = OSDynamicCast (Apple02Audio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);
	FailIf (NULL == control, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState && NULL != audioDevice->theAudioPowerObject) {
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->theAudioPowerObject->setHardwarePowerOn ();
		if (NULL != audioDevice->outMute) {
			audioDevice->outMute->flushValue ();					// Restore hardware to the user's selected state
		}
		wasPoweredDown = TRUE;
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
						muteState->release ();
					} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
						OSNumber *			muteState;
						muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
						if (NULL != audioDevice->outMute) {
							audioDevice->outMute->hardwareValueChanged (muteState);
						}
						muteState->release ();
					}
					break;
				case kIOAudioControlChannelIDDefaultLeft:
					result = audioDevice->volumeLeftChange (newValue);
					break;
				case kIOAudioControlChannelIDDefaultRight:
					result = audioDevice->volumeRightChange (newValue);
					break;
			}
			break;
		case kIOAudioToggleControlSubTypeMute:
			if (audioDevice->gVolLeft != 0 && audioDevice->gVolRight != 0) {
				result = audioDevice->outputMuteChange (newValue);
				if (kIOReturnSuccess == result) {
					// Have to set this here, because we'll call outputMuteChange just to avoid pops, and we don't want gIsMute set in that case.
					audioDevice->gIsMute = newValue;
				}
			}
			break;
		case kIOAudioSelectorControlSubTypeOutput:
			result = kIOReturnUnsupported;
			break;
#if 1
		case kIOAudioLevelControlSubTypePRAMVolume:
			platform = OSDynamicCast (IODTPlatformExpert, getPlatform());
			if (platform) {
				UInt8 							curPRAMVol;
				result = platform->readXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
				curPRAMVol = (curPRAMVol & 0xF8) | newValue;
				result = platform->writeXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount) 1);
			}
			break;
#endif
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
				muteState->release ();
//				debugIOLog (3, "turning on hardware mute flag");
			} else if (newValue != levelControl->getMinValue () && oldValue == levelControl->getMinValue () && FALSE == audioDevice->gIsMute) {
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
				if (NULL != audioDevice->outMute) {
					audioDevice->outMute->hardwareValueChanged (muteState);
				}
				muteState->release ();
//				debugIOLog (3, "turning off hardware mute flag");
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
		OSNumber *			newVolume;
//		debugIOLog (3, "updated volume, setting PRAM");
		audioDevice->WritePRAMVol (leftVol, rightVol);
		newVolume = OSNumber::withNumber ((long long unsigned int)audioDevice->ReadPRAMVol (), 32);
		if (NULL != audioDevice->pramVol) {
			audioDevice->pramVol->hardwareValueChanged (newVolume);
		}
		newVolume->release ();
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->setTimerForSleep ();
	}
	debugIOLog (3, "- Apple02Audio::outputControlChangeHandler (%p, %p, %ld, %ld) returns %X", target, control, oldValue, newValue, result);

	return result;
}

// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn Apple02Audio::volumeMasterChange(SInt32 newValue){
	IOReturn						result = kIOReturnSuccess;

	debugIOLog (3, "+ Apple02Audio::volumeMasterChange");

	result = kIOReturnError;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange(newValue);
	result = volumeRightChange(newValue);

//	(void)setiSubVolume (kIOAudioLevelControlSubTypeLFEVolume, ((newValue * kiSubMaxVolume) / volumeControl->getMaxValue ()) * kiSubVolumePercent / 100);

	result = kIOReturnSuccess;

	debugIOLog (3, "- Apple02Audio::volumeMasterChange, 0x%x", result);
	return result;
}

IOReturn Apple02Audio::volumeLeftChange(SInt32 newValue){
	IOReturn						result;
    AudioHardwareOutput *			theOutput;
	UInt32							idx;
	UInt32							numOutputs;

	debugIOLog (3, "+ Apple02Audio::volumeLeftChange (%ld)", newValue);

	result = kIOReturnError;
	FailIf (NULL == AudioOutputs, Exit);
    
	debugIOLog (3, "... gIsMute %d, AudioOutputs->getCount() = %d", gIsMute, AudioOutputs->getCount());
	if (!gIsMute) {
		numOutputs = AudioOutputs->getCount();
		for (idx = 0; idx < numOutputs; idx++) {
			theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
			if (theOutput) {
				debugIOLog (3,  "... %X theOutput->setVolume ( %X, %X )", (unsigned int)theOutput, (unsigned int)newValue, (unsigned int)gVolRight );
				theOutput->setVolume (newValue, gVolRight);
			} else {
				debugIOLog (3, "... ### NO AudioHardwareOutput*");
			}
		}
	}
    gVolLeft = newValue;

	result = kIOReturnSuccess;
Exit:
	debugIOLog (3, "- Apple02Audio::volumeLeftChange, 0x%x", result);
	return result;
}

IOReturn Apple02Audio::volumeRightChange(SInt32 newValue){
	IOReturn						result;
    AudioHardwareOutput *			theOutput;
	UInt32							idx;
	UInt32							numOutputs;

	debugIOLog (3, "+ Apple02Audio::volumeRightChange (%ld)", newValue);

	result = kIOReturnError;
	FailIf (NULL == AudioOutputs, Exit);

	debugIOLog (3, "... gIsMute %d, AudioOutputs->getCount() = %d", gIsMute, AudioOutputs->getCount());
	if (!gIsMute) {
		numOutputs = AudioOutputs->getCount();
		for (idx = 0; idx < numOutputs; idx++) {
			theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
			if (theOutput) {
				debugIOLog (3,  "... %X theOutput->setVolume ( %X, %X )", (unsigned int)theOutput, (unsigned int)newValue, (unsigned int)gVolRight );
				theOutput->setVolume (gVolLeft, newValue);
			} else {
				debugIOLog (3, "... ### NO AudioHardwareOutput*");
			}
		}
	}
    gVolRight = newValue;

	result = kIOReturnSuccess;
Exit:
	debugIOLog (3, "- Apple02Audio::volumeRightChange, result = 0x%x", result);
	return result;
}

IOReturn Apple02Audio::outputMuteChange(SInt32 newValue){
    IOReturn						result;
	UInt32							idx;
	UInt32							numOutputs;
	AudioHardwareOutput *			theOutput;

    debugIOLog (3, "+ Apple02Audio::outputMuteChange (%ld)", newValue);

    result = kIOReturnError;

	// pass it to the AudioHardwareOutputObjects
	FailIf (NULL == AudioOutputs, Exit);

	numOutputs = AudioOutputs->getCount();
    for (idx = 0; idx < numOutputs; idx++) {
        theOutput = OSDynamicCast (AudioHardwareOutput, AudioOutputs->getObject (idx));
        if (theOutput) {
			theOutput->setMute (newValue);
		}
    }
//    gIsMute = newValue;
    
	result = kIOReturnSuccess;
Exit:
    debugIOLog (3, "- Apple02Audio::outputMuteChange, 0x%x", result);
    return result;
}

IOReturn Apple02Audio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	Apple02Audio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	audioDevice = OSDynamicCast (Apple02Audio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState && NULL != audioDevice->theAudioPowerObject) {
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->theAudioPowerObject->setHardwarePowerOn ();
		if (NULL != audioDevice->outMute) {
			audioDevice->outMute->flushValue ();					// Restore hardware to the user's selected state
		}
		wasPoweredDown = TRUE;
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
		audioDevice->setTimerForSleep ();
	}

	return result;
}

IOReturn Apple02Audio::gainLeftChanged(SInt32 newValue){
	IOReturn				result = kIOReturnSuccess;
    UInt32					idx;
	UInt32					numInputs;
    AudioHardwareInput *	theInput;

    debugIOLog (3, "+ Apple02Audio::gainLeftChanged");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainLeft = newValue;
	if (!mHasHardwareInputGain) {
#ifdef _AML_LOG_INPUT_GAIN
		debugIOLog (3, "Apple02Audio::gainLeftChanged - using software gain (0x%X).", gGainLeft); 
#endif
		driverDMAEngine->setInputGainL(gGainLeft);
	} else {
		numInputs = AudioInputs->getCount();
		for(idx = 0; idx < numInputs; idx++) {
			theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
			if(theInput) 
				theInput->setInputGain(newValue, gGainRight);
		}
	}

EXIT:    
    debugIOLog (3, "- Apple02Audio::gainLeftChanged, %d", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn Apple02Audio::gainRightChanged(SInt32 newValue){
	IOReturn				result = kIOReturnSuccess;
    UInt32					idx;
	UInt32					numInputs;
    AudioHardwareInput *	theInput;

    debugIOLog (3, "+ Apple02Audio::gainRightChanged");    
    
    if(!AudioInputs)
        goto BAIL;
        
    gGainRight = newValue;
	if (!mHasHardwareInputGain) {
#ifdef _AML_LOG_INPUT_GAIN
		debugIOLog (3, "Apple02Audio::gainRightChanged - using software gain (0x%X).", gGainRight); 
#endif
		driverDMAEngine->setInputGainR(gGainRight);
	} else {
		numInputs = AudioInputs->getCount();
		for(idx = 0; idx< numInputs; idx++) {
			theInput = OSDynamicCast(AudioHardwareInput, AudioInputs->getObject(idx));
			if( theInput) theInput->setInputGain(gGainLeft, newValue);
		}
	}

EXIT:    
    debugIOLog (3, "- Apple02Audio::gainRightChanged, %d", (result == kIOReturnSuccess));
    return result;
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

IOReturn Apple02Audio::passThruChanged(SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    debugIOLog (3, "+ Apple02Audio::passThruChanged");
    gIsPlayThroughActive = newValue;
    sndHWSetPlayThrough(!newValue);
    debugIOLog (3, "- Apple02Audio::passThruChanged");
    return result;
}

IOReturn Apple02Audio::inputSelectorChanged(SInt32 newValue){
    AudioHardwareInput *theInput;
    UInt32				idx;
	UInt32				numInputs;
	IOAudioEngine*		audioEngine;
	IOFixed				mindBVol;
	IOFixed				maxdBVol;
	IOFixed				dBOffset;
    IOReturn			result = kIOReturnSuccess;
    
    debugIOLog (3, "+ Apple02Audio::inputSelectorChanged");
    if(AudioInputs) {
		numInputs = AudioInputs->getCount();
        for(idx = 0; idx < numInputs; idx++) {
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
			
//				audioEngine->pauseAudioEngine ();	// Turn these off to fix 3267775
//				audioEngine->beginConfigurationChange ();
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
//				audioEngine->completeConfigurationChange ();
//				audioEngine->resumeAudioEngine ();
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
			
//				audioEngine->pauseAudioEngine ();	// Turn these off to fix 3267775
//				audioEngine->beginConfigurationChange ();
				if (NULL != inGainLeft) {
					inGainLeft->setMinDB (mDefaultInMinDB);
					inGainLeft->setMaxDB (mDefaultInMaxDB);
				}

				if (NULL != inGainRight) {
					inGainRight->setMinDB (mDefaultInMinDB);
					inGainRight->setMaxDB (mDefaultInMaxDB);
				}
//				audioEngine->completeConfigurationChange ();
//				audioEngine->resumeAudioEngine ();
			}
				
			// aml 6.17.02
			if (mInternalMicDualMonoMode != e_Mode_Disabled) {
				driverDMAEngine->setDualMonoMode(e_Mode_Disabled);
			}
	}    

    }  
    debugIOLog (3, "- Apple02Audio::inputSelectorChanged");
    return result;
}


#pragma mark +POWER MANAGEMENT
IOReturn Apple02Audio::configurePowerObject(IOService *provider){
    IOReturn result = kIOReturnSuccess;

    debugIOLog (3, "+ Apple02Audio::configurePowerObject");
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
    debugIOLog (3, "- Apple02Audio::configurePowerObject result = %d", (result == kIOReturnSuccess));
    return(result);
BAIL:
    result = kIOReturnError;
    goto EXIT;
}

void Apple02Audio::setTimerForSleep () {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	if (idleTimer && idleSleepDelayTime != kNoIdleAudioPowerDown) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += idleSleepDelayTime;
		nanoseconds_to_absolutetime (nanos, &fireTime);
		idleTimer->wakeAtTime (fireTime);		// will call idleAudioSleepHandlerTimer
	}
}

void Apple02Audio::sleepHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
	Apple02Audio *				audioDevice;
	UInt32							time = 0;

	audioDevice = OSDynamicCast (Apple02Audio, owner);
	FailIf (NULL == audioDevice, Exit);

	if (audioDevice->getPowerState () != kIOAudioDeviceActive) {
		audioDevice->performPowerStateChange (audioDevice->getPowerState (), kIOAudioDeviceIdle, &time);
	}

Exit:
	return;
}

// Have to call super::setAggressiveness to complete the function call
IOReturn Apple02Audio::setAggressiveness(unsigned long type, unsigned long newLevel) {
	IOReturn				result;
	UInt32					time = 0;

	debugIOLog (3,  "+ Apple02Audio::setAggressiveness ( %ld, %ld )", type, newLevel );
	if (type == kPMPowerSource) {
		debugIOLog (3, "setting power aggressivness state to ");
		switch (newLevel) {
			case kIOPMInternalPower:								// Running on battery only
				debugIOLog (3, "battery power");
				idleSleepDelayTime = kBatteryPowerDownDelayTime;
				setIdleAudioSleepTime (idleSleepDelayTime);
				if (!asyncPowerStateChangeInProgress && getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceIdle, &time);
				}
				break;
			case kIOPMExternalPower:								// Running on AC power
				debugIOLog (3, "wall power");
				// idleSleepDelayTime = kACPowerDownDelayTime;		// idle power down after 5 minutes
				idleSleepDelayTime = kNoIdleAudioPowerDown;
				setIdleAudioSleepTime (idleSleepDelayTime);			// don't tell us about going to the idle state
				if (!asyncPowerStateChangeInProgress && getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceActive, &time);
				}
				break;
			default:
				debugIOLog (3,  "UNKOWN STATE %ld", newLevel );
				break;
		}
	}

	result = super::setAggressiveness(type, newLevel);
	debugIOLog (3,  "- Apple02Audio::setAggressiveness ( %ld, %ld )", type, newLevel );
	return result;
}

// Do all this work on a thread because it can take a considerable amount of time which will delay other software from being able to wake.
IOReturn Apple02Audio::performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microsecondsUntilComplete)
{
	IOReturn				result;

	debugIOLog (3, "+ Apple02Audio::performPowerStateChange (%d, %d) -- ourPowerState = %d", oldPowerState, newPowerState, ourPowerState);

	if (NULL != theAudioPowerObject) {
		*microsecondsUntilComplete = theAudioPowerObject->GetTimeToChangePowerState (ourPowerState, newPowerState);
		if (*microsecondsUntilComplete == 0) {
			*microsecondsUntilComplete =  1;			// Since we are spawning a thread, we have to return a non-zero value.
		}
	}

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);
    
	if ( kIOAudioDeviceIdle == ourPowerState && kIOAudioDeviceActive == newPowerState ) {
		result = performPowerStateChangeThreadAction ( this, (void*)newPowerState, 0, 0, 0 );
	} else {
		if (mPowerThread) {
			thread_call_enter1(mPowerThread, (thread_call_param_t)newPowerState);
		}
	}

	debugIOLog (3, "- Apple02Audio::performPowerStateChange -- ourPowerState = %d", ourPowerState);

	return result;
}

void Apple02Audio::performPowerStateChangeThread (Apple02Audio * aoa, thread_call_param_t newPowerState) {
	IOCommandGate *			cg;

	FailIf (NULL == aoa, Exit);

	FailIf (TRUE == aoa->mTerminating, Exit);	
	cg = aoa->getCommandGate ();
	if (cg) {
		cg->runAction (aoa->performPowerStateChangeThreadAction, newPowerState);
	}

Exit:
	return;
}

IOReturn Apple02Audio::performPowerStateChangeThreadAction (OSObject * owner, void * newPowerState, void * arg2, void * arg3, void * arg4) {
	Apple02Audio *			aoa;
	IOReturn				result;
	
	aoa = (Apple02Audio *)owner;
	debugIOLog (3, "+ Apple02Audio::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	
	result = kIOReturnError;
	
	FailIf (NULL == aoa, Exit);
	
	if (NULL != aoa->theAudioPowerObject) {
		switch ((UInt32)newPowerState) {
			case kIOAudioDeviceSleep:
				if (kIOAudioDeviceSleep != aoa->ourPowerState) {				//	[3193592]
					aoa->outputMuteChange (TRUE);							// Mute before turning off power
					aoa->ourPowerState = kIOAudioDeviceSleep;
					debugIOLog (3, "going to sleep state");
					aoa->theAudioPowerObject->setHardwarePowerOff ();
				}
				break;
			case kIOAudioDeviceIdle:
				if ((kIOAudioDeviceActive == aoa->ourPowerState) || ( kIOAudioDeviceIdle == aoa->ourPowerState )) {	//	[3361022, 3366480]
					aoa->outputMuteChange (TRUE);							// Mute before turning off power
					aoa->ourPowerState = kIOAudioDeviceIdle;					//	[3193592]
					debugIOLog (3, "going idle from active state");
					aoa->theAudioPowerObject->setIdlePowerState ();			//	[3193592]
				} else if (kIOAudioDeviceSleep == aoa->ourPowerState && kNoIdleAudioPowerDown == aoa->idleSleepDelayTime) {
					aoa->ourPowerState = kIOAudioDeviceActive;
					debugIOLog (3, "going active from sleep state");
					aoa->theAudioPowerObject->setHardwarePowerOn ();
					if (NULL != aoa->outMute) {
						aoa->outMute->flushValue ();							// Restore hardware to the user's selected state
					}
				} else if (kIOAudioDeviceSleep == aoa->ourPowerState) {
					debugIOLog (3, "going idle from sleep state");
					aoa->theAudioPowerObject->setHardwarePowerIdleOn ();		//	[3193592]
					//	[3361022, 3366480]	begin {
					aoa->setTimerForSleep();									
					//	}	end	[3361022, 3366480]
					aoa->ourPowerState = kIOAudioDeviceIdle;					//	[3193592]
					if (NULL != aoa->outMute) {
						aoa->outMute->flushValue ();							// Restore hardware to the user's selected state
					}
				} else {
					debugIOLog (3, "Trying to go idle, but we already are");
				}
				break;
			case kIOAudioDeviceActive:
				if (kIOAudioDeviceActive != aoa->ourPowerState) {
					//	[3361022, 3366480]	begin {
					if ( aoa->idleTimer ) {
						aoa->idleTimer->cancelTimeout ();
					}
					//	} end	[3361022, 3366480]
					aoa->ourPowerState = kIOAudioDeviceActive;
					debugIOLog (3, "going to active state");
					aoa->theAudioPowerObject->setHardwarePowerOn ();
					if (NULL != aoa->outMute) {
						aoa->outMute->flushValue ();							// Restore hardware to the user's selected state
					}
				} else {
					debugIOLog (3, "trying to wake, but we're already awake");
				}
				break;
			default:
				;
		}
	}	

	aoa->setProperty ("IOAudioPowerState", aoa->ourPowerState, 32);

	result = kIOReturnSuccess;

Exit:
	aoa->protectedCompletePowerStateChange ();

	debugIOLog (3, "- Apple02Audio::performPowerStateChangeThreadAction -- ourPowerState = %d", aoa->ourPowerState);
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
IOReturn Apple02Audio::sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize) {
	Apple02Audio *				appleOnboardAudio;
	IOReturn						result;
//	char							message[100];

	result = kIOReturnUnsupported;
	appleOnboardAudio = OSDynamicCast (Apple02Audio, (OSObject *)target);
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
IOReturn Apple02Audio::setModemSound (bool state){
    AudioHardwareInput *			theInput;
    UInt32							idx;
	UInt32							numInputs;
	Boolean							wasPoweredDown;

    debugIOLog (3, "+ Apple02Audio::setModemSound");

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
			numInputs = AudioInputs->getCount ();
            for (idx = 0; idx < numInputs; idx++) {
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
			numInputs = AudioInputs->getCount ();
			for (idx = 0; idx < numInputs; idx++) {
				theInput = OSDynamicCast (AudioHardwareInput, AudioInputs->getObject (idx));
				if (NULL != theInput && inputSelector) {
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
		setTimerForSleep ();
	}

    debugIOLog (3, "- Apple02Audio::setModemSound");
    return kIOReturnSuccess;
}

IOReturn Apple02Audio::callPlatformFunction( const OSSymbol * functionName, bool waitForFunction,void *param1, void *param2, void *param3, void *param4 ) {
    debugIOLog (3, "+ Apple02Audio::callPlatformFunction");
    if (functionName->isEqualTo ("setModemSound")) {
        return (setModemSound ((bool)param1));
    }

    debugIOLog (3, "- Apple02Audio::callPlatformFunction");
    return (super::callPlatformFunction (functionName, waitForFunction,param1, param2, param3, param4));
}

#pragma mark +PRAM VOLUME
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
UInt8 Apple02Audio::VolumeToPRAMValue (UInt32 inLeftVol, UInt32 inRightVol) {
	UInt32			pramVolume;						// Volume level to store in PRAM
	UInt32 			averageVolume;					// summed volume
    UInt32		 	volumeRange;
    UInt32 			volumeSteps;
	UInt32			leftVol;
	UInt32			rightVol;

	debugIOLog (3,  "+ Apple02Audio::VolumeToPRAMValue ( 0x%X, 0x%X )", (unsigned int)inLeftVol, (unsigned int)inRightVol );
	pramVolume = 0;											//	[2886446]	Always pass zero as a result when muting!!!
	if ( ( 0 != inLeftVol ) || ( 0 != inRightVol ) ) {		//	[2886446]
		leftVol = inLeftVol;
		rightVol = inRightVol;
		if (NULL != outVolLeft) {
			leftVol -= outVolLeft->getMinValue ();
		}
	
		if (NULL != outVolRight) {
			rightVol -= outVolRight->getMinValue ();
		}
		debugIOLog (3,  "... leftVol = 0x%X, rightVol = 0x%X", (unsigned int)leftVol, (unsigned int)rightVol );
	
		if (NULL != outVolMaster) {
			volumeRange = (outVolMaster->getMaxValue () - outVolMaster->getMinValue () + 1);
			debugIOLog (3,  "... outVolMaster volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != outVolLeft) {
			volumeRange = (outVolLeft->getMaxValue () - outVolLeft->getMinValue () + 1);
			debugIOLog (3,  "... outVolLeft volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != outVolRight) {
			volumeRange = (outVolRight->getMaxValue () - outVolRight->getMinValue () + 1);
			debugIOLog (3,  "... outVolRight volumeRange = 0x%X", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debugIOLog (3,  "... volumeRange = 0x%X **** NO AUDIO LEVEL CONTROLS!", (unsigned int)volumeRange );
		}

		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debugIOLog (3,  "... averageVolume = 0x%X", (unsigned int)volumeRange );
		debugIOLog (3,  "... volumeRange %X, kMaximumPRAMVolume %X", (unsigned int)volumeRange, (unsigned int)kMaximumPRAMVolume );
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
	debugIOLog (3,  "- Apple02Audio::VolumeToPRAMValue returns 0x%X", (unsigned int)pramVolume );
	return (pramVolume & 0x07);
}

UInt32 Apple02Audio::PRAMToVolumeValue (void) {
	UInt32		 	volumeRange;
	UInt32 			volumeSteps;

	if (NULL != outVolLeft) {
		volumeRange = (outVolLeft->getMaxValue () - outVolLeft->getMinValue () + 1);
	} else if (NULL != outVolRight) {
		volumeRange = (outVolRight->getMaxValue () - outVolRight->getMinValue () + 1);
	} else {
		OSDictionary *				AOAprop = NULL;
		OSDictionary *				theRange = NULL;
		OSNumber *					theNumber;
		SInt32						OutminLin;
		SInt32						OutmaxLin;

		volumeRange = kMaximumPRAMVolume;
		FailIf (NULL == (AOAprop = OSDynamicCast (OSDictionary, this->getProperty("AOAAttributes"))), Exit);
		FailIf (NULL == (theRange = OSDynamicCast(OSDictionary, AOAprop->getObject("RangeOut"))), Exit);
		FailIf (NULL == (theNumber = OSDynamicCast(OSNumber, theRange->getObject("minLin"))), Exit);
		OutminLin = (SInt32) theNumber->unsigned32BitValue();
		FailIf (NULL == (theNumber = OSDynamicCast(OSNumber, theRange->getObject("maxLin"))), Exit);
		OutmaxLin = (SInt32) theNumber->unsigned32BitValue();
		volumeRange = (OutmaxLin - OutminLin + 1);
	}

Exit:

	volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume

	return (volumeSteps * ReadPRAMVol ());
}

void Apple02Audio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	UInt8						pramVolume;
	UInt8 						curPRAMVol;
	IODTPlatformExpert * 		platform;
	IOReturn					err;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());

    debugIOLog (3, "+ Apple02Audio::WritePRAMVol leftVol=%lu, rightVol=%lu",leftVol,  rightVol);
    
    if (platform) {
		debugIOLog (3,  "... platform 0x%X", (unsigned int)platform );
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
#if 0
		curPRAMVol = pramVolume ^ 0xFF;
		debugIOLog (3,  "... target pramVolume = 0x%X, curPRAMVol = 0x%X", pramVolume, curPRAMVol );
#endif
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			debugIOLog (3,  "... curPRAMVol = 0x%X before write", (curPRAMVol & 0x07) );
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				debugIOLog (3, "... curPRAMVol = 0x%x",curPRAMVol);
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debugIOLog (3,  "0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
#if 0
					err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
					if ( kIOReturnSuccess == err ) {
						if ( ( 0x07 & curPRAMVol ) != pramVolume ) {
							debugIOLog (3,  "PRAM Read after Write did not compare:  Write = 0x%X, Read = 0x%X", (unsigned int)pramVolume, (unsigned int)curPRAMVol );
						} else {
							debugIOLog (3,  "PRAM verified after write!" );
						}
					} else {
						debugIOLog (3,  "Could not readXPRAM to verify write!" );
					}
#endif
				}
			} else {
				debugIOLog (3,  "PRAM write request is to current value: no I/O" );
			}
		} else {
			debugIOLog (3,  "Could not readXPRAM prior to write! Error 0x%X", err );
		}
	} else {
		debugIOLog (3,  "... no platform" );
	}
    debugIOLog (3, "- Apple02Audio::WritePRAMVol");
}

UInt8 Apple02Audio::ReadPRAMVol (void) {
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

IOReturn Apple02Audio::newUserClient( task_t 			inOwningTask,
										 void *				inSecurityID,
										 UInt32 			inType,
										 IOUserClient **	outHandler )
{
	#pragma unused( inType )
	
    IOReturn 			err;
    IOUserClient *		userClientPtr;
    bool				result;
	
	debugIOLog (3,  "[Apple02Audio] creating user client for task 0x%08lX", ( UInt32 ) inOwningTask );
	
	// Create the user client object.
	
	err = kIOReturnNoMemory;
	userClientPtr = AppleLegacyOnboardAudioUserClient::Create( this, inOwningTask );
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
	debugIOLog (3,  "[Apple02Audio] newUserClient done (err=%d)", err );
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

const IOExternalMethod		AppleLegacyOnboardAudioUserClient::sMethods[] =
{
	//	Read
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::gpioRead,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	//	Write
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::gpioWrite,		// func
		kIOUCScalarIScalarO,										// flags
		2,															// count of input parameters
		0															// count of output parameters
	},
	// gpioGetActiveState
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::gpioGetActiveState,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// gpioSetActiveState
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::gpioSetActiveState,		// func
		kIOUCScalarIScalarO,										// flags
		2,															// count of input parameters
		0															// count of output parameters
	},
	// gpioCheckIfAvailable
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::gpioCheckAvailable,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// hwRegisterRead32
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::hwRegisterRead32,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// hwRegisterWrite32
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::hwRegisterWrite32,		// func
		kIOUCScalarIScalarO,										// flags
		2,															// count of input parameters
		0															// count of output parameters
	},
	// codecReadRegister
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::codecReadRegister,		// func
		kIOUCScalarIStructO,										// flags
		1,															// count of input parameters
		kMaxCodecStructureSize										// size of output structure
	},
	// codecWriteRegister
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::codecWriteRegister,		// func
		kIOUCScalarIStructI,										// flags
		1,															// count of input parameters
		kMaxCodecRegisterWidth										// size of input structure
	},
	// readSpeakerID
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::readSpeakerID,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// codecRegisterSize
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::codecRegisterSize,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// readPRAMVolume
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::readPRAMVolume,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// readDMAState
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::readDMAState,		// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// readStreamFormat
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::readStreamFormat,		// func
		kIOUCScalarIStructO,										// flags
		1,															// count of input parameters
		sizeof ( IOAudioStreamFormat )								// size of output structure
	},
	// readPowerState
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::readPowerState,	// func
		kIOUCScalarIScalarO,										// flags
		1,															// count of input parameters
		1															// count of output parameters
	},
	// hwRegisterWrite32
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::setPowerState,	// func
		kIOUCScalarIScalarO,										// flags
		2,															// count of input parameters
		0															// count of output parameters
	},
	// ksetBiquadCoefficients
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::setBiquadCoefficients,	// func
		kIOUCScalarIStructI,										// flags
		1,															// count of input parameters
		kMaxBiquadWidth												// size of input structure
	},
	//	getBiquadInfo
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::getBiquadInfo,		// func
		kIOUCScalarIStructO,										// flags
		1,															// count of input parameters
		kMaxBiquadInfoSize											// size of output structure
	},
	//	getProcessingParams
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::getProcessingParams,		// func
		kIOUCScalarIStructO,										// flags
		1,															// count of input parameters
		kMaxProcessingParamSize										// size of output structure
	},
	// setProcessingParams
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::setProcessingParams,		// func
		kIOUCScalarIStructI,										// flags
		1,															// count of input parameters
		kMaxProcessingParamSize										// size of input structure
	},
	// invokeInternalFunction
	{
		NULL,														// object
		( IOMethod ) &AppleLegacyOnboardAudioUserClient::invokeInternalFunction,		// func
		kIOUCScalarIStructI,										// flags
		1,															// count of input parameters
		16															// size of input structure
	},
};

const IOItemCount		AppleLegacyOnboardAudioUserClient::sMethodCount = sizeof( AppleLegacyOnboardAudioUserClient::sMethods ) / 
																  sizeof( AppleLegacyOnboardAudioUserClient::sMethods[ 0 ] );

OSDefineMetaClassAndStructors( AppleLegacyOnboardAudioUserClient, IOUserClient )

//===========================================================================================================================
//	Create
//===========================================================================================================================

AppleLegacyOnboardAudioUserClient *	AppleLegacyOnboardAudioUserClient::Create( Apple02Audio *inDriver, task_t inTask )
{
    AppleLegacyOnboardAudioUserClient *		userClient;
    
    userClient = new AppleLegacyOnboardAudioUserClient;
	if( !userClient )
	{
		debugIOLog (3,  "[Apple02Audio] create user client object failed" );
		goto exit;
	}
    
    if( !userClient->initWithDriver( inDriver, inTask ) )
	{
		debugIOLog (3,  "[Apple02Audio] initWithDriver failed" );
		
		userClient->release();
		userClient = NULL;
		goto exit;
	}
	
	debugIOLog (3,  "[Apple02Audio] User client created for task 0x%08lX", ( UInt32 ) inTask );
	
exit:
	return( userClient );
}

//===========================================================================================================================
//	initWithDriver
//===========================================================================================================================

bool	AppleLegacyOnboardAudioUserClient::initWithDriver( Apple02Audio *inDriver, task_t inTask )
{
	bool		result;
	
	debugIOLog (3,  "[Apple02Audio] initWithDriver" );
	
	result = false;
    if( !initWithTask( inTask, NULL, 0 ) )
	{
		debugIOLog (3,  "[Apple02Audio] initWithTask failed" );
		goto exit;
    }
    if( !inDriver )
	{
		debugIOLog (3,  "[Apple02Audio] initWithDriver failed (null input driver)" );
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

void	AppleLegacyOnboardAudioUserClient::free( void )
{
	debugIOLog (3,  "[Apple02Audio] free" );
	
    IOUserClient::free();
}

//===========================================================================================================================
//	clientClose
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::clientClose( void )
{
	debugIOLog (3,  "[Apple02Audio] clientClose" );
	
    if( !isInactive() )
	{
        mDriver = NULL;
    }
    return( kIOReturnSuccess );
}

//===========================================================================================================================
//	clientDied
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::clientDied( void )
{
	debugIOLog (3,  "[Apple02Audio] clientDied" );
	
    return( clientClose() );
}

//===========================================================================================================================
//	getTargetAndMethodForIndex
//===========================================================================================================================

IOExternalMethod *	AppleLegacyOnboardAudioUserClient::getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex )
{
	IOExternalMethod *		methodPtr;
	
	methodPtr = NULL;
	if( inIndex <= sMethodCount )  {
        *outTarget = this;
		methodPtr = ( IOExternalMethod * ) &sMethods[ inIndex ];
    } else {
		debugIOLog (3,  "[Apple02Audio] getTargetAndMethodForIndex - bad index (index=%lu)", inIndex );
	}
	return( methodPtr );
}

//===========================================================================================================================
//	gpioRead
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::gpioRead (UInt32 selector, UInt8 * gpioState) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != gpioState ) {
		*gpioState = mDriver->readGPIO ( selector );
		err = kIOReturnSuccess;
	}
	return (err);
}

//===========================================================================================================================
//	gpioWrite
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::gpioWrite (UInt32 selector, UInt8 data) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver ) {
		mDriver->writeGPIO ( selector, data );
		err = kIOReturnSuccess;
	}
	return (err);
}

//===========================================================================================================================
//	gpioGetActiveState
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::gpioGetActiveState (UInt32 selector, UInt8 * gpioActiveState) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != gpioActiveState ) {
		*gpioActiveState = mDriver->getGPIOActiveState ( selector );
		err = kIOReturnSuccess;
	}
	return (err);
}

//===========================================================================================================================
//	gpioSetActiveState
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::gpioSetActiveState (UInt32 selector, UInt8 gpioActiveState) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver ) {
		mDriver->setGPIOActiveState ( selector, gpioActiveState );
		err = kIOReturnSuccess;
	}
	return (err);
}

//===========================================================================================================================
//	checkIfGPIOExists
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::gpioCheckAvailable ( UInt32 selector, UInt8 * gpioExists ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver || NULL != gpioExists ) {
		*gpioExists = mDriver->checkGpioAvailable ( selector );
		err = kIOReturnSuccess;
	}
	return (err);
}

//===========================================================================================================================
//	hwRegisterRead32
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::hwRegisterRead32 ( UInt32 selector, UInt32 * data ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != data ) {
		err = mDriver->readHWReg32 ( selector, data );
	}
	return (err);
}

//===========================================================================================================================
//	hwRegisterWrite32
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::hwRegisterWrite32 ( UInt32 selector, UInt32 data ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver ) {
		err = mDriver->writeHWReg32 ( selector, data );
	}
	return (err);
}

//===========================================================================================================================
//	codecReadRegister
//
//	This transaction copies the entire codec register cache (up to 512 bytes) in a single transaction in order to 
//	limit the number of user client transactions.  If a register cache larger than 512 bytes exists then the
//	scalarArg1 is used to provide a target 512 byte block address.  For register cache sizes of 512 bytes or less,
//	scalarArg1 will have a value of zero to indicate the base block address.
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::codecReadRegister ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) {
	kern_return_t	err = kIOReturnNotReadable;
	
	if ( NULL != mDriver && NULL != outStructPtr  && NULL != outStructSizePtr ) {
		err = mDriver->readCodecReg ( scalarArg1, outStructPtr, outStructSizePtr );
	}
	return ( err );
}

//===========================================================================================================================
//	codecWriteRegister
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::codecWriteRegister ( UInt32 selector, void * data, UInt32 inStructSize ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != data ) {
		err = mDriver->writeCodecReg ( selector, data );
	}
	return (err);
}

//===========================================================================================================================
//	readSpeakerID
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::readSpeakerID ( UInt32 selector, UInt32 * data ) {
	IOReturn		err = kIOReturnNotReadable;

	if ( NULL != mDriver && NULL != data ) {
		err = mDriver->readSpkrID ( selector, data );
	}
	return (err);
}


//===========================================================================================================================
//	codecRegisterSize
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::codecRegisterSize ( UInt32 selector, UInt32 * codecRegSizePtr ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != codecRegSizePtr ) {
		err = mDriver->getCodecRegSize ( selector, codecRegSizePtr );
	}
	return (err);
}

//===========================================================================================================================
//	readPRAMVolume
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::readPRAMVolume ( UInt32 selector, UInt32 * pramDataPtr ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != pramDataPtr ) {
		err = (UInt32)mDriver->getVolumePRAM ( pramDataPtr );
	}
	return (err);
}


//===========================================================================================================================
//	readDMAState
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::readDMAState ( UInt32 selector, UInt32 * dmaStatePtr ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != dmaStatePtr ) {
		err = (UInt32)mDriver->getDmaState ( dmaStatePtr );
	}
	return err;
}


//===========================================================================================================================
//	readStreamFormat
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::readStreamFormat ( UInt32 selector, IOAudioStreamFormat * outStructPtr, IOByteCount * outStructSizePtr ) {
#pragma unused ( selector )
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != outStructPtr && NULL != outStructSizePtr ) {
		if ( sizeof ( IOAudioStreamFormat ) <= *outStructSizePtr ) {
			err = (UInt32)mDriver->getStreamFormat ( outStructPtr );
			if ( kIOReturnSuccess == err ) {
				*outStructSizePtr = sizeof ( IOAudioStreamFormat );
			}
		}
	}
	return err;
}


//===========================================================================================================================
//	readPowerState
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != powerState ) {
		err = mDriver->readPowerState ( selector, powerState );
	}
	return (err);
}

//===========================================================================================================================
//	setPowerState
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver ) {
		err = mDriver->setPowerState ( selector, powerState );
	}
	return (err);
}

//===========================================================================================================================
//	setBiquadCoefficients
//
//	NOTE:	selector is used to pass a streamID.  Texas & Texas2 only have a single output stream and require that
//			the selector be passed with a value of zero.
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize ) {
	IOReturn		err = kIOReturnNotReadable;
	
	if ( NULL != mDriver && NULL != biquadCoefficients && kMaxBiquadWidth >= coefficientSize ) {
		err = mDriver->setBiquadCoefficients ( selector, biquadCoefficients, coefficientSize );
	}
	return (err);
}


//===========================================================================================================================
//	getBiquadInfo
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::getBiquadInfo ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != outStructPtr && NULL != outStructSizePtr ) {
		err = mDriver->getBiquadInformation ( scalarArg1, outStructPtr, outStructSizePtr );
	}
	return (err);
}


//===========================================================================================================================
//	getProcessingParams
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::getProcessingParams ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != outStructPtr && NULL != outStructSizePtr ) {
		if (  kMaxProcessingParamSize >= *outStructSizePtr ) {
			err = mDriver->getProcessingParameters ( scalarArg1, outStructPtr, outStructSizePtr );
		}
	}
	return (err);
}


//===========================================================================================================================
//	setProcessingParams
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::setProcessingParams ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver && NULL != inStructPtr && kMaxProcessingParamSize >= inStructSize ) {
		err = mDriver->setProcessingParameters ( scalarArg1, inStructPtr, inStructSize );
	}
	return (err);
}


//===========================================================================================================================
//	invokeInternalFunction
//===========================================================================================================================

IOReturn	AppleLegacyOnboardAudioUserClient::invokeInternalFunction ( UInt32 functionSelector, void * inData, UInt32 inDataSize ) {
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != mDriver ) {
		err = mDriver->invokeInternalFunction ( functionSelector, inData );
	}
	return (err);
}



#if 0
/*
		The following code goes into whatever application wants to call into Apple02Audio
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

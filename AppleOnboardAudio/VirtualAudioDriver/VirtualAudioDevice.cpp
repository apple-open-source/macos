/*
  File:VirtualAudioDevice.cpp

  Contains:

  Version:1.0.0

  Copyright:Copyright © 1997-2002 by Apple Computer, Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Computer, Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "VirtualAudioDevice.h"
#include "AudioDebug.h"
#include "VirtualAudioEngine.h"
#include <IOKit/IOKitKeys.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/IOLib.h>

#define super IOAudioDevice

OSDefineMetaClassAndStructors(VirtualAudioDevice, IOAudioDevice)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioDevice::initHardware(IOService *provider)
{
    bool result = false;
    
    debug3IOLog("VirtualAudioDevice[%p]::initHardware(%p)\n", this, provider);
    
    if (!super::initHardware(provider)) {
        goto Done;
    }
    
    // add the hardware init code here
    
    setDeviceName("Virtual Audio Device");
    setDeviceShortName("Virtual");
    setManufacturerName("Apple Computer, Inc.");
	setDeviceTransportType (kIOAudioDeviceTransportTypeOther);

    if (!createAudioEngines()) {
        goto Done;
    }
    
    result = true;
    
Done:

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioDevice::createAudioEngines()
{
    bool result = false;
    OSArray *audioEngineArray;
    
    debug2IOLog("VirtualAudioDevice[%p]::createAudioEngine()\n", this);
    
    audioEngineArray = OSDynamicCast(OSArray, getProperty(AUDIO_ENGINES_KEY));
    
    if (audioEngineArray) {
        OSCollectionIterator *audioEngineIterator;
        
        audioEngineIterator = OSCollectionIterator::withCollection(audioEngineArray);
        if (audioEngineIterator) {
            OSDictionary *audioEngineDict;
            
            while (audioEngineDict = (OSDictionary *)audioEngineIterator->getNextObject()) {
                if (OSDynamicCast(OSDictionary, audioEngineDict) != NULL) {
                    VirtualAudioEngine *audioEngine;
                    
                    audioEngine = new VirtualAudioEngine;
                    if (audioEngine) {
                        if (audioEngine->init(audioEngineDict)) {
                            activateAudioEngine(audioEngine);
                        }
                        audioEngine->release();
                    }
                }
            }
            
            audioEngineIterator->release();
        }
    } else {
        debug2IOLog("VirtualAudioDevice[%p]::createAudioEngine() - Error: no AudioEngine array in personality.\n", this);
        goto Done;
    }
    
    result = true;
    
Done:

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::volumeChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    VirtualAudioDevice *audioDevice;
    
    audioDevice = (VirtualAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->volumeChanged(volumeControl, oldValue, newValue);
    }
    
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::volumeChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    debug5IOLog("VirtualAudioDevice[%p]::volumeChanged(%p, %ld, %ld)\n", this, volumeControl, oldValue, newValue);
    
    if (volumeControl) {
        debug2IOLog("\t-> Channel %ld\n", volumeControl->getChannelID());
    }
    
    // Add hardware volume code change 

    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    VirtualAudioDevice *audioDevice;
    
    audioDevice = (VirtualAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    debug5IOLog("VirtualAudioDevice[%p]::outputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    if (muteControl) {
        debug2IOLog("\t-> Channel %ld\n", muteControl->getChannelID());
    }
    
    // Add output mute code here
    
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::gainChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    VirtualAudioDevice *audioDevice;
    
    audioDevice = (VirtualAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->gainChanged(gainControl, oldValue, newValue);
    }
    
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::gainChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    debug5IOLog("VirtualAudioDevice[%p]::gainChanged(%p, %ld, %ld)\n", this, gainControl, oldValue, newValue);
    
    if (gainControl) {
        debug2IOLog("\t-> Channel %ld\n", gainControl->getChannelID());
    }
    
    // Add hardware gain change code here 

    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::inputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    VirtualAudioDevice *audioDevice;
    
    audioDevice = (VirtualAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->inputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::inputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    debug5IOLog("VirtualAudioDevice[%p]::inputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    if (muteControl) {
        debug2IOLog("\t-> Channel %ld\n", muteControl->getChannelID());
    }
    
    // Add input mute change code here
    
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    VirtualAudioDevice *audioDevice;
    
    audioDevice = (VirtualAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->passThruChanged(passThruControl, oldValue, newValue);
    }
    
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn VirtualAudioDevice::passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue)
{
    debug5IOLog("VirtualAudioDevice[%p]::passThruChanged(%p, %ld, %ld)\n", this, passThruControl, oldValue, newValue);
    
    if (passThruControl) {
        debug2IOLog("\t-> Channel %ld\n", passThruControl->getChannelID());
    }
        
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VirtualAudioDevice::FindSoundNode (void) 
{
	const IORegistryPlane *	dtPlane;
	IORegistryEntry *			regEntry;
	IORegistryIterator *		iterator;
	Boolean					found;
	Boolean					done;
	const char *				name;

	found = FALSE;

	dtPlane = IORegistryEntry::getPlane (kIODeviceTreePlane);
	if (NULL == dtPlane)
		goto Exit;

	iterator = IORegistryIterator::iterateOver (dtPlane, kIORegistryIterateRecursively);
	if (NULL == iterator)
		goto Exit;

	done = FALSE;
	regEntry = iterator->getNextObject ();
	while (NULL != regEntry && FALSE == done) {
		name = regEntry->getName ();
		if (0 == strcmp (name, "mac-io")) {
			// This is where we want to start the search
			iterator->release ();		// release the current iterator and make a new one rooted at "mac-io"
			done = TRUE;
			break;
		}
		regEntry = iterator->getNextObject ();
	}

	// Now the real search begins...
	iterator = IORegistryIterator::iterateOver (regEntry, dtPlane, kIORegistryIterateRecursively);
	if (NULL == iterator)
		goto Exit;

	regEntry = iterator->getNextObject ();
	while (NULL != regEntry && FALSE == found) {
		name = regEntry->getName ();
		if (0 == strcmp (name, "sound")) {
			found = TRUE;
			break;
		}
		regEntry = iterator->getNextObject ();
	}

	iterator->release ();

Exit:
	return found;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOService* VirtualAudioDevice::probe(IOService* provider, SInt32* score)
{
	IOService* result;

	debugIOLog("VirtualAudioDevice:: probe\n");
	if (!FindSoundNode())
	{
		*score = 0x4000;
		result = this;
		debugIOLog("VirtualAudioDevice::probe should load\n");
	}
	else
	{
		debugIOLog("VirtualAudioDevice::probe should not load\n");
		*score = 0;
		result = NULL;
	}
	
	return this;
}

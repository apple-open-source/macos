/*
  File:PhantomAudioDevice.cpp

  Contains:

  Version:1.0.0

  Copyright:Copyright ) 1997-2000 by Apple Computer, Inc., All Rights Reserved.

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

#include "PhantomAudioDevice.h"

#include "PhantomAudioEngine.h"

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/IOLib.h>

#define super IOAudioDevice

OSDefineMetaClassAndStructors(PhantomAudioDevice, IOAudioDevice)

/*
 * initHardware()
 */

bool PhantomAudioDevice::initHardware(IOService *provider)
{
    bool result = false;
    
    IOLog("PhantomAudioDevice[%p]::initHardware(%p)\n", this, provider);
    
    if (!super::initHardware(provider)) {
        goto Done;
    }
    
    // add the hardware init code here
    
	// Pass pointers to the strings so that they can be localized.
    setDeviceName("DeviceName");
    setDeviceShortName("DeviceShortName");
    setManufacturerName("ManufacturerName");
	// Setting this flag causes the HAL to use the strings passed to it as keys into the Localizable.strings file.
	// This is true for control names as well as these string names.  Very useful for selector controls (not shown in this example).
    setProperty (kIOAudioDeviceLocalizedBundleKey, "PhantomAudioDriver.kext");
    
    if (!createAudioEngines()) {
        goto Done;
    }
    
    result = true;
    
Done:

    return result;
}

/*
 * createAudioEngines()
 */

bool PhantomAudioDevice::createAudioEngines()
{
    bool result = false;
    OSArray *audioEngineArray;
    
    IOLog("PhantomAudioDevice[%p]::createAudioEngine()\n", this);
    
    audioEngineArray = OSDynamicCast(OSArray, getProperty(AUDIO_ENGINES_KEY));
    
    if (audioEngineArray) {
        OSCollectionIterator *audioEngineIterator;
        
        audioEngineIterator = OSCollectionIterator::withCollection(audioEngineArray);
        if (audioEngineIterator) {
            OSDictionary *audioEngineDict;
            
            while (audioEngineDict = (OSDictionary *)audioEngineIterator->getNextObject()) {
                if (OSDynamicCast(OSDictionary, audioEngineDict) != NULL) {
                    PhantomAudioEngine *audioEngine;
                    
                    audioEngine = new PhantomAudioEngine;
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
        IOLog("PhantomAudioDevice[%p]::createAudioEngine() - Error: no AudioEngine array in personality.\n", this);
        goto Done;
    }
    
    result = true;
    
Done:

    return result;
}

/*
 * volumeChangeHandler()
 */
 
IOReturn PhantomAudioDevice::volumeChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    PhantomAudioDevice *audioDevice;
    
    audioDevice = (PhantomAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->volumeChanged(volumeControl, oldValue, newValue);
    }
    
    return result;
}

/*
 * volumeChanged()
 */
 
IOReturn PhantomAudioDevice::volumeChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("PhantomAudioDevice[%p]::volumeChanged(%p, %ld, %ld)\n", this, volumeControl, oldValue, newValue);
    
    if (volumeControl) {
        IOLog("\t-> Channel %ld\n", volumeControl->getChannelID());
    }
    
    // Add hardware volume code change 

    return kIOReturnSuccess;
}

/*
 * outputMuteChangeHandler()
 */
 
IOReturn PhantomAudioDevice::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    PhantomAudioDevice *audioDevice;
    
    audioDevice = (PhantomAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

/*
 * outputMuteChanged()
 */
 
IOReturn PhantomAudioDevice::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("PhantomAudioDevice[%p]::outputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    if (muteControl) {
        IOLog("\t-> Channel %ld\n", muteControl->getChannelID());
    }
    
    // Add output mute code here
    
    return kIOReturnSuccess;
}

/*
 * gainChangeHandler()
 */
 
IOReturn PhantomAudioDevice::gainChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    PhantomAudioDevice *audioDevice;
    
    audioDevice = (PhantomAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->gainChanged(gainControl, oldValue, newValue);
    }
    
    return result;
}

/*
 * gainChanged()
 */
 
IOReturn PhantomAudioDevice::gainChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("PhantomAudioDevice[%p]::gainChanged(%p, %ld, %ld)\n", this, gainControl, oldValue, newValue);
    
    if (gainControl) {
        IOLog("\t-> Channel %ld\n", gainControl->getChannelID());
    }
    
    // Add hardware gain change code here 

    return kIOReturnSuccess;
}

/*
 * inputMuteChangeHandler()
 */
 
IOReturn PhantomAudioDevice::inputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    PhantomAudioDevice *audioDevice;
    
    audioDevice = (PhantomAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->inputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

/*
 * inputMuteChanged()
 */
 
IOReturn PhantomAudioDevice::inputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("PhantomAudioDevice[%p]::inputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    if (muteControl) {
        IOLog("\t-> Channel %ld\n", muteControl->getChannelID());
    }
    
    // Add input mute change code here
    
    return kIOReturnSuccess;
}

/*
 * passThruChangeHandler()
 */
 
IOReturn PhantomAudioDevice::passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    PhantomAudioDevice *audioDevice;
    
    audioDevice = (PhantomAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->passThruChanged(passThruControl, oldValue, newValue);
    }
    
    return result;
}

/*
 * passThruChanged()
 */
 
IOReturn PhantomAudioDevice::passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("PhantomAudioDevice[%p]::passThruChanged(%p, %ld, %ld)\n", this, passThruControl, oldValue, newValue);
    
    if (passThruControl) {
        IOLog("\t-> Channel %ld\n", passThruControl->getChannelID());
    }
        
    return kIOReturnSuccess;
}

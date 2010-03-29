/*
  File:SamplePCIAudioDevice.cpp

  Contains:

  Version:1.0.0

  Copyright:Copyright ) 1997-2010 by Apple Computer, Inc., All Rights Reserved.

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

#include "SamplePCIAudioDevice.h"

#include "SamplePCIAudioEngine.h"

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>

#include <IOKit/pci/IOPCIDevice.h>

#define super IOAudioDevice

OSDefineMetaClassAndStructors(SamplePCIAudioDevice, IOAudioDevice)

bool SamplePCIAudioDevice::initHardware(IOService *provider)
{
    bool result = false;
    
    IOLog("SamplePCIAudioDevice[%p]::initHardware(%p)\n", this, provider);
    
    if (!super::initHardware(provider)) {
        goto Done;
    }
    
    // Get the PCI device provider
    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) {
        goto Done;
    }
    
    // Config a map for the PCI config base registers
    // We need to keep this map around until we're done accessing the registers
    deviceMap = pciDevice->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!deviceMap) {
        goto Done;
    }
    
    // Get the virtual address for the registers - mapped in the kernel address space
    deviceRegisters = (SamplePCIAudioDeviceRegisters *)deviceMap->getVirtualAddress();
    if (!deviceRegisters) {
        goto Done;
    }
    
    // Enable the PCI memory access - the kernel will panic if this isn't done before accessing the 
    // mapped registers
    pciDevice->setMemoryEnable(true);

    // add the hardware init code here
    
    setDeviceName("Sample PCI Audio Device");
    setDeviceShortName("PCIAudio");
    setManufacturerName("My Company");

#error Put your own hardware initialization code here...and in other routines!!

    if (!createAudioEngine()) {
        goto Done;
    }
    
    result = true;
    
Done:

    if (!result) {
        if (deviceMap) {
            deviceMap->release();
            deviceMap = NULL;
        }
    }

    return result;
}

void SamplePCIAudioDevice::free()
{
    IOLog("SamplePCIAudioDevice[%p]::free()\n", this);
    
    if (deviceMap) {
        deviceMap->release();
        deviceMap = NULL;
    }
    
    super::free();
}
    
bool SamplePCIAudioDevice::createAudioEngine()
{
    bool result = false;
    SamplePCIAudioEngine *audioEngine = NULL;
    IOAudioControl *control;
    
    IOLog("SamplePCIAudioDevice[%p]::createAudioEngine()\n", this);
    
    audioEngine = new SamplePCIAudioEngine;
    if (!audioEngine) {
        goto Done;
    }
    
    // Init the new audio engine with the device registers so it can access them if necessary
    // The audio engine subclass could be defined to take any number of parameters for its
    // initialization - use it like a constructor
    if (!audioEngine->init(deviceRegisters)) {
        goto Done;
    }
    
    // Create a left & right output volume control with an int range from 0 to 65535
    // and a db range from -22.5 to 0.0
    // Once each control is added to the audio engine, they should be released
    // so that when the audio engine is done with them, they get freed properly
    control = IOAudioLevelControl::createVolumeControl(65535,	// Initial value
                                                        0,		// min value
                                                        65535,	// max value
                                                        (-22 << 16) + (32768),	// -22.5 in IOFixed (16.16)
                                                        0,		// max 0.0 in IOFixed
                                                        kIOAudioControlChannelIDDefaultLeft,
                                                        kIOAudioControlChannelNameLeft,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageOutput);
    if (!control) {
        goto Done;
    }
    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();
    
    control = IOAudioLevelControl::createVolumeControl(65535,	// Initial value
                                                        0,		// min value
                                                        65535,	// max value
                                                        (-22 << 16) + (32768),	// min -22.5 in IOFixed (16.16)
                                                        0,		// max 0.0 in IOFixed
                                                        kIOAudioControlChannelIDDefaultRight,	// Affects right channel
                                                        kIOAudioControlChannelNameRight,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageOutput);
    if (!control) {
        goto Done;
    }
        
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();

    // Create an output mute control
    control = IOAudioToggleControl::createMuteControl(false,	// initial state - unmuted
                                                        kIOAudioControlChannelIDAll,	// Affects all channels
                                                        kIOAudioControlChannelNameAll,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageOutput);
                                
    if (!control) {
        goto Done;
    }
        
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputMuteChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();

    // Create a left & right input gain control with an int range from 0 to 65535
    // and a db range from 0 to 22.5
    control = IOAudioLevelControl::createVolumeControl(65535,	// Initial value
                                                        0,		// min value
                                                        65535,	// max value
                                                        0,		// min 0.0 in IOFixed
                                                        (22 << 16) + (32768),	// 22.5 in IOFixed (16.16)
                                                        kIOAudioControlChannelIDDefaultLeft,
                                                        kIOAudioControlChannelNameLeft,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageInput);
    if (!control) {
        goto Done;
    }
    
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();
    
    control = IOAudioLevelControl::createVolumeControl(65535,	// Initial value
                                                        0,		// min value
                                                        65535,	// max value
                                                        0,		// min 0.0 in IOFixed
                                                        (22 << 16) + (32768),	// max 22.5 in IOFixed (16.16)
                                                        kIOAudioControlChannelIDDefaultRight,	// Affects right channel
                                                        kIOAudioControlChannelNameRight,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageInput);
    if (!control) {
        goto Done;
    }
        
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();

    // Create an input mute control
    control = IOAudioToggleControl::createMuteControl(false,	// initial state - unmuted
                                                        kIOAudioControlChannelIDAll,	// Affects all channels
                                                        kIOAudioControlChannelNameAll,
                                                        0,		// control ID - driver-defined
                                                        kIOAudioControlUsageInput);
                                
    if (!control) {
        goto Done;
    }
        
    control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputMuteChangeHandler, this);
    audioEngine->addDefaultAudioControl(control);
    control->release();

    // Active the audio engine - this will cause the audio engine to have start() and initHardware() called on it
    // After this function returns, that audio engine should be ready to begin vending audio services to the system
    activateAudioEngine(audioEngine);
    // Once the audio engine has been activated, release it so that when the driver gets terminated,
    // it gets freed
    audioEngine->release();
    
    result = true;
    
Done:

    if (!result && (audioEngine != NULL)) {
        audioEngine->release();
    }

    return result;
}

IOReturn SamplePCIAudioDevice::volumeChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    SamplePCIAudioDevice *audioDevice;
    
    audioDevice = (SamplePCIAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->volumeChanged(volumeControl, oldValue, newValue);
    }
    
    return result;
}

IOReturn SamplePCIAudioDevice::volumeChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("SamplePCIAudioDevice[%p]::volumeChanged(%p, %ld, %ld)\n", this, volumeControl, oldValue, newValue);
    
    if (volumeControl) {
        IOLog("\t-> Channel %ld\n", volumeControl->getChannelID());
    }
    
    // Add hardware volume code change 

    return kIOReturnSuccess;
}
    
IOReturn SamplePCIAudioDevice::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    SamplePCIAudioDevice *audioDevice;
    
    audioDevice = (SamplePCIAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

IOReturn SamplePCIAudioDevice::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("SamplePCIAudioDevice[%p]::outputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    // Add output mute code here
    
    return kIOReturnSuccess;
}

IOReturn SamplePCIAudioDevice::gainChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    SamplePCIAudioDevice *audioDevice;
    
    audioDevice = (SamplePCIAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->gainChanged(gainControl, oldValue, newValue);
    }
    
    return result;
}

IOReturn SamplePCIAudioDevice::gainChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("SamplePCIAudioDevice[%p]::gainChanged(%p, %ld, %ld)\n", this, gainControl, oldValue, newValue);
    
    if (gainControl) {
        IOLog("\t-> Channel %ld\n", gainControl->getChannelID());
    }
    
    // Add hardware gain change code here 

    return kIOReturnSuccess;
}
    
IOReturn SamplePCIAudioDevice::inputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOReturn result = kIOReturnBadArgument;
    SamplePCIAudioDevice *audioDevice;
    
    audioDevice = (SamplePCIAudioDevice *)target;
    if (audioDevice) {
        result = audioDevice->inputMuteChanged(muteControl, oldValue, newValue);
    }
    
    return result;
}

IOReturn SamplePCIAudioDevice::inputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
    IOLog("SamplePCIAudioDevice[%p]::inputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
    
    // Add input mute change code here
    
    return kIOReturnSuccess;
}

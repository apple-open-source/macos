/*
  File:VirtualAudioDevice.h

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

#ifndef _SAMPLEAUDIODEVICE_H
#define _SAMPLEAUDIODEVICE_H

#include <IOKit/audio/IOAudioDevice.h>

#define AUDIO_ENGINES_KEY	"AudioEngines"
#define DESCRIPTION_KEY		"Description"
#define BLOCK_SIZE_KEY		"BlockSize"
#define NUM_BLOCKS_KEY		"NumBlocks"
#define NUM_STREAMS_KEY		"NumStreams"
#define FORMATS_KEY		"Formats"
#define SAMPLE_RATES_KEY	"SampleRates"

#define VirtualAudioDevice com_apple_driver_VirtualAudioDevice

class VirtualAudioDevice : public IOAudioDevice
{
	friend class com_apple_driver_VirtualAudioEngine;
	
	OSDeclareDefaultStructors(VirtualAudioDevice)
	virtual IOService* probe(	IOService* provider, SInt32* score );
	virtual bool initHardware(IOService *provider);
	virtual bool createAudioEngines();
	
	static IOReturn volumeChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
	virtual IOReturn volumeChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
	
	static IOReturn outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
	virtual IOReturn outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
	
	static IOReturn gainChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
	virtual IOReturn gainChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
	
	static IOReturn inputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
	virtual IOReturn inputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
	
	static IOReturn passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue);
	virtual IOReturn passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue);
	virtual bool FindSoundNode (void);

};

#endif /* _SAMPLEAUDIODEVICE_H */
/*
 *  AppleHardwareInput.h
 *  Apple02Audio
 *
 *  Created by lcerveau on Wed Jan 03 2001.
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 */

#ifndef _AUDIOHARDWAREINPUT_H
#define _AUDIOHARDWAREINPUT_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareMux.h"
// #include "Apple02Audio.h"  // we should replace with AppleAudioHardware generic class
class AudioHardwareMux;

typedef struct _s_AudioHardwareInputInfo {
    UInt32	sndHWPort; 		
    UInt32	inputPortType;
    UInt32	channels;
    bool 	isOnMuX;
    AudioHardwareMux *theMuxRef;  
      
}AudioHardwareInputInfo;

enum{
    kAudioHardwareInputUnknown = 0,
    kAudioHardwareInputNone = 1,
    kAudioHardwareInputIntMic = 2,
    kAudioHardwareInputExtMic = 3
};

    // 4 char code equivalent of Sound.h for the source input
enum {
    kNoSource                     = 'none',	// no source selection
    kCDSource                     = 'cd  ',	// internal CD player input
    kExtMicSource                 = 'emic',	// external mic input
    kSoundInSource                = 'sinj',	// sound input jack
    kRCAInSource                  = 'irca',	// RCA jack input
    kTVFMTunerSource              = 'tvfm',
    kDAVInSource                  = 'idav',	// DAV analog input
    kIntMicSource                 = 'imic',	// internal mic input
    kMediaBaySource               = 'mbay',	// media bay input
    kModemSource                  = 'modm',	// modem input (internal modem on desktops, PCI input on PowerBooks)
    kPCCardSource                 = 'pcm ',	// PC Card pwm input
    kZoomVideoSource              = 'zvpc',	// zoom video input
    kDVDSource                    = 'dvda',	// DVD audio input
    kMicrophoneArray              = 'mica'	// microphone array
};


class AudioHardwareInput : public IOAudioPort {
    OSDeclareDefaultStructors(AudioHardwareInput);

public:
    static AudioHardwareInput *create(AudioHardwareInputInfo theInputInfo);
    bool deviceSetActive( UInt32 currentDevices );
    void forceActivation(UInt32 selector);
    void attachAudioPluginRef(Apple02Audio *theAudioPlugin);
    UInt32 getInputPortType(void);
	void setInputGain(UInt32 leftGain, UInt32 rightGain);
    
protected:
    bool init(AudioHardwareInputInfo theInputInfo);
    void free();

    void ioLog();
    
    UInt32	sndHWPort; 				// which port the device is connected to
    UInt32	inputPortType;			// type of input port
    UInt32	channels;				// channels affected
    bool 	isOnMuX;				// set if the input is on a Mux
    bool	active;					// set if the input is active
    AudioHardwareMux *theMuxRef;  	//the Mux if there isone

    Apple02Audio *pluginRef;
    UInt32 	gainLeft;
    UInt32 	gainRight;
};

#endif

/*
 *  AudioHardwareDetect.h (definition)
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 * 
 *  The contents of this file constitute Original Code as defined in and
 *  are subject to the Apple Public Source License Version 1.1 (the
 *  "License").  You may not use this file except in compliance with the
 *  License.  Please obtain a copy of the License at
 *  http://www.apple.com/publicsource and read it before using this file.
 * 
 *  This Original Code and all software distributed under the License are
 *  distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 *  An AudioHardwareDetect is a method to see if a device has been
 *  connected to the main CPU. It can be based on a different methods
 *     - A status is polled on a chip (usually the Codec itself) and is tested
 *       against the detect properties. There are two kind of subdetects :
 *           + the InSense : when a device is connected, a bit is set in 
 *             the status. The status is masked with the detect bit-mask, and
 *             compared with the bit-match. If the comparison is stisfied
 *             the detect is activated 
 *	     + the AnyInSense : the presence of one or more bits, tells that
 *	       the detect is activated
 *     -  An interrupt is detected, and depending on its value triggers or not
 *        the detect
 *   
 *   An AudioHardwareDetect is a subclass of the IOAudioJackControl. For now
 *   it barely use the advantage of this class hierarchy. Eventually in case of
 *   activation we will just change the state of the IOAudioJackControl to notify 
 *   the device driver.
 */

#ifndef _AUDIOHARDWAREDETECT_H
#define _AUDIOHARDWAREDETECT_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"

    // Information passed to the "create" method. Ususally this information
    // is extracted by parsing the Open Firmware "sound-objects" properties
    // of the "sound" node
    
typedef struct _s_AudioHardwareDetectInfo {
    short detectKind;
    UInt32 bitMask;
    UInt32 bitMatch;
    UInt32 device;    
}AudioHardwareDetectInfo;

    // Different kind of detects. We could have created a parent class and subclass
    // it for each kind, but as there are not a lot of detect kind.
enum{
    kAudioHardwareDetectUnknown = 0,
    kAudioHardwareDetectInSense = 1,
    kAudioHardwareDetectAnyInSense = 2,
    kAudioHardwareDetectGPIO = 3,
    kAudioHardwareGenericDetectGPIO = 4
};


class IOAudioJackControl;


    //Class declaration
    
class AudioHardwareDetect : public OSObject {

    OSDeclareDefaultStructors(AudioHardwareDetect);
    
public:
    static AudioHardwareDetect *create(UInt32 cntrlID, AudioHardwareDetectInfo myInfo);
    
    void   ioLog();   				  // Useful in debug mode to display the Detect info
    UInt32 refreshDevices(UInt32 currentDevices); // The method to see if the detect should activate or not 
                                                  // a change in the sndHWSpec field of the driver.

protected:

    bool init(UInt32 cntrlID, AudioHardwareDetectInfo myInfo);
    void free();
    
    short  dKind;				// detect kind
    UInt32 dDevice;				// device attached. These are defined in AudioHardwareConstants.h
    UInt32 dbitMask;				// field for status polled based detect
    UInt32 dbitMatch;				// field for status polled based detect

};

#endif

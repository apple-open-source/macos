/*
 *  AudioHardwareDetect.cpp (implementation)
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  An AudioHardwareDetect is a method to see if a device has been
 *  connected to the main CPU. It can be based on a different methods
 *     - A status is polled on a chip (usually the Codec itself) and is tested
 *       agains the detect properties. There are two kind of subdetects :
 *           + the InSense : when a device is connected, a bit is set in 
 *             the status. The status is masked with the detect bit-mask, and
 *             compared with the bit-match. If the comparison is stisfied
 *             the detect is activated 
 *	     + the AnyInSense : the presence of one or more bits, tells that
 *	       the detect is activated
 *     -  An interrupt is detected, and depending on its value triggers or not
 *        the detect
 *
 *  As of this version of the Apple02Audio project (01/2001), and for the Screamer
 *  chip, the test for detect activation is made by creating a timer in the Codec driver
 *  and polling regularly. However we should change this method
 *    + make it work on the IOWorkloop of the driver
 *    + make it as asynchronous as possible
 *
 *  Usually the driver has a field of type sndHWSpec (or UInt32) that contains all devices
 *  detected. This sndHWSpec field is then passed to the AudioHardwareOutput, 
 *  and AudioHardwareINput objects in order to have the audio ports answering to 
 *  a change in the connected device. Eventually, this communication will occur through
 *  a notification system.
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareDetect.h"

#define super OSObject

OSDefineMetaClassAndStructors(AudioHardwareDetect, OSObject)

// Class creation function. Can be called from anywhere

AudioHardwareDetect *AudioHardwareDetect::create(UInt32 cntrlID, AudioHardwareDetectInfo myInfo){
    AudioHardwareDetect *myDetect;

	// we verify it is a known type of AudioHardwareDetect
    if (kAudioHardwareDetectUnknown == myInfo.detectKind) {
        myDetect = 0; 
        goto EXIT;
    }
    
    myDetect = new AudioHardwareDetect;
    
    if(myDetect) {
        if(!(myDetect->init(cntrlID, myInfo))){
            myDetect->release();
            myDetect = 0;
        }            
    }
EXIT:
    return myDetect;
}

// Private creation method
    
bool AudioHardwareDetect::init(UInt32 cntrlID, AudioHardwareDetectInfo myInfo) {
    if(!super::init())
        return false;
    
    dKind = myInfo.detectKind;
    dDevice = myInfo.device;
    dbitMask = myInfo.bitMask;
    if(dKind == kAudioHardwareDetectInSense)
        dbitMatch = myInfo.bitMatch;
    else
        dbitMatch = 0;
    
    return(true);
}

// Release method (maybe we should implement a release??)
void AudioHardwareDetect::free(){
    super::free();
}

// This is the most useful method : given a series of bits
// representing the state of all AudioHardwareDetect, this method
// verifies if its device is connected. If yes it returns
// it, and otherwise 0.
UInt32 AudioHardwareDetect::refreshDevices(UInt32 inSense){
    UInt32 result;
    
    switch(dKind) {
          
        case kAudioHardwareDetectInSense:
            if((inSense & dbitMask) == dbitMatch) 
                result = dDevice;
            else 
                result = 0;
            break;
        case kAudioHardwareDetectAnyInSense:
            if((inSense & dbitMask)  != 0)
                result = dDevice;
            else 
                result = 0;
            break;
        case kAudioHardwareDetectGPIO: 					// We need to add the GPIO detect
			debugIOLog (3, "AudioHardwareDetect::refreshDevices inSense %08lx, dbitMask %08lx, dbitMatch %08lx", inSense, dbitMask, dbitMatch) ;

            if((inSense & dbitMask) == dbitMatch) {
                result = dDevice;
            } else {
                result = 0;
			}
            break;
        case kAudioHardwareDetectUnknown:
        default:
            result = 0;
            break;
    }
    ioLog();
    if(0 != result){
        debugIOLog (3, " --> Detect Activated");
    }
    return(result);
}

void AudioHardwareDetect::ioLog(){
#ifdef DEBUGLOG
     debugIOLog (3,  "+ Detect for device : ");
     switch (dDevice) {
        case kSndHWInternalSpeaker: debugIOLog (3, "kSndHWInternalSpeaker"); break;
        case kSndHWCPUHeadphone: debugIOLog (3, "kSndHWCPUHeadphone"); break;
        case kSndHWCPUExternalSpeaker: debugIOLog (3, "kSndHWCPUExternalSpeaker");break;
        case kSndHWCPUSubwoofer: debugIOLog (3, "kSndHWCPUSubwoofer"); break;
        case kSndHWCPUMicrophone: debugIOLog (3, "kSndHWCPUMicrophone"); break;        
        case kSndHWCPUPlainTalk: debugIOLog (3, "kSndHWCPUPlainTalk"); break;       
        case kSndHWMonitorHeadphone: debugIOLog (3, "kSndHWMonitorHeadphone"); break;
        case kSndHWMonitorPlainTalk: debugIOLog (3, "kSndHWMonitorPlainTalk"); break;
        case kSndHWModemRingDetect: debugIOLog (3, "kSndHWModemRingDetect"); break;
        case kSndHWModemLineCurrent: debugIOLog (3, "kSndHWModemLineCurrent"); break;
        case kSndHWModemESquared: debugIOLog (3, "kSndHWModemESquared"); break;
        default: break;
    }
    
    switch(dKind) {
        case kAudioHardwareDetectInSense:
            debugIOLog (3, " -- Type : InSense");
            debugIOLog (3, " -- Insense mask is %ld, insense match is %ld",dbitMask, dbitMatch );
            break;
        case kAudioHardwareDetectAnyInSense:
            debugIOLog (3, " -- Type : AnyInSense");
            debugIOLog (3, " -- Insense mask is %ld ",dbitMask);
            break;
        case kAudioHardwareDetectGPIO: 				// We need to add the GPIO detect information
            debugIOLog (3, " -- Type : GPIO");
            break;
        case kAudioHardwareDetectUnknown:
        default:
            debugIOLog (3, " -- Type : Unknown");
            break;
    }
#endif
}

/*
 *  AOAHALPlugin.c
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Mon May 28 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AOAHALPlugin.h"
#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>

io_registry_entry_t gControlReference;
io_registry_entry_t mHeadphoneExclusiveControl;
io_connect_t	    gConnectionReference;
io_object_t			ioAudioEngine;
io_object_t			ioAudioDevice;

static void convertDecTo4cc (UInt32 input, char * output);

OSStatus AudioDriverPlugInOpen (AudioDriverPlugInHostInfo * inHostInfo) {
	OSStatus					theResult;
	CFNumberRef					theValue;
	UInt32						theIntVal;
	io_iterator_t				theIterator;
	kern_return_t				theKernelError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInOpen \n");
	theResult = noErr;
	ioAudioEngine = inHostInfo->mIOAudioEngine;
	ioAudioDevice = inHostInfo->mIOAudioDevice;

	theKernelError = IORegistryEntryCreateIterator (inHostInfo->mIOAudioDevice, kIOServicePlane, kIORegistryIterateRecursively, &theIterator);

	// Get the good control entry.
	if (KERN_SUCCESS == theKernelError) {
		gControlReference = IOIteratorNext (theIterator);
		while (gControlReference != 0) {
			if (IOObjectConformsTo (gControlReference, "IOAudioToggleControl")) {
				// Do a real check looking at the subtype and usage
				theValue = IORegistryEntryCreateCFProperty (gControlReference, CFSTR ("IOAudioControlSubType"), kCFAllocatorDefault, 0);
				if (theValue) {
					CFNumberGetValue (theValue, kCFNumberSInt32Type, (void *)&theIntVal);
					CFRelease (theValue);
				}
				if (theIntVal != 'hpex') {
					gControlReference = IOIteratorNext (theIterator);
				} else {
					mHeadphoneExclusiveControl = gControlReference;
					break;			// get out of while loop
				}
			} else {
				gControlReference = IOIteratorNext (theIterator);
			}
		}
	}

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInOpen\n");
	if (theIterator)
		IOObjectRelease (theIterator);

	return theResult;
}

OSStatus AudioDriverPlugInClose (AudioDeviceID inDevice) {
//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInClose \n");

	if (gConnectionReference)
		IOServiceClose (gConnectionReference);
	if (gControlReference)
		IOObjectRelease (gControlReference);

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInClose\n");
	return 0;
}

OSStatus AudioDriverPlugInDeviceGetPropertyInfo (AudioDeviceID inDevice, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 * outSize, Boolean * outWritable) {
	char						theProp[5];
	OSStatus					theResult;
	CFDictionaryRef				theDict;

	convertDecTo4cc (inPropertyID, theProp);

	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInDeviceGetPropertyInfo %s\n", theProp);

	switch (inPropertyID) {
		case kAOAPropertyHeadphoneExclusive:
			if (mHeadphoneExclusiveControl) {
				if (outWritable) *outWritable = TRUE;
				if (outSize) *outSize = sizeof (UInt32);
				theResult = noErr;
			}
			break;
		case kAOAPropertyPowerState:
			if (outWritable) *outWritable = FALSE;
			if (outSize) *outSize = sizeof (UInt32);
			theResult = noErr;
			break;
		case kAOAPropertySelectionsReference:
			theDict = IORegistryEntryCreateCFProperty (ioAudioEngine, CFSTR ("MappingDictionary"), kCFAllocatorDefault, 0);
			if (theDict) {
				CFRelease (theDict);
				if (outWritable) *outWritable = FALSE;
				if (outSize) *outSize = sizeof (UInt32);
				theResult = noErr;
			}
			break;
		default:
			break;
	}

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInDeviceGetPropertyInfo\n");

	return theResult;
}

OSStatus GetIntPropertyData (io_registry_entry_t theEntry, CFStringRef theKey, void * outPropertyData) {
	CFNumberRef					theValue;
	OSStatus					theResult;
	SInt32						theIntVal;

	theResult = kAudioHardwareBadDeviceError;
	theValue = IORegistryEntryCreateCFProperty (theEntry, theKey, kCFAllocatorDefault, 0);
	if (theValue) {
		CFNumberGetValue (theValue, kCFNumberSInt32Type, (void *)&theIntVal);
		if (outPropertyData) *(UInt32 *)outPropertyData = theIntVal;
		CFRelease (theValue);
		theResult = noErr;
	}

	return theResult;
}

OSStatus AudioDriverPlugInDeviceGetProperty (AudioDeviceID inDevice, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 * ioPropertyDataSize, void * outPropertyData) {
	char						theProp[5];
	OSStatus					theResult;
	CFDictionaryRef				theDict;

	theResult = kAudioHardwareUnknownPropertyError;

	convertDecTo4cc (inPropertyID, theProp);
//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInDeviceGetProperty %s\n", theProp);

	switch (inPropertyID) {
		case kAOAPropertyHeadphoneExclusive:
			if (mHeadphoneExclusiveControl) {
				theResult = GetIntPropertyData (mHeadphoneExclusiveControl, CFSTR ("IOAudioControlValue"), outPropertyData);
				if (!theResult) {
					if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);
				}
			}
			break;
		case kAOAPropertyPowerState:
			theResult = GetIntPropertyData (ioAudioDevice, CFSTR ("IOAudioPowerState"), outPropertyData);
			if (!theResult) {
				if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);
			}
			break;
		case kAOAPropertySelectionsReference:
			theDict = IORegistryEntryCreateCFProperty (ioAudioEngine, CFSTR ("MappingDictionary"), kCFAllocatorDefault, 0);
			if (theDict) {
				if (outPropertyData) *(CFDictionaryRef *)outPropertyData = theDict;
				else CFRelease (theDict);
				if (!theResult) {
					if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);
				}
				// Don't release it, the caller will.
			}
			theResult = noErr;
			break;
		default:
			break;
	}

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInDeviceGetProperty\n");

	return theResult;
}

OSStatus AudioDriverPlugInDeviceSetProperty (AudioDeviceID inDevice, const AudioTimeStamp * inWhen, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 inPropertyDataSize, const void * inPropertyData) {
	char					theProp[5];
	OSStatus				theResult;
	UInt32 *				theUInt32ValPtr;
	UInt32					theIntVal;
	CFNumberRef				theCFValue;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInDeviceSetProperty %s\n", theProp);

	switch (inPropertyID) {
		case kAOAPropertyHeadphoneExclusive:
			if (mHeadphoneExclusiveControl) {
				theUInt32ValPtr = (UInt32 *)inPropertyData;
				theIntVal =  *theUInt32ValPtr;
				theCFValue = CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &theIntVal);
				theResult = IORegistryEntrySetCFProperty (mHeadphoneExclusiveControl, CFSTR ("IOAudioControlValue"), theCFValue);
				CFRelease (theCFValue);
				theResult = noErr;
			}
			break;
		case kAOAPropertyPowerState:
		case kAOAPropertySelectionsReference:
			theResult = kAudioHardwareIllegalOperationError;
			break;
		default:
			break;
	}

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInDeviceSetProperty\n");
	return theResult;                                    
}                                

OSStatus AudioDriverPlugInStreamGetPropertyInfo (AudioDeviceID inDevice, io_object_t inIOAudioStream, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 * outSize, Boolean * outWritable) {
	char						theProp[5];
	OSStatus					theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInStreamGetPropertyInfo  %s\n", theProp);

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInStreamGetPropertyInfo\n");
	return theResult;                                    
}

OSStatus AudioDriverPlugInStreamGetProperty (AudioDeviceID inDevice, io_object_t inIOAudioStream, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 * ioPropertyDataSize, void * outPropertyData) {
	char						theProp[5];
	OSStatus					theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInStreamGetProperty  %s\n", theProp);

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInStreamGetProperty\n");
	return theResult;                                    
}

OSStatus AudioDriverPlugInStreamSetProperty (AudioDeviceID inDevice, io_object_t inIOAudioStream, const AudioTimeStamp * inWhen, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 inPropertyDataSize, const void * inPropertyData) {
	char					theProp[5];
	OSStatus				theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_INFO, "+ HAL Plugin : AudioDriverPlugInStreamSetProperty  %s\n", theProp);

//	syslog (LOG_INFO, "- HAL Plugin :  AudioDriverPlugInStreamSetProperty\n");    
	return theResult;                                    
}

static void convertDecTo4cc (UInt32 input, char * output) {
	UInt32					mask;
	UInt32					val32;
	UInt8					theChar;
	short					idx;

	mask = 0xFF000000;

	for (idx = 0; idx < 4; idx++) {
		val32 = (input & (mask >> 8 * idx));
		theChar = (UInt8)(val32 >> 8 * (3 - idx));
		output[idx] = theChar;
	}
	output[idx]='\0';
}

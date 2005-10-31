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

typedef struct {
	io_object_t			ioAudioEngine;
	io_object_t			ioAudioDevice;
	UInt32				deviceID;
} PluginInfo, *PluginInfoPtr;

static io_registry_entry_t		gControlReference;
static io_registry_entry_t		mHeadphoneExclusiveControl;
static io_connect_t				gConnectionReference;
static CFMutableArrayRef		gPluginInfoArray = NULL;
static UInt32					sPluginInstance = 0;

static PluginInfoPtr AudioDriverPlugInGetPlugInInfoForDeviceID (UInt32 inDeviceID);
static io_object_t AudioDriverPlugInGetEngineForDeviceID (UInt32 inDeviceID);
static io_object_t AudioDriverPlugInGetDeviceForDeviceID (UInt32 inDeviceID);
static void ReleasePluginInfo(UInt32 inDeviceID);

static void convertDecTo4cc (UInt32 input, char * output);

OSStatus AudioDriverPlugInOpen (AudioDriverPlugInHostInfo * inHostInfo) {
	OSStatus					theResult;
	CFNumberRef					theValue;
	UInt32						theIntVal;
	io_iterator_t				theIterator;
	kern_return_t				theKernelError;
	//CFStringRef					theString;
	//char						cString[256];
	PluginInfoPtr				thePluginInfo;
		
	//syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInOpen[%ld] ", sPluginInstance);

	theResult = noErr;
	
	if (NULL == gPluginInfoArray) {
		gPluginInfoArray = CFArrayCreateMutable(NULL, 0, NULL);
		//syslog (LOG_ALERT, "creating new gPluginInfoArray = %p", gPluginInfoArray);
	}
	FailIf(NULL == gPluginInfoArray, Exit);
	
	thePluginInfo = (PluginInfoPtr)(malloc(sizeof(PluginInfo))); 
	//syslog (LOG_ALERT, " thePluginInfo = %p, size %d", thePluginInfo, sizeof(PluginInfo));
	FailIf(NULL == thePluginInfo, Exit);
	
	thePluginInfo->ioAudioEngine = inHostInfo->mIOAudioEngine;
	thePluginInfo->ioAudioDevice = inHostInfo->mIOAudioDevice;
	thePluginInfo->deviceID = inHostInfo->mDeviceID;

	CFArrayAppendValue(gPluginInfoArray, thePluginInfo); 

	//syslog (LOG_ALERT, " ioAudioEngine[%ld] = %p", sPluginInstance, thePluginInfo->ioAudioEngine);
	//syslog (LOG_ALERT, " ioAudioDevice[%ld] = %p", sPluginInstance, thePluginInfo->ioAudioDevice);
	//syslog (LOG_ALERT, " ioAudioDeviceID[%ld] = 0x%lX", sPluginInstance, thePluginInfo->deviceID);

	sPluginInstance++;

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

	//theString = IORegistryEntryCreateCFProperty (inHostInfo->mIOAudioEngine, CFSTR ("IOAudioEngineGlobalUniqueID"), kCFAllocatorDefault, 0);
	//CFStringGetCString(theString, cString, 256, 0);
	//syslog (LOG_ALERT, " IOAudioEngine[%p] guid = %s", inHostInfo->mIOAudioEngine, cString);

	if (theIterator)
		IOObjectRelease (theIterator);

Exit:
	//syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInOpen return %ld", theResult);
	return theResult;
}

OSStatus AudioDriverPlugInClose (AudioDeviceID inDeviceID) {
	CFIndex count;
	
	//syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInClose ");

	if (gConnectionReference)
		IOServiceClose (gConnectionReference);
	if (gControlReference)
		IOObjectRelease (gControlReference);
		
	ReleasePluginInfo(inDeviceID);	

	if (gPluginInfoArray) {
		count = CFArrayGetCount(gPluginInfoArray);
		//syslog (LOG_ALERT, "count = %ld", count);
		if (0 == count) {
			CFRelease(gPluginInfoArray);
			gPluginInfoArray = NULL;
			//syslog (LOG_ALERT, "gPluginInfoArray released");
		}
	}
	
	//syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInClose");
	return noErr;
}

OSStatus AudioDriverPlugInDeviceGetPropertyInfo (AudioDeviceID inDeviceID, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 * outSize, Boolean * outWritable) {
	char						theProp[5];
	OSStatus					theResult;
	CFDictionaryRef				theDict;
	CFNumberRef					theNumber;
	//CFStringRef					theString;
	//char						cString[256];
	io_object_t					theEngine;

	convertDecTo4cc (inPropertyID, theProp);

	theResult = kAudioHardwareUnknownPropertyError;

	//syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInDeviceGetPropertyInfo (0x%lX, %ld, %d, %s, %p, %p)", inDeviceID, inLine, isInput, theProp, outSize, outWritable);

	theEngine = AudioDriverPlugInGetEngineForDeviceID(inDeviceID);

	FailIf (nil == theEngine, Exit);

	switch (inPropertyID) {
		case kAOAPropertyPowerState:
			if (outWritable) *outWritable = FALSE;
			if (outSize) *outSize = sizeof (UInt32);
			theResult = noErr;
			break;
		case kAOAPropertySelectionsReference:
			//syslog (LOG_ALERT, "getPropertyInfo for kAOAPropertySelectionsReference");
			//syslog (LOG_ALERT, "   ioAudioEngine %p)", theEngine);	
			
			theDict = IORegistryEntryCreateCFProperty (theEngine, CFSTR ("MappingDictionary"), kCFAllocatorDefault, 0);
			//syslog (LOG_ALERT, "   theDict %p", theDict);
			//theString = IORegistryEntryCreateCFProperty (theEngine, CFSTR ("IOAudioEngineGlobalUniqueID"), kCFAllocatorDefault, 0);
			//syslog (LOG_ALERT, "   theString %p", theString);
			//CFStringGetCString(theString, cString, 256, 0);
			//syslog (LOG_ALERT, "   guid %s", cString);
			if (theDict) {
				CFRelease (theDict);
				if (outWritable) *outWritable = FALSE;
				if (outSize) *outSize = sizeof (UInt32);
				theResult = noErr;
			}
			break;
		case kAOAPropertyAvailableInputsBitmap:
			theNumber = IORegistryEntryCreateCFProperty (theEngine, CFSTR ("InputsBitmap"), kCFAllocatorDefault, 0);
			if (theNumber) {
				CFRelease (theNumber);
				if (outWritable) *outWritable = FALSE;
				if (outSize) *outSize = sizeof (UInt32);
				theResult = noErr;
			}
			break;
		case kAOAPropertyAvailableOutputsBitmap:
			theNumber = IORegistryEntryCreateCFProperty (theEngine, CFSTR ("OutputsBitmap"), kCFAllocatorDefault, 0);
			if (theNumber) {
				CFRelease (theNumber);
				if (outWritable) *outWritable = FALSE;
				if (outSize) *outSize = sizeof (UInt32);
				theResult = noErr;
			}
			break;
		default:
			break;
	}

	//syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInDeviceGetPropertyInfo");
Exit:
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

OSStatus AudioDriverPlugInDeviceGetProperty (AudioDeviceID inDeviceID, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 * ioPropertyDataSize, void * outPropertyData) {
	char						theProp[5];
	OSStatus					theResult;
	CFDictionaryRef				theDict;
	CFNumberRef					theNumber;
	CFStringRef					entryname;
	io_object_t					theEngine;
	io_object_t					theDevice;

	theResult = kAudioHardwareUnknownPropertyError;

	convertDecTo4cc (inPropertyID, theProp);
	//syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInDeviceGetProperty %s", theProp);

	theEngine = AudioDriverPlugInGetEngineForDeviceID(inDeviceID);
	theDevice = AudioDriverPlugInGetDeviceForDeviceID(inDeviceID);

	FailIf (nil == theEngine, Exit);
	FailIf (nil == theDevice, Exit);

	switch (inPropertyID) {
		case kAOAPropertyPowerState:
			theResult = GetIntPropertyData (theDevice, CFSTR ("IOAudioPowerState"), outPropertyData);
			if (!theResult) {
				if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);
			}
			break;
		case kAOAPropertySelectionsReference:
			theDict = IORegistryEntryCreateCFProperty (theEngine, CFSTR ("MappingDictionary"), kCFAllocatorDefault, 0);
			//syslog (LOG_ALERT, "   getProperty: kAOAPropertySelectionsReference theDict %p", theDict);
			if (theDict) {
				if (outPropertyData) *(CFDictionaryRef *)outPropertyData = theDict;
				else CFRelease (theDict);
				if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);
				// Don't release it, the caller will.
			}
			theResult = noErr;
			break;
		case kAOAPropertyAvailableInputsBitmap:
		case kAOAPropertyAvailableOutputsBitmap:
			entryname = ( inPropertyID == kAOAPropertyAvailableInputsBitmap) ? CFSTR ("InputsBitmap") : CFSTR ("OutputsBitmap");
		
			theNumber = IORegistryEntryCreateCFProperty (theEngine, entryname, kCFAllocatorDefault, 0);
			if (theNumber) {
				if (outPropertyData)
				{
					if ( CFNumberGetValue( theNumber, kCFNumberSInt32Type, outPropertyData ))
					{
						if (ioPropertyDataSize) *ioPropertyDataSize = sizeof (UInt32);

						theResult = noErr;
					}
					else
					{
						theResult = kAudioHardwareBadPropertySizeError;
					}
				}
				else
				{
					theResult = noErr;
				}
				CFRelease (theNumber);
			}
			break;
		default:
			break;
	}

	//syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInDeviceGetProperty returns %ld", theResult);
Exit:
	return theResult;
}

OSStatus AudioDriverPlugInDeviceSetProperty (AudioDeviceID inDeviceID, const AudioTimeStamp * inWhen, UInt32 inLine, Boolean isInput, AudioDevicePropertyID inPropertyID, UInt32 inPropertyDataSize, const void * inPropertyData) {
	char					theProp[5];
	OSStatus				theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

	//syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInDeviceSetProperty %s", theProp);

	switch (inPropertyID) {
		case kAOAPropertyPowerState:
		case kAOAPropertySelectionsReference:
		case kAOAPropertyAvailableInputsBitmap:
		case kAOAPropertyAvailableOutputsBitmap:
			theResult = kAudioHardwareIllegalOperationError;
			break;
		default:
			break;
	}

	//syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInDeviceSetProperty");
	return theResult;                                    
}                                

OSStatus AudioDriverPlugInStreamGetPropertyInfo (AudioDeviceID inDeviceID, io_object_t inIOAudioStream, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 * outSize, Boolean * outWritable) {
	char						theProp[5];
	OSStatus					theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInStreamGetPropertyInfo  %s", theProp);

//	syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInStreamGetPropertyInfo");
	return theResult;                                    
}

OSStatus AudioDriverPlugInStreamGetProperty (AudioDeviceID inDeviceID, io_object_t inIOAudioStream, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 * ioPropertyDataSize, void * outPropertyData) {
	char						theProp[5];
	OSStatus					theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInStreamGetProperty  %s", theProp);

//	syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInStreamGetProperty");
	return theResult;                                    
}

OSStatus AudioDriverPlugInStreamSetProperty (AudioDeviceID inDeviceID, io_object_t inIOAudioStream, const AudioTimeStamp * inWhen, UInt32 inChannel, AudioDevicePropertyID inPropertyID, UInt32 inPropertyDataSize, const void * inPropertyData) {
	char					theProp[5];
	OSStatus				theResult;

	convertDecTo4cc (inPropertyID, theProp);
	theResult = kAudioHardwareUnknownPropertyError;

//	syslog (LOG_ALERT, "+ HAL Plugin : AudioDriverPlugInStreamSetProperty  %s", theProp);

//	syslog (LOG_ALERT, "- HAL Plugin :  AudioDriverPlugInStreamSetProperty");    
	return theResult;                                    
}

PluginInfoPtr AudioDriverPlugInGetPlugInInfoForDeviceID (UInt32 inDeviceID) {
	CFIndex count;
	CFIndex index;
	PluginInfoPtr thePluginInfo = NULL;

	FailIf(NULL == gPluginInfoArray, Exit);
	
	count = CFArrayGetCount(gPluginInfoArray);

	for (index = 0; index < count; index++) {
		thePluginInfo = (PluginInfoPtr)CFArrayGetValueAtIndex(gPluginInfoArray, index);
		if (thePluginInfo) {
			if (thePluginInfo->deviceID == inDeviceID) {
				break;
			}
		}
	}
	
Exit:
	return thePluginInfo;
}

io_object_t AudioDriverPlugInGetEngineForDeviceID (UInt32 inDeviceID) {
	io_object_t theEngine = 0;
	PluginInfoPtr thePluginInfo = NULL;

	thePluginInfo = AudioDriverPlugInGetPlugInInfoForDeviceID(inDeviceID);	
	if (thePluginInfo) {
		theEngine = thePluginInfo->ioAudioEngine;
	}

	return theEngine;
}

io_object_t AudioDriverPlugInGetDeviceForDeviceID (UInt32 inDeviceID) {
	io_object_t theDevice = 0;
	PluginInfoPtr thePluginInfo = NULL;

	thePluginInfo = AudioDriverPlugInGetPlugInInfoForDeviceID(inDeviceID);
	if (thePluginInfo) {
		theDevice = thePluginInfo->ioAudioDevice;
	}

	return theDevice;
}

void ReleasePluginInfo(UInt32 inDeviceID) {
	CFIndex count;
	CFIndex index;
	PluginInfoPtr thePluginInfo = NULL;

	FailIf(NULL == gPluginInfoArray, Exit);

	count = CFArrayGetCount(gPluginInfoArray);

	for (index = 0; index < count; index++) {
		thePluginInfo = (PluginInfoPtr)CFArrayGetValueAtIndex(gPluginInfoArray, index);
		if (thePluginInfo) {
			if (thePluginInfo->deviceID == inDeviceID) {
				CFArrayRemoveValueAtIndex(gPluginInfoArray, index);
				free (thePluginInfo);
				break;
			}
		}
	}

Exit:
	return;
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

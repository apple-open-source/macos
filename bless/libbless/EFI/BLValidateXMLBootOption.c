/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  BLValidateXMLBootOption.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/7/06.
 *  Copyright 2006-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <IOKit/IOCFUnserialize.h>

#include "bless.h"
#include "bless_private.h"

#define kBL_GLOBAL_NVRAM_GUID "8BE4DF61-93CA-11D2-AA0D-00E098032B8C"

typedef uint8_t		EFI_UINT8;
typedef uint16_t	EFI_UINT16;
typedef uint16_t	EFI_CHAR16;
typedef uint32_t	EFI_UINT32;
typedef EFI_UINT8	EFI_DEVICE_PATH_PROTOCOL;

typedef struct _BLESS_EFI_LOAD_OPTION {
    EFI_UINT32                          Attributes; 
    EFI_UINT16                          FilePathListLength; 
    EFI_CHAR16                          Description[0]; 
    EFI_DEVICE_PATH_PROTOCOL            FilePathList[0]; 
    EFI_UINT8                           OptionalData[0]; 
} BLESS_EFI_LOAD_OPTION;

static int _getBootOptionNumber(BLContextPtr context, io_registry_entry_t options, uint16_t *bootOptionNumber);
static BLESS_EFI_LOAD_OPTION * _getBootOptionData(BLContextPtr context, io_registry_entry_t options, uint16_t bootOptionNumber, size_t *bootOptionSize);
static EFI_DEVICE_PATH_PROTOCOL * _getBootDevicePath(BLContextPtr context, io_registry_entry_t options, CFStringRef name, size_t *devicePathSize);
static CFArrayRef _getBootDeviceXML(BLContextPtr context, io_registry_entry_t options, CFStringRef name);

static int _validate(BLContextPtr context, BLESS_EFI_LOAD_OPTION *bootOption, 
					 size_t bootOptionSize, EFI_DEVICE_PATH_PROTOCOL *devicePath,
					 size_t devicePathSize, CFArrayRef xmlPath);

int BLValidateXMLBootOption(BLContextPtr context,
							CFStringRef	 xmlName,
							CFStringRef	 binaryName)
{
	
	io_registry_entry_t optionsNode = 0;
	uint16_t		bootOptionNumber = 0;
	int				ret;

    BLESS_EFI_LOAD_OPTION *bootOption = NULL;
	EFI_DEVICE_PATH_PROTOCOL *devicePath = NULL;
	size_t				bootOptionSize, devicePathSize;
	CFArrayRef			xmlPath = NULL;
	
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
	ret = _getBootOptionNumber(context, optionsNode, &bootOptionNumber);
	if(ret)
		return 2;

	bootOption = _getBootOptionData(context, optionsNode, bootOptionNumber, &bootOptionSize);
	if(bootOption == NULL)
		return 3;

	devicePath = _getBootDevicePath(context, optionsNode, binaryName, &devicePathSize);
	if(devicePath == NULL)
		return 4;
	
	xmlPath = _getBootDeviceXML(context, optionsNode, xmlName);
	if(xmlPath == NULL)
		return 5;
	
	ret = _validate(context, bootOption, bootOptionSize, devicePath, devicePathSize, xmlPath);
	
	free(bootOption);
	free(devicePath);
	CFRelease(xmlPath);
    
	IOObjectRelease(optionsNode);

	if(ret) {
		contextprintf(context, kBLLogLevelError,  "Boot option does not match XML representation\n");
		return 1;
	} else {
		contextprintf(context, kBLLogLevelVerbose,  "Boot option matches XML representation\n");
		return 0;
	}
}

static int _getBootOptionNumber(BLContextPtr context, io_registry_entry_t options, uint16_t *bootOptionNumber)
{
	CFDataRef       dataRef;
	const uint16_t	*orderBuffer;
	
	dataRef = IORegistryEntryCreateCFProperty(options, 
											 CFSTR(kBL_GLOBAL_NVRAM_GUID ":BootOrder"),
											 kCFAllocatorDefault, 0);
    
    if(dataRef == NULL) {
        contextprintf(context, kBLLogLevelError,  "Could not access BootOrder\n");
        return 2;
	}
	
	if(CFGetTypeID(dataRef) != CFDataGetTypeID() || CFDataGetLength(dataRef) < sizeof(uint16_t)) {
		if(dataRef) CFRelease(dataRef);
        contextprintf(context, kBLLogLevelError,  "Invalid BootOrder\n");
		return 2;
	}
    
	orderBuffer = (const uint16_t *)CFDataGetBytePtr(dataRef);
	*bootOptionNumber = CFSwapInt16LittleToHost(*orderBuffer);
	
	CFRelease(dataRef);
	
	return 0;
}

static BLESS_EFI_LOAD_OPTION * _getBootOptionData(BLContextPtr context, io_registry_entry_t options, uint16_t bootOptionNumber, size_t *bootOptionSize)
{
    char            bootName[1024];
	CFStringRef		nvramName;
	CFDataRef		dataRef;
	BLESS_EFI_LOAD_OPTION *buffer = NULL;
	
	snprintf(bootName, sizeof(bootName), "%s:Boot%04hx", kBL_GLOBAL_NVRAM_GUID, bootOptionNumber);
	contextprintf(context, kBLLogLevelVerbose,  "Boot option is %s\n", bootName);
	
	nvramName = CFStringCreateWithCString(kCFAllocatorDefault, bootName,kCFStringEncodingUTF8);
	if(nvramName == NULL) {
		return NULL;
	}
	
	dataRef = IORegistryEntryCreateCFProperty(options, 
											  nvramName,
											  kCFAllocatorDefault, 0);
    
    if(dataRef == NULL) {
		CFRelease(nvramName);
        contextprintf(context, kBLLogLevelError,  "Could not access Boot%04hx\n", bootOptionNumber);
        return NULL;
	}

	CFRelease(nvramName);

	if(CFGetTypeID(dataRef) != CFDataGetTypeID()) {
		if(dataRef) CFRelease(dataRef);
        contextprintf(context, kBLLogLevelError,  "Invalid Boot%04hx\n", bootOptionNumber);
		return NULL;
	}
	
	*bootOptionSize = CFDataGetLength(dataRef);
	buffer = (BLESS_EFI_LOAD_OPTION *)calloc(*bootOptionSize, sizeof(char));
	if(buffer == NULL)
		return NULL;
	
	memcpy(buffer, CFDataGetBytePtr(dataRef), *bootOptionSize);
	
	CFRelease(dataRef);

	return buffer;
}

static EFI_DEVICE_PATH_PROTOCOL * _getBootDevicePath(BLContextPtr context, io_registry_entry_t options, CFStringRef name, size_t *devicePathSize)
{
	CFDataRef		dataRef;
	EFI_DEVICE_PATH_PROTOCOL *buffer = NULL;
	
	dataRef = IORegistryEntryCreateCFProperty(options, 
											  name,
											  kCFAllocatorDefault, 0);
    
    if(dataRef == NULL) {
        contextprintf(context, kBLLogLevelError,  "Could not access boot device\n");
        return NULL;
	}
	
	if(CFGetTypeID(dataRef) != CFDataGetTypeID()) {
		if(dataRef) CFRelease(dataRef);
        contextprintf(context, kBLLogLevelError,  "Invalid boot device\n");
		return NULL;
	}
	
	*devicePathSize = CFDataGetLength(dataRef);
	buffer = (EFI_DEVICE_PATH_PROTOCOL *)calloc(*devicePathSize, sizeof(char));
	if(buffer == NULL)
		return NULL;
	
	memcpy(buffer, CFDataGetBytePtr(dataRef), *devicePathSize);

	CFRelease(dataRef);
	
	return buffer;
}

static CFArrayRef _getBootDeviceXML(BLContextPtr context, io_registry_entry_t options, CFStringRef name)
{
	int ret;
	CFStringRef stringVal = NULL;
	char        buffer[1024];
	CFArrayRef	arrayRef;
	
	ret = BLCopyEFINVRAMVariableAsString(context, name, &stringVal);
	if(ret || stringVal == NULL) {
		return NULL;
	}

    if(!CFStringGetCString(stringVal, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
		CFRelease(stringVal);
        return NULL;
    }
    
	CFRelease(stringVal);

    arrayRef = IOCFUnserialize(buffer,
                               kCFAllocatorDefault,
                               0,
                               NULL);
    if(arrayRef == NULL) {
        contextprintf(context, kBLLogLevelError, "Could not unserialize string\n");
        return NULL;
    }
    
    if(CFGetTypeID(arrayRef) != CFArrayGetTypeID()) {
        CFRelease(arrayRef);
        contextprintf(context, kBLLogLevelError, "Bad type in XML string\n");
        return NULL;        
    }
	
	
	return arrayRef;
}

static int _validate(BLContextPtr context, BLESS_EFI_LOAD_OPTION *bootOption, 
					 size_t bootOptionSize, EFI_DEVICE_PATH_PROTOCOL *devicePath,
					 size_t devicePathSize, CFArrayRef xmlPath)
{
	EFI_CHAR16		*description;
	int				i;
	char			debugDesc[100];
	EFI_DEVICE_PATH_PROTOCOL	*bootDev;
	EFI_UINT8		*OptionalData;
	size_t			OptionalDataSize;
	CFIndex			j, count;
	size_t			bufferSize = 0;
	EFI_UINT8		*buffer = NULL;

	
	description = bootOption->Description;
	
	for(i=0; description[i]; i++) {
		debugDesc[i] = (char)CFSwapInt16LittleToHost(description[i]);
	}
	debugDesc[i] = '\0';
	
	// i is the size of the description string
	contextprintf(context, kBLLogLevelVerbose, "Processing boot option '%s'\n", debugDesc);

	bootDev = (EFI_DEVICE_PATH_PROTOCOL	*)&bootOption->Description[i+1];
	bootOption->FilePathListLength = CFSwapInt16LittleToHost(bootOption->FilePathListLength);
	
	if((bootOption->FilePathListLength != devicePathSize)
	   || (0 != memcmp(bootDev, devicePath, devicePathSize))) {
		contextprintf(context, kBLLogLevelVerbose, "Boot device path incorrect\n");	
		return 1;
	}
	
	OptionalData = ((EFI_UINT8 *)bootDev) + bootOption->FilePathListLength;
	OptionalDataSize = bootOptionSize - ((intptr_t)OptionalData - (intptr_t)bootOption);
	
	count = CFArrayGetCount(xmlPath);
	for(j=0; j < count; j++) {
		CFDictionaryRef element = CFArrayGetValueAtIndex(xmlPath, j);
		CFTypeRef	val;
		
		if(CFGetTypeID(element) != CFDictionaryGetTypeID())
			continue;
		
		val = CFDictionaryGetValue(element, CFSTR("IOEFIBootOption"));
		if(val == NULL)
			continue;
		
		if(CFGetTypeID(val) == CFStringGetTypeID()) {
			bufferSize = (CFStringGetLength(val)+1)*2;
			buffer = (EFI_UINT8 *)calloc(bufferSize, sizeof(char));
			
			if(!CFStringGetCString(val, (char *)buffer, bufferSize, kCFStringEncodingUTF16LE)) {
				free(buffer);
				buffer = NULL;
				bufferSize = 0;
				continue;
			}
				
		} else if(CFGetTypeID(val) == CFDataGetTypeID()) {
			bufferSize = CFDataGetLength(val);
			buffer = (EFI_UINT8 *)calloc(bufferSize, sizeof(char));

			memcpy(buffer, CFDataGetBytePtr(val), bufferSize);
		}
		
	}
	
	// if either the boot option or the XML has this, we need to validate
	if(OptionalDataSize || bufferSize) {
		if((OptionalDataSize != bufferSize)
		   || (0 != memcmp(OptionalData, buffer, bufferSize))) {
			contextprintf(context, kBLLogLevelVerbose, "Optional data incorrect\n");	
			free(buffer);
			return 2;
		}
	}
		
	if(buffer)
		free(buffer);
	
	return 0;
}


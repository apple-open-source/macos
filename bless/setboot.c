/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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
 *  setboot.c
 *  bless
 *
 *  Created by Shantonu Sen on 1/14/05.
 *  Copyright 2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: setboot.c,v 1.21 2006/01/02 22:27:28 ssen Exp $
 *
 *  $Log: setboot.c,v $
 *  Revision 1.21  2006/01/02 22:27:28  ssen
 *  <rdar://problem/4395370> bless should not support BIOS systems
 *  For RC_RELEASE=Leopard, keep BIOS support, but preprocess it out
 *  for Herbie and the open source build
 *
 *  Revision 1.20  2005/12/07 04:49:17  ssen
 *  Add support for --netboot options: --booter, --kernel, --mkext.
 *  Should make it easy to set your system to ANI-style netboot.
 *  --mkext doesn't work on ppc
 *
 *  Revision 1.19  2005/12/02 22:51:56  ssen
 *  Infrastructure for --getBoot, to interpret efi-boot-device as
 *  either a network path, or a disk device
 *
 *  Revision 1.18  2005/12/02 19:13:49  ssen
 *  Enhance bless --device so it supports all the good stuff like
 *  --options and --nextonly
 *
 *  Revision 1.17  2005/12/01 20:01:20  ssen
 *  <rdar://problem/4332212> Use unique matching properties for boot device
 *  Change this around so that the main IOMatch dictionary
 *  is persistent and can be copy-pasted
 *
 *  Revision 1.16  2005/11/21 17:58:38  ssen
 *  Fix a bad bug where old boot-args weren't being parsed
 *  properly, leading to panics on next boot.
 *
 *  Revision 1.15  2005/11/17 01:07:35  ssen
 *  fix off-by-1 bug
 *
 *  Revision 1.14  2005/11/17 00:22:10  ssen
 *  <rdar://problem/4344363> Bless needs to zero out kernel/mkext fields when booting from local disk
 *  Unset efi-boot-file and efi-boot-mkext, and filter boot-args
 *
 *  Revision 1.13  2005/11/16 00:13:29  ssen
 *  Validate --server, do special stuff on EFI systems
 *
 *  Revision 1.12  2005/11/11 23:48:57  ssen
 *  <rdar://problem/4311178> Bless should support setting efi Boot#### optional data
 *  <rdar://problem/4311108> Bless should support setting BootNext in EFI
 *  Support --nextonly and --options
 *
 *  Revision 1.11  2005/11/09 22:44:21  ssen
 *  Implement -firmware mode for EFI-based flashers
 *
 *  Revision 1.10  2005/11/05 00:04:28  ssen
 *  <rdar://problem/4255345> bless needs to write the nvram device path with booting on non-BIOS Yellow systems
 *  Poke NVRAM directly with IOKit, to avoid type guessing by nvram(8)
 *
 *  Revision 1.9  2005/11/03 19:46:13  ssen
 *  <rdar://problem/4255345> bless needs to write the nvram device path with booting on non-BIOS Yellow systems
 *  Initial work to support EFI nvram boot selection
 *
 *  Revision 1.8  2005/07/29 23:55:09  ssen
 *  for --setBoot, stub it out for EFI
 *
 *  Revision 1.7  2005/06/24 16:39:48  ssen
 *  Don't use "unsigned char[]" for paths. If regular char*s are
 *  good enough for the BSD system calls, they're good enough for
 *  bless.
 *
 *  Revision 1.6  2005/02/23 19:52:49  ssen
 *  start work on -firmware mode
 *
 *  Revision 1.5  2005/02/09 00:17:56  ssen
 *  If doing a setboot on UFS, HFSX, or RAID, and a label
 *  was not explicitly specified,  query DiskArb and render to label
 *
 *  Revision 1.4  2005/02/08 00:18:45  ssen
 *  Implement support for offline updating of BootX and OF labels
 *  in Apple_Boot partitions, and also for RAIDs. Only works
 *  in --device mode so far
 *
 *  Revision 1.3  2005/02/03 00:42:22  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.2  2005/01/16 02:10:43  ssen
 *  Move updating of RAID booters into common setboot() routine
 *
 *  Revision 1.1  2005/01/14 22:28:22  ssen
 *  <rdar://problem/3861859> bless needs to try getProperty(kIOBootDeviceKey)
 *  Consolidate code to set OF or active BIOS partition
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOCFSerialize.h>
#include <DiskArbitration/DiskArbitration.h>
#include <CoreFoundation/CoreFoundation.h>

#include "enums.h"

#include "bless.h"

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));


static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
							 CFDataRef labelData);

int setefidevice(BLContextPtr context, const char * bsdname, int bootNext, const char *optionalData);
int setefifilepath(BLContextPtr context, const char * path, int bootNext, const char *optionalData);
int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
							 CFStringRef kernelXML, CFStringRef mkextXML,
                             int bootNext);
static int setit(BLContextPtr context, mach_port_t masterPort, const char *bootvar, CFStringRef xmlstring);

static int setefibootargs(BLContextPtr context, mach_port_t masterPort);

int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData)
{
	int err;	
	CFTypeRef bootData = NULL;
	BLPreBootEnvType	preboot;
	
	err = BLGetPreBootEnvironmentType(context, &preboot);
	if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine preboot environment\n");
		return 1;
	}
	
	err = BLGetRAIDBootDataForDevice(context, device, &bootData);
	if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Error while determining if %s is a RAID\n", device );
	    return 3;
	}
	
	if(bootData) {
		// might be either an array or a dictionary
		err = BLUpdateRAIDBooters(context, device, bootData, bootxData, labelData);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error while updating RAID booters for %s\n", device );			
			// we keep going, since BootX may be able to reconstruct the RAID
		}
		CFRelease(bootData);
	} else {
		err = updateAppleBootIfPresent(context, device, bootxData, labelData);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error while updating booter for %s\n", device );			
		}		
	}
		
	if(preboot == kBLPreBootEnvType_OpenFirmware) {
		err = BLSetOpenFirmwareBootDevice(context, device);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
	} else if(preboot == kBLPreBootEnvType_EFI) {
        err = setefidevice(context, device + 5, 0, NULL);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set EFI\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "EFI set successfully\n" );
		}
	} else {
        blesscontextprintf(context, kBLLogLevelError,  "Unknown system type\n");
        return 1;
    }
	
	return 0;	
}


static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
									CFDataRef labelData)
{
	char booterDev[MAXPATHLEN];
	io_service_t            service = 0;
	CFStringRef				name = NULL;
	int32_t					needsBooter	= 0;
	int32_t					isBooter	= 0;
	BLUpdateBooterFileSpec	*spec = NULL;
	int32_t					specCount = 0, currentCount = 0;
	
	int ret;
	
	strcpy(booterDev, "/dev/");
	
	ret = BLDeviceNeedsBooter(context, device,
							  &needsBooter,
							  &isBooter,
							  &service);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine if partition needs booter\n" );		
		return 1;
	}
	
	if(!(needsBooter || isBooter))
		return 0;
	
	for(;;) {
		DADiskRef disk = NULL;
		DASessionRef session = NULL;
		CFDictionaryRef props = NULL;
		CFStringRef	daName = NULL;
		char label[MAXPATHLEN];

		
		if(labelData) break; // no need to generate
		
		session = DASessionCreate(kCFAllocatorDefault);
		if(session == NULL) {
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't connect to DiskArb\n");
			break;
		}
		
		disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device+5);
		if(disk == NULL) {
			CFRelease(session);
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't create DADisk for %s\n",
						  device + 5);
			break;
		}
		
		props = DADiskCopyDescription(disk);
		if(props == NULL) {
			CFRelease(session);
			CFRelease(disk);
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't get properties for %s\n",
						  device + 5);
			break;
		}
		
		daName = CFDictionaryGetValue(props, kDADiskDescriptionVolumeNameKey);
		if(daName == NULL) {
			CFRelease(props);
			CFRelease(disk);
			CFRelease(session);	
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't get properties for %s\n",
							   device + 5);
			break;			
		}
		
		
		
		if(!CFStringGetCString(daName, label, sizeof(label),
							   kCFStringEncodingUTF8)) {
			CFRelease(props);
			CFRelease(disk);
			CFRelease(session);	
			break;
		}

		CFRelease(props);
		CFRelease(disk);
		CFRelease(session);	
		
		ret = BLGenerateOFLabel(context, label, &labelData);
		if(ret)
			labelData = NULL;
		
		break;
	}
	
	if(!(bootxData || labelData)) {
		IOObjectRelease(service);
		return 0;
	}
	
	name = IORegistryEntryCreateCFProperty( service, CFSTR(kIOBSDNameKey),
											kCFAllocatorDefault, 0);
	
	if(name == NULL || CFStringGetTypeID() != CFGetTypeID(name)) {
		IOObjectRelease(service);
		blesscontextprintf(context, kBLLogLevelError,  "Could not find bsd name for %x\n" , service);
		return 2;
	}
	
	IOObjectRelease(service); service = 0;
	
	if(!CFStringGetCString(name,booterDev+5,sizeof(booterDev)-5,kCFStringEncodingUTF8)) {
		CFRelease(name);
		blesscontextprintf(context, kBLLogLevelError,  "Could not find bsd name for %x\n" , service);
		return 3;
	}
	
	CFRelease(name);
	
	if(labelData) specCount += 2;
	if(bootxData) specCount += 1;
	
	spec = calloc(specCount, sizeof(spec[0]));
	
	if(labelData) {
	
		spec[currentCount+0].version = 0;
		spec[currentCount+0].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		spec[currentCount+0].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+0].reqFilename = NULL;
		spec[currentCount+0].payloadData = labelData;
		spec[currentCount+0].postType = 0; // no type
		spec[currentCount+0].postCreator = 0; // no type
		spec[currentCount+0].foundFile = 0;
		spec[currentCount+0].updatedFile = 0;
		
		spec[currentCount+1].version = 0;
		spec[currentCount+1].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER;
		spec[currentCount+1].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+1].reqFilename = NULL;
		spec[currentCount+1].payloadData = labelData;
		spec[currentCount+1].postType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		spec[currentCount+1].postCreator = 0; // no type
		spec[currentCount+1].foundFile = 0;
		spec[currentCount+1].updatedFile = 0;
		
		currentCount += 2;
	}
	
	if(bootxData) {
		spec[currentCount+0].version = 0;
		spec[currentCount+0].reqType = kBL_OSTYPE_PPC_TYPE_BOOTX;
		spec[currentCount+0].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+0].reqFilename = NULL;
		spec[currentCount+0].payloadData = bootxData;
		spec[currentCount+0].postType = 0; // no type
		spec[currentCount+0].postCreator = 0; // no type
		spec[currentCount+0].foundFile = 0;
		spec[currentCount+0].updatedFile = 0;
	}
	
	ret = BLUpdateBooter(context, booterDev, spec, specCount);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Error enumerating HFS+ volume\n");		
		return 1;
	}
	
	if(bootxData) {
		if(!(spec[currentCount].foundFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "No pre-existing BootX found in HFS+ volume\n");
			return 2;
		}			

		if(!(spec[currentCount].updatedFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "BootX was not updated\n");
			return 3;
		}			
	}
	
    if(labelData) {
		if(!(spec[0].foundFile || spec[1].foundFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "No pre-existing OF label found in HFS+ volume\n");
			return 2;
		}
		if(!(spec[0].updatedFile || spec[1].updatedFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "OF label was not updated\n");
			return 3;
		}
	}

	free(spec);
	
	return 0;
}

int setefidevice(BLContextPtr context, const char * bsdname, int bootNext, const char *optionalData)
{
    int ret;

    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    
    ret = BLCreateEFIXMLRepresentationForDevice(context,
                                              bsdname,
                                              optionalData,
                                              &xmlString);
    if(ret) {
        return 1;
    }
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }    
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);    
    CFRelease(xmlString);
    if(ret) return ret;

    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));    
    if(ret) return ret;

    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));    
    if(ret) return ret;
    
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
        
    return ret;
}

static int setit(BLContextPtr context, mach_port_t masterPort, const char *bootvar, CFStringRef xmlstring)
{
    
    io_registry_entry_t optionsNode = 0;
    CFStringRef bootName = NULL;
    kern_return_t kret;
    char    cStr[1024];

    optionsNode = IORegistryEntryFromPath(masterPort, kIODeviceTreePlane ":/options");
    
    if(MACH_PORT_NULL == optionsNode) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
    bootName = CFStringCreateWithCString(kCFAllocatorDefault, bootvar, kCFStringEncodingUTF8);
    if(bootName == NULL) {
        IOObjectRelease(optionsNode);
        return 2;
    }
    
    CFStringGetCString(xmlstring, cStr, sizeof(cStr), kCFStringEncodingUTF8);
    
    blesscontextprintf(context, kBLLogLevelVerbose,  "Setting EFI NVRAM:\n" );
    blesscontextprintf(context, kBLLogLevelVerbose,  "\t%s='%s'\n", bootvar, cStr );

    kret = IORegistryEntrySetCFProperty(optionsNode, bootName, xmlstring);
    if(kret) {
        IOObjectRelease(optionsNode);
        blesscontextprintf(context, kBLLogLevelError,  "Could not set boot device property: %#x\n", kret);
        return 2;        
    }
    
    IOObjectRelease(optionsNode);
        
    return 0;
}

int setefifilepath(BLContextPtr context, const char * path, int bootNext, const char *optionalData)
{
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    int ret;
    
    ret = BLCreateEFIXMLRepresentationForPath(context,
                                              path,
                                              optionalData,
                                              &xmlString);
    if(ret) {
        return 1;
    }
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);
    CFRelease(xmlString);
    if(ret) {
        return 2;
    }        
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));    
    if(ret) return ret;
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));    
    if(ret) return ret;
        
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
        
    return 0;
}

int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
							 CFStringRef kernelXML, CFStringRef mkextXML,
                             int bootNext)
{
    const char *bootString = NULL;
    int ret;
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }
    
    ret = setit(context, kIOMasterPortDefault, bootString, booterXML);
    if(ret) return ret;
    
	if(kernelXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-file", kernelXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));
	}
    if(ret) return ret;

	if(mkextXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-mkext", mkextXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));
	}
    if(ret) return ret;
	
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
    
    return 0;
}

// fetch old args. If set, filter them and reset
static int setefibootargs(BLContextPtr context, mach_port_t masterPort)
{
    
    int             ret;
    char        cStr[1024], newArgs[1024];
    CFStringRef     newString;
    
    ret = BLCopyEFINVRAMVariableAsString(context,
                                   CFSTR("boot-args"),
                                   &newString);
    
    if(ret) {
        blesscontextprintf(context, kBLLogLevelError,  "Error getting NVRAM variable \"boot-args\"\n");        
        return 1;
    }
    
    if(newString == NULL) {
        // nothing set. that's OK
        blesscontextprintf(context, kBLLogLevelVerbose,  "NVRAM variable \"boot-args\" not set.\n");        
        return 0;        
    }
        
    if(!CFStringGetCString(newString, cStr, sizeof(cStr), kCFStringEncodingUTF8)) {
        blesscontextprintf(context, kBLLogLevelError,  "Could not interpret boot-args as string. Ignoring...\n");
        strcpy(cStr, "");
    }
    
    ret = BLPreserveBootArgs(context, cStr, newArgs);
    if(ret) {
        return ret;
    }
        
    newString = CFStringCreateWithCString(kCFAllocatorDefault, newArgs, kCFStringEncodingUTF8);
    if(newString == NULL) {
        return 2;
    }

    ret = setit(context, masterPort, "boot-args", newString);
    if(ret)
        return ret;
    
    return 0;
}


/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: setboot.c,v 1.30 2006/07/19 00:15:36 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOCFSerialize.h>
#include <CoreFoundation/CoreFoundation.h>

#include "enums.h"
#include "bless.h"
#include "bless_private.h"
#include "protos.h"

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif

#if SUPPORT_RAID
static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
							 CFDataRef labelData);
#endif

static int setit(BLContextPtr context, mach_port_t masterPort, const char *bootvar, CFStringRef xmlstring);

static int setefibootargs(BLContextPtr context, mach_port_t masterPort);
static int _forwardNVRAM(BLContextPtr context, CFStringRef from, CFStringRef to);

int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData)
{
	int err;	
	BLPreBootEnvType	preboot;
	
	err = BLGetPreBootEnvironmentType(context, &preboot);
	if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine preboot environment\n");
		return 1;
	}
	
#if SUPPORT_RAID
	CFTypeRef bootData = NULL;

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
#endif // SUPPORT_RAID
		
	if(preboot == kBLPreBootEnvType_OpenFirmware) {
		err = BLSetOpenFirmwareBootDevice(context, device);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
	} else if(preboot == kBLPreBootEnvType_EFI) {
        err = setefidevice(context, device + 5, 0, 0, NULL, NULL, false);
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

#if SUPPORT_RAID
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
		char label[MAXPATHLEN];
#if USE_DISKARBITRATION
		DADiskRef disk = NULL;
		DASessionRef session = NULL;
		CFDictionaryRef props = NULL;
		CFStringRef	daName = NULL;

		
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
#else // !USE_DISKARBITRATION
		strlcpy(label, "Unknown", sizeof(label));
#endif // !USE_DISKARBITRATION
		
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
#endif // SUPPORT_RAID

int setefidevice(BLContextPtr context, const char * bsdname, int bootNext,
				 int bootLegacy, const char *legacyHint, const char *optionalData, bool shortForm)
{
    int ret;

    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    
	if(bootLegacy) {
        if(legacyHint) {
            ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                legacyHint+5,
                                                NULL,
                                                &xmlString,
                                                false);

            if(ret) {
                return 1;
            }
            
            ret = setit(context, kIOMasterPortDefault, "efi-legacy-drive-hint", xmlString);    
            if(ret) return ret;

            ret = _forwardNVRAM(context, CFSTR("efi-legacy-drive-hint-data"), CFSTR("BootCampHD"));
            if(ret) return ret;     
            
            ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-legacy-drive-hint"));    
            if(ret) return ret;
       
        }

        ret = BLCreateEFIXMLRepresentationForLegacyDevice(context,
                                                  bsdname,
                                                  &xmlString);
	} else {
        // the given device may be pointing at a RAID
        CFDictionaryRef dict = NULL;
        CFArrayRef array = NULL;
        char    newBSDName[MAXPATHLEN];
        CFStringRef firstBooter = NULL;
        CFStringRef firstData = NULL;
        
        strcpy(newBSDName, bsdname);
        
        ret = BLCreateBooterInformationDictionary(context, newBSDName,
                                                    &dict);
        if(ret) {
            return 1;
        }
        
        // check to see if there's a booter partition. If so, use it
        array = CFDictionaryGetValue(dict, kBLAuxiliaryPartitionsKey);
        if(array) {
            if(CFArrayGetCount(array) > 0) {
                firstBooter = CFArrayGetValueAtIndex(array, 0);
                if(!CFStringGetCString(firstBooter, newBSDName, sizeof(newBSDName),
                                       kCFStringEncodingUTF8)) {
                    return 1;
                }
                blesscontextprintf(context, kBLLogLevelVerbose, "Substituting booter %s\n", newBSDName);
            }
        }

        // check to see if there's a data partition (without auxiliary) that
        // we should boot from
        if (!firstBooter) {
            array = CFDictionaryGetValue(dict, kBLDataPartitionsKey);
            if(array) {
                if(CFArrayGetCount(array) > 0) {
                    firstData = CFArrayGetValueAtIndex(array, 0);
                    if(!CFStringGetCString(firstData, newBSDName, sizeof(newBSDName),
                                           kCFStringEncodingUTF8)) {
                        return 1;
                    }
                    if (0 == strcmp(newBSDName, bsdname)) {
                        /* same as before, no substitution */
                        firstData = NULL;
                    } else {
                        blesscontextprintf(context, kBLLogLevelVerbose, "Substituting bootable data partition %s\n", newBSDName);
                    }
                }
            }
        }

        ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                    newBSDName,
                                                    optionalData,
                                                    &xmlString,
                                                    shortForm);

        
        CFRelease(dict);

	}
		
    if(ret) {
        return 1;
    }
    
	// TODO merge this code with BLSetEFIBootDevice() [9300208]
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }    
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);    
    CFRelease(xmlString);
    if(ret) return ret;

	ret = efinvramcleanup(context);
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
    
    if(IO_OBJECT_NULL == optionsNode) {
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
        CFRelease(bootName);
        IOObjectRelease(optionsNode);
        blesscontextprintf(context, kBLLogLevelError,  "Could not set boot device property: %#x\n", kret);
        return 2;        
    }
    
    CFRelease(bootName);
    IOObjectRelease(optionsNode);
        
    return 0;
}

int setefifilepath(BLContextPtr context, const char * path, int bootNext,
				   const char *optionalData, bool shortForm)
{
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    int ret;
    struct statfs sb;
    if(0 != blsustatfs(path, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           path);
        return 1;           
    }
    
    // first try to get booter information for the block device.
    // if there is none, we can do our normal path
    // the given device may be pointing at a RAID
    CFDictionaryRef dict = NULL;
    CFArrayRef array = NULL;
    char    newBSDName[MAXPATHLEN];
    CFStringRef firstBooter = NULL;
    CFStringRef firstData = NULL;
    
    strcpy(newBSDName, sb.f_mntfromname + 5);
    
    ret = BLCreateBooterInformationDictionary(context, newBSDName,
                                              &dict);
    if(ret) {
        return 1;
    }
    
    // check to see if there's a booter partition. If so, use it
    array = CFDictionaryGetValue(dict, kBLAuxiliaryPartitionsKey);
    if(array) {
        if(CFArrayGetCount(array) > 0) {
            firstBooter = CFArrayGetValueAtIndex(array, 0);
            if(!CFStringGetCString(firstBooter, newBSDName, sizeof(newBSDName),
                                   kCFStringEncodingUTF8)) {
                return 1;
            }
            blesscontextprintf(context, kBLLogLevelVerbose, "Substituting booter %s\n", newBSDName);
        }
    }
    
    // check to see if there's a data partition (without auxiliary) that
    // we should boot from
    if (!firstBooter) {
        array = CFDictionaryGetValue(dict, kBLDataPartitionsKey);
        if(array) {
            if(CFArrayGetCount(array) > 0) {
                firstData = CFArrayGetValueAtIndex(array, 0);
                if(!CFStringGetCString(firstData, newBSDName, sizeof(newBSDName),
                                       kCFStringEncodingUTF8)) {
                    return 1;
                }
                if (0 == strcmp(newBSDName, sb.f_mntfromname + 5)) {
                    /* same as before, no substitution */
                    firstData = NULL;
                } else {
                    blesscontextprintf(context, kBLLogLevelVerbose, "Substituting bootable data partition %s\n", newBSDName);
                }
            }
        }
    }
    
    
    if(firstBooter || firstData) {
        // so this is probably a RAID. Validate that we were passed a mountpoint
        if(0 != strncmp(sb.f_mntonname, path, MAXPATHLEN)) {
            blesscontextprintf(context, kBLLogLevelError,  "--file not supported for %s\n" ,
                               sb.f_mntonname);
            return 2;
        }
        ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                    newBSDName,
                                                    optionalData,
                                                    &xmlString,
                                                    shortForm);
        
    } else {
        ret = BLCreateEFIXMLRepresentationForPath(context,
                                                  path,
                                                  optionalData,
                                                  &xmlString, shortForm);
        
    }
    
    CFRelease(dict);

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
    
	ret = efinvramcleanup(context);
	if(ret) return ret;
        
    return 0;
}

int setefilegacypath(BLContextPtr context, const char * path, int bootNext,
                            const char *legacyHint, const char *optionalData)
{
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    int ret;
    struct statfs sb;
    if(0 != blsustatfs(path, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           path);
        return 1;           
    }
    
    if(legacyHint) {
        ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                    legacyHint+5,
                                                    NULL,
                                                    &xmlString,
                                                    false);
        
        if(ret) {
            return 1;
        }
        
        ret = setit(context, kIOMasterPortDefault, "efi-legacy-drive-hint", xmlString);    
        if(ret) return ret;
        
        ret = _forwardNVRAM(context, CFSTR("efi-legacy-drive-hint-data"), CFSTR("BootCampHD"));
        if(ret) return ret;     
        
        ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-legacy-drive-hint"));    
        if(ret) return ret;
        
    }
    
    
    ret = BLCreateEFIXMLRepresentationForLegacyDevice(context,
                                                      sb.f_mntfromname + 5,
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
    
	ret = efinvramcleanup(context);
	if(ret) return ret;
    
    return 0;
}

int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
							 CFStringRef kernelXML, CFStringRef mkextXML,
                             CFStringRef kernelcacheXML, int bootNext)
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

    if(kernelcacheXML) {
		ret = setit(context, kIOMasterPortDefault, "efi-boot-kernelcache", kernelcacheXML);
	} else {
		ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-kernelcache"));
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
    
    CFRelease(newString);
    
    ret = BLPreserveBootArgs(context, cStr, newArgs);
    if(ret) {
        return ret;
    }
        
    newString = CFStringCreateWithCString(kCFAllocatorDefault, newArgs, kCFStringEncodingUTF8);
    if(newString == NULL) {
        return 2;
    }

    ret = setit(context, masterPort, "boot-args", newString);
    CFRelease(newString);
    if(ret)
        return ret;
    
    return 0;
}


static int _forwardNVRAM(BLContextPtr context, CFStringRef from, CFStringRef to) {
    
    io_registry_entry_t optionsNode = 0;
    CFTypeRef       valRef;
    kern_return_t   kret;
    
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
    valRef = IORegistryEntryCreateCFProperty(optionsNode, from, kCFAllocatorDefault, 0);
    
    if(valRef == NULL) {
        contextprintf(context, kBLLogLevelError,  "Could not find variable '%s'\n",
                                                    BLGetCStringDescription(from));
        return 2;
    }
    
    blesscontextprintf(context, kBLLogLevelVerbose,  "Setting EFI NVRAM:\n" );
    blesscontextprintf(context, kBLLogLevelVerbose,  "\t%s='...'\n", BLGetCStringDescription(to) );

    kret = IORegistryEntrySetCFProperty(optionsNode, to, valRef);
    if(kret) {
        CFRelease(valRef);
        IOObjectRelease(optionsNode);
        blesscontextprintf(context, kBLLogLevelError,  "Could not set boot property '%s': %#x\n",
                                                        BLGetCStringDescription(to),
                                                        kret);
        return 3;        
    }

    CFRelease(valRef);    
    IOObjectRelease(optionsNode);

    return 0;
}

int efinvramcleanup(BLContextPtr context)
{
	int ret;

	ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-file"));    
    if(ret) return ret;
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-mkext"));    
    if(ret) return ret;
    
    ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-boot-kernelcache"));    
    if(ret) return ret;
    
    ret = setefibootargs(context, kIOMasterPortDefault);
    if(ret) return ret;
    
    return 0;	
}

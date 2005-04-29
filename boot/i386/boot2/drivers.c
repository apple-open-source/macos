/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  drivers.c - Driver Loading Functions.
 *
 *  Copyright (c) 2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <mach-o/fat.h>
#include <libkern/OSByteOrder.h>
#include <mach/machine.h>

#include "sl.h"
#include "boot.h"
#include "bootstruct.h"
#include "xml.h"

struct Module {  
  struct Module *nextModule;
  long          willLoad;
  TagPtr        dict;
  char          *plistAddr;
  long          plistLength;
  char          *driverPath;
};
typedef struct Module Module, *ModulePtr;

struct DriverInfo {
  char *plistAddr;
  long plistLength;
  void *moduleAddr;
  long moduleLength;
};
typedef struct DriverInfo DriverInfo, *DriverInfoPtr;

#define kDriverPackageSignature1 'MKXT'
#define kDriverPackageSignature2 'MOSX'

struct DriversPackage {
  unsigned long signature1;
  unsigned long signature2;
  unsigned long length;
  unsigned long alder32;
  unsigned long version;
  unsigned long numDrivers;
  unsigned long reserved1;
  unsigned long reserved2;
};
typedef struct DriversPackage DriversPackage;

enum {
  kCFBundleType2,
  kCFBundleType3
};

static unsigned long Alder32( unsigned char * buffer, long length );

static long FileLoadDrivers(char *dirSpec, long plugin);
static long NetLoadDrivers(char *dirSpec);
static long LoadDriverMKext(char *fileSpec);
static long LoadDriverPList(char *dirSpec, char *name, long bundleType);
static long LoadMatchedModules(void);
static long MatchPersonalities(void);
static long MatchLibraries(void);
#ifdef NOTDEF
static ModulePtr FindModule(char *name);
static void ThinFatFile(void **loadAddrP, unsigned long *lengthP);
#endif
static long ParseXML(char *buffer, ModulePtr *module, TagPtr *personalities);
static long InitDriverSupport(void);

static ModulePtr gModuleHead, gModuleTail;
static TagPtr    gPersonalityHead, gPersonalityTail;
static char *    gExtensionsSpec;
static char *    gDriverSpec;
static char *    gFileSpec;
static char *    gTempSpec;
static char *    gFileName;


static unsigned long
Alder32( unsigned char * buffer, long length )
{
    long          cnt;
    unsigned long result, lowHalf, highHalf;
    
    lowHalf  = 1;
    highHalf = 0;
  
	for ( cnt = 0; cnt < length; cnt++ )
    {
        if ((cnt % 5000) == 0)
        {
            lowHalf  %= 65521L;
            highHalf %= 65521L;
        }
    
        lowHalf  += buffer[cnt];
        highHalf += lowHalf;
    }

	lowHalf  %= 65521L;
	highHalf %= 65521L;
  
	result = (highHalf << 16) | lowHalf;
  
	return result;
}


//==========================================================================
// InitDriverSupport

static long
InitDriverSupport( void )
{
    gExtensionsSpec = (char *) malloc( 4096 );
    gDriverSpec     = (char *) malloc( 4096 );
    gFileSpec       = (char *) malloc( 4096 );
    gTempSpec       = (char *) malloc( 4096 );
    gFileName       = (char *) malloc( 4096 );

    if ( !gExtensionsSpec || !gDriverSpec || !gFileSpec || !gTempSpec || !gFileName )
        stop("InitDriverSupport error");

    return 0;
}

//==========================================================================
// LoadDrivers

long LoadDrivers( char * dirSpec )
{
    if ( InitDriverSupport() != 0 )
        return 0;

    if ( gBootFileType == kNetworkDeviceType )
    {
        NetLoadDrivers(dirSpec);
    }
    else if ( gBootFileType == kBlockDeviceType )
    {
        if (gMKextName[0] != '\0')
        {
            verbose("LoadDrivers: Loading from [%s]\n", gMKextName);
            if ( LoadDriverMKext(gMKextName) != 0 )
            {
                error("Could not load %s\n", gMKextName);
                return -1;
            }
        }
        else
        {
            strcpy(gExtensionsSpec, dirSpec);
            strcat(gExtensionsSpec, "System/Library/");
            FileLoadDrivers(gExtensionsSpec, 0);
        }
    }
    else
    {
        return 0;
    }

    MatchPersonalities();

    MatchLibraries();

    LoadMatchedModules();

    return 0;
}

//==========================================================================
// FileLoadDrivers

static long
FileLoadDrivers( char * dirSpec, long plugin )
{
    long         ret, length, index, flags, time, bundleType;
    const char * name;

    if ( !plugin )
    {
        long time2;

        ret = GetFileInfo(dirSpec, "Extensions.mkext", &flags, &time);
        if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeFlat))
        {
            ret = GetFileInfo(dirSpec, "Extensions", &flags, &time2);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeDirectory) ||
                (((gBootMode & kBootModeSafe) == 0) && (time == (time2 + 1))))
            {
                sprintf(gDriverSpec, "%sExtensions.mkext", dirSpec);
                verbose("LoadDrivers: Loading from [%s]\n", gDriverSpec);
                if (LoadDriverMKext(gDriverSpec) == 0) return 0;
            }
        }

        strcat(dirSpec, "Extensions");
    }

    verbose("LoadDrivers: Loading from [%s]\n", dirSpec);

    index = 0;
    while (1) {
        ret = GetDirEntry(dirSpec, &index, &name, &flags, &time);
        if (ret == -1) break;

        // Make sure this is a directory.
        if ((flags & kFileTypeMask) != kFileTypeDirectory) continue;
        
        // Make sure this is a kext.
        length = strlen(name);
        if (strcmp(name + length - 5, ".kext")) continue;

        // Save the file name.
        strcpy(gFileName, name);
    
        // Determine the bundle type.
        sprintf(gTempSpec, "%s/%s", dirSpec, gFileName);
        ret = GetFileInfo(gTempSpec, "Contents", &flags, &time);
        if (ret == 0) bundleType = kCFBundleType2;
        else bundleType = kCFBundleType3;

        if (!plugin)
            sprintf(gDriverSpec, "%s/%s/%sPlugIns", dirSpec, gFileName,
                    (bundleType == kCFBundleType2) ? "Contents/" : "");

        ret = LoadDriverPList(dirSpec, gFileName, bundleType);
        if (ret != 0)
        {
            //printf("LoadDrivers: failed for '%s'/'%s'\n", dirSpec, gFileName);
        }

        if (!plugin) 
            ret = FileLoadDrivers(gDriverSpec, 1);
    }

    return 0;
}

//==========================================================================
// 

static long
NetLoadDrivers( char * dirSpec )
{
    long tries;

#if NODEF
    long cnt;

    // Get the name of the kernel
    cnt = strlen(gBootFile);
    while (cnt--) {
        if ((gBootFile[cnt] == '\\')  || (gBootFile[cnt] == ',')) {
        cnt++;
        break;
        }
    }
#endif

    // INTEL modification
    sprintf(gDriverSpec, "%s%s.mkext", dirSpec, bootArgs->bootFile);
    
    verbose("NetLoadDrivers: Loading from [%s]\n", gDriverSpec);
    
    tries = 3;
    while (tries--)
    {
        if (LoadDriverMKext(gDriverSpec) == 0) break;
    }
    if (tries == -1) return -1;

    return 0;
}

//==========================================================================
// loadDriverMKext

static long
LoadDriverMKext( char * fileSpec )
{
    unsigned long    driversAddr, driversLength;
    long             length;
    char             segName[32];
    DriversPackage * package = (DriversPackage *)kLoadAddr;

#define GetPackageElement(e)     OSSwapBigToHostInt32(package->e)

    // Load the MKext.
    length = LoadFile(fileSpec);
    if (length == -1) return -1;

    ThinFatFile((void **)&package, &length);

    // Verify the MKext.
    if (( GetPackageElement(signature1) != kDriverPackageSignature1) ||
        ( GetPackageElement(signature2) != kDriverPackageSignature2) ||
        ( GetPackageElement(length)      > kLoadSize )               ||
        ( GetPackageElement(alder32)    !=
          Alder32((char *)&package->version, GetPackageElement(length) - 0x10) ) )
    {
        return -1;
    }

    // Make space for the MKext.
    driversLength = GetPackageElement(length);
    driversAddr   = AllocateKernelMemory(driversLength);

    // Copy the MKext.
    memcpy((void *)driversAddr, (void *)package, driversLength);

    // Add the MKext to the memory map.
    sprintf(segName, "DriversPackage-%lx", driversAddr);
    AllocateMemoryRange(segName, driversAddr, driversLength,
                        kBootDriverTypeMKEXT);

    return 0;
}

//==========================================================================
// LoadDriverPList

static long
LoadDriverPList( char * dirSpec, char * name, long bundleType )
{
    long      length, driverPathLength;
    ModulePtr module;
    TagPtr    personalities;
    char *    buffer = 0;
    char *    tmpDriverPath = 0;
    long      ret = -1;

    do {
        // Save the driver path.
        
        sprintf(gFileSpec, "%s/%s/%s", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/MacOS/" : "");
        driverPathLength = strlen(gFileSpec) + 1;

        tmpDriverPath = malloc(driverPathLength);
        if (tmpDriverPath == 0) break;

        strcpy(tmpDriverPath, gFileSpec);
  
        // Construct the file spec to the plist, then load it.

        sprintf(gFileSpec, "%s/%s/%sInfo.plist", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/" : "");

        length = LoadFile(gFileSpec);
        if (length == -1) break;

        length = length + 1;
        buffer = malloc(length);
        if (buffer == 0) break;

        strlcpy(buffer, (char *)kLoadAddr, length);

        // Parse the plist.

        ret = ParseXML(buffer, &module, &personalities);
        if (ret != 0) { break; }

        // Allocate memory for the driver path and the plist.

        module->driverPath = tmpDriverPath;
        module->plistAddr = (void *)malloc(length);
  
        if ((module->driverPath == 0) || (module->plistAddr == 0))
            break;

        // Save the driver path in the module.
        //strcpy(module->driverPath, tmpDriverPath);
        tmpDriverPath = 0;

        // Add the plist to the module.

        strlcpy(module->plistAddr, (char *)kLoadAddr, length);
        module->plistLength = length;
  
        // Add the module to the end of the module list.
        
        if (gModuleHead == 0)
            gModuleHead = module;
        else
            gModuleTail->nextModule = module;
        gModuleTail = module;
  
        // Add the persionalities to the personality list.
    
        if (personalities) personalities = personalities->tag;
        while (personalities != 0)
        {
            if (gPersonalityHead == 0)
                gPersonalityHead = personalities->tag;
            else
                gPersonalityTail->tagNext = personalities->tag;
            
            gPersonalityTail = personalities->tag;
            personalities = personalities->tagNext;
        }
        
        ret = 0;
    }
    while (0);
    
    if ( buffer )        free( buffer );
    if ( tmpDriverPath ) free( tmpDriverPath );

    return ret;
}

#if 0
//==========================================================================
// ThinFatFile
// Checks the loaded file for a fat header; if present, updates
// loadAddr and length to be the portion of the fat file relevant
// to the current architecture; otherwise leaves them unchanged.

static void
ThinFatFile(void **loadAddrP, unsigned long *lengthP)
{
    // Check for fat files.
    struct fat_header *fhp = (struct fat_header *)kLoadAddr;
    struct fat_arch *fap = (struct fat_arch *)((void *)kLoadAddr +
					       sizeof(struct fat_header));
    int nfat, swapped;
    void *loadAddr = 0;
    unsigned long length = 0;

    if (fhp->magic == FAT_MAGIC) {
	nfat = fhp->nfat_arch;
	swapped = 0;
    } else if (fhp->magic == FAT_CIGAM) {
	nfat = OSSwapInt32(fhp->nfat_arch);
	swapped = 1;
    } else {
	nfat = 0;
	swapped = 0;
    }

    for (; nfat > 0; nfat--, fap++) {
	if (swapped) {
	    fap->cputype = OSSwapInt32(fap->cputype);
	    fap->offset = OSSwapInt32(fap->offset);
	    fap->size = OSSwapInt32(fap->size);
	}
	if (fap->cputype == CPU_TYPE_I386) {
	    loadAddr = (void *)kLoadAddr + fap->offset;
	    length = fap->size;
	    break;
	}
    }
    if (loadAddr)
	*loadAddrP = loadAddr;
    if (length)
	*lengthP = length;
}
#endif

//==========================================================================
// LoadMatchedModules

static long
LoadMatchedModules( void )
{
    TagPtr        prop;
    ModulePtr     module;
    char          *fileName, segName[32];
    DriverInfoPtr driver;
    long          length, driverAddr, driverLength;
  
    module = gModuleHead;

    while (module != 0)
    {
        if (module->willLoad)
        {
            prop = XMLGetProperty(module->dict, kPropCFBundleExecutable);

            if (prop != 0)
            {
                fileName = prop->string;
                sprintf(gFileSpec, "%s%s", module->driverPath, fileName);
                length = LoadFile(gFileSpec);
            }
            else
                length = 0;

            if (length != -1)
            {
		void *driverModuleAddr = (void *)kLoadAddr;
                if (length != 0)
                {
		    ThinFatFile(&driverModuleAddr, &length);
		}

                // Make make in the image area.
                driverLength = sizeof(DriverInfo) + module->plistLength + length;
                driverAddr = AllocateKernelMemory(driverLength);

                // Set up the DriverInfo.
                driver = (DriverInfoPtr)driverAddr;
                driver->plistAddr = (char *)(driverAddr + sizeof(DriverInfo));
                driver->plistLength = module->plistLength;
                if (length != 0)
                {
                    driver->moduleAddr = (void *)(driverAddr + sizeof(DriverInfo) +
					                     module->plistLength);
                    driver->moduleLength = length;
                }
                else
                {
                    driver->moduleAddr   = 0;
                    driver->moduleLength = 0;
                }

                // Save the plist and module.
                strcpy(driver->plistAddr, module->plistAddr);
                if (length != 0)
                {
                    memcpy(driver->moduleAddr, driverModuleAddr, length);
                }

                // Add an entry to the memory map.
                sprintf(segName, "Driver-%lx", (unsigned long)driver);
                AllocateMemoryRange(segName, driverAddr, driverLength,
                                    kBootDriverTypeKEXT);
            }
        }
        module = module->nextModule;
    }

    return 0;
}

//==========================================================================
// MatchPersonalities

static long
MatchPersonalities( void )
{
    /* IONameMatch support not implemented */
    return 0;
}

//==========================================================================
// MatchLibraries

static long
MatchLibraries( void )
{
    TagPtr     prop, prop2;
    ModulePtr  module, module2;
    long       done;

    do {
        done = 1;
        module = gModuleHead;
        
        while (module != 0)
        {
            if (module->willLoad == 1)
            {
                prop = XMLGetProperty(module->dict, kPropOSBundleLibraries);
                if (prop != 0)
                {
                    prop = prop->tag;
                    while (prop != 0)
                    {
                        module2 = gModuleHead;
                        while (module2 != 0)
                        {
                            prop2 = XMLGetProperty(module2->dict, kPropCFBundleIdentifier);
                            if ((prop2 != 0) && (!strcmp(prop->string, prop2->string)))
                            {
                                if (module2->willLoad == 0) module2->willLoad = 1;
                                break;
                            }
                            module2 = module2->nextModule;
                        }
                        prop = prop->tagNext;
                    }
                }
                module->willLoad = 2;
                done = 0;
            }
            module = module->nextModule;
        }
    }
    while (!done);

    return 0;
}


//==========================================================================
// FindModule

#if NOTDEF
static ModulePtr
FindModule( char * name )
{
    ModulePtr module;
    TagPtr    prop;
    
    module = gModuleHead;
    
    while (module != 0)
    {
        prop = GetProperty(module->dict, kPropCFBundleIdentifier);
        if ((prop != 0) && !strcmp(name, prop->string)) break;
        module = module->nextModule;
    }
    
    return module;
}
#endif /* NOTDEF */

//==========================================================================
// ParseXML

static long
ParseXML( char * buffer, ModulePtr * module, TagPtr * personalities )
{
	long       length, pos;
	TagPtr     moduleDict, required;
	ModulePtr  tmpModule;
  
    pos = 0;
  
    while (1)
    {
        length = XMLParseNextTag(buffer + pos, &moduleDict);
        if (length == -1) break;
    
        pos += length;
    
        if (moduleDict == 0) continue;
        if (moduleDict->type == kTagTypeDict) break;
    
        XMLFreeTag(moduleDict);
    }
  
    if (length == -1) return -1;

    required = XMLGetProperty(moduleDict, kPropOSBundleRequired);
    if ( (required == 0) ||
         (required->type != kTagTypeString) ||
         !strcmp(required->string, "Safe Boot"))
    {
        XMLFreeTag(moduleDict);
        return -2;
    }

    tmpModule = (ModulePtr)malloc(sizeof(Module));
    if (tmpModule == 0)
    {
        XMLFreeTag(moduleDict);
        return -1;
    }
    tmpModule->dict = moduleDict;
  
    // For now, load any module that has OSBundleRequired != "Safe Boot".

    tmpModule->willLoad = 1;

    *module = tmpModule;
  
    // Get the personalities.

    *personalities = XMLGetProperty(moduleDict, kPropIOKitPersonalities);
  
    return 0;
}

#if NOTDEF
static char gPlatformName[64];
#endif

long 
DecodeKernel(void *binary, entry_t *rentry, char **raddr, int *rsize)
{
    long ret;
    compressed_kernel_header * kernel_header = (compressed_kernel_header *) binary;
    u_int32_t uncompressed_size, size;
    void *buffer;
  
#if 0
    printf("kernel header:\n");
    printf("signature: 0x%x\n", kernel_header->signature);
    printf("compress_type: 0x%x\n", kernel_header->compress_type);
    printf("adler32: 0x%x\n", kernel_header->adler32);
    printf("uncompressed_size: 0x%x\n", kernel_header->uncompressed_size);
    printf("compressed_size: 0x%x\n", kernel_header->compressed_size);
    getc();
#endif

    if (kernel_header->signature == OSSwapBigToHostConstInt32('comp')) {
        if (kernel_header->compress_type != OSSwapBigToHostConstInt32('lzss')) {
            error("kernel compression is bad\n");
            return -1;
        }
#if NOTDEF
        if (kernel_header->platform_name[0] && strcmp(gPlatformName, kernel_header->platform_name))
            return -1;
        if (kernel_header->root_path[0] && strcmp(gBootFile, kernel_header->root_path))
            return -1;
#endif
    
        uncompressed_size = OSSwapBigToHostInt32(kernel_header->uncompressed_size);
        binary = buffer = malloc(uncompressed_size);
    
        size = decompress_lzss((u_int8_t *) binary, &kernel_header->data[0],
                               OSSwapBigToHostInt32(kernel_header->compressed_size));
        if (uncompressed_size != size) {
            error("size mismatch from lzss: %x\n", size);
            return -1;
        }
        if (OSSwapBigToHostInt32(kernel_header->adler32) !=
            Alder32(binary, uncompressed_size)) {
            printf("adler mismatch\n");
            return -1;
        }
    }
  
  ThinFatFile(&binary, 0);
  
  ret = DecodeMachO(binary, rentry, raddr, rsize);
  
  return ret;
}

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
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
 * under the License."
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

#include "sl.h"
#include "boot.h"

enum {
  kTagTypeNone = 0,
  kTagTypeDict,
  kTagTypeKey,
  kTagTypeString,
  kTagTypeInteger,
  kTagTypeData,
  kTagTypeDate,
  kTagTypeFalse,
  kTagTypeTrue,
  kTagTypeArray
};

#define kXMLTagPList   "plist "
#define kXMLTagDict    "dict"
#define kXMLTagKey     "key"
#define kXMLTagString  "string"
#define kXMLTagInteger "integer"
#define kXMLTagData    "data"
#define kXMLTagDate    "date"
#define kXMLTagFalse   "false/"
#define kXMLTagTrue    "true/"
#define kXMLTagArray   "array"

#define kPropCFBundleIdentifier ("CFBundleIdentifier")
#define kPropCFBundleExecutable ("CFBundleExecutable")
#define kPropOSBundleRequired   ("OSBundleRequired")
#define kPropOSBundleLibraries  ("OSBundleLibraries")
#define kPropIOKitPersonalities ("IOKitPersonalities")
#define kPropIONameMatch        ("IONameMatch")

struct Tag {
  long       type;
  char       *string;
  struct Tag *tag;
  struct Tag *tagNext;
};
typedef struct Tag Tag, *TagPtr;

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

static long FileLoadDrivers(char *dirSpec, long plugin);
static long NetLoadDrivers(char *dirSpec);
static long LoadDriverMKext(char *fileSpec);
static long LoadDriverPList(char *dirSpec, char *name, long bundleType);
static long LoadMatchedModules(void);
static long MatchPersonalities(void);
static long MatchLibraries(void);
static TagPtr GetProperty(TagPtr dict, char *key);
// static ModulePtr FindModule(char *name);
static long ParseXML(char *buffer, ModulePtr *module, TagPtr *personalities);
static long ParseNextTag(char *buffer, TagPtr *tag);
static long ParseTagList(char *buffer, TagPtr *tag, long type, long empty);
static long ParseTagKey(char *buffer, TagPtr *tag);
static long ParseTagString(char *buffer, TagPtr *tag);
static long ParseTagInteger(char *buffer, TagPtr *tag);
static long ParseTagData(char *buffer, TagPtr *tag);
static long ParseTagDate(char *buffer, TagPtr *tag);
static long ParseTagBoolean(char *buffer, TagPtr *tag, long type);
static long GetNextTag(char *buffer, char **tag, long *start);
static long FixDataMatchingTag(char *buffer, char *tag);
static TagPtr NewTag(void);
static void FreeTag(TagPtr tag);
static char *NewSymbol(char *string);
static void FreeSymbol(char *string);
// static void DumpTag(TagPtr tag, long depth);

static ModulePtr gModuleHead, gModuleTail;
static TagPtr    gPersonalityHead, gPersonalityTail;
static char *    gExtensionsSpec;
static char *    gDriverSpec;
static char *    gFileSpec;
static char *    gTempSpec;
static char *    gFileName;

//==========================================================================
// BootX shim functions.

#define kPageSize     4096
#define RoundPage(x)  ((((unsigned)(x)) + kPageSize - 1) & ~(kPageSize - 1))

static long  gImageFirstBootXAddr;
static long  gImageLastKernelAddr;

static void *
AllocateBootXMemory( long size )
{
	long addr = gImageFirstBootXAddr - size;
  
	if ( addr < gImageLastKernelAddr ) return 0;

    bzero((void *)addr, size);

	gImageFirstBootXAddr = addr;
  
	return (void *)addr;
}

static long
AllocateKernelMemory( long inSize )
{
	long addr = gImageLastKernelAddr;

    gImageLastKernelAddr += RoundPage(inSize);

    if ( gImageLastKernelAddr > gImageFirstBootXAddr )
        stop( "AllocateKernelMemory error" );
        
    kernBootStruct->ksize = gImageLastKernelAddr - kernBootStruct->kaddr;

    return addr;
}

static long
AllocateMemoryRange(char * rangeName, long start, long length, long type)
{
    if ( kernBootStruct->numBootDrivers < NDRIVERS )
    {
        int num = kernBootStruct->numBootDrivers;

        kernBootStruct->driverConfig[num].address = start;
        kernBootStruct->driverConfig[num].size    = length;
        kernBootStruct->driverConfig[num].type    = type;
        kernBootStruct->numBootDrivers++;
    }
    else
    {
        stop( "AllocateMemoryRange error" );
    }
    return 0;
}

// Map BootX types to boot counterparts.

#define gBootFileType    BIOS_DEV_TYPE(gBIOSDev)
enum {
    kNetworkDeviceType = kBIOSDevTypeNetwork,
    kBlockDeviceType   = kBIOSDevTypeHardDrive
};


static long
InitDriverSupport()
{
    gExtensionsSpec = (char *) malloc( 4096 );
    gDriverSpec     = (char *) malloc( 4096 );
    gFileSpec       = (char *) malloc( 4096 );
    gTempSpec       = (char *) malloc( 4096 );
    gFileName       = (char *) malloc( 4096 );

    if ( !gExtensionsSpec || !gDriverSpec || !gFileSpec || !gTempSpec || !gFileName )
        stop("InitDriverSupport error");

    gImageLastKernelAddr = RoundPage( kernBootStruct->kaddr +
                                      kernBootStruct->ksize );

    gImageFirstBootXAddr = ( KERNEL_ADDR + KERNEL_LEN );

    return 0;
}

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
// LoadDrivers

long LoadDrivers( char * dirSpec )
{
    if ( InitDriverSupport() != 0 )
        return 0;

    if ( gBootFileType == kNetworkDeviceType )
    {
        NetLoadDrivers(dirSpec);
    }
    else /* if ( gBootFileType == kBlockDeviceType ) */
    {
        strcpy(gExtensionsSpec, dirSpec);
        strcat(gExtensionsSpec, "System/Library/");
        FileLoadDrivers(gExtensionsSpec, 0);
    }
#if 0
    else
    {
        return 0;
    }
#endif

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
                (((gBootMode & kBootModeSafe) == 0) && (time > time2)))
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
            // printf("LoadDrivers: failed\n");
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

#if 0
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
    sprintf(gDriverSpec, "%s%s.mkext", dirSpec, kernBootStruct->bootFile);
    
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
    long             driversAddr, driversLength;
    char             segName[32];
    DriversPackage * package = (DriversPackage *)kLoadAddr;

#define GetPackageElement(e)     NXSwapBigLongToHost(package->e)

    // Load the MKext.
    if (LoadFile(fileSpec) == -1) return -1;

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
    memcpy((void *)driversAddr, (void *)kLoadAddr, driversLength);

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
    char *	  tmpDriverPath = 0;
    long      ret = -1;

    do {
        // Save the driver path.
        
        sprintf(gFileSpec, "%s/%s/%s", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/MacOS/" : "");
        driverPathLength = strlen(gFileSpec);

        tmpDriverPath = malloc(driverPathLength + 1);
        if (tmpDriverPath == 0) break;

        strcpy(tmpDriverPath, gFileSpec);
  
        // Construct the file spec to the plist, then load it.

        sprintf(gFileSpec, "%s/%s/%sInfo.plist", dirSpec, name,
                (bundleType == kCFBundleType2) ? "Contents/" : "");

        length = LoadFile(gFileSpec);
        if (length == -1) break;

        buffer = malloc(length + 1);
        if (buffer == 0) break;

        strncpy(buffer, (char *)kLoadAddr, length);

        // Parse the plist.

        ret = ParseXML(buffer, &module, &personalities);
        if (ret != 0) break;

        // Allocate memory for the driver path and the plist.

        module->driverPath = AllocateBootXMemory(driverPathLength + 1);
        module->plistAddr  = AllocateBootXMemory(length + 1);
  
        if ((module->driverPath == 0) || (module->plistAddr == 0))
            break;

        // Save the driver path in the module.

        strcpy(module->driverPath, tmpDriverPath);

        // Add the plist to the module.

        strncpy(module->plistAddr, (char *)kLoadAddr, length);
        module->plistLength = length + 1;
  
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
            prop = GetProperty(module->dict, kPropCFBundleExecutable);
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
                    memcpy(driver->moduleAddr, (void *)kLoadAddr, driver->moduleLength);
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
#warning IONameMatch support not implemented
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
                prop = GetProperty(module->dict, kPropOSBundleLibraries);
                if (prop != 0)
                {
                    prop = prop->tag;
                    while (prop != 0)
                    {
                        module2 = gModuleHead;
                        while (module2 != 0)
                        {
                            prop2 = GetProperty(module2->dict, kPropCFBundleIdentifier);
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
// GetProperty

static TagPtr
GetProperty( TagPtr dict, char * key )
{
    TagPtr tagList, tag;

    if (dict->type != kTagTypeDict) return 0;
    
    tag = 0;
    tagList = dict->tag;
    while (tagList)
    {
        tag = tagList;
        tagList = tag->tagNext;
        
        if ((tag->type != kTagTypeKey) || (tag->string == 0)) continue;
        
        if (!strcmp(tag->string, key)) return tag->tag;
    }
    
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
        length = ParseNextTag(buffer + pos, &moduleDict);
        if (length == -1) break;
    
        pos += length;
    
        if (moduleDict == 0) continue;
        if (moduleDict->type == kTagTypeDict) break;
    
        FreeTag(moduleDict);
    }
  
    if (length == -1) return -1;

    required = GetProperty(moduleDict, kPropOSBundleRequired);
    if ( (required == 0) ||
         (required->type != kTagTypeString) ||
         !strcmp(required->string, "Safe Boot"))
    {
        FreeTag(moduleDict);
        return -2;
    }

    tmpModule = AllocateBootXMemory(sizeof(Module));
    if (tmpModule == 0)
    {
        FreeTag(moduleDict);
        return -1;
    }
    tmpModule->dict = moduleDict;
  
    // For now, load any module that has OSBundleRequired != "Safe Boot".

    tmpModule->willLoad = 1;

    *module = tmpModule;
  
    // Get the personalities.

    *personalities = GetProperty(moduleDict, kPropIOKitPersonalities);
  
    return 0;
}

//==========================================================================
// ParseNextTag

static long
ParseNextTag( char * buffer, TagPtr * tag )
{
	long   length, pos;
	char * tagName;

    length = GetNextTag(buffer, &tagName, 0);
    if (length == -1) return -1;

	pos = length;
    if (!strncmp(tagName, kXMLTagPList, 6))
    {
        length = 0;
    }
    else if (!strcmp(tagName, kXMLTagDict))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeDict, 0);
    }
    else if (!strcmp(tagName, kXMLTagDict "/"))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeDict, 1);
    }
    else if (!strcmp(tagName, kXMLTagKey))
    {
        length = ParseTagKey(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagString))
    {
        length = ParseTagString(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagInteger))
    {
        length = ParseTagInteger(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagData))
    {
        length = ParseTagData(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagDate))
    {
        length = ParseTagDate(buffer + pos, tag);
    }
    else if (!strcmp(tagName, kXMLTagFalse))
    {
        length = ParseTagBoolean(buffer + pos, tag, kTagTypeFalse);
    }
    else if (!strcmp(tagName, kXMLTagTrue))
    {
        length = ParseTagBoolean(buffer + pos, tag, kTagTypeTrue);
    }
    else if (!strcmp(tagName, kXMLTagArray))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeArray, 0);
    }
    else if (!strcmp(tagName, kXMLTagArray "/"))
    {
        length = ParseTagList(buffer + pos, tag, kTagTypeArray, 1);
    }
    else
    {
        *tag = 0;
        length = 0;
    }
  
    if (length == -1) return -1;
  
    return pos + length;
}

//==========================================================================
// ParseTagList

static long
ParseTagList( char * buffer, TagPtr * tag, long type, long empty )
{
	long   length, pos;
	TagPtr tagList, tmpTag;
  
    tagList = 0;
    pos = 0;
  
    if (!empty)
    {
        while (1)
        {
            length = ParseNextTag(buffer + pos, &tmpTag);
            if (length == -1) break;

            pos += length;
      
            if (tmpTag == 0) break;
            tmpTag->tagNext = tagList;
            tagList = tmpTag;
        }
    
        if (length == -1)
        {
            FreeTag(tagList);
            return -1;
        }
    }
  
    tmpTag = NewTag();
    if (tmpTag == 0)
    {
        FreeTag(tagList);
        return -1;
    }

    tmpTag->type = type;
    tmpTag->string = 0;
    tmpTag->tag = tagList;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return pos;
}

//==========================================================================
// ParseTagKey

static long
ParseTagKey( char * buffer, TagPtr * tag )
{
    long   length, length2;
    char   *string;
    TagPtr tmpTag, subTag;
  
    length = FixDataMatchingTag(buffer, kXMLTagKey);
    if (length == -1) return -1;
  
    length2 = ParseNextTag(buffer + length, &subTag);
    if (length2 == -1) return -1;
  
    tmpTag = NewTag();
    if (tmpTag == 0)
    {
        FreeTag(subTag);
        return -1;
    }
  
    string = NewSymbol(buffer);
    if (string == 0)
    {
        FreeTag(subTag);
        FreeTag(tmpTag);
        return -1;
    }
  
	tmpTag->type = kTagTypeKey;
    tmpTag->string = string;
    tmpTag->tag = subTag;
    tmpTag->tagNext = 0;
  
    *tag = tmpTag;
  
    return length + length2;
}

//==========================================================================
// ParseTagString

static long
ParseTagString( char * buffer, TagPtr * tag )
{
    long   length;
    char * string;
    TagPtr tmpTag;
  
	length = FixDataMatchingTag(buffer, kXMLTagString);
    if (length == -1) return -1;
  
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
  
    string = NewSymbol(buffer);
    if (string == 0)
    {
        FreeTag(tmpTag);
        return -1;
    }
  
    tmpTag->type = kTagTypeString;
    tmpTag->string = string;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
  
    *tag = tmpTag;
  
    return length;
}

//==========================================================================
// ParseTagInteger

static long
ParseTagInteger( char * buffer, TagPtr * tag )
{
    long   length, integer;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagInteger);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    integer = 0;
    
    tmpTag->type = kTagTypeInteger;
    tmpTag->string = (char *)integer;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagData

static long
ParseTagData( char * buffer, TagPtr * tag )
{
    long   length;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagData);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = kTagTypeData;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagDate

static long
ParseTagDate( char * buffer, TagPtr * tag )
{
    long   length;
    TagPtr tmpTag;
    
    length = FixDataMatchingTag(buffer, kXMLTagDate);
    if (length == -1) return -1;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = kTagTypeDate;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return length;
}

//==========================================================================
// ParseTagBoolean

static long
ParseTagBoolean( char * buffer, TagPtr * tag, long type )
{
    TagPtr tmpTag;
    
    tmpTag = NewTag();
    if (tmpTag == 0) return -1;
    
    tmpTag->type = type;
    tmpTag->string = 0;
    tmpTag->tag = 0;
    tmpTag->tagNext = 0;
    
    *tag = tmpTag;
    
    return 0;
}

//==========================================================================
// GetNextTag

static long
GetNextTag( char * buffer, char ** tag, long * start )
{
    long cnt, cnt2;

    if (tag == 0) return -1;
    
    // Find the start of the tag.
    cnt = 0;
    while ((buffer[cnt] != '\0') && (buffer[cnt] != '<')) cnt++;
    if (buffer[cnt] == '\0') return -1;
    
    // Find the end of the tag.
    cnt2 = cnt + 1;
    while ((buffer[cnt2] != '\0') && (buffer[cnt2] != '>')) cnt2++;
    if (buffer[cnt2] == '\0') return -1;

    // Fix the tag data.
    *tag = buffer + cnt + 1;
    buffer[cnt2] = '\0';
    if (start) *start = cnt;
    
    return cnt2 + 1;
}

//==========================================================================
// FixDataMatchingTag

static long
FixDataMatchingTag( char * buffer, char * tag )
{
    long   length, start, stop;
    char * endTag;
    
    start = 0;
    while (1)
    {
        length = GetNextTag(buffer + start, &endTag, &stop);
        if (length == -1) return -1;
        
        if ((*endTag == '/') && !strcmp(endTag + 1, tag)) break;
        start += length;
    }
    
    buffer[start + stop] = '\0';
    
    return start + length;
}

//==========================================================================
// NewTag

#define kTagsPerBlock (0x1000)

static TagPtr gTagsFree;

static TagPtr
NewTag( void )
{
	long   cnt;
	TagPtr tag;
  
    if (gTagsFree == 0)
    {
        tag = (TagPtr)AllocateBootXMemory(kTagsPerBlock * sizeof(Tag));
        if (tag == 0) return 0;
        
        // Initalize the new tags.
        for (cnt = 0; cnt < kTagsPerBlock; cnt++)
        {
            tag[cnt].type = kTagTypeNone;
            tag[cnt].string = 0;
            tag[cnt].tag = 0;
            tag[cnt].tagNext = tag + cnt + 1;
        }
        tag[kTagsPerBlock - 1].tagNext = 0;

        gTagsFree = tag;
    }

    tag = gTagsFree;
    gTagsFree = tag->tagNext;
    
    return tag;
}

//==========================================================================
// FreeTag

static void
FreeTag( TagPtr tag )
{
	return;
    if (tag == 0) return;
  
    if (tag->string) FreeSymbol(tag->string);
  
    FreeTag(tag->tag);
    FreeTag(tag->tagNext);
  
    // Clear and free the tag.
    tag->type = kTagTypeNone;
    tag->string = 0;
    tag->tag = 0;
    tag->tagNext = gTagsFree;
    gTagsFree = tag;
}

//==========================================================================
// Symbol object.

struct Symbol
{
  long          refCount;
  struct Symbol *next;
  char          string[1];
};
typedef struct Symbol Symbol, *SymbolPtr;

static SymbolPtr FindSymbol(char * string, SymbolPtr * prevSymbol);

static SymbolPtr gSymbolsHead;

//==========================================================================
// NewSymbol

static char *
NewSymbol( char * string )
{
	SymbolPtr symbol;
  
    // Look for string in the list of symbols.
    symbol = FindSymbol(string, 0);
  
    // Add the new symbol.
    if (symbol == 0)
    {
        symbol = AllocateBootXMemory(sizeof(Symbol) + strlen(string));
        if (symbol == 0) return 0;
    
        // Set the symbol's data.
        symbol->refCount = 0;
        strcpy(symbol->string, string);
    
        // Add the symbol to the list.
        symbol->next = gSymbolsHead;
        gSymbolsHead = symbol;
    }
  
    // Update the refCount and return the string.
    symbol->refCount++;
    return symbol->string;
}

//==========================================================================
// FreeSymbol

static void
FreeSymbol( char * string )
{ 
#if 0
    SymbolPtr symbol, prev;
    
    // Look for string in the list of symbols.
    symbol = FindSymbol(string, &prev);
    if (symbol == 0) return;
    
    // Update the refCount.
    symbol->refCount--;
    
    if (symbol->refCount != 0) return;
    
    // Remove the symbol from the list.
    if (prev != 0) prev->next = symbol->next;
    else gSymbolsHead = symbol->next;
    
    // Free the symbol's memory.
    free(symbol);
#endif
}

//==========================================================================
// FindSymbol

static SymbolPtr
FindSymbol( char * string, SymbolPtr * prevSymbol )
{
	SymbolPtr symbol, prev;
  
	symbol = gSymbolsHead;
	prev = 0;
  
	while (symbol != 0)
    {
        if (!strcmp(symbol->string, string)) break;
    
        prev = symbol;
        symbol = symbol->next;
    }
  
    if ((symbol != 0) && (prevSymbol != 0)) *prevSymbol = prev;
  
    return symbol;
}

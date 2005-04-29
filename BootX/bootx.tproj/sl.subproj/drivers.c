/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
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

#include <sl.h>

#define DRIVER_DEBUG 0

#define kPropCFBundleIdentifier ("CFBundleIdentifier")
#define kPropCFBundleExecutable ("CFBundleExecutable")
#define kPropOSBundleRequired   ("OSBundleRequired")
#define kPropOSBundleLibraries  ("OSBundleLibraries")
#define kPropIOKitPersonalities ("IOKitPersonalities")
#define kPropIONameMatch        ("IONameMatch")

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
  unsigned long adler32;
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
static ModulePtr FindModule(char *name);
static long XML2Module(char *buffer, ModulePtr *module, TagPtr *personalities);

static ModulePtr gModuleHead, gModuleTail;
static TagPtr    gPersonalityHead, gPersonalityTail;
static char      gDriverSpec[4096];
static char      gFileSpec[4096];
static char      gTempSpec[4096];
static char      gFileName[4096];

// Public Functions

long LoadDrivers(char *dirSpec)
{
  if (gBootFileType == kNetworkDeviceType) {
    NetLoadDrivers(dirSpec);
  } else if (gBootFileType == kBlockDeviceType) {
    FileLoadDrivers(dirSpec, 0);
  } else {
    return 0;
  }
  
  MatchPersonalities();
  
  MatchLibraries();
  
  LoadMatchedModules();
  
  return 0;
}

// Private Functions

static long FileLoadDrivers(char *dirSpec, long plugin)
{
  long ret, length, index, flags, time, time2, bundleType;
  char *name;
  
  if (!plugin) {
    ret = GetFileInfo(dirSpec, "Extensions.mkext", &flags, &time);
    if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeFlat)) {
      ret = GetFileInfo(dirSpec, "Extensions", &flags, &time2);
      // use mkext if if it looks right or if the folder was bad
      if ((ret != 0) ||
	  ((flags & kFileTypeMask) != kFileTypeDirectory) ||
	  (((gBootMode & kBootModeSafe) == 0) && (time == (time2 + 1)))) {
	sprintf(gDriverSpec, "%sExtensions.mkext", dirSpec);
	printf("FileLoadDrivers: Loading from [%s]\n", gDriverSpec);
	if (LoadDriverMKext(gDriverSpec) == 0) return 0;
      } else if(time != (time2 + 1)) {
	  printf("mkext timestamp isn't quite right (delta: %d); ignoring...\n",
	      time2 - time);
	}
    }
    
    strcat(dirSpec, "Extensions");
  }
  
  printf("FileLoadDrivers: Loading from [%s]\n", dirSpec);
  
  index = 0;
  while (1) {
    ret = GetDirEntry(dirSpec, &index, &name, &flags, &time);
    if (ret == -1) break;

    // Make sure this is a directory.
    if ((flags & kFileTypeMask ) != kFileTypeDirectory) continue;
    
    // Make sure this is a kext.
    length = strlen(name);
    if (strcmp(name + length - 5, ".kext")) continue;
    
    // Save the file name.
    strcpy(gFileName, name);
    
    // Determine the bundle type.
    sprintf(gTempSpec, "%s\\%s", dirSpec, gFileName);
    ret = GetFileInfo(gTempSpec, "Contents", &flags, &time);
    if (ret == 0) bundleType = kCFBundleType2;
    else bundleType = kCFBundleType3;
    
    if (!plugin) {
      sprintf(gDriverSpec, "%s\\%s\\%sPlugIns", dirSpec, gFileName,
	      (bundleType == kCFBundleType2) ? "Contents\\" : "");
    }
    
    ret = LoadDriverPList(dirSpec, gFileName, bundleType);
    
    if (!plugin) {
      ret = FileLoadDrivers(gDriverSpec, 1);
    }
  }
  
  return 0;
}


static long NetLoadDrivers(char *dirSpec)
{
  long tries, cnt;
  
  // Get the name of the kernel
  cnt = strlen(gBootFile);
  while (cnt--) {
    if ((gBootFile[cnt] == '\\')  || (gBootFile[cnt] == ',')) {
      cnt++;
      break;
    }
  }
  
  sprintf(gDriverSpec, "%s%s.mkext", dirSpec, gBootFile + cnt);
  
  printf("NetLoadDrivers: Loading from [%s]\n", gDriverSpec);
  
  tries = 10;
  while (tries--) {
    if (LoadDriverMKext(gDriverSpec) == 0) break;
  }
  if (tries == -1) return -1;
  
  return 0;
}


static long LoadDriverMKext(char *fileSpec)
{
  unsigned long  driversAddr, driversLength, length;
  char           segName[32];
  DriversPackage *package;
  
  // Load the MKext.
  length = LoadThinFatFile(fileSpec, (void **)&package);
  if (length == -1) return -1;
  
  // Verify the MKext.
  if ((package->signature1 != kDriverPackageSignature1) ||
      (package->signature2 != kDriverPackageSignature2)) return -1;
  if (package->length > kLoadSize) return -1;
  if (package->adler32 != Adler32((char *)&package->version,
				  package->length - 0x10)) return -1;
  
  // Make space for the MKext.
  driversLength = package->length;
  driversAddr = AllocateKernelMemory(driversLength);
  
  // Copy the MKext.
  memcpy((void *)driversAddr, (void *)package, driversLength);
  
  // Add the MKext to the memory map.
  sprintf(segName, "DriversPackage-%x", driversAddr);
  AllocateMemoryRange(segName, driversAddr, driversLength);
  
  return 0;
}


static long LoadDriverPList(char *dirSpec, char *name, long bundleType)
{
  long      length, ret, driverPathLength;
  char      *buffer;
  ModulePtr module;
  TagPtr    personalities;
  char      *tmpDriverPath;
  
  // Reset the malloc zone.
  malloc_init((char *)kMallocAddr, kMallocSize);
  
  // Save the driver path.
  sprintf(gFileSpec, "%s\\%s\\%s", dirSpec, name,
	  (bundleType == kCFBundleType2) ? "Contents\\MacOS\\" : "");
  driverPathLength = strlen(gFileSpec);
  tmpDriverPath = malloc(driverPathLength + 1);
  if (tmpDriverPath == 0) return -1;
  strcpy(tmpDriverPath, gFileSpec);
  
  // Construct the file spec.
  sprintf(gFileSpec, "%s\\%s\\%sInfo.plist", dirSpec, name,
	  (bundleType == kCFBundleType2) ? "Contents\\" : "");
  
  length = LoadFile(gFileSpec);
  *((char*)kLoadAddr + length) = '\0';  // terminate for parser safety
  if (length == -1) {
    free(tmpDriverPath);
    return -1;
  }
  
  buffer = malloc(length + 1);
  if (buffer == 0) {
    free(tmpDriverPath);
    return -1;
  }
  strncpy(buffer, (char *)kLoadAddr, length);
  
  ret = XML2Module(buffer, &module, &personalities);
  free(buffer);
  if (ret != 0) {
    // could trap ret == -2 and report missing OSBundleRequired
    free(tmpDriverPath);
    return -1;
  }
  
  // Allocate memory for the driver path and the plist.
  module->driverPath = AllocateBootXMemory(driverPathLength + 1);
  module->plistAddr = AllocateBootXMemory(length + 1);
  
  if ((module->driverPath == 0) | (module->plistAddr == 0)) {
    free(tmpDriverPath);
    return -1;
  }
  
  // Save the driver path in the module.
  strcpy(module->driverPath, tmpDriverPath);
  free(tmpDriverPath);
  
  // Add the origin plist to the module.
  strncpy(module->plistAddr, (char *)kLoadAddr, length);
  module->plistLength = length + 1;
  
  // Add the module to the end of the module list.
  if (gModuleHead == 0) gModuleHead = module;
  else gModuleTail->nextModule = module;
  gModuleTail = module;
  
  // Add the extracted personalities to the list.
  if (personalities) personalities = personalities->tag;
  while (personalities != 0) {
    if (gPersonalityHead == 0) gPersonalityHead = personalities->tag;
    else gPersonalityTail->tagNext = personalities->tag;
    gPersonalityTail = personalities->tag;
    
    personalities = personalities->tagNext;
  }
  
  return 0;
}


static long LoadMatchedModules(void)
{
  TagPtr        prop;
  ModulePtr     module;
  char          *fileName, segName[32];
  DriverInfoPtr driver;
  unsigned long length, driverAddr, driverLength;
  void          *driverModuleAddr;
  
  module = gModuleHead;
  while (module != 0) {
    if (module->willLoad) {
      prop = GetProperty(module->dict, kPropCFBundleExecutable);
      if (prop != 0) {
	fileName = prop->string;
	sprintf(gFileSpec, "%s%s", module->driverPath, fileName);
	length = LoadThinFatFile(gFileSpec, &driverModuleAddr);
      } else length = 0;
      if (length != -1) {
	// Make make in the image area.
	driverLength = sizeof(DriverInfo) + module->plistLength + length;
	driverAddr = AllocateKernelMemory(driverLength);
	
	// Set up the DriverInfo.
	driver = (DriverInfoPtr)driverAddr;
	driver->plistAddr = (char *)(driverAddr + sizeof(DriverInfo));
	driver->plistLength = module->plistLength;
	if (length != 0) {
	  driver->moduleAddr   = (void *)(driverAddr + sizeof(DriverInfo) +
					module->plistLength);
	  driver->moduleLength = length;
	} else {
	  driver->moduleAddr   = 0;
	  driver->moduleLength = 0;
	}
	
	// Save the plist and module.
	strcpy(driver->plistAddr, module->plistAddr);
	if (length != 0) {
	  memcpy(driver->moduleAddr, driverModuleAddr, driver->moduleLength);
	}
	
	// Add an entry to the memory map.
	sprintf(segName, "Driver-%x", driver);
	AllocateMemoryRange(segName, driverAddr, driverLength);
      }
    }
    
    module = module->nextModule;
  }
  
  return 0;
}


static long MatchPersonalities(void)
{
  TagPtr    persionality;
  TagPtr    prop;
  ModulePtr module;
  CICell    ph;
  
  // Try to match each of the personalities.
  for(persionality = gPersonalityHead; persionality != 0;
	persionality = persionality->tagNext) {
    // Get the module name. Make sure it exists and has not
    // already been marked for loading.
    prop = GetProperty(persionality, kPropCFBundleIdentifier);
    if (prop == 0) continue;
    module = FindModule(prop->string);
    if (module == 0) continue;
    if (module->willLoad) continue;
    
    // Look for the exact match property.
    // Try to match with it.
    
    // Look for the old match property.
    // Try to match with it.
    ph = 0;
    prop = GetProperty(persionality, kPropIONameMatch);
    if ((prop != 0) && (prop->tag != 0)) prop = prop->tag;
    while (prop != 0) {
      ph = SearchForNodeMatching(0, 1, prop->string);
      if (ph != 0) break;
      
      prop = prop->tagNext;
    }
    
    // If a node was found mark the module to be loaded.
    if (ph != 0) {
      module->willLoad = 1;
    }
  }
  
  return 0;
}


static long MatchLibraries(void)
{
  TagPtr        prop, prop2;
  ModulePtr     module, module2;
  long          done;
  
  do {
    done = 1;
    module = gModuleHead;
    while (module != 0) {
      if (module->willLoad == 1) {
	prop = GetProperty(module->dict, kPropOSBundleLibraries);
	if (prop != 0) {
	  prop = prop->tag;
	  while (prop != 0) {
	    module2 = gModuleHead;
	    while (module2 != 0) {
	      prop2 = GetProperty(module2->dict, kPropCFBundleIdentifier);
	      if ((prop2 != 0) && (!strcmp(prop->string, prop2->string))) {
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
  } while (!done);
  
  return 0;
}


static ModulePtr FindModule(char *name)
{
  ModulePtr module;
  TagPtr    prop;
  
  module = gModuleHead;
  
  while (module != 0) {
    prop = GetProperty(module->dict, kPropCFBundleIdentifier);
    if ((prop != 0) && !strcmp(name, prop->string)) break;
    module = module->nextModule;
  }
  
  return module;
}

/* turn buffer of XML into a ModulePtr for driver analysis */
static long XML2Module(char *buffer, ModulePtr *module, TagPtr *personalities)
{
  TagPtr         moduleDict = NULL, required;
  ModulePtr      tmpModule;

  if(ParseXML(buffer, &moduleDict) < 0)
    return -1;

  required = GetProperty(moduleDict, kPropOSBundleRequired);
  if ((required == 0) || (required->type != kTagTypeString) ||
      !strcmp(required->string, "Safe Boot")) {
    FreeTag(moduleDict);
    return -2;
  }
  
  tmpModule = AllocateBootXMemory(sizeof(Module));
  if (tmpModule == 0) {
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

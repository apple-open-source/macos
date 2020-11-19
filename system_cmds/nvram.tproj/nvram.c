/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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
cc -o nvram nvram.c -framework CoreFoundation -framework IOKit -Wall
*/

#include <stdio.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <CoreFoundation/CoreFoundation.h>
#include <err.h>
#include <mach/mach_error.h>
#include <sys/stat.h>

// Prototypes
static void UsageMessage(char *message);
static void ParseFile(char *fileName);
static void ParseXMLFile(char *fileName);
static void SetOrGetOFVariable(char *str);
static kern_return_t GetOFVariable(char *name, CFStringRef *nameRef,
				   CFTypeRef *valueRef);
static kern_return_t SetOFVariable(char *name, char *value);
static void DeleteOFVariable(char *name);
static void PrintOFVariables(void);
static void PrintOFVariable(const void *key,const void *value,void *context);
static void SetOFVariableFromFile(const void *key, const void *value, void *context);
static void ClearOFVariables(void);
static void ClearOFVariable(const void *key,const void *value,void *context);
static CFTypeRef ConvertValueToCFTypeRef(CFTypeID typeID, char *value);

static void NVRamSyncNow(char *name);

// Global Variables
static char                *gToolName;
static io_registry_entry_t gOptionsRef;
static bool                gUseXML;
static bool                gUseForceSync;

#if TARGET_OS_BRIDGE /* Stuff for nvram bridge -> intel */
#include <dlfcn.h>
#include <libMacEFIManager/MacEFIHostInterfaceAPI.h>

static kern_return_t LinkMacNVRAMSymbols(void);
static kern_return_t GetMacOFVariable(char *name, char **value);
static kern_return_t SetMacOFVariable(char *name, char *value);
static kern_return_t DeleteMacOFVariable(char *name);

static bool gBridgeToIntel;
static void *gDL_handle;
static void *gNvramInterface;

static void (*hostInterfaceInitialize_fptr)(void);
static void *(*createNvramHostInterface_fptr)(const char *handle);
static kern_return_t (*destroyNvramHostInterface_fptr)(void *interface);
static kern_return_t (*getNVRAMVariable_fptr)(void *interface, char *name, char **buffer, uint32_t *size);
static kern_return_t (*setNVRAMVariable_fptr)(void *interface, char *name, char *buffer);
static kern_return_t (*deleteNVRAMVariable_fptr)(void *interface, char *name);
static void (*hostInterfaceDeinitialize_fptr)(void); /* may not need? */

#endif /* TARGET_OS_BRIDGE */

int main(int argc, char **argv)
{
  long                cnt;
  char                *str, errorMessage[256];
  kern_return_t       result;
  mach_port_t         masterPort;
  int argcount = 0;

  // Get the name of the command.
  gToolName = strrchr(argv[0], '/');
  if (gToolName != 0) gToolName++;
  else gToolName = argv[0];

  result = IOMasterPort(bootstrap_port, &masterPort);
  if (result != KERN_SUCCESS) {
    errx(1, "Error getting the IOMaster port: %s",
        mach_error_string(result));
  }

  gOptionsRef = IORegistryEntryFromPath(masterPort, "IODeviceTree:/options");
  if (gOptionsRef == 0) {
    errx(1, "nvram is not supported on this system");
  }

  for (cnt = 1; cnt < argc; cnt++) {
    str = argv[cnt];
    if (str[0] == '-' && str[1] != 0) {
      // Parse the options.
      for (str += 1 ; *str; str++) {
	switch (*str) {
	case 'p' :
#if TARGET_OS_BRIDGE
        if (gBridgeToIntel) {
          fprintf(stderr, "-p not supported for Mac NVRAM store.\n");
          return 1;
        }
#endif
	  PrintOFVariables();
	  break;

	case 'x' :
          gUseXML = true;
          break;

	case 'f':
#if TARGET_OS_BRIDGE
        if (gBridgeToIntel) {
          fprintf(stderr, "-f not supported for Mac NVRAM store.\n");
          return 1;
        }
#endif
	  cnt++;
	  if (cnt < argc && *argv[cnt] != '-') {
	    ParseFile(argv[cnt]);
	  } else {
	    UsageMessage("missing filename");
	  }
	  break;

    case 'd':
      cnt++;
      if (cnt < argc && *argv[cnt] != '-') {
#if TARGET_OS_BRIDGE
          if (gBridgeToIntel) {
              if ((result = DeleteMacOFVariable(argv[cnt])) != KERN_SUCCESS) {
                  errx(1, "Error deleting variable - '%s': %s (0x%08x)", argv[cnt],
                          mach_error_string(result), result);
              }
          }
          else
#endif
          {
              DeleteOFVariable(argv[cnt]);
          }
      } else {
          UsageMessage("missing name");
      }
      break;

	case 'c':
#if TARGET_OS_BRIDGE
        if (gBridgeToIntel) {
          fprintf(stderr, "-c not supported for Mac NVRAM store.\n");
          return 1;
        }
#endif
	  ClearOFVariables();
	  break;
	case 's':
	  // -s option is unadvertised -- advises the kernel more forcibly to
	  // commit the variable to nonvolatile storage
	  gUseForceSync = true;
	  break;
#if TARGET_OS_BRIDGE
	case 'm':
	  // used to set nvram variables on the Intel side
	  // from the ARM side (Bridge -> Mac)
      fprintf(stdout, "Using Mac NVRAM store.\n");

      LinkMacNVRAMSymbols();
	  gBridgeToIntel = true;
	  break;
#endif

	default:
	  strcpy(errorMessage, "no such option as --");
	  errorMessage[strlen(errorMessage)-1] = *str;
	  UsageMessage(errorMessage);
	}
      }
    } else {
      // Other arguments will be firmware variable requests.
      argcount++;
      SetOrGetOFVariable(str);
    }
  }

  // radar:25206371
  if (argcount == 0 && gUseForceSync == true) {
    NVRamSyncNow("");
  }

  IOObjectRelease(gOptionsRef);

  return 0;
}

// UsageMessage(message)
//
//   Print the usage information and exit.
//
static void UsageMessage(char *message)
{
  warnx("(usage: %s)", message);

  printf("%s [-x] [-p] [-f filename] [-d name] [-c] name[=value] ...\n", gToolName);
  printf("\t-x         use XML format for printing or reading variables\n");
  printf("\t           (must appear before -p or -f)\n");
  printf("\t-p         print all firmware variables\n");
  printf("\t-f         set firmware variables from a text file\n");
  printf("\t-d         delete the named variable\n");
  printf("\t-c         delete all variables\n");
#if TARGET_OS_BRIDGE
  printf("\t-m         set nvram variables on macOS from bridgeOS\n");
#endif
  printf("\tname=value set named variable\n");
  printf("\tname       print variable\n");
  printf("Note that arguments and options are executed in order.\n");

  exit(1);
}


// States for ParseFile.
enum {
  kFirstColumn = 0,
  kScanComment,
  kFindName,
  kCollectName,
  kFindValue,
  kCollectValue,
  kContinueValue,
  kSetenv,

  kMaxStringSize = 0x800,
  kMaxNameSize = 0x100
};


// ParseFile(fileName)
//
//   Open and parse the specified file.
//
static void ParseFile(char *fileName)
{
  long state, ni = 0, vi = 0;
  int tc;
  char name[kMaxNameSize];
  char value[kMaxStringSize];
  FILE *patches;
  kern_return_t kret;

  if (gUseXML) {
    ParseXMLFile(fileName);
    return;
  }

  patches = fopen(fileName, "r");
  if (patches == 0) {
    err(1, "Couldn't open patch file - '%s'", fileName);
  }

  state = kFirstColumn;
  while ((tc = getc(patches)) != EOF) {
    if(ni==(kMaxNameSize-1))
      errx(1, "Name exceeded max length of %d", kMaxNameSize);
    if(vi==(kMaxStringSize-1))
      errx(1, "Value exceeded max length of %d", kMaxStringSize);
    switch (state) {
    case kFirstColumn :
      ni = 0;
      vi = 0;
      if (tc == '#') {
	state = kScanComment;
      } else if (tc == '\n') {
	// state stays kFirstColumn.
      } else if (isspace(tc)) {
	state = kFindName;
      } else {
	state = kCollectName;
	name[ni++] = tc;
      }
      break;

    case kScanComment :
      if (tc == '\n') {
	state = kFirstColumn;
      } else {
	// state stays kScanComment.
      }
      break;

    case kFindName :
      if (tc == '\n') {
	state = kFirstColumn;
      } else if (isspace(tc)) {
	// state stays kFindName.
      } else {
	state = kCollectName;
	name[ni++] = tc;
      }
      break;

    case kCollectName :
      if (tc == '\n') {
	name[ni] = 0;
	warnx("Name must be followed by white space - '%s'", name);
	state = kFirstColumn;
      } else if (isspace(tc)) {
	state = kFindValue;
      } else {
	name[ni++] = tc;
	// state staus kCollectName.
      }
      break;

    case kFindValue :
    case kContinueValue :
      if (tc == '\n') {
	state = kSetenv;
      } else if (isspace(tc)) {
	// state stays kFindValue or kContinueValue.
      } else {
	state = kCollectValue;
	value[vi++] = tc;
      }
      break;

    case kCollectValue :
      if (tc == '\n') {
	if (value[vi-1] == '\\') {
	  value[vi-1] = '\r';
	  state = kContinueValue;
	} else {
	  state = kSetenv;
	}
      } else {
	// state stays kCollectValue.
	value[vi++] = tc;
      }
      break;
    }

    if (state == kSetenv) {
      name[ni] = 0;
      value[vi] = 0;
      if ((kret = SetOFVariable(name, value)) != KERN_SUCCESS) {
        errx(1, "Error setting variable - '%s': %s", name,
             mach_error_string(kret));
      }
      state = kFirstColumn;
    }
  }

  if (state != kFirstColumn) {
    errx(1, "Last line ended abruptly");
  }
}

// ParseXMLFile(fileName)
//
//   Open and parse the specified file in XML format,
//   and set variables appropriately.
//
static void ParseXMLFile(char *fileName)
{
        CFPropertyListRef plist;
        int fd;
        struct stat sb;
        char *buffer;
        CFReadStreamRef stream;
        CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0;

        fd = open(fileName, O_RDONLY | O_NOFOLLOW, S_IFREG);
        if (fd == -1) {
          errx(1, "Could not open %s: %s", fileName, strerror(errno));
        }

        if (fstat(fd, &sb) == -1) {
          errx(1, "Could not fstat %s: %s", fileName, strerror(errno));
        }

        if (sb.st_size > UINT32_MAX) {
          errx(1, "too big for our purposes");
        }

        buffer = malloc((size_t)sb.st_size);
        if (buffer == NULL) {
          errx(1, "Could not allocate buffer");
        }

        if (read(fd, buffer, (size_t)sb.st_size) != sb.st_size) {
          errx(1, "Could not read %s: %s", fileName, strerror(errno));
        }

        close(fd);

        stream = CFReadStreamCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                   (const UInt8 *)buffer,
                                                   (CFIndex)sb.st_size,
                                                   kCFAllocatorNull);
        if (stream == NULL) {
          errx(1, "Could not create stream from serialized data");
        }

        if (!CFReadStreamOpen(stream)) {
          errx(1, "Could not open the stream");
        }

        plist = CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                               stream,
                                               (CFIndex)sb.st_size,
                                               kCFPropertyListImmutable,
                                               &format,
                                               NULL);

        if (plist == NULL) {
          errx(1, "Error parsing XML file");
        }

        CFReadStreamClose(stream);

        CFRelease(stream);

        free(buffer);

        CFDictionaryApplyFunction(plist, &SetOFVariableFromFile, 0);

        CFRelease(plist);
}

// SetOrGetOFVariable(str)
//
//   Parse the input string, then set, append or get
//   the specified firmware variable.
//
static void SetOrGetOFVariable(char *str)
{
  long          set = 0;
  long          append = 0;
  char          *name;
  char          *value;
  CFStringRef        nameRef = NULL;
  CFTypeRef          valueRef = NULL;
  CFMutableStringRef appended = NULL;
  kern_return_t result;

  // OF variable name is first.
  name = str;

  // Find the equal sign for set or += for append
  while (*str) {
    if (*str == '+' && *(str+1) == '=') {
      append = 1;
      *str++ = '\0';
      *str++ = '\0';
      break;
    }

    if (*str == '=') {
      set = 1;
      *str++ = '\0';
      break;
    }
    str++;
  }

  // Read the current value if appending or if no =/+=
  if (append == 1 || (set == 0 && append == 0)) {
#if TARGET_OS_BRIDGE
    if (gBridgeToIntel) {
      result = GetMacOFVariable(name, &value);
      if (result != KERN_SUCCESS) {
        errx(1, "Error getting variable - '%s': %s", name,
             mach_error_string(result));
      }
      nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
      valueRef = CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
      free(value);
    }
    else
#endif
    {
      result = GetOFVariable(name, &nameRef, &valueRef);
      if (result != KERN_SUCCESS) {
        errx(1, "Error getting variable - '%s': %s", name,
             mach_error_string(result));
      }
    }
  }

  if (set == 1) {
    // On sets, the OF variable's value follows the equal sign.
    value = str;
  }

  if (append == 1) {
    // On append, the value to append follows the += substring
    appended = CFStringCreateMutableCopy(NULL, 0, valueRef);
    CFStringAppendCString(appended, str, kCFStringEncodingUTF8);
    value = (char*)CFStringGetCStringPtr(appended, kCFStringEncodingUTF8);
  }

  if (set == 1 || append == 1) {
#if TARGET_OS_BRIDGE
    if (gBridgeToIntel) {
      result = SetMacOFVariable(name, value);
    }
    else
#endif
    {
      result = SetOFVariable(name, value);
      NVRamSyncNow(name);            /* Try syncing the new data to device, best effort! */
    }
    if (result != KERN_SUCCESS) {
      errx(1, "Error setting variable - '%s': %s", name,
           mach_error_string(result));
    }
  } else {
    PrintOFVariable(nameRef, valueRef, 0);
  }
  if ( nameRef ) CFRelease(nameRef);
  if ( valueRef ) CFRelease(valueRef);
  if ( appended ) CFRelease(appended);
}

#if TARGET_OS_BRIDGE
static kern_return_t LinkMacNVRAMSymbols()
{
  gDL_handle = dlopen("libMacEFIHostInterface.dylib", RTLD_LAZY);
  if (gDL_handle == NULL) {
    errx(errno, "Failed to dlopen libMacEFIHostInterface.dylib");
    return KERN_FAILURE; /* NOTREACHED */
  }

  hostInterfaceInitialize_fptr = dlsym(gDL_handle, "hostInterfaceInitialize");
  if (hostInterfaceInitialize_fptr == NULL) {
    errx(errno, "failed to link hostInterfaceInitialize");
  }
  createNvramHostInterface_fptr = dlsym(gDL_handle, "createNvramHostInterface");
  if (createNvramHostInterface_fptr == NULL) {
    errx(errno, "failed to link createNvramHostInterface");
  }
  destroyNvramHostInterface_fptr = dlsym(gDL_handle, "destroyNvramHostInterface");
  if (destroyNvramHostInterface_fptr == NULL) {
    errx(errno, "failed to link destroyNvramHostInterface");
  }
  getNVRAMVariable_fptr = dlsym(gDL_handle, "getNVRAMVariable");
  if (getNVRAMVariable_fptr == NULL) {
    errx(errno, "failed to link getNVRAMVariable");
  }
  setNVRAMVariable_fptr = dlsym(gDL_handle, "setNVRAMVariable");
  if (setNVRAMVariable_fptr == NULL) {
    errx(errno, "failed to link setNVRAMVariable");
  }
  deleteNVRAMVariable_fptr = dlsym(gDL_handle, "deleteNVRAMVariable");
  if (deleteNVRAMVariable_fptr == NULL) {
      errx(errno, "failed to link deleteNVRAMVariable");
  }
  hostInterfaceDeinitialize_fptr = dlsym(gDL_handle, "hostInterfaceDeinitialize");
  if (hostInterfaceDeinitialize_fptr == NULL) {
    errx(errno, "failed to link hostInterfaceDeinitialize");
  }

  /* also do the initialization */
  hostInterfaceInitialize_fptr();
  gNvramInterface = createNvramHostInterface_fptr(NULL);

  return KERN_SUCCESS;
}
#endif

// GetOFVariable(name, nameRef, valueRef)
//
//   Get the named firmware variable.
//   Return it and it's symbol in valueRef and nameRef.
//
static kern_return_t GetOFVariable(char *name, CFStringRef *nameRef,
				   CFTypeRef *valueRef)
{
  *nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name,
				       kCFStringEncodingUTF8);
  if (*nameRef == 0) {
    errx(1, "Error creating CFString for key %s", name);
  }

  *valueRef = IORegistryEntryCreateCFProperty(gOptionsRef, *nameRef, 0, 0);
  if (*valueRef == 0) return kIOReturnNotFound;

  return KERN_SUCCESS;
}

#if TARGET_OS_BRIDGE
// GetMacOFVariable(name, value)
//
// Get the named firmware variable from the Intel side.
// Return the value in value
//
static kern_return_t GetMacOFVariable(char *name, char **value)
{
  uint32_t value_size;

  return getNVRAMVariable_fptr(gNvramInterface, name, value, &value_size);
}
#endif

// SetOFVariable(name, value)
//
//   Set or create an firmware variable with name and value.
//
static kern_return_t SetOFVariable(char *name, char *value)
{
  CFStringRef   nameRef;
  CFTypeRef     valueRef;
  CFTypeID      typeID;
  kern_return_t result = KERN_SUCCESS;

    nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name,
                                        kCFStringEncodingUTF8);
    if (nameRef == 0) {
        errx(1, "Error creating CFString for key %s", name);
    }

    valueRef = IORegistryEntryCreateCFProperty(gOptionsRef, nameRef, 0, 0);
    if (valueRef) {
        typeID = CFGetTypeID(valueRef);
        CFRelease(valueRef);

        valueRef = ConvertValueToCFTypeRef(typeID, value);
        if (valueRef == 0) {
            errx(1, "Error creating CFTypeRef for value %s", value);
        }  result = IORegistryEntrySetCFProperty(gOptionsRef, nameRef, valueRef);
    } else {
        while (1) {
            // In the default case, try data, string, number, then boolean.

            valueRef = ConvertValueToCFTypeRef(CFDataGetTypeID(), value);
            if (valueRef != 0) {
                result = IORegistryEntrySetCFProperty(gOptionsRef, nameRef, valueRef);
                if (result == KERN_SUCCESS) break;
            }

            valueRef = ConvertValueToCFTypeRef(CFStringGetTypeID(), value);
            if (valueRef != 0) {
                result = IORegistryEntrySetCFProperty(gOptionsRef, nameRef, valueRef);
                if (result == KERN_SUCCESS) break;
            }

            valueRef = ConvertValueToCFTypeRef(CFNumberGetTypeID(), value);
            if (valueRef != 0) {
                result = IORegistryEntrySetCFProperty(gOptionsRef, nameRef, valueRef);
                if (result == KERN_SUCCESS) break;
            }

            valueRef = ConvertValueToCFTypeRef(CFBooleanGetTypeID(), value);
            if (valueRef != 0) {
                result = IORegistryEntrySetCFProperty(gOptionsRef, nameRef, valueRef);
                if (result == KERN_SUCCESS) break;
            }

            break;
        }
    }

  CFRelease(nameRef);

  return result;
}

#if TARGET_OS_BRIDGE
static kern_return_t SetMacOFVariable(char *name, char *value)
{
  return setNVRAMVariable_fptr(gNvramInterface, name, value);
}
#endif

// DeleteOFVariable(name)
//
//   Delete the named firmware variable.
//
//
static void DeleteOFVariable(char *name)
{
  SetOFVariable(kIONVRAMDeletePropertyKey, name);
}

#if TARGET_OS_BRIDGE
static kern_return_t DeleteMacOFVariable(char *name)
{
    return deleteNVRAMVariable_fptr(gNvramInterface, name);
}
#endif

static void NVRamSyncNow(char *name)
{
  if (!gUseForceSync) {
    SetOFVariable(kIONVRAMSyncNowPropertyKey, name);
  } else {
    SetOFVariable(kIONVRAMForceSyncNowPropertyKey, name);
  }
}

// PrintOFVariables()
//
//   Print all of the firmware variables.
//
static void PrintOFVariables(void)
{
  kern_return_t          result;
  CFMutableDictionaryRef dict;

  result = IORegistryEntryCreateCFProperties(gOptionsRef, &dict, 0, 0);
  if (result != KERN_SUCCESS) {
    errx(1, "Error getting the firmware variables: %s", mach_error_string(result));
  }

  if (gUseXML) {
    CFDataRef data;

    data = CFPropertyListCreateData( kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, NULL );
    if (data == NULL) {
      errx(1, "Error converting variables to xml");
    }

    fwrite(CFDataGetBytePtr(data), sizeof(UInt8), CFDataGetLength(data), stdout);

    CFRelease(data);

  } else {

    CFDictionaryApplyFunction(dict, &PrintOFVariable, 0);

  }

  CFRelease(dict);
}

// PrintOFVariable(key, value, context)
//
//   Print the given firmware variable.
//
static void PrintOFVariable(const void *key, const void *value, void *context)
{
  long          cnt, cnt2;
  CFIndex       nameLen;
  char          *nameBuffer = 0;
  const char    *nameString;
  char          numberBuffer[10];
  const uint8_t *dataPtr;
  uint8_t       dataChar;
  char          *dataBuffer = 0;
  CFIndex       valueLen;
  char          *valueBuffer = 0;
  const char    *valueString = 0;
  uint32_t      number;
  long		length;
  CFTypeID      typeID;

  if (gUseXML) {
    CFDataRef data;
    CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, &key, &value, 1,
                                              &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
      errx(1, "Error creating dictionary for variable value");
    }

    data = CFPropertyListCreateData( kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, NULL );
    if (data == NULL) {
      errx(1, "Error creating xml plist for variable");
    }

    fwrite(CFDataGetBytePtr(data), sizeof(UInt8), CFDataGetLength(data), stdout);

    CFRelease(dict);
    CFRelease(data);
    return;
  }

  // Get the OF variable's name.
  nameLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key),
      kCFStringEncodingUTF8) + 1;
  nameBuffer = malloc(nameLen);
  if( nameBuffer && CFStringGetCString(key, nameBuffer, nameLen, kCFStringEncodingUTF8) )
    nameString = nameBuffer;
  else {
    warnx("Unable to convert property name to C string");
    nameString = "<UNPRINTABLE>";
  }

  // Get the OF variable's type.
  typeID = CFGetTypeID(value);

  if (typeID == CFBooleanGetTypeID()) {
    if (CFBooleanGetValue(value)) valueString = "true";
    else valueString = "false";
  } else if (typeID == CFNumberGetTypeID()) {
    CFNumberGetValue(value, kCFNumberSInt32Type, &number);
    if (number == 0xFFFFFFFF) sprintf(numberBuffer, "-1");
    else if (number < 1000) sprintf(numberBuffer, "%d", number);
    else sprintf(numberBuffer, "0x%x", number);
    valueString = numberBuffer;
  } else if (typeID == CFStringGetTypeID()) {
    valueLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
        kCFStringEncodingUTF8) + 1;
    valueBuffer = malloc(valueLen + 1);
    if ( valueBuffer && CFStringGetCString(value, valueBuffer, valueLen, kCFStringEncodingUTF8) )
      valueString = valueBuffer;
    else {
      warnx("Unable to convert value to C string");
      valueString = "<UNPRINTABLE>";
    }
  } else if (typeID == CFDataGetTypeID()) {
    length = CFDataGetLength(value);
    if (length == 0) valueString = "";
    else {
      dataBuffer = malloc(length * 3 + 1);
      if (dataBuffer != 0) {
	dataPtr = CFDataGetBytePtr(value);
	for (cnt = cnt2 = 0; cnt < length; cnt++) {
	  dataChar = dataPtr[cnt];
	  if (isprint(dataChar) && dataChar != '%') {
	    dataBuffer[cnt2++] = dataChar;
	  } else {
	    sprintf(dataBuffer + cnt2, "%%%02x", dataChar);
	    cnt2 += 3;
	  }
	}
	dataBuffer[cnt2] = '\0';
	valueString = dataBuffer;
      }
    }
  } else {
    valueString="<INVALID>";
  }

  if ((nameString != 0) && (valueString != 0))
    printf("%s\t%s\n", nameString, valueString);

  if (dataBuffer != 0) free(dataBuffer);
  if (nameBuffer != 0) free(nameBuffer);
  if (valueBuffer != 0) free(valueBuffer);
}

// ClearOFVariables()
//
//   Deletes all OF variables
//
static void ClearOFVariables(void)
{
    kern_return_t          result;
    CFMutableDictionaryRef dict;

    result = IORegistryEntryCreateCFProperties(gOptionsRef, &dict, 0, 0);
    if (result != KERN_SUCCESS) {
      errx(1, "Error getting the firmware variables: %s", mach_error_string(result));
    }
    CFDictionaryApplyFunction(dict, &ClearOFVariable, 0);

    CFRelease(dict);
}

static void ClearOFVariable(const void *key, const void *value, void *context)
{
  kern_return_t result;
  result = IORegistryEntrySetCFProperty(gOptionsRef,
                                        CFSTR(kIONVRAMDeletePropertyKey), key);
  if (result != KERN_SUCCESS) {
    assert(CFGetTypeID(key) == CFStringGetTypeID());
    const char *keyStr = CFStringGetCStringPtr(key, kCFStringEncodingUTF8);
    char *keyBuffer = NULL;
    size_t keyBufferLen = 0;
    if (!keyStr) {
      keyBufferLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key), kCFStringEncodingUTF8) + 1;
      keyBuffer = (char *)malloc(keyBufferLen);
      if (keyBuffer != NULL && CFStringGetCString(key, keyBuffer, keyBufferLen, kCFStringEncodingUTF8)) {
        keyStr = keyBuffer;
      } else {
        warnx("Unable to convert property name to C string");
        keyStr = "<UNPRINTABLE>";
      }
    }

    warnx("Error clearing firmware variable %s: %s", keyStr, mach_error_string(result));
    if (keyBuffer) {
      free(keyBuffer);
    }
  }
}

// ConvertValueToCFTypeRef(typeID, value)
//
//   Convert the value into a CFType given the typeID.
//
static CFTypeRef ConvertValueToCFTypeRef(CFTypeID typeID, char *value)
{
    CFTypeRef     valueRef = 0;
    long          cnt, cnt2, length;
    unsigned long number, tmp;

    if (typeID == CFBooleanGetTypeID()) {
        if (!strcmp("true", value)) valueRef = kCFBooleanTrue;
        else if (!strcmp("false", value)) valueRef = kCFBooleanFalse;
    } else if (typeID == CFNumberGetTypeID()) {
        number = strtol(value, 0, 0);
        valueRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                  &number);
    } else if (typeID == CFStringGetTypeID()) {
        valueRef = CFStringCreateWithCString(kCFAllocatorDefault, value,
                                             kCFStringEncodingUTF8);
    } else if (typeID == CFDataGetTypeID()) {
        length = strlen(value);
        for (cnt = cnt2 = 0; cnt < length; cnt++, cnt2++) {
            if (value[cnt] == '%') {
                if (!ishexnumber(value[cnt + 1]) ||
                    !ishexnumber(value[cnt + 2])) return 0;
                number = toupper(value[++cnt]) - '0';
                if (number > 9) number -= 7;
                tmp = toupper(value[++cnt]) - '0';
                if (tmp > 9) tmp -= 7;
                number = (number << 4) + tmp;
                value[cnt2] = number;
            } else value[cnt2] = value[cnt];
        }
        valueRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)value,
                                               cnt2, kCFAllocatorDefault);
    } else return 0;

    return valueRef;
}

static void SetOFVariableFromFile(const void *key, const void *value, void *context)
{
  kern_return_t result;

  result = IORegistryEntrySetCFProperty(gOptionsRef, key, value);
  if ( result != KERN_SUCCESS ) {
          long nameLen;
          char *nameBuffer;
          char *nameString;

          // Get the variable's name.
          nameLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key),
              kCFStringEncodingUTF8) + 1;
          nameBuffer = malloc(nameLen);
          if( nameBuffer && CFStringGetCString(key, nameBuffer, nameLen, kCFStringEncodingUTF8) )
                  nameString = nameBuffer;
          else {
                  warnx("Unable to convert property name to C string");
                  nameString = "<UNPRINTABLE>";
          }
          errx(1, "Error setting variable - '%s': %s", nameString,
               mach_error_string(result));
  }
}

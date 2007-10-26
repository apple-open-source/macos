/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CoreFoundation.h>

// Prototypes
static void Error(char *format, long item);
static void FatalError(long exitValue, char *format, long item);
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

// Global Variables
static char                *gToolName;
static io_registry_entry_t gOptionsRef;
static bool                gUseXML;


int main(int argc, char **argv)
{
  long                cnt;
  char                *str, errorMessage[256];
  kern_return_t       result;
  mach_port_t         masterPort;
  
  // Get the name of the command.
  gToolName = strrchr(argv[0], '/');
  if (gToolName != 0) gToolName++;
  else gToolName = argv[0];
  
  result = IOMasterPort(bootstrap_port, &masterPort);
  if (result != KERN_SUCCESS) {
    FatalError(-1, "Error (%d) getting the IOMaster port", result);
    exit(-1);
  }
  
  gOptionsRef = IORegistryEntryFromPath(masterPort, "IODeviceTree:/options");
  if (gOptionsRef == 0) {
    FatalError(-1, "nvram is not supported on this system.", -1);
    exit(-1);
  }
  
  for (cnt = 1; cnt < argc; cnt++) {
    str = argv[cnt];
    if (str[0] == '-' && str[1] != 0) {
      // Parse the options.
      for (str += 1 ; *str; str++) {
	switch (*str) {
	case 'p' :
	  PrintOFVariables();
	  break;

	case 'x' :
          gUseXML = true;
          break;

	case 'f':
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
	    DeleteOFVariable(argv[cnt]);
	  } else {
	    UsageMessage("missing name");
	  }
	  break;
	  
	case 'c':
	  ClearOFVariables();
	  break;
	  
	default:
	  strcpy(errorMessage, "no such option as --");
	  errorMessage[strlen(errorMessage)-1] = *str;
	  UsageMessage(errorMessage);
	}
      }
    } else {
      // Other arguments will be firmware variable requests.
      SetOrGetOFVariable(str);
    }
  }
  
  IOObjectRelease(gOptionsRef);
  
  return 0;
}


// Error(format, item)
//
//   Print a message on standard error.
//
static void Error(char *format, long item)
{
  fprintf(stderr, "%s: ", gToolName);
  fprintf(stderr, format, item);
  fprintf(stderr, "\n");
}


// FatalError(exitValue, format, item)
//
//   Print a message on standard error and exit with value.
//
static void FatalError(long exitValue, char *format, long item)
{
  fprintf(stderr, "%s: ", gToolName);
  fprintf(stderr, format, item);
  fprintf(stderr, "\n");
  
  exit(exitValue);
}


// UsageMessage(message)
//
//   Print the usage information and exit.
//
static void UsageMessage(char *message)
{
  Error("(usage: %s)", (long)message);
  
  printf("%s [-x] [-p] [-f filename] [-d name] name[=value] ...\n", gToolName);
  printf("\t-x         use XML format for printing or reading variables\n");
  printf("\t           (must appear before -p or -f)\n");
  printf("\t-p         print all firmware variables\n");
  printf("\t-f         set firmware variables from a text file\n");
  printf("\t-d         delete the named variable\n");
  printf("\t-c         delete all variables\n");
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
  long state, tc, ni = 0, vi = 0;
  char name[kMaxNameSize];
  char value[kMaxStringSize];
  FILE *patches;

  if (gUseXML) {
    ParseXMLFile(fileName);
    return;
  }
  
  patches = fopen(fileName, "r");
  if (patches == 0) {
    FatalError(errno, "Couldn't open patch file - '%s'", (long)fileName);
  }
  
  state = kFirstColumn;
  while ((tc = getc(patches)) != EOF) {
    if(ni==(kMaxNameSize-1)) 
      FatalError(-1,"Name exceeded max length of %d",kMaxNameSize);
    if(vi==(kMaxStringSize-1))
      FatalError(-1,"Value exceeded max length of %d",kMaxStringSize);
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
	Error("Name must be followed by white space - '%s'", (long)name);
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
      if (SetOFVariable(name, value) != KERN_SUCCESS) {
	FatalError(-1, "Error (-1) setting variable - '%s'", (long)name);
      }
      state = kFirstColumn;
    }
  }
  
  if (state != kFirstColumn) {
    FatalError(-1, "Last line ended abruptly", 0);
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
        CFURLRef fileURL = NULL;
        CFStringRef filePath = NULL;
        CFStringRef errorString = NULL;
        CFDataRef data = NULL;
        SInt32 errorCode = 0;

        filePath = CFStringCreateWithCString(kCFAllocatorDefault, fileName, kCFStringEncodingUTF8);
        if (filePath == NULL) {
                FatalError(-1, "Could not create file path string", 0);
        }

        // Create a URL that specifies the file we will create to 
        // hold the XML data.
        fileURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault,    
                                                 filePath,
                                                 kCFURLPOSIXPathStyle,
                                                 false /* not a directory */ );
        if (fileURL == NULL) {
                FatalError(-1, "Could not create file path URL", 0);
        }

        CFRelease(filePath);

        if (! CFURLCreateDataAndPropertiesFromResource(
                    kCFAllocatorDefault,
                    fileURL,
                    &data,
                    NULL,      
                    NULL,
                    &errorCode) || data == NULL ) {
                FatalError(-1, "Error reading XML file (%d)", errorCode);
        }

        CFRelease(fileURL);

        plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                                                data,
                                                kCFPropertyListImmutable,
                                                &errorString);

        CFRelease(data);

        if (plist == NULL) {
                FatalError(-1, "Error parsing XML file", 0);
        }

        if (errorString != NULL) {
                FatalError(-1, "Error parsing XML file: %s", (long)CFStringGetCStringPtr(errorString, kCFStringEncodingUTF8));
        }

        CFDictionaryApplyFunction(plist, &SetOFVariableFromFile, 0);

        CFRelease(plist);
}

// SetOrGetOFVariable(str)
//
//   Parse the input string, then set or get the specified
//   firmware variable.
//
static void SetOrGetOFVariable(char *str)
{
  long          set = 0;
  char          *name;
  char          *value;
  CFStringRef   nameRef;
  CFTypeRef     valueRef;
  kern_return_t result;
  
  // OF variable name is first.
  name = str;
  
  // Find the equal sign for set
  while (*str) {
    if (*str == '=') {
      set = 1;
      *str++ = '\0';
      break;
    }
    str++;
  }
  
  if (set == 1) {
    // On sets, the OF variable's value follows the equal sign.
    value = str;
    
    result = SetOFVariable(name, value);
    if (result != KERN_SUCCESS) {
      FatalError(-1, "Error (-1) setting variable - '%s'", (long)name);
    }
  } else {
    result = GetOFVariable(name, &nameRef, &valueRef);
    if (result != KERN_SUCCESS) {
      FatalError(-1, "Error (-1) getting variable - '%s'", (long)name);
    }
    
    PrintOFVariable(nameRef, valueRef, 0);
    CFRelease(nameRef);
    CFRelease(valueRef);
  }
}


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
    FatalError(-1, "Error (-1) creating CFString for key %s", (long)name);
  }
  
  *valueRef = IORegistryEntryCreateCFProperty(gOptionsRef, *nameRef, 0, 0);
  if (*valueRef == 0) return -1;
  
  return KERN_SUCCESS;
}


// SetOFVariable(name, value)
//
//   Set or create an firmware variable with name and value.
//
static kern_return_t SetOFVariable(char *name, char *value)
{
  CFStringRef   nameRef;
  CFTypeRef     valueRef;
  CFTypeID      typeID;
  kern_return_t result;
  
  nameRef = CFStringCreateWithCString(kCFAllocatorDefault, name,
				      kCFStringEncodingUTF8);
  if (nameRef == 0) {
    FatalError(-1, "Error (-1) creating CFString for key %s", (long)name);
  }
  
  valueRef = IORegistryEntryCreateCFProperty(gOptionsRef, nameRef, 0, 0);
  if (valueRef) {
    typeID = CFGetTypeID(valueRef);
    CFRelease(valueRef);
    
    valueRef = ConvertValueToCFTypeRef(typeID, value);
    if (valueRef == 0) {
      FatalError(-1, "Error (-1) creating CFTypeRef for value %s",(long)value);
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
      
      result = -1;
      break;
    }
  }
  
  CFRelease(nameRef);
  
  return result;
}


// DeleteOFVariable(name)
//
//   Delete the named firmware variable.
//   
//
static void DeleteOFVariable(char *name)
{
  SetOFVariable(kIONVRAMDeletePropertyKey, name);
}


// PrintOFVariables()
//
//   Print all of the firmware variables.
//
static void PrintOFVariables()
{
  kern_return_t          result;
  CFMutableDictionaryRef dict;
  
  result = IORegistryEntryCreateCFProperties(gOptionsRef, &dict, 0, 0);
  if (result != KERN_SUCCESS) {
    FatalError(-1, "Error (%d) getting the firmware variables", result);
  }

  if (gUseXML) {
    CFDataRef data;

    data = CFPropertyListCreateXMLData( kCFAllocatorDefault, dict );
    if (data == NULL) {
      FatalError(-1, "Error (%d) converting variables to xml", result);
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
  uint32_t      number, length;
  CFTypeID      typeID;
  
  // Get the OF variable's name.
  nameLen = CFStringGetLength(key) + 1;
  nameBuffer = malloc(nameLen);
  if( nameBuffer && CFStringGetCString(key, nameBuffer, nameLen, kCFStringEncodingUTF8) )
    nameString = nameBuffer;
  else {
    Error("Error (-1) Unable to convert property name to C string", 0);
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
    valueLen = CFStringGetLength(value) + 1;
    valueBuffer = malloc(valueLen + 1);
    if ( valueBuffer && CFStringGetCString(value, valueBuffer, valueLen, kCFStringEncodingUTF8) )
      valueString = valueBuffer;
    else {
      Error("Error (-1) Unable to convert value to C string", 0);
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
	  if (isprint(dataChar)) dataBuffer[cnt2++] = dataChar;
	  else {
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
      FatalError(-1, "Error (%d) getting the firmware variables", result);
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
    FatalError(-1, "Error (%d) clearing firmware variables", result);
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
          int nameLen;
          char *nameBuffer;
          char *nameString;

          // Get the variable's name.
          nameLen = CFStringGetLength(key) + 1;
          nameBuffer = malloc(nameLen);
          if( nameBuffer && CFStringGetCString(key, nameBuffer, nameLen, kCFStringEncodingUTF8) )
                  nameString = nameBuffer;
          else {
                  Error("Error (-1) Unable to convert property name to C string", 0);
                  nameString = "<UNPRINTABLE>";
          }
          FatalError(-1, "Error (-1) setting variable - '%s'", (long)nameString);
  }
}

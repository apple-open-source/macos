#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <mach-o/dyld.h>

#include "bless.h"
#include "compatCarbon.h"

#define CarbonCorePath "/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/CarbonCore"

static int isCarbonCoreLoaded = 0;

int loadCarbonCore() {
  if (!isCarbonCoreLoaded) {
    struct stat statbuf;
    char *suffix = getenv("DYLD_IMAGE_SUFFIX");
    char path[MAXPATHLEN];

    strcpy(path, CarbonCorePath);

    if (suffix) {
      strcat(path, suffix);
    }

    if (0 <= stat(path, &statbuf)) {
      NSAddLibrary(path);
      isCarbonCoreLoaded = 1;
    } else if(0 <= stat(CarbonCorePath, &statbuf)) {
      NSAddLibrary(CarbonCorePath);
      isCarbonCoreLoaded = 1;
    } else {
      isCarbonCoreLoaded = 0;
    }

  }

  return isCarbonCoreLoaded;

}

OSErr _bless_NativePathNameToFSSpec(char * A, FSSpec * B, long C) {
  OSErr (*dyfunc)(char *, FSSpec *, long) = NULL;

  if(!loadCarbonCore()) {
    return 1;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_NativePathNameToFSSpec", "CarbonCore")) {
    return 2;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_NativePathNameToFSSpec",
						    "CarbonCore"));
  return dyfunc(A, B, C);
}

OSErr _bless_FSMakeFSSpec(short A, long B, signed char * C, FSSpec * D) {
  OSErr (*dyfunc)(short, long, signed char *, FSSpec *) = NULL;

  if(!loadCarbonCore()) {
    return 1;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_FSMakeFSSpec", "CarbonCore")) {
    return 2;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_FSMakeFSSpec",
						    "CarbonCore"));
  return dyfunc(A, B, C, D);
}


short _bless_FSpOpenResFile(const FSSpec *  A, SInt8 B) {
  short (*dyfunc)(const FSSpec *, SInt8) = NULL;

  if(!loadCarbonCore()) {
    return 1;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_FSpOpenResFile", "CarbonCore")) {
    return 2;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_FSpOpenResFile",
						    "CarbonCore"));
  return dyfunc(A, B);
}

Handle _bless_Get1Resource( FourCharCode A, short B)  {
  Handle (*dyfunc)(FourCharCode, short) = NULL;

  if(!loadCarbonCore()) {
    return NULL;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_Get1Resource", "CarbonCore")) {
    return NULL;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_Get1Resource",
						    "CarbonCore"));
  return dyfunc(A, B);
}

void _bless_DetachResource(Handle A)   {
  void (*dyfunc)(Handle) = NULL;

  if(!loadCarbonCore()) {
    return;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_DetachResource", "CarbonCore")) {
    return;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_DetachResource",
						    "CarbonCore"));
  return dyfunc(A);
}

void _bless_DisposeHandle(Handle A)   {
  void (*dyfunc)(Handle) = NULL;

  if(!loadCarbonCore()) {
    return;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_DisposeHandle", "CarbonCore")) {
    return;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_DisposeHandle",
						    "CarbonCore"));
  dyfunc(A);
}

void _bless_CloseResFile(short A)  {
  void (*dyfunc)(short) = NULL;

  if(!loadCarbonCore()) {
    return;
  }
  
  if(!NSIsSymbolNameDefinedWithHint("_CloseResFile", "CarbonCore")) {
    return;
  }

  dyfunc =
    NSAddressOfSymbol(NSLookupAndBindSymbolWithHint("_CloseResFile",
						    "CarbonCore"));
  dyfunc(A);
}

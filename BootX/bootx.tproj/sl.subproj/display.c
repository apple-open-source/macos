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
 *  display.c - Functions to manage and find displays.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

#include "clut.h"

#include "bad_system.h"

#if kMacOSXServer
#include "images.h"
#else
#include "happy_mac.h"
#include "happy_foot.h"
#endif

struct DisplayInfo {
  CICell screenPH;
  CICell screenIH;
  CICell address;
  CICell width;
  CICell height;
  CICell depth;
  CICell linebytes;
};

typedef struct DisplayInfo DisplayInfo, *DisplayInfoPtr;

// The Driver Description
enum {
  kInitialDriverDescriptor	= 0,
  kVersionOneDriverDescriptor	= 1,
  kTheDescriptionSignature	= 'mtej',
  kDriverDescriptionSignature	= 'pdes'
};

struct DriverType {
  unsigned char nameInfoStr[32]; // Driver Name/Info String
  unsigned long	version;         // Driver Version Number - really NumVersion
};
typedef struct DriverType DriverType;

struct DriverDescription {
  unsigned long driverDescSignature; // Signature field of this structure
  unsigned long driverDescVersion;   // Version of this data structure
  DriverType    driverType;          // Type of Driver
  char          otherStuff[512];
};
typedef struct DriverDescription DriverDescription;

#define kNumNetDrivers (7)

char *gNetDriverFileNames[kNumNetDrivers] = {
  "ATYRagePro_ndrv",
  "Spinnaker_ndrv",
  "ATYLT-G_ndrv",
  "ATYLTPro_ndrv",
  "chips65550_ndrv",
  "control_ndrv",
  "ATYRage_ndrv"
};

char *gNetDriverMatchNames[kNumNetDrivers] = {
  "ATY,mach64_3DUPro",
  "ATY,mach64",
  "ATY,264LT-G",
  "ATY,RageLTPro",
  "chips65550",
  "control",
  "ATY,mach64_3DU"
};

static long FindDisplays(void);
static long OpenDisplays(void);
static void DumpDisplaysInfo(void);
static long OpenDisplay(long displayNum);
static long InitDisplay(long displayNum);
static long NetLoadDrivers(void);
static long DiskLoadDrivers(void);
static long LoadDisplayDriver(char *fileSpec);
static long LookUpCLUTIndex(long index, long depth);

static long        gNumDisplays;
static long        gMainDisplayNum;
static DisplayInfo gDisplays[16];

// Public Functions

long InitDisplays(void)
{
  FindDisplays();
  OpenDisplays();
  
  return 0;
}


long LoadDisplayDrivers(void)
{
  long ret;
  
  // Don't bother if there are no displays.
  if (gNumDisplays == 0) return 0;
  
  ret = NetLoadDrivers();
  
#if 0
  switch (gBootFileType) {
  case kNetworkDeviceType :
    ret = NetLoadDrivers();
    break;
    
  case kBlockDeviceType :
    ret = DiskLoadDrivers();
    break;
    
  case kUnknownDeviceType :
    ret = 0;
  }
#endif
  
  return ret;
}


long DrawSplashScreen(void)
{
  DisplayInfoPtr display;
  unsigned char  *happyMac, *happyFoot;
  short          *happyMac16, *happyFoot16;
  long           *happyMac32, *happyFoot32;
  long           cnt, x, y, pixelSize;
  
  if (gMainDisplayNum == -1) return 0;
  
  display = &gDisplays[gMainDisplayNum];
  
  // Make sure the boot display is marked.
  SetProp(display->screenPH, "AAPL,boot-display", NULL, 0);
  
#if kMacOSXServer
  x = (display->width - BIG_WIDTH) / 2;
  y = ((display->height - BIG_HEIGHT)) / 2 + BIG_DY;
  
  CallMethod_5_0(display->screenIH, "draw-rectangle", (long)bigImage,
		 x, y, BIG_WIDTH, BIG_HEIGHT);
  
  x = (display->width - SPIN_WIDTH) / 2;
  y = ((display->height - SPIN_WIDTH) / 2) + 28;
  
  // Set up the spin cursor.
  SpinInit(0, display->screenIH, waitCursors,
	   x, y, SPIN_WIDTH, SPIN_WIDTH);
  
  // Do a spin to start things off.
  Spin();
  
#else
  
  switch (display->depth) {
  case 16 :
    happyMac16 = malloc(kHappyMacWidth * kHappyMacHeight * 2);
    for (cnt = 0; cnt < (kHappyMacWidth * kHappyMacHeight); cnt++)
      happyMac16[cnt] = LookUpCLUTIndex(gHappyMacIcon[cnt], 16);
    happyMac = (char *)happyMac16;
    break;
    
  case 32 :
    happyMac32 = malloc(kHappyMacWidth * kHappyMacHeight * 4);
    for (cnt = 0; cnt < (kHappyMacWidth * kHappyMacHeight); cnt++)
      happyMac32[cnt] = LookUpCLUTIndex(gHappyMacIcon[cnt], 32);
    happyMac = (char *)happyMac32;
    break;
    
  default :
    happyMac = gHappyMacIcon;
    break;
  }
  
  x = (display->width - kHappyMacWidth) / 2;
  y = (display->height - kHappyMacHeight) / 2;
  
  CallMethod_5_0(display->screenIH, "draw-rectangle", (long)happyMac,
		 x, y, kHappyMacWidth, kHappyMacHeight);
  
  if (gBootFileType != kNetworkDeviceType) {
    SpinInit(0, 0, NULL, 0, 0, 0, 0, 0);
  } else {
    Interpret_1_0("ms", 1000);
    
    switch (display->depth) {
    case 16 :
      pixelSize = 2;
      happyFoot16 = malloc(kHappyFootWidth * kHappyFootHeight * 2);
      for (cnt = 0; cnt < (kHappyFootWidth * kHappyFootHeight); cnt++)
	happyFoot16[cnt] = LookUpCLUTIndex(gHappyFootPict[cnt], 16);
      happyFoot = (char *)happyFoot16;
      break;
      
    case 32 :
      pixelSize = 4;
      happyFoot32 = malloc(kHappyFootWidth * kHappyFootHeight * 4);
      for (cnt = 0; cnt < (kHappyFootWidth * kHappyFootHeight); cnt++)
	happyFoot32[cnt] = LookUpCLUTIndex(gHappyFootPict[cnt], 32);
      happyFoot = (char *)happyFoot32;
      break;
      
    default :
      pixelSize = 1;
      happyFoot = gHappyFootPict;
      break;
    }
    
    for (cnt = 0; cnt < kHappyFootHeight - 1; cnt++) {
      
      CallMethod_5_0(display->screenIH, "draw-rectangle", (long)happyMac,
		     x, y - cnt, kHappyMacWidth, kHappyMacHeight);
      
      CallMethod_5_0(display->screenIH, "draw-rectangle",
		     (long)happyFoot + pixelSize *
		     kHappyFootWidth * (kHappyFootHeight - cnt - 1),
		     x + 6, y + kHappyMacHeight - 1 - cnt,
		     kHappyFootWidth, cnt + 1);
      
      CallMethod_5_0(display->screenIH, "draw-rectangle",
		     (long)happyFoot + pixelSize *
		     kHappyFootWidth * (kHappyFootHeight - cnt - 1),
		     x + 15, y + kHappyMacHeight - 1 - cnt,
		     kHappyFootWidth, cnt + 1);
      
      Interpret_1_0("ms", 75);
    }
    
    // Set up the spin cursor.
    SpinInit(1, display->screenIH, happyFoot,
	     x + 15, y + kHappyMacHeight - kHappyFootHeight + 1,
	     kHappyFootWidth, kHappyFootHeight, pixelSize);
  }
#endif
  
  return 0;
}


long DrawBrokenSystemFolder(void)
{
  long           cnt;
  unsigned char  *iconPtr, tmpIcon[1024];
  short          *icon16;
  long           *icon32;
  DisplayInfoPtr display = &gDisplays[gMainDisplayNum];
  
  long x, y;
  
#if kMacOSXServer
  // Set the screen to Medium Blue
  CallMethod_5_0(display->screenIH, "fill-rectangle", 128, 0, 0,
		 display->width, display->height);
  
  // Use the default icon.
  iconPtr = gBrokenSystemFolderIcon;
#else
  // Set the screen to Medium Grey
  CallMethod_5_0(display->screenIH, "fill-rectangle",
		 LookUpCLUTIndex(0xF9, display->depth),
		 0, 0, display->width, display->height);
  
  // Convert the default icon.
  for (cnt = 0; cnt < 1024; cnt++) {
    tmpIcon[cnt] = gBrokenSystemFolderIcon[cnt];
    if (tmpIcon[cnt] == 0x80) tmpIcon[cnt] = 0xF9;
  }
  iconPtr = tmpIcon;
#endif
  
  switch (display->depth) {
  case 16 :
    icon16 = malloc(32 * 32 * 2);
    for (cnt = 0; cnt < (32 * 32); cnt++)
      icon16[cnt] = LookUpCLUTIndex(iconPtr[cnt], 16);
    iconPtr = (char *)icon16;
    break;
    
  case 32 :
    icon32 = malloc(32 * 32 * 4);
    for (cnt = 0; cnt < (32 * 32); cnt++)
      icon32[cnt] = LookUpCLUTIndex(iconPtr[cnt], 32);
    iconPtr = (char *)icon32;
    break;
    
  default :
    break;
  }
  
  // Draw the broken system folder.
  x = (display->width - 32) / 2;
  y = ((display->height - 32)) / 2;
  CallMethod_5_0(display->screenIH, "draw-rectangle",
		 (long)iconPtr, x, y, 32, 32);
  
  return 0;
}


void GetMainScreenPH(Boot_Video_Ptr video)
{
  DisplayInfoPtr display;
  
  if (gMainDisplayNum == -1) {
    // No display, set it to zero.
    video->v_baseAddr = 0;
    video->v_rowBytes = 0;
    video->v_width = 0;
    video->v_height = 0;
    video->v_depth = 0;
  } else {
    display = &gDisplays[gMainDisplayNum];
    
    video->v_baseAddr = display->address;
    video->v_rowBytes = display->linebytes;
    video->v_width = display->width;
    video->v_height = display->height;
    video->v_depth = display->depth;
  }
}

// Private Functions

static long FindDisplays(void)
{
  CICell screenPH, controlPH;
  long   cnt;
  
  // Find all the screens in the system.
  screenPH = 0;
  while (1) {
    screenPH = SearchForNode(screenPH, 1, "device_type", "display");
    if (screenPH != 0) gDisplays[gNumDisplays++].screenPH = screenPH;
    else break;
  }
  
  // Find /chaos/control, and
  // invalidate gStdOutPH if equal (since new OF was downloaded).
  controlPH = FindDevice("/chaos/control");
  if (gStdOutPH == controlPH) gStdOutPH = 0;
  
  // Find the main screen using the screen alias or chaos/control.
  gMainDisplayNum = -1;
  screenPH = FindDevice("screen");
  if (screenPH == -1) screenPH = controlPH;
  for (cnt = 0; cnt < gNumDisplays; cnt++)
    if (gDisplays[cnt].screenPH == screenPH) gMainDisplayNum = cnt;
  
  return 0;
}


static long OpenDisplays(void)
{
  long cnt;
  
  // Open the main screen or
  // look for a main screen if we don't have one.
  if ((gMainDisplayNum == -1) || !OpenDisplay(gMainDisplayNum)) {
    gMainDisplayNum = -1;
    for (cnt = 0; cnt < gNumDisplays; cnt++) {
      if (OpenDisplay(cnt)) {
	gMainDisplayNum = cnt;
	break;
      }
    }
  }
  
  return 0;
}

static void DumpDisplaysInfo(void)
{
  long cnt, length;
  char tmpStr[512];
  
  printf("gNumDisplays: %x, gMainDisplayNum: %x\n",
	 gNumDisplays, gMainDisplayNum);
  
  for (cnt = 0; cnt < gNumDisplays; cnt++) {
    printf("Display: %x, screenPH: %x,  screenIH: %x\n",
	   cnt, gDisplays[cnt].screenPH, gDisplays[cnt].screenIH);
    
    if (gDisplays[cnt].screenPH) {
      length = PackageToPath(gDisplays[cnt].screenPH, tmpStr, 511);
      tmpStr[length] = '\0';
      printf("PHandle Path: %s\n", tmpStr);
    }
    
    if (gDisplays[cnt].screenIH) {
      length = InstanceToPath(gDisplays[cnt].screenIH, tmpStr, 511);
      tmpStr[length] = '\0';
      printf("IHandle Path: %s\n", tmpStr);
    }
    
    printf("address = %x\n", gDisplays[cnt].address);
    printf("linebytes = %x\n", gDisplays[cnt].linebytes);
    printf("width = %x\n", gDisplays[cnt].width);
    printf("height = %x\n", gDisplays[cnt].height);
    printf("depth = %x\n", gDisplays[cnt].depth);
    printf("\n");
  }
}


static long OpenDisplay(long displayNum)
{
  char   screenPath[256];
  CICell screenIH;
  long   ret;
  
  // Try to use mac-boot's ihandle.
  Interpret_0_1("\" _screen-ihandle\" $find if execute else 0 then",
		&screenIH);
  
  // Try to use stdout as the screen's ihandle
  if (gStdOutPH == gDisplays[displayNum].screenPH) screenIH = gStdOutIH;
  
  // Try to open the display.
  if (screenIH == 0) {
    screenPath[255] = '\0';
    ret = PackageToPath(gDisplays[displayNum].screenPH, screenPath, 255);
    if (ret != -1) {
      screenIH = Open(screenPath);
    }
  }
  
  // Save the ihandle for later use.
  gDisplays[displayNum].screenIH = screenIH;
  
  // Initialize the display.
  if (screenIH != 0) InitDisplay(displayNum);
  
  return screenIH != 0;
}


static long InitDisplay(long displayNum)
{
  DisplayInfoPtr display = &gDisplays[displayNum];
  CICell         screenPH = display->screenPH;
  CICell         screenIH = display->screenIH;
  
  // Get the vital info for this screen.
  GetProp(screenPH, "address", (char *)&(display->address), 4);
  GetProp(screenPH, "width", (char *)&(display->width), 4);
  GetProp(screenPH, "height", (char *)&(display->height), 4);
  GetProp(screenPH, "depth", (char *)&(display->depth), 4);
  GetProp(screenPH, "linebytes", (char *)&(display->linebytes), 4);
  
  // Replace some of the drivers words.
  Interpret_3_1(
   " to active-package"
   " value rowbytes"
   " value depthbytes"
   
   " : rect-setup"      // ( adr|index x y w h -- w adr|index xy-adr h )
   "   >r >r rowbytes * swap depthbytes * + frame-buffer-adr +"
   "   r> depthbytes * -rot r>"
   " ;"
   
   " : DRAW-RECTANGLE"                          // ( adr x y w h -- )
   "   rect-setup"                              // ( w adr xy-adr h )
   "   0 ?do"                                   // ( w adr xy-adr )
   "     2dup 4 pick move"
   "     2 pick rowbytes d+"
   "   loop"
   "   3drop"
   " ;"
   
   " : FILL-RECTANGLE"                           // ( index x y w h -- )
   "   rect-setup rot depthbytes case"
   "     1 of dup 8 << or dup 10 << or endof"
   "     2 of dup 10 << or endof"
   "   endcase -rot 0 ?do"
   "     dup 3 pick 3 pick filll"
   "     rowbytes +"
   "   loop"
   "   3drop"
   " ;"
   
   " : READ-RECTANGLE"                            // ( adr x y w h -- )
   "   rect-setup  >r swap r> 0 ?do"
   "     2dup 4 pick move"
   "     rowbytes 3 pick d+"
   "   loop"
   "   3drop"
   " ;"
   
   " frame-buffer-adr"
   " 0 to active-package"
   , display->screenPH, display->linebytes,
   display->depth / 8, &display->address);
  
  // Set the CLUT for 8 bit displays
  if (display->depth == 8) {
    CallMethod_3_0(screenIH, "set-colors", (long)gClut, 0, 256);
  }
  
#if kMacOSXServer  
  // Set the screen to Medium Blue
  CallMethod_5_0(screenIH, "fill-rectangle", 128, 0, 0,
		 display->width, display->height);
#else
  // Set the screen to Medium Grey
  CallMethod_5_0(screenIH, "fill-rectangle",
		 LookUpCLUTIndex(0xF9, display->depth),
		 0, 0, display->width, display->height);
#endif
  
  return 0;
}


static long NetLoadDrivers(void)
{
  long   ret, cnt, curDisplay;
  CICell screenPH;
  char   fileSpec[512];
  
  for (cnt = 0; cnt < kNumNetDrivers; cnt++) {
    
    // See if there is a display for this driver.
    for (curDisplay = 0; curDisplay < gNumDisplays; curDisplay++) {
      screenPH = gDisplays[curDisplay].screenPH;
      if (MatchThis(screenPH, gNetDriverMatchNames[cnt]) == 0) break;
    }
    
    if (curDisplay == gNumDisplays) continue;
    
    sprintf(fileSpec, "%s%sDrivers\\ppc\\IONDRV.config\\%s",
	    gRootDir,
	    (gBootFileType == kNetworkDeviceType) ? "" : "private\\",
	    gNetDriverFileNames[cnt]);
    
    ret = LoadDisplayDriver(fileSpec);
  }
  
  return 0;
}

static long DiskLoadDrivers(void)
{
  long ret, flags, index, time;
  char dirSpec[512], *name;
  
  index = 0;
  while (1) {
    sprintf(dirSpec, "%sprivate\\Drivers\\ppc\\IONDRV.config\\", gRootDir);
    
    ret = GetDirEntry(dirSpec, &index, &name, &flags, &time);
    if (ret == -1) break;
    
    if (flags != kFlatFileType) continue;
    
    strcat(dirSpec, name);
    ret = LoadDisplayDriver(dirSpec);
    
    if (ret == -1) return -1;
  }
  
  return 0;
}

static long LoadDisplayDriver(char *fileSpec)
{
  char              *pef, *currentPef, *buffer;
  long              pefLen, currentPefLen, ndrvUsed;
  long              curDisplay;
  char              descripName[] = " TheDriverDescription";
  long              err;
  DriverDescription descrip;
  DriverDescription curDesc;
  char              matchName[40];
  unsigned long     newVersion;
  unsigned long     curVersion;
  CICell            screenPH;
  
  pefLen = LoadFile(fileSpec);
  if (pefLen == -1) return -1;
  if (pefLen == 0) return 0;
  
  pef = (char *)kLoadAddr;
  
  descripName[0] = strlen(descripName + 1);
  err = GetSymbolFromPEF(descripName, pef, &descrip, sizeof(descrip));
  if(err != 0) {
    printf("\nGetSymbolFromPEF returns %d\n",err);
    return -1;
  }
  if((descrip.driverDescSignature != kTheDescriptionSignature) ||
     (descrip.driverDescVersion != kInitialDriverDescriptor))
    return 0;
  
  strncpy(matchName, descrip.driverType.nameInfoStr + 1,
	  descrip.driverType.nameInfoStr[0]);
  newVersion = descrip.driverType.version;
  
  if((newVersion & 0xffff) == 0x8000)  // final stage, release rev
    newVersion |= 0xff;
  
  ndrvUsed = 0;
  buffer = (char *)malloc(pefLen);
  if (buffer == NULL) {
    printf("No space for the NDRV\n");
    return -1;
  }
  bcopy(pef, buffer, pefLen);
  
  for (curDisplay = 0; curDisplay < gNumDisplays; curDisplay++) {
    screenPH = gDisplays[curDisplay].screenPH;
    
    if (MatchThis(screenPH, matchName) != 0) continue;
    
    err = GetPackageProperty(screenPH, "driver,AAPL,MacOS,PowerPC",
			     &currentPef, &currentPefLen);
    
    if (err == 0) {
      err = GetSymbolFromPEF(descripName,currentPef,&curDesc,sizeof(curDesc));
      if (err != 0) {
	if((curDesc.driverDescSignature == kTheDescriptionSignature) &&
	   (curDesc.driverDescVersion == kInitialDriverDescriptor)) {
	  curVersion = curDesc.driverType.version;
	  if((curVersion & 0xffff) == 0x8000) // final stage, release rev
	    curVersion |= 0xff;

	  if( newVersion <= curVersion)
	    pefLen = 0;
	}
      }
    }
    
    if(pefLen == 0) continue;
    
    printf("Installing patch driver\n");
    
    SetProp(screenPH, "driver,AAPL,MacOS,PowerPC", buffer, pefLen);
    ndrvUsed = 1;
  }
  
  if (ndrvUsed == 0) free(buffer);
  
  return 0;
}


static long LookUpCLUTIndex(long index, long depth)
{
  long result, red, green, blue;
  
  red   = gClut[index * 3 + 0];
  green = gClut[index * 3 + 1];
  blue  = gClut[index * 3 + 2];
  
  switch (depth) {
  case 16 :
    result = ((red & 0xF8) << 7)|((green & 0xF8) << 2)|((blue & 0xF8) >> 3);
    break;
    
  case 32 :
    result = (red << 16) | (green << 8) | blue;
    break;
    
  default :
    result = index;
    break;
  }
  
  return result;
}

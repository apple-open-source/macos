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
 *  display.c - Functions to manage and find display.
 *
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include <IOKit/IOHibernatePrivate.h>

#include "clut.h"
#include "appleboot.h"
#include "failedboot.h"
#include "netboot.h"

struct DisplayInfo {
  CICell screenPH;
  CICell screenIH;
  CICell address;
  CICell width;
  CICell height;
  CICell depth;
  CICell linebytes;
  CICell triedToOpen;
};

typedef struct DisplayInfo DisplayInfo, *DisplayInfoPtr;


static long FindDisplays();
static long OpenDisplays(int fill);
static long OpenDisplay(long displayNum, int fill);
static long InitDisplay(long displayNum, int fill);
static long LookUpCLUTIndex(long index, long depth);

static long        gNumDisplays;
static long        gMainDisplayNum;
static DisplayInfo gDisplays[16];

static unsigned char *gAppleBoot;
static unsigned char *gNetBoot;
static unsigned char *gFailedBoot;

// Public Functions

long InitDisplays(int fill)
{
  FindDisplays();
  OpenDisplays(fill);
  
  return 0;
}

void CloseDisplays(void)
{
  int cnt;
  for (cnt = 0; cnt < gNumDisplays; cnt++) {
    if (gDisplays[cnt].screenIH)
      Close(gDisplays[cnt].screenIH);
  }
}

long DrawSplashScreen(long stage)
{
  DisplayInfoPtr display;
  short          *appleBoot16, *netBoot16;
  long           *appleBoot32, *netBoot32;
  long           cnt, x, y, pixelSize;
  
  if (gMainDisplayNum == -1) return 0;
  
  display = &gDisplays[gMainDisplayNum];
  
  switch (stage) {
  case 0 :
    // Make sure the boot display is marked.
    SetProp(display->screenPH, "AAPL,boot-display", NULL, 0);
    
    switch (display->depth) {
    case 16 :
      appleBoot16 =
	AllocateBootXMemory(kAppleBootWidth * kAppleBootHeight * 2);
      for (cnt = 0; cnt < (kAppleBootWidth * kAppleBootHeight); cnt++)
	appleBoot16[cnt] = LookUpCLUTIndex(gAppleBootPict[cnt], 16);
      gAppleBoot = (char *)appleBoot16;
      break;
      
    case 32 :
      appleBoot32 =
	AllocateBootXMemory(kAppleBootWidth * kAppleBootHeight * 4);
      for (cnt = 0; cnt < (kAppleBootWidth * kAppleBootHeight); cnt++)
	appleBoot32[cnt] = LookUpCLUTIndex(gAppleBootPict[cnt], 32);
      gAppleBoot = (char *)appleBoot32;
      break;
      
    default :
      gAppleBoot = (unsigned char *)gAppleBootPict;
      break;
    }
    
    x = (display->width - kAppleBootWidth) / 2;
    y = (display->height - kAppleBootHeight) / 2 + kAppleBootOffset;
    
    CallMethod(5, 0, display->screenIH, "draw-rectangle", (long)gAppleBoot,
	       x, y, kAppleBootWidth, kAppleBootHeight);
    
    if (gBootFileType != kNetworkDeviceType) {
      SpinInit(0, 0, NULL, 0, 0, 0, 0, 0, 0, 0);
    } else {
      switch (display->depth) {
      case 16 :
	pixelSize = 2;
	netBoot16 =
	  AllocateBootXMemory(kNetBootWidth * kNetBootHeight * kNetBootFrames * 2);
	for (cnt = 0; cnt < (kNetBootWidth * kNetBootHeight * kNetBootFrames); cnt++)
	  netBoot16[cnt] = LookUpCLUTIndex(gNetBootPict[cnt], 16);
	gNetBoot = (char *)netBoot16;
	break;
	
      case 32 :
	pixelSize = 4;
	netBoot32 =
	  AllocateBootXMemory(kNetBootWidth * kNetBootHeight * kNetBootFrames * 4);
	for (cnt = 0; cnt < (kNetBootWidth * kNetBootHeight * kNetBootFrames); cnt++)
	  netBoot32[cnt] = LookUpCLUTIndex(gNetBootPict[cnt], 32);
	gNetBoot = (char *)netBoot32;
	break;
	
      default :
	pixelSize = 1;
	gNetBoot = (unsigned char *)gNetBootPict;
	break;
      }
      
      x = (display->width - kNetBootWidth) / 2;
      y = (display->height - kNetBootHeight) / 2 + kNetBootOffset;
      
      CallMethod(5, 0, display->screenIH, "draw-rectangle", (long)gNetBoot,
		 x, y, kNetBootWidth, kNetBootHeight);
      
      // Set up the spin cursor.
      SpinInit(display->screenIH, gNetBoot,
	       x, y,
	       kNetBootWidth, kNetBootHeight,
	       kNetBootFrames, kNetBootFPS, pixelSize, 0);
    }
    break;
    
  case 1 :
    x = (display->width - kAppleBootWidth) / 2;
    y = (display->height - kAppleBootHeight) / 2 + kAppleBootOffset;
    
    CallMethod(5, 0, display->screenIH, "draw-rectangle", (long)gAppleBoot,
	       x, y, kAppleBootWidth, kAppleBootHeight);
    
    if (gBootFileType == kNetworkDeviceType) {
      x = (display->width - kNetBootWidth) / 2;
      y = (display->height - kNetBootHeight) / 2 + kNetBootOffset;
      
      // Erase the netboot picture with 75% grey.
      CallMethod(5, 0, display->screenIH, "fill-rectangle",
		 LookUpCLUTIndex(0x01, display->depth),
		 x, y, kNetBootWidth, kNetBootHeight);
    }
    break;
    
  default :
    return -1;
    break;
  }
  
  return 0;
}

DECLARE_IOHIBERNATEPROGRESSALPHA

void SplashPreview(void *src, uint8_t * saveunder, uint32_t savelen)
{
  DisplayInfoPtr display;
  uint8_t *  screen;
  uint32_t   rowBytes, pixelShift;
  uint32_t   x, y;
  int32_t    blob;
  uint32_t   alpha, in, color, result;
  uint8_t *  out;
  uint32_t   saveindex[kIOHibernateProgressCount] = { 0 };
  
  if (InitDisplays(0) != 0)  return;
  if (gMainDisplayNum == -1) return;

  display = &gDisplays[gMainDisplayNum];
  screen = (uint8_t *) display->address;
  rowBytes = display->linebytes;
  if (!src || !DecompressData(src, (void *) screen, 
                    display->width, display->height,
                    display->depth >> 3, rowBytes))
  {
    // Set the screen to 75% grey.
    CallMethod(5, 0, display->screenIH, "fill-rectangle",
            LookUpCLUTIndex(0x01, display->depth),
            0, 0, display->width, display->height);
    DrawSplashScreen(0);
  }

  pixelShift = display->depth >> 4;
  if (pixelShift < 1) return;

  screen += ((display->width 
          - kIOHibernateProgressCount * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << (pixelShift - 1))
              + (display->height - kIOHibernateProgressOriginY - kIOHibernateProgressHeight) * rowBytes;
  
  for (y = 0; y < kIOHibernateProgressHeight; y++)
  {
    out = screen + y * rowBytes;
    for (blob = 0; blob < kIOHibernateProgressCount; blob++)
    {
      color = blob ? kIOHibernateProgressDarkGray : kIOHibernateProgressMidGray;
      for (x = 0; x < kIOHibernateProgressWidth; x++)
      {
        alpha  = gIOHibernateProgressAlpha[y][x];
        result = color;
        if (alpha)
        {
          if (0xff != alpha)
          {
            if (1 == pixelShift)
            {
              in = *((uint16_t *)out) & 0x1f;	// 16
              in = (in << 3) | (in >> 2);
            }
            else
                in = *((uint32_t *)out) & 0xff;	// 32
            saveunder[blob * kIOHibernateProgressSaveUnderSize + saveindex[blob]++] = in;
            result = ((255 - alpha) * in + alpha * result + 0xff) >> 8;
          }
          if (1 == pixelShift)
          {
            result >>= 3;
            *((uint16_t *)out) = (result << 10) | (result << 5) | result;	// 16
          }
          else
            *((uint32_t *)out) = (result << 16) | (result << 8) | result;	// 32
        }
        out += (1 << pixelShift);
      }
      out += (kIOHibernateProgressSpacing << pixelShift);
    }
  }
}

void SplashProgress(uint8_t * saveunder, int32_t firstBlob, int32_t select)
{
  DisplayInfoPtr display;
  uint8_t * screen;
  uint32_t  rowBytes, pixelShift;
  uint32_t  x, y;
  int32_t   blob, lastBlob;
  uint32_t  alpha, in, color, result;
  uint8_t * out;
  uint32_t  saveindex[kIOHibernateProgressCount] = { 0 };

  if (gMainDisplayNum == -1) return;

  display = &gDisplays[gMainDisplayNum];
  pixelShift = display->depth >> 4;
  if (pixelShift < 1) return;
  screen = (uint8_t *) display->address;
  rowBytes = display->linebytes;

  screen += ((display->width 
          - kIOHibernateProgressCount * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << (pixelShift - 1))
              + (display->height - kIOHibernateProgressOriginY - kIOHibernateProgressHeight) * rowBytes;

  lastBlob  = (select < kIOHibernateProgressCount) ? select : (kIOHibernateProgressCount - 1);

  screen += (firstBlob * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << pixelShift;

  for (y = 0; y < kIOHibernateProgressHeight; y++)
  {
    out = screen + y * rowBytes;
    for (blob = firstBlob; blob <= lastBlob; blob++)
    {
      color = (blob < select) ? kIOHibernateProgressLightGray : kIOHibernateProgressMidGray;
      for (x = 0; x < kIOHibernateProgressWidth; x++)
      {
        alpha  = gIOHibernateProgressAlpha[y][x];
        result = color;
        if (alpha)
        {
          if (0xff != alpha)
          {
            in = saveunder[blob * kIOHibernateProgressSaveUnderSize + saveindex[blob]++];
            result = ((255 - alpha) * in + alpha * result + 0xff) / 255;
          }
          if (1 == pixelShift)
          {
            result >>= 3;
            *((uint16_t *)out) = (result << 10) | (result << 5) | result;	// 16
          }
          else
            *((uint32_t *)out) = (result << 16) | (result << 8) | result;	// 32
        }
        out += (1 << pixelShift);
      }
      out += (kIOHibernateProgressSpacing << pixelShift);
    }
  }
}

long DrawFailedBootPicture(void)
{
  long           cnt;
  short          *failedBoot16;
  long           *failedBoot32, posX, posY;
  DisplayInfoPtr display = &gDisplays[gMainDisplayNum];
  
  switch (display->depth) {
  case 16 :
    failedBoot16 = AllocateBootXMemory(32 * 32 * 2);
    for (cnt = 0; cnt < (32 * 32); cnt++)
      failedBoot16[cnt] = LookUpCLUTIndex(gFailedBootPict[cnt], 16);
    gFailedBoot = (char *)failedBoot16;
    break;
    
  case 32 :
    failedBoot32 = AllocateBootXMemory(32 * 32 * 4);
    for (cnt = 0; cnt < (32 * 32); cnt++)
      failedBoot32[cnt] = LookUpCLUTIndex(gFailedBootPict[cnt], 32);
    gFailedBoot = (char *)failedBoot32;
    break;
    
  default :
    gFailedBoot = (unsigned char *)gFailedBootPict;
    break;
  }
  
  // Erase the newboot picture with 75% grey.
  posX = (display->width - kNetBootWidth) / 2;
  posY = (display->height - kNetBootHeight) / 2 + kNetBootOffset;
  CallMethod(5, 0, display->screenIH, "fill-rectangle",
	     LookUpCLUTIndex(0x01, display->depth),
	     posX, posY, kNetBootWidth, kNetBootHeight);
  
  // Draw the failed boot picture.
  posX = (display->width - kFailedBootWidth) / 2;
  posY = ((display->height - kFailedBootHeight)) / 2 + kFailedBootOffset;
  CallMethod(5, 0, display->screenIH, "draw-rectangle",
	     (long)gFailedBoot, posX, posY,
	     kFailedBootWidth, kFailedBootHeight);
  
  return 0;
}


void GetMainScreenPH(Boot_Video_Ptr video, int setProperties)
{
  DisplayInfoPtr display;
  long           address, size;
  
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

  if (!setProperties) return;

  size = 256 * 3;
  // Allocate memory and a range for the CLUT.
  address = AllocateKernelMemory(size);
  AllocateMemoryRange("BootCLUT", address, size);
  bcopy((char *)gClut, (char *)address, size);
  
  // Allocate memory and a range for the failed boot picture.
  size = 32 + kFailedBootWidth * kFailedBootHeight;
  address = AllocateKernelMemory(size);
  AllocateMemoryRange("Pict-FailedBoot", address, size);
  ((long *)address)[0] = kFailedBootWidth;
  ((long *)address)[1] = kFailedBootHeight;
  ((long *)address)[2] = kFailedBootOffset;
  bcopy((char *)gFailedBootPict, (char *)(address + 32), size - 32);
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
  gDisplays[gNumDisplays++].screenPH = screenPH;
  if (screenPH == -1) screenPH = controlPH;
  for (cnt = 0; cnt < gNumDisplays; cnt++)
    if (gDisplays[cnt].screenPH == screenPH) gMainDisplayNum = cnt;
  
  return 0;
}


static long OpenDisplays(int fill)
{
  long cnt;
  
  // Open the main screen or
  // look for a main screen if we don't have one.
  if ((gMainDisplayNum == -1) || !OpenDisplay(gMainDisplayNum, fill)) {
    gMainDisplayNum = -1;
    for (cnt = 0; cnt < gNumDisplays; cnt++) {
      if (OpenDisplay(cnt, fill)) {
	gMainDisplayNum = cnt;
	break;
      }
    }
  }
  
  // Open the rest of the displays
  if (gOFVersion >= kOFVersion3x) {
    for (cnt = 0; cnt < gNumDisplays; cnt++) {
      OpenDisplay(cnt, 1);
    }
  }
  
  return 0;
}


static long OpenDisplay(long displayNum, int fill)
{
  char   screenPath[258], displayType[32];
  CICell screenPH, screenIH;
  long   ret, size;
  
  // Only try to open a screen once.
  if (gDisplays[displayNum].triedToOpen) {
    return gDisplays[displayNum].screenIH != 0;
  } else {
    gDisplays[displayNum].triedToOpen = -1;
  }
  
  screenPH = gDisplays[displayNum].screenPH;
  
  // Try to use mac-boot's ihandle.
  Interpret(0, 1, "\" _screen-ihandle\" $find if execute else 0 then",
	    &screenIH);
  if ((screenIH != 0) && (InstanceToPackage(screenIH) != screenPH)) {
    screenIH = 0;
  }
  
  // Try to use stdout as the screen's ihandle
  if ((screenIH == 0) && (gStdOutPH == screenPH)) {
    screenIH = gStdOutIH;
  }
  
  // Try to open the display.
  if (screenIH == 0) {
    screenPath[255] = '\0';
    ret = PackageToPath(screenPH, screenPath, 255);
    if (ret != -1) {
      strcat(screenPath, ":0");
      screenIH = Open(screenPath);
    }
  }
  
  // Find out what type of display is attached.
  size = GetProp(screenPH, "display-type", displayType, 31);
  if (size != -1) {
    displayType[size] = '\0';
    // If the display-type is NONE, don't use the display.
    if (!strcmp(displayType, "NONE")) screenIH = 0;
  }
  
  // Save the ihandle for later use.
  gDisplays[displayNum].screenIH = screenIH;
  
  // Initialize the display.
  if (screenIH != 0) InitDisplay(displayNum, fill);
  
  return screenIH != 0;
}


static long InitDisplay(long displayNum, int fill)
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
  Interpret(3, 1,
   " value depthbytes"
   " value rowbytes"
   " to active-package"
   " frame-buffer-adr value this-frame-buffer-adr"
   
   " : rect-setup"      // ( adr|index x y w h -- w adr|index xy-adr h )
   "   >r >r rowbytes * swap depthbytes * + this-frame-buffer-adr +"
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
   
   " this-frame-buffer-adr"
   " 0 to active-package"
   , display->screenPH, display->linebytes,
   display->depth / 8, &display->address);
  
  // Set the CLUT for 8 bit displays.
  if (display->depth == 8) {
    CallMethod(3, 0, screenIH, "set-colors", (long)gClut, 0, 256);
  }
  
  if (fill)
    // Set the screen to 75% grey.
    CallMethod(5, 0, screenIH, "fill-rectangle",
	     LookUpCLUTIndex(0x01, display->depth),
	     0, 0, display->width, display->height);
  
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


#if 0
static void DumpDisplaysInfo(void);

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
#endif

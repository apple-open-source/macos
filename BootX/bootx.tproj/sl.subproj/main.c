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
 *  main.c - Main functions for BootX.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */


#include <sl.h>

static void Start(void *unused1, void *unused2, ClientInterfacePtr ciPtr);
static void Main(ClientInterfacePtr ciPtr);
static long InitEverything(ClientInterfacePtr ciPtr);
static long DecodeKernel(void);
static long SetUpBootArgs(void);
static long CallKernel(void);
static void FailToBoot(long num);
static long InitMemoryMap(void);
static long GetOFVersion(void);
static long TestForKey(long key);
static long GetBootPaths(void);

const unsigned long StartTVector[2] = {(unsigned long)Start, 0};

char gStackBaseAddr[0x8000];

char *gVectorSaveAddr;
long gImageLastKernelAddr = 0;
long gImageFirstBootXAddr = kLoadAddr;
long gKernelEntryPoint;
long gDeviceTreeAddr;
long gDeviceTreeSize;
long gBootArgsAddr;
long gBootArgsSize;
long gSymbolTableAddr;
long gSymbolTableSize;

long gBootSourceNumber = -1;
long gBootSourceNumberMax;
long gBootDeviceType;
long gBootFileType;
char gBootDevice[256];
char gBootFile[256];
char gRootDir[256];

char gTempStr[4096];

long *gDeviceTreeMMTmp = 0;

long gOFVersion;

char *gKeyMap;

CICell gChosenPH;
CICell gOptionsPH;
CICell gScreenPH;
CICell gMemoryMapPH;
CICell gStdOutPH;

CICell gMMUIH;
CICell gMemoryIH;
CICell gStdOutIH;
CICell gKeyboardIH;

// Private Functions

static void Start(void *unused1, void *unused2, ClientInterfacePtr ciPtr)
{
  long newSP;
  
  // Move the Stack to a chunk of the BSS
  newSP = (long)gStackBaseAddr + sizeof(gStackBaseAddr) - 0x100;
  __asm__ volatile("mr r1, %0" : : "r" (newSP));
  
  Main(ciPtr);
}


static void Main(ClientInterfacePtr ciPtr)
{
  long ret;
  
  ret = InitEverything(ciPtr);
  if (ret != 0) Exit();
  
  // Get or infer the boot paths.
  ret = GetBootPaths();
  if (ret != 0) FailToBoot(1);
  
  DrawSplashScreen();
  
  while (ret == 0) {
    ret = LoadFile(gBootFile);
    if (ret != -1) break;
    
    ret = GetBootPaths();
    if (ret != 0) FailToBoot(2);
  }
  
  ret = DecodeKernel();
  if (ret != 0) FailToBoot(4);
  
  ret = LoadDrivers(gRootDir);
  if (ret != 0) FailToBoot(5);

#if 0    
  ret = LoadDisplayDrivers();
  if (ret != 0) FailToBoot(6);
#endif

  ret = SetUpBootArgs();
  if (ret != 0) FailToBoot(7);
  
  ret = CallKernel();
  
  FailToBoot(8);
}


static long InitEverything(ClientInterfacePtr ciPtr)
{
  long ret, mem_base, mem_base2, size;
  CICell keyboardPH;
  char name[32];
#if 0
  char defaultBootDevice[256];
#endif
  
  // Init the OF Client Interface.
  ret = InitCI(ciPtr);
  if (ret != 0) return -1;
  
  // Get the OF Version
  gOFVersion = GetOFVersion();
  if (gOFVersion == 0) return -1;
  
  // Init the SL Words package.
  ret = InitSLWords(gOFVersion);
  if (ret != 0) return -1;
  
  // Get the phandle for /options
  gOptionsPH = FindDevice("/options");
  if (gOptionsPH == -1) return -1;
  
  // Get the phandle for /chosen
  gChosenPH = FindDevice("/chosen");
  if (gChosenPH == -1) return -1;
  
  // Init the Memory Map.
  ret = InitMemoryMap();
  if (ret != 0) return -1;
  
  // Get IHandles for the MMU and Memory
  size = GetProp(gChosenPH, "mmu", (char *)&gMMUIH, 4);
  if (size != 4) {
    printf("Failed to get the IH for the MMU.\n");
    return -1;
  }
  size = GetProp(gChosenPH, "memory", (char *)&gMemoryIH, 4);
  if (size != 4) {
    printf("Failed to get the IH for the Memory.\n");
    return -1;
  }
  
  // Get stdout's IH, so that the boot display can be found.
  ret = GetProp(gChosenPH, "stdout", (char *)&gStdOutIH, 4);
  if (ret == 4) gStdOutPH = InstanceToPackage(gStdOutIH);
  else gStdOutPH = gStdOutIH = 0;
  
  // Try to find the keyboard using chosen
  ret = GetProp(gChosenPH, "stdin", (char *)&gKeyboardIH, 4);
  if (ret != 4) gKeyboardIH = 0;
  else {
    keyboardPH = InstanceToPackage(gKeyboardIH);
    ret = GetProp(keyboardPH, "name", name, 31);
    if (ret != -1) {
      name[ret] = '\0';
      if (strcmp(name, "keyboard") && strcmp(name, "kbd")) gKeyboardIH = 0;
    } else gKeyboardIH = 0;
  }
  
  // Try to the find the keyboard using open if chosen did not work.
  if (gKeyboardIH == 0) gKeyboardIH = Open("keyboard");
  if (gKeyboardIH == 0) gKeyboardIH = Open("kbd");
  
  // Get the key map set up, and make it up to date.
  gKeyMap = InitKeyMap(gKeyboardIH);
  if (gKeyMap == NULL) return -1;
  UpdateKeyMap();
  
#if 0  
  // On OF 3.x, if the Option key was pressed,
  // set the default boot device and reboot.
  if (gOFVersion >= kOFVersion3x) {
    if (TestForKey(kOptKey)) {
      size = GetProp(gOptionsPH, "default-boot-device", defaultBootDevice,255);
      if (size == -1) {
	Interpret_0_0("set-default boot-device");
      } else {
	defaultBootDevice[size] = '\0';
	SetProp(gOptionsPH, "boot-device", defaultBootDevice, size);
	SetProp(gOptionsPH, "boot-file", 0, 0);
      }
      Interpret_0_0("reset-all");
    }
  }
#endif
  
#if kFailToBoot
  // 'cmd-s' or 'cmd-v' is pressed set outputLevel to kOutputLevelFull
  if (TestForKey(kCommandKey) && (TestForKey('s') || TestForKey('v')))
    SetOutputLevel(kOutputLevelFull);
  else SetOutputLevel(kOutputLevelOff);
#else
  SetOutputLevel(kOutputLevelFull);
#endif
  
  // printf now works.
  printf("\n\nMac OS X Loader\n");
  
  mem_base = Claim(kMallocAddr, kMallocSize, 0);
  if (mem_base == 0) {
    printf("Claim for malloc failed.\n");
    return -1;
  }
  malloc_init((char *)mem_base, kMallocSize);
  
  // malloc now works.
  
  // Claim the memory for the Load Addr
  mem_base = Claim(kLoadAddr, kLoadSize, 0);
  if (mem_base == 0) {
    printf("Claim for Load Area failed.\n");
    return -1;
  }
  
  // Claim the memory for the Image Addr
  if (gOFVersion >= kOFVersion3x) {
    mem_base = Claim(kImageAddr, kImageSize, 0);
    if (mem_base == 0) {
      printf("Claim for Image Area failed.\n");
      return -1;
    }
  } else {
    // Claim the 1:1 mapped chunks first.
    mem_base  = Claim(kImageAddr0, kImageSize0, 0);
    mem_base2 = Claim(kImageAddr2, kImageSize2, 0);
    if ((mem_base == 0) || (mem_base2 == 0)) {
      printf("Claim for Image Area failed.\n");
      return -1;
    }
    
    // Unmap the old xcoff stack.
    CallMethod_2_0(gMMUIH, "unmap", 0x00380000, 0x00080000);
    
    // Grap the physical memory then the logical.
    CallMethod_3_1(gMemoryIH, "claim",
		   kImageAddr1Phys, kImageSize1, 0, &mem_base);
    CallMethod_3_1(gMMUIH, "claim",
		   kImageAddr1, kImageSize1, 0, &mem_base2);
    if ((mem_base == 0) || (mem_base2 == 0)) {
      printf("Claim for Image Area failed.\n");
      return -1;
    }
    
    // Map them together.
    CallMethod_4_0(gMMUIH, "map",
		   kImageAddr1Phys, kImageAddr1, kImageSize1, 0);
  }
  
  bzero((char *)kImageAddr, kImageSize);
  
  // Malloc some space for the Vector Save area.
  gVectorSaveAddr = malloc(kVectorSize);
  if (gVectorSaveAddr == 0) {
    printf("Malloc for Vector Save Area failed.\n");
    return -1;
  }
  
  // Find all the displays and set them up.
  ret = InitDisplays();
  if (ret != 0) {
    printf("InitDisplays failed.\n");
    return -1;
  }
  
  return 0;
}


static long DecodeKernel(void)
{
  long ret;
  
  ret = DecodeMachO();
  if (ret == -1) ret = DecodeElf();
  
  return ret;
}


static long SetUpBootArgs(void)
{
  boot_args_ptr args;
  CICell        memoryPH;
  long          secure = 0, sym = 0, graphicsBoot = 1;
  long          ret, cnt, mem_size, size, dash;
  long          aKey, sKey, vKey, yKey, shiftKey, keyPos;
  char          ofBootArgs[128], *ofArgs, tc, keyStr[8], securityMode[33];
  
  // Save file system cache statistics.
  SetProp(gChosenPH, "BootXCacheHits", (char *)&gCacheHits, 4);
  SetProp(gChosenPH, "BootXCacheMisses", (char *)&gCacheMisses, 4);
  SetProp(gChosenPH, "BootXCacheEvicts", (char *)&gCacheEvicts, 4);
  
  // Allocate some memory for the BootArgs.
  gBootArgsSize = sizeof(boot_args);
  gBootArgsAddr = AllocateKernelMemory(gBootArgsSize);
  
  // Add the BootArgs to the memory-map.
  AllocateMemoryRange("BootArgs", gBootArgsAddr, gBootArgsSize);
  
  args = (boot_args_ptr)gBootArgsAddr;
  
  args->Revision = kBootArgsRevision;
  args->Version = kBootArgsVersion;
  args->machineType = 0;
  
  // Get the security-mode.
  size = GetProp(gOptionsPH, "security-mode", securityMode, 32);
  if (size != -1) {
    securityMode[size] = '\0';
    if (strcmp(securityMode, "none")) secure = 1;
  }
  
  // Check the Keyboard for 'a', 'cmd-s', 'cmd-v', 'y' and shift
  UpdateKeyMap();
  if (!secure) {
    aKey = TestForKey('a');
    sKey = TestForKey(kCommandKey) && TestForKey('s');
    vKey = TestForKey(kCommandKey) && TestForKey('v');
    yKey = TestForKey('y');
  } else {
    aKey = 0;
    sKey = 0;
    vKey = 0;
    yKey = 0;
  }
  shiftKey = TestForKey(kShiftKey);
  
  // if 'cmd-s' or 'cmd-v' was pressed do a text boot.
  if (sKey || vKey) graphicsBoot = 0;
  
  // if 'y' key was pressed send the symbols;
  if (yKey) sym = 1;
  
  // Create the command line.
  if (gOFVersion < kOFVersion3x) {
    ofBootArgs[0] = ' ';
    size = GetProp(gChosenPH, "machargs", ofBootArgs + 1, 126);
    if (size == -1) {
      size = GetProp(gOptionsPH, "boot-command", ofBootArgs, 127);
      if (size == -1) ofBootArgs[0] = '\0';
      else ofBootArgs[size] = '\0';
      // Look for " bootr" but skip the number.
      if (!strncmp(ofBootArgs + 1, " bootr", 6)) {
	strcpy(ofBootArgs, ofBootArgs + 7);
      } else ofBootArgs[0] = '\0';
      SetProp(gChosenPH, "machargs", ofBootArgs, strlen(ofBootArgs) + 1);
    } else ofBootArgs[size] = '\0';
    // Force boot-command to start with 0 bootr.
    sprintf(gTempStr, "0 bootr%s", ofBootArgs);
    SetProp(gOptionsPH, "boot-command", gTempStr, strlen(gTempStr));
  } else {
    size = GetProp(gOptionsPH, "boot-args", ofBootArgs, 127);
    if (size == -1) ofBootArgs[0] = '\0';
    else ofBootArgs[size] = '\0';
  }
  
  if (ofBootArgs[0] != '\0') {
    // Look for special options and copy the rest.
    dash = 0;
    ofArgs = ofBootArgs;
    while ((tc = *ofArgs) != '\0') { 
      tc = tolower(tc);
      
      // Check for entering a dash arg.
      if (tc == '-') {
	dash = 1;
	ofArgs++;
	continue;
      }
      
      // Do special stuff if in a dash arg.
      if (dash) {
	if (tc == 'a') {
	  ofArgs++;
	  aKey = 0;
	}
	else if (tc == 's') {
	  graphicsBoot = 0;
	  ofArgs++;
	  sKey = 0;
	}
	else if (tc == 'v') {
	  graphicsBoot = 0;
	  ofArgs++;
	  vKey = 0;
	}
	else if (tc == 'x') {
	  ofArgs++;
	  shiftKey = 0;
	}
	else if (tc == 'y') {
	  sym = 1;
	  ofArgs++;
	  yKey = 0;
	}
	else {
	  // Check for exiting dash arg
	  if (isspace(tc)) dash = 0;
	  
	  // Copy any non 'a', 's', 'v', 'x' or 'y'
	  ofArgs++;
	}
      } else {
	// Not a dash arg so just copy it.
	ofArgs++;
      }    
    }
  }
  
  // Add any pressed keys (a, s, v, y, shift) to the command line
  keyPos = 0;
  if (aKey || sKey || vKey || yKey || shiftKey) {
    keyStr[keyPos++] = '-';
    
    if (aKey) keyStr[keyPos++] = 'a';
    if (sKey) keyStr[keyPos++] = 's';
    if (vKey) keyStr[keyPos++] = 'v';
    if (yKey) keyStr[keyPos++] = 'y';
    if (shiftKey) keyStr[keyPos++] = 'x';
    
    keyStr[keyPos++] = ' ';
  }
  keyStr[keyPos++] = '\0';
  
  // Send symbols?
  if (!sym && !yKey) gSymbolTableAddr = 0;
  
  sprintf(args->CommandLine, "%s%s symtab=%d",
	  keyStr, ofBootArgs, gSymbolTableAddr);
  
  // Get the memory info
  memoryPH = FindDevice("/memory");
  if (memoryPH == -1) return -1;
  size = GetProp(memoryPH, "reg", (char *)(args->PhysicalDRAM),
		 kMaxDRAMBanks * sizeof(DRAMBank));
  if (size == 0) return -1;
  
  // This is a hack to make the memory look like its all
  // in one big bank.
  mem_size = 0;
  for (cnt = 0; cnt < kMaxDRAMBanks; cnt++) {
    mem_size += args->PhysicalDRAM[cnt].size;
    args->PhysicalDRAM[cnt].base = 0;
    args->PhysicalDRAM[cnt].size = 0;
  }
  args->PhysicalDRAM[0].size = mem_size;      
  
  // Get the video info
  GetMainScreenPH(&args->Video);
  args->Video.v_display  = graphicsBoot;
  
  // Add the DeviceTree to the memory-map.
  // The actuall address and size must be filled in later.
  AllocateMemoryRange("DeviceTree", 0, 0);
  
  ret = FlattenDeviceTree();
  if (ret != 0) return -1;
  
  // Fill in the address and size of the device tree.
  if (gDeviceTreeAddr) {
    gDeviceTreeMMTmp[0] = gDeviceTreeAddr;
    gDeviceTreeMMTmp[1] = gDeviceTreeSize;
  }
  
  args->deviceTreeP = (void *)gDeviceTreeAddr;
  args->deviceTreeLength = gDeviceTreeSize;
  args->topOfKernelData = AllocateKernelMemory(0);
  
  return 0;
}


static long CallKernel(void)
{
  long msr, cnt;
  
  Quiesce();
  
  printf("\nCall Kernel!\n");
  
  msr = 0x00001000;
  __asm__ volatile("mtmsr %0" : : "r" (msr));
  __asm__ volatile("isync");
  
  // Move the Execption Vectors
  bcopy(gVectorSaveAddr, 0x0, kVectorSize);
  for (cnt = 0; cnt < kVectorSize; cnt += 0x20) {
    __asm__ volatile("dcbf 0, %0" : : "r" (cnt));
    __asm__ volatile("icbi 0, %0" : : "r" (cnt));
  }
  
  // Move the Image1 save area for OF 1.x / 2.x
  if (gOFVersion < kOFVersion3x) {
    bcopy((char *)kImageAddr1Phys, (char *)kImageAddr1, kImageSize1);
    for (cnt = kImageAddr1; cnt < kImageSize1; cnt += 0x20) {
      __asm__ volatile("dcbf 0, %0" : : "r" (cnt));
      __asm__ volatile("icbi 0, %0" : : "r" (cnt));
    }
  }
  
  // Make sure everything get sync'd up.
  __asm__ volatile("isync");
  __asm__ volatile("sync");
  __asm__ volatile("eieio");
  
  (*(void (*)())gKernelEntryPoint)(gBootArgsAddr, kMacOSXSignature);
  
  return -1;
}


static void FailToBoot(long num)
{
#if kFailToBoot
  DrawBrokenSystemFolder();
  while (1);
  num = 0;
#else
  printf("FailToBoot: %d\n", num);
  Enter(); // For debugging
#endif
}


static long InitMemoryMap(void)
{
  long result;
  
  result = Interpret_0_1(
			 " dev /chosen"
			 " new-device"
			 " \" memory-map\" device-name"
			 " active-package"
			 " device-end"
			 , &gMemoryMapPH);
  
  return result;
}


static long GetOFVersion(void)
{
  CICell ph;
  char   versStr[256], *tmpStr;
  long   vers, size;
  
  // Get the openprom package
  ph = FindDevice("/openprom");
  if (ph == -1) return 0;
  
  // Get it's model property
  size = GetProp(ph, "model", versStr, 255);
  if (size == -1) return -1;
  versStr[size] = '\0';
  
  // Find the start of the number.
  tmpStr = NULL;
  if (!strncmp(versStr, "Open Firmware, ", 15)) {
    tmpStr = versStr + 15;
  } else if (!strncmp(versStr, "OpenFirmware ", 13)) {
    tmpStr = versStr + 13;
  } else return -1;  
  
  // Clasify by each instance as needed...
  switch (*tmpStr) {
  case '1' :
    vers = kOFVersion1x;
    break;
    
  case '2' :
    vers = kOFVersion2x;
    break;
    
  case '3' :
    vers = kOFVersion3x;
    break;
    
  default :
    vers = 0;
    break;
  }

  return vers;
}


static long TestForKey(long key)
{
  long keyNum;
  long bp;
  char tc;
  
  if (gOFVersion < kOFVersion3x) {
    switch(key) {
    case 'a' :         keyNum =   7; break;
    case 's' :         keyNum =   6; break;
    case 'v' :         keyNum =  14; break;
    case 'y' :         keyNum =  23; break;
    case kCommandKey : keyNum =  48; break;
    case kOptKey     : keyNum =  61; break;
    case kShiftKey   : keyNum =  63; break;
    case kControlKey : keyNum =  49; break;
    default : keyNum = -1; break;
    }
  } else {
    switch(key) {
    case 'a' :         keyNum =   3; break;
    case 's' :         keyNum =  17; break;
    case 'v' :         keyNum =  30; break;
    case 'y' :         keyNum =  27; break;
    case kCommandKey : keyNum = 228; break;
    case kOptKey     : keyNum = 229; break;
    case kShiftKey   : keyNum = 230; break;
    case kControlKey : keyNum = 231; break;
    default : keyNum = -1; break;
    }
    
    // Map the right modifier keys on to the left.
    gKeyMap[28] |= gKeyMap[28] << 4;
  }
  
  if (keyNum == -1) return 0;
  
  bp = keyNum & 7;
  tc = gKeyMap[keyNum >> 3];
  
  return (tc & (1 << bp)) != 0;
}


#define kBootpBootFileOffset (108)

static long GetBootPaths(void)
{
  long ret, cnt, cnt2, cnt3, cnt4, size, partNum, bootplen, bsdplen;
  char *filePath, *buffer;
  
  if (gBootSourceNumber == -1) {
    // Get the boot-device
    size = GetProp(gChosenPH, "bootpath", gBootDevice, 255);
    gBootDevice[size] = '\0';
    if (gBootDevice[0] == '\0') {
      size = GetProp(gOptionsPH, "boot-device", gBootDevice, 255);
      gBootDevice[size] = '\0';
    }
    gBootDeviceType = GetDeviceType(gBootDevice);
    
    // Get the boot-file
    size = GetProp(gChosenPH, "bootargs", gBootFile, 256);
    gBootFile[size] = '\0';
    
    if (gBootFile[0] != '\0') {
      gBootFileType = GetDeviceType(gBootFile);
      gBootSourceNumberMax = 0;
    } else {
      gBootSourceNumber = 0;
      gBootFileType = gBootDeviceType;
      if (gBootFileType == kNetworkDeviceType) gBootSourceNumberMax = 1;
      else gBootSourceNumberMax = 4;
    }
    
    if (gBootFileType == kNetworkDeviceType) {
      SetProp(Peer(0), "net-boot", NULL, 0);
    }
  }
  
  if (gBootSourceNumber >= gBootSourceNumberMax) return -1;
  
  if (gBootSourceNumberMax != 0) {
    switch (gBootFileType) {
    case kNetworkDeviceType :
      // Find the end of the device spec.
      cnt = 0;
      while (gBootDevice[cnt] != ':') cnt++;
      
      // Copy the device spec with the ':'.
      strncpy(gBootFile, gBootDevice, cnt + 1);
      
      // Check for bootp-responce or bsdp-responce.
      bootplen = GetPropLen(gChosenPH, "bootp-response");
      bsdplen  = GetPropLen(gChosenPH, "bsdp-response");
      if ((bootplen > 0) || (bsdplen > 0)) {
	if (bootplen > 0) {
	  buffer = malloc(bootplen);
	  GetProp(gChosenPH, "bootp-response", buffer, bootplen);
	} else {
	  buffer = malloc(bsdplen);
	  GetProp(gChosenPH, "bsdp-response", buffer, bsdplen);
	}
	
	// Flip the slash's to back slash's while looking for the last one.
	cnt = cnt2 = kBootpBootFileOffset;
	while (buffer[cnt] != '\0') {
	  if (buffer[cnt] == '/') {
	    buffer[cnt] = '\\';
	    cnt2 = cnt + 1;
	  }
	  cnt++;
	}
	
	// Add a comma at the front.
	buffer[kBootpBootFileOffset - 1] = ',';
	
	// Append the the root dir to the device spec.
	strncat(gBootFile, buffer + kBootpBootFileOffset - 1,
		cnt2 - kBootpBootFileOffset + 1);
	
	free(buffer);
      } else {
	// Look for the start of the root dir path.
	cnt3 = cnt;
	while (gBootDevice[cnt3] != ',') cnt3++;
	
	// Find the end of the path.  Look for a comma or null.
	cnt2 = cnt3 + 1;
	while ((gBootDevice[cnt2] != '\0') && (gBootDevice[cnt2] != ',')) cnt2++;
	
	// Find the last back slash or comma in the path
	cnt4 = cnt2 - 1;
	while ((gBootDevice[cnt4] != ',') && (gBootDevice[cnt4] != '\\')) cnt4--;
	
	// Copy the IP addresses if needed.
	if (gOFVersion < kOFVersion3x) {
	  strncat(gBootFile, gBootDevice + cnt + 1, cnt3 - cnt - 1);
	}
	
	// Add on the directory path
	strncat(gBootFile, gBootDevice + cnt3, cnt4 - cnt3 + 1);
      }
      
      // Add on the kernel name
      strcat(gBootFile, "mach.macosx");
      
      // Add on postfix
      strcat(gBootFile, gBootDevice + cnt2);
      break;
      
    case kBlockDeviceType :
      // Find the first ':'.
      cnt = 0;
      while ((gBootDevice[cnt] != '\0') && (gBootDevice[cnt] != ':')) cnt++;
      if (gBootDevice[cnt] == '\0') return -1;
      
      // Find the comma after the ':'.
      cnt2 = cnt + 1;
      while ((gBootDevice[cnt2]  != '\0') && (gBootDevice[cnt] != ',')) cnt2++;
      
      // Get just the partition number
      strncpy(gBootFile, gBootDevice + cnt + 1, cnt2 - cnt - 1);
      partNum = atoi(gBootFile);
      
      if (gBootSourceNumber > 1) {
	// Adjust the partition number to the root partition
	if (gOFVersion < kOFVersion3x) {
	  partNum += 1;
	} else {
	  partNum += 2;
	}
      }
      
      // Construct the boot-file
      strncpy(gBootFile, gBootDevice, cnt + 1);
      sprintf(gBootFile + cnt + 1, "%d,%s\\mach_kernel",
	      partNum, ((gBootSourceNumber & 1) ? "" : "\\"));
      break;
      
    default:
      printf("Failed to infer Boot Device Type.\n");
      return -1;
      break;
    }
  }
  
  // Figure out the root dir.
  ret = ConvertFileSpec(gBootFile, gRootDir, &filePath);
  if (ret == -1) return -1;
  
  strcat(gRootDir, ",");
  
  // Add in any extra path to gRootDir.
  cnt = 0;
  while (filePath[cnt] != '\0') cnt++;
  
  if (cnt != 0) {
    for (cnt2 = cnt - 1; cnt2 >= 0; cnt2--) {
      if (filePath[cnt2] == '\\') {
	strncat(gRootDir, filePath, cnt2 + 1);
	break;
      }
    }
  }
  
  SetProp(gChosenPH, "rootpath", gBootFile, strlen(gBootFile) + 1);
  
  gBootSourceNumber++;
  
  return 0;
}

// Public Functions

long GetDeviceType(char *devSpec)
{
  CICell ph;
  long   size;
  char   deviceType[32];
  
  ph = FindDevice(devSpec);
  if (ph == -1) return -1;
  
  size = GetProp(ph, "device_type", deviceType, 31);
  if (size != -1) deviceType[size] = '\0';
  else deviceType[0] = '\0';
  
  if (strcmp(deviceType, "network") == 0) return kNetworkDeviceType;
  if (strcmp(deviceType, "block") == 0) return kBlockDeviceType;
  
  return kUnknownDeviceType;
}


long ConvertFileSpec(char *fileSpec, char *devSpec, char **filePath)
{
  long cnt;
  
  // Find the first ':' in the fileSpec.
  cnt = 0;
  while ((fileSpec[cnt] != '\0') && (fileSpec[cnt] != ':')) cnt++;
  if (fileSpec[cnt] == '\0') return -1;
  
  // Find the next ',' in the fileSpec.
  while ((fileSpec[cnt] != '\0') && (fileSpec[cnt] != ',')) cnt++;
  
  // Copy the string to devSpec.
  strncpy(devSpec, fileSpec, cnt);
  devSpec[cnt] = '\0';
  
  // If there is a filePath start it after the ',', otherwise NULL.
  if (filePath != NULL) {
    if (fileSpec[cnt] != '\0') {
      *filePath = fileSpec + cnt + 1;
    } else {
      *filePath = NULL;
    }
  }
  
  return 0;
}


long MatchThis(CICell phandle, char *string)
{
  long ret, length;
  char *name, *model, *compatible;
  
  ret = GetPackageProperty(phandle, "name", &name, &length);
  if ((ret == -1) || (length == 0)) name = NULL;
  
  ret = GetPackageProperty(phandle, "model", &model, &length);
  if ((ret == -1) || (length == 0)) model = NULL;
  
  ret = GetPackageProperty(phandle, "compatible", &compatible, &length);
  if ((ret == -1) || (length == 0)) model = NULL;
  
  if ((name != NULL) && strcmp(name, string) == 0) return 0;
  if ((model != NULL) && strcmp(model, string) == 0) return 0;
  
  if (compatible != NULL) {
    while (*compatible != '\0') { 
      if (strcmp(compatible, string) == 0) return 0;
      
      compatible += strlen(compatible) + 1;
    }
  }
  
  return -1;
}


void *AllocateBootXMemory(long size)
{
  long addr = gImageFirstBootXAddr - size;
  
  if (addr < gImageLastKernelAddr) return 0;
  
  gImageFirstBootXAddr = addr;
  
  return (void *)addr;
}


long AllocateKernelMemory(long size)
{
  long addr = gImageLastKernelAddr;
  
  gImageLastKernelAddr += (size + 0xFFF) & ~0xFFF;
  
  if (gImageLastKernelAddr > gImageFirstBootXAddr)
    FailToBoot(-1);
  
  return addr;
}


long AllocateMemoryRange(char *rangeName, long start, long length)
{
  long result, *buffer;
  
  buffer = AllocateBootXMemory(2 * sizeof(long));
  if (buffer == 0) return -1;
  
  buffer[0] = start;
  buffer[1] = length;
  
  result = SetProp(gMemoryMapPH, rangeName, (char *)buffer, 2 * sizeof(long));
  if (result == -1) return -1;
  
  return 0;
}


unsigned long Alder32(unsigned char *buffer, long length)
{
  long          cnt;
  unsigned long result, lowHalf, highHalf;
  
  lowHalf = 1;
  highHalf = 0;
  
  for (cnt = 0; cnt < length; cnt++) {
    if ((cnt % 5000) == 0) {
      lowHalf  %= 65521L;
      highHalf %= 65521L;
    }
    
    lowHalf += buffer[cnt];
    highHalf += lowHalf;
  }

  lowHalf  %= 65521L;
  highHalf %= 65521L;
  
  result = (highHalf << 16) | lowHalf;
  
  return result;
}

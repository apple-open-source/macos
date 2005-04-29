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
 *  net.c - File System Module for wrapping TFTP.
 *
 *  Copyright (c) 1999-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include <fs.h>

struct NetPartInfo {
  char devSpec[1024];
};
typedef struct NetPartInfo NetPartInfo, *NetPartInfoPtr;


// Public functions

CICell NetInitPartition(char *devSpec)
{
  NetPartInfoPtr net;
  
  net = (NetPartInfoPtr)AllocateBootXMemory(sizeof(NetPartInfo));
  if (net == 0) return 0;
  
  strcpy(net->devSpec, devSpec);
  
  return (CICell)net;
}

long NetLoadFile(CICell ih, char *filePath)
{
  CICell         netIH;
  NetPartInfoPtr net;
  long           ret, length, triesLeft;
  char           fileSpec[2048];
  
  net = (NetPartInfoPtr)ih;
  
  sprintf(fileSpec, "%s,%s", net->devSpec, filePath);
  
  printf("Opening [%s]...\n", fileSpec);
  
  triesLeft = 10;
  do {
    netIH = Open(fileSpec);
    triesLeft--;
  } while ((netIH == 0) &&  triesLeft);
  if (netIH == 0) return -1;
  
  triesLeft = 10;
  do {
    ret = CallMethod(1, 1, netIH, "load", kLoadAddr, &length);
    if (gOFVersion < kOFVersion3x) {
      if (length == 0) ret = -1;
    }
    triesLeft--;
  } while ((ret != kCINoError) && triesLeft);
  if (ret != kCINoError) return -1;
  
  Close(netIH);
  
  return length;
}

long NetGetDirEntry(CICell ih, char *dirPath, long *dirIndex,
		    char **name, long *flags, long *time)
{
  return -1;
}

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
 *  elf.c - A function to decode a PPC Linux Kernel.
 *
 *  Copyright (c) 2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

#include "elf.h"

// Public Functions

long DecodeElf(void)
{
  ElfHeaderPtr     ehPtr;
  ProgramHeaderPtr phPtr;
  long             cnt, paddr, offset, memsz, filesz, entry, *tmp;
  
  ehPtr = (ElfHeaderPtr)kLoadAddr;
  if (ehPtr->signature != kElfSignature) return 0;
  
  entry = ehPtr->entry & kElfAddressMask;
  
  for (cnt = 0; cnt < ehPtr->phnum; cnt++) { 
    phPtr = (ProgramHeaderPtr)(kLoadAddr+ehPtr->phoff+cnt*ehPtr->phentsize);
    
    if (phPtr->type == kElfProgramTypeLoad) {
      paddr = phPtr->paddr & kElfAddressMask;
      offset = phPtr->offset;
      filesz = phPtr->filesz;
      memsz = phPtr->memsz;
      
      // Get the actual entry if it is in this program.
      if ((entry >= paddr) && (entry < (paddr + filesz))) {
	tmp = (long *)(kLoadAddr + offset + entry);
	if (tmp[2] == 0) entry +=  tmp[0];
	
      }
      entry += paddr;
      
      // Add the kernel to the memory-map.
      AllocateMemoryRange("Kernel-PROGRAM", 0, memsz);
      
      // Set the last address used by the kernel program.
      AllocateKernelMemory(paddr + memsz);
      
      if (paddr < kImageAddr) {
	// Copy the Vectors out of the way.
	bcopy((char *)(kLoadAddr + offset), gVectorSaveAddr,
	      kVectorSize - paddr);
	
	offset += kImageAddr - paddr;
	filesz -= kImageAddr - paddr;
	paddr = kImageAddr;
      }
      
      // Move the program.
      bcopy((char *)(kLoadAddr + offset), (char *)paddr, filesz);
    }
  }
  
  return 0;
}

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  macho.c - Functions for decoding a Mach-o Kernel.
 *
 *  Copyright (c) 1998-2003 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/machine/thread_status.h>

#include <sl.h>

static long DecodeSegment(long cmdBase);
static long DecodeUnixThread(long cmdBase);
static long DecodeSymbolTable(long cmdBase);

static unsigned long gPPCAddress;

// Public Functions

long ThinFatBinaryMachO(void **binary, unsigned long *length)
{
  unsigned long nfat, swapped, size;
  struct fat_header *fhp = (struct fat_header *)*binary;
  struct fat_arch   *fap =
    (struct fat_arch *)((unsigned long)*binary + sizeof(struct fat_header));
  
  if (fhp->magic == FAT_MAGIC) {
    nfat = fhp->nfat_arch;
    swapped = 0;
  } else if (fhp->magic == FAT_CIGAM) {
    nfat = NXSwapInt(fhp->nfat_arch);
    swapped = 1;
  } else {
    return -1;
  }
  
  for (; nfat > 0; nfat--, fap++) {
    if (swapped) {
      fap->cputype = NXSwapInt(fap->cputype);
      fap->offset = NXSwapInt(fap->offset);
      fap->size = NXSwapInt(fap->size);
    }
    
    if (fap->cputype == CPU_TYPE_POWERPC) {
      *binary = (void *) ((unsigned long)*binary + fap->offset);
      size = fap->size;
      break;
    }
  }
  
  if (length != 0) *length = size;
  
  return 0;
}

long DecodeMachO(void *binary)
{
  struct mach_header *mH;
  long   ncmds, cmdBase, cmd, cmdsize, headerBase, headerAddr, headerSize;
  long   cnt, ret;
  
  gPPCAddress = (unsigned long)binary;
  
  headerBase = gPPCAddress;
  cmdBase = headerBase + sizeof(struct mach_header);
  
  mH = (struct mach_header *)(headerBase);
  if (mH->magic != MH_MAGIC) return -1;
  
#if 0
  printf("magic:      %x\n", mH->magic);
  printf("cputype:    %x\n", mH->cputype);
  printf("cpusubtype: %x\n", mH->cpusubtype);
  printf("filetype:   %x\n", mH->filetype);
  printf("ncmds:      %x\n", mH->ncmds);
  printf("sizeofcmds: %x\n", mH->sizeofcmds);
  printf("flags:      %x\n", mH->flags);
#endif
  
  ncmds = mH->ncmds;
  
  for (cnt = 0; cnt < ncmds; cnt++) {
    cmd = ((long *)cmdBase)[0];
    cmdsize = ((long *)cmdBase)[1];
    
    switch (cmd) {
      
    case LC_SEGMENT:
      ret = DecodeSegment(cmdBase);
      break;
      
    case LC_UNIXTHREAD:
      ret = DecodeUnixThread(cmdBase);
      break;
      
    case LC_SYMTAB:
      ret = DecodeSymbolTable(cmdBase);
      break;
      
    default:
      printf("Ignoring cmd type %d.\n", cmd);
      break;
    }
    
    if (ret != 0) return -1;
    
    cmdBase += cmdsize;
  }
  
  // Save the mach-o header.
  headerSize = cmdBase - headerBase;
  headerAddr = AllocateKernelMemory(headerSize);
  bcopy((char *)headerBase, (char *)headerAddr, headerSize);
  
  // Add the Mach-o header to the memory-map.
  AllocateMemoryRange("Kernel-__HEADER", headerAddr, headerSize);
  
  return ret;
}

// Private Functions

static long DecodeSegment(long cmdBase)
{
  struct segment_command *segCmd;
  char   rangeName[32];
  char   *vmaddr, *fileaddr;
  long   vmsize, filesize;
  
  segCmd = (struct segment_command *)cmdBase;
  
  vmaddr = (char *)segCmd->vmaddr;
  vmsize = segCmd->vmsize;
  
  fileaddr = (char *)(gPPCAddress + segCmd->fileoff);
  filesize = segCmd->filesize;
  
#if 0
  printf("segname: %s, vmaddr: %x, vmsize: %x, fileoff: %x, filesize: %x, nsects: %d, flags: %x.\n",
	 segCmd->segname, vmaddr, vmsize, fileaddr, filesize,
	 segCmd->nsects, segCmd->flags);
#endif
  
  // Add the Segment to the memory-map.
  sprintf(rangeName, "Kernel-%s", segCmd->segname);
  AllocateMemoryRange(rangeName, (long)vmaddr, vmsize);

  if (vmsize && (strcmp(segCmd->segname, "__PRELINK") == 0))
    gHaveKernelCache = 1;
  
  // Handle special segments first.
  
  // If it is the vectors, save them in their special place.
  if ((strcmp(segCmd->segname, "__VECTORS") == 0) &&
      ((long)vmaddr < (kVectorAddr + kVectorSize)) &&
      (vmsize != 0) && (filesize != 0)) {
    
    // Copy the first part into the save area.
    bcopy(fileaddr, gVectorSaveAddr,
	  (filesize <= kVectorSize) ? filesize : kVectorSize);
    
    // Copy the rest into memory.
    if (filesize > kVectorSize)
      bcopy(fileaddr + kVectorSize, (char *)kVectorSize,
	    filesize - kVectorSize);
    
    return 0;
  }
  
  // It is nothing special, so do the usual. Only copy sections
  // that have a filesize.  Others are handle by the original bzero.
  if (filesize != 0) {
    bcopy(fileaddr, vmaddr, filesize);
  }
  
  // Adjust the last address used by the kernel
  if ((long)vmaddr + vmsize > AllocateKernelMemory(0)) {
    AllocateKernelMemory((long)vmaddr + vmsize - AllocateKernelMemory(0));
  }
  
  return 0;
}


static long DecodeUnixThread(long cmdBase)
{
  struct ppc_thread_state *ppcThreadState;
  
  // The PPC Thread State starts after the thread command stuct plus,
  // 2 longs for the flaver an num longs.
  ppcThreadState = (struct ppc_thread_state *)
    (cmdBase + sizeof(struct thread_command) + 8);
  
  gKernelEntryPoint = ppcThreadState->srr0;
  
#if 0
  printf("gKernelEntryPoint = %x\n", gKernelEntryPoint);
#endif
  
  return 0;
}


static long DecodeSymbolTable(long cmdBase)
{
  struct symtab_command *symTab, *symTableSave;
  long   tmpAddr, symsSize, totalSize;
  
  symTab = (struct symtab_command *)cmdBase;
  
#if 0
  printf("symoff: %x, nsyms: %x, stroff: %x, strsize: %x\n",
	 symTab->symoff, symTab->nsyms, symTab->stroff, symTab->strsize);
#endif
  
  symsSize = symTab->stroff - symTab->symoff;
  totalSize = symsSize + symTab->strsize;
  
  gSymbolTableSize = totalSize + sizeof(struct symtab_command);
  gSymbolTableAddr = AllocateKernelMemory(gSymbolTableSize);
  
  // Add the SymTab to the memory-map.
  AllocateMemoryRange("Kernel-__SYMTAB", gSymbolTableAddr, gSymbolTableSize);
  
  symTableSave = (struct symtab_command *)gSymbolTableAddr;
  tmpAddr = gSymbolTableAddr + sizeof(struct symtab_command);
  
  symTableSave->symoff = tmpAddr;
  symTableSave->nsyms = symTab->nsyms;
  symTableSave->stroff = tmpAddr + symsSize;
  symTableSave->strsize = symTab->strsize;
  
  bcopy((char *)(gPPCAddress + symTab->symoff),
	(char *)tmpAddr, totalSize);
  
  return 0;
}

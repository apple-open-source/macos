/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 *  load.c - Functions for decoding a Mach-o Kernel.
 *
 *  Copyright (c) 1998-2003 Apple Computer, Inc.
 *
 */

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/machine/thread_status.h>

#include <sl.h>

static long DecodeSegment(long cmdBase, unsigned int*load_addr, unsigned int *load_size);
static long DecodeUnixThread(long cmdBase, unsigned int *entry);


static unsigned long gBinaryAddress;
BOOL   gHaveKernelCache;

// Public Functions

long ThinFatFile(void **binary, unsigned long *length)
{
  unsigned long nfat, swapped, size = 0;
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
    
    if (fap->cputype == CPU_TYPE_I386) {
      *binary = (void *) ((unsigned long)*binary + fap->offset);
      size = fap->size;
      break;
    }
  }
  
  if (length != 0) *length = size;
  
  return 0;
}

long DecodeMachO(void *binary, entry_t *rentry, char **raddr, int *rsize)
{
  struct mach_header *mH;
  unsigned long  ncmds, cmdBase, cmd, cmdsize;
  //  long   headerBase, headerAddr, headerSize;
  unsigned int vmaddr = ~0;
  unsigned int vmend = 0;
  unsigned long  cnt;
  long  ret = -1;
  unsigned int entry;
  
  gBinaryAddress = (unsigned long)binary;
  
  //  headerBase = gBinaryAddress;
  cmdBase = (unsigned long)gBinaryAddress + sizeof(struct mach_header);
  
  mH = (struct mach_header *)(gBinaryAddress);
  if (mH->magic != MH_MAGIC) {
     error("Mach-O file has bad magic number\n");
     return -1;
  }
  
#if NOTDEF
  printf("magic:      %x\n", (unsigned)mH->magic);
  printf("cputype:    %x\n", (unsigned)mH->cputype);
  printf("cpusubtype: %x\n", (unsigned)mH->cpusubtype);
  printf("filetype:   %x\n", (unsigned)mH->filetype);
  printf("ncmds:      %x\n", (unsigned)mH->ncmds);
  printf("sizeofcmds: %x\n", (unsigned)mH->sizeofcmds);
  printf("flags:      %x\n", (unsigned)mH->flags);
  getc();
#endif
  
  ncmds = mH->ncmds;
  
  for (cnt = 0; cnt < ncmds; cnt++) {
    cmd = ((long *)cmdBase)[0];
    cmdsize = ((long *)cmdBase)[1];
    unsigned int load_addr;
    unsigned int load_size;
    
    switch (cmd) {
      
    case LC_SEGMENT:
      ret = DecodeSegment(cmdBase, &load_addr, &load_size);
      if (ret == 0 && load_size != 0) {
          vmaddr = min(vmaddr, load_addr);
          vmend = max(vmend, load_addr + load_size);
      }
      break;
      
    case LC_UNIXTHREAD:
      ret = DecodeUnixThread(cmdBase, &entry);
      break;
      
    default:
#if NOTDEF
      printf("Ignoring cmd type %d.\n", (unsigned)cmd);
#endif
      break;
    }
    
    if (ret != 0) return -1;
    
    cmdBase += cmdsize;
  }
  
  *rentry = (entry_t)( (unsigned long) entry & 0x3fffffff );
  *rsize = vmend - vmaddr;
  *raddr = (char *)vmaddr;
  
  return ret;
}

// Private Functions

static long DecodeSegment(long cmdBase, unsigned int *load_addr, unsigned int *load_size)
{
  struct segment_command *segCmd;
  unsigned long vmaddr, fileaddr;
  long   vmsize, filesize;
  
  segCmd = (struct segment_command *)cmdBase;
  
  vmaddr = (segCmd->vmaddr & 0x3fffffff);
  vmsize = segCmd->vmsize;
  
  fileaddr = (gBinaryAddress + segCmd->fileoff);
  filesize = segCmd->filesize;

  if (filesize == 0) {
      *load_addr = ~0;
      *load_size = 0;
      return 0;
  }
  
#if NOTDEF
  printf("segname: %s, vmaddr: %x, vmsize: %x, fileoff: %x, filesize: %x, nsects: %d, flags: %x.\n",
	 segCmd->segname, (unsigned)vmaddr, (unsigned)vmsize, (unsigned)fileaddr, (unsigned)filesize,
         (unsigned) segCmd->nsects, (unsigned)segCmd->flags);
  getc();
#endif
  
  if (vmaddr < KERNEL_ADDR ||
      (vmaddr + vmsize) > (KERNEL_ADDR + KERNEL_LEN)) {
      stop("Kernel overflows available space");
  }

  if (vmsize && (strcmp(segCmd->segname, "__PRELINK") == 0)) {
    gHaveKernelCache = 1;
  }
  
  // Copy from file load area.
  bcopy((char *)fileaddr, (char *)vmaddr, filesize);
  
  // Zero space at the end of the segment.
  bzero((char *)(vmaddr + filesize), vmsize - filesize);

  *load_addr = vmaddr;
  *load_size = vmsize;

  return 0;
}


static long DecodeUnixThread(long cmdBase, unsigned int *entry)
{
  i386_thread_state_t *i386ThreadState;
  
  i386ThreadState = (i386_thread_state_t *)
    (cmdBase + sizeof(struct thread_command) + 8);
  
  *entry = i386ThreadState->eip;
  
  return 0;
}


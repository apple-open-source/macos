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
 *  macho-to-xcoff.c - Converts a Mach-O file an XCOFF.
 *
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <mach-o/loader.h>
#include <mach/thread_status.h>
#include <mach/ppc/thread_status.h>


char *progname;
static FILE *machoF;
static FILE *xcoffF;

static struct mach_header mhead;

typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned long UInt32;

#ifdef __i386__
#define SWAPL(value) (   (((value) >> 24) & 0xff) | \
			 (((value) >> 8) & 0xff00) | \
			 (((value) << 8) & 0xff0000) | \
			 (((value) << 24) & 0xff000000) )

#define SWAPS(value) (   (((value) >> 8) & 0xff) | \
			 (((value) << 8) & 0xff00) )
#else
#define SWAPS(W)	(W)
#define SWAPL(L)	(L)
#endif

     typedef struct {
       /* File header */
       UInt16 magic;
#define kFileMagic			0x1DF
       UInt16 nSections;
       UInt32 timeAndDate;
       UInt32 symPtr;
       UInt32 nSyms;
       UInt16 optHeaderSize;
       UInt16 flags;
     } XFileHeader;

typedef struct {
  /* Optional header */
  UInt16 magic;
#define kOptHeaderMagic		0x10B
  UInt16 version;
  UInt32 textSize;
  UInt32 dataSize;
  UInt32 BSSSize;
  UInt32 entryPoint;
  UInt32 textStart;
  UInt32 dataStart;
  UInt32 toc;
  UInt16 snEntry;
  UInt16 snText;
  UInt16 snData;
  UInt16 snTOC;
  UInt16 snLoader;
  UInt16 snBSS;
  UInt8 filler[28];
} XOptHeader;

typedef struct {
  char name[8];
  UInt32 pAddr;
  UInt32 vAddr;
  UInt32 size;
  UInt32 sectionFileOffset;
  UInt32 relocationsFileOffset;
  UInt32 lineNumbersFileOffset;
  UInt16 nRelocations;
  UInt16 nLineNumbers;
  UInt32 flags;
} XSection;

enum SectionNumbers {
  kTextSN = 1,
  kDataSN,
  kBSSSN
};

#define kTextName	".text"
#define kDataName	".data"
#define kBSSName	".bss"

static struct {
  XFileHeader file;
  XOptHeader opt;
  XSection text;
  XSection data;
  XSection BSS;
} xHead = {
  {	/* file */
    SWAPS(kFileMagic),			/* magic */
    SWAPS(3),					/* nSections */
    0,							/* timeAndDate */
    0,							/* symPtr */
    0,							/* nSyms */
    SWAPS(sizeof (XOptHeader)),	/* optHeaderSize */
    0
  },
  {	/* opt */
    SWAPS(kOptHeaderMagic),		/* magic */
    0,							/* version */
    0,							/* textSize */
    0,							/* dataSize */
    0,							/* BSSSize */
    0,							/* entryPoint */
    0,							/* textStart */
    0,							/* dataStart */
    0,							/* toc */
    SWAPS(kTextSN),				/* snEntry */
    SWAPS(kTextSN),				/* snText */
    SWAPS(kDataSN),				/* snData */
    SWAPS(0),					/* snTOC */
    SWAPS(0),					/* snLoader */
    SWAPS(kBSSSN),				/* snBSS */
  },
  {	/* text section */
    kTextName
  },
  {	/* data section */
    kDataName
  },
  {	/* BSS section */
    kBSSName
  }
};


static UInt32 textFileOffset;
static UInt32 textSize;
static UInt32 dataFileOffset;
static UInt32 dataSize;
static UInt32 bssSize;


static void usage (char *msg)
{
  printf ("Usage: %s mach-o-file-name xcoff-file-name\n\n%s\n",
	  progname, msg);
  exit (1);
}


static void copyMachOSectionToXCOFF (UInt32 machoOffset, UInt32 sectionSize)
{
  static char buf[65536];

  fseek (machoF, machoOffset, SEEK_SET);

  while (sectionSize) {
    long readSize = sectionSize > sizeof (buf) ? sizeof (buf) : sectionSize;
    long writeSize;
    long actualSize;

    actualSize = fread (buf, 1, readSize, machoF);
    if (actualSize < 0) perror ("read error for section");

    writeSize = actualSize;
    actualSize = fwrite (buf, 1, writeSize, xcoffF);
    if (actualSize < 0) perror ("write error for section");

    sectionSize -= actualSize;
  }
}


int main (int argc, char **argv)
{
  int n;
  char *cmdsP, *cp;
  
#define LCP		((struct load_command *) cp)
#define SCP		((struct segment_command *) cp)
  
  progname = argv[0];
  
  if (argc < 3) usage ("wrong number of parameters");
  
  machoF = fopen (argv[1], "rb");
  if (machoF == 0) perror ("Can't open mach-o file");
  xcoffF = fopen (argv[2], "wb");
  if (xcoffF == 0) perror ("Can't create and/or open XCOFF file");

  n = fread (&mhead, sizeof (mhead), 1, machoF);
  if (n != 1) perror ("error reading mach-o file header");

  if (SWAPL(mhead.magic) != MH_MAGIC
      || SWAPL(mhead.filetype) != MH_EXECUTE)
    usage ("bad mach-o file header");

  cmdsP = malloc (SWAPL(mhead.sizeofcmds));
  if (cmdsP == 0) usage ("cmdsP allocation failed");

  n = fread (cmdsP, SWAPL(mhead.sizeofcmds), 1, machoF);
  if (n != 1) perror ("error reading mach-o commands");

  printf("Mach-o file has magic=0x%08lX, %ld commands\n", SWAPL(mhead.magic), SWAPL(mhead.ncmds));

  for (n = 0, cp = cmdsP; n < SWAPL(mhead.ncmds); ++n, cp += SWAPL(LCP->cmdsize)) {
    
    switch (SWAPL(LCP->cmd)) {
    case LC_SEGMENT:
      printf ("segment: %s: 0x%08lX of 0x%08lX bytes\n",
	      SCP->segname, SWAPL(SCP->vmaddr), SWAPL(SCP->vmsize));
      
      if (strncmp (SCP->segname, SEG_TEXT, sizeof (SCP->segname)) == 0) {
	textFileOffset = SWAPL(SCP->fileoff);
	textSize = SWAPL(SCP->filesize);
	printf ("__TEXT size = 0x%08lX\n", textSize);
	xHead.text.pAddr = xHead.text.vAddr = SCP->vmaddr;
	xHead.text.size = SCP->vmsize;
      } else
	if (strncmp (SCP->segname, SEG_DATA, sizeof (SCP->segname)) == 0) {
	  dataFileOffset = SWAPL(SCP->fileoff);
	  dataSize = SWAPL(SCP->filesize);
	  printf ("__DATA size = 0x%08lX\n", dataSize);
	  bssSize = SWAPL(SCP->vmsize) - SWAPL(SCP->filesize);
	  printf ("__BSS  size = 0x%08lX\n", bssSize);
	  xHead.data.pAddr = xHead.data.vAddr = SCP->vmaddr;
	  
	  /* Use just FILE part -- rest is BSS */
	  xHead.data.size = SCP->filesize;
	} else {
	  printf ("ignoring mach-o section \"%s\"\n", SCP->segname);
	}
      break;
      
    case LC_THREAD:
    case LC_UNIXTHREAD:
      xHead.opt.entryPoint = ((ppc_saved_state_t *) 
			      (cp + sizeof(struct thread_command)
			       + 2 * sizeof(unsigned long)) )->srr0;
      printf("Entry point %lx\n\n", SWAPL(xHead.opt.entryPoint));
      break;
    }
  }
  
  /* Construct BSS out of thin air: the part of the data section
     that is NOT file mapped */
  xHead.BSS.pAddr = xHead.BSS.vAddr = SWAPL(SWAPL(xHead.data.pAddr) + SWAPL(xHead.data.size));
  xHead.BSS.size = SWAPL(bssSize);
  
  /* Calculate the section file offsets in the resulting XCOFF file */
  xHead.text.sectionFileOffset = SWAPL(sizeof (xHead.file) + sizeof (xHead.opt)
				       + sizeof (xHead.text) + sizeof (xHead.data) + sizeof (xHead.BSS));
  xHead.data.sectionFileOffset = SWAPL(SWAPL(xHead.text.sectionFileOffset) + SWAPL(xHead.text.size));

  /* MT - write opt header */
  xHead.opt.textSize = xHead.text.size;
  xHead.opt.dataSize = xHead.data.size;
  xHead.opt.BSSSize = xHead.BSS.size;
  if (argc == 4) sscanf(argv[3],"%lx",&xHead.opt.entryPoint);

  /* Write out the XCOFF file, copying the relevant mach-o file sections */
  fwrite (&xHead, sizeof (xHead), 1, xcoffF);

  copyMachOSectionToXCOFF (textFileOffset, textSize);
  copyMachOSectionToXCOFF (dataFileOffset, dataSize);

  fclose (machoF);
  fclose (xcoffF);

  return 0;
}

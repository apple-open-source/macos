/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

#include "libsaio.h"
#include "memory.h"
#include "kernBootStruct.h"
#include "rcz_common.h"
#include <ufs/ufs/dir.h>
#include <mach-o/fat.h>

static int devMajor[3] = { 6, 3, 1 };  	// sd, hd, fd major dev #'s

//==========================================================================
// Open a file for reading.  If the file doesn't exist,
// try opening the compressed version.

#ifdef RCZ_COMPRESSED_FILE_SUPPORT
int
openfile(char *filename, int ignored)
{
    unsigned char *buf;
    int fd, size, ret;
    unsigned char *addr;
	
    if ((fd = open(filename, 0)) < 0) {
	buf = malloc(256);
	sprintf(buf, "%s%s", filename, RCZ_EXTENSION);
	if ((fd = open(buf, 0)) >= 0) {
	    size = rcz_file_size(fd);
	    addr = (unsigned char *)((KERNEL_ADDR + KERNEL_LEN) - size);
	    ret = rcz_decompress_file(fd, addr);
	    close(fd);
	    if (ret < 0)
		fd = -1;
	    else
		fd = openmem(addr, size);
	}
	free(buf);
    }
    return fd;
}
#else
int
openfile(char *filename, int ignored)
{
    return open(filename, 0);
}
#endif

//==========================================================================
// loadprog

int
loadprog( int                  dev,
          int                  fd,
          struct mach_header * headOut,
          entry_t *            entry,       /* entry point */
          char **              addr,        /* load address */
          int *                size )       /* size of loaded program */
{
    struct mach_header head;
    int file_offset = 0;

read_again:

    /* get file header */
    read(fd, (char *) &head, sizeof(head));

    if ( headOut )
        bcopy((char *) &head, (char *) headOut, sizeof(head));

    if ( head.magic == MH_MAGIC )
    {
        return loadmacho(&head, dev, fd, entry, addr, size, file_offset);
    }
    else if ( file_offset == 0 && 
              ((head.magic == FAT_CIGAM) || (head.magic == FAT_MAGIC)) )
    {
		int swap = (head.magic == FAT_CIGAM) ? 1 : 0;
		struct fat_header * fhp = (struct fat_header *) &head;
		struct fat_arch *   fap;
		int i, narch = swap ? NXSwapLong(fhp->nfat_arch) : fhp->nfat_arch;
		int cpu, size;
		char * buf;

		size = sizeof(struct fat_arch) * narch;
		buf = malloc(size);
        b_lseek(fd, 0, 0);
        read(fd, buf, size);
	
        for ( i = 0, fap = (struct fat_arch *)(buf+sizeof(struct fat_header));
              i < narch;
              i++, fap++ )
        {
            cpu = swap ? NXSwapLong(fap->cputype) : fap->cputype;
            if (cpu == CPU_TYPE_I386)
            {
                /* that's specific enough */
                free(buf);
                file_offset = swap ? NXSwapLong(fap->offset) : fap->offset;
                b_lseek(fd, file_offset, 0);
                goto read_again;
            }
        }
        free(buf);
        error("Fat binary file doesn't contain i386 code\n");
        return -1;
    }
    error("Unrecognized binary format: %08x\n", head.magic);
    return -1;
}

//==========================================================================
// xread
//
// Read from file descriptor. addr is a physical address.

int xread( int    fd,
           char * addr,
           int    size )
{
	char *      orgaddr = addr;
	long		offset;
	unsigned	count;
	long		max;
#define BUFSIZ 8192
	char *      buf;
	int         bufsize = BUFSIZ;

#if 0
    printf("xread: addr=%x, size=%x\n", addr, size);
    sleep(1);
#endif

	buf = malloc(BUFSIZ);
	
	// align your read to increase speed
	offset = tell(fd) & 4095;
	if ( offset != 0 )
		max = 4096 - offset;
	else
		max = bufsize;

	while ( size > 0 )
	{
		if ( size > max ) count = max;
		else count = size;
#if 0
		printf("xread: loop size=%x, count=%x\n", size, count);
		sleep(1);
#endif

		if ( read(fd, buf, count) != count) break;

		bcopy(buf, ptov(addr), count);
		size -= count;
		addr += count;

		max = bufsize;

#if 0
		tick += count;
		if ( tick > (50*1024) )
		{
			putchar('+');
			tick = 0;
		}
#endif
	}

	free(buf);
	return addr-orgaddr;
}

//==========================================================================
// loadmacho

int
loadmacho( struct mach_header * head,
           int                  dev,
           int                  io,
           entry_t *            rentry,
           char **              raddr,
           int *                rsize,
           int                  file_offset )
{
	int          ncmds;
	void *       cmds;
    void *       cp;
	unsigned int entry  = 0;
	int          vmsize = 0;
	unsigned int vmaddr = ~0;
    unsigned int vmend  = 0;

	struct xxx_thread_command {
		unsigned long	cmd;
		unsigned long	cmdsize;
		unsigned long	flavor;
		unsigned long	count;
		i386_thread_state_t state;
	} * th;

	// XXX should check cputype
	cmds = malloc(head->sizeofcmds);
	b_lseek(io, sizeof(struct mach_header) + file_offset, 0);

	if ( read(io, (char *) cmds, head->sizeofcmds) != head->sizeofcmds )
    {
	    error("loadmacho: error reading commands\n");
	    goto shread;
	}

	for ( ncmds = head->ncmds, cp = cmds; ncmds > 0; ncmds-- )
	{
		unsigned int addr;

#define lcp	((struct load_command *) cp)
#define scp	((struct segment_command *) cp)

		switch ( lcp->cmd )
		{
            case LC_SEGMENT:
                addr = (scp->vmaddr & 0x3fffffff) + (int)*raddr;
                if ( scp->filesize )
                {                    
                    vmsize += scp->vmsize;
                    vmaddr  = min(vmaddr, addr);
                    vmend   = max(vmend, addr + scp->vmsize);
                    
                    // Zero any space at the end of the segment.

                    bzero((char *)(addr + scp->filesize),
                          scp->vmsize - scp->filesize);
                    
                    // FIXME:  check to see if we overflow
                    // the available space (should be passed in
                    // as the size argument).
			    
#if 0
                    printf("LC: fileoff %x, filesize %x, off %x, addr %x\n",
                           scp->fileoff, scp->filesize, file_offset, addr);
                    sleep(1);
#endif

                    b_lseek(io, scp->fileoff + file_offset, 0);
                    if ( xread(io, (char *)addr, scp->filesize)
                         != scp->filesize)
                    {
                        error("loadmacho: error loading section\n");
                        goto shread;
                    }
                }
                break;

            case LC_THREAD:
            case LC_UNIXTHREAD:
                th = (struct xxx_thread_command *) cp;
                entry = th->state.eip;
                break;
		}
		cp += lcp->cmdsize;
	}

	kernBootStruct->rootdev = (dev & 0xffffff00) | devMajor[Dev(dev)];

	free(cmds);

	*rentry = (entry_t)( (int) entry & 0x3fffffff );
	*rsize = vmend - vmaddr;
	*raddr = (char *)vmaddr;

	return 0;

shread:
	free(cmds);
	return -1;
}

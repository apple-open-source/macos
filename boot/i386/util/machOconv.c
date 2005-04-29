/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/file.h>
#include <mach-o/loader.h>
#include <libkern/OSByteOrder.h>
#include <unistd.h>

int	infile, outfile;

struct mach_header	mh;
void *		cmds;

boolean_t		swap_ends;

static unsigned long swap(
    unsigned long x
)
{
    if (swap_ends)
	return OSSwapInt32(x);
    else
	return x;
}

int
main(int argc, char *argv[])
{
    kern_return_t	result;
    vm_address_t	data;
    int			nc, ncmds;
    char *		cp;
    
    if (argc == 2) {
	infile = open(argv[1], O_RDONLY);
	if (infile < 0)
	    goto usage;
	outfile = fileno(stdout);
    }
    else if (argc == 3) {
    	infile = open(argv[1], O_RDONLY);
	if (infile < 0)
	    goto usage;
	outfile = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (outfile < 0)
	    goto usage;
    }
    else {
usage:
    	fprintf(stderr, "usage: machOconv inputfile [outputfile]\n");
	exit(1);
    }
    
    nc = read(infile, &mh, sizeof (mh));
    if (nc < 0) {
	perror("read mach header");
	exit(1);
    }
    if (nc < (int)sizeof (mh)) {
	fprintf(stderr, "read mach header: premature EOF %d\n", nc);
	exit(1);
    }
    if (mh.magic == MH_MAGIC)
	swap_ends = FALSE;
    else if (mh.magic == MH_CIGAM)
	swap_ends = TRUE;
    else {
    	fprintf(stderr, "bad magic number %lx\n", (unsigned long)mh.magic);
	exit(1);
    }

    cmds = calloc(swap(mh.sizeofcmds), sizeof (char));
    if (cmds == 0) {
	fprintf(stderr, "alloc load commands: no memory\n");
	exit(1);
    }
    nc = read(infile, cmds, swap(mh.sizeofcmds));
    if (nc < 0) {
	perror("read load commands");
	exit(1);
    }
    if (nc < (int)swap(mh.sizeofcmds)) {
	fprintf(stderr, "read load commands: premature EOF %d\n", nc);
	exit(1);
    }

    for (	ncmds = swap(mh.ncmds), cp = cmds;
		ncmds > 0; ncmds--) {
	    boolean_t	isDATA;
	    unsigned	vmsize;

#define lcp	((struct load_command *)cp)    
	switch(swap(lcp->cmd)) {

	case LC_SEGMENT:
#define scp	((struct segment_command *)cp)
	    isDATA = (strcmp(scp->segname, "__DATA") == 0);
	    if (isDATA)
	    	vmsize = swap(scp->filesize);
	    else
	    	vmsize = swap(scp->vmsize);
	    result = vm_allocate(mach_task_self(), &data, vmsize, TRUE);
	    if (result != KERN_SUCCESS) {
		mach_error("vm_allocate segment data", result);
		exit(1);
	    }

	    lseek(infile, swap(scp->fileoff), L_SET);
	    nc = read(infile, (void *)data, swap(scp->filesize));
	    if (nc < 0) {
		perror("read segment data");
		exit(1);
	    }
	    if (nc < (int)swap(scp->filesize)) {
		fprintf(stderr, "read segment data: premature EOF %d\n", nc);
		exit(1);
	    }

	    nc = write(outfile, (void *)data, vmsize);
	    if (nc < (int)vmsize) {
		perror("write segment data");
		exit(1);
	    }
	    
	    vm_deallocate(mach_task_self(), data, vmsize);
	    break;
	}

	cp += swap(lcp->cmdsize);
    }
	
    exit(0);
}

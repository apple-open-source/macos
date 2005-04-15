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
#import <stdio.h>
#import <stdlib.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/mach_traps.h>

#import "rcz_compress_mem.h"
#import "rcz_decompress_mem.h"

void
usage(void)
{
    fprintf(stderr, "usage: rcz [-v] [-c | -d] [-o <ofile>] infile\n");
    fprintf(stderr, "  -v: verbose mode\n");
    fprintf(stderr, "  -c: compress\n");
    fprintf(stderr, "  -d: decompress\n");
    fprintf(stderr, "  use the special file name '-' to refer to stdin\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    char *infile = NULL, *outfile = NULL;
    int compress = 0;
    int verbose = 0;
    FILE *inf, *outf = NULL;
    unsigned char *inbuf, *outbuf;
    unsigned long length, total;
    kern_return_t ret;

    while ((++argv)[0] != NULL) {
	if (*argv[0] == '-') {
	    switch(*(argv[0]+1)) {
	    case 'c':
		compress = 1;
		break;
	    case 'd':
	    	compress = 0;
		break;
	    case 'o':
	    	if (argv[1]) {
		    outfile = argv[1];
		    argv++;
		} else {
		    usage();
		}
		break;
	    case 'v':
		verbose = 1;
		break;
	    case '\0':
		if (infile)
		    usage();
		infile = "-";
		break;
	    default:
		usage();
		break;
	    }
	} else {
	    if (infile) {
		usage();
	    } else {
		infile = argv[0];
	    }
	}
    }
    if (infile == NULL)
	usage();
        
    if (compress) {
	if (strcmp(infile, "-") == 0) {
	    inf = stdin;
	} else {
	    inf = fopen(infile, "r");
	    if (inf == NULL) {
		perror("open");
		exit(1);
	    }
	}
	if (*outfile) {
		if (strcmp(outfile, "-") == 0) {
	    	outf = stdout;
		} else {
	    	outf = fopen(outfile, "w+");
	    	if (outf == NULL) {
				perror("open outfile");
				exit(1);
	    	}
		}
	}

	fseek(inf,0,2); length = ftell(inf); rewind(inf);
	if ((ret = map_fd(fileno(inf), 0, (vm_address_t *)&inbuf, TRUE, length)) != KERN_SUCCESS) {
	    mach_error("map_fd", ret);	
	    exit(1);
	}
	outbuf = (unsigned char *)malloc(length + (length + 15)/16);
	total = rcz_compress_memory(inbuf, length, outbuf);
	fwrite(outbuf, 1, total, outf);
	if (verbose)
	    fprintf(stderr, "%ld %ld\nCompression ratio: %f\n", length, total, total/(1.0*length));
	fclose(inf);
	vm_deallocate(mach_task_self(), (vm_address_t)inbuf, length);
	fclose(outf);
    } else {
	unsigned char *ptr;
	
	if (strcmp(infile, "-") == 0) {
	    inf = stdin;
	} else {
	    inf = fopen(infile, "r");
	    if (inf == NULL) {
		perror("open");
		exit(1);
	    }
	}
	if (*outfile) {
		if (strcmp(outfile, "-") == 0) {
	    	outf = stdout;
		} else {
	    	outf = fopen(outfile, "w+");
	    	if (outf == NULL) {
			perror("open outfile");
			exit(1);
	    	}
		}
	}
	fseek(inf,0,2); length = ftell(inf); rewind(inf);
	if ((ret = map_fd(fileno(inf), 0, (vm_address_t *)&inbuf, TRUE, length)) != KERN_SUCCESS) {
	    mach_error("map_fd", ret);	
	    exit(1);
	}
	ptr = inbuf;
	ptr += 4;  /* Skip over version number, which is checked
			    later in ex_decompress(). */
	/* Next, figure out malloc size using the next four bytes of the
	compressed stream.
	*/    
	total = *ptr++;
	total = (total<<8) | (*ptr++);
	total = (total<<8) | (*ptr++);
	total = (total<<8) | (*ptr++);
	ptr -= 8;
    /* Now total is the exact byte count of the final, decompressed stream. */
	outbuf = (unsigned char *)malloc(total);
	total = rcz_decompress_memory(ptr, outbuf);
	fwrite(outbuf, 1, total, outf);
	if (verbose)
	    fprintf(stderr, "%ld %ld\nCompression ratio: %f\n", length, total, total/(1.0*length));
	fclose(inf);
	vm_deallocate(mach_task_self(), (vm_address_t)inbuf, length);
	fclose(outf);
    }
    exit(0);
}

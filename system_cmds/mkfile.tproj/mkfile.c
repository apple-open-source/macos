/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1997 Apple Computer, Inc. All Rights Reserved 
 *	
 * HISTORY
 * 29-Aug-97 Daniel Wade (danielw) at Apple
 *	Created.
 *
 */ 



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>

#define BF_SZ 	512	/* Size of write chunks */

extern void usage(char *, char *);
extern void create_file(char *, quad_t, int, int);
extern void err_rm(char *, char *);

int
main (argc, argv)
	int argc;
	char **argv;
{
	char *b_num, *prog_name;
	char *options = "nv";
	char c;
	quad_t multiplier = 1;
	quad_t file_size;
	int len;
	int empty = 0;
	int verbose = 0;

	prog_name = argv[0];	/* Get program name */
    if (1 == argc)
		usage(prog_name, options);

	/* Get options */
	opterr=1;
	
    	while ((c=getopt(argc, argv, options)) != EOF)
        	switch (c) {
        	case 'v':   /* Turn on verbose setting */
			verbose = 1;
			break;
		case 'n':   /* Create an empty file */
			empty = 1;
			break;
		default:
			usage(prog_name, options);
			break;
		}

	/* Stop getting options
	*/
	argv += optind;
	if (*argv == NULL)		/* Is there a size given? */
		usage(prog_name, options);
	
	b_num = *argv++;		/* Size of file and byte multiplier */
	len = strlen(b_num) - 1;

	if (!isdigit(b_num[len])) {
                switch(b_num[len]) {	/* Figure out multiplier */
			case 'B':
                        case 'b':
                                multiplier = 512;
                                break;
			case 'K':
                        case 'k':
                                multiplier = 1024;
                        	break;
			case 'M':
                        case 'm':
                                multiplier = 1024 * 1024;
                                break;
			case 'G':
                        case 'g':
                                multiplier = 1024 * 1024 * 1024;
                                break;
                        default:
                        	usage(prog_name, options);
                }
	}
	
	if (*argv == NULL)		/* Was a file name given? */
		usage(prog_name, options);	

	if ((file_size = strtoq(b_num, NULL, 10)) == 0 )
		err(1, "Bad file size!");

	while ( *argv != NULL ) {	/* Create file for each file_name */
		create_file(*argv, file_size*multiplier, empty, verbose);
		argv++;
	}

	return (0);

}


/* Create a file and make it empty (lseek) or zero'd */

void 
create_file(file_name, size, empty, verbose)
	char *file_name;
	quad_t size;
	int empty;
	int verbose;
{
	char buff[BF_SZ];
	int fd, bytes_written = BF_SZ;
	quad_t i;
	mode_t mode = S_IRUSR | S_IWUSR;

	/* If superuser, then set sticky bit */
	if (!geteuid()) mode |= S_ISVTX;

	if ((fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, mode)) == -1)
                err(1, NULL);


        if (empty) {			/* Create an empty file */
                lseek(fd, (off_t)size-1, SEEK_SET);
                if ( 1 != write(fd, "\0", 1))
			err_rm(file_name, "Write Error");
        }
	else {
		bzero(buff, BF_SZ);

		/*
		 * First loop: write BF_SZ chunks until you have
		 * less then BF_SZ bytes to write.
		 * Second loop: write the remaining bytes.
		 * ERRORS in the write process will cause the
		 * file to be removed before the error is
		 * reported.
		 */
		for (i = size; i > BF_SZ; i -= bytes_written) {
			bytes_written = write (fd, buff, BF_SZ);
			if ( bytes_written == -1 )
                                err_rm (file_name, "Write Error");
		}
		for (; i > 0; i -= bytes_written) {
                   	bytes_written = write (fd, buff, i);
                        if ( bytes_written == -1 )
                                err_rm (file_name, "Write Error");
		}
	}

	if (fchmod(fd, mode))	/* Change permissions */
		err_rm(file_name, NULL);

	if ((close(fd)) == -1)
		err_rm(file_name, NULL);

	if (verbose)
		(void)fprintf(stderr, "%s %qd bytes\n", file_name, size);

}

/* On error remove the file */

void
err_rm(filename, msg)
	char *filename;
	char *msg;
{
	unlink(filename);
	err(1, "(%s removed) %s", filename, msg); 
}


/* Print usage string */
void 
usage (prog_name, options)
	char *prog_name;
	char *options;
{
	(void)fprintf(stderr, 
		"usage: %s [-%s] size[b|k|m|g] filename ...\n", prog_name,  options);
	exit(1);

}

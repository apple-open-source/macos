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
/* Copyright 1993 NeXT, Inc.  All rights reserved. */

/* Replace characters in a file. */

#import <stdio.h>
#import <mach/mach.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/file.h>

void usage(void)
{
    fprintf(stderr,"Usage: yuck <infile> <outfile> <oldstring> <newstring>\n");
    exit(1);
}

char *strnstr(char *s1, char *s2, int len)
{
      register char c1;
      register const char c2 = *s2;

      while (len--) {
	     c1 = *s1++;
	     if (c1 == c2) {
	            register const char *p1, *p2;

	            p1 = s1;
	            p2 = &s2[1];
		    while (*p1++ == (c1 = *p2++) && c1)
			continue;
		    if (c1 == '\0') return ((char *)s1) - 1;
	     }
      }
      return NULL;
}

char *
strFromQuotedStr(char *oldstr)
{
    char newstr[1024], *p;
    char c;
    
    p = newstr;
    while (*oldstr) {
	switch (c = *oldstr++) {
	case '\\':
		switch(c = *oldstr++) {
		case 'r':
			*p++ = '\r';
			break;
		case 'n':
			*p++ = '\n';
			break;
		case 't':
			*p++ = '\t';
			break;
		default:
			*p++ = c;
			break;
		}
		break;
	default:
		*p++ = c;
		break;
	}
    }
    *p = '\0';
    p = (char *)malloc(strlen(newstr) + 1);
    strcpy(p, newstr);
    return p;
}

main(int argc, char **argv)
{
    int c, fd, ofd, filesize;
    kern_return_t r;
    char *infile, *outfile, *memfile, *oldstring, *os, *newstring;
    struct stat statbuf;
    
    if (argc != 5)
	usage();
    
    infile = argv[1];
    outfile = argv[2];
    fd = open(infile, O_RDONLY);
    if (fd < 0) {
	perror("open infile");
	exit(1);
    }
    if (fstat(fd, &statbuf) < 0) {
	perror("stat infile");
	exit(1);
    }
    ofd = open(outfile, O_TRUNC|O_RDWR|O_CREAT, 0644);
    if (ofd < 0) {
	perror("open outfile");
	exit(1);
    }
    filesize = statbuf.st_size;
    oldstring = strFromQuotedStr(argv[3]);
    newstring = strFromQuotedStr(argv[4]);
    if (strlen(newstring) > strlen(oldstring)) {
	fprintf(stderr, "Warning: new string is bigger than old string.\n");
    }
    r = map_fd(fd, (vm_offset_t)0, (vm_offset_t *)&memfile, TRUE,
	   (vm_size_t)filesize);
    
    if (r != KERN_SUCCESS) {
	mach_error("Error calling map_fd()", r);
	exit(1);
    } else {
	os = (char *)strnstr(memfile, oldstring, filesize);
	if (os == NULL) {
	    fprintf(stderr, "String not found\n");
	    exit(1);
	}
	while (*newstring)
	    *os++ = *newstring++;
	*os++ = *newstring++;
	lseek(fd, 0, 0);
	c = write(ofd, memfile, filesize);
	if (c < filesize) {
	    perror("write outfile");
	    exit(2);
	}
	exit(0);
    }
}

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
   Library: compressor for executable files.

   R. E. Crandall, July 1995
   
   Copyright 1995 NeXT Computer, Inc.
   All rights reserved.
   
 */

#include "rcz_common.h"
#include <stdlib.h>
#include <unistd.h>
static unsigned short que[QLEN];
 
#define REWIND	-1

// extern int read(int fd, char *buf, int len);
extern int  b_lseek(int fdesc, unsigned int addr, int ptr);

static unsigned char *buf;
static int buf_count;

#define BUF_SIZE 8192

static void
alloc_buf(int fd)
{
    buf = (unsigned char *)malloc(BUF_SIZE);
    buf_count = 0;
}

static unsigned char
get_byte(int fd)
{
    static unsigned char *ptr;

    if (buf_count == 0) {
	buf_count = read(fd, buf, BUF_SIZE);
	ptr = buf;
	if (buf_count <= 0)
	    return 0xFF;
    }
    buf_count--;
    return *ptr++;
}

static void
free_buf(void)
{
    buf_count = 0;
    free(buf);
}


int
rcz_file_size(
    int in_fd
)
{
    unsigned int version, length;
    
    alloc_buf(in_fd);
    b_lseek(in_fd, 0, 0);
    version = get_byte(in_fd);
    version = (version<<8) | (get_byte(in_fd));
    version = (version<<8) | (get_byte(in_fd));
    version = (version<<8) | (get_byte(in_fd));
 
    if(version != METHOD_17_JUL_95) {
	return (-1);
//    	fprintf(stderr, "Incompatible version.\n");
//	return(0);
    }
       
    length = get_byte(in_fd);
    length = (length<<8) | (get_byte(in_fd));
    length = (length<<8) | (get_byte(in_fd));
    length = (length<<8) | (get_byte(in_fd));
    free_buf();
    return length;
}

int
rcz_decompress_file(
    int in_fd,
    unsigned char *out
)
/* Returns actual number of bytes emitted as decompressed stream 'out.'
   Note that the 'in' stream contains this byte count already.
   
   Returns -1 if the input stream was not in compressed format.
 */
{
    unsigned int c, j, k, jmatch, jabove;
    int length;
    int even_length, word, token;
    unsigned char *outorigin = out;

    length = rcz_file_size(in_fd);
    if (length < 0)
	return length;

    alloc_buf(in_fd);
    b_lseek(in_fd, 8, 0);
    for(c=0; c < QLEN; c++) que[c] = c;
    even_length = 2*(length/2);
    while((int)(out-outorigin) < even_length) {
    	token = get_byte(in_fd);
        token = (token<<8) | (get_byte(in_fd));
        token = (token<<8) | (get_byte(in_fd));
        token = (token<<8) | (get_byte(in_fd));
    	c = 1<<31;	
	for(k = 0; k<32; k++) {
		if(c & token) {
		      jmatch = get_byte(in_fd);
		      word = que[jmatch];
		  /* Next, dynamically process the queue for match. */
		      jabove = (F1*jmatch) >> 4;
		      for(j = jmatch; j > jabove; j--) {
	      		     que[j] = que[j-1];
		      }
		      que[jabove] = word;
		} else {
		  /* Next, dynamically process the queue for unmatch. */
		    word = get_byte(in_fd);
		    word = (word << 8) | (get_byte(in_fd));
		    for(j=QLEN-1; j > ABOVE; j--) {
	      		que[j] = que[j-1];
	            } 
		    que[ABOVE] = word;
		}
		*out++ = (word >> 8) & 0xff;
		*out++ = (word) & 0xff;
		if((int)(out-outorigin) >= even_length) break;
		c >>= 1;
	}	
    }		
    if(even_length != length) *out++ = get_byte(in_fd);
    free_buf();
    return(length);
}

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
   Library: compressor for executable files.

   R. E. Crandall, July 1995
   
   Copyright 1995 NeXT Computer, Inc.
   All rights reserved.
   
 */

#import "rcz_common.h"
 
static unsigned short que[QLEN];

unsigned int
rcz_decompress_memory(unsigned char *in, unsigned char *out) 
/* Returns actual number of bytes emitted as decompressed stream 'out.'
   Note that the 'in' stream contains this byte count already.
   
   Returns -1 if the input stream was not in compressed format.
 */
{
    unsigned int c, j, k, jmatch, jabove;
    unsigned int length, even_length, word, token, version;
    unsigned char *outorigin = out;
    int *a, *b;

    version = *in++;
    version = (version<<8) | (*in++);
    version = (version<<8) | (*in++);
    version = (version<<8) | (*in++);
 
    if(version != METHOD_17_JUL_95) {
	return (-1);
//    	fprintf(stderr, "Incompatible version.\n");
//	return(0);
    }
       
    length = *in++;
    length = (length<<8) | (*in++);
    length = (length<<8) | (*in++);
    length = (length<<8) | (*in++);

    for(c=0; c < QLEN; c++) que[c] = c;
    even_length = 2*(length/2);
    while((int)(out-outorigin) < even_length) {
    	token = *in++;
        token = (token<<8) | (*in++);
        token = (token<<8) | (*in++);
        token = (token<<8) | (*in++);
    	c = 1<<31;	
	for(k = 0; k<32; k++) {
		if(c & token) {
		      jmatch = *in++;
		      word = que[jmatch];
		  /* Next, dynamically process the queue for match. */
		      jabove = (F1*jmatch) >> 4;
		      for(j = jmatch; j > jabove; j--) {
	      		     que[j] = que[j-1];
		      }
		      que[jabove] = word;
		} else {
		  /* Next, dynamically process the queue for unmatch. */
		    word = *in++;
		    word = (word << 8) | (*in++);
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
    if(even_length != length) *out++ = *in++;
    return(length);
}

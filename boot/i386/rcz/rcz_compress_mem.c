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
rcz_compress_memory(
    unsigned char *in,
    unsigned int inbytes,
    unsigned char *out
) 
/* Returns actual number of bytes emitted as compressed stream 'out.' */
{
    unsigned int c, ct, j, jmatch, jabove, match;
    unsigned int data[32], version;
    unsigned int word, token, tokenct, total;
    unsigned char *outorigin = out;

/* First, put version number into stream. */
    *out++ = (METHOD_17_JUL_95>>24) & 0xff;
    *out++ = (METHOD_17_JUL_95>>16) & 0xff;
    *out++ = (METHOD_17_JUL_95>>8) & 0xff;
    *out++ = (METHOD_17_JUL_95) & 0xff;
/* Next, put the initial size into stream. */
    *out++ = (inbytes>>24) & 0xff;
    *out++ = (inbytes>>16) & 0xff;
    *out++ = (inbytes>>8) & 0xff;
    *out++ = (inbytes) & 0xff;
    
    for(ct=0; ct < QLEN; ct++) que[ct] = ct;
    word = token = tokenct = 0;
    for(ct = 0; ct < inbytes; ct++) {
	/* Next, update bucket-brigade register. */
	  word = (word << 8) | (*in++);  
	  if(ct % 2 == 1) {
	        word &= 0xffff;
	  	match = 0;
	  	for(j=0; j < QLEN; j++) {
	  		if(que[j] == word) {
				match = 1;
				jmatch = j;
				break;
			}
	  	}
		token = (token<<1) | match;
		if(match) { /* 16-bit symbol is in queue. */
			c = que[jmatch];
			jabove = (F1 * jmatch) >> 4;
			for(j = jmatch; j > jabove; j--) {
	      		     que[j] = que[j-1];
			}
			que[jabove] = c;
			data[tokenct++] = jmatch;
		} else {   /* 16-bit symbol is not in queue. */
		    for(j=QLEN-1; j > ABOVE; j--) {
	      		que[j] = que[j-1];
	            } 
		    que[ABOVE] = word;
		    data[tokenct++] = word;
		}
		if(tokenct == 32) {  /* Unload tokens and data. */
		    *out++ = (token>>24) & 0xff;
		    *out++ = (token>>16) & 0xff;
		    *out++ = (token>>8) & 0xff;
		    *out++ = (token) & 0xff;
		    c = (1<<31);
		    for(j = 0; j < tokenct; j++) {
		    	if(token & c) *out++ = data[j] & 0xff;
			   else {
			       *out++ = (data[j] >> 8) & 0xff;
			       *out++ = (data[j]) & 0xff;
			   }
			c >>= 1;
		    } 
		    token = tokenct = 0;
		}
	  }
    }
    if(tokenct > 0) { /* Flush final token and data. */
        token <<= (32-tokenct);
    	*out++ = (token>>24) & 0xff;
	*out++ = (token>>16) & 0xff;
	*out++ = (token>>8) & 0xff;
	*out++ = (token) & 0xff;
	c = (1<<31);
	for(j = 0; j < tokenct; j++) {
		if(token & c) *out++ = data[j] & 0xff;
		   else {
		       *out++ = (data[j] >> 8) & 0xff;
		       *out++ = (data[j]) & 0xff;
		   }
		c >>= 1;
	} 
    }
    if(ct % 2 == 1) *out++ = (word) & 0xff;
    return((int)(out-outorigin));
}

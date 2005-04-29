#ifndef _LIBMD_WRAPPER_H_
#define _LIBMD_WRAPPER_H_
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

#include <openssl/md5.h>

/* This is a magic number taken from the FreeBSD libmd Makefile */
#define LENGTH 16

void  MD5Init(MD5_CTX *context);
char *MD5File(const char *filename, char *buf);
char *MD5End(MD5_CTX *ctx, char *buf);
void  MD5Final(unsigned char digest[16], MD5_CTX *context);
void  MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen);
char *MD5Data(const unsigned char *data, unsigned int len, char *buf) ;

#endif /* _LIBMD_WRAPPER_H_ */

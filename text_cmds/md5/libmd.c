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

/* This is a minimal wrapper around OpenSSL's libcrypto, as a compatibility
 * layer for FreeBSD's libmd library.  The goal is to minimize the number
 * of md5 implementations on the system, but still maintain source 
 * compatibility with projects that need libmd.
 *    - Rob Braun (bbraun) 1/18/2002
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libmd.h" 

void MD5Init(MD5_CTX *context)
{
    MD5_Init(context);
}

void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen)
{
	MD5_Update(context, input, inputLen);
}

void
MD5Final (unsigned char digest[16], MD5_CTX *context)
{
    MD5_Final(digest, context);
}

char *
MD5End(MD5_CTX *ctx, char *buf)
{
    int i;
    unsigned char digest[LENGTH];
    static const char hex[]="0123456789abcdef";

    if (!buf)
        buf = malloc(2*LENGTH + 1);
    if (!buf)
        return 0;
    MD5Final(digest, ctx);
    for (i = 0; i < LENGTH; i++) {
        buf[i+i] = hex[digest[i] >> 4];
        buf[i+i+1] = hex[digest[i] & 0x0f];
    }
    buf[i+i] = '\0';
    return buf;
}

char *MD5File(const char *filename, char *buf)
{
    unsigned char buffer[BUFSIZ];
    MD5_CTX ctx;
    int f,i,j;

    MD5Init(&ctx);
    f = open(filename,O_RDONLY);
    if (f < 0) return 0;
    while ((i = read(f,buffer,sizeof buffer)) > 0) {
        MD5Update(&ctx,buffer,i);
    }
    j = errno;
    close(f);
    errno = j;
    if (i < 0) return 0;
    return MD5End(&ctx, buf);
}

char *MD5Data(const unsigned char *data, unsigned int len, char *buf)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, data, len);
	return MD5End(&ctx, buf);
}

/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef _OSSL_BUFFER_H_
#define _OSSL_BUFFER_H_

typedef struct buf_mem_st {
	int	length;
	char *	data;
	int	max;
} BUF_MEM;

/* Rewrite symbols */
#define BUF_MEM_new		ossl_BUF_MEM_new
#define BUF_MEM_free		ossl_BUF_MEM_free
#define BUF_MEM_grow		ossl_BUF_MEM_grow
#define BUF_MEM_grow_clean	ossl_BUF_MEM_grow_clean
#define BUF_strlcpy		ossl_BUF_strlcpy
#define BUF_strlcat		ossl_BUF_strlcat


BUF_MEM *BUF_MEM_new(void);
void BUF_MEM_free(BUF_MEM *b);
int BUF_MEM_grow(BUF_MEM *str, int len);
int BUF_MEM_grow_clean(BUF_MEM *str, int len);
size_t BUF_strlcpy(char *dst, const char *src, size_t size);
size_t BUF_strlcat(char *dst, const char *src, size_t size);

#endif /* _OSSL_BUFFER_H_ */

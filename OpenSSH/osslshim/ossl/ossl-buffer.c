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

#include <stdlib.h>
#include <string.h>

#include "ossl-buffer.h"

BUF_MEM *
BUF_MEM_new(void)
{
	BUF_MEM *b;

	b = malloc(sizeof(BUF_MEM));
	if (NULL == b) {
		return (NULL);
	}

	b->length = 0;
	b->max = 0;
	b->data = NULL;

	return (b);
}


void
BUF_MEM_free(BUF_MEM *b)
{
	if (NULL == b) {
		return;
	}

	if (NULL != b->data) {
		memset(b->data, 0, (unsigned int)b->max);
		free(b->data);
	}
	free(b);
}


int
BUF_MEM_grow(BUF_MEM *str, int len)
{
	char *ret;
	unsigned int n;

	if (str->length >= len) {
		str->length = len;
		return (len);
	}
	if (str->max >= len) {
		memset(&str->data[str->length], 0, len-str->length);
		str->length = len;
		return (len);
	}
	n = (len + 3) / 3 * 4;
	if (str->data == NULL) {
		ret = malloc(n);
	} else{
		ret = realloc(str->data, n);
	}
	if (ret == NULL) {
		/* BUFerr(BUF_F_BUF_MEM_GROW,ERR_R_MALLOC_FAILURE); */
		len = 0;
	} else {
		str->data = ret;
		str->max = n;
		memset(&str->data[str->length], 0, len-str->length);
		str->length = len;
	}

	return (len);
}


int
BUF_MEM_grow_clean(BUF_MEM *str, int len)
{
	char *ret;
	unsigned int n;

	if (str->length >= len) {
		memset(&str->data[len], 0, str->length-len);
		str->length = len;
		return (len);
	}
	if (str->max >= len) {
		memset(&str->data[str->length], 0, len-str->length);
		str->length = len;
		return (len);
	}
	n = (len + 3) / 3 * 4;
	if (NULL == str->data) {
		ret = malloc(n);
	} else {
		if (n <= 0) {
			return (0);
		}
		if ((ret = malloc(n)) != NULL) {
			memcpy(ret, str->data, str->max);
			memset(str->data, 0, str->max);
			free(str->data);
		}
	}
	if (ret == NULL) {
		len = 0;
	} else {
		str->data = ret;
		str->max = n;
		memset(&str->data[str->length], 0, len - str->length);
		str->length = len;
	}
	return (len);
}


size_t
BUF_strlcpy(char *dst, const char *src, size_t size)
{
	size_t l = 0;

	for ( ; size > 1 && *src; size--) {
		*dst++ = *src++;
		l++;
	}
	if (size) {
		*dst = '\0';
	}
	return (l + strlen(src));
}


size_t
BUF_strlcat(char *dst, const char *src, size_t size)
{
	size_t l = 0;

	for ( ; size > 0 && *dst; size--, dst++) {
		l++;
	}
	return (l + BUF_strlcpy(dst, src, size));
}

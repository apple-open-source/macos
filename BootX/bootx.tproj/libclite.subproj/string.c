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
/*
 *  string.c - string operations.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include "libclite.h"

/*#if DONT_USE_GCC_BUILT_IN_STRLEN*/

#define tolower(c)     ((int)((c) & ~0x20))
#define toupper(c)     ((int)((c) | 0x20))

int strlen(const char *s)
{
	int n;

	n = 0;
	while (*s++) n++;
	return(n);
}

/*#endif*/

int
strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return (*s1 - *s2);
}

int strncmp(const char *s1, const char *s2, size_t len)
{
	register int n = len;
	while (--n >= 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return(0);
	return(n<0 ? 0 : *s1 - *--s2);
}

char *
strcpy(char *s1, const char *s2)
{
	register char *ret = s1;
	while (*s1++ = *s2++)
		continue;
	return ret;
}

char *
strncpy(char *s1, const char *s2, size_t n)
{
	register char *ret = s1;
	while (n && (*s1++ = *s2++))
		n--;
	if (!n) *s1=0;
	return ret;
}

int
ptol(char *str)
{
	register int c = *str;

	if (c <= '7' && c >= '0')
		c -= '0';
	else if (c <= 'h' && c >= 'a')
		c -= 'a';
	else c = 0;
	return c;
}

int
atoi(const char *str)
{
	register int sum = 0;
	while (*str == ' ' || *str == '\t')
		str++;
	while (*str >= '0' && *str <= '9') {
		sum *= 10;
		sum += *str++ - '0';
	}
	return sum;
}


char *strncat(char *s1, const char *s2, size_t n)
{
	register char *ret = s1;
	while (*s1)
		s1++;
	while (n-- && *s2)
		*s1++ = *s2++;
	*s1 = '\0';
	return ret;
}

char *strcat(char *s1, const char *s2)
{
	return(strncat(s1, s2, strlen(s2)));
}

/*#if STRNCASECMP*/
int strncasecmp(const char *s1, const char *s2, size_t len)
{
	register int n = len;
	while (--n >= 0 && tolower(*s1) == tolower(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(n<0 ? 0 : tolower(*s1) - tolower(*--s2));
}
/*#endif*/


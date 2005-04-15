/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.2 (the "License").  You may not use this file
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
/* string operations */

#include "libsa.h"

void * memset(void * dst, int val, size_t len)
{
    asm( "rep; stosb"
       : "=c" (len), "=D" (dst)
       : "0" (len), "1" (dst), "a" (val)
       : "memory" );

    return dst;
}

void * memcpy(void * dst, const void * src, size_t len)
{
    asm( "rep; movsb"
       : "=c" (len), "=D" (dst), "=S" (src)
       : "0" (len), "1" (dst), "2" (src)
       : "memory" );

    return dst;
}

void bcopy(const void * src, void * dst, size_t len)
{
	memcpy(dst, src, len);
}

void bzero(void * dst, size_t len)
{
    memset(dst, 0, len);
}

#if 0
/* #if DONT_USE_GCC_BUILT_IN_STRLEN */

#define tolower(c)     ((int)((c) & ~0x20))
#define toupper(c)     ((int)((c) | 0x20))

int strlen(const char * s)
{
	int n = 0;
	while (*s++) n++;
	return(n);
}

/*#endif*/
#endif

int
strcmp(const char * s1, const char * s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return (*s1 - *s2);
}

#if 0
int strncmp(const char * s1, const char * s2, size_t len)
{
	register int n = len;
	while (--n >= 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return(0);
	return(n<0 ? 0 : *s1 - *--s2);
}

char *
strcpy(char * s1, const char * s2)
{
	register char *ret = s1;
	while (*s1++ = *s2++)
		continue;
	return ret;
}
#endif

char *
strncpy(char * s1, const char * s2, size_t n)
{
	register char *ret = s1;
	while (n && (*s1++ = *s2++))
		n--;
	return ret;
}

char *
strlcpy(char * s1, const char * s2, size_t n)
{
	register char *ret = s1;
	while (n && (*s1++ = *s2++))
		n--;
	if (!n) *--s1=0;
	return ret;
}

#if 0
int
ptol(const char *str)
{
	register int c = *str;

	if (c <= '7' && c >= '0')
		c -= '0';
	else if (c <= 'h' && c >= 'a')
		c -= 'a';
	else c = 0;
	return c;
}
#endif

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

#if 0
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

#if STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t len)
{
	register int n = len;
	while (--n >= 0 && tolower(*s1) == tolower(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(n<0 ? 0 : tolower(*s1) - tolower(*--s2));
}
#endif
#endif

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
 *  libclite.h - Headers for the LibC Lite Functions
 *
 *  Ported from i386's libsa and libsaio
 *
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_LIBCLITE_H_
#define _BOOTX_LIBCLITE_H_

#include <sys/types.h>

#include <stdarg.h>
#include <stddef.h>

// ci_io.c
extern int putchar(int ch);
extern int puts(const char *str);

// prf.c
extern void prf(const char *fmt, unsigned int *adx, int (*putfn_p)(int ch),
		void *putfn_arg);

// printf.c
extern int printf(const char *format, ...);

// sprintf.c
extern int sprintf(char *str, const char *fmt, ...);

// string.c
extern int strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t len);
extern char *strcpy(char *s1, const char *s2);
extern char *strncpy(char *s1, const char *s2, size_t n);
extern char *strncat(char *s1, const char *s2, size_t n);
extern char *strcat(char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t len);

// strtol.c
extern int isupper(char c);
extern int isalpha(char c);
extern int isspace(char c);
extern int isdigit(char c);
extern char tolower(char c);
extern long strtol(const char *nptr, char **endptr, register int base);
extern u_quad_t strtouq(const char *nptr, char **endptr, register int base);

// zalloc.c
extern void malloc_init(char *start, int size);
extern void malloc_error_init(void (*error_fn)());
extern void *malloc(size_t size);
extern void free(void *pointer);
extern void *realloc(void *start, size_t newsize);

// mem.c
extern void *memcpy(void *dst, const void *src, size_t len);
extern void *memset(void *dst, int ch, size_t len);
extern int memcmp(const void *b1, const void *b2, size_t len);
extern void bcopy(const void *src, void *dst, size_t len);
extern void bzero(void *dst, int len);

// bsearch.c
extern void *bsearch(const void *key, const void *base, size_t nmemb,
		     size_t size, int (*compar)(const void *, const void *));

// bswap.c
extern int16_t bswap16(int16_t data);
extern int32_t bswap32(int32_t data);

#endif /* ! _BOOTX_LIBCLITE_H_ */

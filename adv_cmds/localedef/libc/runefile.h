/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Ruslan Ermilov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _RUNEFILE_H_
#define	_RUNEFILE_H_

#include <sys/types.h>

#ifndef _CACHED_RUNES
#define	_CACHED_RUNES	(1 << 8)
#endif

typedef struct {
	int32_t		min;
	int32_t		max;
	int32_t		map;
} _FileRuneEntry;

typedef struct {
	char		magic[8];
	char		encoding[32];

	uint32_t	runetype[_CACHED_RUNES];
	int32_t		maplower[_CACHED_RUNES];
	int32_t		mapupper[_CACHED_RUNES];

	int32_t		runetype_ext_nranges;
	int32_t		maplower_ext_nranges;
	int32_t		mapupper_ext_nranges;

	int32_t		variable_len;
#ifdef __APPLE__
	int32_t		ncharclasses;
#endif
} _FileRuneLocale;

#ifdef __APPLE__
/*
 * These versions accurately portray the old format, which tried to mimic the
 * _RuneEntry/_RuneLocale structures in the on-disk format and thus, had some
 * 32-bit pointers interspersed in interesting ways.
 *
 * The future versions, above, will be the existing FreeBSD way of laying it
 * out, which just gets copied manually into a _RuneLocale rather than using
 * some more clever techniques.
 */
typedef struct {
	int32_t		min;
	int32_t		max;
	int32_t		map;
	int32_t		__types_fake;
} _FileRuneEntry_A;

typedef struct {
	char		magic[8];
	char		encoding[32];

	int32_t		__sgetrune_fake;
	int32_t		__sputrune_fake;
	int32_t		__invalid_rune;

	uint32_t	runetype[_CACHED_RUNES];
	int32_t		maplower[_CACHED_RUNES];
	int32_t		mapupper[_CACHED_RUNES];

	int32_t		runetype_ext_nranges;
	int32_t		__runetype_ext_ranges_fake;
	int32_t		maplower_ext_nranges;
	int32_t		__maplower_ext_ranges_fake;
	int32_t		mapupper_ext_nranges;
	int32_t		__mapupper_ext_ranges_fake;

	int32_t		__variable_fake;
	int32_t		variable_len;

	int32_t		ncharclasses;
	int32_t		__charclasses_fake;
} _FileRuneLocale_A;

typedef struct {
	char		name[14];	/* CHARCLASS_NAME_MAX = 14 */
	__uint32_t	mask;		/* charclass mask */
} _FileRuneCharClass;

#define	_FILE_RUNE_MAGIC_A	"RuneMagA"	/* Indicates version A of RuneLocale */
#define	_FILE_RUNE_MAGIC_B	"RuneMagB"	/* Indicates version B of RuneLocale */
#endif
#define	_FILE_RUNE_MAGIC_1	"RuneMag1"

#endif	/* !_RUNEFILE_H_ */

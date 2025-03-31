/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _RES_DEBUG_H_
#define _RES_DEBUG_H_

/* Define RES_DEBUG_DISABLED to build without debug support */
#ifdef RES_DEBUG_DISABLED
#   define DprintC(cond, fmt, ...) /*empty*/
#   define Dprint(fmt, ...) /*empty*/
#   define Aerror(statp, file, string, error, address, alen) /*empty*/
#   define Perror(statp, file, string, error) /*empty*/
#else
#   include <errno.h>
#   ifndef RES_DEBUG_PREFIX
#   		define RES_DEBUG_PREFIX ""
#   endif
#   define _DebugPrint(filedesc, fmt, ...)\
		do {\
			int _saved_errno = errno;\
			fprintf(filedesc, RES_DEBUG_PREFIX fmt "\n", ##__VA_ARGS__);\
			errno = _saved_errno;\
		} while(0)
#   define DprintC(cond, fmt, ...)\
		do {\
			if (cond) {\
				_DebugPrint(stdout, fmt, ##__VA_ARGS__);\
			} else {\
			}\
		} while(0)
#   define DprintQ(cond, str, query, size)\
	do {\
		DprintC(cond, str);\
		if (cond) {\
			res_pquery(statp, query, size, stdout);\
		}\
	} while(0)
#   define Dprint(fmt, ...) DprintC((statp->options & RES_DEBUG), fmt, ##__VA_ARGS__)
#endif

#endif /* _RES_DEBUG_H_ */ 
/*! \file */

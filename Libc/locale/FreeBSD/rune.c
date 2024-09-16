/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rune.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/rune.c,v 1.12 2004/07/29 06:16:19 tjr Exp $");

#include "xlocale_private.h"

#include "namespace.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <runetype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mblocal.h"
#include "runefile.h"

#include "un-namespace.h"

static void
destruct_ctype(void *v)
{
	struct xlocale_ctype *data = v;

	if (&_DefaultRuneLocale != data->_CurrentRuneLocale)
		free(data->_CurrentRuneLocale);
	free(data);
}

static struct xlocale_ctype *
_Read_RuneMagi_A(FILE *fp, _FileRuneLocale_A *frl, void *lastp)
{
	size_t data_size;
	struct xlocale_ctype *data;
	_RuneLocale *rl;
	_FileRuneEntry_A *frr;
	_RuneEntry *rr;
	_FileRuneCharClass *frcc;
	int x, saverr;
	void *variable;
	_FileRuneEntry_A *runetype_ext_ranges;
	_FileRuneEntry_A *maplower_ext_ranges;
	_FileRuneEntry_A *mapupper_ext_ranges;
	int runetype_ext_len = 0;

	data = NULL;
	rl = NULL;
	variable = frl + 1;

#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
	frl->__invalid_rune =  ntohl(frl->__invalid_rune);
	frl->variable_len = ntohl(frl->variable_len);
	frl->ncharclasses = ntohl(frl->ncharclasses);
	frl->runetype_ext_nranges = ntohl(frl->runetype_ext_nranges);
	frl->maplower_ext_nranges = ntohl(frl->maplower_ext_nranges);
	frl->mapupper_ext_nranges = ntohl(frl->mapupper_ext_nranges);
#endif

	runetype_ext_ranges = (_FileRuneEntry_A *)variable;
	variable = runetype_ext_ranges + frl->runetype_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	maplower_ext_ranges = (_FileRuneEntry_A *)variable;
	variable = maplower_ext_ranges + frl->maplower_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	mapupper_ext_ranges = (_FileRuneEntry_A *)variable;
	variable = mapupper_ext_ranges + frl->mapupper_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	frr = runetype_ext_ranges;
	for (x = 0; x < frl->runetype_ext_nranges; ++x) {
		uint32_t *types;

#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
		frr[x].min = ntohl(frr[x].min);
		frr[x].max = ntohl(frr[x].max);
		frr[x].map = ntohl(frr[x].map);
#endif
		if (frr[x].map == 0) {
			int len = frr[x].max - frr[x].min + 1;
			types = variable;
			variable = types + len;
			runetype_ext_len += len;
			if (variable > lastp) {
				errno = EINVAL;
				goto invalid;
			}
		}
	}

	/*
	 * macOS additionally has a custom charclass table to account for.
	 */
	frcc = variable;
	variable = frcc + frl->ncharclasses;

	if ((char *)variable + frl->variable_len > (char *)lastp) {
		errno = EINVAL;
		goto invalid;
	}

	/*
	 * Convert from disk format to host format.
	 */
	data_size = sizeof(struct xlocale_ctype) +
	    (frl->runetype_ext_nranges + frl->maplower_ext_nranges +
	    frl->mapupper_ext_nranges) * sizeof(_RuneEntry) +
	    runetype_ext_len * sizeof(*rr->__types) +
	    frl->ncharclasses * sizeof(_FileRuneCharClass) +
	    frl->variable_len;
	data = calloc(1, data_size);
	if (data == NULL)
		goto invalid;

	data->header.header.retain_count = 1;
	data->header.header.destructor = destruct_ctype;

	rl = data->_CurrentRuneLocale = calloc(1,
	    sizeof(*data->_CurrentRuneLocale));
	if (rl == NULL) {
		free(data);
		goto invalid;
	}
	rl->__variable = data + 1;

	memcpy(rl->__magic, _RUNE_MAGIC_A, sizeof(rl->__magic));
	assert(sizeof(rl->__encoding) == sizeof(frl->encoding));
	memcpy(rl->__encoding, frl->encoding, sizeof(frl->encoding));

	rl->__invalid_rune = frl->__invalid_rune;
	rl->__variable_len = frl->variable_len;
	rl->__runetype_ext.__nranges = frl->runetype_ext_nranges;
	rl->__maplower_ext.__nranges = frl->maplower_ext_nranges;
	rl->__mapupper_ext.__nranges = frl->mapupper_ext_nranges;
	rl->__ncharclasses = frl->ncharclasses;

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->__runetype[x] = ntohl(frl->runetype[x]);
		rl->__maplower[x] = ntohl(frl->maplower[x]);
		rl->__mapupper[x] = ntohl(frl->mapupper[x]);
	}

	/*
	 * rl->__variable has already essentially been validated above by
	 * ensuring that the variable data (found at the very end) does not pass
	 * the end of the file.
	 */
	rl->__runetype_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__runetype_ext.__ranges +
	    rl->__runetype_ext.__nranges;

	rl->__maplower_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__maplower_ext.__ranges +
	    rl->__maplower_ext.__nranges;

	rl->__mapupper_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__mapupper_ext.__ranges +
	    rl->__mapupper_ext.__nranges;

	rl->__charclasses = (_RuneCharClass *)rl->__variable;
	rl->__variable = rl->__charclasses + rl->__ncharclasses;

	variable = mapupper_ext_ranges + frl->mapupper_ext_nranges;
	frr = runetype_ext_ranges;
	rr = rl->__runetype_ext.__ranges;
	for (x = 0; x < rl->__runetype_ext.__nranges; ++x) {
		uint32_t *types;

		/* Already byteswapped above as needed */
		rr[x].__min = frr[x].min;
		rr[x].__max = frr[x].max;
		rr[x].__map = frr[x].map;

		if (rr[x].__map == 0) {
			int len = rr[x].__max - rr[x].__min + 1;
			types = variable;
			variable = types + len;
			rr[x].__types = rl->__variable;
			rl->__variable = rr[x].__types + len;
			while (len-- > 0) {
				rr[x].__types[len] = ntohl(types[len]);
			}
		} else
			rr[x].__types = NULL;
	}

	frr = maplower_ext_ranges;
	rr = rl->__maplower_ext.__ranges;
	for (x = 0; x < rl->__maplower_ext.__nranges; ++x) {
		rr[x].__min = ntohl(frr[x].min);
		rr[x].__max = ntohl(frr[x].max);
		rr[x].__map = ntohl(frr[x].map);
	}

	frr = mapupper_ext_ranges;
	rr = rl->__mapupper_ext.__ranges;
	for (x = 0; x < frl->mapupper_ext_nranges; ++x) {
		rr[x].__min = ntohl(frr[x].min);
		rr[x].__max = ntohl(frr[x].max);
		rr[x].__map = ntohl(frr[x].map);
	}

	if (frl->ncharclasses > 0) {
		frcc = variable;
		variable = frcc + frl->ncharclasses;
		if (variable > lastp) {
			errno = EINVAL;
			goto invalid;
		}

		rl->__charclasses = (_RuneCharClass *)rl->__variable;
		rl->__variable = (void *)(rl->__charclasses + rl->__ncharclasses);
		for (x = 0; x < frl->ncharclasses; ++x) {
			memcpy(rl->__charclasses[x].__name,
			    frcc[x].name, sizeof(frcc[x].name));
			rl->__charclasses[x].__mask = ntohl(frcc[x].mask);
		}
	}

	/* memcpy variable */
	memcpy(rl->__variable, variable, rl->__variable_len);

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->__variable_len)
		rl->__variable = 0;

	if (!rl->__runetype_ext.__nranges)
		rl->__runetype_ext.__ranges = 0;

	if (!rl->__maplower_ext.__nranges)
		rl->__maplower_ext.__ranges = 0;

	if (!rl->__mapupper_ext.__nranges)
		rl->__mapupper_ext.__ranges = 0;

	data->__datasize = data_size;
	return (data);
invalid:
	saverr = errno;
	free(rl);
	free(data);
	errno = saverr;
	return (NULL);
}

struct xlocale_ctype *
_Read_RuneMagi(FILE *fp)
{
	size_t data_size;
	char *fdata;
	struct xlocale_ctype *data;
	void *lastp;
	_FileRuneLocale *frl;
	_RuneLocale *rl;
	_FileRuneEntry *frr;
	_RuneEntry *rr;
	_FileRuneCharClass *frcc;
	struct stat sb;
	int x, saverr;
	void *variable;
	_FileRuneEntry *runetype_ext_ranges;
	_FileRuneEntry *maplower_ext_ranges;
	_FileRuneEntry *mapupper_ext_ranges;
	int runetype_ext_len = 0;

	data = NULL;
	rl = NULL;
	if (_fstat(fileno(fp), &sb) < 0)
		return (NULL);

	if (sb.st_size < sizeof(_FileRuneLocale)) {
		errno = EFTYPE;
		return (NULL);
	}

	fdata = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
	    fileno(fp), 0);
	if (fdata == MAP_FAILED) {
		errno = EINVAL;
		return (NULL);
	}

	frl = (_FileRuneLocale *)(void *)fdata;
	lastp = fdata + sb.st_size;

	/*
	 * The legacy format has two major differences:
	 * 1.) All of the data structures are a little bloated because we were
	 *    storing it as their Libc representation, embedded pointers and
	 *    all.
	 *
	 * 2.) It used big endian everywhere, so we needed to byteswap in many,
	 *    many places.
	 *
	 * Instead of teaching localedef(1) to write out the legacy format,
	 * we took advantage of the magic bump and mandated that new LC_CTYPE
	 * be of native endianness, and it no longer contains padding where
	 * pointers would normally be.
	 */
	if (memcmp(frl->magic, _FILE_RUNE_MAGIC_A, sizeof(frl->magic)) == 0) {
		/*
		 * The old format was larger; re-verify the size now that we
		 * know it's an A record.
		 */
		if (sb.st_size < sizeof(_FileRuneLocale_A)) {
			errno = EFTYPE;
			return (NULL);
		}
		data = _Read_RuneMagi_A(fp, (void *)frl, lastp);
		saverr = errno;
		munmap(fdata, sb.st_size);
		if (data == NULL)
			errno = saverr;
		return (data);

	}

	variable = frl + 1;

	if (memcmp(frl->magic, _FILE_RUNE_MAGIC_B, sizeof(frl->magic)) != 0) {
		errno = EFTYPE;
		goto invalid;
	}

	runetype_ext_ranges = (_FileRuneEntry *)variable;
	variable = runetype_ext_ranges + frl->runetype_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	maplower_ext_ranges = (_FileRuneEntry *)variable;
	variable = maplower_ext_ranges + frl->maplower_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	mapupper_ext_ranges = (_FileRuneEntry *)variable;
	variable = mapupper_ext_ranges + frl->mapupper_ext_nranges;
	if (variable > lastp) {
		errno = EINVAL;
		goto invalid;
	}

	frr = runetype_ext_ranges;
	for (x = 0; x < frl->runetype_ext_nranges; ++x) {
		uint32_t *types;

		if (frr[x].map == 0) {
			int len = frr[x].max - frr[x].min + 1;
			types = variable;
			variable = types + len;
			runetype_ext_len += len;
			if (variable > lastp) {
				errno = EINVAL;
				goto invalid;
			}
		}
	}

	/*
	 * macOS additionally has a custom charclass table to account for.
	 */
	frcc = variable;
	variable = frcc + frl->ncharclasses;

	if ((char *)variable + frl->variable_len > (char *)lastp) {
		errno = EINVAL;
		goto invalid;
	}

	/*
	 * Convert from disk format to host format.
	 */
	data_size = sizeof(struct xlocale_ctype) +
	    (frl->runetype_ext_nranges + frl->maplower_ext_nranges +
	    frl->mapupper_ext_nranges) * sizeof(_RuneEntry) +
	    runetype_ext_len * sizeof(*rr->__types) +
	    frl->ncharclasses * sizeof(_FileRuneCharClass) +
	    frl->variable_len;
	data = calloc(1, data_size);
	if (data == NULL)
		goto invalid;

	data->header.header.retain_count = 1;
	data->header.header.destructor = destruct_ctype;

	/*
	 * XXX The above should be allocating this structure + the rest, not
	 * an `xlocale_ctype`.
	 */
	rl = data->_CurrentRuneLocale = calloc(1,
	    sizeof(*data->_CurrentRuneLocale));
	if (rl == NULL) {
		free(data);
		goto invalid;
	}
	rl->__variable = data + 1;

	memcpy(rl->__magic, _RUNE_MAGIC_A, sizeof(rl->__magic));
	assert(sizeof(rl->__encoding) == sizeof(frl->encoding));
	memcpy(rl->__encoding, frl->encoding, sizeof(frl->encoding));

	rl->__invalid_rune = 0;	/* Deprecated */
	rl->__variable_len = frl->variable_len;
	rl->__runetype_ext.__nranges = frl->runetype_ext_nranges;
	rl->__maplower_ext.__nranges = frl->maplower_ext_nranges;
	rl->__mapupper_ext.__nranges = frl->mapupper_ext_nranges;
	rl->__ncharclasses = frl->ncharclasses;

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->__runetype[x] = frl->runetype[x];
		rl->__maplower[x] = frl->maplower[x];
		rl->__mapupper[x] = frl->mapupper[x];
	}

	/*
	 * rl->__variable has already essentially been validated above by
	 * ensuring that the variable data (found at the very end) does not pass
	 * the end of the file.
	 */
	rl->__runetype_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__runetype_ext.__ranges +
	    rl->__runetype_ext.__nranges;

	rl->__maplower_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__maplower_ext.__ranges +
	    rl->__maplower_ext.__nranges;

	rl->__mapupper_ext.__ranges = (_RuneEntry *)rl->__variable;
	rl->__variable = rl->__mapupper_ext.__ranges +
	    rl->__mapupper_ext.__nranges;

	rl->__charclasses = (_RuneCharClass *)rl->__variable;
	rl->__variable = rl->__charclasses + rl->__ncharclasses;

	variable = mapupper_ext_ranges + frl->mapupper_ext_nranges;
	frr = runetype_ext_ranges;
	rr = rl->__runetype_ext.__ranges;
	for (x = 0; x < rl->__runetype_ext.__nranges; ++x) {
		uint32_t *types;

		rr[x].__min = frr[x].min;
		rr[x].__max = frr[x].max;
		rr[x].__map = frr[x].map;

		if (rr[x].__map == 0) {
			int len = rr[x].__max - rr[x].__min + 1;
			types = variable;
			variable = types + len;
			rr[x].__types = rl->__variable;
			rl->__variable = rr[x].__types + len;
			while (len-- > 0) {
				rr[x].__types[len] = types[len];
			}
		} else
			rr[x].__types = NULL;
	}

	frr = maplower_ext_ranges;
	rr = rl->__maplower_ext.__ranges;
	for (x = 0; x < rl->__maplower_ext.__nranges; ++x) {
		rr[x].__min = frr[x].min;
		rr[x].__max = frr[x].max;
		rr[x].__map = frr[x].map;
	}

	frr = mapupper_ext_ranges;
	rr = rl->__mapupper_ext.__ranges;
	for (x = 0; x < frl->mapupper_ext_nranges; ++x) {
		rr[x].__min = frr[x].min;
		rr[x].__max = frr[x].max;
		rr[x].__map = frr[x].map;
	}

	if (frl->ncharclasses > 0) {
		frcc = variable;
		variable = frcc + frl->ncharclasses;
		if (variable > lastp) {
			errno = EINVAL;
			goto invalid;
		}

		rl->__charclasses = (_RuneCharClass *)rl->__variable;
		rl->__variable = (void *)(rl->__charclasses + rl->__ncharclasses);
		for (x = 0; x < frl->ncharclasses; ++x) {
			memcpy(rl->__charclasses[x].__name,
			    frcc[x].name, sizeof(frcc[x].name));
			rl->__charclasses[x].__mask = frcc[x].mask;
		}
	}

	/* memcpy variable */
	memcpy(rl->__variable, variable, rl->__variable_len);
	munmap(fdata, sb.st_size);

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->__variable_len)
		rl->__variable = 0;

	if (!rl->__runetype_ext.__nranges)
		rl->__runetype_ext.__ranges = 0;

	if (!rl->__maplower_ext.__nranges)
		rl->__maplower_ext.__ranges = 0;

	if (!rl->__mapupper_ext.__nranges)
		rl->__mapupper_ext.__ranges = 0;

	data->__datasize = data_size;
	return (data);
invalid:
	saverr = errno;
	munmap(fdata, sb.st_size);
	free(rl);
	free(data);
	errno = saverr;
	return (NULL);
}

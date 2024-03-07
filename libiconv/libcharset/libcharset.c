/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <langinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <os/variant_private.h>

#define LIBCHARSET_SUBSYSTEM	"com.apple.libiconv.libcharset"

static char default_from[] = "*";

/*
 * We'll hold a solid 8 entries in .data before we allocate a larger map.
 */
static struct charset_map_entry {
	char *fromname;
	char *toname;
} charset_mappings_static[8] = {
	{ &default_from[0], "UTF-8" },	/* Built-in */
};

static struct charset_map_entry *charset_map;
static size_t charset_elems;

#ifndef MIN
#define MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

#ifndef __XSTRING
#define __XSTRING(x)	__STRING(x)
#endif

#define LOCALE_CHARSET_MAXSZ	64
#define	LOCALE_CHARSET_MAXSCALE	64
#define LOCALE_CHARSET_FMT	"%" __XSTRING(LOCALE_CHARSET_MAXSZ) "s"

#define LOCALE_CHARSET_DEFAULT	"ASCII"
#define LOCALE_CHARSET_FILE	"charset.alias"

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define LIBCHARSET_FUZZING_EXPORT
#else
#define LIBCHARSET_FUZZING_EXPORT	static
#endif

static bool
libcharset_map(struct charset_map_entry **map, size_t *nelem, size_t *mapsz,
    const char *fromcs, const char *tocs)
{
	struct charset_map_entry *entry;

	if (*nelem == *mapsz) {
		struct charset_map_entry *newmap;
		size_t newsz, scalesz;

		/*
		 * Need to (re-)allocate the map.  Double it until we hit 128
		 * entries (arbitrary), then just scale it by 64 each time.
		 */
		scalesz = MIN(LOCALE_CHARSET_MAXSCALE, *mapsz);
		newsz = *mapsz + scalesz;

		/*
		 * Allocate - we don't use realloc here so that we still have
		 * at least a chunk of the mappings if we run out of memory.
		 */
		newmap = calloc(newsz, sizeof(**map));
		if (newmap == NULL)
			return (false);

		memcpy(newmap, *map, *mapsz * sizeof(**map));

		if (*map != &charset_mappings_static[0])
			free(*map);
		*map = newmap;
		*mapsz = newsz;
	}

	entry = &(*map)[*nelem];

	entry->fromname = strdup(fromcs);

	if (tocs[0] == '\0')
		tocs = LOCALE_CHARSET_DEFAULT;
	entry->toname = strdup(tocs);

	if (entry->fromname == NULL || entry->toname == NULL) {
		free(entry->fromname);
		entry->fromname = NULL;
		free(entry->toname);
		entry->toname = NULL;
		return (false);
	}

	*nelem += 1;
	return (true);
}

LIBCHARSET_FUZZING_EXPORT struct charset_map_entry *
libcharset_build_map_fp(FILE *fp, size_t *oelem)
{
	char fcs[LOCALE_CHARSET_MAXSZ + 1], tcs[LOCALE_CHARSET_MAXSZ + 1];
	struct charset_map_entry *map = &charset_mappings_static[0];
	size_t mapsz = nitems(charset_mappings_static), nelem = 0;

	/*
	 * Without a charset.aliases we map it straight it to UTF-8, but with a
	 * valid, malformed charset.aliases we would consider it an empty
	 * mapping and fallback to ASCII.
	 */
	while (!feof(fp) && !ferror(fp)) {
		int ch;

		/* Skip any whitespace or comments. */
		do {
			ch = getc(fp);
		} while (isspace(ch) && ch != EOF);

		if (ch == '#') {
			size_t ignored;

			/* Eat the rest of the line, try again. */
			(void)fgetln(fp, &ignored);
			continue;
		} else if (ch == EOF)
			break;

		/* Ate one too many, push it back for convenience. */
		(void)ungetc(ch, fp);

		/* Try to parse a codeset mapping out of it. */
		if (fscanf(fp, LOCALE_CHARSET_FMT " " LOCALE_CHARSET_FMT,
		    &fcs[0], &tcs[0]) != 2)
			break;	/* Malformed */

		if (fcs[0] == '\0')
			break;	/* Malformed */

		/*
		 * GNU libcharset seems to accept an '#' in either from or to
		 * codeset names, so we'll just remain compatible there.
		 */
		if (!libcharset_map(&map, &nelem, &mapsz, fcs, tcs))
			break;	/* Malformed */

		/*
		 * No use wasting any more memory, there won't be any matching
		 * entries.  There's not even any use in validating the rest of
		 * the file since we just drop errors silently.
		 */
		if (strcmp(fcs, "*") == 0)
			break;
	}

	/*
	 * Malformed files will just roll with whatever we did manage to grab,
	 * so we'll do the same.
	 */
	*oelem = nelem;
	return (map);
}

static struct charset_map_entry *
libcharset_build_map(size_t *oelem)
{
	const char *aliasdir;
	FILE *fp = NULL;
	struct charset_map_entry *map = NULL;
	int dfd = -1, fd = -1;

	aliasdir = getenv("CHARSETALIASDIR");
	if (aliasdir == NULL) {
		/*
		 * Use the built-in map, which has just one wildcard element
		 * here.
		 */
		*oelem = 1;
		return (&charset_mappings_static[0]);
	}

	/*
	 * We have a new CHARSETALIASDIR, any failure from here indicates an
	 * empty map.  We won't bother zapping the default entry, we'll just
	 * overwrite it.
	 */
	if (aliasdir[0] == '\0')
		goto out;

	/* We don't bubble up errors, just use an empty map. */
	dfd = open(aliasdir, O_RDONLY | O_DIRECTORY);
	if (dfd == -1)
		goto out;

	fd = openat(dfd, LOCALE_CHARSET_FILE, O_RDONLY);
	if (fd == -1 || (fp = fdopen(fd, "r")) == NULL)
		goto out;

	map = libcharset_build_map_fp(fp, oelem);
	assert(map != NULL);

out:
	if (map == NULL)
		*oelem = 0;
	if (fp != NULL)
		fclose(fp);
	if (dfd != -1)
		close(dfd);

	return (map);
}

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void
libcharset_set_map(void *map, size_t nelems)
{

	/*
	 * With map == NULL, we need to release the previous mappings so that we
	 * can keep going.  In normal operation, the map never gets freed once
	 * we build it.
	 */
	if (map == NULL) {
		bool map_in_data = charset_map == &charset_mappings_static[0];

		if (!map_in_data ||
		    charset_map[0].fromname != &default_from[0]) {
			struct charset_map_entry *entry;

			for (size_t i = 0; i < charset_elems; i++) {
				entry = &charset_map[i];

				free(entry->fromname);
				free(entry->toname);
			}
		}

		if (!map_in_data)
			free(charset_map);

		charset_map = NULL;
		charset_elems = 0;
		return;
	}

	charset_map = map;
	charset_elems = nelems;
}
#endif

static struct charset_map_entry *
libcharset_get_map(size_t *oelem)
{

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	assert(charset_map != NULL);
#endif
	if (charset_map == NULL)
		charset_map = libcharset_build_map(&charset_elems);

	*oelem = charset_elems;
	return (charset_map);
}

static const char *
locale_charset_mapped(const char *name)
{
	struct charset_map_entry *entry, *map;
	size_t nelem;

	map = libcharset_get_map(&nelem);
	assert(map != NULL || nelem == 0);

	for (size_t i = 0; i < nelem; i++) {
		entry = &map[i];

		/* Catch-all; use it. */
		if (strcmp(entry->fromname, "*") == 0)
			return (entry->toname);
		else if (strcmp(entry->fromname, name) == 0)
			return (entry->toname);
	}

	/* No wildcard, no entry, pass it back as-is. */
	return (name);
}

const char *
locale_charset(void)
{
	bool chkenv;
	const char *csname = NULL, *ret = NULL;

	/*
	 * We'll check the obscure environment variable for a codeset to use
	 * instead of whatever nl_langinfo returns on internal builds just to
	 * simplify our testing flow.  It's not exactly crucial that we don't
	 * do this on customer builds, it's just that there doesn't seem to be
	 * a compelling reason to allow an override that GNU libcharset does
	 * not.
	 */
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	chkenv = true;
#else
	chkenv = os_variant_has_internal_content(LIBCHARSET_SUBSYSTEM);
#endif

	if (chkenv)
		csname = getenv("LIBCHARSET_CODESET");
	if (csname == NULL)
		csname = nl_langinfo(CODESET);

	ret = locale_charset_mapped(csname);
	assert(ret[0] != '\0');
	return (ret);
}

void
libcharset_set_relocation_prefix(const char *a __unused, const char *b __unused)
{
	/*
	 * Binary compatible stub to match what libiconv does with its
	 * equivalent libiconv_set_relocation_prefix.
	 */
}

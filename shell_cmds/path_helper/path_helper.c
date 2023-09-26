/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#include <err.h>
#include <fts.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "usage: path_helper [-c | -s]");
	exit(1);
}

// Append path segment if it does not exist.  Reallocate
// the path buffer as necessary.
static int
append_path_segment(char **path, const char *segment)
{
	if (*path == NULL || segment == NULL)
		return -1;

	size_t pathlen = strlen(*path);
	size_t seglen = strlen(segment);

	if (seglen == 0)
		return 0;

	// Does the segment already exist in the path?
	// (^|:)segment(:|$)
	char *match = strstr(*path, segment);
	while (match) {
		if ((match == *path || match[-1] == ':') &&
		    (match[seglen] == ':' || match[seglen] == 0)) {
			return 0;
		}
		match = strstr(match + 1, segment);
	}

	// size = pathlen + ':' + segment + '\0'
	size_t size = pathlen + seglen + 2;
	*path = reallocf(*path, size);
	if (*path == NULL)
		return -1;

	if (pathlen > 0)
		strlcat(*path, ":", size);
	strlcat(*path, segment, size);
	return 0;
}

// Convert fgetln output into a sanitized segment
// escape quotes, dollars, etc.
static char *
read_segment(const char *line, size_t len)
{
	int escapes = 0;
	size_t i, j;

	for (i = 0; i < len; ++i) {
		char c = line[i];
		if (c == '\"' || c == '\'' || c == '$') {
			++escapes;
		}
	}

	size_t size = len + escapes + 1;

	char *segment = calloc(1, size);
	if (segment == NULL)
		return NULL;

	for (i = 0, j = 0; i < len; ++i, ++j) {
		char c = line[i];
		if (c == '\"' || c == '\'' || c == '$') {
			segment[j++] = '\\';
			segment[j] = c;
		} else if (c == '\n') {
			segment[j] = 0;
			break;
		} else {
			segment[j] = line[i];
		}
	}

	return segment;
}

// Sort FTS results in lexicographical order.
static int
find_compare(const FTSENT **s1, const FTSENT **s2)
{
	long n1, n2;
	char *e1, *e2;

	// check for numerical prefixes
	n1 = strtol((*s1)->fts_name, &e1, 10);
	n2 = strtol((*s2)->fts_name, &e2, 10);
	if (e1 > (*s1)->fts_name && e2 > (*s2)->fts_name) {
		// order by numerical prefix
		if (n1 > n2)
			return 1;
		if (n2 > n1)
			return -1;
		// if equal, order by remainder
		return (strcoll(e1, e2));
	}
	return (strcoll((*s1)->fts_name, (*s2)->fts_name));
}

// Construct a path variable, starting with the contents
// of the given environment variable, adding the contents
// of the default file and files in the path directory.
static char *
construct_path(char *env_var, char *defaults_path, char *dir_path)
{
	FTS *fts;
	FTSENT *ent;

	char *result = calloc(sizeof(char), 1);

	char *dirpathv[] = { defaults_path, dir_path, NULL };

	char *root = getenv("PATH_HELPER_ROOT");
	if (root != NULL) {
		if (asprintf(&dirpathv[0], "%s%s", root, defaults_path) < 0 ||
		    asprintf(&dirpathv[1], "%s%s", root, dir_path) < 0) {
			err(1, NULL);
		}
	}

	fts = fts_open(dirpathv, FTS_PHYSICAL | FTS_XDEV, find_compare);
	if (!fts) {
		perror(dir_path);
		return NULL;
	}

	while ((ent = fts_read(fts)) != NULL) {
		// only interested in regular files, one level deep
		if (ent->fts_info != FTS_F) {
			if (ent->fts_level >= 1)
				fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		FILE *f = fopen(ent->fts_accpath, "r");
		if (f == NULL) {
			warn("%s", ent->fts_accpath);
			continue;
		}

		for (;;) {
			size_t len;
			char *line = fgetln(f, &len);
			if (line == NULL)
				break;
			char *segment = read_segment(line, len);

			append_path_segment(&result, segment);
		}

		fclose(f);
	}
	fts_close(fts);

	// merge in any existing custom PATH elemenets
	char *str = getenv(env_var);
	if (str)
		str = strdup(str);
	while (str) {
		char *sep = strchr(str, ':');
		if (sep)
			*sep = 0;

		append_path_segment(&result, str);
		if (sep) {
			str = sep + 1;
		} else {
			str = NULL;
		}
	}

	return result;
}

int
main(int argc, char *argv[])
{
	enum { STYLE_CSH, STYLE_SH } style = STYLE_SH;
	int opt;

	// default to csh style, if $SHELL ends with "csh".
	char *shell = getenv("SHELL");
	if (shell != NULL) {
		char *str = strstr(shell, "csh");
		if (str)
			style = STYLE_CSH;
	}

	while ((opt = getopt(argc, argv, "cs")) != -1) {
		switch (opt) {
		case 'c':
			style = STYLE_CSH;
			break;
		case 's':
			style = STYLE_SH;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	char *path = construct_path("PATH", "/etc/paths", "/etc/paths.d");
	char *manpath = NULL;

	// only adjust manpath if already set
	bool do_manpath = (getenv("MANPATH") != NULL);
	if (do_manpath) {
		manpath = construct_path("MANPATH", "/etc/manpaths",
		    "/etc/manpaths.d");
	}

	if (style == STYLE_CSH) {
		printf("setenv PATH \"%s\";\n", path);
		if (do_manpath)
			printf("setenv MANPATH \"%s\":;\n", manpath);
	} else {
		printf("PATH=\"%s\"; export PATH;\n", path);
		if (do_manpath)
			printf("MANPATH=\"%s:\"; export MANPATH;\n", manpath);
	}

	return 0;
}

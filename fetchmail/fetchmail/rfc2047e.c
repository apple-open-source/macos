/*
    rfc2047e.c - encode a string as per RFC-2047
    Copyright (C) 2004  Matthias Andree

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define _GNU_SOURCE
#include "fetchmail.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static const char noenc[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
static const char encchars[] = "!\"#$%&'*+,-./0123456789:;<>@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^`abcdefghijklmnopqrstuvwxyz{|}~";
static const char ws[] = " \t\r\n";

#ifdef TEST
void report (FILE *fp, const char *format, ...) { (void)fp; (void)format;}
#endif

static int needs_enc(const char *string) {
    if (strspn(string, noenc) < strlen(string))
	return 1;
    if (strncmp(string, "=?", 2) == 0
	    && strcmp(string + strlen(string) - 2, "?=") == 0)
	return 1;
    return 0;
}

static char *encode_words(char *const *words, int nwords, const char *charset)
{
    char *out, *t, *v;
    size_t l = 0;
    int i;

    for (i = 0; i < nwords; i++)
	l += strlen(words[i]) * 3; /* worst case, encode everything */
    l += (strlen(charset) + 8) * (l/60 + 1);

    out = v = (char *)xmalloc(l);
    t = stpcpy(out, "=?");
    t = stpcpy(t, charset);
    t = stpcpy(t, "?Q?");
    for (i = 0; i < nwords; i++) {
	const char *u;
	for (u = words[i]; *u; u++) {
	    if (t - v >= 69) {
		t = stpcpy(t, "?=\r\n=?");
		v = t - 2;
		t = stpcpy(t, charset);
		t = stpcpy(t, "?Q?");
	    }
	    if (*u == ' ') { *t++ = '_'; continue; }
	    if (strchr(encchars, *u)) { *t++ = *u; continue; }
	    sprintf(t, "=%02X", (unsigned int)((unsigned char)*u));
	    t += 3;
	}
    }
    strcpy(t, "?=");
    return out;
}

/** RFC-2047 encode string with given charset. Only the Q encoding
 * (quoted-printable) supported at this time.
 * WARNING: this code returns a static buffer!
 */
char *rfc2047e(const char *string, const char *charset) {
    static char *out;
    char *t;
    const char *r;
    int count, minlen, idx, i;
    char **words = NULL;
    size_t l;

    assert(strlen(charset) < 40);
    if (out) {
	free(out);
	out = NULL;
    }

    /* phase 1: split original into words */
    /* 1a: count, 1b: copy */
    count = 0;
    r = string;
    while (*r) {
	count++;
	r += strcspn(r, ws);
	if (!*r) break;
	count++;
	r += strspn(r, ws);
    }
    words = (char **)xmalloc(sizeof(char *) * (count + 1));

    idx = 0;
    r = string;
    while (*r) {
	l = strcspn(r, ws);
	words[idx] = (char *)xmalloc(l+1);
	memcpy(words[idx], r, l);
	words[idx][l] = '\0';
	idx++;
	r += l;
	if (!*r) break;
	l = strspn(r, ws);
	words[idx] = (char *)xmalloc(l+1);
	memcpy(words[idx], r, l);
	words[idx][l] = '\0';
	idx++;
	r += l;
    }

    /* phase 2: encode words */
    /* a: find ranges of adjacent words to need encoding */
    /* b: encode ranges */

    idx = 0;
    while (idx < count) {
	int end; char *tmp;

	if (!needs_enc(words[idx])) {
	    idx += 2;
	    continue;
	}
	for (end = idx + 2; end < count; end += 2) {
	    if (!needs_enc(words[end]))
		break;
	}
	end -= 2;
	tmp = encode_words(&words[idx], end - idx + 1, charset);
	free(words[idx]);
	words[idx] = tmp;
	for (i = idx + 1; i <= end; i++)
	    words[i][0] = '\0';
	idx = end + 2;
    }

    l = 0;
    for (idx = 0; idx < count; idx++) {
	l += strlen(words[idx]);
    }

    /* phase 3: limit lengths */
    minlen = strlen(charset) + 7;
    /* allocate ample memory */
    out = (char *)xmalloc(l + (l / (72 - minlen) + 1) * (minlen + 2) + 1);

    if (count)
	t = stpcpy(out, words[0]);
    else
	t = out, *out = 0;

    l = strlen(out);

    for (i = 1; i < count; i+=2) {
	size_t m;
	char *tmp;

	m = strlen(words[i]);
	if (i + 1 < count)
	    m += strcspn(words[i+1], "\r\n");
	if (l + m > 74)
	    l = 0, t = stpcpy(t, "\r\n");
	t = stpcpy(t, words[i]);
	if (i + 1 < count) {
	    t = stpcpy(t, words[i+1]);
	}
	tmp = strrchr(out, '\n');
	if (tmp == NULL)
	    tmp = out;
	else
	    tmp++;
	l = strlen(tmp);
    }

    /* free memory */
    for (i = 0; i < count; i++) free(words[i]);
    free(words);
    return out;
}

#ifdef TEST
int main(int argc, char **argv) {
    char *t;

    if (argc > 1) {
	t = rfc2047e(argv[1], argc > 2 ? argv[2] : "utf-8");
	printf( " input: \"%s\"\n"
		"output: \"%s\"\n", argv[1], t);
	free(t);
    }
    return EXIT_SUCCESS;
}
#endif

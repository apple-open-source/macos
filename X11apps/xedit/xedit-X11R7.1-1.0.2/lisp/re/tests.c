/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86$ */

/*
 * Compile with:	cc -o tests tests.c -L. -lre
 */

#include <stdio.h>
#include <string.h>
#include "re.h"

int
main(int argc, char *argv[])
{
    re_cod cod;
    re_mat mat[10];
    int line, ecode, i, len, group, failed;
    long eo, so;
    char buf[8192];
    char str[8192];
    FILE *fp = fopen("tests.txt", "r");

    if (fp == NULL) {
	fprintf(stderr, "failed to open tests.txt\n");
	exit(1);
    }

    ecode = line = group = failed = 0;
    cod.cod = NULL;
    while (fgets(buf, sizeof(buf), fp)) {
	++line;
	if (buf[0] == '#' || buf[0] == '\n')
	    continue;
	else if (buf[0] == '/') {
	    char *ptr = strrchr(buf, '/');

	    if (ptr == buf) {
		fprintf(stderr, "syntax error at line %d\n", line);
		break;
	    }
	    else {
		int flags = 0;

		refree(&cod);
		for (*ptr++ = '\0'; *ptr; ptr++) {
		    if (*ptr == 'i')
			flags |= RE_ICASE;
		    else if (*ptr == 'n')
			flags |= RE_NEWLINE;
		}
		ecode = recomp(&cod, buf + 1, flags);
		failed = ecode;
	    }
	}
	else if (buf[0] == '>') {
	    if (cod.cod == NULL) {
		fprintf(stderr, "no previous pattern at line %d\n", line);
		break;
	    }
	    len = strlen(buf) - 1;
	    buf[len] = '\0';
	    strcpy(str, buf + 1);
	    for (i = 0, --len; i < len - 1; i++) {
		if (str[i] == '\\') {
		    memmove(str + i, str + i + 1, len);
		    --len;
		    switch (str[i]) {
			case 'a':
			    str[i] = '\a';
			    break;
			case 'b':
			    str[i] = '\b';
			    break;
			case 'f':
			    str[i] = '\f';
			    break;
			case 'n':
			    str[i] = '\n';
			    break;
			case 'r':
			    str[i] = '\r';
			    break;
			case 't':
			    str[i] = '\t';
			    break;
			case 'v':
			    str[i] = '\v';
			    break;
			default:
			    break;
		    }
		}
	    }
	    group = 0;
	    ecode = reexec(&cod, str, 10, &mat[0], 0);
	    if (ecode && ecode != RE_NOMATCH) {
		reerror(failed, &cod, buf, sizeof(buf));
		fprintf(stderr, "%s, at line %d\n", buf, line);
		break;
	    }
	}
	else if (buf[0] == ':') {
	    if (failed) {
		len = strlen(buf) - 1;
		buf[len] = '\0';
		if (failed == RE_EESCAPE && strcmp(buf, ":EESCAPE") == 0)
		    continue;
		if (failed == RE_ESUBREG && strcmp(buf, ":ESUBREG") == 0)
		    continue;
		if (failed == RE_EBRACK && strcmp(buf, ":EBRACK") == 0)
		    continue;
		if (failed == RE_EPAREN && strcmp(buf, ":EPAREN") == 0)
		    continue;
		if (failed == RE_EBRACE && strcmp(buf, ":EBRACE") == 0)
		    continue;
		if (failed == RE_EBADBR && strcmp(buf, ":EBADBR") == 0)
		    continue;
		if (failed == RE_ERANGE && strcmp(buf, ":ERANGE") == 0)
		    continue;
		if (failed == RE_ESPACE && strcmp(buf, ":ESPACE") == 0)
		    continue;
		if (failed == RE_BADRPT && strcmp(buf, ":BADRPT") == 0)
		    continue;
		if (failed == RE_EMPTY && strcmp(buf, ":EMPTY") == 0)
		    continue;
		reerror(failed, &cod, buf, sizeof(buf));
		fprintf(stderr, "Error value %d doesn't match: %s, at line %d\n",
			failed, buf, line);
		break;
	    }
	    else if (!ecode) {
		fprintf(stderr, "found match when shoudn't, at line %d\n", line);
		break;
	    }
	}
	else {
	    if (failed) {
		reerror(failed, &cod, buf, sizeof(buf));
		fprintf(stderr, "%s, at line %d\n", line);
		break;
	    }
	    if (sscanf(buf, "%ld,%ld:", &so, &eo) != 2) {
		fprintf(stderr, "expecting match offsets at line %d\n", line);
		break;
	    }
	    else if (ecode) {
		fprintf(stderr, "didn't match, at line %d\n", line);
		break;
	    }
	    else if (group >= 10) {
		fprintf(stderr, "syntax error at line %d (too many groups)\n",
			line);
		break;
	    }
	    else if (so != mat[group].rm_so || eo != mat[group].rm_eo) {
		fprintf(stderr, "match failed at line %d, got %ld,%ld: ",
			line, mat[group].rm_so, mat[group].rm_eo);
		if (mat[group].rm_so < mat[group].rm_eo)
		    fwrite(str + mat[group].rm_so,
		 	   mat[group].rm_eo - mat[group].rm_so, 1, stderr);
		fputc('\n', stderr);
		break;
	    }
	    ++group;
	}
    }

    fclose(fp);

    return (ecode);
}

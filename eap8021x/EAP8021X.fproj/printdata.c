
/*
 * Copyright (c) 2001-2008 Apple Inc. All rights reserved.
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

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "printdata.h"

void
fprint_bytes(FILE * out_f, const u_char * data_p, int n_bytes)
{
    int i;

    if (out_f == NULL) {
	out_f = stdout;
    }
    for (i = 0; i < n_bytes; i++) {
	char * space;

	if (i == 0) {
	    space = "";
	}
	else if ((i % 8) == 0) {
	    space = "  ";
	}
	else {
	    space = " ";
	}
	fprintf(out_f, "%s%02x", space, data_p[i]);
    }
    fflush(out_f);
    return;
}

void
fprint_data(FILE * out_f, const u_char * data_p, int n_bytes)
{
    if (out_f == NULL) {
	out_f = stdout;
    }
#define CHARS_PER_LINE 	16
    char		line_buf[CHARS_PER_LINE + 1];
    int			line_pos;
    int			offset;

    for (line_pos = 0, offset = 0; offset < n_bytes; offset++, data_p++) {
	if (line_pos == 0)
	    fprintf(out_f, "%04x ", offset);

	line_buf[line_pos] = isprint(*data_p) ? *data_p : '.';
	fprintf(out_f, " %02x", *data_p);
	line_pos++;
	if (line_pos == CHARS_PER_LINE) {
	    line_buf[CHARS_PER_LINE] = '\0';
	    fprintf(out_f, "  %s\n", line_buf);
	    line_pos = 0;
	}
	else if (line_pos == (CHARS_PER_LINE / 2))
	    fprintf(out_f, " ");
    }
    if (line_pos) { /* need to finish up the line */
	char * extra_space = "";
	if (line_pos < (CHARS_PER_LINE / 2)) {
	    extra_space = " ";
	}
	for (; line_pos < CHARS_PER_LINE; line_pos++) {
	    fprintf(out_f, "   ");
	    line_buf[line_pos] = ' ';
	}
	line_buf[CHARS_PER_LINE] = '\0';
	fprintf(out_f, "  %s%s\n", extra_space, line_buf);
    }
    fflush(out_f);
    return;
}

void
print_bytes(const u_char * data, int len)
{
    fprint_bytes(NULL, data, len);
}

void
print_data(const u_char * data, int len)
{
    fprint_data(NULL, data, len);
}

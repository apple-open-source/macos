/* $Xorg: ind_token.c,v 1.3 2000/08/17 19:42:13 cpqbld Exp $ */

/***********************************************************

Copyright (c) 1990, 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

Copyright (c) 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Sun Microsystems,
and the X Consortium, not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/



#include <stdio.h>
#include "indexer.h"

/*
 * Get a token from an input line.
 * Tokens are delimited by DELIM characters.
 */
char	*
get_field(line, char_p)
	char	*line;
	int	*char_p;
{

	char	*start_pos;

	skip_space(line, char_p);
	if (line[*char_p] == '\0')
		return("");
	start_pos = line + *char_p;
	while (line[*char_p] != DELIM && line[*char_p] != '\0' &&
		 line[*char_p] != '\n')
		(*char_p)++;
	line[(*char_p)++] = '\0';
	return(start_pos);
}

/*
 * Skip white space
 */
skip_space(line, char_p)
	char	*line;
	int	*char_p;
{
	while (line[*char_p] == SPACE) {
		(*char_p)++;
	}
}


/*
 * Skip the initial starting string
 */
int
skip_start(line, char_p)
	char	*line;
	int	*char_p;
{
	char	*field;

	field = get_field(line, char_p);
	if (strcmp(field, ".IE") == 0)		/*  Found correct start */
		return (TRUE);
	else if (strcmp(field, ".SE") == 0)	/*  Found special entry */
		return (TRUE);
	else {
		fprintf(stderr,
		"%s: line %d should start with .IE or .SE - line ignored\n",
		command_name, line_number);
		return (FALSE);
	}
}

/*
 * Obtain the type of the index entry
 */
int
get_index_type(line, char_p)
	char	*line;
	int	*char_p;
{

	char	*field;


	field = get_field(line, char_p);
	if (strcmp(field, "ENTRY") == 0)
		return(ENTRY);
	else if (strcmp(field, "DOCUMENT") == 0)
		return(DOCUMENT);
	else {
		fprintf(stderr,
		"%s: line %d unknown index type - line ignored\n",
		command_name, line_number);
	return(0);			/*  Incorrect Type  */
	}
}

/*
 * Obtain the type of the page entry
 */
int
get_page_type(line, char_p)
	char	*line;
	int	*char_p;
{

	char	*field;


	field = get_field(line, char_p);
	if (strcmp(field, "") == 0) {
		field = get_field(line, char_p);	/*  skip null field  */
		return(PAGE_NORMAL);
	}
	else if (strcmp(field, "PAGE") == 0) {
		field = get_field(line, char_p);
		if (strcmp(field, "NORMAL") == 0)
			return(PAGE_NORMAL);
		else if (strcmp(field, "MAJOR") == 0)
			return(PAGE_MAJOR);
		else if (strcmp(field, "START") == 0)
			return(PAGE_START);
		else if (strcmp(field, "END") == 0)
			return(PAGE_END);
		else
			return(0);		/*  Incorrect Page Type  */
	}
	else if (strcmp(field, "PRINT") == 0) {
		return(PAGE_PRINT);
	} else {
		fprintf(stderr,
		"%s: line %d unknown page type - line ignored\n",
		command_name, line_number);
		return(0);			/*  Incorrect Type  */
	}
}

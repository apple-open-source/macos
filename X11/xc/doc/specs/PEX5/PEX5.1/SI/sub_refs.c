/* $Xorg: sub_refs.c,v 1.3 2000/08/17 19:42:14 cpqbld Exp $ */

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
#include <ctype.h>
#include "xref.h"

struct codeword_entry	*root;


substitute_references(line, char_p)
	char	line[MAXLINE];
	int	*char_p;
{
	int	reference_type;
	char	current_token[MAXLINE];
	char	temp_token[MAXLINE];
	int	token_type;
	struct codeword_entry	*node;

	for (token_type = get_token(line, char_p, current_token, SKIP_SPACES);
		 token_type != ENDOFLINE_TOKEN;
		 token_type = get_token(line, char_p, current_token, DONT_SKIP_SPACES)) {

		if (token_type != DELIMITER_TOKEN) {
			printf("%s", current_token);
			continue;
		}
					 /*  Look for @ sign  */
		if (strcmp(current_token, "@") != 0) {
			printf("%s", current_token);
			continue;
		}
					/*  Cater to doubled up @ signs  */
		token_type = get_token(line, char_p,
					current_token, DONT_SKIP_SPACES);
		if (token_type == ENDOFLINE_TOKEN)
			break;
		if (strcmp(current_token, "@") == 0) {
			printf("%s", current_token);
			continue;
		}
		if (token_type != ALPHA_TOKEN) {
			printf("@%s", current_token);
			continue;
		}
		if (SCComp(current_token, "numberof") == TRUE)
			reference_type = NUMBER;
		else if (SCComp(current_token, "titleof") == TRUE)
			reference_type = TITLE;
		else if (SCComp(current_token, "pagenumber") == TRUE)
			reference_type = TITLE;
		else {
			fprintf(stderr, "%s: Unknown command %s at line %d of file %s\n",
			command_name, current_token, line_number, current_filename);
			printf("@%s", current_token);
			continue;
		}
		token_type = get_token(line, char_p, current_token, DONT_SKIP_SPACES);
		if (token_type != DELIMITER_TOKEN) {
			printf("%s", current_token);
			continue;
		}
		if (strcmp(current_token, "(") != 0) {
			if (reference_type == NUMBER)
				printf("@NumberOf%s", current_token);
			else
				printf("@TitleOf%s", current_token);
			continue;
		}
		token_type = get_token(line, char_p, current_token, DONT_SKIP_SPACES);
		if (token_type != ALPHA_TOKEN) {
			if (reference_type == NUMBER)
				printf("@NumberOf(%s", current_token);
			else
				printf("@TitleOf(%s", current_token);
			continue;
		}
					/*  Locate codeword in database  */
		if ((node = locate_codeword_entry(root, current_token)) == NULL) {
			fprintf(stderr, "%s: Unknown codeword %s at line %d of file %s\n",
			command_name, current_token, line_number, current_filename);
			if (reference_type == NUMBER)
				printf("@NumberOf(%s", current_token);
			else
				printf("@TitleOf(%s", current_token);
			continue;
		}
		strcpy(temp_token, current_token);
		token_type = get_token(line, char_p, current_token, DONT_SKIP_SPACES);
		if (token_type != DELIMITER_TOKEN) {
			if (reference_type == NUMBER)
				printf("@NumberOf(%s%s", temp_token, current_token);
			else
				printf("@TitleOf(%s%s", temp_token, current_token);
			continue;
		}
		if (strcmp(current_token, ")") != 0) {
			if (reference_type == NUMBER)
				printf("@NumberOf(%s%s", temp_token, current_token);
			else
				printf("@TitleOf(%s%s", temp_token, current_token);
			continue;
		}
		print_reference(node, reference_type);
	}
	putchar('\n');
}


int
try_reference_line(line, char_p)
	char	*line;
	int	char_p;
{
	char	current_token[MAXLINE];
	int	token_type;

					/*  Look for .  */
	token_type = get_token(line, char_p, current_token, SKIP_SPACES);
	if (token_type != DELIMITER_TOKEN)
		return (FALSE);
	if (strcmp(current_token, ".") != 0)
		return (FALSE);
					/*  Look for XR  */
	token_type = get_token(line, char_p, current_token, DONT_SKIP_SPACES);
	if (token_type != ALPHA_TOKEN)
		return (FALSE);
	if (strcmp(current_token, "XR") != 0)
		return (FALSE);

	return (TRUE);
}

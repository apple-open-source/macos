/* $Xorg: token.c,v 1.3 2000/08/17 19:42:14 cpqbld Exp $ */

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


/*
 * Get a token from an input line.
 * Tokens are delimited by runs of spaces
 * Tokens can be enclosed in " signs
 */
int
get_token(line, char_p, token, treat_spaces)
	char	*line;
	int	*char_p;
	char	token[];
	int  	treat_spaces;
{

	int	tok_pos;
	int	quoted;

	token[0] = '\0';
	tok_pos = 0;

	if (line[*char_p] == '\0' || line[*char_p] == '\n')
		return(ENDOFLINE_TOKEN);
					/*  Either remove runs of whitespace  */
					/*  or return the stuff  */
	if (treat_spaces == SKIP_SPACES) {
		while (line[*char_p] == SPACE || line[*char_p] == TAB)
			(*char_p)++;
	} else {
		if (isspace(line[*char_p])) {
			while (line[*char_p] == SPACE || line[*char_p] == TAB)
				token[tok_pos++] = line[(*char_p)++];
			token[tok_pos] = '\0';
			return(SPACES_TOKEN);
		}
	}
					/*  Treat delimiters here  */
	if (ispunct(line[*char_p])) {
		if (line[*char_p] != '"') {
			token[tok_pos++] = line[(*char_p)++];
			token[tok_pos] = '\0';
			return(DELIMITER_TOKEN);
		}
	}

					/*  Treat quoted strings here  */
	if (line[*char_p] == '"') {
		(*char_p)++;
		while (line[*char_p] != '\0' && line[*char_p] != '\n') {
			token[tok_pos] = line[(*char_p)++];
					/*  Check for doubled up quotes  */
			if (token[tok_pos] != '"') {
				tok_pos++;
			} else {
				if (line[*char_p] == '"') {
					tok_pos++;
					(*char_p)++;
				} else {
					token[tok_pos] = '\0';
					return(STRING_TOKEN);
				}
			}
		}
		token[tok_pos] = '\0';
		return(STRING_TOKEN);
	}
					/*  Treat Alphanumerics here  */
	if (isalpha(line[*char_p])) {
		while (line[*char_p] != '\0' && line[*char_p] != '\n') {
			token[tok_pos] = line[*char_p];
			if (!(isalnum(line[*char_p]) || line[*char_p] == '_')) {
				token[tok_pos] = '\0';
				return(ALPHA_TOKEN);
			}
			tok_pos++;
			(*char_p)++;
		}
		token[tok_pos] = '\0';
		return(ALPHA_TOKEN);
	}
					/*  Treat numerics here  */
	if (isdigit(line[*char_p])) {
		while (line[*char_p] != '\0' && line[*char_p] != '\n') {
			token[tok_pos] = line[*char_p];
			if (!isdigit(line[*char_p])) {
				token[tok_pos] = '\0';
				return(NUMBER_TOKEN);
			}
			tok_pos++;
			(*char_p)++;
		}
		token[tok_pos] = '\0';
		return(NUMBER_TOKEN);
	}
}


/*
 * Obtain the type of the codeword entry
 */
int
get_codeword_type(line, char_p)
	char	*line;
	int	*char_p;
{

	char	token[MAXLINE];
	int	token_type;


	token_type = get_token(line, char_p, token, SKIP_SPACES);
	if (token_type != DELIMITER_TOKEN)
		return(0);
	token_type = get_token(line, char_p, token, DONT_SKIP_SPACES);
	if (token_type != ALPHA_TOKEN)
		return(0);
	if (strcmp(token, "H") == 0)
		return(HEADING);
	else if (strcmp(token, "TN") == 0)
		return(TABLE);
	else if (strcmp(token, "FN") == 0)
		return(FIGURE);
	else {
		return(0);			/*  Not a type we want Type  */
	}
}



int
SCComp(string_1, string_2)
	char *string_1, *string_2;
{
	char char_1, char_2;

	if (strlen(string_1) != strlen(string_2))
		return(FALSE);

	while(*string_1 != '\0') {
		char_1 = *string_1++;
		char_2 = *string_2++;
		if (isupper(char_1))
			char_1 = tolower(char_1);
		if (isupper(char_2))
			char_2 = tolower(char_2);
		if (char_1 != char_2)
			return(FALSE);
	}
	return(TRUE);
}

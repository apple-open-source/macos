/* $Xorg: dump_cw.c,v 1.3 2000/08/17 19:42:13 cpqbld Exp $ */

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


#include <ctype.h>
#include <stdio.h>
#include "xref.h"



/*  dump_codeword_entry dumps a codeword entry in
    the tree of codeword entries and returns the address of the node
    corresponding to the entry dumpd.  */

dump_codewords(node)
	struct codeword_entry	*node;
{


	if (node == NULL)
		return;
	dump_codewords(node->lesser);
	print_node(node);
	dump_codewords(node->greater);
}

print_node(node)
	struct codeword_entry	*node;
{
	char	out_string[MAXLINE];
	char	rest_string[MAXLINE];


	printf("codeword = %s\n", node->codeword);
	printf("%s\n", node->title);
			/*  Handle first level conversion specially */
	if (node->appendix)
		number_to_letters(node->h1_counter, out_string);
	else
		sprintf(out_string, "%d", node->h1_counter);

	strcpy(rest_string, "");
	if (node->entry_type == HEADING) {
		if (node->h5_counter != 0)
			sprintf(rest_string, ".%d.%d.%d.%d", node->h2_counter,
			node->h3_counter, node->h4_counter, node->h5_counter);
		else if (node->h4_counter != 0)
			sprintf(rest_string, ".%d.%d.%d", node->h2_counter,
			node->h3_counter, node->h4_counter);
		else if (node->h3_counter != 0)
			sprintf(rest_string, ".%d.%d", node->h2_counter,
			node->h3_counter);
		else if (node->h2_counter != 0)
			sprintf(rest_string, ".%d", node->h2_counter);
		strcat(out_string, rest_string);
		
	}
	else if (node->entry_type == TABLE) {
		if (document_type == MAJOR_SECTIONED) {
			sprintf(rest_string, "-%d", node->table_number);
			strcat(out_string, rest_string);
		}
		else
			sprintf(out_string, "%d", node->table_number);
	}
	else if (node->entry_type == FIGURE) {
		if (document_type == MAJOR_SECTIONED) {
			sprintf(rest_string, "-%d", node->figure_number);
			strcat(out_string, rest_string);
		}
		else
			sprintf(out_string, "%d", node->figure_number);
	}
	puts(out_string);
}

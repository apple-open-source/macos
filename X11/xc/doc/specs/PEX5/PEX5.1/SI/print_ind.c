/* $Xorg: print_ind.c,v 1.3 2000/08/17 19:42:14 cpqbld Exp $ */

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
#include "indexer.h"

#define	PRIMARY_INDENT		2
#define	SECONDARY_INDENT	6

print_index(node)
	struct index_entry	*node;
{


	if (node == NULL)
		return;
	print_index(node->lesser);
	print_node(node, previous_index_entry);
	previous_index_entry = node;
	print_index(node->greater);
}


/*
 * Print out an Index entry
 */
print_node(node, previous)
struct index_entry	*node;
struct index_entry	*previous;
{

	int	level;
	char	first_char;


/*
 *  We have to decide if this index entry is a new primary term or is the
 *  continuation of a primary term with a different secondary term.  If
 *  the primary term of this index entry differs from the primary term of
 *  the previous index entry, this is a new index entry and we have to set
 *  and unset various of the troff flags and change vertical spacing and so on.
 */

	if (previous == NULL)	/*  This is the very first entry  */
		level = PRIMARY;
	else
		if (strcmp(previous->terms[PRIMARY], node->terms[PRIMARY]) == 0)
			level = SECONDARY;	/*  Secondary terms differ  */
		else
			level = PRIMARY;	/*  Primary terms differ  */

/*
 *  We only need to print the primary index term if the level is PRIMARY, that is,
 *  this is a new primary entry.  If the level is SECONDARY, it means we are
 *  printing secondary terms and the primary term has already been printed
 *  sometime in the past.  If this is a new primary term, we have to do
 *  three things:
 *  1.  We have to check for a change in the first character of the primary
 *      index term so that we print the large bold-faced character as a
 *      caption for this term -- this is what gets us the letters of the
 *      alphabet as captions in the final index.
 *  2.  We have to set a continuation string consisting of the primary index
 *      term, so that when you have a list of secondary terms that start on a
 *      new page or a new column on the same page, you get the primary entry
 *      with the word 'continued' after it so that the reader is reassured
 *      as to what the primary entry is and also to lead the eye to the
 *      fact of a continued primary term.
 *  3.  We then actually want to print the primary index term.
 */
	if (level == PRIMARY) {
		set_continuation_string("");
		print_first_character(node);
		if (string_exists(node->print_field[PRIMARY]))
			set_continuation_string(node->print_field[PRIMARY]);
		else
			set_continuation_string(node->terms[PRIMARY]);
		print_primary_terms(node);
		if (string_exists(node->terms[SECONDARY]))
			printf("\n");
	}

/*
 *  Now we print any secondary terms associated with this primary term
 */
	if (string_exists(node->terms[SECONDARY]))
		print_secondary_terms(node);


/*
 *  Finally, we print the page numbers associated with this term.
 */
	print_pages(node);
}


print_first_character(node)
struct index_entry	*node;
{

	char	first_char;

	static char	current_first_char = '\0';

				/*  check for change in first character  */
				/*  so that we can print a caption  */
	first_char = node->terms[PRIMARY][0];
	if (isalpha(first_char))
		if (islower(first_char))
			first_char = toupper(first_char);
	if (ispunct(first_char)) {
		if (current_first_char == '\0') {
			printf(".sp\n");
			printf(".ne 3\n");
			indent(SECONDARY_INDENT - PRIMARY_INDENT, 0);
			printf("\\fISpecial Characters\\fP\n");
		}
	} else if (first_char != current_first_char) {
		printf(".sp\n");
		printf(".ne 3\n");
		indent(SECONDARY_INDENT - PRIMARY_INDENT, 0);
		printf("\\fB\\s+3%c\\s-3\\fP\n", first_char);
	}
	current_first_char = first_char;
}

set_continuation_string(string)
char	*string;
{

	static  char	*current_continuation_string = "";

	if (strcmp(string, current_continuation_string) != 0)
		current_continuation_string = string;
	if (strcmp(current_continuation_string, "") == 0)
		printf(".rm iC\n");
	else
		printf(".ds iC \\&%s, \\fIcontinued\\fP\n", current_continuation_string);
}


print_primary_terms(node)
struct index_entry	*node;
{


	set_index_level(PRIMARY);
	indent(PRIMARY_INDENT, PRIMARY_INDENT);
	change_vertical_spacing(VERTICAL_CHANGE);
	if (string_exists(node->print_field[PRIMARY]))
		printf("\\&%s", node->print_field[PRIMARY]);
	else
		printf("\\&%s", node->terms[PRIMARY]);
}


print_secondary_terms(node)
struct index_entry	*node;
{

	set_index_level(SECONDARY);
	indent(SECONDARY_INDENT, SECONDARY_INDENT - PRIMARY_INDENT);
	change_vertical_spacing(-VERTICAL_CHANGE);
	if (string_exists(node->print_field[SECONDARY]))
		printf("\\&%s", node->print_field[SECONDARY]);
	else
		printf("\\&%s", node->terms[SECONDARY]);
}


set_index_level(level)
	int	level;
{

	static	int	current = 0;

	if (level == current)
		return;
	else {
		current = level;
		printf(".nr iL  %d\n", current);
	}
}

change_vertical_spacing(amount)
	int	amount;
{

	static	int	current = VERTICAL_CHANGE;

	if (amount == current)
		return;
	else {
		if (amount > 0)
			printf(".vs +%d\n", amount);
		else
			printf(".vs %d\n", amount);
		current = amount;
	}
}

/*
 * Indent a specified number of levels 
 */
indent(indent_amount, temporary_indent)
	int	indent_amount;
	int	temporary_indent;
{
	static	int	current_indent;		/* Current indent position */

	if (indent_amount != current_indent) {
		printf("'in %d\n", indent_amount);
		current_indent = indent_amount;
	}
	if (temporary_indent > 0)
		printf(".ti -%d\n", temporary_indent);
}

/*
 * Un-indent a specified number of levels 
 */
unindent()
{
	printf(".in 0\n");
}

/*
 * Put out the page numbers associated with an index node.
 */
print_pages(node)
struct index_entry	*node;
{
	struct page_entry	*page;

	for (page = node->page_entry; page != NULL; page = page->next_page) {

			/*  Print normal page references  */
		if (page->page_type == PAGE_NORMAL || page->page_type == PAGE_PRINT)
			printf(", %s", page->page_number);

			/*  Print major page references in boldface text  */
		if (page->page_type == PAGE_MAJOR)
			printf(", \\fB%s\\fP", page->page_number);

			/*  Page ranges -- if a range starts and ends on
			    the same page we just print the starting page
			    number as a normal index entry and skip over 
			    the ending page number of the range  */
		if (page->page_type == PAGE_START) {
			if (page->next_page != NULL) {
				if (page->next_page->page_type == PAGE_END) {
					if (strcmp(page->page_number,
						page->next_page->page_number) == 0)
						printf(", %s", page->page_number);
					else
						printf(", %s \\fI\\s-1thru\\s+1\\fP %s",
							page->page_number,
							page->next_page->page_number);
					page = page->next_page;
				}
			} else {
				fprintf (stderr,
					"%s: unmatched range for terms \"%s\" \"%s\"\n",
					command_name, node->terms[PRIMARY], node->terms[SECONDARY]);
			}
		}
	}
	printf ("\n");
}

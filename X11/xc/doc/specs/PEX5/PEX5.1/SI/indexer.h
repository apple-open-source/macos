/* $Xorg: indexer.h,v 1.3 2000/08/17 19:42:13 cpqbld Exp $ */

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


#define LEVELS		2
#define PRIMARY		0
#define SECONDARY	1
#define	FALSE	0
#define	TRUE	1

/*
 *  Types of Index Entries.  There are three types today:
 *  ENTRY     means this is a straightforward index entry
 *  DOCUMENT  means this is a special entry describing the name of the
 *            document -- this is used when building the master index.
 */

#define	ENTRY		1
#define	DOCUMENT	3
/*
 * Structure for an index entry.  An index entry has the following components:
 * index_type  indicates the type of entry that this is, and is one of the
 *             three types listed above.
 * terms       are the primary and secondary collating terms for this entry.
 * greater     points to index entries that collate greater than this entry.
 * lesser      points to index entries that collate less than this entry.
 * equal       points to index entries that collate equal to this
 *             entry, but whose printing terms are different.
 * print_field is what is printed in place of the collating terms.  If the
 *             print_field entries are null, the collating terms are printed.
 * page_entry  is a list of page numbers for this entry.  last_page keeps
 *             track of the last page entry in the list.
 */
struct	index_entry	{
	int	index_type;		/*  Type of entry  */
	char	*terms[LEVELS];		/* Levels of terms */
	struct	index_entry	*greater;  /* Entries greater than this one */
	struct	index_entry	*lesser;  /* Entries less than this one */
	struct	index_entry	*equal;  /* Chain to equal index terms */
	char	*print_field[LEVELS];	/* What to print instead of the term */
	struct	page_entry	*page_entry;	/* List of page numbers */
	struct	page_entry	*last_page;	/* Last page entry */
	char	*document;	/* Name of document */
};


/*
 *  Types of Page Entries.  There are five types today:
 *  PAGE_NORMAL  means this is a normal page entry -- just print it with no
 *               special treatment.
 *  PAGE_MAJOR   means this is a principal page entry -- the page number is
 *               printed in bold faced text and first in the list of page numbers.
 *  PAGE_START   means this is the start of a page range.
 *  PAGE_END     means this is the end of a page range.
 *  PAGE_PRINT   means that the string following the word PRINT is printed
 *               instead of the page number on which this index entry appears.
 */
#define	PAGE_NORMAL	1
#define	PAGE_MAJOR	2
#define	PAGE_START	3
#define	PAGE_END	4
#define	PAGE_PRINT	5
struct	page_entry	{
	int	page_type;		/* What type of entry this is */
	char	*page_number;		/* A page number */
	struct	page_entry	*next_page;	/* Linked list */
};


int	line_number;		/*  Line number in input file  */
char	*command_name;		/*  Name of command  */
struct index_entry	*previous_index_entry;

#define MAXLINE	512
#define VERTICAL_CHANGE	1
#define DELIM	'\t'
#define SPACE	' '
#define TAB	'\t'

#define	strdup(str)	strcpy(malloc(strlen(str) + 1), str)
#define new(type)	(type *) calloc(sizeof(type), 1)
#define string_exists(arg)	(arg != NULL && strcmp(arg, "") != 0)

char	*malloc();
char	*calloc();
char	*strcpy();
char	get_char();
char	*get_field();
char	*skipspace();
struct index_entry	*get_index_terms();
struct page_entry	*get_page_number();
struct index_entry	*insert_index_entry();

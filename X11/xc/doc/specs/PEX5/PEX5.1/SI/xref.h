/* $Xorg: xref.h,v 1.3 2000/08/17 19:42:14 cpqbld Exp $ */

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


#define	FALSE	0
#define	TRUE	1

/*
 * Structure for a codeword entry
 */
struct	codeword_entry	{
	int	entry_type;		/*  Type of entry  */
	char	*codeword;		/*  Actual Codeword  */
	char	*title;			/*  Text of Title  */
	int	h1_counter;		/*  Chapter Level Counter  */
	int	h2_counter;		/*  Section Level Counter  */
	int	h3_counter;		/*  SubSection Level Counter  */
	int	h4_counter;		/*  Paragraph Level Counter  */
	int	h5_counter;		/*  SubParagraph Level Counter  */
	int	table_number;		/*  Table Number  */
	int	figure_number;		/*  Figure Number  */
	int	appendix;		/*  TRUE if this is an appendix  */
	int	page_number;		/*  Page Number (not yet available)  */
	struct	codeword_entry	*lesser;/*  pointer to lesser number */
	struct	codeword_entry	*greater;/*  pointer to greater number */
};

				/*  Codeword Types  */
#define	HEADING		1
#define	TABLE		2
#define	FIGURE		3
#define	CROSSREF	4
				/*  Phase of Processing  */
#define	GATHER_REFERENCES	1
#define	SUBSTITUTE_REFERENCES	2
				/*  Document Types  */
#define	MINOR_SECTIONED	1
#define	MAJOR_SECTIONED	2
				/*  Reference Types  */
#define	NUMBER	1
#define	TITLE	2
				/*  Instructions to the token reader  */
#define	SKIP_SPACES	1
#define	DONT_SKIP_SPACES	2
				/*  Types of tokens  */
#define	SPACES_TOKEN	1
#define	DELIMITER_TOKEN	2
#define	STRING_TOKEN	3
#define	ALPHA_TOKEN	4
#define	NUMBER_TOKEN	5
#define	ENDOFLINE_TOKEN	6

FILE	*current_file;		/*  Current input file  */
char	*current_filename;	/*  Name of current input file  */
int	line_number;		/*  Line number in current file  */
int	document_type;		/*  Major Sectioned or Minor Sectioned  */
char	*command_name;		/*  Name of command  */
struct codeword_entry	*previous_codeword_entry;

#define MAXLINE	512

#define SPACE	' '
#define TAB	'\t'

#define	strdup(str)	strcpy(malloc(strlen(str) + 1), str)
#define new(type)	(type *) calloc(sizeof(type), 1)
#define exists(arg)	(strcmp(arg, "") != 0)

char	*malloc();
char	*calloc();
char	*strcpy();
char	get_char();
char	*get_field();
char	*skipspace();
struct codeword_entry	*build_codeword_entry();
struct codeword_entry	*insert_codeword_entry();
struct codeword_entry	*locate_codeword_entry();

/*===========================================================================
 Copyright (c) 1998-2000, The Santa Cruz Operation 
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 *Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 *Neither name of The Santa Cruz Operation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission. 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 DAMAGE. 
 =========================================================================*/

/* $Id: global.h,v 1.1.1.2 2002/01/09 18:50:32 umeshv Exp $ */

/*	cscope - interactive C symbol cross-reference
 *
 *	global type, data, and function definitions
 */

#include "config.h"
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>	/* isalpha, isdigit, etc. */
#include <signal.h>	/* SIGINT and SIGQUIT */
#include <stdio.h>	/* standard I/O package */
#include <stdlib.h>     /* standard library functions */
#include <string.h>	/* string functions */
#include "constants.h"	/* misc. constants */
#include "invlib.h"	/* inverted index library */
#include "library.h"	/* library function return values */

#ifndef RETSIGTYPE
#if SVR2 || BSD && !sun
#define RETSIGTYPE int
#else
#define RETSIGTYPE void
#endif
#endif /* RETSIGTYPE */



typedef	enum	{		/* boolean data type */
	NO,
	YES
} BOOL;

typedef	enum	{		/* findinit return code */
	NOERROR,
	NOTSYMBOL,
	REGCMPERROR
} FINDINIT;

typedef	struct {		/* mouse action */
	int	button;
	int	percent;
	int	x1;
	int	y1;
	int	x2;
	int	y2;
} MOUSE;

struct cmd {			/* command history struct */
	struct	cmd *prev, *next;	/* list ptrs */
	int	field;			/* input field number */
	char	*text;			/* input field text */
};

/* digraph data for text compression */
extern	char	dichar1[];	/* 16 most frequent first chars */
extern	char	dichar2[];	/* 8 most frequent second chars 
				   using the above as first chars */
extern	char	dicode1[];	/* digraph first character code */
extern	char	dicode2[];	/* digraph second character code */

/* and some macros to help using dicodes: */
/* Check if a given pair of chars is compressable as a dicode: */
#define IS_A_DICODE(inchar1, inchar2)					   \
  (dicode1[(unsigned char)(inchar1)] && dicode2[(unsigned char)(inchar2)])
/* Combine the pair into a dicode */
#define DICODE_COMPRESS(inchar1, inchar2)		\
  ((0200 - 2) + dicode1[(unsigned char)(inchar1)]	\
   + dicode2[(unsigned char)(inchar2)])

/* main.c global data */
extern	char	*editor, *home, *shell, *lineflag;	/* environment variables */
extern 	BOOL	lineflagafterfile;
extern	char	*argv0;		/* command name */
extern	BOOL	compress;	/* compress the characters in the crossref */
extern	BOOL	dbtruncated;	/* database symbols truncated to 8 chars */
extern	int	dispcomponents;	/* file path components to display */
#if CCS
extern	BOOL	displayversion;	/* display the C Compilation System version */
#endif
extern	BOOL	editallprompt;	/* prompt between editing files */
extern	int	fileargc;	/* file argument count */
extern	char	**fileargv;	/* file argument values */
extern	int	fileversion;	/* cross-reference file version */
extern	BOOL	incurses;	/* in curses */
extern	BOOL	invertedindex;	/* the database has an inverted index */
extern	BOOL	isuptodate;	/* consider the crossref up-to-date */
extern	BOOL	kernelmode;	/* don't use DFLT_INCDIR - bad for kernels */
extern	BOOL	linemode;	/* use line oriented user interface */
extern	BOOL	recurse_dir;	/* recurse dirs when searching for src files */
extern	char	*namefile;	/* file of file names */
extern	char	*newreffile;	/* new cross-reference file name */
extern	FILE	*newrefs;	/* new cross-reference */
extern	BOOL	ogs;		/* display OGS book and subsystem names */
extern	FILE	*postings;	/* new inverted index postings */
extern	char	*prependpath;	/* prepend path to file names */
extern	FILE	*refsfound;	/* references found file */
extern	BOOL	select_large;	/* enable more than 9 select lines */
extern	int	symrefs;	/* cross-reference file */
extern	char	temp1[];	/* temporary file name */
extern	char	temp2[];	/* temporary file name */
extern	long	totalterms;	/* total inverted index terms */
extern	BOOL	trun_syms;	/* truncate symbols to 8 characters */
extern	char	tempstring[8192]; /* global dummy string buffer */

/* command.c global data */
extern	BOOL	caseless;	/* ignore letter case when searching */
extern	BOOL	*change;	/* change this line */
extern	BOOL	changing;	/* changing text */
extern	int	selecting;
extern	int	curdispline;
extern	char	newpat[];	/* new pattern */
extern	char	pattern[];	/* symbol or text pattern */

/* crossref.c global data */
extern	long	dboffset;	/* new database offset */
extern	BOOL	errorsfound;	/* prompt before clearing error messages */
extern	long	fileindex;	
extern	long	lineoffset;	/* source line database offset */
extern	long	npostings;	/* number of postings */
extern	int	symbols;	/* number of symbols */

/* dir.c global data */
extern	char	currentdir[];	/* current directory */
extern	char	**incdirs;	/* #include directories */
extern	char	**srcdirs;	/* source directories */
extern	char	**srcfiles;	/* source files */
extern	int	nincdirs;	/* number of #include directories */
extern	int	nsrcdirs;	/* number of source directories */
extern	int	nsrcfiles;	/* number of source files */
extern	int	msrcfiles;	/* maximum number of source files */

/* display.c global data */
extern 	int	booklen;	/* OGS book name display field length */
extern	int	*displine;	/* screen line of displayed reference */
extern	int	disprefs;	/* displayed references */
extern	int	fcnlen;		/* function name display field length */
extern	int	field;		/* input field */
extern	int	filelen;	/* file name display field length */
extern	unsigned fldcolumn;	/* input field column */
extern	int	mdisprefs;	/* maximum displayed references */
extern	int	nextline;	/* next line to be shown */
extern	FILE	*nonglobalrefs;	/* non-global references file */
extern	int	numlen;		/* line number display field length */
extern	int	topline;	/* top line of page */
extern	int	bottomline;	/* bottom line of page */
extern	long	searchcount;	/* count of files searched */
extern	int	subsystemlen;	/* OGS subsystem name display field length */
extern	int	totallines;	/* total reference lines */
extern	const char dispchars[];	/* display chars for jumping to lines */

/* find.c global data */
extern	char	block[];	/* cross-reference file block */
extern	char	blockmark;	/* mark character to be searched for */
extern	long	blocknumber;	/* block number */
extern	char	*blockp;	/* pointer to current character in block */
extern	int	blocklen;	/* length of disk block read */

/* lookup.c global data */
extern	struct	keystruct {
	char	*text;
	char	delim;
	struct	keystruct *next;
} keyword[];

/* mouse.c global data */
extern	BOOL	mouse;		/* mouse interface */

#if UNIXPC
extern	BOOL	unixpcmouse;		/* UNIX PC mouse interface */
#endif

/* cscope functions called from more than one function or between files */ 

char	*filepath(char *file);
char	*findcalledby(char *pattern);
char	*findcalling(char *pattern);
char	*findallfcns(char *dummy);
char	*finddef(char *pattern);
char	*findfile(char *dummy);
char	*findinclude(char *pattern);
char	*findsymbol(char *pattern);
char	*findregexp(char *egreppat);
char	*findstring(char *pattern);
char	*inviewpath(char *file);
char	*lookup(char *ident);
char	*mygetenv(char *variable, char *deflt);
char	*pathcomponents(char *path, int components);
char	*readblock(void);
char	*scanpast(char c);


void	addcmd(int f, char *s);
void	addsrcfile(char *name, char *path);
void	askforchar(void);
void	askforreturn(void);
void	atfield(void);
void	cannotwrite(char *file);
void	cannotopen(char *file);
void	clearmsg2(void);
void	crossref(char *srcfile);
void    dispinit(void);
void	display(void);
void	drawscrollbar(int top, int bot);
void	edit(char *file, char *linenum);
void	editall(void);
void	editref(int);
void	entercurses(void);
void	exitcurses(void);
void	findcleanup(void);
void    freesrclist(void);
void    freeinclist(void);
void    freecrossref(void);
void	freefilelist(void);
void	help(void);
void	incfile(char *file, char *type);
void    includedir(char *dirname);
void    initsymtab(void);
void	makefilelist(void);
void	mousecleanup(void);
void	mousemenu(void);
void	mouseinit(void);
void	mousereinit(void);
void	myexit(int sig);
void	myperror(char *text);
void	ogsnames(char *file, char **subsystem, char **book);
void	progress(char *what, long current, long max);
void	putfilename(char *srcfile);
void	postmsg(char *msg);
void	postmsg2(char *msg);
void	posterr(char *msg,...);
void	putposting(char *term, int type);
void	putstring(char *s);
void	rebuild(void);
void	resetcmd(void);
void	seekline(int line);
void	setfield(void);
void	shellpath(char *out, int limit, char *in);
void    sourcedir(char *dirlist);
void	myungetch(int c);
void	warning(char *text);
void	writestring(char *s);

BOOL	command(int commandc);
BOOL	infilelist(char *file);
BOOL	readrefs(char *filename);
BOOL	search(void);
BOOL	writerefsfound(void);

FILE	*myfopen(char *path, char *mode);
FINDINIT findinit(char *pattern);
MOUSE	*getmouseaction(char leading_char);
struct	cmd *currentcmd(void);
struct	cmd *prevcmd(void);
struct	cmd *nextcmd(void);

int	egrep(char *file, FILE *output, char *format);
int	getline(char s[], unsigned size, int firstchar, BOOL iscaseless);
int	mygetch(void);
int	myopen(char *path, int flag, int mode);
int	hash(char *ss);
int	execute(char *a, ...);
long	dbseek(long offset);



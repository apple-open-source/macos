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


/*	cscope - interactive C symbol cross-reference
 *
 *	main functions
 */

#include "build.h"

#include "global.h"		/* FIXME: get rid of this! */

#include "library.h"
#include "scanner.h"
#include "version.h"		/* for FILEVERSION */
#include "vp.h"

#if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
#include <ncurses.h>
#else
#include <curses.h>
#endif

/* Exported variables: */

BOOL	buildonly = NO;		/* only build the database */
BOOL	unconditional = NO;	/* unconditionally build database */
BOOL	fileschanged;		/* assume some files changed */

char	*invname = INVNAME;	/* inverted index to the database */
char	*invpost = INVPOST;	/* inverted index postings */
char	*reffile = REFFILE;	/* cross-reference file path name */
char	*newreffile;		/* new cross-reference file name */
FILE	*newrefs;		/* new cross-reference */
FILE	*postings;		/* new inverted index postings */
int	symrefs = -1;		/* cross-reference file */

INVCONTROL invcontrol;		/* inverted file control structure */

/* Local variables: */

static char *newinvname;	/* new inverted index file name */
static char *newinvpost;	/* new inverted index postings file name */
static long traileroffset;	/* file trailer offset */

/* Internal prototypes: */

static	void	cannotindex(void);
static	int	compare(const void *s1, const void *s2);
static	void	copydata(void);
static	void	copyinverted(void);
static	char	*getoldfile(void);
static	void	movefile(char *new, char *old);
static	void	putheader(char *dir);
static	void	putinclude(char *s);
static	void	putlist(char **names, int count);
static	BOOL	samelist(FILE *oldrefs, char **names, int count);

/* Error handling routine if inverted index creation fails */

static void
cannotindex(void)
{
	(void) fprintf(stderr, "cscope: cannot create inverted index; ignoring -q option\n");
	invertedindex = NO;
	errorsfound = YES;
	(void) fprintf(stderr, "cscope: removed files %s and %s\n", newinvname, newinvpost);
	(void) unlink(newinvname);
	(void) unlink(newinvpost);
}

/* see if the name list is the same in the cross-reference file */

static BOOL
samelist(FILE *oldrefs, char **names, int count)
{
	char	oldname[PATHLEN + 1];	/* name in old cross-reference */
	int	oldcount;
	int	i;

	/* see if the number of names is the same */
	if (fscanf(oldrefs, "%d", &oldcount) != 1 ||
	    oldcount != count) {
		return(NO);
	}
	/* see if the name list is the same */
	for (i = 0; i < count; ++i) {
		if (fscanf(oldrefs, "%s", oldname) != 1 ||
		    strnotequal(oldname, names[i])) {
			return(NO);
		}
	}
	return(YES);
}


/* create the file name(s) used for a new cross-referene */

void setup_build_filenames(char *reffile)
{
    char *path;			/* file pathname */
    char *s;			/* pointer to basename in path */

    path = mymalloc(strlen(reffile) + 10);
    (void) strcpy(path, reffile);
    s = mybasename(path);
    *s = '\0';
    (void) strcat(path, "n");
    ++s;
    (void) strcpy(s, mybasename(reffile));
    newreffile = stralloc(path);
    (void) strcpy(s, mybasename(invname));
    newinvname = stralloc(path);
    (void) strcpy(s, mybasename(invpost));
    newinvpost = stralloc(path);
    free(path);
}

/* open the database */

void
opendatabase(void)
{
	if ((symrefs = vpopen(reffile, O_BINARY | O_RDONLY)) == -1) {
		cannotopen(reffile);
		myexit(1);
	}
	blocknumber = -1;	/* force next seek to read the first block */
	
	/* open any inverted index */
	if (invertedindex == YES &&
	    invopen(&invcontrol, invname, invpost, INVAVAIL) == -1) {
		askforreturn();		/* so user sees message */
		invertedindex = NO;
	}
}

/* rebuild the database */

void
rebuild(void)
{
	(void) close(symrefs);
	if (invertedindex == YES) {
		invclose(&invcontrol);
		nsrcoffset = 0;
		npostings = 0;
	}
	build();
	opendatabase();

	/* revert to the initial display */
	if (refsfound != NULL) {
		(void) fclose(refsfound);
		refsfound = NULL;
	}
}

/* build the cross-reference */

void
build(void)
{
	int	i;
	FILE	*oldrefs;	/* old cross-reference file */
	time_t	reftime;	/* old crossref modification time */
	char	*file;			/* current file */
	char	*oldfile;		/* file in old cross-reference */
	char	newdir[PATHLEN + 1];	/* directory in new cross-reference */
	char	olddir[PATHLEN + 1];	/* directory in old cross-reference */
	char	oldname[PATHLEN + 1];	/* name in old cross-reference */
	int	oldnum;			/* number in old cross-ref */
	struct	stat statstruct;	/* file status */
	int	firstfile;		/* first source file in pass */
	int	lastfile;		/* last source file in pass */
	int	built = 0;		/* built crossref for these files */
	int	copied = 0;		/* copied crossref for these files */
	long	fileindex;		/* source file name index */
	BOOL	interactive = YES;	/* output progress messages */

	/* normalize the current directory relative to the home directory so
	   the cross-reference is not rebuilt when the user's login is moved */
	(void) strcpy(newdir, currentdir);
	if (strcmp(currentdir, home) == 0) {
		(void) strcpy(newdir, "$HOME");
	}
	else if (strncmp(currentdir, home, strlen(home)) == 0) {
		(void) sprintf(newdir, "$HOME%s", currentdir + strlen(home));
	}
	/* sort the source file names (needed for rebuilding) */
	qsort(srcfiles, (unsigned) nsrcfiles, sizeof(char *), compare);

	/* if there is an old cross-reference and its current directory matches */
	/* or this is an unconditional build */
	if ((oldrefs = vpfopen(reffile, "rb")) != NULL && unconditional == NO &&
	    fscanf(oldrefs, "cscope %d %s", &fileversion, olddir) == 2 &&
	    (strcmp(olddir, currentdir) == 0 || /* remain compatible */
	     strcmp(olddir, newdir) == 0)) {
		/* get the cross-reference file's modification time */
		(void) fstat(fileno(oldrefs), &statstruct);
		reftime = statstruct.st_mtime;
		if (fileversion >= 8) {
			BOOL	oldcompress = YES;
			BOOL	oldinvertedindex = NO;
			BOOL	oldtruncate = NO;
			int	c;
			/* see if there are options in the database */
			for (;;) {
				while((c = getc(oldrefs)) == ' ') {
					;
				}
				if (c != '-') {
					(void) ungetc(c, oldrefs);
					break;
				}
				switch (c = getc(oldrefs)) {
				case 'c':	/* ASCII characters only */
					oldcompress = NO;
					break;
				case 'q':	/* quick search */
					oldinvertedindex = YES;
					(void) fscanf(oldrefs, "%ld", &totalterms);
					break;
				case 'T':	/* truncate symbols to 8 characters */
					oldtruncate = YES;
					break;
				}
			}
			/* check the old and new option settings */
			if (oldcompress != compress || oldtruncate != trun_syms) {
				posterr("cscope: -c or -T option mismatch between command line and old symbol database\n");
				goto force;
			}
			if (oldinvertedindex != invertedindex) {
				(void) posterr("cscope: -q option mismatch between command line and old symbol database\n");
				if (invertedindex == NO) {
					posterr("cscope: removed files %s and %s\n",
					    invname, invpost);
					(void) unlink(invname);
					(void) unlink(invpost);
				}
				goto outofdate;
			}
			/* seek to the trailer */
			if (fscanf(oldrefs, "%ld", &traileroffset) != 1 ||
			    fseek(oldrefs, traileroffset, SEEK_SET) == -1) {
				posterr("cscope: incorrect symbol database file format\n");
				goto force;
			}
		}
		/* if assuming that some files have changed */
		if (fileschanged == YES) {
			goto outofdate;
		}
		/* see if the directory lists are the same */
		if (samelist(oldrefs, srcdirs, nsrcdirs) == NO ||
		    samelist(oldrefs, incdirs, nincdirs) == NO ||
		    fscanf(oldrefs, "%d", &oldnum) != 1 ||	/* get the old number of files */
		    (fileversion >= 9 && fscanf(oldrefs, "%*s") != 0)) {	/* skip the string space size */
			goto outofdate;
		}
		/* see if the list of source files is the same and
		   none have been changed up to the included files */
		for (i = 0; i < nsrcfiles; ++i) {
			if (fscanf(oldrefs, "%s", oldname) != 1 ||
			    strnotequal(oldname, srcfiles[i]) ||
			    lstat(srcfiles[i], &statstruct) != 0 ||
			    statstruct.st_mtime > reftime) {
				goto outofdate;
			}
		}
		/* the old cross-reference is up-to-date */
		/* so get the list of included files */
		while (i++ < oldnum && fscanf(oldrefs, "%s", oldname) == 1) {
			addsrcfile(oldname);
		}
		(void) fclose(oldrefs);
		return;
		
	outofdate:
		/* if the database format has changed, rebuild it all */
		if (fileversion != FILEVERSION) {
			(void) fprintf(stderr, "cscope: converting to new symbol database file format\n");
			goto force;
		}
		/* reopen the old cross-reference file for fast scanning */
		if ((symrefs = vpopen(reffile, O_BINARY | O_RDONLY)) == -1) {
			(void) fprintf(stderr, "cscope: cannot open file %s\n", reffile);
			myexit(1);
		}
		/* get the first file name in the old cross-reference */
		blocknumber = -1;
		(void) readblock();	/* read the first cross-ref block */
		(void) scanpast('\t');	/* skip the header */
		oldfile = getoldfile();
	}
	else {	/* force cross-referencing of all the source files */
	force:	reftime = 0;
		oldfile = NULL;
	}
	/* open the new cross-reference file */
	if ((newrefs = myfopen(newreffile, "wb")) == NULL) {
		(void) fprintf(stderr, "cscope: cannot open file %s\n", reffile);
		myexit(1);
	}
	if (invertedindex == YES && (postings = myfopen(temp1, "wb")) == NULL) {
		cannotwrite(temp1);
		cannotindex();
	}
	putheader(newdir);
	fileversion = FILEVERSION;
	if (buildonly == YES && !isatty(0)) {
		interactive = NO;
	}
	else {
		searchcount = 0;
	}
	/* output the leading tab expected by crossref() */
	dbputc('\t');

	/* make passes through the source file list until the last level of
	   included files is processed */
	firstfile = 0;
	lastfile = nsrcfiles;
	if (invertedindex == YES) {
		srcoffset = mymalloc((nsrcfiles + 1) * sizeof(long));
	}
	for (;;) {
		progress("Building symbol database", (long)built,
			 (long)lastfile);
		if (linemode == NO)
			refresh();

		/* get the next source file name */
		for (fileindex = firstfile; fileindex < lastfile; ++fileindex) {
			
			/* display the progress about every three seconds */
			if (interactive == YES && fileindex % 10 == 0) {
				progress("Building symbol database",
					 (long)fileindex, (long)lastfile);
			}
			/* if the old file has been deleted get the next one */
			file = srcfiles[fileindex];
			while (oldfile != NULL && strcmp(file, oldfile) > 0) {
				oldfile = getoldfile();
			}
			/* if there isn't an old database or this is a new file */
			if (oldfile == NULL || strcmp(file, oldfile) < 0) {
				crossref(file);
				++built;
			}
			/* if this file was modified */
			else if (lstat(file, &statstruct) == 0 &&
			    statstruct.st_mtime > reftime) {
				crossref(file);
				++built;
				
				/* skip its old crossref so modifying the last
				   source file does not cause all included files
				   to be built.  Unfortunately a new file that is
				   alphabetically last will cause all included
				   files to be build, but this is less likely */
				oldfile = getoldfile();
			}
			else {	/* copy its cross-reference */
				putfilename(file);
				if (invertedindex == YES) {
					copyinverted();
				}
				else {
					copydata();
				}
				++copied;
				oldfile = getoldfile();
			}
		}
		/* see if any included files were found */
		if (lastfile == nsrcfiles) {
			break;
		}
		firstfile = lastfile;
		lastfile = nsrcfiles;
		if (invertedindex == YES) {
			srcoffset = myrealloc(srcoffset,
			    (nsrcfiles + 1) * sizeof(long));
		}
		/* sort the included file names */
		qsort(&srcfiles[firstfile], (unsigned) (lastfile - 
			firstfile), sizeof(char *), compare);
	}
	/* add a null file name to the trailing tab */
	putfilename("");
	dbputc('\n');
	
	/* get the file trailer offset */
	traileroffset = dboffset;
	
	/* output the source and include directory and file lists */
	putlist(srcdirs, nsrcdirs);
	putlist(incdirs, nincdirs);
	putlist(srcfiles, nsrcfiles);
	if (fflush(newrefs) == EOF) {	/* rewind doesn't check for write failure */
		cannotwrite(newreffile);
		/* NOTREACHED */
	}
	/* create the inverted index if requested */
	if (invertedindex == YES) {
		char	sortcommand[PATHLEN + 1];

		if (fflush(postings) == EOF) {
			cannotwrite(temp1);
			/* NOTREACHED */
		}
		(void) fstat(fileno(postings), &statstruct);
		(void) fclose(postings);
		(void) sprintf(sortcommand, "env LC_ALL=C sort -T %s %s", tmpdir, temp1);
		if ((postings = mypopen(sortcommand, "r")) == NULL) {
			(void) fprintf(stderr, "cscope: cannot open pipe to sort command\n");
			cannotindex();
		}
		else {
			if ((totalterms = invmake(newinvname, newinvpost, postings)) > 0) {
				movefile(newinvname, invname);
				movefile(newinvpost, invpost);
			}
			else {
				cannotindex();
			}
			(void) mypclose(postings);
		}
		(void) unlink(temp1);
		(void) free(srcoffset);
	}
	/* rewrite the header with the trailer offset and final option list */
	rewind(newrefs);
	putheader(newdir);
	(void) fclose(newrefs);
	
	/* close the old database file */
	if (symrefs >= 0) {
		(void) close(symrefs);
	}
	if (oldrefs != NULL) {
		(void) fclose(oldrefs);
	}
	/* replace it with the new database file */
	movefile(newreffile, reffile);
}
	
/* string comparison function for qsort */

static int
compare(const void *arg_s1, const void *arg_s2)
{
	const char **s1 = (const char **) arg_s1;
	const char **s2 = (const char **) arg_s2;
			
	return(strcmp(*s1, *s2));
}

/* seek to the trailer, in a given file */
void seek_to_trailer(FILE *f) 
{
    if (fscanf(f, "%ld", &traileroffset) != 1) {
	posterr("cscope: cannot read trailer offset from file %s\n", reffile);
	myexit(1);
    }
    if (fseek(f, traileroffset, SEEK_SET) == -1) {
	posterr("cscope: cannot seek to trailer in file %s\n", reffile);
	myexit(1);
    }
}

/* get the next file name in the old cross-reference */

static char *
getoldfile(void)
{
	static	char	file[PATHLEN + 1];	/* file name in old crossref */

	if (blockp != NULL) {
		do {
			if (*blockp == NEWFILE) {
				skiprefchar();
				putstring(file);
				if (file[0] != '\0') {	/* if not end-of-crossref */
					return(file);
				}
				return(NULL);
			}
		} while (scanpast('\t') != NULL);
	}
	return(NULL);
}

/* Free all storage allocated for filenames: */
void free_newbuildfiles(void)
{
    free(newinvname);
    free(newinvpost);
    free(newreffile);
}	

/* output the cscope version, current directory, database format options, and
   the database trailer offset */

static void
putheader(char *dir)
{
	dboffset = fprintf(newrefs, "cscope %d %s", FILEVERSION, dir);
	if (compress == NO) {
		dboffset += fprintf(newrefs, " -c");
	}
	if (invertedindex == YES) {
		dboffset += fprintf(newrefs, " -q %.10ld", totalterms);
	}
	else {	/* leave space so if the header is overwritten without -q
		   because writing the inverted index failed, the header is the
		   same length */
		dboffset += fprintf(newrefs, "              ");
	}
	if (trun_syms == YES) {
		dboffset += fprintf(newrefs, " -T");
	}

	dboffset += fprintf(newrefs, " %.10ld\n", traileroffset);
#ifdef PRINTF_RETVAL_BROKEN
	dboffset = ftell(newrefs); 
#endif
}

/* put the name list into the cross-reference file */

static void
putlist(char **names, int count)
{
	int	i, size = 0;
	
	(void) fprintf(newrefs, "%d\n", count);
	if (names == srcfiles) {

		/* calculate the string space needed */
		for (i = 0; i < count; ++i) {
			size += strlen(names[i]) + 1;
		}
		(void) fprintf(newrefs, "%d\n", size);
	}
	for (i = 0; i < count; ++i) {
		if (fputs(names[i], newrefs) == EOF ||
		    putc('\n', newrefs) == EOF) {
			cannotwrite(newreffile);
			/* NOTREACHED */
		}
	}
}

/* copy this file's symbol data */

static void
copydata(void)
{
	char	symbol[PATLEN + 1];
	char	*cp;

	setmark('\t');
	cp = blockp;
	for (;;) {
		/* copy up to the next \t */
		do {	/* innermost loop optimized to only one test */
			while (*cp != '\t') {
				dbputc(*cp++);
			}
		} while (*++cp == '\0' && (cp = readblock()) != NULL);
		dbputc('\t');	/* copy the tab */
		
		/* get the next character */
		if (*(cp + 1) == '\0') {
			cp = readblock();
		}
		/* exit if at the end of this file's data */
		if (cp == NULL || *cp == NEWFILE) {
			break;
		}
		/* look for an #included file */
		if (*cp == INCLUDE) {
			blockp = cp;
			putinclude(symbol);
			writestring(symbol);
			setmark('\t');
			cp = blockp;
		}
	}
	blockp = cp;
}

/* copy this file's symbol data and output the inverted index postings */

static void
copyinverted(void)
{
	char	*cp;
	char	c;
	int	type;	/* reference type (mark character) */
	char	symbol[PATLEN + 1];

	/* note: this code was expanded in-line for speed */
	/* while (scanpast('\n') != NULL) { */
	/* other macros were replaced by code using cp instead of blockp */
	cp = blockp;
	for (;;) {
		setmark('\n');
		do {	/* innermost loop optimized to only one test */
			while (*cp != '\n') {
				dbputc(*cp++);
			}
		} while (*++cp == '\0' && (cp = readblock()) != NULL);
		dbputc('\n');	/* copy the newline */
		
		/* get the next character */
		if (*(cp + 1) == '\0') {
			cp = readblock();
		}
		/* exit if at the end of this file's data */
		if (cp == NULL) {
			break;
		}
		switch (*cp) {
		case '\n':
			lineoffset = dboffset + 1;
			continue;
		case '\t':
			dbputc('\t');
			blockp = cp;
			type = getrefchar();
			switch (type) {
			case NEWFILE:		/* file name */
				return;
			case INCLUDE:		/* #included file */
				putinclude(symbol);
				goto output;
			}
			dbputc(type);
			skiprefchar();
			putstring(symbol);
			goto output;
		}
		c = *cp;
		if (c & 0200) {	/* digraph char? */
			c = dichar1[(c & 0177) / 8];
		}
		/* if this is a symbol */
		if (isalpha((unsigned char)c) || c == '_') {
			blockp = cp;
			putstring(symbol);
			type = ' ';
		output:
			putposting(symbol, type);
			writestring(symbol);
			if (blockp == NULL) {
				return;
			}
			cp = blockp;
		}
	}
	blockp = cp;
}

/* replace the old file with the new file */

static void
movefile(char *new, char *old)
{
	(void) unlink(old);
	if (rename(new, old) == -1) {
		(void) myperror("cscope");
		posterr("cscope: cannot rename file %s to file %s\n",
			new, old);
		myexit(1);
	}
}

/* process the #included file in the old database */

static void
putinclude(char *s)
{
	dbputc(INCLUDE);
	skiprefchar();
	putstring(s);
	incfile(s + 1, s);
}


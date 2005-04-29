/* $Xorg: xkbevd.h,v 1.3 2000/08/17 19:54:49 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
/* $XFree86: xc/programs/xkbevd/xkbevd.h,v 3.5 2003/11/17 22:20:53 dawes Exp $ */

#ifndef XKBEVD_H
#define	XKBEVD_H 1

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBbells.h>

#include "utils.h"

extern	FILE *		yyin;
extern	char *	 	scanFile;
extern	int	 	lineNum;
extern	int	 	scanInt;
extern	char *		scanIntStr;
extern	int	 	scanIntClass;
extern	char *		scanStr;
extern	int	 	scanStrLine;

extern	Display *	dpy;
extern	int		xkbOpcode;
extern	int		xkbEventCode;
extern	Bool		detectableRepeat;

extern	XkbDescPtr	xkb;

#define	UnknownAction	0
#define	NoAction	1
#define	EchoAction	2
#define	PrintEvAction	3
#define	ShellAction	4
#define	SoundAction	5

typedef struct _ActDef {
	int		type;
	char *		text;
	unsigned	priv;
} ActDefRec,*ActDefPtr;

#define	EventDef	0
#define	VariableDef	1

typedef struct _CfgEntry {
	unsigned char	entry_type;
	unsigned char	event_type;
	union {
	    char *	str;
	    Atom	atom;
	    unsigned	priv;
	} 			name;
	ActDefRec		action;
	struct _CfgEntry *	next;
} CfgEntryRec,*CfgEntryPtr;

extern void InterpretConfigs(
	CfgEntryPtr	/* cfgs */
);

extern char *SubstituteEventArgs(
    char *		/* cmd */,
    XkbEvent *		/* ev */
);

extern void PrintXkbEvent(
	FILE *		/* file */,
	XkbEvent *	/* ev */
);

extern int setScanState ( char * file, int line );
extern int CFGParseFile ( FILE *file );

extern int yylex(void);
extern int yyparse(void);
extern int yyerror(char *s);
extern int yywrap(void);
#endif /* XKBEVD_H */

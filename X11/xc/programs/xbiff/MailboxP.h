/* $XConsortium: MailboxP.h,v 1.22 94/04/17 20:43:27 rws Exp $ */
/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xbiff/MailboxP.h,v 1.2 2001/04/19 19:54:51 dawes Exp $ */

#ifndef _XawMailboxP_h
#define _XawMailboxP_h

#include "Mailbox.h"
#include <X11/Xaw/SimpleP.h>

#if defined(SYSV)
#define MAILBOX_DIRECTORY "/usr/mail"
#elif defined(SVR4)
#define MAILBOX_DIRECTORY "/var/mail"
#elif defined(CSRG_BASED)
#include <paths.h>
#ifdef _PATH_MAILDIR
#define MAILBOX_DIRECTORY _PATH_MAILDIR
#endif
#elif defined(__linux__)
#define MAILBOX_DIRECTORY "/var/spool/mail"
#endif

#ifndef MAILBOX_DIRECTORY
#define MAILBOX_DIRECTORY "/usr/spool/mail"
#endif

typedef struct {			/* new fields for mailbox widget */
    /* resources */
    int update;				/* seconds between updates */
    Pixel foreground_pixel;		/* color index of normal state fg */
    String filename;			/* filename to watch */
    String check_command;		/* command to exec for mail check */
    Boolean flipit;			/* do flip of full pixmap */
    int volume;				/* bell volume */
    Boolean once_only;			/* ring bell only once on new mail */
    /* local state */
    GC gc;				/* normal GC to use */
    long last_size;			/* size in bytes of mailboxname */
    XtIntervalId interval_id;		/* time between checks */
    Boolean flag_up;			/* is the flag up? */
    struct _mbimage {
	Pixmap bitmap, mask;		/* depth 1, describing shape */
	Pixmap pixmap;			/* full depth pixmap */
	int width, height;		/* geometry of pixmaps */
    } full, empty;
    Boolean shapeit;			/* do shape extension */
    struct {
	Pixmap mask;
	int x, y;
    } shape_cache;			/* last set of info */
} MailboxPart;

typedef struct _MailboxRec {		/* full instance record */
    CorePart core;
    SimplePart simple;
    MailboxPart mailbox;
} MailboxRec;


typedef struct {			/* new fields for mailbox class */
    int dummy;				/* stupid C compiler */
} MailboxClassPart;

typedef struct _MailboxClassRec {	/* full class record declaration */
    CoreClassPart core_class;
    SimpleClassPart simple_class;
    MailboxClassPart mailbox_class;
} MailboxClassRec;

extern MailboxClassRec mailboxClassRec;	 /* class pointer */

#endif /* _XawMailboxP_h */

/*
 * $XConsortium: util.c /main/42 1996/01/14 16:51:55 kaleb $
 * $XFree86: xc/programs/xmh/util.c,v 3.6 2001/10/31 22:50:31 tsi Exp $
 *
 *
 *			  COPYRIGHT 1987
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */

/* util.c -- little miscellaneous utilities. */

#include "xmh.h"
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>

#ifndef abs
#define abs(x)		((x) < 0 ? (-(x)) : (x))
#endif

static char *SysErrorMsg (int n)
{
    char *s = strerror(n);

    return (s ? s : "no such error");
}

/* Something went wrong; panic and quit. */

void Punt(char *str)
{
    (void) fprintf( stderr, "%s: %s\nerrno = %d; %s\007\n",
		    progName, str, errno, SysErrorMsg(errno) );
    if (app_resources.debug) {
	(void)fprintf(stderr, "forcing core dump.\n");
	(void) fflush(stderr);
	abort();
    }
    else {
	(void)fprintf(stderr, "exiting.\n");
	(void)fflush(stderr);
	_exit(-1);
    }
}


int myopen(char *path, int flags, int mode)
{
    int fid;
    fid = open(path, flags, mode);
    if (fid >= 0) DEBUG2("# %d : %s\n", fid, path)
    return fid;
}


FILE *myfopen(char *path, char *mode)
{
    FILE *result;
    result = fopen(path, mode);
    if (result)  DEBUG2("# %d : %s\n", fileno(result), path)
    return result;
}



void myclose(int fid)
{
    if (close(fid) < 0) Punt("Error in myclose!");
    DEBUG1( "# %d : <Closed>\n", fid)
}


void myfclose(FILE *file)
{
    int fid = fileno(file);
    if (fclose(file) < 0) Punt("Error in myfclose!");
    DEBUG1("# %d : <Closed>\n", fid)
}



/* Return a unique file name. */

char *MakeNewTempFileName(void)
{
    static char name[60];
    static int  uniqueid = 0;
    do {
	(void) sprintf(name, "%s/xmh_%ld_%d", app_resources.temp_dir,
		       (long)getpid(), uniqueid++);
    } while (FileExists(name));
    return name;
}


/* Make an array of string pointers big enough to hold n+1 entries. */

char **MakeArgv(int n)
{
    char **result;
    result = ((char **) XtMalloc((unsigned) (n+1) * sizeof(char *)));
    result[n] = 0;
    return result;
}


char **ResizeArgv(char **argv, int n)
{
    argv = ((char **) XtRealloc((char *) argv, (unsigned) (n+1) * sizeof(char *)));
    argv[n] = 0;
    return argv;
}

/* Open a file, and punt if we can't. */

FILEPTR FOpenAndCheck(char *name, char *mode)
{
    FILEPTR result;
    result = myfopen(name, mode);
    if (result == NULL) {
	char str[500];
	perror(progName);
	(void)sprintf(str, "Error in FOpenAndCheck(%s, %s)", name, mode);
	Punt(str);
    }
    return result;
}


/* Read one line from a file. */

static char *DoReadLine(FILEPTR fid, char lastchar)
{
    static char *buf;
    static int  maxlength = 0;
    char   *ptr, c;
    int     length = 0;
    ptr = buf;
    c = ' ';
    while (c != '\n' && !feof(fid)) {
	c = getc(fid);
	if (length++ > maxlength - 5) {
	    if (maxlength)
		buf = XtRealloc(buf, (unsigned) (maxlength *= 2));
	    else
		buf = XtMalloc((unsigned) (maxlength = 512));
	    ptr = buf + length - 1;
	}
	*ptr++ = c;
    }
    if (!feof(fid) || length > 1) {
	*ptr = 0;
	*--ptr = lastchar;
	return buf;
    }
    return NULL;
}


char *ReadLine(FILEPTR fid)
{
    return DoReadLine(fid, 0);
}


/* Read a line, and keep the CR at the end. */

char *ReadLineWithCR(FILEPTR fid)
{
    return DoReadLine(fid, '\n');
}



/* Delete a file, and Punt if it fails. */

void DeleteFileAndCheck(char *name)
{
    if (strcmp(name, "/dev/null") != 0 && unlink(name) == -1) {
	char str[500];
	perror(progName);
	(void)sprintf(str, "DeleteFileAndCheck(%s) failed!", name);
	Punt(str);
    }
}

void CopyFileAndCheck(char *from, char *to)
{
    int fromfid, tofid, n;
    char buf[512];
    fromfid = myopen(from, O_RDONLY, 0666);
    tofid = myopen(to, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fromfid < 0 || tofid < 0) {
	perror(progName);
	(void)sprintf(buf, "CopyFileAndCheck(%s->%s) failed!", from, to);
	Punt(buf);
    }
    do {
	n = read(fromfid, buf, 512);
	if (n) (void) write(tofid, buf, n);
    } while (n);
    myclose(fromfid);
    myclose(tofid);
}


void RenameAndCheck(char *from, char *to)
{
    if (rename(from, to) == -1) {
	if (errno != EXDEV) {
	    char str[500];
	    perror(progName);
	    (void)sprintf(str, "RenameAndCheck(%s->%s) failed!", from, to);
	    Punt(str);
	}
	CopyFileAndCheck(from, to);
	DeleteFileAndCheck(from);
    }
}


char *CreateGeometry(int gbits, int x, int y, int width, int height)
{
    char   *result, str1[10], str2[10], str3[10], str4[10];
    if (gbits & WidthValue)
	(void) sprintf(str1, "=%d", width);
    else
	(void) strcpy(str1, "=");
    if (gbits & HeightValue)
	(void) sprintf(str2, "x%d", height);
    else
	(void) strcpy(str2, "x");
    if (gbits & XValue)
	(void) sprintf(str3, "%c%d", (gbits & XNegative) ? '-' : '+', abs(x));
    else
	(void) strcpy(str3, "");
    if (gbits & YValue)
	(void) sprintf(str4, "%c%d", (gbits & YNegative) ? '-' : '+', abs(y));
    else
	(void) strcpy(str4, "");
    result = XtMalloc((unsigned) 22);
    (void) sprintf(result, "%s%s%s%s", str1, str2, str3, str4);
    return result;
}

int FileExists(char *file)
{
    return (access(file, F_OK) == 0);
}

long LastModifyDate(char *file)
{
    struct stat buf;
    if (stat(file, &buf)) return -1;
    return buf.st_mtime;
}

int GetFileLength(char *file)
{
    struct stat buf;
    if (stat(file, &buf)) return -1;
    return buf.st_size;
}

Boolean IsSubfolder(char *foldername)
{
    return (strchr(foldername, '/')) ? True : False;
}

void SetCurrentFolderName(Scrn scrn, char *foldername)
{
    scrn->curfolder = foldername;
    ChangeLabel((Widget) scrn->folderlabel, foldername);
}


void ChangeLabel(Widget widget, char *str)
{
    static Arg arglist[] = {{XtNlabel, (XtArgVal)NULL}};
    arglist[0].value = (XtArgVal) str;
    XtSetValues(widget, arglist, XtNumber(arglist));
}


Widget CreateTextSW(
    Scrn scrn,
    char *name,
    ArgList args,
    Cardinal num_args)
{
    /* most text widget options are set in the application defaults file */

    return XtCreateManagedWidget( name, asciiTextWidgetClass, scrn->widget,
				  args, num_args);
}


Widget CreateTitleBar(Scrn scrn, char *name)
{
    Widget result;
    int height;
    static Arg arglist[] = {
	{XtNlabel, (XtArgVal)NULL},
    };
    arglist[0].value = (XtArgVal) app_resources.banner; /* xmh version */
    result = XtCreateManagedWidget( name, labelWidgetClass, scrn->widget,
				    arglist, XtNumber(arglist) );
    height = GetHeight(result);
    XawPanedSetMinMax(result, height, height);
    return result;
}

void Feep(int type, int volume, Window win)
{
#ifdef XKB
    XkbStdBell(theDisplay, win, volume, type);
#else
    XBell(theDisplay, volume);
#endif
}


MsgList CurMsgListOrCurMsg(Toc toc)
{
    MsgList result;
    Msg curmsg;
    result = TocCurMsgList(toc);
    if (result->nummsgs == 0 && (curmsg = TocGetCurMsg(toc))) {
	FreeMsgList(result);
	result = MakeSingleMsgList(curmsg);
    }
    return result;
}


int GetHeight(Widget w)
{
    Dimension height;
    Arg args[1];

    XtSetArg(args[0], XtNheight, &height);
    XtGetValues( w, args, (Cardinal)1 );
    return (int)height;
}


int GetWidth(Widget w)
{
    Dimension width;
    Arg args[1];

    XtSetArg(args[0], XtNwidth, &width);
    XtGetValues( w, args, (Cardinal)1 );
    return (int)width;
}


Toc SelectedToc(Scrn scrn)
{
    Toc	toc;

    /* tocs of subfolders are created upon the first reference */

    if ((toc = TocGetNamed(scrn->curfolder)) == NULL) 
        toc = TocCreate(scrn->curfolder);
    return toc;
}


Toc CurrentToc(Scrn scrn)
{
    /* return the toc currently being viewed */

    return scrn->toc;
}


int strncmpIgnoringCase(char *str1, char *str2, int length)
{
    int i, diff;
    for (i=0 ; i<length ; i++, str1++, str2++) {
        diff = ((*str1 >= 'A' && *str1 <= 'Z') ? (*str1 + 'a' - 'A') : *str1) -
	       ((*str2 >= 'A' && *str2 <= 'Z') ? (*str2 + 'a' - 'A') : *str2);
	if (diff) return diff;
    }
    return 0;
}


void StoreWindowName(Scrn scrn, char *str)
{
    static Arg arglist[] = {
	{XtNiconName,	(XtArgVal) NULL},
	{XtNtitle,	(XtArgVal) NULL},
    };
    arglist[0].value = arglist[1].value = (XtArgVal) str;
    XtSetValues(scrn->parent, arglist, XtNumber(arglist));
}


/* Create an input-only window with a busy-wait cursor. */

void InitBusyCursor(Scrn scrn)
{
    unsigned long		valuemask;
    XSetWindowAttributes	attributes;

    /* the second condition is for the pick scrn */
    if (! app_resources.block_events_on_busy || scrn->wait_window) 
	return;	

    /* Ignore device events while the busy cursor is displayed. */

    valuemask = CWDontPropagate | CWCursor;
    attributes.do_not_propagate_mask =	(KeyPressMask | KeyReleaseMask |
					 ButtonPressMask | ButtonReleaseMask |
					 PointerMotionMask);
    attributes.cursor = app_resources.busy_cursor;

    /* The window will be as big as the display screen, and clipped by
     * it's own parent window, so we never have to worry about resizing.
     */

    scrn->wait_window =
	XCreateWindow(XtDisplay(scrn->parent), XtWindow(scrn->parent), 0, 0, 
		      rootwidth, rootheight, (unsigned int) 0, CopyFromParent,
		      InputOnly, CopyFromParent, valuemask, &attributes);
}

void ShowBusyCursor(void)
{
    register int	i;

    for (i=0; i < numScrns; i++)
	if (scrnList[i]->mapped)
	    XMapWindow(theDisplay, scrnList[i]->wait_window);
}

void UnshowBusyCursor(void)
{
    register int	i;
    
    for (i=0; i < numScrns; i++)
	if (scrnList[i]->mapped)
	    XUnmapWindow(theDisplay, scrnList[i]->wait_window);
}


void SetCursorColor(
    Widget		widget,
    Cursor		cursor,
    unsigned long	foreground)
{
    XColor	colors[2];
    Arg		args[2];
    Colormap	cmap;

    colors[0].pixel = foreground;
    XtSetArg(args[0], XtNbackground, &(colors[1].pixel));
    XtSetArg(args[1], XtNcolormap, &cmap);
    XtGetValues(widget, args, (Cardinal) 2);
    XQueryColors(XtDisplay(widget), cmap, colors, 2);
    XRecolorCursor(XtDisplay(widget), cursor, &colors[0], &colors[1]);
}

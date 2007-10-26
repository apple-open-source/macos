/* $XConsortium: misc.c,v 1.31 94/12/16 21:36:53 gildea Exp $ */
/* $XdotOrg: app/xman/misc.c,v 1.8 2005/11/08 06:33:33 jkj Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

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
/* $XFree86: xc/programs/xman/misc.c,v 1.10 2003/08/02 17:35:48 herrb Exp $ */

/*
 * xman - X window system manual page display program.
 * Author:    Chris D. Peterson, MIT Project Athena
 * Created:   October 27, 1987
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "globals.h"
#include "vendor.h"
#include <X11/Xos.h> 		/* sys/types.h and unistd.h included in here */
#include <sys/stat.h>
#include <errno.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Shell.h>

static FILE * Uncompress(ManpageGlobals * man_globals, char * filename);
#ifndef HAS_MKSTEMP
static Boolean UncompressNamed(ManpageGlobals * man_globals, char * filename,
			       char * output);
static Boolean UncompressUnformatted(ManpageGlobals * man_globals,
				     char * entry, char * filename);
#else
static Boolean UncompressNamed(ManpageGlobals * man_globals, char * filename,
			       char * output, FILE ** output_fd);
static Boolean UncompressUnformatted(ManpageGlobals * man_globals,
				     char * entry, char * filename,
				     FILE **file);
#endif
#ifdef HANDLE_ROFFSEQ
static Boolean ConstructCommand(char * cmdbuf, char * path, char * filename, char * tempfile);
#endif

#if defined(ISC) || defined(__SCO__) || defined(__UNIXWARE__)
static char *uncompress_format = NULL;
static char *uncompress_formats[] =
      {  UNCOMPRESS_FORMAT_1,
         UNCOMPRESS_FORMAT_2,
         UNCOMPRESS_FORMAT_3
      };
#endif

/*	Function Name: PopupWarning
 *	Description: This function pops upa warning message.
 *	Arguments: string - the specific warning string.
 *	Returns: none
 */

extern Widget top;
static Widget warnShell, warnDialog;

static void
PopdownWarning(Widget w, XtPointer client, XtPointer call)
{
  XtPopdown((Widget)client);
}

void
PopupWarning(ManpageGlobals * man_globals, char * string)
{
  int n;
  Arg wargs[3];
  Dimension topX, topY;
  char buffer[BUFSIZ];
  Boolean hasPosition;

  snprintf( buffer, sizeof(buffer), "Xman Warning: %s", string);
  hasPosition = FALSE;
  if (top)
  {
    n=0;
    XtSetArg(wargs[n], XtNx, &topX); n++;
    XtSetArg(wargs[n], XtNy, &topY); n++;
    XtGetValues(top, wargs, n);
    hasPosition = TRUE;
  }

  if (man_globals != NULL)
    ChangeLabel(man_globals->label, buffer);
  if (man_globals->label == NULL) {
    n=0;
    if (hasPosition)
    {
      XtSetArg(wargs[n], XtNx, topX); n++;
      XtSetArg(wargs[n], XtNy, topY); n++;
    }
    XtSetArg(wargs[n], XtNtransientFor, top); n++;
    warnShell = XtCreatePopupShell("warnShell", transientShellWidgetClass,
				   initial_widget, wargs, n);
    XtSetArg(wargs[0], XtNlabel, buffer);
    warnDialog = XtCreateManagedWidget("warnDialog", dialogWidgetClass,
				       warnShell, wargs, 1);
    XawDialogAddButton(warnDialog, "dismiss", PopdownWarning,
		       (XtPointer)warnShell);
    XtRealizeWidget(warnShell);
    Popup(warnShell, XtGrabNone);
  }
}

/*	Function Name: PrintError
 *	Description: This Function prints an error message and exits.
 *	Arguments: string - the specific message.
 *	Returns: none. - exits tho.
 */

void
PrintError(char * string)
{
  fprintf(stderr,"Xman Error: %s\n",string);
  exit(EXIT_FAILURE);
}

/*	Function Name: OpenFile
 *	Description: Assignes a file to the manpage.
 *	Arguments: man_globals - global structure.
 *                 file        - the file pointer.
 *	Returns: none
 */

void
OpenFile(ManpageGlobals * man_globals, FILE * file)
{
  Arg arglist[1];
  Cardinal num_args = 0;
  
  if (man_globals->curr_file) {
#if 0 /* Ownership rules need to be fixed first */
    fclose(man_globals->curr_file);
#endif
  }
  man_globals->curr_file = file;

  XtSetArg(arglist[num_args], XtNfile, man_globals->curr_file); num_args++;
  XtSetValues(man_globals->manpagewidgets.manpage, arglist, num_args);
}


/*	Function Name: FindManualFile
 *	Description: Opens the manual page file given the entry information.
 *	Arguments: man_globals - the globals info for this manpage.
 *                 section_num - section number of the man page.
 *                 entry_num   - entry number of the man page.
 *	Returns: fp - the file pointer
 *
 * NOTES:
 *
 * If there is a uncompressed section it will look there for uncompresed
 * manual pages first and then for individually compressed file in the
 * uncompressed section.
 *
 * If there is a compressed directory then it will also look there for
 * the manual pages.
 *
 * If both of these fail then it will attempt to format the manual page.
 */

FILE *
FindManualFile(ManpageGlobals * man_globals, int section_num, int entry_num)
{
  FILE * file;
  char path[BUFSIZ], page[BUFSIZ], section[BUFSIZ], *temp;
  char filename[BUFSIZ];
  char * entry = manual[section_num].entries[entry_num];
  int len_cat = strlen(CAT);
#if defined(ISC) || defined(__SCO__) || defined(__UNIXWARE__)
  int i;
#endif

  temp = CreateManpageName(entry, 0, 0);
  snprintf(man_globals->manpage_title, sizeof(man_globals->manpage_title),
    "The current manual page is: %s.", temp);
  XtFree(temp);

  ParseEntry(entry, path, section, page);

/*
 * Look for uncompressed files first.
 */
#if defined(__OpenBSD__) || defined(__NetBSD__)
  /* look in machine subdir first */
  snprintf(filename, sizeof(filename), "%s/%s%s/%s/%s", path, CAT,
	  section + len_cat, MACHINE, page);
  if ( (file = fopen(filename,"r")) != NULL)
    return(file);
#endif

  snprintf(filename, sizeof(filename), "%s/%s%s/%s", 
    path, CAT, section + len_cat, page);
  if ( (file = fopen(filename,"r")) != NULL)
    return(file);

/*
 * Then for compressed files in an uncompressed directory.
 */

#if !defined(ISC) && !defined(__UNIXWARE__)
#if defined(__OpenBSD__) || defined(__NetBSD__)
  /* look in machine subdir first */
  snprintf(filename, sizeof(filename), "%s/%s%s/%s/%s.%s", path, CAT,
	  section + len_cat, MACHINE, page, COMPRESSION_EXTENSION);
  if ( (file = Uncompress(man_globals, filename)) != NULL)
    return(file);
#endif
  snprintf(filename, sizeof(filename), "%s/%s%s/%s.%s", path, CAT,
	  section + len_cat, page, COMPRESSION_EXTENSION);
  if ( (file = Uncompress(man_globals, filename)) != NULL)
    return(file);
#ifdef GZIP_EXTENSION
  else {
#if defined(__OpenBSD__) || defined(__NetBSD__)
      /* look in machine subdir first */
      snprintf(filename, sizeof(filename), "%s/%s%s/%s/%s.%s", path, CAT,
	      section + len_cat, MACHINE, page, GZIP_EXTENSION);
      if ( (file = Uncompress(man_globals, filename)) != NULL)
	  return(file);
#endif
    snprintf(filename, sizeof(filename), "%s/%s%s/%s.%s", path, CAT,
	    section + len_cat, page, GZIP_EXTENSION);
    if ( (file = Uncompress(man_globals, filename)) != NULL)
      return(file);
  }
#endif
#else
  for(i = 0; i < strlen(COMPRESSION_EXTENSIONS); i++) {
      snprintf(filename, sizeof(filename), "%s/%s%s/%s.%c", path, CAT,
            section + len_cat, page, COMPRESSION_EXTENSIONS[i]);
      uncompress_format = uncompress_formats[i];
#ifdef DEBUG
      printf("Trying .%c ...\n", COMPRESSION_EXTENSIONS[i]);
#endif
      if ( (file = Uncompress(man_globals, filename)) != NULL)
	return(file);
  }
#endif

/*
 * And lastly files in a compressed directory.
 *
 * The directory is not actually compressed it is just named man#.Z
 * and all files in it are compressed without the .Z extension.
 * HP does it this way (really :-).
 */

  snprintf(filename, sizeof(filename), "%s/%s%s.%s/%s", path, CAT, 
	   section + len_cat, COMPRESSION_EXTENSION, page);
  if ( (file = Uncompress(man_globals, filename)) != NULL)
    return(file);
/*
 * We did not find any preformatted manual pages, try to format it.
 */

  return(Format(man_globals, entry));
}

/*	Function Namecompress
 *	Description: This function will attempt to find a compressed man
 *                   page and uncompress it.
 *	Arguments: man_globals - the psuedo global info.
 *                 filename - name of file to uncompress.
 *	Returns:; a pointer to the file or NULL.
 */

static FILE *
Uncompress(ManpageGlobals * man_globals, char * filename)
{
  char tmp_file[BUFSIZ], error_buf[BUFSIZ];
  FILE * file;

#ifndef HAS_MKSTEMP
  if ( !UncompressNamed(man_globals, filename, tmp_file) )
    return(NULL);

  else if ((file = fopen(tmp_file, "r")) == NULL) {
      sprintf(error_buf, "Something went wrong in retrieving the %s",
	      "uncompressed manual page try cleaning up /tmp.");
      PopupWarning(man_globals, error_buf);
  }
#else
  if (!UncompressNamed(man_globals, filename, tmp_file, &file)) {
      sprintf(error_buf, "Something went wrong in retrieving the %s",
	      "uncompressed manual page try cleaning up /tmp.");
      PopupWarning(man_globals, error_buf);
      return(NULL);
  }
#endif

  unlink(tmp_file);		/* remove name in tree, it will remain
				   until we close the fd, however. */
  return(file);
}

/*	Function Name: UncompressNamed
 *	Description: This function will attempt to find a compressed man
 *                   page and uncompress it.
 *	Arguments: man_globals - the psuedo global info.
 *                 filename - name of file to uncompress.
 * RETURNED        output - the file name output (must be an allocated string).
 *	Returns:; TRUE if the file was found.
 */

#ifndef HAS_MKSTEMP
static Boolean
UncompressNamed(ManpageGlobals * man_globals, char * filename, char * output)
#else
static Boolean
UncompressNamed(ManpageGlobals * man_globals, char * filename, char * output,
		FILE ** output_fd)
#endif
{
  char tmp[BUFSIZ], cmdbuf[BUFSIZ], error_buf[BUFSIZ];
  struct stat junk;
#ifdef HAS_MKSTEMP
  int fd;
#endif

  if (stat(filename, &junk) != 0) { /* Check for existance of the file. */
    if (errno != ENOENT) {
      snprintf(error_buf, sizeof(error_buf),
	       "Error while stating file %s, errno = %d", filename, errno);
      PopupWarning(man_globals, error_buf);
    }
    return(FALSE);
  }

/*
 * Using stdin is necessary to fool zcat since we cannot guarentee
 * the .Z extension.
 */

  strcpy(tmp, MANTEMP);		/* get a temp file. */
#ifndef HAS_MKSTEMP
  (void) mktemp(tmp);
#else
  fd = mkstemp(tmp);
  if (fd < 0) {
      PopupWarning(man_globals, "Error creating a temp file");
      return FALSE;
  }
  *output_fd = fdopen(fd, "r");
#endif
  strcpy(output, tmp);

#ifdef GZIP_EXTENSION
  if (streq(filename + strlen(filename) - strlen(GZIP_EXTENSION),
	    GZIP_EXTENSION))
    snprintf(cmdbuf, sizeof(cmdbuf), GUNZIP_FORMAT, filename, output);
  else
#endif
  snprintf(cmdbuf, sizeof(cmdbuf), UNCOMPRESS_FORMAT, filename, output);
  if(system(cmdbuf) == 0) 	/* execute search. */
    return(TRUE);

  snprintf(error_buf, sizeof(error_buf),
	   "Error while uncompressing, command was: %s", cmdbuf);
  PopupWarning(man_globals, error_buf);
  return(FALSE);
}

#if defined(SMAN) && defined(SFORMAT)
/*	Function Name: SgmlToRoffNamed
 *	Description: This function will attempt to find an SGML man
 *                   page and convert it to roff format.
 *	Arguments: man_globals - the psuedo global info.
 *                 filename - name of file to uncompress.
 * RETURNED        output - the file name output (must be an allocated string).
 *	Returns:; TRUE if the file was found.
 */

#ifndef HAS_MKSTEMP
static Boolean
SgmlToRoffNamed(ManpageGlobals * man_globals, char * filename, char * output)
#else
static Boolean
SgmlToRoffNamed(ManpageGlobals * man_globals, char * filename, char * output,
		FILE ** output_fd)
#endif
{
  char tmp[BUFSIZ], cmdbuf[BUFSIZ], error_buf[BUFSIZ];
  struct stat junk;
#ifdef HAS_MKSTEMP
  int fd;
#endif

  if (stat(filename, &junk) != 0) { /* Check for existance of the file. */
    if (errno != ENOENT) {
      snprintf(error_buf, sizeof(error_buf),
	       "Error while stating file %s, errno = %d", filename, errno);
      PopupWarning(man_globals, error_buf);
    }
    return(FALSE);
  }

  strcpy(tmp, MANTEMP);		/* get a temp file. */
#ifndef HAS_MKSTEMP
  (void) mktemp(tmp);
#else
  fd = mkstemp(tmp);
  if (fd < 0) {
      PopupWarning(man_globals, "Error creating a temp file");
      return FALSE;
  }
  *output_fd = fdopen(fd, "r");
#endif
  strcpy(output, tmp);

  snprintf(cmdbuf, sizeof(cmdbuf), "%s %s > %s", SFORMAT, filename, output);
  if(system(cmdbuf) == 0) 	/* execute search. */
    return(TRUE);

  snprintf(error_buf, sizeof(error_buf),
	   "Error while converting from sgml, command was: %s", cmdbuf);
  PopupWarning(man_globals, error_buf);
  return(FALSE);
}
#endif /* defined (SMAN) && defined(SFORMAT) */

/*	Function Name: Format
 *	Description: This funtion formats the manual pages and interfaces
 *                   with the user.
 *	Arguments: man_globals - the psuedo globals
 *                 file - the file pointer to use and return
 *                 entry - the current entry struct.
 *                 current_box - The current directory being displayed.
 *	Returns: none.
 */

/* ARGSUSED */

FILE *
Format(ManpageGlobals * man_globals, char * entry)
{
  FILE * file = NULL;
#ifdef HAS_MKSTEMP
  int fd;
#endif
  Widget manpage = man_globals->manpagewidgets.manpage;
  char cmdbuf[BUFSIZ], tmp[BUFSIZ], filename[BUFSIZ], error_buf[BUFSIZ];
  char path[BUFSIZ], sect[BUFSIZ];
  XEvent event;
  Position x,y;			/* location to pop up the
				   "would you like to save" widget. */

#ifndef HAS_MKSTEMP
  if ( !UncompressUnformatted(man_globals, entry, filename) ) {
#else
  if ( !UncompressUnformatted(man_globals, entry, filename, &file) ) {
#endif
    /* We Really could not find it, this should never happen, yea right. */
    snprintf(error_buf, sizeof(error_buf),
	     "Could not open manual page, %s", entry);
    PopupWarning(man_globals, error_buf);
    XtPopdown( XtParent(man_globals->standby) );
    return(NULL);
  }

#ifndef HAS_MKSTEMP
  if ((file = fopen(filename, "r")) != NULL) {
#else
  if (file != NULL) {
#endif
    char line[BUFSIZ];

    if (fgets(line, sizeof(line), file) != NULL) {
	if (strncmp(line, ".so ", 4) == 0) {
	  line[strlen(line) - 1] = '\0';
	  fclose(file);
	  unlink(filename);
	  if (line[4] != '/') {
	    char *ptr = NULL;

	    strcpy(tmp, entry);
	    if ((ptr = rindex(tmp, '/')) != NULL) {
	      *ptr = '\0';
	      if ((ptr = rindex(tmp, '/')) != NULL)
		ptr[1] = '\0';
	    }
	  }
	  else
	    *tmp = '\0';
	  snprintf(filename, sizeof(filename), "%s%s", tmp, line + 4);

	  return (Format(man_globals, filename));
	}
    }
    fclose(file);
  }

  Popup(XtParent(man_globals->standby), XtGrabExclusive);
  while ( !XCheckTypedWindowEvent(XtDisplay(man_globals->standby),
				  XtWindow(man_globals->standby),
				  Expose, &event) );
  XtDispatchEvent( &event );
  XFlush(XtDisplay(man_globals->standby));

  strcpy(tmp,MANTEMP);		          /* Get a temp file. */
#ifndef HAS_MKSTEMP
  (void) mktemp(tmp);
#else
  fd = mkstemp(tmp);
  file = fdopen(fd, "r");
#endif
  strcpy(man_globals->tempfile, tmp);

  ParseEntry(entry, path, sect, NULL);

#ifndef HANDLE_ROFFSEQ
#ifndef HAS_MKSTEMP
  snprintf(cmdbuf, sizeof(cmdbuf), "cd %s ; %s %s %s > %s %s", path, TBL,
	  filename, FORMAT, man_globals->tempfile, "2> /dev/null");
#else
  snprintf(cmdbuf, sizeof(cmdbuf), "cd %s ; %s %s %s >> %s %s", path, TBL,
	  filename, FORMAT, man_globals->tempfile, "2> /dev/null");
#endif
#else
  /* Handle more flexible way of specifying the formatting pipeline */
  if (! ConstructCommand(cmdbuf, path, filename, man_globals->tempfile)) {
     sprintf(error_buf, "Constructed command was too long!");
     PopupWarning(man_globals, error_buf);
     file = NULL;
  }
  else
#endif /* HANDLE_ROFFSEQ */

  if(system(cmdbuf) != 0) {	/* execute search. */
    snprintf(error_buf, sizeof(error_buf),
	    "Something went wrong trying to run the command: %s", cmdbuf);
    PopupWarning(man_globals, error_buf);
    file = NULL;
  }
  else {
#ifndef HAS_MKSTEMP
    if ((file = fopen(man_globals->tempfile,"r")) == NULL) {
      sprintf(error_buf, "Something went wrong in retrieving the %s",
	      "temp file, try cleaning up /tmp");
      PopupWarning(man_globals, error_buf);
    }
    else {
#endif

      XtPopdown( XtParent(man_globals->standby) );

      if ( (man_globals->save == NULL) ||
	   (man_globals->manpagewidgets.manpage == NULL) )
	unlink(man_globals->tempfile);
      else {
	char * ptr, catdir[BUFSIZ];

	/*
	 * If the catdir is writeable then ask the user if he/she wants to
	 * write the man page to it.
	 */

	strcpy(catdir, man_globals->save_file);
	if ( (ptr = rindex(catdir, '/')) != NULL) {
	  *ptr = '\0';

	  if ( access(catdir, W_OK) != 0 )
	    unlink(man_globals->tempfile);
	  else {
	    x = (Position) Width(man_globals->manpagewidgets.manpage)/2;
	    y = (Position) Height(man_globals->manpagewidgets.manpage)/2;
	    XtTranslateCoords(manpage, x, y, &x, &y);
	    PositionCenter( man_globals->save, (int) x, (int) y, 0, 0, 0, 0);
	    XtPopup( man_globals->save, XtGrabExclusive);
	  }
	}
	else
	  unlink(man_globals->tempfile);
      }
#ifndef HAS_MKSTEMP
    }
#endif
  }

 /*
  * If the original was compressed or in another format, delete temporary file.
  */
  if (man_globals->deletetempfile) 
    unlink(filename);

  return(file);
}

#ifdef HANDLE_ROFFSEQ
/*      Function Name: ConstructCommand
 *      Description: Constructs the pipeline of commands necessary to format
 *                   a manual page.
 *      Arguments: cmdbuf - the buffer into which to write the command
 *                 path - the directory in which the original man page resides
 *                 filename - the (uncompressed) manpage source file
 *                 tempfile - the name of a temporary file to direct the final
 *                  output of the pipeline into
 *      Returns: TRUE if the command fit into the buffer, FALSE if it would
 *               be too long (more than BUFSIZ characters)
 */
static Boolean
ConstructCommand(cmdbuf, path, filename, tempfile)
   char *cmdbuf, *path, *filename, *tempfile;
{
   /* The original code did the following to produce a command line:
    *   sprintf(cmdbuf,"cd %s ; %s %s %s > %s %s", path, TBL,
    *      filename, FORMAT, man_globals->tempfile, "2> /dev/null");
    * We are more flexible and follow more or less the algorithm used
    * by the Linux man command:
    *  + Obtain a string of letters from the following sources in order
    *    of preference:
    *    + a command line option (not implemented in xman; it's probably not
    *      useful)
    *    + the first line of the manpage source, if it is of the form:
    *      '\" <string>
    *    + the MANROFFSEQ environment variable
    *    + a default string; this is "".
    *  + Interpret the string as a pipeline of filters:
    *    + e = eqn   g = grap   p = pic   t = tbl   v = vgrind   r = refer
    *  + zsoelim is always run as the first preprocessor in any case.
    *
    * Strictly speaking we should save a catpage iff the string comes
    * from the file or is the default.
    *
    * You'll notice that we format a man page into ASCII text output and then
    * attempt to interpret things like L^HL as bold and so forth. This
    * is so obviously the Wrong Thing it's untrue.
    */
   char *c = cmdbuf;           /* current posn in buffer */
   int left = BUFSIZ;          /* space left in buffer */
   int used;
   char *fmt;
   FILE *file;
   char fmtbuf[128];
   int gotfmt = 0;             /* set to 1 if we got a directive from source */
   char *fname = NULL;
#ifdef __UNIXOS2__
   int i;
#endif

   fmt = NULL;
   /* If you have a command line option that gives a setting for fmt,
      set it here. */

   if (!fmt) {
      /* This is the tricky bit: extract a format string from the source file
       * Annoyingly, filename might be relative or absolute. We cheat and
       * use system to get the thing to a known absoute filename.
       */
      if (filename[0] == '/') {
         fname = filename;
      } else {
         fname = malloc(strlen(path) + 1 + strlen(filename) + 1);
         if (!fname)
            return FALSE;
         sprintf(fname, "%s/%s", path, filename);
      }
      if ((file = fopen(fname, "r")) &&
          (fgets(fmtbuf, sizeof(fmtbuf), file)) &&
          (!memcmp(fmtbuf, "'\\\" ", 4))) {
                              /* that's squote-backslash-dquote-space */
         int len;
         fmt = fmtbuf + 3;
         len = strlen(fmt);
         if (len && (fmt[len-1] == '\n')) {
            fmt[len-1] = 0;
            gotfmt++;
         }
      }
      if (fname && fname != filename)
         free(fname);
      if (!gotfmt)                                /* not there or some error */
      {
         fmt = getenv("MANROFFSEQ");
      }
   }

   if (!fmt)
   {
      fmt = DEFAULT_MANROFFSEQ;
   }


   /* Start with the first fixed part of the command line */
#ifdef __UNIXOS2__
   for (i = 0; i < strlen(path); i++) {
     if (path[i] == '/')
       path[i] = '\\';
   }
   used = snprintf(c, left, "cd %s & %s %s ", path, ZSOELIM, filename);
#else
   used = snprintf(c, left, "cd %s; %s %s ", path, ZSOELIM, filename);
#endif
   left -= used;
   c += used;
   if (left <= 1)
      return (FALSE);

   /* Now add preprocessors of the form '| processor' */
   for ( ; *fmt; fmt++)
   {
      char *filter;
      switch (*fmt)
      {
         case 'e':
            filter = EQN;
            break;
         case 'g':
            filter = GRAP;
            break;
         case 'p':
            filter = PIC;
            break;
         case 't':
            filter = TBL;
            break;
         case 'v':
            filter = VGRIND;
            break;
         case 'r':
            filter = REFER;
            break;
         default:
            filter = NULL;
            break;
      }
      if (filter)
      {
         used = snprintf(c, left, " | %s ", filter);
         left -= used;
         c += used;
         if (left <= 1)
            return (FALSE);
      }
   }

   /* Now add the fixed trailing part 'formatprog > tempfile 2> /dev/null' */
#ifdef __UNIXOS2__
   used = snprintf(c, left, " | %s > %s 2>NUL", FORMAT, tempfile);
#else
#ifndef HAS_MKSTEMP
   used = snprintf(c, left, " | %s > %s 2>/dev/null", FORMAT, tempfile);
#else
   used = snprintf(c, left, " | %s >> %s 2>/dev/null", FORMAT, tempfile);
#endif
#endif /* __UNIXOS2__ */
   left -= used;
   if (left <= 1)
      return (FALSE);

   return (TRUE);
}
#endif /* HANDLE_ROFFSEQ */

/*	Function Name: UncompressUnformatted
 *	Description: Finds an uncompressed unformatted manual page.
 *	Arguments: man_globals - the psuedo global structure.
 *                 entry - the manual page entry.
 * RETURNED        filename - location to put the name of the file.
 *	Returns: TRUE if the file was found.
 */

static Boolean
#ifndef HAS_MKSTEMP
UncompressUnformatted(ManpageGlobals * man_globals, char * entry,
		      char * filename)
#else
UncompressUnformatted(ManpageGlobals * man_globals, char * entry,
		      char * filename, FILE **file)
#endif
{
  char path[BUFSIZ], page[BUFSIZ], section[BUFSIZ], input[BUFSIZ];
  int len_cat = strlen(CAT), len_man = strlen(MAN);
#if defined(SMAN) && defined(SFORMAT)
  int len_sman = strlen(SMAN);
#endif

  ParseEntry(entry, path, section, page);

#if defined(__OpenBSD__) || defined(__NetBSD__)
  /*
   * look for uncompressed file in machine subdir first
   */
  snprintf(filename, BUFSIZ, "%s/%s%s/%s/%s", path, MAN,
	  section + len_cat, MACHINE, page);
  if ( access( filename, R_OK ) == 0 ) {
    man_globals->compress = FALSE;
    man_globals->gzip = FALSE;
    man_globals->deletetempfile = FALSE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file),
    	     "%s/%s%s/%s/%s", path, CAT, section + len_cat, MACHINE, page);
    return(TRUE);
  }
 /*
  * Then for compressed files in an uncompressed directory.
  */
  snprintf(input, sizeof(input), "%s.%s", filename, COMPRESSION_EXTENSION);
#ifndef HAS_MKSTEMP
  if ( UncompressNamed(man_globals, input, filename) ) {
#else
  if ( UncompressNamed(man_globals, input, filename, file) ) {
#endif
    man_globals->compress = TRUE;
    man_globals->deletetempfile = TRUE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file),
	     "%s/%s%s/%s.%s", path, CAT, section + len_cat, page, 
	     COMPRESSION_EXTENSION);
    return(TRUE);
  }
#ifdef GZIP_EXTENSION
  else {
    snprintf(input, sizeof(input), "%s.%s", filename, GZIP_EXTENSION);
#ifndef HAS_MKSTEMP
    if ( UncompressNamed(man_globals, input, filename) ) {
#else
    if ( UncompressNamed(man_globals, input, filename, file) ) {
#endif
      man_globals->compress = TRUE;
      man_globals->gzip = TRUE;
      man_globals->deletetempfile = TRUE;
      snprintf(man_globals->save_file, sizeof(man_globals->save_file),
	       "%s/%s%s/%s.%s", path, CAT, section + len_cat, page,
	       GZIP_EXTENSION);
      return(TRUE);
    }
  }
#endif /* GZIP_EXTENSION */
#endif /* __OpenBSD__ || __NetBSD__ */
/*
 * Look for uncompressed file first.
 */

  snprintf(filename, BUFSIZ, "%s/%s%s/%s", path, MAN, section + len_man, page);
  if ( access( filename, R_OK ) == 0 ) {
    man_globals->compress = FALSE;
    man_globals->gzip = FALSE;
    man_globals->deletetempfile = FALSE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file), 
	     "%s/%s%s/%s", path, CAT, section + len_cat, page);
    return(TRUE);
  }

#if defined(SMAN) && defined(SFORMAT)
 /*
  * Look for uncompressed sgml file next.
  */

  snprintf(input, BUFSIZ, "%s/%s%s/%s", path, SMAN, section + len_sman, page);
#ifndef HAS_MKSTEMP
  if ( SgmlToRoffNamed(man_globals, input, filename) ) {
#else
  if ( SgmlToRoffNamed(man_globals, input, filename, file) ) {
#endif
    man_globals->compress = FALSE;
    man_globals->gzip = FALSE;
    man_globals->deletetempfile = TRUE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file),
            "%s/%s%s/%s", path, CAT, section + len_cat, page);
    return(TRUE);
  }
#endif

/*
 * Then for compressed files in an uncompressed directory.
 */

  snprintf(input, sizeof(input), "%s.%s", filename, COMPRESSION_EXTENSION);
#ifndef HAS_MKSTEMP
  if ( UncompressNamed(man_globals, input, filename) ) {
#else
  if ( UncompressNamed(man_globals, input, filename, file) ) {
#endif
    man_globals->compress = TRUE;
    man_globals->deletetempfile = TRUE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file),
	     "%s/%s%s/%s.%s", path, CAT, section + len_cat, page, 
	     COMPRESSION_EXTENSION);
    return(TRUE);
  }
#ifdef GZIP_EXTENSION
  else {
    snprintf(input, sizeof(input), "%s.%s", filename, GZIP_EXTENSION);
#ifndef HAS_MKSTEMP
    if ( UncompressNamed(man_globals, input, filename) ) {
#else
    if ( UncompressNamed(man_globals, input, filename, file) ) {
#endif	
      man_globals->compress = TRUE;
      man_globals->gzip = TRUE;
      man_globals->deletetempfile = TRUE;
      snprintf(man_globals->save_file, sizeof(man_globals->save_file),
	       "%s/%s%s/%s.%s", path, CAT, section + len_cat, page, 
	       GZIP_EXTENSION);
      return(TRUE);
    }
  }
#endif
/*
 * And lastly files in a compressed directory.
 */

  snprintf(input, sizeof(input), "%s/%s%s.%s/%s", path,
	  MAN, section + len_man, COMPRESSION_EXTENSION, page);
#ifndef HAS_MKSTEMP
  if ( UncompressNamed(man_globals, input, filename) ) {
#else
  if ( UncompressNamed(man_globals, input, filename, file) ) {
#endif
    man_globals->compress = TRUE;
    man_globals->deletetempfile = TRUE;
    snprintf(man_globals->save_file, sizeof(man_globals->save_file),
	     "%s/%s%s.%s/%s", path, CAT, section + len_cat, 
    	     COMPRESSION_EXTENSION, page);
    return(TRUE);
  }
  return(FALSE);
}

/*	Function Name: AddCursor
 *	Description: This function adds the cursor to the window.
 *	Arguments: w - the widget to add the cursor to.
 *                 cursor - the cursor to add to this widget.
 *	Returns: none
 */

void
AddCursor(Widget w, Cursor cursor)
{
  XColor colors[2];
  Arg args[10];
  Cardinal num_args = 0;
  Colormap c_map;

  if (!XtIsRealized(w)) {
    PopupWarning(NULL, "Widget is not realized, no cursor added.\n");
    return;
  }

  XtSetArg( args[num_args], XtNcolormap, &c_map); num_args++;
  XtGetValues( w, args, num_args);

  colors[0].pixel = resources.cursors.fg_color;
  colors[1].pixel = resources.cursors.bg_color;

  XQueryColors (XtDisplay(w), c_map, colors, 2);
  XRecolorCursor(XtDisplay(w), cursor, colors, colors+1);
  XDefineCursor(XtDisplay(w),XtWindow(w),cursor);
}

/*	Function Name: ChangeLabel
 *	Description: This function changes the label field of the
 *                   given widget to the string in str.
 *	Arguments: w - the widget.
 *                 str - the string to change the label to.
 *	Returns: none
 */

void
ChangeLabel(Widget w, char * str)
{
  Arg arglist[3];		/* An argument list. */

  if (w == NULL) return;

  XtSetArg(arglist[0], XtNlabel, str);

/* shouldn't really have to do this. */
  XtSetArg(arglist[1], XtNwidth, 0);
  XtSetArg(arglist[2], XtNheight, 0);

  XtSetValues(w, arglist, (Cardinal) 1);
}

/*
 * In an ideal world this would be part of the XToolkit, and I would not
 * have to do it, but such is life sometimes.  Perhaps in X11R3.
 */

/*	Function Name: PositionCenter
 *	Description: This function positions the given widgets center
 *                   in the following location.
 *	Arguments: widget - the widget widget to postion
 *                 x,y - The location for the center of the widget
 *                 above - number of pixels above center to locate this widget
 *                 left - number of pixels left of center to locate this widget
 *                 h_space, v_space - how close to get to the edges of the
 *                                    parent window.
 *	Returns: none
 *      Note:  This should only be used with a popup widget that has override
 *             redirect set.
 */

void
PositionCenter(Widget widget, int x, int y, int above, int left, int v_space, int h_space)
{
  Arg wargs[2];
  int x_temp,y_temp;		/* location of the new window. */
  int parent_height,parent_width; /* Height and width of the parent widget or
				   the root window if it has no parent. */

  x_temp = x - left - Width(widget) / 2 + BorderWidth(widget);
  y_temp = y - above -  Height(widget) / 2 + BorderWidth(widget);

  parent_height = HeightOfScreen(XtScreen(widget));
  parent_width = WidthOfScreen(XtScreen(widget));

/*
 * Check to make sure that all edges are within the viewable part of the
 * root window, and if not then force them to be.
 */

  if (x_temp < h_space)
    x_temp = v_space;
  if (y_temp < v_space)
    (y_temp = 2);

  if ( y_temp + Height(widget) + v_space > parent_height )
      y_temp = parent_height - Height(widget) - v_space;

  if ( x_temp + Width(widget) + h_space > parent_width )
      x_temp = parent_width - Width(widget) - h_space;

  XtSetArg(wargs[0], XtNx, x_temp);
  XtSetArg(wargs[1], XtNy, y_temp);
  XtSetValues(widget, wargs, 2);
}

/*	Function Name: ParseEntry(entry, path, sect, page)
 *	Description: Parses the manual pages entry filenames.
 *	Arguments: str - the full path name.
 *                 path - the path name.      RETURNED
 *                 sect - the section name.   RETURNED
 *                 page - the page name.      RETURNED
 *	Returns: none.
 */

void
ParseEntry(char *entry, char *path, char *sect, char *page)
{
  char *c, temp[BUFSIZ];

  strcpy(temp, entry);

  c = rindex(temp, '/');
  if (c == NULL)
    PrintError("index failure in ParseEntry.");
  *c++ = '\0';
  if (page != NULL)
    strcpy(page, c);

  c = rindex(temp, '/');
  if (c == NULL)
    PrintError("index failure in ParseEntry.");
  *c++ = '\0';
#if defined(SFORMAT) && defined(SMAN)
  /* sgmltoroff sometimes puts an extra ./ in the path to .so entries */
  if (strcmp(c, ".") == 0) {
      c = rindex(temp, '/');
      if (c == NULL)
	  PrintError("index failure in ParseEntry.");
      *c++ = '\0';
  }
#endif      
#if defined(__OpenBSD__) || defined(__NetBSD__)
  /* Skip machine subdirectory if present */
  if (strcmp(c, MACHINE) == 0) {
      c = rindex(temp, '/');
      if (c == NULL)
	  PrintError("index failure in ParseEntry.");
      *c++ = '\0';
  }
#endif
  if (sect != NULL)
    strcpy(sect, c);

  if (path != NULL)
    strcpy(path, temp);
}

/*      Function Name: GetGlobals
 *      Description: Gets the psuedo globals associated with the
 *                   manpage associated with this widget.
 *      Arguments: w - a widget in the manpage.
 *      Returns: the psuedo globals.
 *      Notes: initial_widget is a globals variable.
 *             manglobals_context is a global variable.
 */

ManpageGlobals *
GetGlobals(Widget w)
{
  Widget temp;
  caddr_t data;

  while ( (temp = XtParent(w)) != initial_widget && (temp != NULL))
    w = temp;

  if (temp == NULL)
    XtAppError(XtWidgetToApplicationContext(w),
	       "Xman: Could not locate widget in tree, exiting");

  if (XFindContext(XtDisplay(w), XtWindow(w),
		   manglobals_context, &data) != XCSUCCESS)
    XtAppError(XtWidgetToApplicationContext(w),
	       "Xman: Could not find global data, exiting");

  return( (ManpageGlobals *) data);
}

/*      Function Name: SaveGlobals
 *      Description: Saves the psuedo globals on the widget passed
 *                   to this function, although GetGlobals assumes that
 *                   the data is associated with the popup child of topBox.
 *      Arguments: w - the widget to associate the data with.
 *                 globals - data to associate with this widget.
 *      Returns: none.
 *      Notes: WIDGET MUST BE REALIZED.
 *             manglobals_context is a global variable.
 */

void
SaveGlobals(Widget w, ManpageGlobals * globals)
{
  if (XSaveContext(XtDisplay(w), XtWindow(w), manglobals_context,
		   (caddr_t) globals) != XCSUCCESS)
    XtAppError(XtWidgetToApplicationContext(w),
	       "Xman: Could not save global data, are you out of memory?");
}

/*      Function Name: RemoveGlobals
 *      Description: Removes the psuedo globals from the widget passed
 *                   to this function.
 *      Arguments: w - the widget to remove the data from.
 *      Returns: none.
 *      Notes: WIDGET MUST BE REALIZED.
 *             manglobals_context is a global variable.
 */

void
RemoveGlobals(Widget w)
{
  if (XDeleteContext(XtDisplay(w), XtWindow(w),
		     manglobals_context) != XCSUCCESS)
    XtAppError(XtWidgetToApplicationContext(w),
	       "Xman: Could not remove global data?");
}

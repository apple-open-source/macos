/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclxfconf.c,v 3.31 2002/05/31 18:45:57 dawes Exp $ */
/*
 * Copyright 1996,1999 by Joseph V. Moss <joe@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Joseph Moss not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Joseph Moss makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * JOSEPH MOSS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL JOSEPH MOSS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $XConsortium: tclxfconf.c /main/3 1996/10/23 11:44:07 kaleb $ */


/*

  This file contains Tcl bindings to the XF86Config file read/write routines

 */

#include "X.h"
#include "os.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Parser.h"
#include "xf86tokens.h"

#include "xf86Xinput.h"
#include "mouse.h"

#include "tcl.h"

#include "xfsconf.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

XF86ConfigPtr config_list;

Tcl_Interp *errinterp;

Bool Must_have_memory = FALSE;

/* Error handling functions */

void
VErrorF(f, args)
    const char *f;
    va_list args;
{
    char tmpbuf[1024];
    vsprintf(tmpbuf, f, args);
    Tcl_AppendResult(errinterp, tmpbuf, (char *) NULL);
}

void
ErrorF(const char * f, ...)
{
    va_list args;
    va_start(args, f);
    VErrorF(f, args);
    va_end(args);
}

void
FatalError(const char *f, ...)
{
    va_list args;
    ErrorF("\nFatal server error:\n");
    va_start(args, f);
    VErrorF(f, args);
    va_end(args);
    ErrorF("\n");
}

/*
   Adds the config file R/W commands to the Tcl interpreter
*/

int
XF86Config_Init(interp)
    Tcl_Interp	*interp;
{
	Tcl_CreateCommand(interp, "xf86config_readfile",
		TCL_XF86ReadXF86Config, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86config_writefile",
		TCL_XF86WriteXF86Config, (ClientData) NULL,
		(void (*)()) NULL);

	return TCL_OK;
}

/*
  Convert the given value to a string representation in the specified
  base.  If the value is zero, return an empty string
*/

char *NonZeroStr(val, base)
  unsigned long val;
  int base;
{
	static char tmpbuf[16];

	if (val) {
		if (base == 16)
			sprintf(tmpbuf, "%#lx", val);
		else
			sprintf(tmpbuf, "%ld", val);
		return tmpbuf;
	} else
		return "";
}

/* other subroutines */

#define DIR_FILE	"/fonts.dir"

/*
 * xf86GetPathElem --
 *	Extract a single element from the font path string starting at
 *	pnt.  The font path element will be returned, and pnt will be
 *	updated to point to the start of the next element, or set to
 *	NULL if there are no more.
 *
 * Taken from xf86Config.c.
 */
char *
get_path_elem(pnt)
     char **pnt;
{
  char *p1;

  p1 = *pnt;
  *pnt = strchr(*pnt, ',');
  if (*pnt != NULL) {
    **pnt = '\0';
    *pnt += 1;
  }
  return(p1);
}

/*
 * xf86ValidateFontPath --
 *	Validates the user-specified font path.  Each element that
 *	begins with a '/' is checked to make sure the directory exists.
 *	If the directory exists, the existence of a file named 'fonts.dir'
 *	is checked.  If either check fails, an error is printed and the
 *	element is removed from the font path.
 *
 * Taken from xf86Config.c.
 */
#define CHECK_TYPE(mode, type) ((S_IFMT & (mode)) == (type))
char *
validate_font_path(path)
     char *path;
{
  char *tmp_path, *out_pnt, *path_elem, *next, *p1, *dir_elem;
  struct stat stat_buf;
  int flag;
  int dirlen;

  tmp_path = (char *)XtCalloc(1,strlen(path)+1);
  out_pnt = tmp_path;
  path_elem = NULL;
  next = path;
  while (next != NULL) {
    path_elem = get_path_elem(&next);
#ifndef __UNIXOS2__
    if (*path_elem == '/') {
      dir_elem = (char *)XtCalloc(1, strlen(path_elem) + 1);
      if ((p1 = strchr(path_elem, ':')) != 0)
#else
    /* OS/2 must prepend X11ROOT */
    if (*path_elem == '/') {
      path_elem = (char*)__XOS2RedirRoot(path_elem);
      dir_elem = (char*)XtCalloc(1, strlen(path_elem) + 1);
      if (p1 = strchr(path_elem+2, ':'))
#endif
	dirlen = p1 - path_elem;
      else
	dirlen = strlen(path_elem);
      strncpy(dir_elem, path_elem, dirlen);
      dir_elem[dirlen] = '\0';
      flag = stat(dir_elem, &stat_buf);
      if (flag == 0)
	if (!CHECK_TYPE(stat_buf.st_mode, S_IFDIR))
	  flag = -1;
      if (flag != 0) {
        ErrorF("Warning: The directory \"%s\" does not exist.\n", dir_elem);
	ErrorF("         Entry deleted from font path.\n");
	continue;
      }
      else {
	p1 = (char *)XtMalloc(strlen(dir_elem)+strlen(DIR_FILE)+1);
	strcpy(p1, dir_elem);
	strcat(p1, DIR_FILE);
	flag = stat(p1, &stat_buf);
	if (flag == 0)
	  if (!CHECK_TYPE(stat_buf.st_mode, S_IFREG))
	    flag = -1;
#ifndef __UNIXOS2__
	XtFree(p1);
#endif
	if (flag != 0) {
	  ErrorF("Warning: 'fonts.dir' not found (or not valid) in \"%s\".\n", 
		 dir_elem);
	  ErrorF("          Entry deleted from font path.\n");
	  ErrorF("          (Run 'mkfontdir' on \"%s\").\n", dir_elem);
	  continue;
	}
      }
      XtFree(dir_elem);
    }

    /*
     * Either an OK directory, or a font server name.  So add it to
     * the path.
     */
    if (out_pnt != tmp_path)
      *out_pnt++ = ',';
    strcat(out_pnt, path_elem);
    out_pnt += strlen(path_elem);
  }
  return(tmp_path);
}

/*
 * xf86TokenToString --
 *	returns the string corresponding to token
 */
char *
token_to_string(SymTabPtr table, int token)
{
  int i;

  for (i = 0; table[i].token >= 0 && table[i].token != token; i++)
    ;
  if (table[i].token < 0)
    return("unknown");
  else
    return((char *) table[i].name);
}
 
/*
 * xf86StringToToken --
 *	returns the string corresponding to token
 */
int
string_to_token(table, string)
     SymTabPtr table;
     char *string;
{
  int i;

  for (i = 0; table[i].token >= 0 && NameCompare(string, table[i].name); i++)
    ;
  return(table[i].token);
}


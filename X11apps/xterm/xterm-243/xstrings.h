/* $XTermId: xstrings.h,v 1.14 2008/12/30 17:10:43 tom Exp $ */

/* $XFree86: xc/programs/xterm/xstrings.h,v 1.5 2006/02/13 01:14:59 dickey Exp $ */

/************************************************************

Copyright 2000-2007,2008 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#ifndef included_xstrings_h
#define included_xstrings_h 1

extern char *x_basename(char * /* name */);
extern char *x_getenv(const char * /* name */);
extern char *x_nonempty(char * /* s */);
extern char *x_skip_blanks(char * /* s */);
extern char *x_skip_nonblanks(char * /* s */);
extern char *x_strdup(const char * /* s */);
extern char *x_strindex(char * /* s1 */, char * /* s2 */);
extern char *x_strtrim(char * /* s */);
extern char x_toupper(int /* ch */);
extern int x_strcasecmp(const char * /* s1 */, const char * /* s2 */);
extern int x_strncasecmp(const char * /* s1 */, const char * /* s2 */, unsigned  /* n */);

#endif /* included_xstrings_h */

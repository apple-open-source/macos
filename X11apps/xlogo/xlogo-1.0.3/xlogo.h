/*

Copyright 2004 Roland Mainz <roland.mainz@nrubsig.org>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

 */

#ifndef XLOGO_XLOGO_H
#define XLOGO_XLOGO_H 1

#include <X11/Intrinsic.h>

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))

#define Error(x) { printf x ; exit(EXIT_FAILURE); }
#define Log(x)   { if(userOptions.verbose) printf x; }
#define Msg(x)   { if((!userOptions.quiet) || userOptions.verbose) printf x; }

typedef struct {
  Boolean verbose;
  Boolean quiet;
  Boolean printAndExit;
  String  printername;
  String  printfile;
} XLogoResourceData, *XLogoResourceDataPtr;

/* Global vars */
extern const char        *ProgramName; /* program name (from argv[0]) */
extern XLogoResourceData  userOptions;

#endif /* !XLOGO_XLOGO_H */


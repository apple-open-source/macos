/* $Xorg: RxI.h,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#ifndef _RxI_h
#define _RxI_h

#include "Rx.h"
#include <X11/Xos.h>		/* for strcmp() etc... */
#include <stdlib.h>
#include <stdio.h>


/* "wrappers" to std functions */
#ifdef NETSCAPE_PLUGIN
#include "npapi.h"
#define Malloc(s) NPN_MemAlloc(s)
#define Realloc(p, olds, s) _RxRealloc(p, olds, s)
#define Free(p) { if (p) NPN_MemFree(p); }
extern void * _RxRealloc(void *p, size_t olds, size_t s);
#else
#define Malloc(s) malloc(s)
#define Realloc(p, olds, s) realloc(p, s)
#define Free(p) { if (p) free(p); }
#endif

#ifdef NEED_STRCASECMP
#define Strcasecmp(s1,s2) _RxStrcasecmp(s1,s2)
#define Strncasecmp(s1,s2,n) _RxStrncasecmp(s1,s2,n)
extern int _RxStrcasecmp(const char *, const char *);
extern int _RxStrncasecmp(const char *, const char *, size_t);
#else
#define Strcasecmp(s1,s2) strcasecmp(s1,s2)
#define Strncasecmp(s1,s2,n) strncasecmp(s1,s2,n)
#endif

/* recognized parameter names */
#define RX_VERSION		"VERSION"
#define RX_ACTION		"ACTION"
#define RX_EMBEDDED		"EMBEDDED"
#define RX_AUTO_START		"AUTO-START"
#define RX_WIDTH		"WIDTH"
#define RX_HEIGHT		"HEIGHT"
#define RX_APP_GROUP		"APP-GROUP"
#define RX_REQUIRED_SERVICES	"REQUIRED-SERVICES"
#define RX_UI			"UI"
#define RX_PRINT		"PRINT"

/* X protocol private parameter names */
#define RX_X_UI_INPUT_METHOD	"X-UI-INPUT-METHOD"
#define RX_X_UI_LBX		"X-UI-LBX"
#define RX_X_PRINT_LBX		"X-PRINT-LBX"
#define RX_X_AUTH		"X-AUTH"
#define RX_X_UI_AUTH		"X-UI-AUTH"
#define RX_X_PRINT_AUTH		"X-PRINT-AUTH"
#define RX_X_UI_LBX_AUTH	"X-UI-LBX-AUTH"
#define RX_X_PRINT_LBX_AUTH	"X-PRINT-LBX-AUTH"


/* recognized parameter values */
#define RX_YES			"YES"
#define RX_NO			"NO"


/* HTTP GET request delimiters */
#define RX_QUERY_DELIMITER	'?'
#define RX_PARAM_DELIMITER	';'


#endif /* _RxI_h */

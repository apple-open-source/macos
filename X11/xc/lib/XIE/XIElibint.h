/* $Xorg: XIElibint.h,v 1.5 2001/02/09 02:03:41 xorgcvs Exp $ */
/*

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/lib/XIE/XIElibint.h,v 3.6 2001/12/14 19:54:32 dawes Exp $ */

#ifndef _XIELIBINT_H_
#define _XIELIBINT_H_

#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/Xfuncs.h>
#include <X11/extensions/XIElib.h>
#include <X11/extensions/XIEproto.h>



/* -------------------------------------------------------------------------
 * Display extension data structures and macros.
 * ------------------------------------------------------------------------- */

/*
 * For each display initialized by XieInitialize(), a record is allocated
 * which holds various information about that display.  These records are
 * maintained in a linked list.  The record for the most recently referenced
 * display is always kept at the beginning of the list (for quick access).
 */

typedef struct _XieExtInfo
{
    Display             *display;       /* pointer to X display structure */
    XExtCodes		*extCodes;      /* extension codes */
    XieExtensionInfo	*extInfo;	/* extension information */
    struct _XieExtInfo  *next;       	/* next in list */
} XieExtInfo;


/*
 * Insert a new record in the beginning of the linked list.
 */

#define ADD_EXTENSION_INFO(_display, _info) \
\
{ \
    _info->display = _display; \
\
    _info->next = _XieExtInfoHeader; \
    _XieExtInfoHeader = _info; \
}


/*
 * Remove the record assosicated with '_display' from the linked list
 * and return a pointer to it in '_info'.
 */

#define REMOVE_EXTENSION_INFO(_display, _info) \
\
{ \
    XieExtInfo	*prev = NULL; \
\
    _info = _XieExtInfoHeader; \
\
    while (_info && _info->display != _display) \
    { \
	prev = _info; \
	_info = _info->next; \
    } \
\
    if (_info) \
	if (!prev) \
	    _XieExtInfoHeader = _info->next; \
	else \
	    prev->next = _info->next; \
}	


/*
 * Return the info assosicated with '_display' in '_info'.
 * If the info is not the first in the list, move it to the front.
 */

#define GET_EXTENSION_INFO(_display, _info) \
\
{ \
    if ((_info = _XieExtInfoHeader)) \
    { \
        if (_XieExtInfoHeader->display != _display) \
        { \
	    XieExtInfo	*prev = _XieExtInfoHeader; \
\
	    _info = _info->next; \
	    while (_info && _info->display != _display) \
	    { \
	        prev = _info; \
	        _info = _info->next; \
	    } \
\
	    if (_info) \
	    { \
	        prev->next = _info->next; \
	        _info->next = _XieExtInfoHeader; \
	        _XieExtInfoHeader = _info; \
	    } \
	} \
    } \
}






#define PAD(_size) (3 - (((_size) + 3) & 0x3))

#define PADDED_BYTES(_bytes) (_bytes + PAD (_bytes))

#define NUMWORDS(_size) (((unsigned int) ((_size) + 3)) >> 2)

#define NUMBYTES(_len) (((unsigned int) (_len)) << 2)

#define LENOF(_ctype) (SIZEOF (_ctype) >> 2)





/* -------------------------------------------------------------------------
 * Macros for setting up requests.
 * ------------------------------------------------------------------------- */

/*
 * Request names and opcodes.
 */

#if !defined(UNIXCPP) || defined(ANSICPP)
#define REQNAME(_name_) xie##_name_##Req
#define REQOPCODE(_name_) X_ie##_name_
#define REQSIZE(_name_) sz_xie##_name_##Req
#else
#define REQNAME(_name_) xie/**/_name_/**/Req
#define REQOPCODE(_name_) X_ie/**/_name_
#define REQSIZE(_name_) sz_xie/**/_name_/**/Req
#endif


/* 
 * GET_REQUEST sets up a request to be sent to the X server.  If there isn't
 * enough room left in the X buffer, it is flushed before the new request
 * is started.
 */

#define GET_REQUEST(_name, _req) \
    if ((display->bufptr + REQSIZE(_name)) > display->bufmax) \
        _XFlush (display); \
    _req = (char *) (display->last_req = display->bufptr); \
    display->bufptr += REQSIZE(_name); \
    display->request++


/*
 * GET_REQUEST_EXTRA is the same as GET_REQUEST and except that an additional
 * "extraBytes" are allocated after the request.  "extraBytes" will be
 * padded to a word boundary.
 */

#define GET_REQUEST_EXTRA(_name, _extraBytes, _req) \
    if ((display->bufptr + REQSIZE(_name) + \
	PADDED_BYTES (_extraBytes)) > display->bufmax) _XFlush (display); \
    _req = (char *) (display->last_req = display->bufptr); \
    display->bufptr += (REQSIZE(_name) + PADDED_BYTES (_extraBytes)); \
    display->request++


/*
 * BEGIN_REQUEST_HEADER and END_REQUEST_HEADER are used to hide
 * the extra work that has to be done on 64 bit clients.  On such
 * machines, all structure pointers must point to an 8 byte boundary.
 * As a result, we must first store the request header info in
 * a static data stucture, then bcopy it into the transport buffer.
 */

#ifndef WORD64

#define BEGIN_REQUEST_HEADER(_name, _pBuf, _pReq) \
{ \
    XieExtInfo *_xieExtInfo; \
    GET_EXTENSION_INFO (display, _xieExtInfo); \
    _pReq = (REQNAME(_name) *) _pBuf;

#define END_REQUEST_HEADER(_name, _pBuf, _pReq) \
    _pBuf += REQSIZE(_name); \
}

#else /* WORD64 */

#define BEGIN_REQUEST_HEADER(_name, _pBuf, _pReq) \
{ \
    XieExtInfo *_xieExtInfo; \
    REQNAME(_name) tReq; \
    GET_EXTENSION_INFO (display, _xieExtInfo); \
    _pReq = &tReq;

#define END_REQUEST_HEADER(_name, _pBuf, _pReq) \
    memcpy (_pBuf, _pReq, REQSIZE(_name)); \
    _pBuf += REQSIZE(_name); \
}

#endif /* WORD64 */


/*
 * Macros used to store the request header info.
 */

#define STORE_REQUEST_HEADER(_name, _req) \
    _req->reqType = _xieExtInfo->extCodes->major_opcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name)) >> 2;


#define STORE_REQUEST_EXTRA_HEADER(_name, _extraBytes, _req) \
    _req->reqType = _xieExtInfo->extCodes->major_opcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name) + PADDED_BYTES (_extraBytes)) >> 2;




typedef int (*XieTechFuncPtr) (char **, XiePointer, int);

typedef struct _XieTechFuncRec {
    int technique;
    XieTechFuncPtr techfunc;
    struct _XieTechFuncRec *next;
} XieTechFuncRec;




/* 
 * See if XSynchronize has been called.  If so, send request right away.
 */

#define SYNC_HANDLE(_display)\
    if ((_display)->synchandler) (*(_display)->synchandler) (_display)


/*
 * Read a reply into a scratch buffer.
 */

#define XREAD_INTO_SCRATCH(_display, _pBuf, _numBytes) \
    _pBuf = (char *) _XAllocTemp (_display, _numBytes); \
    _XRead (_display, _pBuf, _numBytes);

#define FINISH_WITH_SCRATCH(_display, _pBuf, _numBytes) \
    _XFreeTemp (_display, _pBuf, _numBytes);


/*
 * Externally defined globals
 */

extern XieExtInfo	*_XieExtInfoHeader;
extern void		(*(_XieElemFuncs[]))(char **, XiePhotoElement *);
extern XieTechFuncRec 	*_XieTechFuncs[];
extern Bool 		_XieTechFuncsInitialized;

extern Bool             _XieFloError (Display *, XErrorEvent *, xError *);
extern Status           _XieColorAllocEvent (Display *, XEvent *, xEvent *);
extern Status           _XieDecodeNotifyEvent (Display *, XEvent *, xEvent *);
extern Status           _XieExportAvailableEvent (Display *, XEvent *, xEvent *);
extern Status           _XieImportObscuredEvent (Display *, XEvent *, xEvent *);
extern Status           _XiePhotofloDoneEvent (Display *, XEvent *, xEvent *);
extern Status           _XieRegisterTechFunc (int, int, XieTechFuncPtr);
extern XieTechFuncPtr   _XieLookupTechFunc (int, int);
extern int              _XieCloseDisplay (Display *, XExtCodes *);
extern int              _XiePhotofloSize (XiePhotoElement *, int);
extern int              _XieTechniqueLength (int, int, XiePointer);
extern void             _XieEncodeTechnique (char **, int, int, XiePointer);
extern void             _XieInitTechFuncTable (void);
extern void             _XiePrintError (Display *display, XErrorEvent *error, void *fp);
extern xieTypFloat	_XieConvertToIEEE(double);

/* for X-Window system protocol elements */
#define sz_CARD32 				4
#define sz_CARD16 				2
#define sz_CARD8 				1
#define sz_INT32 				4
#define sz_INT16 				2

#endif /* _XIELIBINT_H_ */

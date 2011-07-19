/* **********************************************
 *						*
 * header file file descriptor (FD) code        *
 *						*
 *	James Peterson, 1987			*
 * Copyright (C) 1987 MCC
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of MCC not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  MCC makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MCC DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MCC BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *						*
 *						*
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 ********************************************** */

#ifndef XSCOPE_FD_H
#define XSCOPE_FD_H

/* 
   the following structure remembers for each file descriptor its
   state.  In particular, we need to know if it is busy or free
   and if it is in use, by whom.
*/
#ifdef USE_XTRANS
#define TRANS_CLIENT
#define TRANS_SERVER
#define X11_t
#include <X11/Xtrans/Xtrans.h>
#else
typedef void *XtransConnInfo;
#endif
#include <sys/select.h>

typedef int FD;

struct FDDescriptor
{
    Boolean Busy;
    void    (*InputHandler)(int);
    void    (*FlushHandler)(int);
#ifdef USE_XTRANS
    XtransConnInfo trans_conn;
#endif
};

extern struct FDDescriptor *FDD /* array of FD descriptors */ ;
extern short   MaxFD /* maximum number of FD's possible */ ;

extern short   nFDsInUse /* number of FD's actually in use */ ;

extern fd_set  ReadDescriptors /* bit map of FD's in use -- for select  */ ;
extern fd_set  WriteDescriptors /* bit map of write blocked FD's -- for select */;
extern fd_set  BlockedReadDescriptors /* bit map of FD's blocked from reading */;
extern short   HighestFD /* highest FD in use -- for select */ ;

/* need to change the MaxFD to allow larger number of fd's */
#define StaticMaxFD FD_SETSIZE

extern void InitializeFD(void);
extern void UsingFD(FD fd, void (*Handler)(int), void (*FlushHandler)(int),
		    XtransConnInfo trans_conn);
extern void NotUsingFD(FD fd);

extern FD AcceptConnection (FD  ConnectionSocket);
extern FD MakeConnection (const char *server, short port, int report,
			  XtransConnInfo *trans_conn);

extern int MainLoop(void);

#ifdef USE_XTRANS
extern XtransConnInfo GetXTransConnInfo(FD fd);
#endif

#endif /* XSCOPE_FD_H */

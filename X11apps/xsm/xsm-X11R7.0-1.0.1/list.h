/* $Xorg: list.h,v 1.4 2001/02/09 02:05:59 xorgcvs Exp $ */
/******************************************************************************

Copyright 1993, 1998  The Open Group

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
******************************************************************************/
/* $XFree86: xc/programs/xsm/list.h,v 1.5 2001/08/01 00:45:07 tsi Exp $ */

#ifndef _LIST_H_
#define _LIST_H_

typedef struct _List {
    struct _List	*prev;
    struct _List	*next;
    char		*thing;
} List;

extern List *ListInit(void);
extern List *ListFirst(List *l);
extern List *ListNext(List *l);
extern void ListFreeAll(List *l);
extern void ListFreeAllButHead(List *l);
extern List *ListAddFirst(List *l, char *v);
extern List *ListAddLast(List *l, char *v);
extern void ListFreeOne(List *e);
extern Status ListSearchAndFreeOne(List *l, char *thing);
extern int ListCount(List *l);

#endif

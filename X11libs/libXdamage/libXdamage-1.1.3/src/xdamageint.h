/*
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _XDAMAGEINT_H_
#define _XDAMAGEINT_H_

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/damageproto.h>
#include "Xdamage.h"

typedef struct _XDamageExtDisplayInfo {
    struct _XDamageExtDisplayInfo  *next;    /* keep a linked list */
    Display                 *display;	    /* which display this is */
    XExtCodes               *codes;	    /* the extension protocol codes */
    int			    major_version;  /* -1 means we don't know */
    int			    minor_version;  /* -1 means we don't know */
} XDamageExtDisplayInfo;

/* replaces XExtensionInfo */
typedef struct _XDamageExtInfo {
    XDamageExtDisplayInfo    *head;          /* start of the list */
    XDamageExtDisplayInfo    *cur;           /* most recently used */
    int                     ndisplays;      /* number of displays */
} XDamageExtInfo;

extern XDamageExtInfo XDamageExtensionInfo;
extern const char XDamageExtensionName[];

XDamageExtDisplayInfo *
XDamageFindDisplay (Display *dpy);

#define XDamageHasExtension(i) ((i) && ((i)->codes))

#define XDamageCheckExtension(dpy,i,val) \
  if (!XDamageHasExtension(i)) { return val; }

#define XDamageSimpleCheckExtension(dpy,i) \
  if (!XDamageHasExtension(i)) { return; }

#endif /* _XDAMAGEINT_H_ */
